#include "./lib/basic.h"


void server_setup_conn (int*, struct sockaddr_in*);
int server_reliable_conn (int, struct sockaddr_in*);
void server_reliable_close (int, struct sockaddr_in*, int);

int main (int argc, char** argv) {
	
	int answer, control, bytes, num_files;
	int server_sock, child_sock;
	struct sockaddr_in server_address, client_address;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	char *path = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(client_address);
	pid_t pid;
	int fd;
	off_t end_file, file_control;
	char *list_files[MAX_FILE_LIST];

	server_setup_conn(&server_sock, &server_address);
	
	while(1) {
		set_timeout(server_sock, 0);//recvfrom all'inizio è bloccante (si fa con timeout==0)
		
		if (server_reliable_conn(server_sock, &client_address) == 0) {//se un client non riesce a ben connettersi, il server non forka
			pid = fork();
			
			if (pid < 0) {
				printf("SERVER: fork error\n");
			}

			if (pid == 0) {
				pid = getpid();
				child_sock = create_socket(REQUEST_SEC);//REQUEST_SEC secondi di timeout per scegliere il servizio

				control = sendto(child_sock, READY, strlen(READY), 0, (struct sockaddr *)&client_address, addr_len);
				if (control < 0) {
					printf("SERVER %d: port comunication failed\n", pid);
				}
request:
				set_timeout_sec(child_sock, REQUEST_SEC);
				printf("\n\nSERVER %d: waiting for request...\n", pid);
				memset(buff, 0, sizeof(buff));
				if (recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len) < 0) {
					printf("SERVER %d: request failed\n", pid);
					free(buff);
					free(path);
					close(child_sock);
					return 0;
				}

				switch(*(int*)buff) {

					case LIST:
						printf("SERVER %d: LIST request\n", pid);
						num_files = files_from_folder_server(list_files);
						fd = open("file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
						if(fd<0){
							printf("SERVER: error opening file_list\n");
							close(child_sock);
							return 1;
						}

						i=0;
						while(i<num_files) {
							memset(buff, 0, sizeof(buff));
							snprintf(buff, strlen(list_files[i])+2, "%s\n", list_files[i]); //+2 per terminatore di stringa e \n
							write(fd, buff, strlen(buff));
							i++;
						}						

						set_timeout(child_sock, TIMEOUT_PKT);
						sender(child_sock, &client_address, FLYING, LOST_PROB, fd);
						close(fd);
						remove("file_list.txt");
						break;

					case GET:
						printf("SERVER %d: DOWNLOAD request\n", pid);
						set_timeout_sec(child_sock, SELECT_FILE_SEC);//Voglio sapere il nome del file
						memset(buff, 0, sizeof(buff));
						control = recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len); 
						if (control < 0) {
							printf("SERVER %d: file transfer failed (1)\n", pid);
							free(buff);
							free(path);
							close(child_sock);
							return 1;
						}
						//+1 per lo /0 altrimenti lo sostituisce a ultimo carattere
						snprintf(path, 12+strlen(buff)+1, "serverFiles/%s", buff); 
						fd = open(path, O_RDONLY);
						if(fd == -1){
							printf("SERVER %d: file not found\n", pid);
							//comunico al client che il file non è presente
						    if (sendto(server_sock, NFOUND, strlen(NFOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
						        printf("SERVER %d: error sendto\n", pid);
						        return 1;
						    }
							free(buff);
							free(path);
							close(child_sock);
							return 1;
						}
						//comunico al client che il file è presente e può essere scaricato
					    if (sendto(server_sock, FOUND, strlen(FOUND), 0, (struct sockaddr *)&client_address, addr_len) < 0) {
					        printf("SERVER %d: error sendto\n", pid);
					        return 1;
					    }
						set_timeout(child_sock, TIMEOUT_PKT);
						sender(child_sock, &client_address, FLYING, LOST_PROB, fd);
						close(fd);
						break;

					case PUT:
						printf("SERVER %d: UPLOAD request\n", pid);
						set_timeout_sec(child_sock, SELECT_FILE_SEC);//Voglio sapere nome del file
						memset(buff, 0, sizeof(buff));
						control = recvfrom(child_sock, buff, PKT_SIZE, 0, (struct sockaddr *)&client_address, &addr_len);
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
							printf("SERVER %d: The file already exists\n", pid);
							close(fd);
							return 1;
						}
						close(fd);
						//ricevi il file
						fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0666);
						set_timeout(child_sock, TIMEOUT_PKT);
						control=receiver(child_sock, &client_address, FLYING, LOST_PROB, fd);
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

					case QUIT:
						server_reliable_close(child_sock, &client_address, pid);
						free(buff);
						free(path);
						close(child_sock);
						return 0;

					default:
						printf("Wrong request\n\n");
						break;
				}				
				goto request;
			}
		}		
	}
	return 0;
}





void server_setup_conn (int *server_sock, struct sockaddr_in *server_addr) {
	//creazione socket
	if ((*server_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("SERVER: socket creation error\n");
		exit(-1);
	}

	//configurazione socket
	memset((void *)server_addr, 0, sizeof(*server_addr));
	server_addr->sin_family = AF_INET;
	server_addr->sin_port = htons(SERVER_PORT);
	server_addr->sin_addr.s_addr = htonl(INADDR_ANY);

	//assegnazione indirizzo al socket
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
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, SYN, strlen(SYN)) != 0) {
        printf("SERVER: connection failed (receiving SYN)\n");
        return 1;
    }
    set_timeout_sec(server_sock, 1);//timeout attivato alla ricezione del SYN
 
    //invio del SYNACK
    control = sendto(server_sock, SYNACK, strlen(SYNACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER: connection failed (sending SYNACK)\n");
        return 1;
    }
 
    //in attesa del ACK_SYNACK
    memset(buff, 0, sizeof(buff));
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, ACK_SYNACK, strlen(ACK_SYNACK)) != 0) {
        printf("SERVER: connection failed (receiving ACK_SYNACK)\n");
        return 1;
    }
 
    printf("SERVER: connection established\n");
    return 0;
}

void server_reliable_close (int server_sock, struct sockaddr_in* client_addr, int pid) {
    
    int control;
    char *buff = calloc(PKT_SIZE, sizeof(char));
    socklen_t addr_len = sizeof(*client_addr);
 
    set_timeout_sec(server_sock, 1);//timeout attivato per ricezione FIN
   
    //in attesa di ricevere FIN
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
        printf("SERVER %d: close connection failed (receiving FIN)\n", pid);
        return;
    }
 	
    //invio del FINACK
    control = sendto(server_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER %d: close connection failed (sending FINACK)\n", pid);
        return;
    }
 
	control = sendto(server_sock, FIN, strlen(FIN), 0, (struct sockaddr *)client_addr, addr_len);
    if (control < 0) {
        printf("SERVER %d: close connection failed (sending FIN)\n", pid);
        return;
    }

    //in attesa del FINACK
    memset(buff, 0, sizeof(buff));
    control = recvfrom(server_sock, buff, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);
    if (control < 0 || strncmp(buff, FINACK, strlen(FINACK)) != 0) {
        printf("SERVER %d: close connection failed (receiving FINACK)\n", pid);
        return;
    }
 
    printf("SERVER %d: connection closed\n", pid);
    return;
}


 
