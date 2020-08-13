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
#include <sys/param.h>

#include "utility.c"

#define WIN_SIZE 15

int ReceiveBase, WindowEnd;
int sock;
int seq_num, expected_seq_num, tot_pkts, tot_received;
int *check_pkt_received;
packet pkt_aux, *pkt;
socklen_t addr_len = sizeof(struct sockaddr_in);
struct sockaddr_in *client_addr;
bool allocated;

void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N);
void alarm_routine();
void checkSegment( struct sockaddr_in *, int socket);
void send_cumulative_ack();

void print_recvd_status(){ //DEBUG
	for (int n=ReceiveBase-1;n<ReceiveBase+WIN_SIZE-1;n++){
		printf ("seq %d | %d\n",n+1,check_pkt_received[n]);
	}
}

void mark_recvd(int seq){
	check_pkt_received[seq-1] = 1;
	tot_received++;
	printf ("\nRICEVUTO %d | tot recvd:%d\n",seq_num,tot_received);
}

int is_received(int seq){
	if (check_pkt_received[seq-1] == 1){
		return 1;
	}
	return 0;
 }

void move_window(){
	printf ("\n===== SLIDING WINDOW =====\n");
	//DEBUG
	int oldBase = ReceiveBase;
	int oldExpd = expected_seq_num;
	int oldWind = WindowEnd;
	//END_DEBUG
	int j = ReceiveBase;
	for (j = ReceiveBase;j<=WindowEnd;j++){
		printf ("checking SEQ:%d | Status: %d\n",j,is_received(j));
		if (is_received(j)){
			expected_seq_num++;
			ReceiveBase++;
			WindowEnd = MIN(ReceiveBase + WIN_SIZE,tot_pkts);
		}
		else{
			break;
		}
	}
	printf("======================");
	printf ("\nSpostata Finestra | Base: %d -> %d | Expd: %d -> %d | Wind: %d -> %d\n",oldBase,ReceiveBase,oldExpd,expected_seq_num,oldWind,WindowEnd);
}

void initialize_recv(){ // Per trasferire un nuovo file senza disconnessione
	ReceiveBase = 1;
	WindowEnd = WIN_SIZE;
	seq_num = 0;
	expected_seq_num = 1;
	tot_pkts = 1;
	allocated = false;
	tot_received = 0;
}


int receiver(int socket, struct sockaddr_in *sender_addr, int N, int loss_prob, int fd){
	srand (time(NULL)); // Generazione di numeri random per simulare la perdita di un pacchetto
	initialize_recv();
	sock = socket;
	socklen_t addr_len=sizeof(struct sockaddr_in);
	client_addr = sender_addr;
	off_t file_dim;
	long i = 0;

	printf("\n====== INIZIO DEL RECEIVER ======\n\n");
	printf("File transfer started\nWait...\n");

	
	memset(&pkt_aux, 0, sizeof(packet));



	while(tot_received != tot_pkts){							

		memset(&pkt_aux, 0, sizeof(packet));

		if((recvfrom(socket, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)sender_addr, &addr_len)<0)) {
			perror("error receive pkt ");
			continue;
		}

		if (!allocated){	//Alloca le risorse per i pacchetti in ricezione e per l'array di interi che tiene traccia dei pkt ricevuti
			tot_pkts = pkt_aux.num_pkts;
			pkt=calloc(tot_pkts, sizeof(packet));
			check_pkt_received=calloc(tot_pkts, sizeof(int));
			allocated = true;
		}

		seq_num = pkt_aux.seq_num;
		checkSegment(sender_addr, socket);
	}
	

	//SCRITTURA FILE IN RICEZIONE
	set_timer(0);
	printf("====== SCRITTURA FILE ======\nPacchetti da scrivere: %d\n", tot_pkts);
	for(i=0; i<tot_pkts; i++){
		write(fd, pkt[i].data, pkt[i].pkt_dim); //scrivo un pacchetto alla volta in ordine sul file
		printf("Scritto pkt | SEQ: %d, IND: %ld\n", pkt[i].seq_num, i);
	}
	printf("============================\n");
}


	
void checkSegment(struct sockaddr_in *client_addr, int socket){

	struct itimerval it_val;
	packet new_pkt;

	signal(SIGALRM, alarm_routine);
	memset(&new_pkt, 0, sizeof(packet));
	
	// SIMULAZIONE PKT PERSO/CORROTTO
	if (is_packet_lost(LOST_PROB)){
		printf ("\n!!! DEBUG: PACCHETTO PERSO (1) !!! | PKT: %d\n",pkt_aux.seq_num);
		return;
	}

	// PRINT RIEPILOGO
	printf ("\nRicevuto:%d | Atteso:%d | RecvBase:%d | WindowEnd:%d | TotRicevuti:%d(1)" , seq_num, expected_seq_num,ReceiveBase,WindowEnd,tot_received);

	if (expected_seq_num < seq_num && seq_num <= WindowEnd && !is_received(seq_num)){
		mark_recvd(seq_num);
		printf ("\n\npkt %d bufferizzato (1)\n\n",seq_num);
		printf("\nPerso pkt: %d", expected_seq_num);
		memset(pkt+seq_num-1, 0, sizeof(packet));
		pkt[pkt_aux.seq_num-1] = pkt_aux; //-1 perche pkt[k] ha seq number k+1
		set_timer(100000);			// AVVIO TIMER
		send_cumulative_ack(expected_seq_num);
		return;
	}

	// Arrivo ordinato di segmento con numero di sequenza atteso
	if (seq_num == expected_seq_num){
		mark_recvd(seq_num);
		move_window();

		memset(pkt+seq_num-1, 0, sizeof(packet));
		pkt[pkt_aux.seq_num-1] = pkt_aux; //-1 perche pkt[k] ha seq number k+1
		set_timer(100000);			// AVVIO TIMER

		// Per ogni pacchetto ordinato correttamente ricevuto riparte il timer di 500ms			??? FORSE NON RIPARTE IL TIMER
		printf ("before while | tot received:%d\n",tot_received);
		while (tot_received!=tot_pkts){
			printf ("after  while | tot received:%d\n",tot_received);
			recvfrom(socket, &new_pkt, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len);

			if (is_packet_lost(LOST_PROB)){
				printf ("\n!!! DEBUG: PACCHETTO PERSO (2) !!! | PKT: %d\n",new_pkt.seq_num);
				return;
			}			

			printf ("\nRicevuto:%d | Atteso:%d | RecvBase:%d | WindowEnd:%d (2)" , new_pkt.seq_num, expected_seq_num,ReceiveBase,WindowEnd);
			if (new_pkt.seq_num == expected_seq_num){
				mark_recvd(new_pkt.seq_num);
				seq_num = new_pkt.seq_num;
				printf(" E'il pacchetto atteso nei 500ms -> stop timer");
				memset(pkt+new_pkt.seq_num-1, 0, sizeof(packet));
				pkt[seq_num-1] = new_pkt;
				move_window();
				set_timer(0);
				break;
			}

			else if (expected_seq_num < new_pkt.seq_num && new_pkt.seq_num <= WindowEnd && !is_received(new_pkt.seq_num)){ // Arrivo di pkt con buco -> VIENE BUFFERIZZATO LO STESSO
				mark_recvd(new_pkt.seq_num);
				printf ("\n\npkt %d bufferizzato (2)\n\n",new_pkt.seq_num);
				printf("\nPerso pkt: %d", expected_seq_num);
				memset(pkt+new_pkt.seq_num-1, 0, sizeof(packet));
				pkt[new_pkt.seq_num-1] = new_pkt;
				set_timer(0);
				break;
			}
		}
		send_cumulative_ack(expected_seq_num);		
	}
}

// INVIO ACK CUMULATIVO
void send_cumulative_ack(int ack_number){		
	if(sendto(sock, &ack_number, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
		perror("Error send ack\n");
		return;
		}
	else{ 
		printf("\t->INVIO ACK: %d | tot received:%d\n", ack_number,tot_received);
	}
}

void alarm_routine(){
	printf("\nTimer Scaduto, nessun pacchetto da inviare trovato.");
	send_cumulative_ack(expected_seq_num);
	return;
}

