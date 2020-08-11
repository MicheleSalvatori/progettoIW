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
	ReceiveBase = 1;
	WindowEnd = 0;
	seq_num = 0;
	expected_seq_num = 1;
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
	printf ("\nRicevuto:%d | Atteso:%d ", seq_num, expected_seq_num);

	if (seq_num > expected_seq_num){
		printf("\nPacchetto perso: %d",expected_seq_num);
		send_cumulative_ack(expected_seq_num);
		return;
	}

	// Arrivo ordinato di segmento con numero di sequenza atteso
	if (seq_num == expected_seq_num){
		new_write++; 					
		expected_seq_num++;

		memset(pkt+seq_num, 0, sizeof(packet));
		pkt[pkt_aux.seq_num] = pkt_aux;
		lastAcked = seq_num;
		set_timer(100000);			// AVVIO TIMER

		// Per ogni pacchetto ordinato correttamente ricevuto riparte il timer di 500ms			??? FORSE NON RIPARTE IL TIMER
		while(recvfrom(socket, &new_pkt, PKT_SIZE, 0, (struct sockaddr *)client_addr, &addr_len)){
			if (is_packet_lost(LOST_PROB)){
				printf ("\n!!! DEBUG: PACCHETTO PERSO (2) !!! | PKT: %d\n",new_pkt.seq_num);
				break;
			}			
			if(new_pkt.seq_num ==-1) {
				seq_num = -1;
				set_timer(0);		// STOPPO IL TIMER -> DOWNLOAD TERMINATO
				return;
			}

			printf ("\nRicevuto:%d | Atteso:%d ", new_pkt.seq_num, expected_seq_num);
			if (new_pkt.seq_num == expected_seq_num){
				seq_num = new_pkt.seq_num;
				printf(" E'il pacchetto atteso nei 500ms -> stop timer");
				memset(pkt+seq_num, 0, sizeof(packet));
				pkt[seq_num] = new_pkt;
				lastAcked = seq_num;
				expected_seq_num = seq_num+1;
				new_write++;
				set_timer(0);
				send_cumulative_ack(expected_seq_num);
				break;
			}
			if (new_pkt.seq_num > expected_seq_num){	// Arrivo di pkt con buco -> VIENE BUFFERIZZATO LO STESSO
				printf("\nPerso pkt: %d", expected_seq_num);
				memset(pkt+new_pkt.seq_num, 0, sizeof(packet));
				pkt[new_pkt.seq_num] = new_pkt;
				new_write++;
				set_timer(0);
				send_cumulative_ack(expected_seq_num);
				break;
			}																				// Questi else forse si possono raggruppare
			// else {		
			// 	printf("ELSE 2\n");					//Arrivi di pkt fuori finestra
			// 	send_cumulative_ack(expected_seq_num);
			// 	break;
			// }
		}		
	}
}

// INVIO ACK CUMULATIVO
void send_cumulative_ack(int ack_number){		
	if(sendto(sock, &ack_number, sizeof(int), 0, (struct sockaddr *)client_addr, addr_len) < 0) {
		perror("Error send ack\n");
		return;
		}
	else{ 
		printf("\t->INVIO ACK: %d", ack_number);
	}
}

void alarm_routine(packet new_pkt,struct sockaddr_in *client_addr,socklen_t addr_len){
	printf("\nTimer Scaduto, nessun pacchetto da inviare trovato.");
	send_cumulative_ack(expected_seq_num);
	return;
}