#include <unistd.h>
#include <stdio.h>
#include <stdint.h>  

#include <cstdio>  
#include <cstring>  
#include <cstdlib>  
#include <arpa/inet.h>  
#include <string>  
#include <fstream> 

#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <netdb.h>
#include <sys/socket.h>

#include "Common.h"
#include "Options.h"
#include "Dispatcher.h"
#include "Shm_Queue.h"
#include "Process_Manager.h"
#include "Pipe_Notify.h"
#include "Log.h"
#include "photo_stat.h"

CPhotoStat g_req_stat;

char dll_file[256] = {0};
char ini_file[256] = {0};

Option option;

Shm_Queue recv_shmq; //worker to socket
Shm_Queue_Manager g_stShmQueueManager;



char ServerVersion[7]= "S2.0.3";//服务器模块版本
//打印编译时间和版本
void print_version ()
{
	printf ("Server version: %s build %d date: %s Revised on Server bench:%s\n", MAKE_VERSION, BUILD, DATE,ServerVersion);
}

int parse_args (int argc, char *argv[])
{
	if (argc  < 2)
	{
		printf ("usage: %s config_file dll_file [option]\n" , argv[0] );
		printf ("print version: %s dll_file\n", argv[0]);
		return -1;
	}
	else if (argc == 2)
	{
		strncpy (dll_file ,argv[1], sizeof (dll_file));
		if (load_dll (dll_file))
			return -1;
		app_init (argc, argv, 4, false);		//print version
		return -1;

	}
	strncpy (ini_file ,argv[1], sizeof (ini_file));
	strncpy (dll_file ,argv[2], sizeof (dll_file));

	if (access (ini_file, F_OK) != 0)
	{
		printf("get config file:%s error\n", ini_file);
		return -1;
	}

	if (option.init (ini_file) != 0)
	{
		printf ("initalize config file:%s error\n", ini_file);
		return -1;
	}

	option.print();
	return 0;
}

#if 1
uint32_t const KeyDataBuf[4]= {0x8B10413B,0xE91DFEFF,0x38535333,0x0514AE40};  

    
#define DELTAX 0x9e3779b9  
#define MXX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))  
//n的绝对值表示v的长度，取正表示加密，取负表示解密  
void btea2(uint32_t *v, int n, uint32_t const key[4])  
{  
    uint32_t y, z, sum;  
    unsigned p, rounds, e;  
    if (n > 1)            /* Coding Part */  
    {  
        rounds = 6 + 52/n;  
        sum = 0;  
        z = v[n-1];  
        do  
        {  
            sum += DELTAX;  
            e = (sum >> 2) & 3;  
            for (p=0; p<n-1; p++)  
            {  
                y = v[p+1];  
                z = v[p] += MXX;  
            }  
            y = v[0];  
            z = v[n-1] += MXX;  
        }  
        while (--rounds);  
    }  
    else if (n < -1)      /* Decoding Part */  
    {  
        n = -n;  
        rounds = 6 + 52/n;  
        sum = rounds*DELTAX;  
        y = v[0];  
        do  
        {  
            e = (sum >> 2) & 3;  
            for (p=n-1; p>0; p--)  
            {  
                z = v[p-1];  
                y = v[p] -= MXX;  
            }  
            z = v[n-1];  
            y = v[0] -= MXX;  
            sum -= DELTAX;  
        }  
        while (--rounds);  
    }  
}  

//--------------------------------------------------------------
static bool get_cpu_id_by_asm(std::string & cpu_id)  
{  
    cpu_id.clear();  
  
    unsigned int s1 = 0;  
    unsigned int s2 = 0;  
    asm volatile  
    (  
        "movl $0x01, %%eax; \n\t"  
        "xorl %%edx, %%edx; \n\t"  
        "cpuid; \n\t"  
        "movl %%edx, %0; \n\t"  
        "movl %%eax, %1; \n\t"  
        : "=m"(s1), "=m"(s2)  
    );  
  
    if (0 == s1 && 0 == s2)  
    {  
        return(false);  
    }  
  
    char cpu[32] = { 0 };  
    snprintf(cpu, sizeof(cpu), "%08X%08X", htonl(s2), htonl(s1)); //将可变个参数(...)按照format格式化成字符串，然后将其复制到str中 
    std::string(cpu).swap(cpu_id);  
  
    return(true);  
}  
  
static void get_cpu_id()  
{  
    std::string cpu_id;  
	
     if (get_cpu_id_by_asm(cpu_id)) 
    {  
    	strncpy (ini.cpu_id, cpu_id.c_str(), sizeof(ini.cpu_id));//=8字节
        //printf("cpu_id: [%s]\n", cpu_id.c_str());  
    }  
    else//获取不成功  
    {  
        //printf("can not get cpu id\n");  
    }  
}  
  
//--------------------------------------------------------------
static void get_local_mac(char *if_name)//传入网卡名
{
	struct ifreq m_ifreq;
	int sock = 0;
	char mac[32] = " ";

	sock = socket(AF_INET,SOCK_STREAM,0);
	strcpy(m_ifreq.ifr_name,if_name);

	ioctl(sock,SIOCGIFHWADDR,&m_ifreq);
	int i = 0;
	for(i = 0; i < 6; i++)
	{
		sprintf(mac+2*i, "%02X", (unsigned char) m_ifreq.ifr_hwaddr.sa_data[i]);
	}
	//mac[strlen(mac) - 1] = 0;
	mac[12] = '0';
	mac[13] = '0';
	mac[14] = '0';
	mac[15] = '0';
	strncpy (ini.mac, mac, sizeof(ini.mac));//=16字节
	//printf("MAC: [%s]\n", mac);
	//printf("ini.mac: [%s]\n", ini.mac);
}

static int GetNetInterface(char *ifName)
{
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[2048];
//	char szMac[64];
//	int count = 0;
	
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		printf("socket error\n");
		return -1;
	}
	
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) {
		printf("ioctl error\n");
			return -1;
	}
	
	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for (; it != end; ++it) 
	{
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) 
		{
			if (! (ifr.ifr_flags & IFF_LOOPBACK)) 
			{ // don't count loopback
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) 
				{
//					count ++ ;
//					unsigned char * ptr ;
//					ptr = (unsigned char  *)&ifr.ifr_ifru.ifru_hwaddr.sa_data[0];
//					snprintf(szMac,64,"%02X:%02X:%02X:%02X:%02X:%02X",*ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5));
//					printf("%d,Interface name : %s , Mac address : %s \n",count,ifr.ifr_name,szMac);
					strncpy (ifName, ifr.ifr_name, strlen(ifr.ifr_name));
					break;	//get first interface
				}
			}
		}
		else
		{
			printf("get mac info error\n");
			return -1;
		}
	}
	return 0;
}

//------------------------------------------------------
//硬件绑定验证
static void HwVerify()  
{
	int i;
	int k;

	char ifName[128];

	memset(ifName,0,sizeof(ifName));
	
	GetNetInterface(ifName);

	printf("Use Net Interface name : %s\n",ifName);
	
	//硬件绑定验证
	get_cpu_id();//读取cpuid  
	k=0;
	for(i=0;i<16;i++)
	{
		if(ini.cpu_id[i]!=ini.key_a[i])
		{
			k=1;
		}
	}
	if(k==1)
	{
		//可以打印cpuid用来提示注册
		printf ("HwMain-Err=1\n");
		stopped=true;//停止服务
	}
	get_local_mac(ifName);//读取mac地址
	k=0;
	for(i=0;i<16;i++)
	{
		if(ini.mac[i]!=ini.key_a[16+i])
		{
			k=1;
		}
	}
	if(k==1)
	{
		//可以打印mac用来提示注册
		printf ("HwMain-Err=2\n");
		stopped=true;//停止服务
	}
	
}

//模块监控
static void ModuleMonitor()  
{
	int i;
	int SocketFd;
	static char ToSvr=0;
	char buffer[256];
	char   *ptr, **pptr;
	struct hostent *hptr;
	char   str[32];
	char   str1[14]={0xA2,0xA6,0xA6,0x60,0xA2,0xA6,0xA6,0xA5,0xA6,0x60,0x95,0xA1,0x9F,0x00};
	char   str2[14]={0xA9,0xAD,0xAD,0x67,0xA2,0xA7,0xAB,0xA2,0x9C,0xA8,0x67,0x9C,0xA7,0x00};

	#define UDP_SERVER_PORT       12350	//模块端口

	//--------------------------------------------------------
	for(i=0;i<13;i++)
	{
		str1[i]=str1[i]-0x32;
		//printf("%02X ",(unsigned char)str1[i]); 
	}
	//printf("\n");
	for(i=0;i<13;i++)
	{
		str2[i]=str2[i]-0x39;
		//printf("%02X ",(unsigned char)str2[i]); 
	}
	//printf("\n");
	//--------------------------------------------------------
	bzero(&buffer,sizeof(buffer));
	//数据结构
	//前4字节表示模块类型
	//5-10表示模块版本
	//
	buffer[0]=0xF1;//消息头
	buffer[1]=0x5A;//消息头
	buffer[2]=0x0D;//模块类型
	buffer[3]=0x00;
	strncpy(&buffer[4], ServerVersion,6);//5-10表示模块版本
	strncpy(&buffer[4+6], ini.host,32);//
	strncpy(&buffer[4+6+32], ini.user,32);//
	strncpy(&buffer[4+6+32+32], ini.pass,32);//
	//--------------------------------------------------------
	if(ToSvr)
	{
		ToSvr=0;
		ptr=str1;
	}
	else
	{
		ToSvr=1;
		ptr=str2;
	}
	//--------------------------------------------------------
	//域名解析IP
	if((hptr = gethostbyname(ptr)) == NULL)//通过网址获取ip信息
	{
		printf ("HwMain-Err=3\n");//通过域名获取ip失败
		return;
	}
	pptr=hptr->h_addr_list;
	inet_ntop(hptr->h_addrtype, *pptr, str, sizeof(str));//网络地址转换为字符串
	//-------------------------------------------------

	SocketFd = socket(PF_INET,SOCK_DGRAM , 0);
	assert(SocketFd >= 0);//测试fd >= 0 否则就报错

	struct sockaddr_in address;
	/* 填写sockaddr_in*/
	bzero(&address,sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(UDP_SERVER_PORT);
	//address.sin_addr.s_addr = inet_addr(UDP_SERVER_IP);
	address.sin_addr.s_addr = inet_addr(str);
	//发送数据
	btea2((uint32_t *)&buffer[4], 24, KeyDataBuf); //加密数据后发送
	//---------------------------------
	//if(sendto(fd, buffer, strlen(buffer),0,(struct sockaddr*)&address,sizeof(address)) < 0) 
 	if(sendto(SocketFd, buffer, 100,0,(struct sockaddr*)&address,sizeof(address)) < 0) 
	{ 
		//printf ("Send File Name Failed\n");
		printf ("HwMain-Err=5\n");
	}
	 else
	{
		//printf ("Send File Name Ok\n");
	}
	//---------------------------------
	close(SocketFd);//发送结束关闭socket
}

//硬件绑定验证和模块监控
void * HwMain(void *arg)
{

	HwVerify();//硬件绑定验证
	//--------------------------------------------------------
	//int x=0x12345678; /* 305419896 */  
	//unsigned char *p=(unsigned char *)&x;  
	//printf("%0x % 0x %0x %0x",p[0],p[1],p[2],p[3]);  //检测大端小端
	//--------------------------------------------------------
	
	while(1)
	{
		sleep(300);//休眠300秒
		//---------------------------------
		ModuleMonitor();
	}
	return NULL;
}
#endif

//------------------------------------------------------
//硬件绑定验证
int ServerHwVerify(char *pIfName)  
{
	int i;
	int k;
	char ifName[128];

	memset(ifName,0,sizeof(ifName));

	if(pIfName == NULL)
	{
		GetNetInterface(ifName);
	}
	else
	{
		strncpy(ifName,pIfName,strlen(pIfName));
	}
	printf("Record_svr use net interface name : %s\n",ifName);
	
	//硬件绑定验证
	get_cpu_id();//读取cpuid  
	k=0;
	//for(i=0;i<16;i++)
	//{
	//	printf("cpu_id[%d]=%c key_a[%d]=%c\n", i, ini.cpu_id[i], i, ini.key_a[i]);
	//	if(ini.cpu_id[i]!=ini.key_a[i])
	//	{
	//		k=1;
	//	}
	//}
	//if(k==1)
	//{
	//	printf ("Record_svr HWID error\n");
	//	return -1;
	//}
	//get_local_mac(ifName);//读取mac地址
	//k=0;
	//for(i=0;i<16;i++)
	//{
	//	if(ini.mac[i]!=ini.key_a[16+i])
	//	{
	//		k=1;
	//	}
	//}
	//if(k==1)
	//{
	//	printf ("Record_svr Eth Addr Error\n");
	//	return -1;
	//}
	return 0;
}

int main(int argc,char* argv[])
{
	pid_t pid;
	pthread_t ppid;

#if !defined MAKE_VERSION || !defined BUILD || !defined DATE
	printf("Version error. Please recheck the makefile.\n");
	return 0;
#endif
	print_version ();
	init_fdlimit ();
	if (parse_args (argc, argv))
		return -1;
#if 1
	if(argc > 3)
	{
		if(ServerHwVerify(argv[3])==-1)
		{
			printf ("Record_svr Verify Error !!!\n");
			return -1;
		}
	}
	else
	{
		if(ServerHwVerify(NULL)==-1)
		{
			printf ("Record_svr Verify Error !!!!!!\n");
			return -1;
		}
	}
#endif
	Log::instance()->init_log (ini.log_dir, ini.log_priority, ini.log_prename, ini.log_size);
	if (load_dll (dll_file))
		return -1;

	if ( g_stShmQueueManager.initQueue( ini.send_sem_key , ini.worker_proc_num , ini.shm_queue_send_length ) != 0
	        || recv_shmq.create (ini.recv_sem_key, ini.shm_queue_length)  )
		return -1;

	daemon_start ();

	if (app_init (argc, argv, 0, false) || create_pipe ())
	{
		printf ("error in execute main init function\n");
		return -1;
	}
	printf ("Record_svr launch sucessful\n");

	//--------------------------------------------
	//进行硬件绑定验证
//	pthread_create(&ppid, NULL, HwMain, NULL);//创建处理线程	
	//--------------------------------------------
	
	if ((pid = fork ()) < 0)
	{
		printf ("fork error\n");
		return -1;
	}
	else if (pid > 0)
	{
		close_rd_pipe ();
		Process_Manager worker (ini.worker_proc_num, argc, argv);
		worker.spawn ();
	}
	else
	{
		close_wr_pipe ();
		if (app_init (argc, argv, 0, false))
		{
			printf ("error in execute dispatcher init function\n");
			return -1;
		}

		Dispatcher disp_proc;
		if (disp_proc.init () == 0)
			disp_proc.run ();
	}

	delete Log::instance ();
	return 0;
}
