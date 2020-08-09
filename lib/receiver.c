#include "comm.h"

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
#include <signal.h>
#include <fcntl.h>

#define WIN_SIZE 5

int error_count;
int *check_pkt;
int base, max, window;
int ReceiveBase;
int WindowEnd;
int seq_num = 0;
int new_write = 0;
int expected_seq_num = 0;
packet pkt_aux;
packet *pkt;

void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N);
void alarm_routine();
void checkSegment( struct sockaddr_in *, int);

void inputs_wait(char *s){
	char c;
	printf("%s\n", s);
	while (c = getchar() != '\n');
}

int receiver(int socket, struct sockaddr_in *sender_addr, int N, int loss_prob, int fd){
	socklen_t addr_len=sizeof(struct sockaddr_in);
	off_t file_dim;
	long i = 0;
	


	printf("\n====== INIZIO DEL RECEIVER ======\n\n");
	printf("File transfer started\nWait...\n");

	pkt=calloc(6000, sizeof(packet));		// al posto di 6000 va la dim finestra
	check_pkt=calloc(6000, sizeof(packet));
	memset(&pkt_aux, 0, sizeof(packet));
	
	ReceiveBase = 0;

	while(seq_num!=-1){//while ho pachetti da ricevere
		WindowEnd = ReceiveBase + WIN_SIZE-1;
		// recv_window(socket, sender_addr, pkt, fd, WIN_SIZE);

		memset(&pkt_aux, 0, sizeof(packet));

		if((recvfrom(socket, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)sender_addr, &addr_len)<0)) {
			perror("error receive pkt ");
			return -1;
		}
		seq_num = pkt_aux.seq_num;
		printf("SeqNum: %d\n",seq_num);
		checkSegment(sender_addr, socket);
	}

	//scrittura nuovi pacchetti
	printf("====== SCRITTURA FILE ======\nPacchetti da scrivere: %d\n", new_write);
	for(i=0; i<new_write; i++){
		write(fd, pkt[i].data, pkt[i].pkt_dim); //scrivo un pacchetto alla volta in ordine sul file
		printf("Scritto pkt | SEQ: %d, IND: %ld\n", pkt[i].seq_num, i);
		// base=(base+1)%N;	//sposto la finestra di uno per ogni check==1 resettato
		// max=(base+window-1)%N;
	}
	printf("============================\n");
}


// TODO funzioni gestione timer in file utility.c
	
void checkSegment(struct sockaddr_in *client_addr, int socket){

	struct itimerval it_val;
	packet new_pkt;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	int lastToAck = 0;

	signal(SIGALRM, alarm_routine);
	memset(&new_pkt, 0, sizeof(packet));

//se è il pacchetto di fine file, termino
	if(seq_num ==-1) {
		printf("-----1");
		return;
	}

	// Arrivo ordinato di segmento con numero di sequenza atteso
	if (seq_num == expected_seq_num){
		printf("\n\nArrivo ordinato: %d\n", seq_num);
		expected_seq_num++;
		new_write++;

		memset(pkt+seq_num, 0, sizeof(packet));
		pkt[pkt_aux.seq_num] = pkt_aux;


		// SETTAGGIO TIMER
		it_val.it_value.tv_sec = 2;
    	it_val.it_value.tv_usec = 0;
    	it_val.it_interval.tv_sec = 0;
	    it_val.it_interval.tv_usec = 0;
		if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
  			perror("setitimer");
			exit(1);
			}
		printf("Timer avviato\n");

		// Per ogni pacchetto ordinato correttamente ricevuto riparte il timer di 500ms
		while(recvfrom(socket, &new_pkt, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len)){
			printf("\nPacchetto ricevuto: %d\n", new_pkt.seq_num);
			
			if(new_pkt.seq_num ==-1) {
				seq_num = -1;
				// STOPPO IL TIMER -> DOWNLOAD TERMINATO
				it_val.it_value.tv_sec = 0;
    			it_val.it_value.tv_usec = 0;
    			it_val.it_interval.tv_sec = 0;
	   			it_val.it_interval.tv_usec = 0;

				if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
				perror("setitimer");
				exit(1);
				}
				return;
			}

			if (new_pkt.seq_num == expected_seq_num){
				printf("Pacchetto successivo %d arrivato\n", new_pkt.seq_num);
				seq_num = new_pkt.seq_num;
				memset(pkt+seq_num, 0, sizeof(packet));
				pkt[seq_num] = new_pkt;
				expected_seq_num++;
				new_write++;
			}else{
				// se non è quello successivo che ci facciamo a questo mannaggia i pescetti?
				break;
			}
			// reset timer
			if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
				perror("setitimer");
				exit(1);
				}
			printf("Timer Resettato\n");
		}

		// INVIO ACK CUMULATIVO 
		if(sendto(socket, &new_pkt.seq_num, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
			perror("Error send ack\n");
			return;
			}
		else{ 
			printf("Ack inviato: %d\n", seq_num);
		}

	}
}

void alarm_routine(){
	printf("\n\nTimeout exipered next packed not found\n");
	return;
}



/*
void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N){
	int i;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	printf("\n====== INIZIO RECV WINDOW ======\n\n");
	printf ("ReceiveBase : %d\n",ReceiveBase);
	printf ("WindowEnd: %d\n",WindowEnd);
	printf("\n================================\n\n");
	
	memset(&pkt_aux, 0, sizeof(packet));
	printf("ctrl (1)\n");

	if((recvfrom(socket, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len)<0)) {
		perror("error receive pkt ");
		return;
	}
	seq_num = pkt_aux.seq_num;

	//se è il pacchetto di fine file, termino
	if(pkt_aux.seq_num==-1) {
		printf("ctrl (9)\n");
		return;
	}

	printf("pkt ricevuto %d\n", seq_num);

	if(check_pkt[seq_num]==1){ //se è interno, lo scarto se gia ACKato
		printf("ctrl (6)\n");
		return;
	}
	if(WindowEnd<seq_num && seq_num<ReceiveBase){
		printf("ctrl (7)\n");
		return;
	}
	else if(check_pkt[seq_num]==1){ //se è interno, lo scarto se gia ACKato
		printf("ctrl (8)\n");
		return;
	}

	// Arrivo ordinato di segmento con numero di sequenza atteso
	if (seq_num == expected_seq_num){
		printf("Arrivo ordinato di un segmento\n");
		printf("SeqNum: %d, Expected_seq_num: %d, ReceiveBase: %d, ReceiveBase+Win_Size: %d\n", seq_num, expected_seq_num,ReceiveBase, ReceiveBase+WIN_SIZE);
		expected_seq_num++;

		// ATTENDO 500ms per l'arrivo ordinato di un altro segmento
		//recvfrom()

		
		// ALTIMENTI INVIO ACK
	}

	// Arrivo non ordinato di un segmento con numero di sequenza superiore a quello atteso
	if (seq_num > expected_seq_num){
		// Invio immediato di un ack duplicato indicando il numero di sequenza del segmento atteso
		// ovvero il pacchetto all'estremità inferiore del buco
	}


	//invio ack del pacchetto seq_num
	if(sendto(socket, &seq_num, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
		printf("Error send ack\n");
		perror("ERRORE");
		return;
	}
	printf("ack inviato %d\n", seq_num);
	ReceiveBase++;							//VEDERE DOVE VA INCREMENTATA
	new_write++;

	printf("ctrl (3)\n");
	memset(pkt+pkt_aux.seq_num, 0, sizeof(packet));
	printf("ctrl (4)\n");
	printf("aux seq num: %d\n",pkt_aux.seq_num);
	pkt[pkt_aux.seq_num] = pkt_aux;
	printf("ctrl (a)\n");
	check_pkt[pkt_aux.seq_num] = 1;
	printf("ctrl (b)\n");
}
*/

