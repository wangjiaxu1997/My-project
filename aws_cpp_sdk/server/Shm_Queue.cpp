#include "Shm_Queue.h"
#include "Common.h"
#include "Options.h"
#include "Log.h"
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>

int Shm_Queue::create (key_t sem_key, int length)
{
	sem_id = semget(sem_key, 1, IPC_CREAT | 0664);
	if (sem_id < 0) 
	{
		fprintf (stderr, "semget (key==%d):%s\n",sem_key, strerror(errno));
		return -1;
	}

	semun arg;
	arg.val = 1;
	semctl (sem_id, 0, SETVAL, arg);

	shm_len = length;
	shm_addr = (queue_header*) mmap(NULL, length , PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON , -1 , 0);
		
	shm_addr->head = sizeof (queue_header);
	shm_addr->tail = shm_addr->head;

	return 0;
}

Shm_Queue::Shm_Queue()
{
}

Shm_Queue::~Shm_Queue()
{
	munmap (shm_addr, shm_len);
	shm_addr = NULL;
}

int Shm_Queue::release ()
{
	sembuf op;

	op.sem_num = 0;
	op.sem_flg = SEM_UNDO;
	op.sem_op = 1;
	
	while (semop (sem_id, &op, 1) == -1)
	{
		if (errno == EINTR)
			continue;
		if (errno == EIDRM)
		{
			LOG (LOG_ERROR, "%d sem removed,semop error\n", sem_id);
			exit (-1);
		}
		else
		{
			LOG (LOG_ERROR, "semop error,%s\n", sem_id, strerror(errno));
			return -1;
		}
	}	

	return 0;
}

int Shm_Queue::acquire ()
{
	sembuf op;

	op.sem_num = 0;
	op.sem_flg = SEM_UNDO;
	op.sem_op = -1;

	if (semop (sem_id, &op, 1) == -1)
	{
		if (errno == EINTR)
		{
			LOG (LOG_ERROR, "%d sem interrupt,semop error\n", sem_id);
			return -1;
		}
		if (errno == EIDRM)
		{
			LOG (LOG_ERROR, "%d sem removed,semop error\n", sem_id);
			exit (-1);
		}
		else
		{
			LOG (LOG_ERROR, "semop error,%s\n", sem_id, strerror(errno));
			return -1;
		}
    }

	return 0;
}

bool Shm_Queue::check_mb(const shm_block* mb)
{
	if (mb == NULL)
	{
		LOG (LOG_ERROR, "Shm_Queue::check_mb(mb=NULL)");
		return false;
	}

	if(mb->length > ini.socket_bufsize + MB_HEAD_LENGTH 
		|| mb->length < MB_HEAD_LENGTH)
	{
		LOG (LOG_ERROR, "Shm_Queue::check_mb(length=%d,head=%d,max=%d)", 
			mb->length,MB_HEAD_LENGTH,ini.socket_bufsize);
		return false;
	}
	
	if(mb->type < 0 )
	{
		LOG (LOG_ERROR, "Shm_Queue::check_mb(mb->type=%d)", mb->type);
		return false;
	}

	return true;
}

int Shm_Queue::pop (shm_block* mb, bool locked, bool blocked)
{
	int ret_code = -1;

	if (locked && acquire () == -1)
		return -1;

	if (pop_wait (blocked) == 0)
	{
		shm_block* tail_mb = get_tail_mb ();
		if (!check_mb (tail_mb))
			exit (1);

		int head_pos = shm_addr->head;
		if (shm_addr->tail < head_pos && shm_addr->tail + tail_mb->length > head_pos)
		{
			LOG (LOG_ERROR, "bug:shm_queue tail=%d,head=%d,current length=%d",
				shm_addr->tail, head_pos, tail_mb->length);
			exit (1);
		}

		if (shm_addr->tail > head_pos && shm_addr->tail + tail_mb->length > shm_len)
		{
			LOG (LOG_ERROR, "bug:shm_queue tail=%d,head=%d,current length=%d",
				shm_addr->tail, head_pos, tail_mb->length);
			exit (1);
		}

		memcpy (mb, tail_mb, MB_HEAD_LENGTH);
		memcpy (mb->data, (char*)tail_mb + MB_HEAD_LENGTH, MB_DATA_LENGTH (tail_mb));
		shm_addr->tail = shm_addr->tail + tail_mb->length;

		LOG (LOG_TRACE, "pid=%d, pop length=%d,type=%d,tail=%d,head=%d,id=%llu",
			getpid(), mb->length, mb->type, shm_addr->tail, shm_addr->head, mb->id);

		ret_code = 0;
	}

	if (locked)
		release ();

	return ret_code;
}

int Shm_Queue::align_queue_tail ()
{
	shm_block* tail_mb = get_tail_mb ();
	if (shm_len - shm_addr->tail < MB_HEAD_LENGTH || tail_mb->type == PAD_REQUEST)
		if (shm_addr->head < shm_addr->tail)
			shm_addr->tail = sizeof (queue_header);

	return 0;
}

int Shm_Queue::align_queue_head (const shm_block* mb)
{
	int tail_pos = shm_addr->tail;
	int head_pos = shm_addr->head;
	int surplus = shm_len - head_pos;
	
	if (surplus < mb->length && tail_pos == sizeof (queue_header))
	{
		LOG (LOG_ERROR, "Shm_Queue::align_queue_head,surplus=%d,mb_len=%d",
			surplus, mb->length); 
			return -1;
	}

	if (surplus < MB_HEAD_LENGTH)
	{
		shm_addr->head = sizeof (queue_header);
		return 0;
 	}

	shm_block* next_mb = get_head_mb ();
	if (surplus < mb->length)
 	{
		next_mb->length = surplus;
		next_mb->type = PAD_REQUEST;
		next_mb->id = 0 ;
		shm_addr->head = sizeof (queue_header);
	}

	return 0;
}

int Shm_Queue::pop_wait (bool blocked)
{
	timespec tv = {0, Q_SLEEP_TIME};
	align_queue_tail ();

pop_wait_again:

	while (shm_addr->tail == shm_addr->head)
	{
		if (!blocked || stopped)
			return -1;
		nanosleep (&tv, NULL);
	}
	align_queue_tail ();
	
	if (shm_addr->tail == shm_addr->head)
	{
		LOG (LOG_ERROR, "[pop_wait] shm_queue tail=%d,head=%d",shm_addr->tail, shm_addr->head);
		goto pop_wait_again;
	}

	return 0;
}

void Shm_Queue::report (int *head, int *tail)
{
	*head = shm_addr->head;
	*tail = shm_addr->tail;
}

int Shm_Queue::push_wait (const shm_block* mb, bool blocked)
{
	timespec tv = {0, Q_SLEEP_TIME};

	if (align_queue_head (mb) == -1)
		return -1;

push_wait_again:
	while (shm_addr->tail - shm_addr->head > 0 && shm_addr->tail - shm_addr->head < mb->length + 1)
	{
		if (!blocked || stopped)
		{
			LOG (LOG_ERROR,"push wait error,tail=%d,head=%d,mb_len=%d", shm_addr->tail, shm_addr->head, mb->length);
			return -1;
		}
		nanosleep (&tv, NULL);
	}

	if (align_queue_head (mb) == -1)
		return -1;

	if (shm_addr->tail - shm_addr->head > 0 && shm_addr->tail - shm_addr->head < mb->length + 1)
		goto push_wait_again;

	return 0;
}

int Shm_Queue::push (const shm_block* mb, bool locked, bool blocked)
{
	int ret_code = -1;
	if (!check_mb (mb))
		return -1;

	if (locked &&  acquire () == -1)
		return -1;

	if (push_wait (mb, blocked) == 0)
	{
		char *next_mb = (char*)get_head_mb ();

		memcpy (next_mb, mb, MB_HEAD_LENGTH);
		memcpy (next_mb + MB_HEAD_LENGTH, mb->data, MB_DATA_LENGTH(mb));
		shm_addr->head = shm_addr->head + mb->length;

		LOG (LOG_TRACE, "pid=%d,push length=%d,type=%d,tail=%d,head=%d,shm len=%d,id=%llu",
			 getpid(), mb->length, mb->type, shm_addr->tail, shm_addr->head, shm_len, mb->id);
		ret_code = 0;
	}

	if (locked)
		release ();

	return ret_code;
}


////////////////////////////////////////////////////////////////
//// Shm_Queue_Manager
////////////////////////////////////////////////////////////////

Shm_Queue_Manager::Shm_Queue_Manager()
{
	m_stQueUnits.clear();
}

Shm_Queue_Manager::~Shm_Queue_Manager()
{
	clear();
}

int Shm_Queue_Manager::clear()
{
	QueUnitItor itor = m_stQueUnits.begin();
	for( ; itor != m_stQueUnits.end() ; ++itor )
	{
		delete itor->pFifo;
		delete itor->pQueue;
	}
	m_stQueUnits.clear();

	return 0;
}

int Shm_Queue_Manager::initQueue( key_t base , int count , int len )
{
	clear();

	for( int i = 0 ; i < count ; ++i )
	{
		Shm_Queue* que = new Shm_Queue;

		assert( que != NULL );

		if( que->create( base + i  , len ) != 0 )
		{
			delete que;
			LOG( LOG_ERROR , "Create Que key[%d] size[%d] Fail[%s]" , base + i , len , strerror( errno ) );
			return -1;
		}

		QueUnit stQueUnit;

		stQueUnit.pFifo = NULL;
		stQueUnit.pQueue = que;

		m_stQueUnits.push_back( stQueUnit );	
	}

	if( m_stQueUnits.size( ) > 0 )
		return 0;

	return -1;
}

int Shm_Queue_Manager::initFIFO( char* path , int count )
{
	for( int i = 0 ; i < count ; ++i )
	{
		CFIFO* f = new CFIFO;
		
		assert( f != NULL );
	
		char szFileName[ 128 ] = { 0 };
		snprintf( szFileName , sizeof( szFileName ) - 1 , "%s_%d" , path , i );

		int iRet = f->MakeFifo( szFileName , 0777 );
        if( iRet == 0 )
			iRet = f->Open( szFileName , O_WRONLY | O_APPEND );
	
		if( iRet != 0 )
		{
			delete f;
			LOG( LOG_ERROR , "Create FIFO[%s] Fail[%s]" , szFileName , strerror( errno ) );
			return -1;
		}

		m_stQueUnits[ i ].pFifo = f;	
	}

	return 0;
}

int Shm_Queue_Manager::push( const shm_block* mb , bool locked , bool blocked, unsigned int lUin  )
{	
	QueUnit& stQueUnit = m_stQueUnits[ lUin % m_stQueUnits.size( ) ];

	LOG( LOG_TRACE , "Shm_Queue_Manager::push A PACKET UIN[%d] FIFO[%p] QUEUE[%p]" , lUin , stQueUnit.pFifo , stQueUnit.pQueue );

	short shMsgNotify = 1;

	int iRet = stQueUnit.pQueue->push( mb , locked , blocked );

	stQueUnit.pFifo->Write( &shMsgNotify , sizeof(short) );

	return iRet;
}

