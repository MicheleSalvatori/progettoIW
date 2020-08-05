/* daytime_clientUDP.c - code for example client program that uses UDP 
   Ultima revisione: 14 gennaio 2008 */

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h>

#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERV_PORT   5193 
#define MAXLINE     1024

int main(int argc, char *argv[ ]) {
  int   sockfd, n;
  char  recvline[MAXLINE + 1];
  struct    sockaddr_in   servaddr;

  if (argc != 2) { /* controlla numero degli argomenti */
    fprintf(stderr, "utilizzo: daytime_clientUDP <indirizzo IP server>\n");
    exit(1);
  }

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { /* crea il socket */
    perror("errore in socket");
    exit(1);
  }

  memset((void *)&servaddr, 0, sizeof(servaddr));      /* azzera servaddr */
  servaddr.sin_family = AF_INET;       /* assegna il tipo di indirizzo */
  servaddr.sin_port = htons(SERV_PORT);  /* assegna la porta del server */
  /* assegna l'indirizzo del server prendendolo dalla riga di comando. L'indirizzo � una stringa da convertire in intero secondo network byte order. */
  if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
                /* inet_pton (p=presentation) vale anche per indirizzi IPv6 */
    fprintf(stderr, "errore in inet_pton per %s", argv[1]);
    exit(1);
  }

  /* Invia al server il pacchetto di richiesta*/
  if (sendto(sockfd, NULL, 0, 0, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    perror("errore in sendto");
    exit(1);
  }
  /* Legge dal socket il pacchetto di risposta */
  n = recvfrom(sockfd, recvline, MAXLINE, 0 , NULL, NULL);
  if (n < 0) {
    perror("errore in recvfrom");
    exit(1);
  }
  if (n > 0) {
    recvline[n] = 0;        /* aggiunge il carattere di terminazione */
    if (fputs(recvline, stdout) == EOF)   {  /* stampa recvline sullo stdout */
      fprintf(stderr, "errore in fputs");
      exit(1);
    }
  }
  exit(0);
}
