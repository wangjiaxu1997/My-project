
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "Dispatcher.h"
#include "Options.h"
#include "Pipe_Notify.h"
#include "Net.h"
#include "Log.h"


#define UDP_RECV_AGAIN_NUM	100


int isInnerIP( unsigned long ip )
{
	   unsigned long a_ip =  htonl(ip);	
       int bValid = -1;
       if( (a_ip>>24 == 0xa) || (a_ip>>16 == 0xc0a8) || (a_ip>>22 == 0x2b0) )
       {
          bValid = 0;
       }
       return bValid;
}



char* GetShm(int iKey, int iSize, int iFlag)
{
	int iShmID;
	char* sShm;
	char sErrMsg[50];

	if ((iShmID = shmget(iKey, iSize, iFlag)) < 0)
	{
		sprintf(sErrMsg, "shmget %d %d", iKey, iSize);
		perror(sErrMsg);
		return NULL;
	}
	if ((sShm = (char*)shmat(iShmID, NULL ,0)) == (char *) -1)
	{
		perror("shmat");
		return NULL;
	}
	if (mlock(sShm, iSize))
	{
		perror("mlock");
		return NULL;
	}
	return sShm;
}

int openShm(volatile char ** ppShm,  int iShmKey, int iElementSize)
{
	char* sShm = NULL;
	if (!(sShm=GetShm(iShmKey, iElementSize, 0666)))
	{
		return -1;
	}
	*ppShm = sShm;
	return 0;
}

Dispatcher::Dispatcher()
{
	max_fd = 0;
	iSvrSessionNum = 0;
	udp_slab = NULL;

        if ((epfd = epoll_create (MAXFDS)) < 0)
        {
                LOG (LOG_ERROR, "epoll_create failed");
                exit (1);
        }
        ep_events = (epoll_event*) calloc (MAXFDS, sizeof (epoll_event));
}

Dispatcher::~Dispatcher()
{
	map<int,Connect_Session_t*>::iterator pos;
	for (pos=cn_session.begin(); pos!=cn_session.end(); ++pos)	
	{
		Connect_Session_t* s = pos->second;
		remove_session ((int)CONNECTION_FD(s->id));
	}

	if (udp_slab != NULL)
	{
		delete udp_slab;
		udp_slab = NULL;
	}

        free (ep_events);
        close (epfd);

}

int Dispatcher::insert_session(Connect_Session_t* s)
{
	int key = CONNECTION_FD (s->id);
	cn_session.insert (make_pair (key, s));

	add_in_event (key);
//	LOG (LOG_TRACE, "insert into session,fd=%d,type=%d,id=%llu", key, s->type,s->id);
	return 0;
}

Connect_Session_t* Dispatcher::find_session(int fd)
{
	map<int,Connect_Session_t*>::iterator pos;
	pos = cn_session.find (fd);
	if (pos == cn_session.end ())
	{
		LOG (LOG_TRACE, "can't find fd = %d session", fd);
		return NULL;
	}

	return pos->second;
}

int Dispatcher::remove_session(int fd)
{
	map<int,Connect_Session_t*>::iterator pos;

	if ((pos = cn_session.find (fd)) == cn_session.end())
	{
		LOG (LOG_ERROR, "remove session,can't find session map \n");
		return -1;
	}

	max_fd -- ;

	Connect_Session_t* s = pos->second;
//	LOG (LOG_TRACE, "remove session,fd=%d,type=%d,stamp=%d", pos->first, s->type, s->stTv.tv_sec);
	delete s;
	cn_session.erase (pos);

	return 0;
}

int Dispatcher::enqueue_shm(Connect_Session_t* s, int buf_len, unsigned int uin)
{
	shm_block mb;
	mb.length = buf_len + MB_HEAD_LENGTH;
	mb.data = s->recv_mb;
	mb.protocol = s->protocol;
	mb.sockinfo = get_sockinfo (CONNECTION_FD(s->id));
	memcpy(&mb.stTv, &s->stTv, sizeof(struct timeval));
	if (s->type == UDP_LISTEN)
	{
        mb.type = UDP_REQUEST;
		mb.sockinfo += SOCK_DGRAM;
	}
    else
	{
        mb.type = TCP_REQUEST;
		mb.sockinfo += SOCK_STREAM;
	}
	mb.id = s->id;

	return g_stShmQueueManager.push(&mb, false, false, uin) ; 
	//send_shmq.push (&mb, false, false);
}

int Dispatcher::check_timeout(Connect_Session_t* s, int tNow, int timeout)
{
	int difftime = tNow - s->stTv.tv_sec;
	if (difftime > timeout || difftime < 0)
	{
		LOG (LOG_TRACE,"check_timeout:%d", difftime);
		return handle_tcp_close ( s , CLOSE_STASTUS_TIMEOUT );
	}

	return 0;
}

int Dispatcher::check_timeout()
{
	time_t tNow = time( NULL );
	static time_t tLastCheckTime = tNow;
	static time_t tLastPurgeTime = tNow;

	//avoid check_timeout in every epoll_wait return
	if( tNow - tLastCheckTime <= ini.check_timeout)
		return 0;

	tLastCheckTime = tNow;

	map<int,Connect_Session_t*>::iterator pos;
	int difftime = 0;

//	if(!ini.press_test)
	{
		for (pos=cn_session.begin(); pos!=cn_session.end(); ++pos)
		{
			Connect_Session_t* s = pos->second;
			if( s->type == TCP_STREAM && s->protocol == 1 ) {
				//Since we have got the pos, erase directly... ipc:yuyang
				//check_timeout( s , tNow, ini.socket_timeout );
				difftime = tNow - s->stTv.tv_sec;
				if (difftime > ini.socket_timeout || difftime < 0)	{
					int fd = CONNECTION_FD (s->id);
					if (fd != pipe_rd_fd() && fd > 0)
					{
						app_close (s->id);
						max_fd--;
						delete s;
					}
					cn_session.erase (pos);
				}
			}
		}
	}
/*
	if( tNow - tLastPurgeTime > ini.monitor_time)
	{
		time_t tExpireSession = tNow - 86400*60;
		g_pstUserInfo->duty_purge( tExpireSession );
	}
*/
	if(tNow - tLastPurgeTime > 60)
	{
		tLastPurgeTime = tNow;
		//LOG(LOG_INFO, "STAT: USER[%d], ONLINE[%d]", g_pstUserInfo->size(), g_pstOnline->size());
	}
	return 0;
}

int Dispatcher::bind_tcp (const char* ip_addr, short port, int backlog, int p )
{
	int listenfd = open_tcp_port (ip_addr, port, backlog);
	if (listenfd == -1)
		exit (1);

	int64_t id= CONNECTION_ID (inet_addr (ip_addr), port, listenfd);
	Connect_Session_t* s = new Connect_Session_t (id, TCP_LISTEN , p );
	if (s->recv_mb == NULL || s->send_mb == NULL)
	{
		LOG (LOG_ERROR, "malloc failed, %s", strerror(errno));
		exit (1);
	}
	insert_session (s);
	iSvrSessionNum ++;

	return 0;
}

int Dispatcher::bind_udp (const char* ip_addr, short port,int p)
{
	int listenfd = open_udp_port (ip_addr, port);
	if (listenfd == -1)
		exit (1);

	int64_t id= CONNECTION_ID (inet_addr (ip_addr), port, listenfd);
	Connect_Session_t* s = new Connect_Session_t (id, UDP_LISTEN,p);
	if (s->recv_mb == NULL || s->send_mb == NULL)
	{
		LOG (LOG_ERROR, "malloc failed, %s", strerror(errno));
		exit (1);
	}

	insert_session (s);
	iSvrSessionNum ++;

	return 0;
}

int Dispatcher::init()
{
	int udp_count = 0;

	LOG (LOG_TRACE, "--------------------------------------------------------------");
	add_in_event (pipe_rd_fd ());
	for (int i = 0; i < option.bind_count; i++)
	{
		if (bind_port[i].type == 0)
		{
			bind_udp (bind_port[i].ip, bind_port[i].port, bind_port[i].protocol);
			udp_count ++;
		}

		if (bind_port[i].type == 1)
			bind_tcp (bind_port[i].ip, bind_port[i].port, ini.backlog, bind_port[i].protocol);

	}//end for

	if (udp_count > 0)
	{
		udp_slab = new Connect_Session_t (0, UDP_LISTEN,0); 
		if (udp_slab->recv_mb == NULL || udp_slab->send_mb == NULL)
		{
			LOG (LOG_ERROR, "malloc failed,%s", strerror(errno));
			exit (1);
		}
	}

	if ( g_stShmQueueManager.initFIFO( ini.fifo , ini.worker_proc_num ) != 0 )
	{
			LOG (LOG_ERROR, "init shm queue manager fifo fail,%s", strerror(errno));
			return -1;
	}
	return 0;
}

int Dispatcher::handle_tcp_close( Connect_Session_t* s , CLOSE_STASTUS status )
{
	int fd = CONNECTION_FD (s->id);
	if (fd == pipe_rd_fd() || fd <= 0)
		return -1;
	close (fd);
	remove_session (fd);
	
	return 0; 
}

int Dispatcher::handle_udp_input(Connect_Session_t* s)
{
	int app_len, ret_code = 0;

	for (int i = 0; i < UDP_RECV_AGAIN_NUM; i++)
	{
		if (recv_udp_buffer (CONNECTION_FD (s->id), udp_slab) != 0)
			return -1;
	
		//app_len = app_input (udp_slab->recv_mb, udp_slab->recv_len, udp_slab->id);
		unsigned int uin =0;
		udp_slab->protocol = s->protocol;
		app_len = app_input (udp_slab->recv_mb, udp_slab->recv_len, udp_slab->id , udp_slab->protocol,  uin );
		if (app_len != udp_slab->recv_len || enqueue_shm (udp_slab, udp_slab->recv_len, uin) != 0)
		{
			LOG (LOG_ERROR, "app_input return %d,length=%d", app_len, udp_slab->recv_len);
			ret_code = -1;
		}

		LOG (LOG_TRACE, "app_input return %d", app_len);
		udp_slab->recv_len = 0;
	}

	return 0;
}

int Dispatcher::handle_tcp_input(Connect_Session_t* s)
{
	if (recv_tcp_buffer (s) == -1)
	{
		handle_tcp_close ( s , CLOSE_STASTUS_RECV_RROR );
		return -1;
	}

	if (s->recv_len == 0)
		return 0;

	while( s->recv_len > 0 )
	{
		unsigned int uin =0;
		int app_len = app_input (s->recv_mb, s->recv_len, s->id , s->protocol, uin );
		
		if(s->protocol == 1 || s->protocol == 2  )
		{
			static struct timeval stNow;
			static unsigned long time_interval = 0;
			gettimeofday(&stNow, NULL);
			time_interval = (stNow.tv_sec - s->stTv.tv_sec) * 1000000 + (stNow.tv_usec - s->stTv.tv_usec);
			if(time_interval > 100000)	// > 100 ms...
				LOG (LOG_DEBUG, "FD[%d] RECV: TC[%d] LEN[%d:%d]", (int)CONNECTION_FD(s->id), time_interval, app_len, s->recv_len);
			memcpy(&s->stTv, &stNow, sizeof(struct timeval));

			///判断内网IP
			/*
			if ( -1 == isInnerIP( (unsigned long) CONNECTION_IP( mb.id ) ) )
			{
				struct in_addr addr;
				unsigned long temp =   CONNECTION_IP( mb.id ) ;
				memcpy(&addr, &temp, 4);
				LOG(LOG_ERROR , "Insert push message Request from invalid IP:%s ,refuesed." , inet_ntoa( addr ) );
				continue; 
			}
			*/

		}
	
		//错误的包
		if (app_len < 0 || app_len > ini.socket_bufsize)
		{
			handle_tcp_close (s , CLOSE_STASTUS_PACKET_ERROR );
			return -1;
		}
	
		//没有收完的包
		if (app_len > s->recv_len || app_len == 0)
			return 0;
	
		if (enqueue_shm(s, app_len) == -1)
		{
			handle_tcp_close( s , CLOSE_STASTUS_ENQUEUE_ERROR );
			return -1;
		}
	
		//完整的包
		if (app_len == s->recv_len)
		{
			s->recv_len = 0;
		}
		//多收的包
		else 
		{
			s->recv_len = s->recv_len - app_len;
			if( s->recv_len > 0 )
				memmove (s->recv_mb, s->recv_mb + app_len, s->recv_len);
		}
	}

	return 0;
}

int Dispatcher::handle_output(const shm_block* mb)
{
	int fd = CONNECTION_FD (mb->id);
	Connect_Session_t* s = find_session(fd);
	if (s == NULL)
	{
		LOG (LOG_TRACE, "discard message,fd=%d", fd);
		return -1;
	}

	if (s->type == UDP_LISTEN)
		return send_udp_buffer (mb);

	//below is TCP socket
	if (s->type != TCP_STREAM || s->id != mb->id)
	{
		LOG (LOG_TRACE, "discard message,shm id=%llu,session id=%llu", mb->id, s->id);
		return -1;
	}

	s->flag = mb->flag;

	if( mb->flag < 0 )
		handle_tcp_close ( s , CLOSE_STASTUS_SERVICE_REQUEST );

	if (send_buffer (s, mb) != 0)
		handle_tcp_close( s , CLOSE_STASTUS_SEND_ERROR );
	else if (s->send_len > 0)
	{
		struct epoll_event ev;
		ev.events = EPOLLIN | EPOLLOUT;
		ev.data.fd = fd;
		epoll_ctl (epfd, EPOLL_CTL_MOD, ev.data.fd, &ev);
	}
	else
	{	
		if( s->flag == 0 )
		{
			handle_tcp_close( s , CLOSE_STASTUS_SERVICE_REQUEST );
		}
		else
		{		
			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.fd = fd;
			epoll_ctl (epfd, EPOLL_CTL_MOD, ev.data.fd, &ev);
		}
	}

	return 0;
}
int Dispatcher::handle_accept(Connect_Session_t* s)
{
	int sockfd, newfd;
	struct sockaddr_in peer;

	if(s->type != TCP_LISTEN)
	{
		LOG (LOG_ERROR,"accept session type=%d,invalid",s->type);
		return -1;
	}

	sockfd = CONNECTION_FD(s->id);
	newfd = accept_tcp (sockfd, &peer);
	if (newfd < 0)
		return -1;
	LOG ( LOG_TRACE , "FD[%d] ACCEPT CONNECTION",  newfd );

	int64_t id = CONNECTION_ID (peer.sin_addr.s_addr, peer.sin_port, newfd);
	
	Connect_Session_t* ss = new Connect_Session_t (id, TCP_STREAM , s->protocol );
	if (ss->recv_mb == NULL || ss->send_mb == NULL)
	{
		LOG (LOG_ERROR,"malloc session buffer failed");
		delete ss;
		return -1;
	}

	insert_session (ss);

	return 0;
}
void Dispatcher::run_once (shm_block &mb, int ready)
{
	Connect_Session_t* s = NULL; 

	for (int i = 0; i < ready; i++)
	{
		s = find_session (ep_events[i].data.fd);
		if (s == NULL)
			continue;
		if (s->type == TCP_STREAM && (ep_events[i].events & (EPOLLHUP | EPOLLERR)))
		{
			handle_tcp_close ( s , CLOSE_STASTUS_EPOLL_ERROR );
			continue;
		}
		if (ep_events[i].events & EPOLLIN)
		{
			switch (s->type)
			{
				case TCP_LISTEN:
					handle_accept (s);
					break;
				case UDP_LISTEN:
					handle_udp_input (s);
					break;
				case TCP_STREAM:
					handle_tcp_input (s);
					break;
				default:
					LOG (LOG_ERROR, "invalid Connect_Session_t type=%d", s->type);
			}
		}
		else if (s->type == TCP_STREAM)
		{
			if( send_tcp_session (s) == -1 )
			{
				handle_tcp_close( s , CLOSE_STASTUS_SEND_ERROR );
			}
			else
			{
				if(s->protocol == 1)
					check_timeout (s, (int)time(NULL), ini.socket_timeout);
			}
		}
	}
}

void Dispatcher::run()
{
	shm_block mb;
	mb.data = (char *) malloc (ini.socket_bufsize);
	int timeout = 1000;
	if (mb.data == NULL)
	{
		LOG (LOG_ERROR, "malloc failed");
		return ;
	}

	while (!stopped)
	{

		int ready = epoll_wait (epfd, ep_events, max_fd, timeout);

		if (ready < 0)
		{
			continue;
		}
		else if (ready > 0)
			LOG (LOG_TRACE, "nactive sockets:%d", ready);

		read_pipe ();
		//这里不能放在pipe可读的条件里
		for (; recv_shmq.pop (&mb, false, false) == 0 ;)
			handle_output (&mb);

		run_once (mb, ready);
		check_timeout();
	}

	free (mb.data);
	if (app_fini != NULL)
		app_fini (1);
}
