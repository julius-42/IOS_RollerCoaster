#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include "proj2.h"

// print error message, clean up mem and exit program with code 1


void cleanup(SharedMem* shm, int shm_id){
    if(shm != NULL){
        sems_destroy(shm);
    }
}

void err_exit(char* msg){
    fprintf(stderr, "%s", msg);
    //clean up
    exit(1);
}

void dispatcher_process(SharedMem* shm){
    printf("dispatcher check\n");
    sem_hold(&shm->mutex);
    printf("hold done\n");
}

void cart_process(SharedMem* shm, int idx){
    printf("cart %d check\n", idx);
    sem_hold(&shm->mutex);
    printf("cart hold\n");
}

void visitor_process(SharedMem* shm, int idx){
    printf("visitor %d check\n", idx);
    printf("will increase sem\n");
    sem_signal(&shm->mutex);
}


// semaphore functions with error checking
sem_t* sem_create(int val){
    sem_t* sem;
    int n = sem_init(sem, 0, val);
    if(n == -1) err_exit("sem_init failed\n");
    return sem;
}

void sem_hold(sem_t* sem){
    int n = sem_wait(sem);
    if(n != 0) err_exit("sem_wait failed\n");
}

void sem_signal(sem_t* sem){
    int n = sem_post(sem);
    if(n != 0) err_exit("sem_post failed\n");
}

void sems_destroy(SharedMem* shm){
    if(!shm) return;

    if(sem_destroy(&shm->mutex) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->mutex_w) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->boarding_start) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->boarding_end) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->leaving_start) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->leaving_end) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->cart_queue) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->cart_left) != 0) fprintf(stderr, "sem_destroy failed");
    if(sem_destroy(&shm->exit_station) != 0) fprintf(stderr, "sem_destroy failed");
}

// parse argument to int and check validate format and range
void validate_arg(int low, int high, char* arg, int* out){
    if(sscanf(arg, "%d", out) != 1 || (low >= *out || *out >= high)){
        err_exit("Argument is set incorrectly or out of range\n");
    }
}


int main(int argc, char* argv[]){

    Args args;
    SharedMem* shm;

    // argument parsing and validation
    if(argc != 7){
        fprintf(stderr,"Wrong format of arguments\n");
        return 1;
    }

    // V - carts, N - visitors, K - cart size, TV - ride time, TN - queue entry time, O - cart delay
    validate_arg(0, 10, argv[1], &args.V);
    validate_arg(0, 10000, argv[2], &args.N);
    validate_arg(3, 41, argv[3], &args.K);
    validate_arg(-1, 1001, argv[4], &args.TV);
    validate_arg(-1, 1001, argv[5], &args.TN);
    validate_arg(0, 101, argv[6], &args.O);

    // initialize shared memory segment
    shm = mmap(NULL, sizeof(SharedMem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    shm->action_ct = 0;
    shm->boarded_ct = 0;
    shm->left_ct = 0;
    shm->closed = 0;
    shm->last_ride = 0;
    shm->mutex = *sem_create(1);

    pid_t pids[1 + args.V + args.N];
    int idx = 0;

    // create process for Dispatcher
    pids[idx] = fork();
    if(pids[idx] == 0){
            dispatcher_process(shm);
            exit(0);
    }
    idx++;

    // create processes for Carts
    for(int i = 1; i <= args.V; i++){
            pids[idx] = fork();
            if(pids[idx] == 0){
                    cart_process(shm, i);
                exit(0);
    }
            idx++;
    }

    // create processes for Visitors
    for(int i = 1; i <= args.N; i++){
            pids[idx] = fork();
            if(pids[idx] == 0){
                    visitor_process(shm, i);
        exit(0);
            }
            idx++;
    }

    // wait for all processes to end
    while(wait(NULL) > 0);

    return 0;
}