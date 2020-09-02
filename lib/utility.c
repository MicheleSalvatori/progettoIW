#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include "utility.h"

// Utilizzata per il debug e l'analisi dei pacchetti inviati
void inputs_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}

// Genera un numero casuale e ritorna true o false in base alla probabilita di perdita passata in input
bool is_packet_lost(int prob){
  int random = rand() %100;
 // printf ("Random Number: %d\n",random);
  if (random<=prob){
	  return true;
  }
  return false;
}

// Stampa il timestamp con precisione ai microsecondi
char *time_stamp(){
	struct timeval tv;
	struct tm* ptm;
	char time_string[40];
 	long microseconds;
	char *timestamp = (char *)malloc(sizeof(char) * 16);

	gettimeofday(&tv,0);
	ptm = localtime (&tv.tv_sec);
	strftime (time_string, sizeof (time_string), "%H:%M:%S", ptm); //"%Y-%m-%d %H:%M:%S" full timestamp con data 
	microseconds = tv.tv_usec;
	sprintf (timestamp,"[%s.%03ld]", time_string, microseconds);
	return timestamp;
}

// Imposta il timer di ritrasmissione
void set_timer(int micro){
    struct itimerval it_val;
	int sec = 0;
	if (micro >= 1000000){
		sec = 1;
		micro = 0;
	}
	it_val.it_value.tv_sec = sec;
	it_val.it_value.tv_usec = micro;
	it_val.it_interval.tv_sec = 0;
	it_val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		perror("setitimer");
		exit(1);
	}
}