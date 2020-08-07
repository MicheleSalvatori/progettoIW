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
#include "comm.h"

#define WIN_SIZE 5 			//Dimensione della finestra di trasmissione;
//#define MAX_TIMEOUT 800000 	//Valore in ms del timeout massimo
//#define MIN_TIMEOUT 800		//Valore in ms del timeout minimo

void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N);

void input_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}

int *check_pkt;
int err_count;//conta quante volte consecutivamente è fallita la ricezione
int tot_pkts, tot_ack, num_packet_sent;
int base, max, window;
int ack_num;

int SendBase = 0;		// Base della finestra di trasmissione (pkt piu vecchio non acked)
int NextSeqNum = 0;
int WindowEnd = WIN_SIZE-1;		// Seq number atteso (ult)
packet *pkt;

void sender(int socket, struct sockaddr_in *receiver_addr, int N, int lost_prob, int fd) {
	socklen_t addr_len = sizeof(struct sockaddr_in);
	char *buff = calloc(PKT_SIZE, sizeof(char));
	int i, new_read;
	off_t file_dim;
	
	srand(time(NULL));

    printf("\n====== INIZIO DEL SENDER ======\n\n");
	
	//calcolo tot_pkts
	file_dim = lseek(fd, 0, SEEK_END);
	if(file_dim%(PKT_SIZE-sizeof(int)-sizeof(short int))==0){
		tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int));
	}
	else{
		tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int))+1;
	}
	pkt=calloc(tot_pkts, sizeof(packet));
	check_pkt=calloc(tot_pkts, sizeof(int));//0=da inviare, 1 inviato non ackato, 2 ackato. non serve che ruoti.		//MOMENTANEO

	printf ("Numero Pacchetti: %d\n",tot_pkts);
	lseek(fd, 0, SEEK_SET);

	struct timeval end, start;
	gettimeofday(&start, NULL);

	// Preparazione primi WIN_SIZE pkt
	for(i=0; i<tot_pkts; i++){
		pkt[i].seq_num = i;
		pkt[i].pkt_dim=read(fd, pkt[i].data, PKT_SIZE-sizeof(int)-sizeof(short int));
		if(pkt[i].pkt_dim==-1){
			pkt[i].pkt_dim=0;
		}
	}

	//inizio trasmissione
	//tot_ack=0;
	//tot_sent=0;
	//err_count=0;
    num_packet_sent = 0;
	int tot_acked = 0;

	while(num_packet_sent<tot_pkts){ //while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		WindowEnd = SendBase + WIN_SIZE-1;
		if (WindowEnd >= tot_pkts){		// DA CAMBIARE
			WindowEnd = tot_pkts-1;
		}
		send_window(socket, receiver_addr, pkt, lost_prob, WIN_SIZE);
		//input_wait("premi invio\n");
		
		/*
		//Invio dei oacchetti semplice senza finestra
        if (sendto(socket, pkt+num_packet_sent, PKT_SIZE, 0, (struct sockaddr *)receiver_addr, addr_len)<0){
			printf ("Packet error, pkt num: %d\n",num_packet_sent);
			perror ("Error Message");
			exit(-1);
		}
		else{
			printf("Packet %d\n", pkt[num_packet_sent].seq_num);
			printf("Packet Sent Num: %d\n",num_packet_sent);
		}
		num_packet_sent++;	
		
		if (recvfrom(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)receiver_addr, &addr_len)<0){
			printf("ACK NUMBER: %d\n\n",ack_num);
			perror("ACK RECVFROM ERROR");
		}
		else {
			printf("\nACK RICEVUTO: %d\n",ack_num);void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N);
			printf("Ricevuto correttamente pkt: %d\n",ack_num-1);
			printf("In attesa del packt numero: %d\n\n",ack_num);
			tot_acked++;
		}*/
	}


	// Ciclo funzionante su tutti i pkt (senza finestra e sendbase)
	/*while(num_packet_sent<tot_pkts){ //while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		//send_window(socket, receiver_addr, pkt, lost_prob, N);
        if (sendto(socket, pkt+num_packet_sent, PKT_SIZE, 0, (struct sockaddr *)receiver_addr, addr_len)<0){
			printf ("Packet error, pkt num: %d\n",num_packet_sent);
			perror ("Error Message");
			exit(-1);
		}
        num_packet_sent++;	
		if (recvfrom(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)receiver_addr, &addr_len)<0){
			printf("ACK NUMBER: %d\n\n",ack_num);
			perror("ACK RECVFROM ERROR");
		}
		else {
			printf("\nACK RICEVUTO: %d\n",ack_num);
			printf("Ricevuto correttamente pkt: %d\n",ack_num-1);
			printf("In attesa del packt numero: %d\n\n",ack_num);
		}
	}*/
	
	//fine trasmissione
	for(i=0; i<MAX_ERR; i++){
		printf("Transmission end\n");
		memset(buff, 0, PKT_SIZE);
		((packet*)buff)->seq_num=-1;
		if(sendto(socket, buff, sizeof(int), 0, (struct sockaddr *)receiver_addr, addr_len) > 0) {
			printf("File transfer finished\n");
			gettimeofday(&end, NULL);
			double tm=end.tv_sec-start.tv_sec+(double)(end.tv_usec-start.tv_usec)/1000000;
			double tp=file_dim/tm;
			printf("Transfer time: %f sec [%f KB/s]\n", tm, tp/1024);
			close(fd);
			return;
		}
	}
	close(fd);
	return;
}

//Invia tutti i pacchetti nella finestra
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N){

	printf("\n====== INIZIO SEND WINDOW ======\n\n");
	int i, j;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	printf ("SendBase : %d\n",SendBase);
	printf ("WindowEnd: %d\n",WindowEnd);

	// Caso in cui la finestra non è ancora piena di pkt in volo
	if(SendBase<WindowEnd){
		printf("Case: SendBase<WindowEnd\n"); 
		for(i=SendBase; i<=WindowEnd; i++){
			printf("check_pkt[%d]:%d\n",i, check_pkt[i]);
			if(check_pkt[i]==0 && (num_packet_sent< tot_pkts)){
				if (sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len)<0){
					perror ("PACKET LOST (1)");
				}
				else {
					printf("Packet seq: %d sent\n", pkt[i].seq_num);
				}
				SendBase++;
				num_packet_sent++;
				check_pkt[i]=1;
				}
			}

	}





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
	
	return new_read;
} */