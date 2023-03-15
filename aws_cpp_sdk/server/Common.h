#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <string.h>

#include <inttypes.h>
#include <sys/time.h>

#define ERROR_MSG_MAX		512
#define POLL_WAIT_TIME		100
#define POLL_SEND_MAX		100
#define Q_SLEEP_TIME		100


#define PTT_PKT_PING_REQ		(0x11L)
#define PTT_PKT_PING_RESP		(0x12L)
#define PTT_PKT_PUSH_REQ		(0x130L)	//server req
#define PTT_PKT_PUSH_RESP		(0x140L)
#define PTT_PKT_PUSH_VERSION_REQ	(0x131L) //sub cmd. used in cmdItem
#define PTT_PKT_PUSH_VERSION_RESP	(0x132L)
#define PTT_PKT_PUSH_TIPS_REQ		(0x133L)
#define PTT_PKT_PUSH_TIPS_RESP		(0x134L)


#pragma pack(1)


typedef struct
{
	//机器状态
	uint32_t iServerId;
	uint32_t iReq;
	uint32_t iDelay;
	uint32_t iMaxDelay;
	uint32_t lLastAccessTime;		//最后访问时间
} ServerState;
typedef struct
{
	uint32_t iServerId;
	int iNowConfIndex;	//指向最新更新的配置共享内存的指针
	ServerState astStateShm[2];
} LocalStateShm;


enum
{
    UDP_REQUEST = 0,
    TCP_REQUEST,
    UDP_RESPONSE,
    TCP_RESPONSE,

    INT_REQUEST,
    INT_RESPONSE,

    PAD_REQUEST
};

struct shm_block
{
	int length;
	// 32(ip)+16(port)+16(socketfd)
	int64_t id;	//cid
	unsigned char protocol;	//协议类型
	/*
	* 0: extern call request
	* 1: extern call response
	* 2: intern call request
	* 3: intern call response
	* 4: fill bytes, discard
	*/
	int64_t sockinfo;
	char type;
	char flag;  //note here close flag 0 mean send and close fd 1 mean only send
	struct timeval stTv;
	char* data;
};

typedef struct _tagUser_Session
{
	int		sid;	// session id 
	unsigned int	uin;	// login uin
	int		ip;		// login ip
	time_t	create;	// session create time
	time_t	stamp;	// session op time
	int64_t cid;	// connect session id
//	char		reference[ 128 ];
	unsigned short broadcast;
	unsigned short	reference;
	
	_tagUser_Session()
	{
		sid = 0;
		uin = 0;
		ip = 0;
		create = 0;
		stamp = 0;
		cid = 0;
		broadcast = 0;
		reference = 0;
		
//		memset( reference , 0 , sizeof( reference ) );
	}

} User_Session;



struct RTP
{
	unsigned char rtpheader[12];
	unsigned char extheader[4];
	unsigned int sender;
	unsigned int receiver;
	unsigned char flag;
};
#pragma pack()


#define MB_HEAD_LENGTH	((int)(sizeof (shm_block) - sizeof (char*)))
#define MB_DATA_LENGTH(mb)	(mb->length - MB_HEAD_LENGTH)

//pipe
enum
{
	UDP_LISTEN = 0,
	TCP_LISTEN,
	PIPE_STREAM,
	TCP_STREAM,
};


//HTTP_PROCESS_RESULT
enum
{
	HTTP_SUCCESS = 0,
	HTTP_LONGCONN_SUCCESS,
	HTTP_SSO_ERROR,
	HTTP_MSGRET_ERROR,
	HTTP_STATUS_ERROR,
	HTTP_USERBIND_ERROR,
	HTTP_TOOMANY_SESSIONS,
};

typedef unsigned int uint;
typedef int64_t lint;

/*
typedef struct _tagUser_Session
{
	time_t	stamp;	// session create time
	char name[ 52 ];
	
	_tagUser_Session()
	{
		stamp = 0;
		memset( name , 0 , sizeof( name ) );
	}

} User_Session;
*/

struct Connect_Session_t
{
	int64_t id;
	char	type;
	unsigned char protocol;
	struct timeval stTv;
	char* recv_mb;
	int recv_len;
	char* send_mb;
	int send_len;   // byte need be send

	int send_pos;   // send pos ... 
	
	char flag;		//note here close flag 0 mean send and close fd 1 mean only send

	Connect_Session_t (int64_t key, char t,int p);
	~Connect_Session_t ();
};

extern int64_t CONNECTION_ID (int ip, unsigned short port, int fd); 
extern int load_dll(const char* dll_name);
extern void init_fdlimit ();
extern void daemon_start ();

//start ipc:yuyang, Date: 2010-02-04
#if 0
#define CONNECTION_FD(x)	(x&0xFFFF)
#define CONNECTION_PORT(x)	((x>>16)&0xFFFF )
#endif

#define CONNECTION_FD(x)	(x&0xFFFFFFFF)
#define CONNECTION_IP(x)	((x>>32)&0xFFFFFFFF)

//end ipc:yuyang, Date: 2010-02-04

typedef int (*init_factory)(int,char**,int, bool);
typedef void (*fini_factory)(int);
typedef void (*stat_factory)(int,int,int);

typedef int (*handle_input_factory)(const char*,int,int64_t ,int, unsigned int& );
typedef int (*handle_close_factory)(int64_t);
typedef int (*handle_process_factory)(char*,int&,int64_t,int);
typedef int (*handle_check_timeout_factory)();
typedef int (*handle_check_msg_factory)();

/////////////////////////////
typedef int(*aws_upload_callback_factory)(const char*);
typedef int(*aws_init_factory)(const char*, const char*, const char*, const char*, int);
typedef int(*handle_upload_file_factory)(const char*, const char*, const char*, const char*, int);

extern aws_init_factory aws_init;
extern aws_upload_callback_factory aws_upload_callback;
extern handle_upload_file_factory sync_upload_file;
extern handle_upload_file_factory asyns_upload_file;
extern handle_upload_file_factory random_http_put;
///////////////////////////


extern bool stopped;
extern init_factory app_init;
extern fini_factory app_fini;
extern stat_factory app_stat;

//worker
extern handle_process_factory app_process;
//network
extern handle_close_factory app_close;
extern handle_input_factory app_input;
extern handle_check_timeout_factory app_check_timeout;
extern handle_check_msg_factory app_send_msg;
#endif

