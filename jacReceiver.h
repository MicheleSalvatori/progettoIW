int receiver(int socket, struct sockaddr_in *sender_addr, int N, int loss_prob, int fd);
void recv_window(int socket, struct sockaddr_in *client_addr, packet *pkt, int fd, int N);

