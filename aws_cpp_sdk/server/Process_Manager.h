#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <signal.h>

#include "fifo.h"
#include "Shm_Queue.h"

class Process_Manager
{
public:
	Process_Manager (int proc_num,int argc,char* argv[]);
	~Process_Manager ();

	int spawn ();
protected:
	void monitor ();
	void child_run (int index ,bool bPrint = false);
private:
	int argc;
	char** argv;

	pid_t *child_pids;
	int current_count;
	int max_child_count;
	
	CFIFO m_stFIFO;
///
	Shm_Queue* m_psend_shmq;
	int m_iEpollFD ;
};
#endif

