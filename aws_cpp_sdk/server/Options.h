#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Ini_File.h"

#define MSG_NUM  8
struct MSG_Server_t
{
	char	id[8];
	char 	svr[24];
	char	bindip[24];
	char	bindport[12];	
	char	bindtable[32];
};

struct Ini_Option_t
{
	char	log_dir[128];
	char	bind_file[128];

	char	log_prename[32];
	short	log_priority;
	int		log_size;

	key_t	send_sem_key;
	key_t	recv_sem_key;
	int	shm_queue_length;
	int shm_queue_send_length;

	//fifo
	char	fifo[128];

	int	socket_timeout;
	int	session_timeout;
	int	socket_bufsize;
	int	backlog;
	int	worker_proc_num;

	int	monitor_time;
	int check_timeout;

	//fastdf
	char fdfsconf[128];

	//aws::s3参数
	char accessKey[64];
	char secretKey[64];
	char endPoint[64];
	char bucket[32];
	char region[32];
	char depositDir[32];
	int useVirtualAddressing;
	int s3Switch;

	//DB
	char host[32];
	unsigned short db_port;
	char user[32];
	char pass[32];
	char dbname[32];

	char cpu_id[16];//cpu id
	char mac[16];//mac地址=实际使用5个字节，方便加密计算

	char key_a[32];//cpu id mac使用32字节

	char szsvrversion[32];
	char szsvrmaketime[32];
	int laststart;
	
};

#define SERVER_MODE_LONG_POLL	1
#define SERVER_MODE_SERVER_PUSH	2

struct Bind_Option_t
{
	char 	ip[16];
	u_short port;
	int		type;
	int		protocol;
};

class Option
{
public:
	Option();
	~Option();

	int init(const char* config_file);
	void print();
	
	int bind_count;
protected:
	int	parse_network(const char* config_file);	
	int	parse_ini();

	IniFile ini_file;
};

extern Bind_Option_t bind_port[16];

extern Ini_Option_t ini;
extern Option option;
#endif

