void sender(int socket, struct sockaddr_in *receiver_addr, int N, int loss_prob, int fd);
void send_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int lost_prob, int N);
int recv_ack(int socket, struct sockaddr_in *client_addr, int fd, int N);
