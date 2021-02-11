CC=gcc
CFLAGS=-Wall -pthread -g
LIBS=-lz

OBJS=compress.o chunk_archive.o options.o queue.o comp.o
OBJS2=queue.o test_queue.o 

PROGS= comp test

all: $(PROGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

comp: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

test: $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(LIBS)

clean: 
	rm -f $(PROGS) *.o *~
