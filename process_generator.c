#include "headers.h"
#include "Queue.h"

// Global variables
Queue *q;
int msgid; // Message queue ID

// Function prototypes
void clearResources(int);
void readfile();
int getAlgorithmChoice();
void sendProcessDataToScheduler();

int main(int argc, char *argv[]) {
    signal(SIGINT, clearResources);

    // Generate a unique key for the message queue
    key_t key = ftok("processes.txt", 'B');
    if (key == -1) {
        perror("Error: Failed to generate key using ftok");
        exit(EXIT_FAILURE);
    }

    // Create the message queue
    msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Error: Failed to create message queue");
        clearResources(0);
        exit(EXIT_FAILURE);
    }

    // Initialization
    readfile(); // Read processes from file into a queue
    int algorithmChoice = getAlgorithmChoice(); // User selects scheduling algorithm

    // Fork the scheduler process
    pid_t pid = fork();
    if (pid == -1) {
        perror("Error: Failed to fork scheduler process");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Child process: Execute the scheduler
        system("gcc scheduler.c -o scheduler");
        execlp("./scheduler", "scheduler", NULL); 
        perror("Error: Failed to execute scheduler");
        exit(EXIT_FAILURE);
    }

    // Parent process: Process Generator
    printf("Scheduler process started with PID %d\n", pid);

    // Initialize clock after creating the scheduler
    initClk();

    // Generate and send processes to the scheduler
    sendProcessDataToScheduler();

    // Clear resources before exiting
    destroyClk(true);
    clearResources(0);

    return 0;
}

void clearResources(int signum) {
    // Clear message queue
    if (msgid != -1) {
        msgctl(msgid, IPC_RMID, NULL);
    }

    if (q) {
        free(q);
    }

    destroyClk(false); // Destroy clock resources
    printf("Resources cleared. Exiting...\n");
    exit(EXIT_SUCCESS);
}

void readfile() {
    FILE *file = fopen("processes.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[100];
    struct processData process;
    q = createQueue(); // Initialize the process queue

    // Skip the header line
    fgets(line, sizeof(line), file);

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = 0;

        if (sscanf(line, "%d %d %d %d", &process.id, &process.arrivaltime, &process.runningtime, &process.priority) == 4) {
            enqueue(q, process); // Add process to the queue
        } else {
            fprintf(stderr, "Error parsing line: %s\n", line);
        }
    }

    fclose(file);
}

int getAlgorithmChoice() {
    int choice;
    do {
        printf("\nSelect scheduling algorithm:\n");
        printf("1. Shortest Job First (SJF)\n");
        printf("2. Round Robin (RR)\n");
        printf("3. Priority Scheduling\n");
        printf("Enter your choice (1-3): ");

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n'); // Clear invalid input
        } else if (choice < 1 || choice > 3) {
            printf("Invalid choice. Please enter a number between 1 and 3.\n");
        }
    } while (choice < 1 || choice > 3);
    return choice;
}

void sendProcessDataToScheduler() {
    struct processData process;
    int currentTime = getClk();

    while (peek(q).arrivaltime <= currentTime) { // Loop until the queue is empty

        process = dequeue(q); // Dequeue the process

        // Send the message to the scheduler
        if (msgsnd(msgid, &process, sizeof(struct processData), 0) == -1) {
            perror("Error: Failed to send message to scheduler");
            clearResources(0);
            exit(EXIT_FAILURE);
        }

        printf("Sent process ID %d to scheduler (Arrival: %d, Runtime: %d, Priority: %d)\n",
                   process.id, process.arrivaltime, process.runningtime, process.priority);

        // Wait briefly before checking again to prevent busy waiting
        usleep(100000); // 100 ms delay
    }
}

