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
#include "./lib/sender.h"
#include "./lib/utility.h"

void client_setup_conn (int*, struct sockaddr_in*);
void client_reliable_conn (int, struct sockaddr_in*);
void client_reliable_close (int client_sock, struct sockaddr_in *server_addr);

int files_from_folder_client(char *list_files[MAX_FILE_LIST]) {
  /* apre la cartella e prende tutti i nomi dei file presenti in essa,
   * inserendoli in un buffer e ritornando il numero di file presenti
   */
  int i = 0;
  DIR *dp;
  struct dirent *ep;
  for(; i < MAX_FILE_LIST; ++i) {
    if ((list_files[i] = malloc(MAX_NAMEFILE_LEN * sizeof(char))) == NULL) {
      perror("malloc list_files");
      exit(EXIT_FAILURE);
    }
  }

  dp = opendir(CLIENT_FOLDER);
  if(dp != NULL){
    i = 0;
    while((ep = readdir(dp))) {
      if(strncmp(ep->d_name, ".", 1) != 0 && strncmp(ep->d_name, "..", 2) != 0){
        strncpy(list_files[i], ep->d_name, MAX_NAMEFILE_LEN);
        ++i;
      }
    }
    closedir(dp);
  }else{
    perror ("Couldn't open the directory");
  }
  return i;
}


int main (int argc, char** argv) {

	int control, answer, bytes, num_files;
	int client_sock;
	int list = LIST, get = GET, put = PUT, close_conn = CLOSE;
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

menu:
  printf("\n============= COMMAND LIST ================\n");
  printf("1) List available files on the server\n");
  printf("2) Download a file from the server\n");
  printf("3) Upload a file to the server\n");
  printf("4) Close connection\n");
  printf("============================================\n\n");
  printf("> Choose an operation: ");
  if(scanf("%d", &answer) > 0 && (answer == LIST || answer == GET || answer == PUT)){
	alarm(0);
  }
  printf("\n");

  switch (answer) {
    case LIST:
		control = sendto(client_sock, (void*)&list, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
		if (control < 0) {
		printf("CLIENT: request failed (sending)\n");
		exit(-1);
		}
		fd = open("clientFiles/file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666); //Apro il file con la lista dei file del server

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

		close(fd);
		remove("clientFiles/file_list.txt");
		break;
		}

	case GET:
		control = sendto(client_sock, (void *)&get, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
		if (control < 0) {
			printf("CLIENT: request failed (sending)\n");
			exit(-1);
		}
		
		printf("Type the file name to download: ");
		memset(buff, 0, sizeof(buff));
		if(scanf("%s", buff)>0) {
			alarm(0);
		}
		
		//controlla se il file esiste già nella directory del client
		char *aux = calloc(PKT_SIZE, sizeof(char));
		snprintf(aux, 12+strlen(buff)+1, "clientFiles/%s", buff);
		fd = open(aux, O_RDONLY);
		if(fd>0){
			char overwrite;
			printf("\nFile già presente nella directory locale. Vuoi sovrascrivere il file? y/n: ");
			scanf(" %c", &overwrite);
			if (overwrite == 'Y' || overwrite == 'y'){
				//overwrite
			}
			else {
				sendto(client_sock, NOVERW, strlen(NOVERW), 0, (struct sockaddr *)&server_address, addr_len);
				goto menu;
			}
			remove(aux);
		}
		close(fd);

		//invio del nome del file al server
		control = sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len);
		if (control < 0) {
			printf("CLIENT: request failed (sending)\n");
			exit(-1);
		}
		snprintf(path, 12+strlen(buff)+1, "clientFiles/%s", buff);
		fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);

		//in attesa del server se il file è presente o meno
		if (recvfrom(client_sock, buff, strlen(NFOUND), 0, (struct sockaddr *)&server_address, &addr_len) < 0) {
			printf("CLIENT: error recvfrom\n");
		}
		if (strncmp(buff, NFOUND, strlen(NFOUND)) == 0) { //file non presente sul server se ricevo notfound
			printf("CLIENT: file not found on server\n");
			close(fd);
			remove(path);
			exit(-1);
		}
		
		control=receiver(client_sock, &server_address, FLYING, LOST_PROB, fd);
		if (control == -1) {
			close(fd);
			remove(path);
			break;
		}
		close(fd);
		break;


	case PUT:

		control = sendto(client_sock, (void *)&put, sizeof(int), 0, (struct sockaddr *)&server_address, sizeof(server_address));
		if (control < 0) {
			printf("CLIENT: request failed (sending)\n");
			exit(-1);
		}

		printf("File available to upload: \n\n");
		num_files = files_from_folder_client(list_files);
		for (i = 0; i < num_files; i++) {
			printf("%s\n", list_files[i]);	
		}

		printf("\nType the file name to upload: ");
		memset(buff, 0, sizeof(buff));
		if(scanf("%s", buff)>0) {
			alarm(0);
		}
		
		//comunico al server il nome del file che sto trasferendo
		control = sendto(client_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&server_address, addr_len);
		if (control < 0) {
			printf("CLIENT: request failed (sending)\n");
			exit(-1);
		}
		//+1 per lo /0 altrimenti lo sostituisce a ultimo carattere
		snprintf(path, 12+strlen(buff)+1, "clientFiles/%s", buff); 
		fd = open(path, O_RDONLY);
		if(fd == -1){
			printf("CLIENT: file not found\n");
			return 1;
		}
		sender(client_sock, &server_address, FLYING, LOST_PROB, fd);
		break;

	case CLOSE:
		control = sendto(client_sock, (void *)&close_conn, sizeof(int), 0, (struct sockaddr *)&server_address, sizeof(server_address));
		if (control < 0) {
			printf("CLIENT: request failed (sending)\n");
			exit(-1);
		}
		client_reliable_close(client_sock, &server_address);
		return 0;
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
	printf("\n================= CONNECTION SETUP =================\n");
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
  printf("%s CLIENT: invio ACK_SYNACK\n", time_stamp());

	control = sendto(client_sock, ACK_SYNACK, strlen(ACK_SYNACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending ACK_SYNACK)\n");
		exit(-1);
	}

	printf("%s CLIENT: connection established\n", time_stamp());
	printf("===================================================\n\n");
}

void client_reliable_close (int client_sock, struct sockaddr_in *server_addr) {
	
	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);

	printf("\n================= CONNECTION CLOSE =================\n");
	//Invio del FIN
	printf("%s CLIENT: invio FIN\n", time_stamp());
	control = sendto(client_sock, FIN, strlen(FIN), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending FIN)\n");
		exit(-1);
	}

	//in attesa del FINACK
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FINACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FINACK, strlen(FINACK)) != 0) {
		printf("CLIENT: close connection failed (receiving FINACK)\n");
		exit(-1);
	}
	printf("%s CLIENT: ricevuto FINACK\n", time_stamp());

	//in attesa del FIN
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FIN), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
		printf("CLIENT: close connection failed (receiving FIN)\n");
		exit(-1);
	}
	printf("%s CLIENT: ricevuto FIN\n", time_stamp());

	//invio del FINACK
	printf("%s CLIENT: invio FINACK\n", time_stamp());
	control = sendto(client_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close connection failed (sending FINACK)\n");
		exit(-1);
	}		

	printf("CLIENT: connection closed\n");
	printf("===================================================\n\n");
}
