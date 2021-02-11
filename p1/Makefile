
CC=gcc
CFLAGS=-Wall -pthread -g
LIBS=
OBJS=swap.o options.o
OBJS2=swap2.o options.o
OBJS3=swap3.o options.o

PROGS= swap swap2 swap3

all: $(PROGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

swap: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

swap2: $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $(OBJS2) $(LIBS)

swap3: $(OBJS3)
	$(CC) $(CFLAGS) -o $@ $(OBJS3) $(LIBS)

clean:
	rm -f $(PROGS) *.o *~

