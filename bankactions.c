#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bankactions.h"
#include <sys/file.h>
#include <unistd.h>
char* bankbalance = "bank_balance.txt";
char* tmpbank = "temporary.txt";
  // Function for opening and locking a file for task *arg*
  FILE* openLock(char *name, char *arg){
      FILE *ret;
     if(!(ret= fopen(name,arg) )){
        perror("fopen(bankbalance)");
        return NULL;
     }
   if(flock(fileno(ret), LOCK_EX) == -1 ){
        perror("flock() or fileno()");
        return NULL;
     }
     return ret;
  }
  // Closes and unlocks given file
    int closeUnlock(FILE* fp){
    if(flock(fileno(fp), LOCK_UN) ==-1){
        perror("flock(bankfile, LOCK_UN)");
        return -1;
    }
    fclose(fp);
    return 0;
  }
  //
  // Function for depositing currency of amount *amount* to account *account*
  // If there is not a account with that name creates a account with that name and given balance
  // 
  // When this function is called adds the clients socket to database, client receives messages through it
  // Changes the socket information to the current socket of account everytime this is called; 
  int deposit(int amount, int account,  int trc, int socket){
     char buffer[200];
     FILE *bankfile, *tmp;
     if((bankfile = openLock(bankbalance, "r"))== NULL){
        perror("Opening bankfile");
        return -1;
     }
     if((tmp = openLock(tmpbank, "w"))== NULL){ // creates new bank database
        perror("Opening tmp bank");
        return -1;
     }
  
    int ac, am,tmpSock, hasAc = 0;
    while(fscanf(bankfile, "%d %d %d", &ac, &am, &tmpSock) != EOF){  // Loops through bank database
        if(ac == account){    // if wanted account, does changes
            am += amount;
            hasAc =1;
            tmpSock = socket;
        }
        fprintf(tmp, "%d %d %d\n", ac, am,tmpSock);   // Writes read data to the new bank database
    }
    if(hasAc == 0){     // Account was not yet on bank database add it with initial deposit
        fprintf(tmp, "%d %d %d\n", account, amount, socket);
    }
    if(closeUnlock(bankfile) == -1){    // Close current database
        return -1;
    }
    remove(bankbalance); // remove old database
    if(closeUnlock(tmp)== -1){
        return -1; 
    }
    rename(tmpbank, bankbalance);   //renames new database with old database name
    if(trc == 0){ // If function call didn't come from transfer, write to user that the deposit was succesfull
        int j;
        j = sprintf(buffer, "Deposit of %d to account ", amount);
        j = sprintf(buffer +j, "%d was succesfull.\n", account) ;
        write(socket, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
    return 0; 
  }
  //
  // Function which withdraws money from given account
  int withdraw(int amount, int account, int trc){
     FILE *bankfile, *tmp; // To text files, old bank databse and new datanase
     char buffer[200];
     if((bankfile = openLock(bankbalance, "r"))== NULL){
        perror("Opening bankfile");
        return -1;
     }
     if((tmp = openLock(tmpbank, "w"))== NULL){
        perror("Opening tmp bank");
        return -1;
     }
    int balan,  ac, am, socket,tmpsocket,  hasAc = 0;
    int balanceDoesntCut = 0;
    while(fscanf(bankfile, "%d %d %d", &ac, &am, &tmpsocket) != EOF){ //loop through old database 
        if(ac == account){ // if account was found
            hasAc = 1;
            socket = tmpsocket;
            if(am < amount){    // if doesn't have  enough balance for withdraw
              printf("Accounnt balance isn't large enough for withdraw of size %d\n Balance of accou  nt %d: %d", amount, ac, am);
              strcpy(buffer, "Account balance to low for withdraw of that size.\n");
              write(socket, buffer, sizeof(buffer));
              balance(account,  0);
              balanceDoesntCut = 1;
            }else{      // if account has enough balance
                am -= amount;
                balan = am;
                printf("Withdrawn %d from account %d. Account balance now %d\n", amount, ac, am );
            }
        }
        fprintf(tmp, "%d %d %d\n", ac, am, tmpsocket);
    }
    if(closeUnlock(bankfile) == -1){ return -1; }
    remove(bankbalance); // remove old bank database
    if(closeUnlock(tmp)== -1){ return -1; }
    rename(tmpbank, bankbalance); // rename new database as old database
    if(hasAc == 0){ // if account didn't exist ERROR
        perror("NO account with that number\n");
        return -1;
    } // IF account exist and call was made from transfer 
    if(balanceDoesntCut == 1 && trc ==0){
        strcpy(buffer, "Balance is to low.\n");
        write(socket, buffer, sizeof(buffer));
    }
    else if(trc ==0 && hasAc == 1){ // else send to user message
        int j;
        j = sprintf(buffer, "Withdrew amount %d from account ", amount);
        j+= sprintf(buffer +j, " %d.\n  ", account);
        sprintf(buffer +j, "Balance now %d.\n", balan);
        write(socket, buffer, sizeof(buffer));
    }
    return 0;
  }
  //
  // Function for checking current bank accounts balance
  int balance( int account,  int trc){
    char buffer[200];
    FILE *bankfile;
     if((bankfile = openLock(bankbalance, "r"))== NULL){
        perror("Opening bankfile");
        return -1;
     }
    int balance, socket =-1;
    int ac, am,tmpsocket, hasAc = 0;
    while(fscanf(bankfile, "%d %d %d", &ac, &am, &tmpsocket) != EOF){ // Loops through bank file
        if(ac == account){
            hasAc = 1;
            balance = am;
            socket = tmpsocket;
        }
    }
    if(closeUnlock(bankfile) == -1){ return -1; }
    if(hasAc == 0){
        perror("No account with that number\n");
        return -1;
    }
    else if(trc ==0){
        int j;
        j = sprintf(buffer, "Balance of %d account is ", account);
        j = sprintf(buffer +j, "%d.\n", balance) ;
        write(socket, buffer, sizeof(buffer));
        printf("%s", buffer);
    }
    if(trc== 1){
        return socket;
    }
    return balance;
  }
  //
  // Function for transfering trf amount of curency from account1 to account2
    int transfer(int account1, int account2, int trf){
    int ac1Balance =0, ac2Balance=0, socket1;
    char buffer[200];
    if((socket1 =ac1Balance=balance(account1 ,1 ))< trf){ // Uses balance function to check if both account exist 
        perror("Balance of of account %d is to small\n");
        return -1;
    }
    ac2Balance = balance(account2,1 );
    if(ac1Balance ==-1 || ac2Balance==-1){
        perror("Both accounts don't exist\n");
        return -1;
    }
    if(withdraw(trf, account1,1 ) == -1){ //  trie to withdraw from first account
        perror("Withdraw from account1 failed\n");
        return -1;
    }
    if(deposit(trf, account2, 0,1 )== -1){  // tries to deposit withdrawn amount to account2
        perror("Deposit to account2 failed\n");
        return -1;
    }

    int j;
    j = sprintf(buffer, "%d transfered from account  ",trf);
    j += sprintf(buffer +j, "%d to account ",account1) ;
    sprintf(buffer+j , "%d.\n", account2);
    write(socket1, buffer, sizeof(buffer));
    printf("%s", buffer);
    return 0;

  }


  int executeAction(char *action){
    char cTmp, command;
    int account1,account2, amount ;
    int socket;
    command = action[0];
    switch(command){ // switch that checks which action user wants from bank
      case 'l':
          if((sscanf(action, "%c %d", &cTmp,  &account1)) !=2){
          printf("Failed to check account %d's balance\n",  account1);
          return -1;
        }else{
          return balance(account1,0);
        }
        break;
      case 'w':
      if((sscanf(action, "%c %d %d", &cTmp,  &account1, &amount)) != 3){
          printf("Failed to withdraw amount %d from account %d\n", amount, account1);
          return -1;
        }else{
          return withdraw(amount, account1, 0);
        }
        break;
      case 't':
      if((sscanf(action, "%c %d %d %d", &cTmp,  &account1, &account2,  &amount)) != 4){
          printf("Transfer failed\n ");
          return -1;
        }else{
            printf("Account1 = %d, account 2 %d\n", account1, account2);
          return transfer(account1, account2, amount );
        }
        break;
      case 'd':
      if((sscanf(action, "%c %d %d %d", &cTmp,  &account1, &amount, &socket)) != 4){
          printf("Failed to deposit amount %d to account %d\n", amount, account1);
          return -1;
        }else{
          return deposit(amount, account1,0 , socket);
        }
        break;
      default:
        break;
  
    }
  
    return 0;
  }

