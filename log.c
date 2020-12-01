#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

int writeToLog(int fp, char* str){
	time_t clock;
	time(&clock);
	struct tm *atTheMo = localtime(&clock);
	int h, min, sec, mon, day, year;
	char logInf[400];
	h = atTheMo->tm_hour;
	min = atTheMo -> tm_min;
	sec = atTheMo -> tm_sec;
	mon =atTheMo->tm_mon;		
	day = atTheMo-> tm_mday;
	year = atTheMo -> tm_year;
	// writes current time information from struct tm to logInf 
	sprintf(logInf, "%d.%d.%d %d:%d:%d :", day,mon+1, 1900+ year, h, min, sec);
	sprintf(logInf+strlen(logInf),"%s", str);
	int w;
	// writes logInf to log-file fp. 
	// If fp is used tries agen
	while( (w = write(fp, logInf, strlen(logInf))) == -1){
		if((w != EWOULDBLOCK) & ( w !=  EAGAIN)){
			break;
		}	
	}
	return 0;

}
