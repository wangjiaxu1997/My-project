#ifndef NET_H
#define NET_H
#include "Common.h"

int send_buffer (Connect_Session_t* s, const shm_block* mb);
int send_udp_buffer(const shm_block* mb);
int send_tcp_session (Connect_Session_t* s);

int open_tcp_port (const char* ip, unsigned short port, int backlog);
int open_udp_port (const char* ip, unsigned short port);
int accept_tcp (int listenfd, struct sockaddr_in *peer);

int recv_tcp_buffer (Connect_Session_t* s);
int recv_udp_buffer (int sockfd, Connect_Session_t* s);

int64_t get_sockinfo (int sockfd);
#endif

