#include <stdint.h>

// PARAMETRI DI DEFAULT
#define SERVER_PORT 25490
#define SERVER_IP "127.0.0.1"

// DEFINIZIONE DEI MESSAGGI DEI PACCHETTI
#define SYN "syn"
#define SYNACK "synack"
#define ACK_SYNACK "ack_synack"
#define FIN "fin"
#define FINACK "ackfin"
#define FOUND "found"
#define NFOUND "notfound"
#define NOVERW "nooverwrite"

// DEFINIZIONE DEI COMANDI
#define LIST 1
#define GET 2
#define PUT 3
#define CLOSE 4

// PARAMETRI SULLA TRASMISSIONE
#define LOST_PROB 15			// 0%<=LOST_PROB<=100%
#define TRAN_WIN 32				// Dimensione della finestra di trasmissione
#define RECV_WIN 32				// Dimensione della finestra di ricezione
#define PKT_SIZE 1500			// Dimensione del pacchetto


// PARAMETRI SULLE CARTELLE CLIENT/SERVER
#define CLIENT_FOLDER "./clientFiles/"
#define SERVER_FOLDER "./serverFiles/"


// PARAMETRI SUI FILE
#define MAX_FILE_LIST 100		// Massimo numero di file mostrati nella lista
#define MAX_NAMEFILE_LEN 127	// Massimo numero di caratteri mostrato nel filename


// COSTANTI PER IL CALCOLO DEL TIMEOUT
#define ALPHA 0.125
#define BETA  0.250


// DEFINIZIONE DELLA STRUTTURA DI UN PACCHETTO
typedef struct packet{
	int seq_num;
	short int pkt_dim;
	char data[PKT_SIZE-2*sizeof(int)-sizeof(short int)]; 	//Riservo spazio come dimensione del pacchetto - intero del seq number - intero della dim pacchetto - num pkts
	int num_pkts;											//Indica il numero totale dei pacchetti da ricevere (meglio eof?)
	uint64_t sent_time; 									//Tempo di invio del pkt in microsecondi
} packet;
