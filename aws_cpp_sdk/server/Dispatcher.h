
#ifndef	DISPATCHER_H
#define DISPATCHER_H

#include <map>
#include <sys/epoll.h>
#include <sys/shm.h>
#include <sys/mman.h>

#include "Shm_Queue.h"
#include "Common.h"
//#include "myepoll.h"
#include "fifo.h"

enum CLOSE_STASTUS
{ 
	CLOSE_STASTUS_TIMEOUT = 0,
	CLOSE_STASTUS_RECV_RROR,
	CLOSE_STASTUS_PACKET_ERROR,
	CLOSE_STASTUS_ENQUEUE_ERROR,
	CLOSE_STASTUS_SERVICE_REQUEST,
	CLOSE_STASTUS_SEND_ERROR,
	CLOSE_STASTUS_EPOLL_ERROR,
	
};

#define ONCE_HANDLE_NUM	100
using namespace std;

extern int openShm(volatile char ** ppShm,  int iShmKey, int iElementSize);

class Dispatcher
{
public:
	Dispatcher();
	~Dispatcher();
	
	int init();
	void run();

protected:

	struct epoll_event *ep_events;
	int epfd;

	int	max_fd;
	int iSvrSessionNum;
	map<int, Connect_Session_t*> cn_session;
	
	CFIFO m_cFIFO;

	int bind_tcp(const char* ip, short port, int backlog,int p);
	int bind_udp(const char* ip, short port,int p);

	int insert_session(Connect_Session_t* s);
	int remove_session(int fd);
	int update_session(int64_t id,time_t stamp);
	Connect_Session_t* find_session(int fd); 
	
	int handle_tcp_input(Connect_Session_t* s);
	int handle_udp_input(Connect_Session_t* s);
	int handle_tcp_close( Connect_Session_t* s , CLOSE_STASTUS status );
	int handle_output(const shm_block* mb);
	int handle_accept(Connect_Session_t* s);

	int enqueue_shm(Connect_Session_t* s, int buf_len, unsigned int uin=0);
	int check_timeout(Connect_Session_t* s,int tNow, int timeout);
	int check_timeout();
	void run_once (shm_block &mb, int ready);
private:
	Connect_Session_t* udp_slab;
	LocalStateShm *pReportShm;


	inline void add_in_event (int fd)
  	{
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.fd = fd;
		epoll_ctl (epfd, EPOLL_CTL_ADD, fd, &ev);
		max_fd ++;
	}
};
#endif

