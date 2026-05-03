#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "proj2.h"


SharedMem* shm = NULL;
FILE* out = NULL; 


SharedMem* shm_create(){
    SharedMem *shm = mmap(NULL, sizeof(SharedMem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return shm;
}
 
int shm_init(SharedMem *shm, int V){
    memset(shm, 0, sizeof(*shm));
    shm->action_ct = 0;
    shm->boarded_ct = 0;
    shm->left_ct = 0;
    shm->last_ride = 0;
    shm->closed = 0;
 
    if (sem_init(&shm->mutex, 1, 1) != 0) return -1;
    if (sem_init(&shm->mutex_w, 1, 1) != 0) return -1;
    if (sem_init(&shm->boarding_ready, 1, 0) != 0) return -1;
    if (sem_init(&shm->boarding_start, 1, 0) != 0) return -1;
    if (sem_init(&shm->boarding_done, 1, 0) != 0) return -1;
    if (sem_init(&shm->leaving_start, 1, 0) != 0) return -1;
    if (sem_init(&shm->leaving_done, 1, 0) != 0) return -1;
    if (sem_init(&shm->entry_station, 1, 0) != 0) return -1;

    for(int i = 0; i < V; i++){
        if (sem_init(&shm->exit_turn[i], 1, (i == 0))) return -1;
    }
    return 0;
}

void shm_destroy(SharedMem* shm, int V){
    if(shm == NULL | shm == MAP_FAILED){
        sem_destroy(&shm->mutex);
        sem_destroy(&shm->mutex_w);
        sem_destroy(&shm->boarding_ready);
        sem_destroy(&shm->boarding_start);
        sem_destroy(&shm->boarding_done);
        sem_destroy(&shm->leaving_start);
        sem_destroy(&shm->leaving_done);
        sem_destroy(&shm->entry_station);
        
        for(int i = 0; i < V; i++){
            sem_destroy(&shm->exit_turn[i]);
        }

        munmap(shm, sizeof(SharedMem));
    }
}


void dispatcher_process(SharedMem* shm, int V, int N, int K, int TV, int TN, int O){
    
    sem_wait(&shm->mutex);
    printf("%d: D: started\n", shm->action_ct);
    shm->action_ct++;
    sem_post(&shm->mutex);

    int unserved = N - shm->v_done;

    while(unserved > 0){

        int trip;
        if(shm->v_queued > K){
            trip = K;
        }
        else{
            trip = shm->v_queued;
        }

        sem_wait(&shm->mutex);
        printf("%d: D: next cart\n", shm->action_ct);
        shm->trip = trip;
        shm->action_ct++;
        sem_post(&shm->mutex);

        sem_post(&shm->boarding_ready);
        
        sem_wait(&shm->boarding_done);
        
        usleep(O);
    
        unserved = N - shm->v_done;
    }

    sem_wait(&shm->mutex);
    printf("%d: D: closing\n", shm->action_ct);
    shm->action_ct++;
    shm->closed = 1;
    shm->last_ride = 1;
    sem_post(&shm->mutex);

    exit(0);
}

void cart_process(SharedMem* shm, int idx, int V, int N, int K, int TV, int TN, int O){
    
    sem_wait(&shm->mutex);
    printf("%d: V %d: started\n", shm->action_ct, idx);
    shm->action_ct++;
    sem_post(&shm->mutex);

    sem_post(&shm->entry_station);

    while(!shm->closed){
        sem_wait(&shm->boarding_ready);

        sem_wait(&shm->mutex);
        printf("%d: V %d: boarding started\n", shm->action_ct, idx);
        shm->action_ct++;
        sem_post(&shm->mutex);

        for(int i = 0; i < shm->trip; i++){
            sem_post(&shm->boarding_start);
        }

        sem_wait(&shm->boarding_done);
        int ride_time = rand() % (2*TV + 1 - TV) + TV;
        usleep(ride_time);
    }

    exit(0);
}

void visitor_process(SharedMem* shm, int idx, int V, int N, int K, int TV, int TN, int O){
    
    sem_wait(&shm->mutex);
    printf("%d: N %d: started\n", shm->action_ct, idx);
    shm->action_ct++;
    sem_post(&shm->mutex);

    int delay = rand() % (TN + 1 - 0) + 0;
    usleep(delay);
    
    sem_wait(&shm->mutex);
    printf("%d: N %d: queue\n", shm->action_ct, idx);
    shm->v_queued++;
    shm->v_done++;
    shm->action_ct++;
    sem_post(&shm->mutex);

    sem_wait(&shm->boarding_start);
    sem_wait(&shm->mutex);
    printf("%d: N %d: boarding\n", shm->action_ct, idx);
    shm->action_ct++;
    shm->boarded_ct++;
    shm->v_queued--;
    sem_post(&shm->mutex);

    if(shm->boarded_ct == shm->trip){
        sem_post(&shm->boarding_done);
    }

    exit(0);
}

void cleanup(int V){
    shm_destroy(shm,V);
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
    SharedMem* shm;
    out = stdout;

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

    // out = fopen("proj2.out", "w");
    // if (!out) {
    //     perror("fopen proj2.out failed");
    //     return 1;
    // }

    shm = shm_create();
    if(!shm){
        fclose(out);
        return 1;
    }
    if(shm_init(shm, V) != 0){
        perror("sem_init failed");
        cleanup(V);
        return 1;
    }


    // create process for Dispatcher
    pid_t pid = fork();
    if (pid < 0) {perror("fork dispatcher failed"); cleanup(V); return 1;}
    if(pid == 0){
        dispatcher_process(shm,V,N,K,TV,TN,O);
    }

    // create processes for Carts
    for(int i = 1; i <= V; i++){
        pid = fork();
        if (pid < 0) {perror("fork cart failed"); cleanup(V); return 1;}
        if(pid == 0){
            cart_process(shm, i,V,N,K,TV,TN,O);
        }
    }

    // create processes for Visitors
    for(int i = 1; i <= N; i++){
        pid = fork();
        if (pid < 0) {perror("fork visitor failed"); cleanup(V); return 1;}
        if(pid == 0){
            visitor_process(shm, i,V,N,K,TV,TN,O);
        }
    }

    // wait for all processes to end
    while(wait(NULL) > 0);

    cleanup(V);
    return 0;
}