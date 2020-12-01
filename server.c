#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/un.h>
#include <pthread.h>
//#include "linkedList.h"
#include "bankactions.h"
#include <mqueue.h>
#include <sys/shm.h>
#include "common.h"
#include <sys/stat.h>
#include "log.h"
#define logfile "logfile.txt"
pthread_t thread_pool[desk_size];       // Sevice desk threads
pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
mqd_t mes_que[desk_size];           // Message Queue's
pthread_t master_thread, messenger;   // Thread which checks for new socket connections
char *master_queue = "/master_q";       // thread which writes to bank database
int client_socket;                    // Global parm.  for new connection socket
int que_full = 0;                // Global parm. for "master" thread to slow down service desks if its full
mqd_t mas_que;
static int log;
// SERVICE DESK
// Reads messages from its own queue, sends them to "master" to be writen on bank database
// If "master" queue waits for master to send signal before continues
void * handler( void* p_deskNum){
    int desk = *((int *) p_deskNum); // casting void pointer to int
    free(p_deskNum);        
    pthread_t id = pthread_self();  // when terminated thread automatically free's resources on its use
    pthread_detach(id);
    mqd_t que = mes_que[desk];
    char buffer[BUFSIZE];
    int bytes_read;
      mqd_t master;
    master = mq_open(master_queue, O_WRONLY); 
    if(master ==-1){
        perror("open(master)");
        exit(EXIT_FAILURE);
    }
    while(1){
        pthread_mutex_lock(&mutex);
        if(que_full == 1){              //  Waits if "master" queue is full untill signaled it can continue
            pthread_cond_wait(&cond_var, &mutex);
        }
        pthread_mutex_unlock(&mutex);
        bytes_read =mq_receive(que, buffer, BUFSIZE, NULL);     
        buffer[bytes_read] = '\0';
       
        if(mq_send(master, buffer, BUFSIZE, 0)!= 0){
            perror("mq_send(master), handler");
        }  // Send received messages to masters queue 
        memset(buffer, 0, sizeof(buffer));
   }
    return NULL;
}
// Master function
//  Gets data from service desk's, writes it to bank database
//
void *master(void* p){
    struct mq_attr mattr; 
    mattr.mq_flags =0;
    mattr.mq_maxmsg=10;
    mattr.mq_msgsize = 200;
    mattr.mq_curmsgs =0;
    mas_que = mq_open(master_queue, O_CREAT | O_RDONLY, 0644, &mattr);
    if(mas_que ==-1){
        writeToLog(log, "Error opening master queue\n");
        perror("master_queue()");
    }

    char buffer[BUFSIZE];
    memset(buffer, 0, sizeof(buffer));
    strcpy(buffer, "All bank actions from last session underneath:\n ");
    writeToLog(log, buffer); 	
    memset(buffer, 0, sizeof(buffer));
    int client;
    long max = mattr.mq_maxmsg;
    char sock_string[10];
    char* exit = "exit";
    while(1){
        mq_getattr(mas_que,&mattr);
        if(mattr.mq_curmsgs >= max){      // Checks if queue full, if so tell's service desks, to pause untill queue isn't full
            pthread_mutex_lock(&mutex);
            que_full =1;
            pthread_mutex_unlock(&mutex);
        } else{                 // Tells service desks "master" queue isn't full
            pthread_mutex_lock(&mutex);
            que_full =0;
            pthread_cond_signal(&cond_var);
            pthread_mutex_unlock(&mutex);
        }  
        if( mq_receive(mas_que, buffer, BUFSIZE, NULL)== 0){
            perror("mq_receive(master)");
        }
        if(buffer[0] == 'd'){         // if a deposit message adds socket to message so client can receive messages
            pthread_mutex_lock(&mutex);
            client = client_socket; 
            sprintf(sock_string, " %d", client);
            buffer[strlen(buffer)-1] ='\0';
            strcpy(buffer +strlen(buffer),sock_string);
            memset(sock_string, 0, sizeof(sock_string));
            pthread_mutex_unlock(&mutex);
        }
        executeAction(buffer);      // Calls bankactions library to write command to bank database
        if(strncmp(buffer, exit, strlen(exit))!= 0){
            strcpy(buffer +strlen(buffer), "\n");
            writeToLog(log, buffer); 	
        }
        memset(buffer, 0, sizeof(buffer));
    }
    return NULL;
}

// Thread which waits for connections from clients 
// stores them to global variable
void* accept_messages(void *p_socket){
    int socket = *((int *)p_socket);
    free(p_socket);
    int client;
    while(1){
        if((client = accept(socket, NULL, NULL))==-1 ){
          perror("accept()");
          exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&mutex);
        client_socket = client;
        pthread_mutex_unlock(&mutex);
    }
  return NULL;
}

// closes pthreds and closes queues
// unlinks everything 
int cleanup(){
    char qname[200];    
    for(int i =0; i < desk_size; i++){
        if(pthread_cancel(thread_pool[i])!=0){
            perror("pthread_cancel(thead_pool)");
            exit(EXIT_FAILURE);
        }
    } 
    if(pthread_cancel(master_thread)){
        perror("pthread_cancel(master");
        exit(EXIT_FAILURE);
    }
    if(pthread_cancel(messenger)){
        perror("pthread_cancel(master");
        exit(EXIT_FAILURE);
    }
    memset(qname, 0, sizeof(qname));
    for(int i =0; i < desk_size;i++){
        mq_close(mes_que[i]);
        sprintf(qname, "/qued%d", i);
        mq_unlink(qname);
        memset(qname, 0, sizeof(qname));
    }
    mq_close(mas_que);
    mq_unlink(master_queue);
    return 0;

}
// Main function
// Creates shared memory pool
// Creates service desk queues
// Creates service desks, message receiver and master queue
// Creates Socket where clients can connect to
// Runs untill users send writes "quit" on terminal
// Cleans everything up before exit
int  main(int agrc, char *agrv[]){
    log = open(logfile, O_RDWR | O_APPEND | O_CREAT| O_NONBLOCK, 0777);
    struct mq_attr attr[desk_size]; // attr array for service desks creation
    char qname[200];    // buffer used for service desk queue's
    int shm_id; //shared memory id
    char *ptr;
    int next[desk_size]; // Used to write to shared memory
    struct sockaddr_un address;
    int masFd;    // Server socket
    int *sock = malloc(sizeof(int)); // pointer which is used to send servers socket to message_receiver thread
    if((shm_id = shmget(key_num, shm_size, IPC_CREAT | S_IRUSR | S_IWUSR))< 0){ //get shared memory
        writeToLog(log, "Error shmget()\n"); 	
        perror("shmget()");
        exit(EXIT_FAILURE);
    }
    char* shm = (char*) shmat(shm_id, 0, 0);    // Pointer for shared memory
    if(shm==(char *) -1){
        writeToLog(log, "Error shmat()\n"); 	
        perror("shmat()");
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&master_thread, NULL, master, NULL)!= 0){ // creates "master" thread
        writeToLog(log, "Error creating master thread\n"); 	
        perror("pthread_create()  ");
        exit(EXIT_FAILURE);
        }
    ptr = shm + sizeof(next);
    for(int i =0; i< desk_size; i++){   //Loop through shared memory, so that ptr* (pointer to shared memory keeps up)
      next[i] = sprintf(ptr, "/qued%d", i) +1; 
      ptr += next[i];
    }
    memcpy(shm, &next, sizeof(next));  // copy data to shared memory (message queue names)
   
    for(int i =0; i < desk_size; i++ ){ // Loop which creates service desks and service desk queue's
        int* itmp = malloc(sizeof(int));
        *itmp = i;
        if(pthread_create(&thread_pool[i], NULL, handler, itmp)!= 0){   // Creates service desk
        writeToLog(log, "Error creating service desk\n"); 	
            perror("pthread_create()  ");
            exit(EXIT_FAILURE);
        }
        mqd_t tmpmsg; 
        attr[i].mq_flags =0;
        attr[i].mq_maxmsg=10;
        attr[i].mq_msgsize = 200;
        attr[i].mq_curmsgs =0;
        sprintf(qname, "/qued%d", i);
        tmpmsg = mq_open(qname, O_CREAT | O_RDONLY, 0664, &attr[i]); // Service desk message queue's
        mes_que[i] = tmpmsg;
        if(tmpmsg==-1 || mes_que[i] ==-1){
            perror("que()");
        }
        memset(qname, 0, sizeof(qname));
    }
    // create socket
    masFd = socket(AF_UNIX, SOCK_STREAM, 0);    
    if(masFd< -1){
        perror("master socket failure");
        exit(EXIT_FAILURE);
    }
  
    // unlink if there is something left from last time
    if( unlink(SOCKET_PATH) == -1 &&( errno != ENOENT)){
       perror("socket removing");
      exit(EXIT_FAILURE);
    }
  
    memset(&address, 0, sizeof(struct sockaddr_un));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path)-1);
  
    //binding socket
    if(bind(masFd, (struct sockaddr *) &address, sizeof(address) )== -1){
        perror("bind()");
        exit(EXIT_FAILURE);
    }
    if(listen(masFd, BACKLOG)== -1){
      perror("listen() ");
      exit(EXIT_FAILURE);
    }
    *sock = masFd;
    if(pthread_create(&messenger, NULL, accept_messages, sock)!= 0){  // Creates messge receiver thread
        perror("pthread_create()  ");
        exit(EXIT_FAILURE);
        }
    printf("Write quit to terminal to terminate program.\n");
    char buffer[BUFSIZE];
    memset(buffer, 0, sizeof(buffer));
    while(read(STDIN_FILENO, buffer, sizeof(buffer))>0 ){     //Loops through user input untill receives quit
        if(strcmp(buffer, "quit\n") ==0){
            break;
            printf("%s", buffer);
        }
      memset(buffer, 0, sizeof(buffer));
    }

    /* CLEAN UP*/
    cleanup();
    shmdt(shm);
    shmctl(shm_id, IPC_RMID, NULL);
    exit(0);
    return 0;
}
