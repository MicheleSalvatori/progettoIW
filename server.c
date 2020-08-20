#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include "./lib/comm.h"
#include "./lib/sender.h"
#include "./lib/receiver.h"

#define PKT_SIZE 1500
#define SERVER_PORT 25490
#define LIST 1
#define REQUEST_SEC 10
#define READY "ready"

#define SYN "syn"
#define SYNACK "synack"
#define ACK_SYNACK "ack_synack"



void server_setup_conn( int *, struct sockaddr_in *);
int server_reliable_conn(int , struct sockaddr_in *); // vedere se per tcp va bene
char *time_stamp();

// da mettere in un file utility.c
void set_timeout_sec(int sockfd, int timeout) {
	//Imposta il timeout della socket in secondi
	struct timeval time;
	time.tv_sec = timeout;
	time.tv_usec = 0;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		printf("setsockopt set_timeout");
		exit(-1);
	}
}

void set_timeout(int sockfd, int timeout) {
	// Imposta il timeout della socket in microsecondi
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = timeout;
	if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time, sizeof(time)) < 0) {
		printf("setsockopt set_timeout");
		exit(-1);
	}
}


int create_socket(int timeout) {

	struct sockaddr_in new_addr;
	int sockfd;
	//creazione socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("SERVER: socket creation error\n");
		exit(-1);
	}
//
	//configurazione socket
	memset((void *)&new_addr, 0, sizeof(new_addr));
	new_addr.sin_family = AF_INET;
	new_addr.sin_port = htons(0);
	new_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//assegnazione indirizzo al socket
	if (bind(sockfd, (struct sockaddr *)&new_addr, sizeof(new_addr)) < 0) {
		printf("SERVER: socket bind error\n");
		exit(-1);
	}
	//set_timeout_sec(sockfd, timeout);
	return sockfd;
}

int files_from_folder_server(char *list_files[MAX_FILE_LIST]) {
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

  dp = opendir(SERVER_FOLDER);
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


int main(int argc, char **argv){
    int server_sock, child_sock;
    struct sockaddr_in server_address, client_address;
    socklen_t addr_len = sizeof(client_address);
    char *buff = calloc(PKT_SIZE, sizeof(char));
    char *path = calloc(PKT_SIZE, sizeof(char));
    // char *buffToSend = calloc(PKT_SIZE, sizeof(char));
		char *buffToSend = "CRISTALLINO";
    FILE *fptr;
    pid_t pid;
		int fd;
    int control, num_files;
		char *list_files[MAX_FILE_LIST];

    server_setup_conn(&server_sock, &server_address);

    while (1) {

      if (server_reliable_conn(server_sock, &client_address) == 0) {//se un client non riesce a ben connettersi, il server non forka
          control = sendto(server_sock, READY, strlen(READY), 0, (struct sockaddr *)&client_address, addr_len);
					printf("====================================================");
          if (control < 0){
						perror("Ready error");
            printf("SERVER: port comunication failed\n");
          }

 request:
          // set_timeout_sec(child_sock, REQUEST_SEC);
          printf("\nSERVER waiting for request....\n");
          memset(buff, 0, sizeof(buff));

          if (recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0){
            printf("SERVER %d: request failed\n", pid);
            free(buff);
            free(path);
            // close(serv)attesa syn;
            return 0;
          }
          printf ("Command Received: %d\n",*buff);

          switch(*(int*)buff) {

  					case LIST:
  						printf("SERVER: LIST request\n");
							num_files = files_from_folder_server(list_files);

							fd = open("file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
							if(fd<0){
								printf("SERVER: error opening file_list\n");
								// close(child_sock);
								return 1;
							}


							// Scrivo tutti i file in serverFiles nel file che verrà inviato al client
							i=0;
							while(i<num_files) {
								memset(buff, 0, sizeof(buff));
								snprintf(buff, strlen(list_files[i])+2, "%s\n", list_files[i]); //+2 per terminatore di stringa e \n
								write(fd, buff, strlen(buff));
								i++;
							}

							read(fd, (void *)&buffToSend, strlen(buffToSend));
              sender(server_sock, &client_address, FLYING, LOST_PROB, fd);
              close(fd);
							remove("file_list.txt");
							break;

            case GET:
              printf("SERVER: DOWNLOAD request\n");
              //set_timeout_sec(child_sock, SELECT_FILE_SEC); //Voglio sapere il nome del file
              memset(buff, 0, sizeof(buff));
              control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len); 
              if (control < 0) {
                printf("SERVER %d: file transfer failed (1)\n", pid);
                perror("ERROR");
                free(buff);
                free(path);
                close(child_sock);
                return 1;
              }
              //+1 per lo /0 altrimenti lo sostituisce a ultimo carattere
              snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff); 
              fd = open(path, O_RDONLY);
              printf("PATH: %s\n",path);
              printf("FD  : %d\n",fd);
              if(fd == -1){
                printf("SERVER %d: file not found\n", pid);
                //comunico al client che il file non è presente
                  if (sendto(server_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                      printf("SERVER %d: error sendto\n", pid);
                      return 1;
                  }
                free(buff);
                free(path);
                close(server_sock);
                return 1;
              }
              //comunico al client che il file è presente e può essere scaricato
                if (sendto(server_sock, FOUND, strlen(FOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
                    printf("SERVER %d: error sendto\n", pid);
                    return 1;
                }
              //set_timeout(child_sock, TIMEOUT_PKT);
              sender(server_sock, &client_address, FLYING, LOST_PROB, fd);          
              break;  
  
					case PUT:
						printf("SERVER %d: UPLOAD request\n", pid);
						//set_timeout_sec(child_sock, SELECT_FILE_SEC);//Voglio sapere nome del file
						memset(buff, 0, sizeof(buff));
						control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
						if (control < 0) {
							printf("SERVER %d: file transfer failed (1)\n", pid);
							free(buff);
							free(path);
							close(child_sock);
							return 1;
						}
						snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff);
						//controlla se il file esiste già
						fd = open(path, O_RDONLY);
						if(fd>0){
							printf("SERVER %d: The file already exists, you can not overwrite files on server.\n", pid);
							close(fd);
							return 1;
						}
						close(fd);
						//ricevi il file
						fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);
						//set_timeout(child_sock, TIMEOUT_PKT);
						control=receiver(server_sock, &client_address, FLYING, LOST_PROB, fd);
						if(control == -1) {
							close(fd);
							remove(path);
							free(buff);
							free(path);
							close(child_sock);
							return 1;
						}
						close(fd);
						break;   
  					}
            goto request;
        }
      }
    // }
    return 0;
}

void server_setup_conn( int *server_sock, struct sockaddr_in *server_addr){

  if ((*server_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("SERVER: socket creation error \n");
    exit(-1);
  }

  //configurazione socket
	memset((void *)server_addr, 0, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(SERVER_PORT);
	server_addr->sin_addr.s_addr = htonl(INADDR_ANY);


  /* htons = host-to-network
  converte porta da rappresentazione/binaria dell'indirizzo/numero di porta
  a valore binario da inserire nella struttura sockaddr_in*/

  /* associa al socket l'indirizzo e porta locali, serve a far sapere al SO a quale processo vanno inviati i dati ricevuti dalla rete*/
  /* sockfd = descrittore socket
      addr = puntatore a struck contentente l'indirizzo locale -> RICHIEDE struck sockadrr * addr
      len = dimensione in byte della struct sopra */


  if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
		printf("SERVER: socket bind error\n");
		exit(-1);
	}
}

int server_reliable_conn (int server_sock, struct sockaddr_in* client_addr) {

    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);

    //in attesa di ricevere SYN
		printf("\n================= CONNECTION SETUP =================\n");
		printf("%s SERVER: attesa syn\n", time_stamp());
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, SYN, strlen(SYN)) != 0) {
        printf("SERVER: connection failed (receiving SYN)\n");
        return 1;
    }
    // set_timeout_sec(server_sock, 1);//timeout attivato alla ricezione del SYN

    //invio del SYNACK
		printf("%s SERVER: invio SYNACK\n", time_stamp());

    control = sendto(server_sock, SYNACK, strlen(SYNACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER: connection failed (sending SYNACK)\n");
        return 1;
    }

    //in attesa del ACK_SYNACK
		printf("%s SERVER: attesa ack_synack\n", time_stamp());

    memset(buff, 0, sizeof(buff));
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, ACK_SYNACK, strlen(ACK_SYNACK)) != 0) {
        printf("SERVER: connection failed (receiving ACK_SYNACK)\n");
        return 1;
    }

    printf("%s SERVER: connection established\n", time_stamp());
    return 0;
}

// FUNZIONE PER time_stamp da mettere in utility.c
// aggiungere millisecondi o utilizzare un altro modo

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