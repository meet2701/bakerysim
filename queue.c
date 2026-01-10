// ############## LLM Generated Code Begins ##############

#include "queue.h"
#include <stdio.h>

Queue *create_queue()
{
    Queue *q = (Queue *)malloc(sizeof(Queue));
    if (!q)
    {
        perror("Failed to allocate queue");
        exit(EXIT_FAILURE);
    }
    q->front = q->rear = NULL;
    return q;
}

void enqueue(Queue *q, void *data)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (!newNode)
    {
        perror("Failed to allocate queue node");
        exit(EXIT_FAILURE);
    }
    newNode->data = data;
    newNode->next = NULL;
    if (q->rear == NULL)
    {
        q->front = q->rear = newNode;
        return;
    }
    q->rear->next = newNode;
    q->rear = newNode;
}

void *dequeue(Queue *q)
{
    if (q->front == NULL)
    {
        return NULL;
    }
    Node *temp = q->front;
    void *data = temp->data;
    q->front = q->front->next;
    if (q->front == NULL)
    {
        q->rear = NULL;
    }
    free(temp);
    return data;
}

void *peek(Queue *q)
{
    if (q->front == NULL)
    {
        return NULL;
    }
    return q->front->data;
}

int is_empty(Queue *q)
{
    return q->front == NULL;
}

void destroy_queue(Queue *q)
{
    while (!is_empty(q))
    {
        dequeue(q);
    }
    free(q);
}

// ############## LLM Generated Code Ends ##############