#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include "Pipe_Notify.h"
#include "Process_Manager.h"
#include "Common.h"
#include "Shm_Queue.h"
#include "Options.h"
#include "Log.h"
#include <arpa/inet.h>
#include "photo_stat.h"
extern CPhotoStat g_req_stat;

Process_Manager::Process_Manager(int proc_num, int c,char* v[])
{
	if (proc_num < 0 || proc_num > 65535)
		perror ("proc_num:"), exit(1);

	argc = c;
	argv = v;
	max_child_count = proc_num;
	current_count = 0;

	int length = sizeof (pid_t) * proc_num;
	child_pids = (pid_t*)calloc (1, length);
}

Process_Manager::~Process_Manager()
{
	if (child_pids != NULL)
	{
		free (child_pids);
		child_pids = NULL;
	}
}

void Process_Manager::child_run (int index ,bool bPrint)
{
	shm_block mb;
	char buffer[ 1024 ] = {0};
	if ((mb.data = (char*) malloc (ini.socket_bufsize)) == NULL)
	{
		LOG (LOG_ERROR,"malloc failed\n");
		return ;
	}
	if (app_init (argc, argv, 2, bPrint))
	{
		printf ("error in execute child init function\n");
		return ;
	}

	char tmp[180] = {0};
	sprintf(tmp, "../stat/stat_%d", index);
	
	g_req_stat.init(60, tmp, "", "", 5);

	//init FIFO ...
	char szFileName[ 128 ] = { 0 };
	snprintf( szFileName , sizeof( szFileName ) - 1 , "%s_%d" , ini.fifo , index );
	
	int iRet = m_stFIFO.MakeFifo( szFileName, 0777 );
	if ( iRet == 0 )
		iRet = m_stFIFO.Open( szFileName , O_NONBLOCK | O_RDONLY );
	
	if ( iRet != 0 )
	{
		LOG( LOG_ERROR , "Create FIFO[%s] Fail[%s]" ,szFileName , strerror( errno ) );
		return ;
	}

	m_psend_shmq = g_stShmQueueManager.GetQueue( index );

	m_iEpollFD = epoll_create( MAXFDS );

	struct epoll_event * m_pEvents = (epoll_event*) calloc (MAXFDS, sizeof (epoll_event));;
	
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = m_stFIFO.GetFd() ;

	epoll_ctl ( m_iEpollFD , EPOLL_CTL_ADD , ev.data.fd , &ev );
	LOG(LOG_TRACE,"ADD FD:%d",m_stFIFO.GetFd());
	int limit =0;//just for control the call check time out Frequency
	while (!stopped)
	{
		int timeout = 1000;
		int ready = epoll_wait( m_iEpollFD , m_pEvents , 10 , timeout );
		
		for ( int i = 0 ; i < ready ; ++i )
		{
			if ( m_pEvents[i].events & EPOLLIN && m_pEvents[i].data.fd == m_stFIFO.GetFd() )
			{
				m_stFIFO.Read( buffer , sizeof( buffer ) - 2 );	
						
				while (m_psend_shmq->pop (&mb, false, false) == 0)
				{

					int type = mb.type;

					if (type == UDP_REQUEST || type == TCP_REQUEST)
					{
			            int body_len = MB_DATA_LENGTH((&mb));
			            int ret_code = app_process (mb.data, body_len, mb.id, mb.protocol );
			            LOG (LOG_TRACE,"app_process return %d", ret_code);

			            if (body_len > 0 && body_len < ini.socket_bufsize)
			            {
			                if (ret_code == 0)
			                {
			                    mb.type = (type == UDP_REQUEST) ? UDP_RESPONSE: TCP_RESPONSE;
			                	mb.flag = 0;
							}
			                else if ( ret_code > 0 )
			                {
			                    mb.type = INT_RESPONSE;
			                    mb.flag = 1;
							}
							else
								mb.flag = -1;

			                if ( mb.flag >= 0 )
			                    mb.length = MB_HEAD_LENGTH + body_len;
			                else
								mb.length = MB_HEAD_LENGTH;

							recv_shmq.push (&mb, true, true);
							write_pipe ();
			            }
					}
				}
			}
			else if ( (m_pEvents[i].events & EPOLLERR ||  m_pEvents[i].events & EPOLLHUP) && m_pEvents[i].data.fd == m_stFIFO.GetFd()  )
			{
				//add by chenjiegui 2021.4.16
				LOG( LOG_ERROR , "===FD[%d] Fail" , m_pEvents[i].data.fd );

				epoll_ctl ( m_iEpollFD , EPOLL_CTL_DEL , m_pEvents[i].data.fd ,&m_pEvents[i]);

				if ( m_pEvents[i].data.fd == m_stFIFO.GetFd())
				{
					iRet = m_stFIFO.MakeFifo( szFileName, 0777 );
					if ( iRet == 0 )
						iRet = m_stFIFO.Open( szFileName , O_NONBLOCK | O_RDONLY );					
					if ( iRet != 0 )
					{
						LOG( LOG_ERROR , "Create FIFO[%s] Fail[%s]" ,szFileName , strerror( errno ) );
						continue;
					}

					struct epoll_event ev;
					ev.events = EPOLLIN;
					ev.data.fd = m_stFIFO.GetFd();
					epoll_ctl ( m_iEpollFD , EPOLL_CTL_ADD , ev.data.fd , &ev );
				}
            }
		}
		app_check_timeout();
	}
	free (mb.data);
	if (app_fini != NULL)
		app_fini (2);
}

void Process_Manager::monitor ()
{
	LOG (LOG_TRACE, "start montior worker process ...");
	int s_head, s_tail, r_head, r_tail;
	int hour = -1;

	while (!stopped)
	{
		sleep (ini.monitor_time);
       		
		static time_t tLastCheckTime = time( NULL );
		time_t tNow = time( NULL );
		
		if( tNow - tLastCheckTime >= 60)
		{
			tLastCheckTime = tNow;
			recv_shmq.report (&r_head, &r_tail);
			//send_shmq.report (&s_head, &s_tail);
			LOG(LOG_STATS, "SENDQ: [%d:%d] RECVQ: [%d:%d]", s_head, s_tail, r_head, r_tail);
		}
		
		for (int i=0; i < max_child_count; i++)
		{
			int status = 0;
			int result = waitpid( child_pids[i] , &status , WNOHANG );

			if( child_pids[i] == 0 || result != 0 || status != 0 )
			{
				LOG( LOG_ERROR , "Child Process[%d] exit status[%d]" , result , status );
				
				pid_t pid = fork ();
				if (pid > 0)
					child_pids[i] = pid;  	
				else if (pid == 0)
				{
					char s[64] = { 0 };
					snprintf(s, 64, "%s_sub%d",ini.log_prename, i);
					Log::instance()->init_log(ini.log_dir, ini.log_priority, s, ini.log_size);
					child_run (i);
					exit (0);
				}
				else
					memset(&child_pids[i], 0x0 ,sizeof (pid_t));
			}//if
		}//for
	}//while

	if (app_fini != NULL)
		app_fini (0);
}

int Process_Manager::spawn ()
{
	for (int i = 0; i <	max_child_count; i++)
	{
		pid_t child_pid = fork ();
		if (child_pid < 0)
			perror ("fork:"), exit(1);
		else if (child_pid == 0)
		{
			char s[64] = { 0 };
			snprintf(s, 64, "%s_sub%d",ini.log_prename, i);
			Log::instance()->init_log(ini.log_dir, ini.log_priority, s, ini.log_size);
			if(i == 0)
				child_run (i, true);
			else
				child_run(i);
			exit(0);
		}
		else
		{
			current_count ++;
			child_pids [i] = child_pid;
		}
	}

	monitor ();
	return 0;
}
