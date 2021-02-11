#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>

// circular array
typedef struct _queue {
    pthread_mutex_t *mutex;
    pthread_cond_t *queue_full;
    pthread_cond_t *queue_empty;
    int size;
    int used;
    int first;
    void **data;
} _queue;

#include "queue.h"

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));

    q->mutex = malloc(sizeof(pthread_mutex_t));
    q->queue_full = malloc(sizeof(pthread_cond_t));
    q->queue_empty = malloc(sizeof(pthread_cond_t));
    
    pthread_mutex_init(q->mutex, NULL);
    pthread_cond_init(q->queue_full, NULL);
    pthread_cond_init(q->queue_empty, NULL);
    
    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size*sizeof(void *));
    
    return q;
}

int q_elements(queue q) {
    int res;
    pthread_mutex_lock(q->mutex);
    res = q->used;
    pthread_mutex_unlock(q->mutex);
    return res;
}

int q_insert(queue q, void *elem) {
    pthread_mutex_lock(q->mutex);
    
    while(q->size == q->used) {
        pthread_cond_wait(q->queue_full, q->mutex);
    }

    q->data[(q->first+q->used) % q->size] = elem;    
    q->used++;

    if(q->used == 1) {
        pthread_cond_broadcast(q->queue_empty);
    }

    pthread_mutex_unlock(q->mutex);
    return 1;
}

void *q_remove(queue q) {
    void *res;
    pthread_mutex_lock(q->mutex);

    while(q->used == 0) {
        pthread_cond_wait(q->queue_empty, q->mutex);
    }
    
    res = q->data[q->first];
    q->first = (q->first+1) % q->size;
    q->used--;

    if(q->used == q->size - 1){
        pthread_cond_broadcast(q->queue_full);
    }
    pthread_mutex_unlock(q->mutex);

    return res;
}

void q_destroy(queue q) {
    
    pthread_mutex_destroy(q->mutex);
    pthread_cond_destroy(q->queue_full);
    pthread_cond_destroy(q->queue_empty);
    
    free(q->mutex);
    free(q->queue_full);
    free(q->queue_empty);
    
    free(q->data);
    free(q);
}
