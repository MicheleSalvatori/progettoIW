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



int error_count;
int *check_pkt;
int base, max, window;
packet pkt_aux;
packet *pkt;

int receiver(int socket, struct sockaddr_in *sender_addr, int N, int loss_prob, int fd){
	socklen_t addr_len=sizeof(struct sockaddr_in);
	off_t file_dim;
    int seq_num, new_write;
	
/*	srand(time(NULL));
	base=0;
	window=N/2;
	max=window-1;//eg. base=6-->max=(6+4-1)%8=9%8=1 (6 7 0 1)
	pkt=calloc(N, sizeof(packet));
	check_pkt=calloc(N, sizeof(int));
*/
	printf("\n====== INIZIO DEL RECEIVER ======\n\n");
	printf("File transfer started\nWait...\n");
	new_write = 0;
	printf("cntrl1\n");
	pkt=calloc(6000, sizeof(packet));
	//memset(pkt+new_write, 0, sizeof(packet));
	printf("cntrl2\n");

	while(pkt[new_write].seq_num!=-1){//while ho pachetti da ricevere
		printf("cntrl3\n");
		// recv_window(socket, sender_addr, pkt, fd, N);
		memset(pkt+new_write, 0, sizeof(packet));
		printf("cntrl4\n");

        if((recvfrom(socket, &pkt[new_write], PKT_SIZE, 0, (struct sockaddr *)sender_addr, &addr_len)<0)) {
			printf("error receive pkt\n");
			perror("Error");
			error_count++;
			return -1;
	    }
		else{
			printf("File received\n\n");
			seq_num = pkt[new_write].seq_num;
			int ack_num = seq_num+1;
			sendto(socket, &ack_num, sizeof(int), 0, (struct sockaddr *)sender_addr, addr_len); //Invio ACK con seq del prossimo pkt atteso
			printf("CLIENT: pkt ricevuto %d\n", seq_num);
			new_write++;
		}
	}

	//scrittura nuovi pacchetti
	for(i=0; i<new_write; i++){
		write(fd, pkt[i].data, pkt[i].pkt_dim); //scrivo un pacchetto alla volta in ordine sul file
		// base=(base+1)%N;	//sposto la finestra di uno per ogni check==1 resettato
		// max=(base+window-1)%N;
	}
}
	

/*
void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N){
	int i, new_write=0, seq_num;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	memset(&pkt_aux, 0, sizeof(packet));

	if((recvfrom(socket, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len)<0) && (error_count<MAX_ERR)) {
		//printf("error receive pkt\n");
		error_count++;
		if(ADAPTIVE) {
			increase_timeout(socket);
		}
		return;
	}
	if(ADAPTIVE) {
		decrease_timeout(socket);
	}
	error_count = 0;

	//se è il pacchetto di fine file, termino
	if(pkt_aux.seq_num==-1) {
		return;
	}
	seq_num = pkt_aux.seq_num;
	//printf("pkt ricevuto %d\n", seq_num);

	//invio ack del pacchetto seq_num
	if(correct_send(LOST_PROB)){
		if(sendto(socket, &seq_num, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
			//printf("Error send ack\n");
		}
		//printf("ack inviato %d\n", seq_num);
	}

	if(base<max){		//se mi arriva un pacchetto esterno alla finestra, lo scarto
		if(((0<=seq_num) && (seq_num<base)) || ((max<seq_num) && (seq_num<N))){
			return;
		}
		else if(check_pkt[seq_num]==1){//se è interno, lo scarto se gia ACKato
			return;
		}
	}
	else{			//se mi arriva un pacchetto esterno alla finestra, lo scarto
		if(max<seq_num && seq_num<base){
			return;
		}
		else if(check_pkt[seq_num]==1){//se è interno, lo scarto se gia ACKato
			return;
		}
	}

	memset(pkt+pkt_aux.seq_num, 0, sizeof(packet));
	pkt[pkt_aux.seq_num] = pkt_aux;
	check_pkt[pkt_aux.seq_num] = 1;

	//setto new_write al numero di pacchetti da scrivere sul file
	if(base<max){
		for(i=base; i<=max; i++) {
			if(check_pkt[i]==1) {
				check_pkt[i]=0; //reset variabile di controllo
				new_write++; //aumento di uno i pacchetti che dovrò scrivere sul file
			}
			else{
				break;
			}
		}
	}
	else{
		for(i=base; i<N; i++) {
			if(check_pkt[i]==1) {
				check_pkt[i]=0; //reset variabile di controllo
				new_write++; //aumento di uno i pacchetti che dovrò leggere dal file
			}
			else{
				break;
			}
		}
		if(window-new_write==max+1){//se i check==2 ancora possibili sono pari alla posizione max+1
			for(i=0; i<=max; i++) {
				if(check_pkt[i]==1) {
					check_pkt[i]=0; //reset variabile di controllo
					new_write++; //aumento di uno i pacchetti che dovrò leggere dal file
				}
				else{
					break;
				}
			}
		}
	}
	*/


