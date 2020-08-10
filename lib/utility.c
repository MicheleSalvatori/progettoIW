#include <sys/time.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>


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


void inputs_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}