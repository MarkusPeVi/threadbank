#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include "common.h"
int  detach(char* block){
    return shmdt(block);
}

int get_memory(char* fil, int size){
  key_t key = ftok(fil, 0);
  if(key == -2){
      perror("ftok()");
      return -1;
  }
  return shmget(key, size, 0644 | IPC_CREAT);
}

char * add_mem(char* fil, int siz){
  int block = get_memory(fil, siz);
  char *res;
  if( block < 0){
      perror("shmget()");
    return NULL;
  }
  res =shmat(block, NULL, 0);
  if(res == (char *) -1){
      perror("shmat()");
      return NULL;
  }
  return res;
}

int del(char* fil){
    int block = get_memory(fil, 0);
    return shmctl(block, IPC_RMID, NULL);
}
