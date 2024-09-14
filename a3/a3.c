#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <arpa/inet.h>

#define RESP_PIPE_35896 "RESP_PIPE_35896"
#define REQ_PIPE_35896 "REQ_PIPE_35896"
#define SHM_NAME "/QvHVpCxe"
#define SHM_SIZE 1062203

int shmfd = -1;
void *shmAddr = NULL;
void *fileAddr = NULL;
size_t fileSize = 0;

int read_str_field(int pipefd, char *buffer);
void write_str_field(int pipefd, const char *str);
int read_nb_field(int pipefd, unsigned int *nb);
void write_nb_field(int pipefd, unsigned int nb);
void echo(int resp_pipe);
void create_shm(int req_pipe, int resp_pipe);
void write_to_shm(int req_pipe, int resp_pipe);
void map_file(int req_pipe, int resp_pipe);
void read_file_offset(int req_pipe, int resp_pipe);
void read_file_section(int req_pipe, int resp_pipe);
void read_logical_space_offset(int req_pipe, int resp_pipe);

int main() {
    // create pipe
    if (mkfifo(RESP_PIPE_35896, 0666) != 0) {
        printf("ERROR\n");
        perror("cannot create the response pipe");
        return 1;
    }

    // open  pipe for reading
    int req_pipe = open(REQ_PIPE_35896, O_RDONLY);
    if (req_pipe == -1) {
        printf("ERROR\n");
        perror("cannot open the request pipe");
        unlink(RESP_PIPE_35896);
        return 1;
    }

    // open pipe for writing
    int resp_pipe = open(RESP_PIPE_35896, O_WRONLY);
    if (resp_pipe == -1) {
        printf("ERROR\n");
        perror("cannot open the response pipe");
        close(req_pipe);
        unlink(RESP_PIPE_35896);
        return 1;
    }

    char message[] = {5, 'H', 'E', 'L', 'L', 'O'};
    write(resp_pipe, message, sizeof(message));
    
    printf("SUCCESS\n");

    char req_type[256];
    
    while (read_str_field(req_pipe, req_type) > 0) {
        if (strcmp(req_type, "ECHO") == 0) {
            echo(resp_pipe);
        } else if (strcmp(req_type, "CREATE_SHM") == 0) {
            create_shm(req_pipe, resp_pipe);
        } else if (strcmp(req_type, "WRITE_TO_SHM") == 0) {
            write_to_shm(req_pipe, resp_pipe);
        } else if (strcmp(req_type, "MAP_FILE") == 0) {
            map_file(req_pipe, resp_pipe);
        } else if (strcmp(req_type, "READ_FROM_FILE_OFFSET") == 0) {
            read_file_offset(req_pipe, resp_pipe);
        } else if (strcmp(req_type, "READ_FROM_FILE_SECTION") == 0) {
            read_file_section(req_pipe, resp_pipe);
        } else if (strcmp(req_type, "READ_FROM_LOGICAL_SPACE_OFFSET") == 0) {
            read_logical_space_offset(req_pipe, resp_pipe);
        } else if (strcmp(req_type, "EXIT") == 0) {
            break;
        } else {
            // ERROR
            write_str_field(resp_pipe, req_type);
            write_str_field(resp_pipe, "ERROR");
        }
    }

    // close pipes
    close(req_pipe);
    close(resp_pipe);
    unlink(RESP_PIPE_35896);

    // clean up mapping memory
    if (fileAddr != NULL) {
        munmap(fileAddr, fileSize);
    }
    if (shmAddr != NULL) {
        munmap(shmAddr, SHM_SIZE);
        close(shmfd);
        shm_unlink(SHM_NAME);
    }

    return 0;
}

// read a string field from the pipe
int read_str_field(int pipefd, char *buffer) {
    unsigned char size;
    if (read(pipefd, &size, 1) != 1) {
        return -1;
    }
    if (read(pipefd, buffer, size) != size) {
        return -1;
    }
    buffer[size] = '\0';
    return size + 1; // total bytes read
}

// read a nb field from the pipe
int read_nb_field(int pipefd, unsigned int *nb) {
    if (read(pipefd, nb, sizeof(unsigned int)) != sizeof(unsigned int)) {
        return -1;
    }
    return sizeof(unsigned int);
}

// write a string field to the pipe
void write_str_field(int pipefd, const char *str) {
    unsigned char size = strlen(str);
    write(pipefd, &size, 1);
    write(pipefd, str, size);
}

// write a nb field to the pipe
void write_nb_field(int pipefd, unsigned int nb) {
    write(pipefd, &nb, sizeof(unsigned int));
}

void echo(int resp_pipe) {
    write_str_field(resp_pipe, "ECHO");
    write_nb_field(resp_pipe, 35896);
    write_str_field(resp_pipe, "VARIANT");
}

void create_shm(int req_pipe, int resp_pipe) {
    unsigned int shm_size;
    if (read_nb_field(req_pipe, &shm_size) > 0) {
        shmfd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0664);
        if (shmfd < 0) {
            write_str_field(resp_pipe, "CREATE_SHM");
            write_str_field(resp_pipe, "ERROR");
        } else {
            if (ftruncate(shmfd, shm_size) == 0) {
                shmAddr = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
                if (shmAddr == MAP_FAILED) {
                    write_str_field(resp_pipe, "CREATE_SHM");
                    write_str_field(resp_pipe, "ERROR");
                } else {
                    write_str_field(resp_pipe, "CREATE_SHM");
                    write_str_field(resp_pipe, "SUCCESS");
                }
            } else {
                write_str_field(resp_pipe, "CREATE_SHM");
                write_str_field(resp_pipe, "ERROR");
            }
        }
    } else {
        write_str_field(resp_pipe, "CREATE_SHM");
        write_str_field(resp_pipe, "ERROR");
    }
}

void write_to_shm(int req_pipe, int resp_pipe) {
    unsigned int offset, value;
    if (read_nb_field(req_pipe, &offset) > 0 && read_nb_field(req_pipe, &value) > 0) {
        if (offset < SHM_SIZE && (offset + sizeof(unsigned int)) <= SHM_SIZE) {
            memcpy((unsigned char *)shmAddr + offset, &value, sizeof(unsigned int));
            write_str_field(resp_pipe, "WRITE_TO_SHM");
            write_str_field(resp_pipe, "SUCCESS");
        } else {
            write_str_field(resp_pipe, "WRITE_TO_SHM");
            write_str_field(resp_pipe, "ERROR");
        }
    } else {
        write_str_field(resp_pipe, "WRITE_TO_SHM");
        write_str_field(resp_pipe, "ERROR");
    }
}

void map_file(int req_pipe, int resp_pipe) {
    char file_name[256];
    if (read_str_field(req_pipe, file_name) > 0) {
        int file_fd = open(file_name, O_RDONLY);
        if (file_fd < 0) {
            write_str_field(resp_pipe, "MAP_FILE");
            write_str_field(resp_pipe, "ERROR");
        } else {
            struct stat file_stat;
            if (fstat(file_fd, &file_stat) == 0) {
                fileSize = file_stat.st_size;
                fileAddr = mmap(0, fileSize, PROT_READ, MAP_PRIVATE, file_fd, 0);
                if (fileAddr == MAP_FAILED) {
                    write_str_field(resp_pipe, "MAP_FILE");
                    write_str_field(resp_pipe, "ERROR");
                } else {
                    write_str_field(resp_pipe, "MAP_FILE");
                    write_str_field(resp_pipe, "SUCCESS");
                }
            } else {
                write_str_field(resp_pipe, "MAP_FILE");
                write_str_field(resp_pipe, "ERROR");
            }
            close(file_fd);
        }
    } else {
        write_str_field(resp_pipe, "MAP_FILE");
        write_str_field(resp_pipe, "ERROR");
    }
}

void read_file_offset(int req_pipe, int resp_pipe) {
    unsigned int offset, no_of_bytes;
    if (read_nb_field(req_pipe, &offset) > 0 && read_nb_field(req_pipe, &no_of_bytes) > 0) {
        if (fileAddr != NULL && shmAddr != NULL && offset + no_of_bytes <= fileSize) {
            memcpy(shmAddr, (unsigned char *)fileAddr + offset, no_of_bytes);
            write_str_field(resp_pipe, "READ_FROM_FILE_OFFSET");
            write_str_field(resp_pipe, "SUCCESS");
        } else {
            write_str_field(resp_pipe, "READ_FROM_FILE_OFFSET");
            write_str_field(resp_pipe, "ERROR");
        }
    } else {
        write_str_field(resp_pipe, "READ_FROM_FILE_OFFSET");
        write_str_field(resp_pipe, "ERROR");
    }
}

void read_file_section(int req_pipe, int resp_pipe) {
    unsigned int section_no, offset, no_of_bytes;
    if (read_nb_field(req_pipe, &section_no) > 0 && read_nb_field(req_pipe, &offset) > 0 && read_nb_field(req_pipe, &no_of_bytes) > 0) {
        if (fileAddr != NULL && shmAddr != NULL) {

            unsigned char *fileptr = (unsigned char *)fileAddr;
            // unsigned short headerSize = *(unsigned short *)(fileptr + 4);
            // unsigned char version = *(fileptr + 6);
            unsigned char no_of_sections = *(fileptr + 7);

            if (section_no < 1 || section_no > no_of_sections) {
                write_str_field(resp_pipe, "READ_FROM_FILE_SECTION");
                write_str_field(resp_pipe, "ERROR");
                return;
            }

            unsigned char *sectionptr = fileptr + 8 + (section_no - 1) * 20;

            char sect_name[9];
            memcpy(sect_name, sectionptr, 8);
            sect_name[8] = '\0';
            uint32_t sect_offset = *(uint32_t *)(sectionptr + 12);
            uint32_t sect_size = *(uint32_t *)(sectionptr + 16);

            if (offset + no_of_bytes > sect_size) {
                write_str_field(resp_pipe, "READ_FROM_FILE_SECTION");
                write_str_field(resp_pipe, "ERROR");
                return;
            }

            memcpy(shmAddr, fileptr + sect_offset + offset, no_of_bytes);
            write_str_field(resp_pipe, "READ_FROM_FILE_SECTION");
            write_str_field(resp_pipe, "SUCCESS");
        } else {
            write_str_field(resp_pipe, "READ_FROM_FILE_SECTION");
            write_str_field(resp_pipe, "ERROR");
        }
    } else {
        write_str_field(resp_pipe, "READ_FROM_FILE_SECTION");
        write_str_field(resp_pipe, "ERROR");
    }
}

void read_logical_space_offset(int req_pipe, int resp_pipe) {
    unsigned int logical_offset, no_of_bytes;
    if (read_nb_field(req_pipe, &logical_offset) > 0 && read_nb_field(req_pipe, &no_of_bytes) > 0) {
        if (fileAddr != NULL && shmAddr != NULL) {
            unsigned char *fileptr = (unsigned char *)fileAddr;
            unsigned char no_of_sections = *(fileptr + 7);
            unsigned int logicaladdress = 0;
            unsigned int gasit = 0;

            for (unsigned char i = 0; i < no_of_sections; i++) {
                unsigned char *sectionptr = fileptr + 8 + i * (20);
                uint32_t sect_offset = *(uint32_t *)(sectionptr + 12);
                uint32_t sect_size = *(uint32_t *)(sectionptr + 16);

                if (logical_offset < logicaladdress + sect_size) {
                    uint32_t file_offset = sect_offset + (logical_offset - logicaladdress);
                    if (file_offset + no_of_bytes <= fileSize) {
                        memcpy(shmAddr, fileptr + file_offset, no_of_bytes);
                        write_str_field(resp_pipe, "READ_FROM_LOGICAL_SPACE_OFFSET");
                        write_str_field(resp_pipe, "SUCCESS");
                        gasit = 1;
                        break;
                    }
                }
                logicaladdress += (sect_size + 1023) & ~1023;
            }

            if (!gasit) {
                write_str_field(resp_pipe, "READ_FROM_LOGICAL_SPACE_OFFSET");
                write_str_field(resp_pipe, "ERROR");
            }
        } else {
            write_str_field(resp_pipe, "READ_FROM_LOGICAL_SPACE_OFFSET");
            write_str_field(resp_pipe, "ERROR");
        }
    } else {
        write_str_field(resp_pipe, "READ_FROM_LOGICAL_SPACE_OFFSET");
        write_str_field(resp_pipe, "ERROR");
    }
}