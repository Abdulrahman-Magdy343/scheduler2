#include "headers.h"
#include "Queue.h"
#include <limits.h> // For INT_MAX

// Global queue to store processes
Queue *readyQueue;

// Function to compare processes for SJF (Shortest Job First)
int compareSJF(const void *a, const void *b)
{
    return ((struct processData *)a)->runningtime - ((struct processData *)b)->runningtime;
}

// Function to compare processes for Priority (lower value = higher priority)
int comparePriority(const void *a, const void *b)
{
    return ((struct processData *)a)->priority - ((struct processData *)b)->priority;
}

// Round Robin Scheduling
void roundRobin(struct processData process, int timeQuantum)
{
    // Add the process to the global queue
    enqueue(readyQueue, process);

    struct processData p;
    while (!isEmpty(readyQueue))
    {
        p = dequeue(readyQueue);
        int timeToRun = (p.runningtime > timeQuantum) ? timeQuantum : p.runningtime;
        // Simulate process running
        sleep(timeToRun);
        p.runningtime -= timeToRun;
        if (p.runningtime > 0)
        {
            enqueue(readyQueue, p); // Add back to queue if not finished
        }
    }
}

// Shortest Job First Scheduling
void shortestJobFirst()
{
    struct CPUproc p;
    while (!isEmpty(readyQueue))
    {
        p = dequeue(readyQueue);
        forkProcess(p);
        sleep(p.runningtime); // Simulate process running
    }
}

// Priority Scheduling
void priorityScheduling(struct processData process)
{
    // Add the process to the global queue
    enqueue(readyQueue, process);

    int size = queueSize(readyQueue); // Get the size of the queue
    struct processData processes[size];

    // Copy the queue elements to an array for sorting
    for (int i = 0; i < size; ++i)
    {
        processes[i] = dequeue(readyQueue);
    }

    // Sort the array based on priority
    qsort(processes, size, sizeof(struct processData), comparePriority);

    // Enqueue the processes back to the queue after sorting
    for (int i = 0; i < size; ++i)
    {
        enqueue(readyQueue, processes[i]);
    }

    struct processData p;
    while (!isEmpty(readyQueue))
    {
        p = dequeue(readyQueue);
        sleep(p.runningtime); // Simulate process running
    }
}

int main(int argc, char *argv[])
{
    initClk();

    // Initialize the global queue
    readyQueue = createQueue();

    int algorithmChoice;
    int timeQuantum; // For Round Robin
    struct processData incomingProcess;

    // Example input for testing
    incomingProcess.arrivaltime = 0;
    incomingProcess.priority = 1;
    incomingProcess.runningtime = 5;
    incomingProcess.id = 101;

    // Assume we receive processes and algorithm choice from a generator
    algorithmChoice = 1; // For example: Shortest Job First

    switch (algorithmChoice)
    {
    case 1:
        shortestJobFirst(incomingProcess);
        break;
    case 2:
        timeQuantum = 2; // Example time quantum
        roundRobin(incomingProcess, timeQuantum);
        break;
    case 3:
        priorityScheduling(incomingProcess);
        break;
    default:
        fprintf(stderr, "Invalid algorithm choice.\n");
    }

    destroyClk(true);
    return 0;
}
