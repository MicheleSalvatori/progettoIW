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

#define SERV_PORT   5193
#define MAXLINE     1024
#define PKT_SIZE 1500
#define MAX_FILE_LIST 100
#define MAX_NAMEFILE_LEN 127
#define SERVER_FOLDER "./serverFiles/"


typedef struct packet{
	int seq_num;
	short int pkt_dim;
	char data[PKT_SIZE-sizeof(int)-sizeof(short int)];
} packet;

int *check_pkt;
int err_count;//conta quante volte consecutivamente è fallita la ricezione
int tot_pkts, tot_ack, tot_sent;
int base, max, window, num_files,i;
packet *pkt;
char *list_files[MAX_FILE_LIST];
int fd;
off_t file_dim;

int getListFiles(char **);

int main(int argc, char **argv){
    int sockfd;
    socklen_t len;
    struct sockaddr_in addr;
    char buff[MAXLINE];
    char *buffToSend = "Risposta Server\n";
    FILE *fptr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))<0){
        perror ("Errore creazione socket");
        exit(1);
    }

    memset((void *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERV_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* htons = host-to-network
    converte porta da rappresentazione/binaria dell'indirizzo/numero di porta
    a valore binario da inserire nella struttura sockaddr_in*/


    /* associa al socket l'indirizzo e porta locali, serve a far sapere al SO a quale processo vanno inviati i dati ricevuti dalla rete*/
    /* sockfd = descrittore socket
        addr = puntatore a struck contentente l'indirizzo locale -> RICHIEDE struck sockadrr * addr
        len = dimensione in byte della struct sopra */

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr))<0) {
        perror ("Errore nell'esecuzione della funzione bind");
        exit(1);
    }

    /* UDP: recvfrom riceve i pacchetti da qualunque macchina
        -1 in caso di errore, altrimenti restituisce il numero di byte ricevuti
        FLAG posto a 0 ??*/

    while (1)
    {
        if (recvfrom(sockfd, buff, MAXLINE, 0, (struct sockaddr *)&addr, &len)<0){
            perror("Errore durante l'esecuzione della funzione recvfrom");
            exit(1);
        }

      num_files = getListFiles(list_files);
			fd = open("file_list.txt",O_CREAT | O_TRUNC | O_RDWR, 0666);
			if (fd < 0){
				printf("SERVER: error opening file_list\n");
				// close child_sock
				return 1;
			}

			i = 0;
			while (i < num_files){
				memset(buff, 0, sizeof(buff));
				snprintf(buff, strlen(list_files[i])+2, "%s\n", list_files[i]);  //+2 per terminatore di stringa e \n
				write(fd, buff, strlen(buff));
				i++;
			}

			pkt = calloc(32, sizeof(packet));

			// calcolo numero pkt necessari
			file_dim = lseek(fd, 0, SEEK_END);
			if(file_dim%(PKT_SIZE-sizeof(int)-sizeof(short int))==0){
				tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int));
			}
			else{
				tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int))+1;
			}
			lseek(fd, 0, SEEK_SET);

			//inizializzazione finestra di invio e primi pacchetti
			for(i=base; i<window; i++){
				pkt[i].seq_num = i;
				pkt[i].pkt_dim=read(fd, pkt[i].data, PKT_SIZE-sizeof(int)-sizeof(short int));
				if(pkt[i].pkt_dim==-1){
					pkt[i].pkt_dim=0;
				}
			}
			while(sendto(sockfd, pkt+i, PKT_SIZE, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
					printf("Packet send error(1)\n");
			}
			close(fd);
			remove("file_list.txt");
			
    }
    exit(0);
}

int getListFiles(char *list_files[MAX_FILE_LIST]){
	/*
	apre la cartella e prende tutti i nomi dei file presenti inserendoli in
	un buffer e ritornando il numero degli elementi presenti.
	*/

	DIR *dp;
	int i = 0;
	struct dirent *ep;
	for (; i < MAX_FILE_LIST; ++i){
		if ((list_files[i] = malloc(MAX_NAMEFILE_LEN * sizeof(char))) == NULL){
			perror("Errore malloc list_files");
			exit(EXIT_FAILURE);
		}
	}

	dp = opendir(SERVER_FOLDER);
	if (dp != NULL){
		i = 0;
		while ((ep = readdir(dp))){
			if (strncmp (ep -> d_name, ".", 1) != 0 && strncmp(ep->d_name, "..", 2) != 0){
				strncpy(list_files[i], ep->d_name, MAX_NAMEFILE_LEN);
        ++i;
			}
		}
		closedir(dp);
	}else{
		perror("Impossibile accedere alla directory");
	}
	return i;
}
