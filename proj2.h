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
        int closed;
        int boarded_ct;
        int left_ct;
        int last_ride;
        sem_t mutex;
        sem_t mutex_w;
        sem_t boarding_start;
        sem_t boarding_end;
        sem_t cart_queue;
        sem_t cart_left;
        sem_t leaving_start;
        sem_t leaving_end;
        sem_t exit_station;
} SharedMem;

void err_exit(char* msg);

sem_t* sem_create(int val);
void sem_hold(sem_t *sem);
void sem_signal(sem_t *sem);
void sems_destroy(SharedMem* shm);

#endif