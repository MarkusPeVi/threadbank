
#ifndef BANKACTIONS_H_ 
#define BANKACTIONS_H_
#include <stdio.h>

int executeAction(char *action);
int deposit(int amount, int account, int trc, int socket);
int closeUnlock(FILE *fp);
FILE* openLock(char *name, char *arg);
int balance(int account,  int trc);
int withdraw(int amount, int account,  int trc);
int transfer(int ac1, int ac2, int am );
#endif     
