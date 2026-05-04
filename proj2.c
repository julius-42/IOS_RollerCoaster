#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "proj2.h"

FILE* out = NULL;

SharedMem* shm_create(){
    SharedMem *shm = mmap(NULL, sizeof(SharedMem), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    return shm;
}
 
int shm_init(SharedMem *shm, int V, int N){
    memset(shm, 0, sizeof(*shm));
    shm->action_ct = 0;
    shm->last_ride = 0;
    shm->v_remaining = N;
    shm->closed = 0;
 
    if (sem_init(&shm->mutex, 1, 1) != 0) return -1;
    if (sem_init(&shm->mutex_w, 1, 1) != 0) return -1;
    if (sem_init(&shm->boarding_ready, 1, 0) != 0) return -1;
    if (sem_init(&shm->boarding_start, 1, 0) != 0) return -1;
    if (sem_init(&shm->boarding_done, 1, 0) != 0) return -1;
    if (sem_init(&shm->all_boarded, 1, 0) != 0) return -1;
    if (sem_init(&shm->leaving_start, 1, 0) != 0) return -1;
    if (sem_init(&shm->leaving_done, 1, 0) != 0) return -1;
    if (sem_init(&shm->entry_station, 1, 0) != 0) return -1;

    for(int i = 0; i < V; i++){
        if (sem_init(&shm->exit_turn[i], 1, (i == 0))) return -1;
    }
    return 0;
}

void shm_destroy(SharedMem* shm, int V){
    if(shm == NULL | shm == MAP_FAILED) return;

    sem_destroy(&shm->mutex);
    sem_destroy(&shm->mutex_w);
    sem_destroy(&shm->boarding_ready);
    sem_destroy(&shm->boarding_start);
    sem_destroy(&shm->boarding_done);
    sem_destroy(&shm->all_boarded);
    sem_destroy(&shm->leaving_start);
    sem_destroy(&shm->leaving_done);
    sem_destroy(&shm->entry_station);
    
    for(int i = 0; i < V; i++){
        sem_destroy(&shm->exit_turn[i]);
    }

    munmap(shm, sizeof(SharedMem));
}


void dispatcher_process(SharedMem* shm, int V, int N, int K, int TV, int TN, int O){

    safe_print(shm, "D: started\n");

    while(1){

        sem_wait(&shm->mutex);
        int remaining = N - shm->v_boarded_total;
        sem_post(&shm->mutex);
 
        if (remaining <= 0)
            break;
 
        int trip = (remaining >= K) ? K : remaining;

        
        sem_wait(&shm->mutex);
        shm->trip = trip;
        shm->last_ride = (remaining <= K) ? 1 : 0;
        sem_post(&shm->mutex);

        sem_wait(&shm->entry_station);
        
        safe_print(shm, "D: next cart\n");
        
        sem_post(&shm->boarding_ready);
        sem_wait(&shm->all_boarded);

        usleep(O);
    }
    
    safe_print(shm, "D: closing\n");
    
    sem_wait(&shm->mutex);
    shm->closed = 1;
    sem_post(&shm->mutex);
    
    for (int i = 0; i < V; i++){
        sem_post(&shm->boarding_ready);
    }
    
    exit(0);
}

void cart_process(SharedMem* shm, int idx, int V, int N, int K, int TV, int TN, int O){
    
    safe_print(shm, "V %d: started\n", idx);

    sem_post(&shm->entry_station);

    while(1){
        sem_wait(&shm->boarding_ready);

        sem_wait(&shm->mutex);
        int closed = shm->closed;
        int trip   = shm->trip;
        sem_post(&shm->mutex);

        if(closed) break;

        safe_print(shm, "V %d: boarding started\n", idx);

        for(int i = 0; i < trip; i++){
            sem_post(&shm->boarding_start);
        }

        for(int i = 0; i < trip; i++){
            sem_wait(&shm->boarding_done);
        }

        safe_print(shm, "V %d: boarding complete\n", idx);
        sem_post(&shm->all_boarded);

        int ride_time = rand() % (2*TV + 1 - TV) + TV;
        usleep(ride_time);

        //fprintf(out, "%d: ride done\n",idx);
        sem_wait(&shm->exit_turn[idx-1]);

        safe_print(shm, "V %d: leaving started\n", idx);

        for(int i = 0; i < trip; i++){
            sem_post(&shm->leaving_start);
        }

        for(int i = 0; i < trip; i++){
            sem_wait(&shm->leaving_done);
        }

        safe_print(shm, "V %d: leaving complete\n", idx);

        sem_post(&shm->exit_turn[idx % V]);
 
        sem_post(&shm->entry_station);
    }

    safe_print(shm, "V %d: closed\n", idx);
    exit(0);
}

void visitor_process(SharedMem* shm, int idx, int V, int N, int K, int TV, int TN, int O){
    
    safe_print(shm, "N %d: started\n", idx);

    int delay = rand() % (TN + 1 - 0) + 0;
    usleep(delay);
    
    safe_print(shm, "N %d: queue\n", idx);

    sem_wait(&shm->boarding_start);

    safe_print(shm, "N %d: boarding\n", idx);

    sem_wait(&shm->mutex);
    shm->v_boarded_total++;
    sem_post(&shm->mutex);

    sem_post(&shm->boarding_done);

    sem_wait(&shm->leaving_start);
    safe_print(shm, "N %d: leaving\n", idx);
    sem_post(&shm->leaving_done);

    exit(0);
}

void cleanup(SharedMem* shm, int V){
    shm_destroy(shm,V);
    if(out) fclose(out);
}

void safe_print(SharedMem* shm, const char *fmt, ...)
{
    va_list ap;

    sem_wait(&shm->mutex_w);

    shm->action_ct++;
    fprintf(out, "%d: ", shm->action_ct);

    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);

    fflush(out);

    sem_post(&shm->mutex_w);
}

// parse argument to int and check validate format and range
void validate_arg(int low, int high, char* arg, int* out){
    if(sscanf(arg, "%d", out) != 1 || (low >= *out || *out >= high)){
        fprintf(stderr, "Argument(s) set incorrectly or out of range\n");
    }
}


int main(int argc, char* argv[]){

    int V, N, K, TV, TN, O;
    SharedMem* shm = NULL;
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
    if(shm_init(shm, V, N) != 0){
        perror("sem_init failed");
        cleanup(shm, V);
        return 1;
    }


    // create process for Dispatcher
    pid_t pid = fork();
    if (pid < 0) {perror("fork dispatcher failed"); cleanup(shm, V); return 1;}
    if(pid == 0){
        dispatcher_process(shm,V,N,K,TV,TN,O);
    }

    // create processes for Carts
    for(int i = 1; i <= V; i++){
        pid = fork();
        if (pid < 0) {perror("fork cart failed"); cleanup(shm, V); return 1;}
        if(pid == 0){
            cart_process(shm, i,V,N,K,TV,TN,O);
        }
    }

    // create processes for Visitors
    for(int i = 1; i <= N; i++){
        pid = fork();
        if (pid < 0) {perror("fork visitor failed"); cleanup(shm, V); return 1;}
        if(pid == 0){
            visitor_process(shm, i,V,N,K,TV,TN,O);
        }
    }

    // wait for all processes to end
    while(wait(NULL) > 0);

    cleanup(shm, V);
    return 0;
}