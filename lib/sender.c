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

#define WIN_SIZE 15 			//Dimensione della finestra di trasmissione;
#define TO_MICRO 200000

/* ACK CUMULATIVO UTILIZZATO: Inviare un ACK = N indica che tutti i segmenti fino a N-1 sono stati ricevuti e che ora aspetto il byute numero N

*/

int SendBase;		// Base della finestra di trasmissione: più piccolo numero di sequenza dei segmenti trasmessi ma di cui non si è ancora ricevuto ACK
int NextSeqNum;		// Sequence Number del prossimo pkt da inviare, quindi primo pacchetto nella finestra ma non in volo
int WindowEnd;
int ack_num;
int tot_acked;
int tot_pkts;
bool fileTransfer = true;
bool isTimerStarted = false;
struct itimerval it_val;
struct timeval end, start;
socklen_t addr_len;
struct sockaddr_in *client_addr;
off_t file_dim;

int sock;

int *check_pkt;
packet *pkt;

void timeout_routine();
void set_timer_send(int sec,int micro);
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N);

void end_transmission(){
	printf("====== Transmission end =======\n");
	set_timer_send(0,0); //stop timer
	printf("File transfer finished\n");
	gettimeofday(&end, NULL);
	double tm=end.tv_sec-start.tv_sec+(double)(end.tv_usec-start.tv_usec)/1000000;
	double tp=file_dim/tm;
	printf("Transfer time: %f sec [%f KB/s]\n", tm, tp/1024);
	printf("===========================\n");
}

void set_timer_send(int sec,int micro){
    struct itimerval it_val;
    
        it_val.it_value.tv_sec = sec;
    	it_val.it_value.tv_usec = micro;
    	it_val.it_interval.tv_sec = 0;
	    it_val.it_interval.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
  			perror("setitimer");
			exit(1);
			}
		// printf("Timer avviato\n");
}

void fast_retrasmission(int rtx_seq){
	if (sendto(sock, pkt+rtx_seq, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
		perror("Errore ritrasmissione pkt");
	}else{
		printf("FAST RETRANSMIT -> PKT: %d\n", (pkt+rtx_seq)->seq_num);
	}
}

void print_packet_status (int seq){
	char *status;
	if (check_pkt[seq-1] == 0){
		status = "Da Inviare";
	}
	else if (check_pkt[seq-1] == 1){
		status = "Inviato non Acked";
	}
	else if (check_pkt[seq-1] == 2){
		status = "Già Acked";
	}
	printf ("Stato PKT %d | %s\n",seq,status);
}

void print_window_status(){							//STAMPA LO STATO DI INVIO DEI PKT PER IL DEBUG
	for (int j = SendBase;j<SendBase+20;j++){
		if (j == tot_pkts){
			break;
		}
		print_packet_status (j);
	}
}

void cumulative_ack(int received_ack){
	for (int k = 0; k<received_ack-1; k++){
		check_pkt[k] = 2;
	}	
}

void initialize_send(){ // Per trasferire un nuovo file senza disconnessione
	SendBase = 1;
	NextSeqNum = 1;
	ack_num = 0;
	tot_acked = 0;
	tot_pkts = 0;	
	fileTransfer = true;
	isTimerStarted = false;
}

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
		if (ack_num>SendBase){
			printf ("Ricevuto ACK numero: %d\n",ack_num);
			printf ("Incrementato SendBase | %d -> %d\n\n",SendBase,ack_num);
			SendBase = ack_num;
			tot_acked = ack_num-1;
			duplicate_ack_count = 1;
			cumulative_ack(ack_num);
			if (WindowEnd - SendBase >0){
				set_timer_send(0,TO_MICRO);
				isTimerStarted = true;
			}
			if (tot_acked == tot_pkts){
				fileTransfer = false; //Stoppa il thread e l'invio dei pacchetti se arrivati alla fine del file
				end_transmission();
			}
		}
		else {
			printf ("Ricevuto ACK duplicato: %d\n",ack_num);
			duplicate_ack_count++;
			if (duplicate_ack_count == 3){
				printf ("\n\n !!! TRE ACK DUPLICATI !!! | ACK: %d\n\n",ack_num);
				fast_retrasmission(ack_num-1);
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
	sock = socket;
	initialize_send();
	printf ("SENDER SEND BASE: %d\n",SendBase);
	client_addr = receiver_addr;
	addr_len = sizeof(struct sockaddr_in);
	char *buff = calloc(PKT_SIZE, sizeof(char));
	int i;

	struct thread_args t_args;
	t_args.addr_len = addr_len;
	t_args.client_addr = receiver_addr;
	t_args.socket = socket;

	
	int ret = pthread_create(&thread,NULL,receive_ack,(void*)&t_args); //Creazione thread per ascolto ricezione ack
	
	srand(time(NULL));
	
	//calcolo tot_pkts
	file_dim = lseek(fd, 0, SEEK_END);
	int pkt_data_size = PKT_SIZE-2*sizeof(int)-sizeof(short int);
	if(file_dim%pkt_data_size==0){
		tot_pkts = file_dim/pkt_data_size;
	}
	else{
		tot_pkts = (file_dim/pkt_data_size)+1;
	}
	printf("\n====== INIZIO DEL SENDER | PKTS: %d ======\n\n",tot_pkts);

	pkt=calloc(tot_pkts, sizeof(packet));
	check_pkt=calloc(tot_pkts, sizeof(int));
	lseek(fd, 0, SEEK_SET);

	gettimeofday(&start, NULL);

	// ASSEGNAZIONE DEI NUMERI DI SEQUENZA AI PACCHETTI
	for(i=0; i<tot_pkts; i++){
		pkt[i].seq_num = i+1;
		pkt[i].num_pkts = tot_pkts;
		pkt[i].pkt_dim=read(fd, pkt[i].data, pkt_data_size);
		printf ("%d | %d\n",i,pkt[i].pkt_dim);
		if(pkt[i].pkt_dim==-1){
			pkt[i].pkt_dim=0;
		}
	}
	input_wait("INIZIA TRASMISSIONE");

	//INIZIO TRASMISSIONE PACCHETTI
	while(fileTransfer){ //while ho pachetti da inviare
		send_window(socket, receiver_addr, pkt, lost_prob, WIN_SIZE);
	}
}

//Invia tutti i pacchetti nella finestra
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N){
	WindowEnd = MIN(tot_pkts,SendBase+WIN_SIZE-1);
	signal(SIGALRM, timeout_routine);

	//printf("\n====== INIZIO SEND WINDOW ======\n\n");
	//printf ("SendBase : %d\n",SendBase);
	//printf ("WindowEnd: %d\n",WindowEnd);
	//printf("\n================================\n\n");
	int i, j;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	// Caso in cui la finestra non è ancora piena di pkt in volo
		
	for(i=NextSeqNum-1; i<WindowEnd; i++){					// ho messo -1 non so perchè
		WindowEnd = MIN(tot_pkts,SendBase+WIN_SIZE-1);
		//input_wait("!! CONTINUE !!\n");

		if(check_pkt[i]==0){
			if (!isTimerStarted){
				set_timer_send(0,TO_MICRO);
				isTimerStarted = true;
			}
			if (sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
				perror ("PACKET LOST (1)");
			}
			else {
				printf("Inviato PKT num: %d\n", pkt[i].seq_num);
				NextSeqNum++;
				check_pkt[i]=1;
			}
		}
	}
	//print_send_status();
	//printf("\n====== FINE SEND WINDOW ======\n\n");
}

void timeout_routine(){
	printf("\nTimer Scaduto, PKT perso\n");
	fast_retrasmission(SendBase-1);
	set_timer_send(0,TO_MICRO);
	isTimerStarted = true;
	return;
}