#ifndef   SHM_QUEUE_H
#define   SHM_QUEUE_H
#include "Common.h"
#include <sys/types.h>
#include "Options.h"
#include "Log.h"
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>

#include "guardian.h"

#pragma pack(1)
struct queue_header
{
	volatile int head;
	volatile int tail;
};
#pragma pack()


class Shm_Queue
{
public:
	Shm_Queue();
	~Shm_Queue();
	int create (key_t sem, int shm_len);
	int push(const shm_block* data, bool locked, bool blocked);
	int pop (shm_block* data, bool locked, bool blocked);
	void report (int *head, int *tail);
protected:
	int release ();
	int acquire ();

	bool check_mb(const shm_block* mem_area);
	int pop_wait (bool blocked);
	int push_wait (const shm_block* mb, bool blocked);
	int align_queue_head (const shm_block* mb);
	int align_queue_tail ();

	inline shm_block* get_head_mb ()
	{
		return (shm_block*)((char*)shm_addr + shm_addr->head);
	}
	inline shm_block* get_tail_mb ()
	{
		return (shm_block*)((char*)shm_addr + shm_addr->tail);
	}

	int sem_id;
	int shm_len;

	queue_header*	shm_addr;
};

//extern Shm_Queue send_shmq;
extern Shm_Queue recv_shmq;
extern Shm_Queue open_shmq;


#include <vector>
#include "fifo.h"

typedef struct _tagQueUnit
{
	CFIFO* pFifo;
	Shm_Queue* pQueue;	
} QueUnit;

typedef std::vector< QueUnit > QueUnits;
typedef QueUnits::iterator QueUnitItor;

class Shm_Queue_Manager
{
public:
	Shm_Queue_Manager( );

	~Shm_Queue_Manager();

	int initQueue( key_t base , int count , int len ); 
	int initFIFO( char* path , int count ); 

	int push( const shm_block* data , bool locked , bool blocked ,unsigned int lUin);

	Shm_Queue* GetQueue( int index )
	{
		return m_stQueUnits[ index % m_stQueUnits.size() ].pQueue;
	}

private:
	int clear();

private:
	QueUnits m_stQueUnits;	
};

extern Shm_Queue_Manager g_stShmQueueManager;

#endif

