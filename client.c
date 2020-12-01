#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <mqueue.h>
#include <sys/shm.h>
#include "common.h"
#include <sys/socket.h> 
#include <sys/un.h> 
#include <unistd.h>
#include <signal.h>


void signal_ha(int sig){ // signal handler, makes sure chilld process is killed
    kill(0, SIGTERM);
    exit(0);
}

// Simple function which receives and prints out messages from bankserver
// Run by a chilld process 
int receive_messages(int socket){
    int read_size;
    char readbuf[BUFSIZE];
    while((read_size= read(socket, readbuf, sizeof(readbuf)))> 0){
        if(read_size== -1){
            perror("read(soccket)");
            exit(EXIT_FAILURE);
        }
        printf("%s" ,readbuf);
        memset(&readbuf, 0, sizeof(readbuf));
    }
  return 0;
}
// Creates and connects a socket to bank server
// creates a chilld process which reads messages for that socket
int socket_connect(){
    struct sockaddr_un address;
    int socketFd;
    socketFd= socket(AF_UNIX, SOCK_STREAM, 0);
    if(socketFd ==-1){
        perror("socket()");
        exit(EXIT_FAILURE);
    }
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path)-1);
    if(connect(socketFd, (struct sockaddr *) &address, sizeof(address)-1) ==-1){
          perror("connect()");
          exit(EXIT_FAILURE);
    }
    int pid = fork();
    if(pid == 0){
        receive_messages(socketFd);
    }
  return 0;
}

// Open the shared memory and read from there names of available message ques
// Checcks which queue is shortes, with attr
// connects to that queue
// reads stdin on a loop
// USE DEPOSIT AS FIRST MESSAGE:
// BECAUSE: -> creates tpc connection to server
//              is able to get account balance information
//Works otherwise also, but banks information won't get to client
int main(){
    signal(SIGINT, signal_ha); // signal handler
    int shm_id;
    char *shm;
    char buffer[BUFSIZE];
    mqd_t mq_pool[desk_size], mq;
    struct mq_attr attr;
    int smallest_que =0;
    long que_size;
    if((shm_id = shmget(key_num, shm_size, 0))<0){   // get the shared memory
      perror("shmget()");
      exit(EXIT_FAILURE);
    }
    shm = shmat(shm_id, 0, 0);
    int *p;
    char* shared_memory[desk_size];
    char *ptr;
    p = (int *) shm;
    ptr = shm +sizeof(int)*(desk_size);
    for(int i =0; i < desk_size; i++){      // Loop through shared memory to get message queue's
        shared_memory[i] = ptr;
        if(i < desk_size -1){
            ptr += *p++;
        }else{
            ptr += *p;
        }
    }
    mq_pool[0] = mq_open(shared_memory[0], O_WRONLY);
    mq_getattr(mq_pool[0], &attr);
    que_size = attr.mq_curmsgs;
    for(int i =1; i < desk_size; i++){      // Loop that check which queue is shortest
        mq_pool[i] = mq_open(shared_memory[i], O_WRONLY);  
        if(mq_pool[i] ==-1){
            perror("open(mq_pool)");
            exit(EXIT_FAILURE);
        }
        mq_getattr(mq_pool[i], &attr);
        if(que_size > attr.mq_curmsgs){
            smallest_que =i;
        }
    }
    for(int i =0; i < desk_size; i++){      // Closes connection to non needed queues
        if(i != smallest_que){
            mq_close(mq_pool[i]);       
        }
    }
    mq = mq_pool[smallest_que];
    do {          // Starts reading messages from stdin and send them to message queue
        fflush(stdout);
        memset(&buffer, 0, BUFSIZE);
        fgets(buffer, BUFSIZE, stdin);
        if(buffer[0] == 'd'){
            socket_connect();
        }
        if(strcmp(buffer, MSG_STOP)!= 0){   /* send the message */
            mq_send(mq, buffer, BUFSIZE, 0);
        }
    } while (strncmp(buffer, MSG_STOP, strlen(MSG_STOP))); // Loop stops when message is "exit" 
    kill(0, SIGINT);   /* cleanup  */
    mq_close(mq);
    shmdt(shm);
    exit(0);
    return 0;
}

