#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/param.h>
#include "comm.h"

pthread_t thread;
struct thread_args
 {
    struct sockaddr_in *client_addr;
    socklen_t addr_len;
	int socket;
};

#define WIN_SIZE 5 			//Dimensione della finestra di trasmissione;
//#define MAX_TIMEOUT 800000 	//Valore in ms del timeout massimo
//#define MIN_TIMEOUT 800		//Valore in ms del timeout minimo

/* ACK CUMULATIVO UTILIZZATO: Inviare un ACK = N indica che tutti i segmenti fino a N-1 sono

*/

int SendBase;		// Base della finestra di trasmissione: più piccolo numero di sequenza dei segmenti trasmessi ma di cui non si è ancora ricevuto ACK
int NextSeqNum;		// Sequence Number del prossimo pkt da inviare, quindi primo pacchetto nella finestra ma non in volo
int ack_num;
int tot_acked;
int tot_pkts;
bool fileTransfer = true;

int *check_pkt;
packet *pkt;

void initialize_send(){ // Per trasferire un nuovo file senza disconnessione
	SendBase = 0;
	NextSeqNum = 0;
	ack_num = 0;
	tot_acked = 0;
	tot_pkts = 0;	
	fileTransfer = true;
}

void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N);

void *receive_ack(void *arg){
	struct thread_args *args = arg;
	socklen_t addr_len = args->addr_len;
	struct sockaddr_in *client_addr = args->client_addr;
	int socket = args->socket;

	int duplicate_ack_count = 1;

	while(fileTransfer){
		if (recvfrom(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)client_addr, &addr_len) < 0){
			perror ("Errore ricezione ack");
			exit(-1);
		}
		printf ("THREAD: Ricevuto ACK | DuplicateAckCount = %d\n",duplicate_ack_count);
		if (ack_num+1>SendBase){
			printf("Ricevuto ACK num: %d\n\n",ack_num);
			SendBase = ack_num+1;
			tot_acked = ack_num;
			duplicate_ack_count = 1;
			if (tot_acked == tot_pkts-1){
				fileTransfer = false; //Stoppa il thread e l'invio dei pacchetti se arrivati alla fine del file
			}
		}
		else {
			printf ("Ricevuto ACK duplicato: %d\n",ack_num);
			duplicate_ack_count++;
			if (duplicate_ack_count == 3){
				printf ("\n\n !!! TRE ACK DUPLICATI !!! | ACK: %d\n\n",ack_num);
				printf ("FAST RETRANSMISSION PKT: %d\n",(pkt+ack_num+1)->seq_num);
				if (sendto(socket, pkt+ack_num+1, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
					perror("Errore ritrasmissione pkt");
				};
				duplicate_ack_count = 1;
			}
		}
	}
}

void input_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}

void sender(int socket, struct sockaddr_in *receiver_addr, int N, int lost_prob, int fd) {
	initialize_send();
	printf ("SENDER SEND BASE: %d\n",SendBase);
	socklen_t addr_len = sizeof(struct sockaddr_in);
	char *buff = calloc(PKT_SIZE, sizeof(char));
	int i, new_read;
	off_t file_dim;

	struct thread_args t_args;
	t_args.addr_len = addr_len;
	t_args.client_addr = receiver_addr;
	t_args.socket = socket;

	
	int ret = pthread_create(&thread,NULL,receive_ack,(void*)&t_args); //Creazione thread per ascolto ricezione ack
	
	srand(time(NULL));
	
	//calcolo tot_pkts
	file_dim = lseek(fd, 0, SEEK_END);
	if(file_dim%(PKT_SIZE-sizeof(int)-sizeof(short int))==0){
		printf ("Calcolo tot_pkts\n");
		tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int));
	}
	else{
		printf ("Calcolo tot_pkts\n");
		tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int))+1;
	}
	printf("\n====== INIZIO DEL SENDER | PKTS: %d ======\n\n",tot_pkts);

	pkt=calloc(tot_pkts, sizeof(packet));
	check_pkt=calloc(tot_pkts, sizeof(int));//0=da inviare, 1 inviato non ackato, 2 ackato. non serve che ruoti.		//MOMENTANEO

	printf ("Numero Pacchetti: %d\n",tot_pkts);
	lseek(fd, 0, SEEK_SET);

	struct timeval end, start;
	gettimeofday(&start, NULL);

	// Assegno i seq num ai pkt
	for(i=0; i<tot_pkts; i++){
		pkt[i].seq_num = i;
		pkt[i].pkt_dim=read(fd, pkt[i].data, PKT_SIZE-sizeof(int)-sizeof(short int));
		if(pkt[i].pkt_dim==-1){
			pkt[i].pkt_dim=0;
		}
	}

	//INIZIO TRASMISSIONE PACCHETTI
	while(fileTransfer){ //while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		printf ("Ciclo While\nACKED: %d\nPKTS: %d\n\n",tot_acked,tot_pkts);
		input_wait("CONTINUE");
		if (SendBase+WIN_SIZE-1 >= tot_pkts && tot_pkts >= WIN_SIZE){		// DA CAMBIARE
			SendBase = tot_pkts-(WIN_SIZE-1);	//Tolto tot_pkts-1 perche non funge con invio di un file con 1 solo pkt (funziona anche con video.mp4 daje)
		}
		send_window(socket, receiver_addr, pkt, lost_prob, WIN_SIZE);
	}
	
	//fine trasmissione
	for(i=0; i<MAX_ERR; i++){ //Perche i<MAX_ERR? non ci serve
		printf("====== Transmission end =======\n");
		memset(buff, 0, PKT_SIZE);
		((packet*)buff)->seq_num=-1;
		if(sendto(socket, buff, sizeof(int), 0, (struct sockaddr *)receiver_addr, addr_len) > 0) {
			printf("File transfer finished\n");
			gettimeofday(&end, NULL);
			double tm=end.tv_sec-start.tv_sec+(double)(end.tv_usec-start.tv_usec)/1000000;
			double tp=file_dim/tm;
			printf("Transfer time: %f sec [%f KB/s]\n", tm, tp/1024);
			printf("===========================\n");
			tot_pkts = 0;
			if (close(fd)<0){
                printf ("File closing error with fd = %d\n",fd);
                perror("error (2)");
              }
              else {
                printf ("File closed\n");
              }
			return;
		}
	}
}

//Invia tutti i pacchetti nella finestra
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N){
	int cycle_end;
	cycle_end = MIN(tot_pkts,SendBase+WIN_SIZE-1);

	printf("\n====== INIZIO SEND WINDOW ======\n\n");
	printf ("SendBase : %d\n",SendBase);
	printf ("WindowEnd: %d\n",cycle_end);
	printf("\n================================\n\n");
	int i, j;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	
	//input_wait("enter");

	// Caso in cui la finestra non è ancora piena di pkt in volo
	if(NextSeqNum<SendBase+WIN_SIZE-1){
		
		for(i=NextSeqNum; i<cycle_end; i++){
			cycle_end = MIN(tot_pkts,SendBase+WIN_SIZE-1);
			if(check_pkt[i]==0){
				if (sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
					perror ("PACKET LOST (1)");
				}
				else {
					printf("Inviato PKT num : %d\n", pkt[i].seq_num);
					if (NextSeqNum!=tot_pkts-1){ //Altrimenti chiedo pkt 263 se ne ho 262
						NextSeqNum++;
					}
					
					printf("NextSeqNum: %d\n",NextSeqNum);
					check_pkt[i]=1;
				}
			}
		}
	}
	printf("\n====== FINE SEND WINDOW ======\n\n");
	printf ("SendBase : %d\n",SendBase);
	printf ("WindowEnd: %d\n",SendBase+WIN_SIZE-1);
	printf("\n================================\n\n");
}