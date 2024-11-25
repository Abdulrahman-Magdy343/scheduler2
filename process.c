#include "headers.h"
#include <signal.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>

#define SHM_KEY 12345 // Shared memory key

int remainingtime;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Error: Remaining time not provided\n");
        return 1;
    }

    remainingtime = atoi(argv[1]); // Remaining time from command-line argument

    // Attach to shared memory
    int shm_id = shmget(SHM_KEY, sizeof(int), 0666);
    if (shm_id == -1) {
        perror("Error: Failed to access shared memory");
        return 1;
    }

    int *shared_remaining_time = (int *)shmat(shm_id, NULL, 0);
    if (shared_remaining_time == (void *)-1) {
        perror("Error: Failed to attach shared memory");
        return 1;
    }

    initClk();

    while (remainingtime > 0) {
        sleep(1); // Simulate process execution
        remainingtime--;
        *shared_remaining_time = remainingtime; // Update shared memory
    }

    printf("Process %d finished execution.\n", getpid());

    // Notify the scheduler of process completion
    kill(getppid(), SIGUSR1);

    // Detach from shared memory
    shmdt(shared_remaining_time);

    destroyClk(false);
    return 0;
}
