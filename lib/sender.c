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
#include "comm.h"

pthread_t thread;
struct thread_args
 {
    struct sockaddr_in *client_addr;
    socklen_t addr_len;
	int socket;
};

//TODO al secondo download (GET) senza disconnessione resta la vecchia sendbase mentre il numero di pacchetti è calcolato giusto, vedi output!!
//TODO non funziona piu il comando list perchè sendbase=windowend=0
//TODO rimettere windowend perche almeno imposto la dimensione della finestra al minimo se < WIN SIZE pkt totali

#define WIN_SIZE 32 			//Dimensione della finestra di trasmissione;
//#define MAX_TIMEOUT 800000 	//Valore in ms del timeout massimo
//#define MIN_TIMEOUT 800		//Valore in ms del timeout minimo

int SendBase = 0;		// Base della finestra di trasmissione: più piccolo numero di sequenza dei segmenti trasmessi ma di cui non si è ancora ricevuto ACK
int NextSeqNum = 0;		// Sequence Number del prossimo pkt da inviare, quindi primo pacchetto nella finestra ma non in volo
int ack_num;
int tot_acked = 0;
int tot_pkts = 0;
bool fileTransfer = true;

void send_reset(){ // Per trasferire un nuovo file senza disconnessione
	SendBase = 0;
	NextSeqNum = 0;
	ack_num = 0;
	tot_acked = 0;
	tot_pkts = 0;	
	fileTransfer = true;
}

void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N);

void *receive_ack(void *arg){
	printf ("______ THREAD CREATED ______\n");
	struct thread_args *args = arg;
	socklen_t addr_len = args->addr_len;
	struct sockaddr_in *client_addr = args->client_addr;
	int socket = args->socket;

	while(fileTransfer){
		printf ("______ THREAD In attesa di ACK ______\nFileTransfer = %d\n",fileTransfer);
		if (recvfrom(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)client_addr, &addr_len) < 0){
			perror ("Errore ricezione ack");
		}
		else {
			printf("Ricevuto ACK num: %d\n\n",ack_num);
			SendBase = ack_num+1;
			tot_acked = ack_num;
			if (tot_acked == tot_pkts-1){
				fileTransfer = false;
			}
		}
	}

}

void input_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}

int *check_pkt;
int num_packet_sent;
int base, max, window;	
packet *pkt;

void sender(int socket, struct sockaddr_in *receiver_addr, int N, int lost_prob, int fd) {
	send_reset();
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

	//inizio trasmissione
    num_packet_sent = 0;

	while(fileTransfer){ //while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		printf ("Ciclo While\nACKED: %d\nPKTS: %d\n\n",tot_acked,tot_pkts);
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

	printf("\n====== INIZIO SEND WINDOW ======\n\n");
	printf ("SendBase : %d\n",SendBase);
	printf ("WindowEnd: %d\n",SendBase+WIN_SIZE-1);
	printf("\n================================\n\n");
	int i, j;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	//input_wait("enter");

	// Caso in cui la finestra non è ancora piena di pkt in volo
	if(NextSeqNum<SendBase+WIN_SIZE-1){
		for(i=NextSeqNum; i<=SendBase+WIN_SIZE-1; i++){
			if(check_pkt[i]==0 && (num_packet_sent< tot_pkts)){
				if (sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
					perror ("PACKET LOST (1)");
				}
				else {
					printf("Inviato PKT num : %d\n", pkt[i].seq_num);
					num_packet_sent++;
					if (NextSeqNum!=tot_pkts-1){ //Altrimenti chiedo pkt 263 se ne ho 262
						NextSeqNum++;
					}
					
					printf("NextSeqNum: %d\n",NextSeqNum);
					check_pkt[i]=1;
				}
			}
			if (tot_pkts == 1){ //TEMPORANEO E SBAGLIATO
				fileTransfer = false;
				break;
			}
		}
	}
	printf("\n====== FINE SEND WINDOW ======\n\n");
	printf ("SendBase : %d\n",SendBase);
	printf ("WindowEnd: %d\n",SendBase+WIN_SIZE-1);
	printf("\n================================\n\n");
}

/*int recv_ack(int socket, struct sockaddr_in *client_addr, int fd, int N){//ritorna -1 se ci sono errori, 0 altrimenti
	int i, ack_num=0, new_read=0;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	
	//qualsiasi sia l'errore (timeout ecc...) --> err_count++;
	if(recvfrom(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)client_addr, &addr_len) < 0){
		//printf("Recv_ack error\n");
		err_count++;
		if(ADAPTIVE) {
			increase_timeout(socket);
		}

		if(base<max){//PROVA INVIO DI TUTTI I PACCHETTI NON ACKATI DELLA FINESTRA (NON FUNZIONA ANCORA)
			for(i=base; i<=max; i++){
				if(check_pkt[i] == 1) {
					tot_sent--;
					check_pkt[i]=0; //ritrasmetto pacchetti finestra
				}
			}
		}			if(check_pkt[i]==0 && (tot_sent < tot_pkts)){
				if(correct_send(lost_prob)) {
					while(sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len) < 0) {
					    printf("Packet send error(3)\n");
					}
					//printf("Packet %d sent\n", pkt[i].seq_num);
				}
				//else printf("Packet %d lost\n", pkt[i].seq_num);
				tot_sent++;
				check_pkt[i]=1;
					tot_sent--;
					check_pkt[i]=0; //ritrasmetto pacchetti finestra
				}
			}
			for(i=0; i<=max; i++){
				if(check_pkt[i] == 1) {
					tot_sent--;
					check_pkt[i]=0; //ritrasmetto pacchetti finestra
				}
			}
		}
		return -1;
	}
	if(ADAPTIVE) {
		decrease_timeout(socket);
	}
	//ricezione pacchetto avvenuta
	//printf("Recv_ack %d\n", ack_num);
	err_count = 0;
	//set variabile di controllo del pacchetto ricevuto ack_num
	if(base<max){
		if(base<=ack_num && ack_num<=max){//se mi arriva un ack fuori ordine, interno alla finestra
			if(check_pkt[ack_num]!=2){//se non era gia segnato come ACKato, segnalo
				check_pkt[ack_num]=2;
				tot_ack++;
			}
		}
	}
	else{					//se mi arriva un ack fuori ordine, interno alla finestra
		if(((0<=ack_num) && (ack_num<=max)) || ((base<=ack_num) && (ack_num<N))){
			if(check_pkt[ack_num]!=2){//se non era gia segnato come ACKato, segnalo
				check_pkt[ack_num]=2;
				tot_ack++;
			}
		}
	}
	//setto new_read al numero di pacchetti da leggere dal file
	if(base<max){
		for(i=base; i<=max; i++) {
			if(check_pkt[i] == 2) {
				check_pkt[i]=0; //reset variabile di controllo
				new_read++; //aumento di uno i pacchetti che dovrò leggere dal file
			}
			else{
				break;
			}
		}
	}
	else{
		for(i=base; i<N; i++) {
			if(check_pkt[i] == 2) {
				check_pkt[i]=0; //reset variabile di controllo
				new_read++; //aumento di uno i pacchetti che dovrò leggere dal file
			}
			else{
				break;
			}
		}
		if(window-new_read==max+1){//se i check==2 ancora possibili sono pari alla posizione max+1
			for(i=0; i<=max; i++) {
				if(check_pkt[i] == 2) {
					check_pkt[i]=0; //reset variabile di controllo
					new_read++; //aumento di uno i pacchetti che dovrò leggere dal file
				}
				else{
					break;
				}
			}
		}
	}

	//caricamento nuovi pacchetti
	for(i=0; i<new_read; i++){
		base=(base+1)%N;	//sposto la finestra di uno per ogni check==2 resettato
		max=(base+window-1)%N;  //eg. base=6-->max=(6+4-1)%8=9%8=1 (6 7 0 1)

		memset(pkt+max, 0, sizeof(packet));
		pkt[max].seq_num = max;
		pkt[max].pkt_dim=read(fd, pkt[max].data, PKT_SIZE-sizeof(int)-sizeof(short int));
		if(pkt[max].pkt_dim==-1){
			pkt[max].pkt_dim=0;
		}
	}
	
	return new_read; */