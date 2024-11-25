#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct processData {
    int arrivaltime;
    int priority;
    int runningtime;
    int id;
};

// Structure for a node in the queue
typedef struct Node {
    struct processData data;
    struct Node *next;
} Node;

// Structure for the queue itself
typedef struct Queue {
    Node *front;
    Node *rear;
} Queue;

// Function to create a new queue
Queue* createQueue() {
    Queue *q = (Queue*)malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    return q;
}

// Function to add a processData to the rear of the queue
void enqueue(Queue *q, struct processData data) {
    Node *newNode = (Node*)malloc(sizeof(Node));
    newNode->data = data;
    newNode->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

// Function to remove a processData from the front of the queue
struct processData dequeue(Queue *q) {
    if (q->front == NULL) {
        // Handle empty queue (you might want to return a default value or signal an error)
        struct processData empty = {-1, -1, -1, -1}; // Example of a default value
        return empty;
    }
    Node *temp = q->front;
    struct processData data = temp->data;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    return data;
}

// Function to peek at the front of the queue
struct processData peek(Queue *q) {
    if (q->front == NULL) {
        // Handle empty queue (you might want to return a default value or signal an error)
        struct processData empty = {-1, -1, -1, -1}; // Example of a default value
        return empty;
    }
    return q->front->data;
}

// Function to check if the queue is empty
int isEmpty(Queue *q) {
    return (q->front == NULL);
}

// Function to get the size of the queue
int queueSize(Queue *q) {
    int size = 0;
    Node *current = q->front;
    while (current != NULL) {
        size++;
        current = current->next;
    }
    return size;
}

#endif // QUEUE_H
