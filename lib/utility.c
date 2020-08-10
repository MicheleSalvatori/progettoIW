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

// Imposta il timer del ricevente per l'invio di ack cumulativi
void set_timer(int time){
    struct itimerval it_val;
    
        it_val.it_value.tv_sec = 0;
    	it_val.it_value.tv_usec = time;
    	it_val.it_interval.tv_sec = 0;
	    it_val.it_interval.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
  			perror("setitimer");
			exit(1);
			}
		printf("Timer avviato\n");
}

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