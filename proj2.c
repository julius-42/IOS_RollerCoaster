#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "proj2.h"


SharedMem *shm = NULL;
FILE *out = NULL; 


SharedMem* shm_create(){
    SharedMem *shm = mmap(NULL, sizeof(SharedMem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return shm;
}
 
int shm_init(SharedMem *shm){
    memset(shm, 0, sizeof(*shm));
    shm->action_ct = 0;
    shm->boarded_ct = 0;
    shm->left_ct = 0;
    shm->last_ride = 0;
    shm->closed = 0;
 
    if (sem_init(&shm->mutex, 1, 1) != 0) return -1;
    if (sem_init(&shm->mutex_w, 1, 1) != 0) return -1;
    if (sem_init(&shm->next_cart, 1, 0) != 0) return -1;
    if (sem_init(&shm->boarding_start, 1, 0) != 0) return -1;
    if (sem_init(&shm->boarding_done, 1, 0) != 0) return -1;
    if (sem_init(&shm->leaving_start, 1, 0) != 0) return -1;
    if (sem_init(&shm->leaving_done, 1, 0) != 0) return -1;
    if (sem_init(&shm->exit_station, 1, 1) != 0) return -1;
    if (sem_init(&shm->close_sem, 1, 0) != 0) return -1;
    return 0;
}

void shm_destroy(SharedMem* shm){
    if(shm == NULL | shm == MAP_FAILED){
        sem_destroy(&shm->mutex);
        sem_destroy(&shm->mutex_w);
        sem_destroy(&shm->next_cart);
        sem_destroy(&shm->boarding_start);
        sem_destroy(&shm->boarding_done);
        sem_destroy(&shm->leaving_start);
        sem_destroy(&shm->leaving_done);
        sem_destroy(&shm->exit_station);
        sem_destroy(&shm->close_sem);

        munmap(shm, sizeof(SharedMem));
    }
}


void dispatcher_process(SharedMem* shm){
    printf("dispatcher check\n");
    sem_wait(&shm->mutex);
    printf("hold done\n");
}

void cart_process(SharedMem* shm, int idx){
    printf("cart %d check\n", idx);
    sem_wait(&shm->mutex);
    printf("cart hold\n");
}

void visitor_process(SharedMem* shm, int idx){
    printf("visitor %d check\n", idx);
    printf("will increase sem\n");
    sem_post(&shm->mutex);
}

void cleanup(){
    shm_destroy(shm);
    if(out) fclose(out);
}

// parse argument to int and check validate format and range
void validate_arg(int low, int high, char* arg, int* out){
    if(sscanf(arg, "%d", out) != 1 || (low >= *out || *out >= high)){
        fprintf(stderr, "Argument(s) set incorrectly or out of range\n");
    }
}


int main(int argc, char* argv[]){

    int V, N, K, TV, TN, O;
    Args args;
    SharedMem* shm;

    // argument parsing and validation
    if(argc != 7){
        fprintf(stderr, "Arguments usage: %s V N K TV TN O\n", argv[0]);
        return 1;
    }

    // V - carts, N - visitors, K - cart size, TV - ride time, TN - queue entry time, O - cart delay
    validate_arg(0, 10, argv[1], &V);
    validate_arg(0, 10000, argv[2], &N);
    validate_arg(3, 41, argv[3], &K);
    validate_arg(-1, 1001, argv[4], &TV);
    validate_arg(-1, 1001, argv[5], &TN);
    validate_arg(0, 101, argv[6], &O);

    out = fopen("proj2.out", "w");
    if (!out) {
        perror("fopen proj2.out failed");
        return 1;
    }

    shm = shm_create();
    if(!shm){
        fclose(out);
        return 1;
    }
    if(shm_init(shm) != 0){
        perror("sem_init failed");
        cleanup();
        return 1;
    }

    pid_t pids[1 + args.V + args.N];
    int idx = 0;

    // create process for Dispatcher
    pids[idx] = fork();
    if(pids[idx] == 0){
        dispatcher_process(shm);
    }
    idx++;

    // create processes for Carts
    for(int i = 1; i <= args.V; i++){
        pids[idx] = fork();
        if(pids[idx] == 0){
            cart_process(shm, i);
        }
        idx++;
    }

    // create processes for Visitors
    for(int i = 1; i <= args.N; i++){
        pids[idx] = fork();
        if(pids[idx] == 0){
            visitor_process(shm, i);
        }
        idx++;
    }

    // wait for all processes to end
    while(wait(NULL) > 0);

    cleanup();
    return 0;
}