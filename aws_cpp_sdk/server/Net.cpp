#include "Net.h"
#include "Log.h"
#include "Options.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <error.h>
#include <errno.h>

int recv_tcp_buffer (Connect_Session_t* s)
{
	int buf_len = ini.socket_bufsize * s->protocol;
	int sockfd = CONNECTION_FD (s->id);

	int recv_bytes = recv (sockfd, s->recv_mb + s->recv_len, buf_len - s->recv_len, 0);
	if (recv_bytes == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			return 0;
		else
			return -1;
	}
	else if (recv_bytes ==	0)
	{
		LOG (LOG_TRACE, "recv tcp packet error, fd=%d,%s", sockfd, strerror (errno));
		return -1;
	} 

	s->recv_len += recv_bytes;	
	LOG (LOG_TRACE,"recv tcp packet ok,fd=%d,length=%d,id=%llu", sockfd, recv_bytes, s->id);

	return 0;		
}

int recv_udp_buffer(int sockfd, Connect_Session_t* s)
{
	struct sockaddr_in addr;

	socklen_t addrlen = sizeof addr;
	int recv_bytes = recvfrom (sockfd, s->recv_mb, ini.socket_bufsize, MSG_TRUNC,
			(struct sockaddr*)&addr, &addrlen);

	if (recv_bytes == -1 || recv_bytes == 0)
	{
		s->recv_len = 0;
		if (errno != EAGAIN)
			LOG (LOG_ERROR, "recvfrom fd=%d,%s", sockfd, strerror (errno));
		return -1;
	}

	s->recv_len = recv_bytes;
	s->id = CONNECTION_ID (addr.sin_addr.s_addr, addr.sin_port, sockfd);
	LOG (LOG_TRACE,"recv udp packet ok,fd=%d,length=%d,id=%llu", sockfd, recv_bytes, s->id);

	return 0;
}

int send_tcp_session(Connect_Session_t* s)
{
	int sockfd = CONNECTION_FD(s->id);
	//先发送session中的数据
	if (s->send_len > 0)
	{
		int bytes_tr = send(sockfd, s->send_mb + s->send_pos , s->send_len, 0);
		if (bytes_tr == -1)
		{
			if (errno != EINTR && errno != EWOULDBLOCK)
			{
				LOG (LOG_ERROR, "send error,fd=%d,length=%d,%s", sockfd, s->send_len, strerror (errno));
				return -1;
			}
			bytes_tr = 0;
		}

		LOG (LOG_TRACE, "send session buffer,total len=%d,send len=%d,id=%llu",s->send_len, bytes_tr, s->id);
		//s->stamp = time (NULL);
		if (bytes_tr < s->send_len && bytes_tr >= 0)
		{
			s->send_len = s->send_len - bytes_tr;
			//memmove (s->send_mb, s->send_mb + bytes_tr, s->send_len);

			s->send_pos += bytes_tr;
		} 
		else if (bytes_tr == s->send_len)
		{
			s->send_len = 0;
			s->send_pos = 0;
			
			if( s->flag == 0 )
				return -1;
		}
	}

	return 0;
}

int send_tcp_buffer(Connect_Session_t* s, const shm_block* mb)
{
	int bytes_tr, surplus;
	int sockfd = CONNECTION_FD (s->id);

	if (mb == NULL || MB_DATA_LENGTH(mb) == 0)
		return 0;

	bytes_tr = send (sockfd, mb->data, MB_DATA_LENGTH(mb), 0);
	if (bytes_tr == -1)
	{
		if (errno != EINTR && errno != EWOULDBLOCK)
		{
			LOG (LOG_ERROR, "send error,length=%d,%s",s->send_len, strerror (errno));
			return -1;
		}
		bytes_tr = 0;	
	}
	LOG (LOG_TRACE, "send tcp buffer,total=%d,send len=%d,id=%llu",MB_DATA_LENGTH(mb), bytes_tr, s->id);
	//s->stamp = time (NULL);
	if ((surplus = MB_DATA_LENGTH(mb) - bytes_tr) > 0)
	{
		memcpy (s->send_mb, mb->data + bytes_tr, surplus);
		s->send_len = surplus;
	}

	return 0;
}

int send_udp_buffer(const shm_block* mb)
{
	//如果buffer中没有数据就返回
	if (mb == NULL || MB_DATA_LENGTH(mb) == 0)
		return 0;
	
	struct sockaddr_in address;
	int sockfd = CONNECTION_FD (mb->id);
	int length = MB_DATA_LENGTH(mb);

	address.sin_family = AF_INET; 
	address.sin_addr.s_addr = (unsigned int)(mb->id >> 32);
	address.sin_port =  (unsigned short)((mb->id >> 16) & 0xFFFF);

	LOG (LOG_TRACE, "sendto,ip=%s,port=%u,fd=%d", inet_ntoa (address.sin_addr), ntohs(address.sin_port), sockfd);

	if (sendto (sockfd, mb->data, length, 0, 
			(struct sockaddr *)&address, sizeof(address)) !=  length)
	{
		LOG (LOG_ERROR, "sendto error,%s",strerror(errno));
		return -1;
	}

	LOG (LOG_TRACE, "send udp buffer,length=%d,id=%llu,fd=%d", length, mb->id, sockfd);
	return 0;
}

int send_buffer (Connect_Session_t* s,const shm_block* mb)
{
	if (send_tcp_session (s) != 0)
		return -1;

	if (s->send_len == 0)
		return send_tcp_buffer (s, mb);

	//has session,append mb
	if (s->send_pos + s->send_len + MB_DATA_LENGTH(mb) > ini.socket_bufsize * s->protocol)
	{
		LOG (LOG_ERROR, "session buffer overflows,s_len=%d,mb_len=%d",
			s->send_pos + s->send_len , MB_DATA_LENGTH(mb));
		return -1;
	}

	memcpy (s->send_mb + s->send_len + s->send_pos , mb->data, MB_DATA_LENGTH(mb));
	s->send_len += MB_DATA_LENGTH(mb);

	return 0;
}

int open_tcp_port (const char* ip, unsigned short port, int backlog)
{
	int	listenfd;
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons (port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) == -1) 
	{ 
		fprintf (stderr, "socket error:%s\n",strerror(errno)); 
		return -1;
	} 
	int reuse_addr = 1;
	setsockopt (listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof reuse_addr);

	if (bind (listenfd, (struct sockaddr *)(&servaddr), sizeof(struct sockaddr)) == -1) 
	{ 
		fprintf (stderr, "bind %s:%d error:%s\n", ip, port, strerror(errno)); 
		return -1;
	} 
	
	if (listen (listenfd, backlog) == -1) 
	{ 
		fprintf (stderr, "listen error:%s\n",strerror(errno)); 
		return -1;
	} 
	
	int val = fcntl(listenfd, F_GETFL, 0); 
	val |= O_NONBLOCK; 
	fcntl (listenfd, F_SETFL, val);

	printf ("open tcp %s:%u\t[ok]\n", ip, port);//%d->%u jhd
	return listenfd;
}

int open_udp_port (const char* ip, unsigned short port)
{
	int	listenfd;
	struct sockaddr_in servaddr;

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons (port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);

	if ((listenfd = socket (AF_INET, SOCK_DGRAM, 0)) == -1) 
	{ 
		fprintf (stderr, "socket error:%s\n", strerror(errno)); 
		return -1;
	} 

	if (bind (listenfd, (struct sockaddr *)(&servaddr), sizeof(struct sockaddr)) == -1) 
	{ 
		fprintf (stderr, "bind %s:%d error:%s\n", ip, port, strerror(errno)); 
		return -1;
	} 
	
	int val = fcntl(listenfd, F_GETFL, 0); 
	val |= O_NONBLOCK; 
	fcntl (listenfd, F_SETFL, val);
	//set socket buffer
	int bufsize = 250*1024;
	setsockopt (listenfd, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof (int));
	setsockopt (listenfd, SOL_SOCKET, SO_SNDBUF, (char *) &bufsize, sizeof (int));
	
    printf ("open udp %s:%u\t[ok]\n", ip, port);//%d->%u jhd
	return listenfd;
}

int accept_tcp (int sockfd, struct sockaddr_in *peer)
{
	socklen_t peer_size;
	int newfd;

	for ( ; ; ) 
	{
		peer_size = sizeof(struct sockaddr_in); 
		if ((newfd = accept(sockfd, (struct sockaddr *)peer, &peer_size)) < 0)
		{
			if (errno == EINTR)
				continue;         /* back to for () */

			LOG (LOG_ERROR,"accept failed,listenfd=%d", sockfd);
			return -1;
		}

		break;
	}

	//set nonblock
	int val = fcntl(newfd, F_GETFL,0); 
	val |= O_NONBLOCK; 
	fcntl(newfd, F_SETFL, val);
	
	//set socket buffer
	int bufsize = 64*1024;
	setsockopt (newfd, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof (int));
	setsockopt (newfd, SOL_SOCKET, SO_SNDBUF, (char *) &bufsize, sizeof (int));

	LOG (LOG_TRACE, "accept connection,fd=%d,ip=%s,port=%d", 
		sockfd, inet_ntoa(peer->sin_addr), ntohs(peer->sin_port));
	return newfd;
}

int64_t get_sockinfo (int sockfd)
{
	int64_t sockinfo,tmp;
	struct sockaddr_in local;
	socklen_t local_size;

	local_size = sizeof (sockaddr_in);
	if (getsockname (sockfd, (struct sockaddr *)&local, &local_size) != 0)
	{
		LOG (LOG_ERROR, "getsockname error:%s", strerror(errno));
		return -1;
	}

	tmp = ntohl (local.sin_addr.s_addr);
	sockinfo = tmp << 32;
	tmp = ntohs (local.sin_port);
	sockinfo += (tmp << 16);
	return sockinfo;	
}
