#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "queue.h"

#define QUEUE_SIZE 4
#define NUM_THREADS 2

struct thread_info {
	pthread_t       thread_id;        // id returned by pthread_create()
	int             thread_num;       // application defined thread #
};

struct args {
	int thread_num; 
    int num;
	queue cola;
};

void testInsert(void *ptr){
    struct args *args =  ptr;
    
    printf("thread %d intentando insertar %d \n", args->thread_num, args->num);
    q_insert(args->cola, &args->num);
    printf("insertando elemento %d \n", args->num);
    //printf("insertados %d elementos\n",q_elements(args->cola));
    
}

void testRemove(void *ptr) {
    struct args *args =  ptr;
    int *j;

    printf("thread %d intentando eliminar \n", args->thread_num);
    j = q_remove(args->cola);
    printf("eliminado elemento %d \n", *j);
    //printf("insertados %d elementos\n",q_elements(args->cola));
}

void *testQueue(void *ptr){
    
    int j;
    struct args *args =  ptr;
        
    for(j = 0; j<QUEUE_SIZE; j++){
        testInsert(ptr);
        usleep(500000);
        testRemove(ptr);
    }

    printf(" Elementos en la cola %d \n", q_elements(args->cola));

    return NULL;
}

int main(int argc, char *argv[]) 
{ 
    int i;
    struct args *args = malloc(sizeof(struct args) * NUM_THREADS);
    struct thread_info *threads = malloc(sizeof(struct thread_info) * NUM_THREADS);

    queue cola;

    srand (time(NULL));
    
    cola = q_create(QUEUE_SIZE);   

    for(i=0; i<NUM_THREADS; i++){
        args[i].thread_num = i;
        args[i].cola = cola;
        threads[i].thread_num = i;

        args[i].num = rand() % 10 + 1;
        
        if ( 0 != pthread_create(&threads[i].thread_id, NULL,
					 testQueue, &args[i])) {
			printf("Could not create thread #%d", i);
			exit(1);
		}
    }

    for(i=0; i<NUM_THREADS; i++)
        pthread_join(threads[i].thread_id, NULL);

    q_destroy(cola);

    pthread_exit(NULL);

    free(threads);
    free(args);
    exit (0);
}
