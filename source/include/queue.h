
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

// Node for a request in the queue
typedef struct QNode {
    void* data;
    struct QNode* next;
} QNode;

// The queue itself
typedef struct {
    QNode *front, *rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Queue;

Queue* queue_create();
void queue_destroy(Queue* q);
void queue_enqueue(Queue* q, void* data);
void* queue_dequeue(Queue* q);

#endif // QUEUE_H