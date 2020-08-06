#include "basic.h"

int *check_pkt;
int err_count;//conta quante volte consecutivamente è fallita la ricezione
int tot_pkts, tot_ack, tot_sent;
int base, max, window;
packet *pkt;

void sender(int socket, struct sockaddr_in *receiver_addr, int N, int lost_prob, int fd) {
	socklen_t addr_len = sizeof(struct sockaddr_in);
	char *buff = calloc(PKT_SIZE, sizeof(char));
	int i, new_read;
	off_t file_dim;
	
	srand(time(NULL));
	base=0;
	window=N/2;
	max=window-1;
	pkt=calloc(N, sizeof(packet));
	check_pkt=calloc(N, sizeof(int));//0=da inviare, 1 inviato non ackato, 2 ackato. non serve che ruoti.
	
	//calcolo tot_pkts
	file_dim = lseek(fd, 0, SEEK_END);
	if(file_dim%(PKT_SIZE-sizeof(int)-sizeof(short int))==0){
		tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int));
	}
	else{
		tot_pkts = file_dim/(PKT_SIZE-sizeof(int)-sizeof(short int))+1;
	}
	lseek(fd, 0, SEEK_SET);

	struct timeval end, start;
	gettimeofday(&start, NULL);
	//inizializzazione finestra di invio e primi pacchetti
	for(i=base; i<window; i++){
		pkt[i].seq_num = i;
		pkt[i].pkt_dim=read(fd, pkt[i].data, PKT_SIZE-sizeof(int)-sizeof(short int));
		if(pkt[i].pkt_dim==-1){
			pkt[i].pkt_dim=0;
		}
	}

	//inizio trasmissione
	tot_ack=0;
	tot_sent=0;
	err_count=0;
	while(tot_ack<tot_pkts && err_count<MAX_ERR){//while ho pachetti da inviare e non ho MAX_ERR ricezioni consecutive fallite
		send_window(socket, receiver_addr, pkt, lost_prob, N);		
		recv_ack(socket, receiver_addr, fd, N);
	}
	
	if(err_count==MAX_ERR){
		printf("File transfer failed (inactive receiver)\n");
		close(fd);
		return;
	}
	
	//fine trasmissione
	for(i=0; i<MAX_ERR; i++){
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
	printf("File transfer failed\n");
	close(fd);
	return;
}

void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N){
	int i, j;
	socklen_t addr_len = sizeof(struct sockaddr_in);
	if(base<max){
		for(i=base; i<=max; i++){
			if(check_pkt[i]==0 && (tot_sent < tot_pkts)){
				if(correct_send(lost_prob)) {
					while(sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len) < 0) {
					    printf("Packet send error(1)\n");
					}
					//printf("Packet %d sent\n", pkt[i].seq_num);
				}
				//else printf("Packet %d lost\n", pkt[i].seq_num);
				tot_sent++;
				check_pkt[i]=1;
			}
		}
	}
	else{
		for(i=base; i<N; i++){
			if(check_pkt[i]==0 && (tot_sent < tot_pkts)){
				if(correct_send(lost_prob)) {
					while(sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len) < 0) {
					    printf("Packet send error(2)\n");
					}
					//printf("Packet %d sent\n", pkt[i].seq_num);
				}
				//else printf("Packet %d lost\n", pkt[i].seq_num);
				tot_sent++;
				check_pkt[i]=1;
			}
		}
		for(i=0; i<=max; i++){
			if(check_pkt[i]==0 && (tot_sent < tot_pkts)){
				if(correct_send(lost_prob)) {
					while(sendto(socket, pkt+i, PKT_SIZE, 0, (struct sockaddr *)client_addr, addr_len) < 0) {
					    printf("Packet send error(3)\n");
					}
					//printf("Packet %d sent\n", pkt[i].seq_num);
				}
				//else printf("Packet %d lost\n", pkt[i].seq_num);
				tot_sent++;
				check_pkt[i]=1;
			}
		}
	}
}

int recv_ack(int socket, struct sockaddr_in *client_addr, int fd, int N){//ritorna -1 se ci sono errori, 0 altrimenti
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
		}
		else{
			for(i=base; i<N; i++){
				if(check_pkt[i] == 1) {
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
}