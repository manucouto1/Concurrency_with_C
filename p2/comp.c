#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 10

#define COMPRESS 1
#define DECOMPRESS 0

struct thread_info {
	pthread_t       thread_id;        // id returned by pthread_create()
	int             thread_num;       // application defined thread #
};

struct worker_args {
    pthread_mutex_t *mutex;
    int thread_num;
    int *chunks;
    queue in;		 
    queue out;
    chunk (*process)(chunk);
};

struct producer_args {
    queue in;
    int size;
    int chunks;
    int fd;
};

struct consumer_args {
    queue out;
    int chunks;
    archive ar;
};

struct producer_args_d {
    queue in;
    int chunks;
    archive ar; 
};

struct consumer_args_d {
    queue out;
    int fd;
    archive ar;
};

// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void *worker(void *ptr) {
    chunk ch, res;
    struct worker_args *args = ptr;

    printf(" Working with thread %d \n", args->thread_num);

    
    while(1) {
        pthread_mutex_lock(args->mutex);

        if(*args->chunks == 0){
            pthread_mutex_unlock(args->mutex);
            return NULL;
        }
        *args->chunks = *args->chunks - 1;
        pthread_mutex_unlock(args->mutex);

        ch = q_remove(args->in);
        printf(" Compresing element %d \n", *args->chunks);
        
        res = args->process(ch);
        free_chunk(ch);
        
        q_insert(args->out, res);
    }
    pthread_mutex_unlock(args->mutex);

    printf(" exit thread %d \n", args->thread_num);

    return NULL;
}

// read input file and send chunks to the in queue
void *producer_comp(void *ptr){
    int i, offset;
    chunk ch;

    struct producer_args *args = ptr;

    for(i=0; i<args->chunks; i++) {
        
        ch = alloc_chunk(args->size);

        offset=lseek(args->fd, 0, SEEK_CUR);

        ch->size   = read(args->fd, ch->data, args->size);
        ch->num    = i;
        ch->offset = offset;
        
        q_insert(args->in, ch);
        printf(" Creating element %d \n",i);
    }

    return NULL;
}

// send chunks to the output archive file
void *consumer_comp(void *ptr){
    chunk ch;
    int i;
    struct consumer_args *args = ptr;

    
    for(i=0; i<args->chunks; i++) {

        ch = q_remove(args->out);
        
        add_chunk(args->ar, ch);
        printf(" Written element %d \n",i);
        free_chunk(ch);
    }

    return NULL;
}

void *producer_decomp(void *ptr){
    
    int i;
    chunk ch;
    struct producer_args_d *args = ptr;

    for(i=0; i<chunks(args->ar); i++) {
        ch = get_chunk(args->ar, i);
        q_insert(args->in, ch);
        printf(" Creating element %d \n", i);
    }

    return NULL;
}

void *consumer_decomp(void *ptr){

    int i;
    chunk ch;

    struct consumer_args_d *args = ptr;

    for(i=0; i<chunks(args->ar); i++) {
        ch=q_remove(args->out);
        lseek(args->fd, ch->offset, SEEK_SET);
        write(args->fd, ch->data, ch->size);
        free_chunk(ch);
        printf(" Writen element %d \n", i);
    }

    return NULL;
}
// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
    int fd, i;
    struct stat st;

    struct thread_info *workers;
    struct thread_info producer, consumer;
    
    struct producer_args producer_args;
    struct worker_args *worker_args;
    struct consumer_args consumer_args;
    
    pthread_mutex_t *mutex;
    int *chunks;
    int chunks_aux;

    char comp_file[256];
    archive ar;
    queue in, out;
    

    workers = malloc(sizeof(struct thread_info) * opt.num_threads);
    worker_args = malloc(sizeof(struct worker_args) * opt.num_threads);
    mutex = malloc(sizeof(pthread_mutex_t));
    chunks = malloc(sizeof(int));

    pthread_mutex_init(mutex, NULL);

    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }
    
    fstat(fd, &st);
    
    *chunks = st.st_size/opt.size+(st.st_size % opt.size ? 1:0);
    chunks_aux = *chunks;

    if(opt.out_file) {
        strncpy(comp_file,opt.out_file,255);
    } else {
        strncpy(comp_file, opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }
    
    printf(" Num chunks %d \n", *chunks);
    printf(" Cola size %d \n", opt.queue_size);

    ar = create_archive_file(comp_file);

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    // read input file and send chunks to the in queue

    producer_args.chunks = *chunks;
    producer_args.in = in;
    producer_args.size = opt.size; 
    printf("SIZE >> %d\n", producer_args.size);
    producer_args.fd = fd;
    
    if( 0 != pthread_create(&producer.thread_id, NULL,
            producer_comp, &producer_args)) {
        printf("Could not create thread chunkit\n");
    }

    // compression of chunks from in to out
    for(i = 0; i < opt.num_threads; i++){
        workers[i].thread_num = i;
        
        worker_args[i].thread_num = i;
        worker_args[i].in = in;
        worker_args[i].out = out;
        worker_args[i].process = zcompress;
        worker_args[i].chunks = chunks;
        worker_args[i].mutex = mutex;

        if ( 0 != pthread_create(&workers[i].thread_id, NULL,
                        worker, &worker_args[i])) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    // send chunks to the output archive file
    consumer_args.chunks = chunks_aux;
    consumer_args.out = out;
    consumer_args.ar = ar;

    if( 0 != pthread_create(&consumer.thread_id, NULL,
            consumer_comp, &consumer_args)) {
        printf("Could not create thread dechunit\n");
    }

    
    for(i = 0; i<opt.num_threads; i++)
        pthread_join(workers[i].thread_id, NULL);
    pthread_join(producer.thread_id, NULL);
    pthread_join(consumer.thread_id, NULL);

    close_archive_file(ar);
    close(fd);
    q_destroy(in);
    q_destroy(out);
    pthread_mutex_destroy(mutex);

    free(workers);
    free(worker_args);
    free(mutex);
    free(chunks);
    
    pthread_exit(NULL); // Equivale a un exit

  
}


// Decompress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the decompressed file

void decomp(struct options opt) {
    int fd, i;
    //struct stat st;
    struct producer_args_d producer_args_d;
    struct worker_args *worker_args;
    struct consumer_args_d consumer_args_d;

    struct thread_info *workers;
    struct thread_info producer, consumer;
    
    pthread_mutex_t *mutex;
    int *n_chunks;

    char uncomp_file[256];
    archive ar;
    queue in, out;
    
    workers = malloc(sizeof(struct thread_info) * opt.num_threads);
    worker_args = malloc(sizeof(struct worker_args) * opt.num_threads);
    mutex = malloc(sizeof(pthread_mutex_t));
    n_chunks = malloc(sizeof(int));

    pthread_mutex_init(mutex, NULL);
    
    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };
    
    *n_chunks = chunks(ar);

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    } 

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);

    // read chunks with compressed data
    
    producer_args_d.in = in;
    producer_args_d.ar = ar;
    
    if( 0 != pthread_create(&producer.thread_id, NULL,
        producer_decomp, &producer_args_d)) {
        printf("Could not create thread dechunit\n");
    }

    // decompress from in to out
    for(i = 0; i<opt.num_threads; i++){
        workers[i].thread_num = i;

        worker_args[i].thread_num = i;
        worker_args[i].in = in;
        worker_args[i].out = out;
        worker_args[i].process = zdecompress;
        worker_args[i].chunks = n_chunks;
        worker_args[i].mutex = mutex;

        if( 0 != pthread_create(&workers[i].thread_id, NULL,
            worker, &worker_args[i])) {
            printf("Could not create thread dechunit\n");
        }
    }
    
    consumer_args_d.out = out;
    consumer_args_d.ar = ar;
    consumer_args_d.fd = fd;

    // write chunks from output to decompressed file
    if( 0 != pthread_create(&consumer.thread_id, NULL,
        consumer_decomp, &consumer_args_d)) {
        printf("Could not create thread dechunit\n");
    }
    
    
    
    for(i = 0; i<opt.num_threads; i++)
        pthread_join(workers[i].thread_id, NULL);
    pthread_join(producer.thread_id, NULL);
    pthread_join(consumer.thread_id, NULL);


    pthread_mutex_destroy(mutex);

    close_archive_file(ar);    
    close(fd);
    q_destroy(in);
    q_destroy(out);

    free(workers);
    free(worker_args);
    free(mutex);
    free(n_chunks);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {    
    struct options opt;
    
    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;
    
    read_options(argc, argv, &opt);
    
    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);

    return 0;
}
    
    
