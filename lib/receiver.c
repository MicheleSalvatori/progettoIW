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

#include "utility.c"

#define WIN_SIZE 5

int error_count;
int *check_pkt;
int base, max, window;
int ReceiveBase, WindowEnd;
int sock;
int seq_num, new_write, expected_seq_num, lastAcked;
packet pkt_aux, *pkt;
socklen_t addr_len = sizeof(struct sockaddr_in);
struct sockaddr_in *client_addr;

void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N);
void alarm_routine();
void checkSegment( struct sockaddr_in *, int socket);
void send_cumulative_ack();

void initialize_recv(){ // Per trasferire un nuovo file senza disconnessione
	ReceiveBase = 0;
	WindowEnd = 0;
	seq_num = 0;
	expected_seq_num = 0;
	new_write = 0;
	lastAcked = 0;
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

	pkt=calloc(6000, sizeof(packet));		// al posto di 6000 va la dim finestra
	check_pkt=calloc(6000, sizeof(packet));
	memset(&pkt_aux, 0, sizeof(packet));

	while(seq_num!=-1){							//while ho pachetti da ricevere
		WindowEnd = ReceiveBase + WIN_SIZE-1;

		memset(&pkt_aux, 0, sizeof(packet));

		if((recvfrom(socket, &pkt_aux, PKT_SIZE, 0, (struct sockaddr *)sender_addr, &addr_len)<0)) {
			perror("error receive pkt ");
			continue;
		}
		
		seq_num = pkt_aux.seq_num;
		checkSegment(sender_addr, socket);
		}
	
	//SCRITTURA
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

	signal(SIGALRM, alarm_routine);
	memset(&new_pkt, 0, sizeof(packet));
	
	// SIMULAZIONE PKT PERSO/CORROTTO
	if (is_packet_lost(LOST_PROB)){
		printf ("\n!!! DEBUG: PACCHETTO PERSO (1) !!! | PKT: %d\n",pkt_aux.seq_num);
		return;
	}
	
	//PKT FINE FILE
	if(seq_num ==-1) {
		printf("PKT FINE FILE RICEVUTO\n");
		return;
	}

	// PRINT RIEPILOGO
	printf ("\nRicevuto:%d | Atteso:%d | LastAcked:%d\n", seq_num, expected_seq_num, lastAcked);

	if (seq_num > expected_seq_num){
		send_cumulative_ack(lastAcked);
		return;
	}

	// Arrivo ordinato di segmento con numero di sequenza atteso
	if (seq_num == expected_seq_num){
		new_write++; 					// Incremento numero pacchetti da scrivere
		expected_seq_num++;

		memset(pkt+seq_num, 0, sizeof(packet));
		pkt[pkt_aux.seq_num] = pkt_aux;
		lastAcked = seq_num;
		// SETTAGGIO TIMER
		set_timer(100000);

		// Per ogni pacchetto ordinato correttamente ricevuto riparte il timer di 500ms
		while(recvfrom(socket, &new_pkt, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len)){
			if (is_packet_lost(LOST_PROB)){
				printf ("\n!!! DEBUG: PACCHETTO PERSO (2) !!! | PKT: %d\n",new_pkt.seq_num);
				set_timer(100000);
				break;
			}			
			if(new_pkt.seq_num ==-1) {
				seq_num = -1;
				// STOPPO IL TIMER -> DOWNLOAD TERMINATO
				set_timer(0);
				return;
			}

			printf ("Ricevuto:%d | Atteso:%d | LastAcked:%d\n", new_pkt.seq_num, expected_seq_num, lastAcked);
			if (new_pkt.seq_num == expected_seq_num){
				seq_num = new_pkt.seq_num;

				memset(pkt+seq_num, 0, sizeof(packet));
				pkt[seq_num] = new_pkt;
				lastAcked = seq_num;
				expected_seq_num++;
				new_write++;
			}
			else if (new_pkt.seq_num > expected_seq_num){	// Arrivo di pkt con buco
				send_cumulative_ack(lastAcked);
				break;
			}																				// Questi else forse si possono raggruppare
			else {							//Arrivi di pkt fuori finestra
				send_cumulative_ack(lastAcked);
				break;
			}
			set_timer(100000);
		}		
	}
}

// INVIO ACK CUMULATIVO
void send_cumulative_ack(int ack_number){			
	printf ("\n==========INVIO ACK CUMULATIVO===========\n");
	if(sendto(sock, &ack_number, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
		perror("Error send ack\n");
		return;
		}
	else{ 
		printf("Ack inviato: %d\n", ack_number);
		printf("======================================\n");
	}
}

void alarm_routine(packet new_pkt,struct sockaddr_in *client_addr,socklen_t addr_len){
	printf("\n\nTimer Scaduto, nessun pacchetto da inviare trovato.\n");
	send_cumulative_ack(lastAcked);
	return;
}