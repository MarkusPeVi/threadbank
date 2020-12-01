CC = gcc
CFLAGS = -g -D_POSIX_SOURCE -Wall -pedantic -pthread -lrt
BINS= server client 
OBJS= server.o common.o bankactions.o log.o 
OBJS2 = client.o common.o log.o
all: $(BINS)


client: $(OBJS2)
	$(CC) $(CFLAGS) -o $@ $^

server: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%,o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf *.log *.DSYM *.o $(BINS)

