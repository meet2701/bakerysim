// ############## LLM Generated Code Begins ##############

#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>

// Node for the linked list queue
typedef struct Node
{
    void *data;
    struct Node *next;
} Node;

// Queue structure
typedef struct
{
    Node *front;
    Node *rear;
} Queue;

// Function prototypes for queue operations
Queue *create_queue();
void enqueue(Queue *q, void *data);
void *dequeue(Queue *q);
void *peek(Queue *q);
int is_empty(Queue *q);
void destroy_queue(Queue *q);

#endif // QUEUE_H

// ############## LLM Generated Code Ends ##############