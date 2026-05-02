#include <semaphore.h>

#ifndef PROJ2_
#define PROJ2_

typedef struct {
        int V;
        int N;
        int K;
        int TV;
        int TN;
        int O;
} Args;

typedef struct {
    Args args;
        int action_ct;
        int boarded_ct;
        int left_ct;
        int last_ride;
        int closed;
        sem_t mutex;
        sem_t mutex_w;
        sem_t next_cart;
        sem_t close_sem;
        sem_t boarding_start;
        sem_t boarding_done;
        sem_t exit_station;
        sem_t leaving_start;
        sem_t leaving_done;
} SharedMem;

void err_exit(char* msg);

SharedMem* shm_create();
int shm_init(SharedMem *shm);
void shm_destroy(SharedMem* shm);

#endif