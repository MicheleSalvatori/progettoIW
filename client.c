#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>


#include "./lib/comm.h"
#include "./lib/receiver.h"

void client_setup_conn (int*, struct sockaddr_in*);
void client_reliable_conn (int, struct sockaddr_in*);
char *time_stamp();



int main (int argc, char** argv) {

	int control, answer, bytes, num_files;
	int client_sock;
	int list = LIST, get = GET, put = PUT, quit = QUIT;
	struct sockaddr_in server_address;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	char *path = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(server_address);
	int fd;
	off_t end_file, file_control;
	char *list_files[MAX_FILE_LIST];
	char buf[1200];

  client_setup_conn(&client_sock , &server_address);
  client_reliable_conn(client_sock, &server_address);

  memset(buff, 0, sizeof(buff));
  // attende pacchetto READY dal server
  control = recvfrom(client_sock, buff, strlen(READY), 0, (struct sockaddr *)&server_address, &addr_len);
  if (control < 0){
    printf("CLIENT: server errore READY\n");
    exit(-1);
	
  }

menu:
  printf("\n1) List available files on server\n");
  printf("Choose an operation: ");
  if(scanf("%d", &answer) > 0 && answer == LIST ){
  //  alarm(0);
  }
  printf("\n");

  switch (answer) {
    case LIST:
    control = sendto(client_sock, (void*)&list, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
    if (control < 0) {
      printf("CLIENT: request failed (sending)\n");
      exit(-1);
    }
    fd = open("file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666); //Apro il file con la lista dei file del server (vedere se va messo in clientFiles)

   	control = receiver(client_sock, &server_address, 0, 0, fd);
			if(control == -1) {
				close(fd);
				remove("clientFiles/file_list.txt");
			}


	// LETTURA FILE
    end_file = lseek(fd, 0, SEEK_END);
    if (end_file >0){
      lseek(fd, 0, SEEK_SET);
      read(fd, buff, end_file);
			printf("\n==================== FILE LIST =====================\n");
      printf("%s", buff);
			printf("====================================================\n");
    }
  }

  goto menu;

}



void client_setup_conn (int *client_sock, struct sockaddr_in *server_addr) {

	//creazione socket
	if ((*client_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("CLIENT: socket creation error\n");
		exit(-1);
	}

	//configurazione socket
	memset((void *)server_addr, 0, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(SERVER_PORT);

	if (inet_aton(SERVER_IP, &server_addr->sin_addr) == 0) {
		printf("CLIENT: ip conversion error\n");
		exit(-1);
	}
}

void client_reliable_conn (int client_sock, struct sockaddr_in *server_addr) {

	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);

	//passo 1 del three-way-handashake per setup di connessione
	// set_timeout_sec(client_sock, 1);
  printf("%s CLIENT: invio syn\n", time_stamp());

	control = sendto(client_sock, SYN, strlen(SYN), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending SYN)\n");
		exit(-1);
	}

	//in attesa del SYNACK
  printf("%s CLIENT: attesa synack\n", time_stamp());

	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(SYNACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, SYNACK, strlen(SYNACK)) != 0) {
		printf("CLIENT: connection failed (receiving SYNACK)\n");
		exit(-1);
	}

	//invio del ACK_SYNACK
  //sleep(1); //1 secondo prima di inviare SYNACK
  printf("%s CLIENT: invio ACK_SYNACK\n", time_stamp());

	control = sendto(client_sock, ACK_SYNACK, strlen(ACK_SYNACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending ACK_SYNACK)\n");
		exit(-1);
	}

	printf("%s CLIENT: connection established\n", time_stamp());
}


// FUNZIONE PER time_stamp da mettere in utility.c
char *time_stamp(){
	// implementata con libreria sys/time.h
	char *timestamp = (char *)malloc(sizeof(char) * 16);
	time_t ltime;
	ltime=time(NULL);
	struct tm *tm;
	tm=localtime(&ltime);
	sprintf(timestamp,"[%04d/%02d/%02d %02d:%02d:%02d]", tm->tm_year+1900, tm->tm_mon+1,
  tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	return timestamp;
}
