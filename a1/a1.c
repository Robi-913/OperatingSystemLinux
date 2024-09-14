#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <arpa/inet.h>

/*
definirea valorilor harcodate pentru fisierul de tip SF
*/
#define MAGIC "SELD"
#define MAGIC_LEN 4
#define VERSION_MIN 68
#define VERSION_MAX 156
#define NO_OF_SECTIONS_MIN 4
#define NO_OF_SECTIONS_MAX 14
#define HEADER_SIZE 2 
#define VERSION_SIZE 1 
#define NO_OF_SECTIONS_SIZE 1
#define SECTION_TYPE_REQUIRED 49

/*
aceasta structura de unitati de 32 de biti salveaza valorile de tip name, type,
offset si size pentru fisierele SF
*/
typedef struct {
    char sect_name[9];
    uint32_t sect_type;
    uint32_t sect_offset;
    uint32_t sect_size;
} SectionHeader;

/*
declarari de functii (impelemtarile se afla dupa functia main)
*/
void displayVariant();
void displayHelp();
void listDirectory(const char *path, bool recursive, const char *sizeFilter, bool hasPermWrite);
void parseSF(const char* filePath);
bool isValidSectionType(uint32_t type);
void extractSF(const char* filePath, int sectionNr, int lineNr);
void listFiles(const char *path, bool isInitialCall);
bool isValidSFFile(const char* filePath);

/*
cu ajutorul strcmp imi verific parametrii de la argumtele programului unde argc este nr de argumente 
si argv este vectorul se caractere care stockeaza parametrii si unde argv[0] este ignorat deoarece 
acesta salveaza numele programului apoi verificarile se fac de argv[1] pana maxim la argv[5]
*/
int main(int argc, char *argv[]) {
    if (argc < 2) {
        displayHelp();
        return 1;
    }

    if (strcmp(argv[1], "variant") == 0) {
        displayVariant();
        return 0;
    } else if (strcmp(argv[1], "list") == 0) {
        bool recursive = false;
        const char *listpath = NULL;
        const char *sizeFilter = NULL;
        bool hasPermWrite = false;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "recursive") == 0) {
                recursive = true;
            } else if (strncmp(argv[i], "size_greater=", 13) == 0) {
                sizeFilter = argv[i] + 13;
            } else if (strcmp(argv[i], "has_perm_write") == 0) {
                hasPermWrite = true;
            } else if (strncmp(argv[i], "path=", 5) == 0) {
                listpath = argv[i] + 5;
            }
        }

        if (!listpath) {
            printf("ERROR\nno path\n");
            displayHelp();
            return -1;
        }
        
        printf("SUCCESS\n");
        listDirectory(listpath, recursive, sizeFilter, hasPermWrite);
        
    } else if (strcmp(argv[1], "parse") == 0) {
        if (argc == 3 && strncmp(argv[2], "path=", 5) == 0) {
            const char* parsePath = argv[2] + 5;
            parseSF(parsePath);
        } else {
            displayHelp();
            return -1;
        }
    } else if(strcmp(argv[1], "extract") == 0) {
        if (argc != 5) {
            displayHelp();
            return -1;
        }

        const char* extractPath = NULL;
        int sectionNr = 0, lineNr = 0;
        
        for (int i = 2; i < argc; i++) {
            if (strncmp(argv[i], "path=", 5) == 0) {
                extractPath = argv[i] + 5;
            } else if (strncmp(argv[i], "section=", 8) == 0) {
                sectionNr = atoi(argv[i] + 8);
            } else if (strncmp(argv[i], "line=", 5) == 0) {
                lineNr = atoi(argv[i] + 5);
            }
        }

        extractSF(extractPath, sectionNr, lineNr);
        
    } else if(strcmp(argv[1], "findall") == 0){
        if (argc == 3 && strncmp(argv[2], "path=", 5) == 0) {
        const char* findallPath = argv[2] + 5;
       
        listFiles(findallPath, true);
    } else {
        displayHelp();
        return -1;
    }
       
    }else{
         displayHelp();
        return -1; 
    }

    return 0;
}

/*
aici imi afisez varianta de tema cu ajutorul functiei printf
*/
void displayVariant() {
    printf("35896\n");
}

/*
aici am o functie ajutatoare cu care afisez utilizatorului ce comenzi poate folosi si cum trebuie scrise
*/
void displayHelp() {
    printf("./a1 [comand]\n");
    printf("comand:\n"
           "'variant' -- afiseaza varianta temei;\n"
           "'list [recursive] [size_greater=value] [has_perm_write] path=<dir_path>' -- listeaza fisierele si subdiectoarele conform criteriilor specificate;\n"
           "'parse path=<file_path>' -- verifica integritatea unui fisier SF;\n"
           "'extract path=<file_path> section=<sect_nr> line=<line_nr>' -- programul caute si sa afiseze o anumita portiune a fisierului SF;\n"
           "'findall path=<dir_path>' -- programul va afisa doar fisierele SF valide;\n"
          );
}

/*
in aceasta functie 4 parametrii pathul in sine daca este recursiva cautarea  si una dintre cele 2 filtre: filtrare dupa marimea fisierului sau dupa
permisiunea de scriere
1. folosesc dir pentru a deschide directorul si sa citesc datele daca nu afisez un mesaj de eroare
2. merg cu un while in caz ca vom avea parametru de recusrivitate true
3. if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue; acesta linie previne printarea directorului curect sau cel parinte
iar entry este structura care stockeaza numele directorului sau fisierului  if (stat(fullPath, &statbuf) == -1) continue; aceasta linie de cod incearca sa citeasca 
datele pentru statbuf si daca aceasta nu reuseste atunci se trece la urmatoarea iteratie
4. cu snprintf practic imi creez fullpathul lipind de pathul original numele directoarelor sau fisierelor
5. pentru filtru de size verific cu ajutorul lui statbuf(aceasta structura imi citeste informatiile despre director precum ar fi mode sau size) si verific daca sizeul este
mai mic sau egal cu ce valoare citesc de la argumente
6. tot cu ajutorul valori mode din statbuf verific daca are permisiune de scruiere sau nu
7. daca filtrele sunt corecte atunci printam toate directoarele sau fisierele care se incadreaza
8. daca avem recursivitate verificam daca este director pathul respectiv sau daca este fisier daca este director merge recursiv la urmatoarele directoare sau fisiere
9. la final se printeaza directoare in functie de necesitate
10. inchidem directorul dupa ce nu mai exista alte fisiere sau directoare care sa se printeze
*/
void listDirectory(const char *path, bool recursive, const char *sizeFilter, bool hasPermWrite) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char fullPath[1024];

    if (!(dir = opendir(path))) {
        fprintf(stderr, "ERROR\ninvalid directory path\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        if (stat(fullPath, &statbuf) == -1) continue;

        bool filterSize = true;
        if (sizeFilter) {
            long sizeThreshold = atol(sizeFilter);
            if (statbuf.st_size <= sizeThreshold) {
                filterSize = false;
            }
        }

        bool permWriteFilter = true;
        if (hasPermWrite && !(statbuf.st_mode & S_IWUSR)) {
            permWriteFilter = false;
        }

        if (filterSize && permWriteFilter) {
            printf("%s\n", fullPath);
        }

        if (recursive && S_ISDIR(statbuf.st_mode)) {
            listDirectory(fullPath, recursive, sizeFilter, hasPermWrite);
        }
    }

    closedir(dir);
}

/*
aceasta functie ajutatoare returneaza true daca section typeul apartine valorilor predefinite de cerinta altfel returneaza false
*/
bool isValidSectionType(uint32_t type) {
    switch(type) {
        case 72:
        case 52:
        case 49:
        case 51:
        case 64:
            return true;
        default:
            return false;
    }
}

/*
in aceasta functie de parseSF verific integritatea unui fisier sf dupa cerintele data in tema
1. cu ajutorul functiei de sistem open and read deschid si citesc datele din fisier
2. imi definesc un vector in care imi stockez cuvantul magic si doua unitati intregi fara semn de 8 biti unde pun valorile in functie de cum citesc fisierul
3. citesc cuvandul gaic si il verific daca este corect si daca are marimea potrivita daca nu afisez un mesaj de eroare
4. lseek(fd, HEADER_SIZE, SEEK_CUR); acesta linie de cod imi sare peste octetul cu valoarea headerului
5. citesc versiunea si o verific dupa valorile date in cerinta daca nu afisez un mesaj de eroare
6. imi verific no of section daca este 2 sau daca se incadreaza in intervalul 4-14 daca nu afisez un mesaj de eroare
7. in off_t imi stockez la ce pozitie sunt pentru a putea sa ne reintoarcem (offsetul de la inceputul fiserului pana in punctul curent)
8. dupa imi creez o structura in care citesc name, type, offset si size pentru a valida datele (folosesc isValidSectionType sa verific section typeul)
9. daca toate valorile sunt corecte puse in fisier ma intorc la pozitia anterioara si printez mesajul SUCCESS urmat de informatiile urmatoare:
version, nr_section, sect_name, sect_type, sect_offset si sect_size
10. La final inchidem directorul in care am lucrat
*/
void parseSF(const char* filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        printf("ERROR\ninvalid file\n");
        return;
    }

    char magic[MAGIC_LEN + 1] = {0};
    uint8_t version;
    uint8_t no_of_sections;

    if (read(fd, magic, MAGIC_LEN) != MAGIC_LEN || strncmp(magic, MAGIC,MAGIC_LEN) != 0) {
        printf("ERROR\nwrong magic\n");
        close(fd);
        return;
    }

    lseek(fd, HEADER_SIZE, SEEK_CUR);

    if (read(fd, &version, VERSION_SIZE) != 1 || version < VERSION_MIN || version > VERSION_MAX) {
        printf("ERROR\nwrong version\n");
        close(fd);
        return;
    }

    if (read(fd, &no_of_sections, NO_OF_SECTIONS_SIZE) != 1 || (no_of_sections != 2 && (no_of_sections < NO_OF_SECTIONS_MIN || no_of_sections > NO_OF_SECTIONS_MAX))) {
        printf("ERROR\nwrong sect_nr\n");
        close(fd);
        return;
    }

    off_t sectionsStart = lseek(fd, 0, SEEK_CUR);

    bool validTypes = true;
    for (int i = 0; i < no_of_sections; i++) {
        SectionHeader header;
        read(fd, header.sect_name, 8);
        read(fd, &header.sect_type, sizeof(header.sect_type));

        if (!isValidSectionType(header.sect_type)) {
            validTypes = false;
            printf("ERROR\nwrong sect_types\n");
            break;
        }

        lseek(fd, sizeof(header.sect_offset) + sizeof(header.sect_size), SEEK_CUR);
    }

    if (validTypes) {
        lseek(fd, sectionsStart, SEEK_SET);

        printf("SUCCESS\nversion=%u\nnr_sections=%u\n", version, no_of_sections);

        for (uint8_t i = 0; i < no_of_sections; i++) {
            SectionHeader header;
            read(fd, header.sect_name, 8);
            header.sect_name[8] = '\0'; 
            read(fd, &header.sect_type, sizeof(header.sect_type));
            read(fd, &header.sect_offset, sizeof(header.sect_offset));
            read(fd, &header.sect_size, sizeof(header.sect_size));

            printf("section%d: %s %u %u\n", i + 1, header.sect_name, header.sect_type, header.sect_size);
        }
    }

    close(fd);
}

/*
in functia de extractSF caut si afisez continutul unei linii dintro anumita sectiune
1. Deschis fisierul cu open si citesc datele cu ajutorul functiei read (ambele functii sunt functii de sistem)
2. lseek(fd, MAGIC_LEN + HEADER_SIZE + VERSION_SIZE, SEEK_SET); cu lseekul trec peste octetii lui magic, heder si version si intr-o variabila
unitate fara semn imi stockez no of sections si verific daca valoarea este valida sau nu
3. cu ajutorul structurii definite de mine imi citesc toate datele din fisier
4. dupa ce ce am retinut valoarea lui section o verificam daca apartine vectorului predefinit sau nu
5. dupa aceea aloc dinamic un vector cu sizul care reprezinta lungimea sirului si mai adaug 1 pentru carcaterul null
6. dupa aceea de la pozitia curecta a offsetului ma deplasez in fisier pentru a gasi linile si a salva continutul lor in vectorul alocat
7. folosesc strtok(sectionContent, "\n"); pentru a indeparta caracterul 0A(\n) si sa pun null intre linii
8. dupa ce am despartit sirul printam a catea linie dorim in functie de ce linie si sectie sa citit daca nu afisam un mesaj de eroare
9. dam free la vectorul alocat dinamic pentru a elimina memoria alocat si sa nu avem probleme de memory memory leak
10. inchidem fisierul dupa ce am terminat de lucru cu el
*/
void extractSF(const char* filePath, int sectionNr, int lineNr) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        printf("ERROR\ninvalid file\n");
        return;
    }

    lseek(fd, MAGIC_LEN + HEADER_SIZE + VERSION_SIZE, SEEK_SET);
    
    uint8_t no_of_sections;
    if (read(fd, &no_of_sections, NO_OF_SECTIONS_SIZE) != NO_OF_SECTIONS_SIZE) {
        printf("ERROR\ninvalid file\n");
        close(fd);
        return;
    }

    if (no_of_sections != 2 && (no_of_sections < NO_OF_SECTIONS_MIN || no_of_sections > NO_OF_SECTIONS_MAX)) {
        printf("ERROR\ninvalid section\n");
        close(fd);
        return;
    }

    SectionHeader sh;
    bool sectionFound = false;

    for (int i = 1; i <= no_of_sections; i++) {
        lseek(fd, 8, SEEK_CUR);
        if (read(fd, &sh.sect_type, sizeof(sh.sect_type)) != sizeof(sh.sect_type) ||
            read(fd, &sh.sect_offset, sizeof(sh.sect_offset)) != sizeof(sh.sect_offset) ||
            read(fd, &sh.sect_size, sizeof(sh.sect_size)) != sizeof(sh.sect_size)) {
            printf("ERROR\ninvalid file\n");
            close(fd);
            return;
        }

        if (i == sectionNr && isValidSectionType(sh.sect_type)) {
            sectionFound = true;
            break;
        }
    }

    if (!sectionFound) {
        printf("ERROR\ninvalid section\n");
        close(fd);
        return;
    }

    char* sectionContent = (char*)calloc(sh.sect_size + 1, sizeof(char));
    if (sectionContent == NULL) {
        printf("ERROR\nmemory allocation failed\n");
        close(fd);
        return;
    }

    lseek(fd, sh.sect_offset, SEEK_SET);
    if (read(fd, sectionContent, sh.sect_size) != sh.sect_size) {
        printf("ERROR\ninvalid file\n");
        free(sectionContent);
        close(fd);
        return;
    }

    char* lineNeeded = strtok(sectionContent, "\n");
    for (int currentLine = 1; currentLine < lineNr; ++currentLine) {
        if (lineNeeded != NULL) {
            lineNeeded = strtok(NULL, "\n");
        } else {
            break;
        }
    }

    if (lineNeeded != NULL) {
        printf("SUCCESS\n%s\n", lineNeeded);
    } else {
        printf("ERROR\ninvalid line\n");
    }

    free(sectionContent);
    close(fd);
}

/*
aceasta este o functie ajutatoare care imi verifica daca in fisierul meu exista macar o sectiune type cu valoarea 49 si returneaza true
daca nu returneaza false
am verificat cuvantul magic, header, si versiune dupa aceea mi-am luat nr of section si am mers cu un for de la 0 la < nr of section 
pentru a verifica daca exista macar o sectiune care sa aiba 49, in continuare validez restul fisierului
*/
bool isValidSFFile(const char* filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        return false;
    }

    char magic[MAGIC_LEN + 1] = {0};
    uint8_t version;
    uint8_t no_of_sections;

    if (read(fd, magic, MAGIC_LEN) != MAGIC_LEN || strncmp(magic, MAGIC,MAGIC_LEN) != 0) {
        close(fd);
        return false;
    }

    lseek(fd, HEADER_SIZE, SEEK_CUR);

    if (read(fd, &version, VERSION_SIZE) != 1 || version < VERSION_MIN || version > VERSION_MAX) {
        close(fd);
        return false;
    }

    if (read(fd, &no_of_sections, NO_OF_SECTIONS_SIZE) != 1 || (no_of_sections != 2 && (no_of_sections < NO_OF_SECTIONS_MIN || no_of_sections > NO_OF_SECTIONS_MAX))) {
        close(fd);
        return false;
    }

    int hasSectionType49  = 0;

    bool validTypes = true;
    for (int i = 0; i < no_of_sections; i++) {
        SectionHeader header;
        read(fd, header.sect_name, 8);
        read(fd, &header.sect_type, sizeof(header.sect_type));

        if (!isValidSectionType(header.sect_type)) {
            validTypes = false;
        }

        if(header.sect_type==49){
            hasSectionType49++;
        }

        lseek(fd, sizeof(header.sect_offset) + sizeof(header.sect_size), SEEK_CUR);
    }

    if (validTypes) {
        if(hasSectionType49){
            return true;
        }
    }

    close(fd);
    return false;

}

/*
functia listFiles listeaza toate fisierele care sunt sf si indeplinesc conditia de la isValidSFFile
1. cu dir imi deschid directorul si caut recursiv cu ajutorul whileului pentru a gasi toate fisierele corecte
2. cu ajutorul lui s_isdir si s_isreg verific daca pathul este director sau fisier normal daca este director merg mai departe 
recursiv pentru a gasi fisierele, daca nu este director atunci verific cu un if integritatea fisierului si printez
SUCCESS urmat de fisier
3. isInitialCall il folosesc pentru a printa doar o singura data cuvantul SUCCESS si sa pot continua cu cautarea
recursiva pentru fisiere
4. la final closedir inchidem directorul
*/
void listFiles(const char *path, bool isInitialCall) {
    DIR *dir = opendir(path);
    if (!dir) {
        printf("ERROR\ninvalid directory path\n");
        return;
    }

    if (isInitialCall) {
        printf("SUCCESS\n");
    }

    struct dirent *entry;
    char filePath[2048];
    struct stat statbuf;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);
        if (lstat(filePath, &statbuf) != 0) {
            continue;
        }

        if (S_ISREG(statbuf.st_mode) && isValidSFFile(filePath)) {
            printf("%s\n", filePath);
        } else if (S_ISDIR(statbuf.st_mode)) {
            listFiles(filePath, false);
        }
    }

    closedir(dir);
}

