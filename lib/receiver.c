#include "comm.h"

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

#define WIN_SIZE 5

int error_count;
int *check_pkt;
int base, max, window;
int ReceiveBase;
int WindowEnd;
int seq_num = 0;
int new_write = 0;
packet pkt_aux;
packet *pkt;
void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N);

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
		printf("SeqNum: %d\n",seq_num);
		WindowEnd = ReceiveBase + WIN_SIZE-1;
		recv_window(socket, sender_addr, pkt, fd, WIN_SIZE);
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
		printf("error receive pkt\n");
		return;
	}
	//se è il pacchetto di fine file, termino
	seq_num = pkt_aux.seq_num;
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


