#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>

#include "a2_helper.h"
#include <stdint.h>  // Correct header for intptr_t in C

#define NUM_THREADS_P3 4
#define NUM_THREADS_P8 6

sem_t sem_T3_1, sem_T8_1, sem_T8_5;
sem_t sem_T3_2_start, sem_T3_2_end;

void* thread_routine(void* arg);
void create_threads(int process_id, int num_threads);
void init_semaphores();
void destroy_semaphores();

int main() {
    init();
    info(BEGIN, 1, 0);

    init_semaphores();

    pid_t pid_2 = fork();
    if (pid_2 == 0) {
        info(BEGIN, 2, 0);

        pid_t pid_8 = fork();
        if (pid_8 == 0) {
            info(BEGIN, 8, 0);
            create_threads(8, NUM_THREADS_P8);
            info(END, 8, 0);
            exit(0);
        }

        wait(NULL);
        info(END, 2, 0);
        exit(0);
    }

    pid_t pid_3 = fork();
    if (pid_3 == 0) {
        info(BEGIN, 3, 0);
        create_threads(3, NUM_THREADS_P3);
        info(END, 3, 0);
        exit(0);
    }

    wait(NULL); // Wait for all child processes to finish
    destroy_semaphores();
    info(END, 1, 0);
    return 0;
}

void init_semaphores() {
    sem_init(&sem_T3_1, 0, 0);
    sem_init(&sem_T8_1, 0, 0);
    sem_init(&sem_T8_5, 0, 0);
    sem_init(&sem_T3_2_start, 0, 0);
    sem_init(&sem_T3_2_end, 0, 0);
}

void destroy_semaphores() {
    sem_destroy(&sem_T3_1);
    sem_destroy(&sem_T8_1);
    sem_destroy(&sem_T8_5);
    sem_destroy(&sem_T3_2_start);
    sem_destroy(&sem_T3_2_end);
}

void create_threads(int process_id, int num_threads) {
    pthread_t threads[num_threads];
    int thread_ids[num_threads];  // Store thread IDs and process ID information directly

    for (int i = 0; i < num_threads; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread_routine, (void*)(intptr_t)(process_id * 100 + thread_ids[i]));
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}

void* thread_routine(void* arg) {
    int full_id = (int)(intptr_t)arg;
    int process_id = full_id / 100;
    int thread_id = full_id % 100;

    if (process_id == 3) {
        switch (thread_id) {
            case 1:
                sem_wait(&sem_T8_1); // Wait for T8.1 to finish
                info(BEGIN, 3, thread_id);
                info(END, 3, thread_id);
                sem_post(&sem_T3_1); // Signal T8.5 can start
                break;
            case 2:
                sem_post(&sem_T3_2_start); // Allow T3.3 to start
                info(BEGIN, 3, thread_id);
                sem_wait(&sem_T3_2_end); // Wait for T3.3 to finish
                info(END, 3, thread_id);
                break;
            case 3:
                sem_wait(&sem_T3_2_start); // Wait for T3.2 to start
                info(BEGIN, 3, thread_id);
                info(END, 3, thread_id);
                sem_post(&sem_T3_2_end); // Signal T3.2 can finish
                break;
            case 4:
                info(BEGIN, 3, thread_id);
                info(END, 3, thread_id);
                break;
        }
    } else if (process_id == 8) {
        info(BEGIN, 8, thread_id);
        if (thread_id == 1) {
            info(END, 8, thread_id);
            sem_post(&sem_T8_1); // Signal T3.1 can start
        } else if (thread_id == 5) {
            sem_wait(&sem_T3_1); // Wait for T3.1 to finish
            info(END, 8, thread_id);
        } else {
            info(END, 8, thread_id);
        }
    }

    return NULL;
}

