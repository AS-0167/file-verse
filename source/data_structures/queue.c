
#include "queue.h"
#include <stdlib.h>

Queue* queue_create() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    if (!q) return NULL;

    q->front = q->rear = NULL;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

void queue_destroy(Queue* q) {
    if (!q) return;
    // Note: This doesn't free the data inside the queue
    QNode* current = q->front;
    while (current != NULL) {
        QNode* temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    free(q);
}

void queue_enqueue(Queue* q, void* data) {
    pthread_mutex_lock(&q->lock);

    QNode* temp = (QNode*)malloc(sizeof(QNode));
    temp->data = data;
    temp->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = temp;
    } else {
        q->rear->next = temp;
        q->rear = temp;
    }
    q->size++;
    
    pthread_cond_signal(&q->cond); // Signal that an item is available
    pthread_mutex_unlock(&q->lock);
}

void* queue_dequeue(Queue* q) {
    pthread_mutex_lock(&q->lock);

    // Wait until the queue is not empty
    while (q->front == NULL) {
        pthread_cond_wait(&q->cond, &q->lock);
    }

    QNode* temp = q->front;
    void* data = temp->data;

    q->front = q->front->next;

    if (q->front == NULL) {
        q->rear = NULL;
    }
    q->size--;
    free(temp);

    pthread_mutex_unlock(&q->lock);
    return data;
}