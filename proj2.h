#include <semaphore.h>

#ifndef PROJ2_
#define PROJ2_

#define MAX_CARTS 40

typedef struct {
        int action_ct;
        int v_boarded_total;
        int trip;
        int v_remaining;
        int last_ride;
        int closed;
        sem_t mutex;
        sem_t mutex_w;
        sem_t boarding_ready;
        sem_t boarding_start;
        sem_t boarding_done;
        sem_t all_boarded;
        sem_t leaving_start;
        sem_t leaving_done;
        sem_t entry_station;
        sem_t exit_turn[MAX_CARTS];
} SharedMem;

void err_exit(char* msg);

SharedMem* shm_create();
int shm_init(SharedMem *shm, int V, int N);
void shm_destroy(SharedMem* shm, int V);
void safe_print(SharedMem* shm, const char* fmt, ...);

#endif