#include "headers.h"

#define MAX_PROCESSES 100
#define SHM_KEY 12345 // Key for shared memory

struct Metrics process_metrics[MAX_PROCESSES];
int metrics_count = 0; // Number of completed processes

// CPU Process Control Block structure
struct CPUproc {
    int id;
    int arrivaltime;
    int runningtime;
    int priority;
    int remainingtime;
    int state; // 0: ready, 1: running, 2: finished
    int waitingtime;
    int finishtime;
};

// Global Variables
int msgid_processes;         // Message queue ID for process generator
Queue *ready_queue;          // Queue for ready processes
volatile int completedProcessID = -1; // Process ID of the completed process (set by signal handler)
int algorithmChoice;         // Chosen scheduling algorithm

// Shared Memory Variables
int shm_id;                  // Shared memory ID
int *shared_remaining_time;  // Pointer to shared memory for remaining time

// Function Prototypes
void listenForProcessMessages();
void handleSignal(int sig, siginfo_t *info, void *context);
void logProcess(struct CPUproc *, const char *);
void cleanup(int);
void scheduleProcesses();
void forkProcess(struct CPUproc process);

// Scheduling Algorithms
void shortJobFirst();
void roundRobin(int timeQuantum);

int main() {
    ready_queue = createQueue(); // Initialize the ready queue
    signal(SIGINT, cleanup);

    // Set up the signal handler for process completion notifications
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handleSignal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error: Failed to set up signal handler");
        exit(EXIT_FAILURE);
    }

    // Create shared memory for remaining time
    shm_id = shmget(SHM_KEY, sizeof(int), 0666 | IPC_CREAT);
    if (shm_id == -1) {
        perror("Error: Failed to create shared memory");
        exit(EXIT_FAILURE);
    }

    // Attach shared memory
    shared_remaining_time = (int *)shmat(shm_id, NULL, 0);
    if (shared_remaining_time == (void *)-1) {
        perror("Error: Failed to attach shared memory");
        exit(EXIT_FAILURE);
    }

    // Create the message queue for process generator communication
    key_t key_processes = ftok("processes.txt", 'B');
    if (key_processes == -1) {
        perror("Error: Failed to generate key using ftok");
        exit(EXIT_FAILURE);
    }

    msgid_processes = msgget(key_processes, 0666 | IPC_CREAT);
    if (msgid_processes == -1) {
        perror("Error: Failed to create message queue for processes");
        exit(EXIT_FAILURE);
    }

    // Main polling loop
    while (1) {
        listenForProcessMessages();  // Check for new processes
        scheduleProcesses();        // Arrange and fork processes based on scheduling algorithm
        printf("Shared remaining time: %d\n", *shared_remaining_time); // Monitor shared memory
        usleep(100000);             // Prevent busy waiting (100 ms delay)
    }

    cleanup(0);
    return 0;
}

void listenForProcessMessages() {
    struct CPUproc proc;

    // Non-blocking receive from the process generator message queue
    if (msgrcv(msgid_processes, &proc, sizeof(struct processData), 1, IPC_NOWAIT) == -1) {
        // No new messages, continue
        return;
    }

    // Create a new CPUproc for the received process
    struct CPUproc process;
    process.id = proc.id;
    process.arrivaltime = proc.arrivaltime;
    process.runningtime = proc.runningtime;
    process.priority = proc.priority;
    process.remainingtime = proc.runningtime;
    process.state = 0; // Ready state
    process.waitingtime = 0;
    process.finishtime = 0;

    enqueue(ready_queue, process);

    printf("Received process ID: %d, Arrival Time: %d, Running Time: %d, Priority: %d\n",
           process.id, process.arrivaltime, process.runningtime, process.priority);

    logProcess(&process, "received");
}

void scheduleProcesses() {
    // Arrange the processes in the ready queue based on the selected algorithm
    switch (algorithmChoice) {
        case 1:
            shortJobFirst();
            break;
        case 2:
            preemptiveHighestPriorityFirst();
            break;
        case 3:
            roundRobin();
            break;
        default:
            printf("Invalid scheduling algorithm choice.\n");
            cleanup(0);
    }

    // Fork and execute processes
    while (!isEmpty(ready_queue)) {
        struct CPUproc process = dequeue(ready_queue);
        forkProcess(process);
    }
}
void listenForProcessMessages() {
    struct processData msg;

    // Non-blocking receive from the process generator message queue
    if (msgrcv(msgid_processes, &msg, sizeof(struct processData), 0, IPC_NOWAIT) == -1) {
        // No new messages, continue
        return;
    }

    if (msg.id == -1) { // Special message containing the algorithm choice
        printf("Received algorithm choice: %d\n", msg.priority);
        algorithmChoice = msg.priority; // Update the global algorithm choice
        switch (algorithmChoice) {
            case 1:
                printf("Selected Shortest Job First (SJF)\n");
                break;
            case 2:
                printf("Selected Round Robin (RR)\n");
                break;
            case 3:
                printf("Selected Priority Scheduling\n");
                break;
            default:
                printf("Invalid algorithm choice\n");
                cleanup(0);
        }
    } else {
        // Create a new PCB for the received process
        struct CPUproc process;
        process.id = msg.id;
        process.arrivaltime = msg.arrivaltime;
        process.runningtime = msg.runningtime;
        process.priority = msg.priority;
        process.remainingtime = msg.runningtime;
        process.state = 0; // Ready state
        process.waitingtime = 0;
        process.finishtime = 0;

        enqueue(ready_queue, process);

        printf("Received process ID: %d, Arrival Time: %d, Running Time: %d, Priority: %d\n",
               process.id, process.arrivaltime, process.runningtime, process.priority);

        logProcess(&process, "received");
    }
}

pid_t forkProcess(struct CPUproc process, int isRoundRobin = 0, int timeQuantum = 0) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Error: Failed to fork process");
        return -1; // Return -1 to indicate an error
    }
    else return pid;
}



void handleSignal(int sig, siginfo_t *info, void *context) {
    if (sig == SIGUSR1) {
        printf("Process completed. Shared remaining time: %d\n", *shared_remaining_time);
    }
}

void logProcess(struct CPUproc *process, const char *action) {
    FILE *logFile = fopen("scheduler.log", "a");
    if (logFile == NULL) {
        perror("Error: Failed to open log file");
        return;
    }

    fprintf(logFile, "At time %d process %d %s arrival %d total %d remain %d priority %d\n",
            getClk(), process->id, action, process->arrivaltime,
            process->runningtime, process->remainingtime, process->priority);

    // Log metrics when the process finishes
    if (strcmp(action, "finished") == 0) {
        int turnaround_time = process->finishtime - process->arrivaltime;
        int waiting_time = turnaround_time - process->runningtime;
        float weighted_turnaround_time = (float)turnaround_time / process->runningtime;

        process_metrics[metrics_count++] = (struct Metrics) {
            .id = process->id,
            .turnaround_time = turnaround_time,
            .weighted_turnaround_time = weighted_turnaround_time,
            .waiting_time = waiting_time
        };
    }

    fclose(logFile);
}


void cleanup(int signum) {
    // Calculate final statistics
    int total_turnaround_time = 0, total_waiting_time = 0;
    float total_weighted_turnaround_time = 0.0;
    float sum_squared_turnaround = 0.0;

    for (int i = 0; i < metrics_count; i++) {
        total_turnaround_time += process_metrics[i].turnaround_time;
        total_waiting_time += process_metrics[i].waiting_time;
        total_weighted_turnaround_time += process_metrics[i].weighted_turnaround_time;
        sum_squared_turnaround += pow(process_metrics[i].turnaround_time, 2);
    }

    float avg_turnaround_time = (float)total_turnaround_time / metrics_count;
    float avg_waiting_time = (float)total_waiting_time / metrics_count;
    float avg_weighted_turnaround_time = total_weighted_turnaround_time / metrics_count;

    float std_dev_turnaround_time = sqrt(sum_squared_turnaround / metrics_count - pow(avg_turnaround_time, 2));

    // Write metrics to output file
    FILE *outputFile = fopen("metrics_output.txt", "w");
    if (outputFile == NULL) {
        perror("Error: Failed to open metrics output file");
        return;
    }

    fprintf(outputFile, "Average Turnaround Time: %.2f\n", avg_turnaround_time);
    fprintf(outputFile, "Average Waiting Time: %.2f\n", avg_waiting_time);
    fprintf(outputFile, "Average Weighted Turnaround Time: %.2f\n", avg_weighted_turnaround_time);
    fprintf(outputFile, "Standard Deviation of Turnaround Time: %.2f\n", std_dev_turnaround_time);
    fclose(outputFile);

    printf("Metrics written to metrics_output.txt\n");

    // Clean up resources
    if (msgid_processes != -1) {
        msgctl(msgid_processes, IPC_RMID, NULL);
    }
    shmdt(shared_remaining_time);
    shmctl(shm_id, IPC_RMID, NULL);

    printf("Scheduler shutting down. Resources cleared.\n");
    exit(EXIT_SUCCESS);
}


void shortestJobFirst()
{
    struct CPUproc p;
    while (!isEmpty(readyQueue))
    {
        p = dequeue(readyQueue);
        t_pid pid = forkProcess(p);
        if (pid == 0) {
            // Child process: Execute the process.c program
            char remainingTimeStr[10];
            char roundRobinFlag[10];
            char quantumStr[10];

            snprintf(remainingTimeStr, sizeof(remainingTimeStr), "%d", process.remainingtime);
            snprintf(roundRobinFlag, sizeof(roundRobinFlag), "%d", 0); // 1 for Round Robin, 0 otherwise
            snprintf(quantumStr, sizeof(quantumStr), "%d", 0);

            system("gcc process.c -o process"); // Compile process.c
            execlp("./process", "./process", remainingTimeStr, roundRobinFlag, quantumStr, NULL);
            perror("Error: Failed to execute process");
            exit(EXIT_FAILURE);

        } 
        else 
        {
            // Parent process: Log the forked process
            printf("Forked process ID: %d\n", pid);
            logProcess(&process, "started");

            // Initialize shared memory with the remaining time
            *shared_remaining_time = process.remainingtime;
        }    
    }
}

void roundRobin(int timeQuantum) {
    while (!isEmpty(ready_queue)) {
        struct CPUproc process = dequeue(ready_queue);

        // Fork or resume the process
        if (process.state == 0) { // New process
            process.state = 1; // Running state
            process.pid = fork();
        } else if (process.state == 1) { // Previously paused process
            printf("Resuming process %d\n", process.id);
            kill(process.pid, SIGCONT);
        }

        // Let the process run for the time quantum or remaining time
        int timeToRun = (process.remainingtime > timeQuantum) ? timeQuantum : process.remainingtime;
        int start_time = getClk();

        while (getClk() - start_time < timeToRun) {
            // Busy wait to simulate process running for the time quantum
        }

        if (pid == 0) {
            // Child process: Execute the process.c program
            char remainingTimeStr[10];
            char roundRobinFlag[10];
            char quantumStr[10];

            snprintf(remainingTimeStr, sizeof(remainingTimeStr), "%d", process.remainingtime);
            snprintf(roundRobinFlag, sizeof(roundRobinFlag), "%d", 1); // 1 for Round Robin, 0 otherwise
            snprintf(quantumStr, sizeof(quantumStr), "%d", timeQuantum);

            system("gcc process.c -o process"); // Compile process.c
            execlp("./process", "./process", remainingTimeStr, roundRobinFlag, quantumStr, NULL);
            perror("Error: Failed to execute process");
            exit(EXIT_FAILURE);

        } 
        else 
        {
            // Parent process: Log the forked process
            printf("Forked process ID: %d\n", pid);
            logProcess(&process, "started");

            // Initialize shared memory with the remaining time
            *shared_remaining_time = process.remainingtime;
        }

        if (process.remainingtime > 0) {
            printf("Pausing process %d with remaining time %d\n", process.id, process.remainingtime);
            process.state = 1; // Update state to paused
            kill(process.pid, SIGSTOP); // Pause the process
            enqueue(ready_queue, process); // Requeue the process
        } else {
            printf("Process %d completed execution.\n", process.id);
            process.state = 2; // Finished
            logProcess(&process, "finished");
            kill(process.pid, SIGUSR1); // Notify the process of completion
        }
    }
}

void preemptiveHighestPriorityFirst() {
    int size = queueSize(ready_queue); // Get the size of the queue

    struct CPUproc processes[size];

    // Dequeue all processes to sort them
    for (int i = 0; i < size; i++) {
        processes[i] = dequeue(ready_queue);
    }

    // Sort the array based on priority
    qsort(processes, size, sizeof(struct CPUproc), comparePriority);

    // Enqueue the processes back to the queue after sorting
    for (int i = 0; i < size; ++i) {
        enqueue(ready_queue, processes[i]);
    }

    struct CPUproc *current_process = NULL;
    pid_t current_pid = -1; // Track the PID of the currently running process

    while (!isEmpty(ready_queue)) {
        struct CPUproc next_process = dequeue(ready_queue);

        // If there's no current process, fork the next one
        if (current_process == NULL) {
            current_process = &next_process;
            current_pid = fork();

            if (current_pid == 0) {
                // Child process: Execute process.c
                char remainingTimeStr[10];
                snprintf(remainingTimeStr, sizeof(remainingTimeStr), "%d", current_process->remainingtime);

                execlp("./process", "./process", remainingTimeStr, "0", "0", NULL);
                perror("Error: Failed to execute process");
                exit(EXIT_FAILURE);
            }

            // Parent process
            printf("Process with ID %d is now running.\n", current_process->id);
            *shared_remaining_time = current_process->remainingtime; // Write to shared memory
        } else {
            // Check if the next process has a higher priority (lower number)
            if (next_process.priority < current_process->priority) {
                printf("Preempting process %d with process %d\n", current_process->id, next_process.id);

                // Suspend current process and re-enqueue it
                kill(current_pid, SIGSTOP);
                enqueue(ready_queue, *current_process);

                // Run the next process
                current_process = &next_process;
                current_pid = fork();

                if (current_pid == 0) {
                    // Child process: Execute process.c
                    char remainingTimeStr[10];
                    snprintf(remainingTimeStr, sizeof(remainingTimeStr), "%d", current_process->remainingtime);
                    system("gcc process.c -o process"); // Compile process.c
                    execlp("./process", "./process", remainingTimeStr, "0", "0", NULL);
                    perror("Error: Failed to execute process");
                    exit(EXIT_FAILURE);
                }

                // Parent process
                printf("Process with ID %d is now running.\n", current_process->id);
                *shared_remaining_time = current_process->remainingtime; // Write to shared memory
            } else {
                // Re-enqueue the next process if it has lower priority
                enqueue(ready_queue, next_process);
            }
        }

        // Wait for the current process to complete or be preempted
        pause();

        // Clear the current process if it has finished
        if (*shared_remaining_time == 0) {
            printf("Process with ID %d has finished.\n", current_process->id);
            logProcess(current_process, "finished");
            current_process = NULL;
            current_pid = -1;
        }
    }
}
