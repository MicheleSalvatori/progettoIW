#include "./lib/basic.h"


void client_setup_conn (int*, struct sockaddr_in*);
void client_reliable_conn (int, struct sockaddr_in*);
void client_reliable_close (int, struct sockaddr_in*); 
void* alarm_routine();

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

	signal(SIGALRM, (void(*) (int))alarm_routine);

	client_setup_conn(&client_sock, &server_address);

	client_reliable_conn(client_sock, &server_address);

	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(READY), 0, (struct sockaddr *)&server_address, &addr_len);
	if (control < 0) {
		printf("CLIENT: server dispatching failed\n");
		exit(-1);
	}
	
	printf("**************************\n");
	printf("CONFIGURATION PARAMETERS\nWindow = %d\nLoss probability = %d\nTimeout data = %d\n", FLYING, LOST_PROB, TIMEOUT_PKT);
	printf("**************************\n");
menu1:
	printf("Select request: (%d seconds)\n\n", REQUEST_SEC);
	alarm(REQUEST_SEC);
	printf("1) List available files on server\n2) Download file from server\n3) Upload file on server\n4) Quit\n");
	printf("Choose an operation: ");
	if(scanf("%d", &answer) > 0 && (answer == LIST || answer == GET || answer == PUT || answer == QUIT)){
		alarm(0);
	}
	printf("\n");

	switch (answer) {

		case LIST:
			control = sendto(client_sock, (void *)&list, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			fd = open("clientFiles/file_list.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
			set_timeout(client_sock, TIMEOUT_PKT);
			control = receiver(client_sock, &server_address, FLYING, LOST_PROB, fd);
			if(control == -1) {
				close(fd);
				remove("clientFiles/file_list.txt");
			}
			end_file = lseek(fd, 0, SEEK_END);
			if(end_file>0){
				lseek(fd, 0, SEEK_SET);
				read(fd, buff, end_file);
				printf("%s", buff);			
			}
			close(fd);
			remove("clientFiles/file_list.txt");
			printf("\n\n");
			break;

		case GET:
			control = sendto(client_sock, (void *)&get, sizeof(int), 0, (struct sockaddr *)&server_address, addr_len);
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			printf("Type the file name to download (30 seconds): ");
			alarm(SELECT_FILE_SEC);
			memset(buff, 0, sizeof(buff));
			if(scanf("%s", buff)>0) {
				alarm(0);
			}
			
			//controlla se il file esiste già 
			char *aux = calloc(PKT_SIZE, sizeof(char));
			snprintf(aux, 12+strlen(buff)+1, "clientFiles/%s", buff);
			fd = open(aux, O_RDONLY);
			if(fd>0){
				printf("CLIENT: The file already exists\n");
				close(fd);
				exit(-1);
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
			set_timeout(client_sock, TIMEOUT_PKT);
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

			printf("\nType the file name to upload (30 seconds): ");
			alarm(SELECT_FILE_SEC);
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
			set_timeout(client_sock, TIMEOUT_PKT);
			sender(client_sock, &server_address, FLYING, LOST_PROB, fd);
			break;

		case QUIT:
			control = sendto(client_sock, (void *)&quit, sizeof(int), 0, (struct sockaddr *)&server_address, sizeof(server_address));
			if (control < 0) {
				printf("CLIENT: request failed (sending)\n");
				exit(-1);
			}
			client_reliable_close(client_sock, &server_address);
			return 0;

		default:
			printf("Wrong type\n\n");
			break;
	}
	goto menu1;
	return 0;
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
	set_timeout_sec(client_sock, 1);
	control = sendto(client_sock, SYN, strlen(SYN), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending SYN)\n");
		exit(-1);
	}

	//in attesa del SYNACK
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(SYNACK), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, SYNACK, strlen(SYNACK)) != 0) {
		printf("CLIENT: connection failed (receiving SYNACK)\n");
		exit(-1);
	}

	//invio del ACK_SYNACK
	//sleep(1); //1 secondo prima di inviare SYNACK
	control = sendto(client_sock, ACK_SYNACK, strlen(ACK_SYNACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: connection failed (sending ACK_SYNACK)\n");
		exit(-1);
	}		

	printf("CLIENT: connection established\n");
}

void client_reliable_close (int client_sock, struct sockaddr_in *server_addr) {
	
	int control;
	char *buff = calloc(PKT_SIZE, sizeof(char));
	socklen_t addr_len = sizeof(*server_addr);

	//passo 1 della chiusura affidabile
	set_timeout_sec(client_sock, 1);
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

	//in attesa del FIN
	memset(buff, 0, sizeof(buff));
	control = recvfrom(client_sock, buff, strlen(FIN), 0, (struct sockaddr *)server_addr, &addr_len);
	if (control < 0 || strncmp(buff, FIN, strlen(FIN)) != 0) {
		printf("CLIENT: close connection failed (receiving FIN)\n");
		exit(-1);
	}

	//invio del FINACK
	control = sendto(client_sock, FINACK, strlen(FINACK), 0, (struct sockaddr *)server_addr, addr_len);
	if (control < 0) {
		printf("CLIENT: close connection failed (sending FINACK)\n");
		exit(-1);
	}		

	printf("CLIENT: connection closed\n");
}

void* alarm_routine(){
    printf("\nTimeout expired\n");
    exit(-1);
}
