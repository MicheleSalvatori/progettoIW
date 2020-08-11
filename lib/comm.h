// parametri di default

#define SERVER_PORT 25490
#define SERVER_IP "127.0.0.1"

#define SYN "syn"
#define SYNACK "synack"
#define ACK_SYNACK "ack_synack"
#define READY "ready"
#define FIN "fin"
#define FINACK "ackfin"
#define FOUND "found"
#define NFOUND "notfound"

#define LIST 1
#define GET 2
#define PUT 3
#define QUIT 4

#define LOST_PROB 10	// 0%<=LOST_PROB<=100%
#define FLYING 32	//max flying packets number
#define MAX_ERR 25
#define ADAPTIVE 1	//impostare a 0 per abolire timeout adattativo

//in microsecondi
#define TIMEOUT_PKT 24000	//valore minimo 8000
#define TIME_UNIT 4000		//valore minimo di cui si può variare timeout
#define MAX_TIMEOUT 800000  //800 millisecondi timeout massimo scelto
#define MIN_TIMEOUT 8000	//8 millisecondi timeout minimo

//in secondi
#define REQUEST_SEC 10
#define SELECT_FILE_SEC 30

char i;
// #define fflush(stdin) while ((i = getchar()) != '\n' && i != EOF)

#define PKT_SIZE 1500
#define MAX_INPUT_LINE 128

//tempo di attesa in secondi
#define MAX_WAIT_TIME 300
#define CLIENT_FOLDER "./clientFiles/"
#define SERVER_FOLDER "./serverFiles/"

// 2^16 - 1 = 65'535 numero di porta massimo
#define MAX_PORT 65535
#define MAX_FILE_LIST 100
#define MAX_NAMEFILE_LEN 127


typedef struct packet{
	int seq_num;
	short int pkt_dim;
	char data[PKT_SIZE-sizeof(int)-sizeof(short int)]; //Riservo spazio come dimensione del pacchetto - intero del seq number - intero della dim pacchetto
} packet;
