#include "Options.h"
#include <string>
#include <stdio.h>
#include <stdint.h>  
using namespace std;

//-------------------------------------------------------
uint32_t const KeyData[4]={0xEB038B75,0xB4395DC0,0x742357E8,0x59EB1AFF};//密钥
uint32_t const KeyDataKeyA[4]={0x750CFF75,0x08FF1544,0x08FF1544,0x8040008B};//硬件绑定密钥A

static void btea(uint32_t *v, int n, uint32_t const key[4]);
void HexStrToStr(unsigned char *hex,unsigned char len,unsigned char *str);
void StrToHexStr(char *str,unsigned char len,char *HexStr);
int checksum(unsigned char *str,unsigned char len);
//-------------------------------------------------------

Bind_Option_t bind_port[16];
Ini_Option_t ini;

Option::Option()
{
	memset (&ini, 0x0, sizeof (ini));
	memset (&bind_port, 0x0, sizeof (bind_port));
	bind_count = 0;
}

Option::~Option()
{
}

int Option::init(const char* config_file)
{
	if (!ini_file.open (config_file))
	{
		printf ("open config file:%s failed\n", config_file);
		return -1;
	}

	if (parse_ini () != 0 || parse_network ((const char*)ini.bind_file) != 0)
		return -1;

	return 0;
}

int	Option::parse_ini()
{
	string tmp;

	char CharBuf16[16];
	char CharBuf32[32];

	tmp = ini_file.read ("ipc", "SEND_SEM_KEY");
	if (tmp.empty ())
		return -1;
	ini.send_sem_key = atoi (tmp.c_str());

	tmp = ini_file.read ("ipc", "RECV_SEM_KEY");
	if (tmp.empty ())
		return -1;
	ini.recv_sem_key = atoi (tmp.c_str());

	tmp = ini_file.read ("ipc", "SHM_QUEUE_LENGTH");
	if (tmp.empty ())
		return -1;
	ini.shm_queue_length = atoi (tmp.c_str());

	tmp = ini_file.read ("ipc", "SHM_QUEUE_SEND_LENGTH");
		if (tmp.empty ())
			return -1;
	ini.shm_queue_send_length = atoi (tmp.c_str());


	tmp = ini_file.read ("ipc", "FIFO_PATH");
	if (tmp.empty ())
		memset (ini.fifo, 0x0, sizeof ini.fifo);
	else
		strncpy (ini.fifo, tmp.c_str(), sizeof ini.fifo);

	tmp = ini_file.read ("logger", "LOG_PRENAME");
	if (tmp.empty ())
		memset (ini.log_prename, 0x0, sizeof ini.log_prename);
	else
		strncpy (ini.log_prename, tmp.c_str(), sizeof ini.log_prename);

	tmp = ini_file.read ("logger", "LOG_PRIORITY");
	if (tmp.empty ())
		return -1;
	ini.log_priority = atoi (tmp.c_str());

	tmp = ini_file.read ("logger", "LOG_SIZE");
	if (tmp.empty ())
		return -1;
	ini.log_size = atoi (tmp.c_str());

	tmp = ini_file.read ("misc", "SOCKET_TIMEOUT");
	if (tmp.empty ())
		return -1;
	ini.socket_timeout = atoi (tmp.c_str());

	tmp = ini_file.read ("misc", "SESSION_TIMEOUT");
	if (tmp.empty ())
		return -1;
	ini.session_timeout = atoi (tmp.c_str());

	tmp = ini_file.read ("misc", "CHECK_TIMEOUT");
	if (tmp.empty ())
		ini.check_timeout = 10;
	else
		ini.check_timeout = atoi (tmp.c_str());

	tmp = ini_file.read ("misc", "SOCKET_BUFSIZE");
	if (tmp.empty ())
		return -1;
	ini.socket_bufsize = atoi (tmp.c_str());

	tmp = ini_file.read ("misc", "ACCEPT_BACKLOG");
	if (tmp.empty ())
		ini.backlog = 10;
	else
		ini.backlog = atoi (tmp.c_str());

	tmp = ini_file.read ("misc", "MONITOR_TIME");
	if (tmp.empty ())
		ini.monitor_time = 86400;
	else
		ini.monitor_time = atoi (tmp.c_str());
	
	tmp = ini_file.read ("misc", "WORKER_PROC_NUM");
	if (tmp.empty ())
		return -1;
	ini.worker_proc_num = atoi (tmp.c_str());

	tmp = ini_file.read ("path", "LOG_DIR");
	strncpy (ini.log_dir, tmp.c_str(), sizeof ini.log_dir);

	tmp = ini_file.read ("path", "BIND_FILE");
	strncpy (ini.bind_file, tmp.c_str(), sizeof ini.bind_file);
	if (strlen (ini.log_dir) == 0 || strlen(ini.bind_file) == 0)
	{
		printf("config file LOG_DIR:%s,BIND_FILE:%s error\n" ,ini.log_dir, ini.bind_file);
		return -1;
	}

	//fdfs
	tmp = ini_file.read ("FASTDFS", "FDFS_CONF");
	if (tmp.empty ())
		return -1;
	else
		strncpy (ini.fdfsconf, tmp.c_str(), sizeof ini.fdfsconf);

	//aws::s3参数
	tmp = ini_file.read("sdk", "accessKey");
	if (tmp.empty())
		memset(ini.accessKey, 0x0, sizeof ini.accessKey);
	else
		strncpy(ini.accessKey, tmp.c_str(), sizeof ini.accessKey);

	tmp = ini_file.read("sdk", "secretKey");
	if (tmp.empty())
		memset(ini.secretKey, 0x0, sizeof ini.secretKey);
	else
		strncpy(ini.secretKey, tmp.c_str(), sizeof ini.secretKey);

	tmp = ini_file.read("sdk", "endPoint");
	if (tmp.empty())
		memset(ini.endPoint, 0x0, sizeof ini.endPoint);
	else
		strncpy(ini.endPoint, tmp.c_str(), sizeof ini.endPoint);

	tmp = ini_file.read("sdk", "region");
	if (tmp.empty())
		memset(ini.region, 0x0, sizeof ini.region);
	else
		strncpy(ini.region, tmp.c_str(), sizeof ini.region);

	tmp = ini_file.read("sdk", "bucket");
	if (tmp.empty())
		memset(ini.bucket, 0x0, sizeof ini.bucket);
	else
		strncpy(ini.bucket, tmp.c_str(), sizeof ini.bucket);

	tmp = ini_file.read("sdk", "depositDir");
	if (tmp.empty())
		memset(ini.depositDir, 0x0, sizeof ini.depositDir);
	else
		strncpy(ini.depositDir, tmp.c_str(), sizeof ini.depositDir);

	tmp = ini_file.read("sdk", "useVirtualAddressing");
	if (tmp.empty())
		return -1;
	ini.useVirtualAddressing = atoi(tmp.c_str());

	tmp = ini_file.read("sdk", "switch");
	if (tmp.empty())
		return -1;
	ini.s3Switch = atoi(tmp.c_str());

	//db
	memset(ini.host, 0 , sizeof(ini.host));
	strcpy(ini.host, ini_file.read("db", "host").c_str());

	ini.db_port = 0;
	tmp = ini_file.read ("db", "port");
	if (tmp.empty ())
		ini.db_port = 0;
	else
		ini.db_port = atoi (tmp.c_str());
	printf("db_port=%d\n",ini.db_port);
	memset(ini.user, 0 , sizeof(ini.user));
	strcpy(ini.user,ini_file.read("db", "user").c_str());
	memset(ini.pass, 0 , sizeof(ini.pass));
	strcpy(ini.pass,ini_file.read("db", "pass").c_str());
	memset(ini.dbname, 0 , sizeof(ini.dbname));
	strcpy(ini.dbname,ini_file.read("db", "dbname").c_str());

	snprintf(ini.szsvrversion,sizeof(ini.szsvrversion), "%s",MAKE_VERSION);
	snprintf(ini.szsvrmaketime,sizeof(ini.szsvrmaketime), "%s",DATE);
	ini.laststart = time(NULL);
	//----------------------------------------
	tmp = ini_file.read ("key", "KEY_A");//读取硬件绑定key
	if (tmp.empty ())
		return -1;
	strncpy (ini.key_a, tmp.c_str(), sizeof(ini.key_a));
	//----------------------------------------
	//key_a解密
	strncpy(CharBuf32, ini.key_a, sizeof (CharBuf32));//复制数据到暂存
	//printf ("KEY_CPUIDMAC-32:%32s\n", CharBuf32);
	HexStrToStr((unsigned char *)CharBuf32,32,(unsigned char *)CharBuf16);//HEX字符串转HEX 
	//btea((uint32_t *)CharBuf16,4,KeyDataKeyA);//加密
	btea((uint32_t *)CharBuf16,-4,KeyDataKeyA);//解密
	StrToHexStr(CharBuf16,16,CharBuf32);//HEX转HEX字符串
	//printf ("KEY_KEYA-32:%32s\n", CharBuf32);
	strncpy(ini.key_a, CharBuf32, sizeof (ini.key_a));//保存数据
	//----------------------------------------
	return 0;
}

int	Option::parse_network(const char* config_file)
{
	IniFile net_file;
	char CharBuf[16];
	char CharBuf16[16];
	char CharBuf20[20];
	char CharBuf40[40];
	char CharBuf80[80];

	
	if (!net_file.open (config_file))
	{
		printf ("open config file:%s failed\n", config_file);
		return -1;
	}

	string	section_str;
	section_str = net_file.read ("main", "BIND_COUNT");
	if (section_str.empty ())
		return -1;
	bind_count = atoi (section_str.c_str());

	if (bind_count <= 0 || bind_count > 16)
	{
		printf ("config file:%s BIND_COUNT error\n",section_str.c_str());
		return -1;
	}

	for (int i=0; i<bind_count; i++)
	{
		char sect_head[32];
		sprintf( sect_head, "port%d", i);

		section_str = net_file.read (sect_head, "IP");//读取IP地址
		if (section_str.empty ())//判断IP地址是否为空
		{
			printf ("config file:%s ip error\n", config_file);
			return -1;
		}
		else
		{
			//---------------------------------------------------------------------------
			//section_str="3DCDCDF51C6CD94CE3509C612A589574C46228C2CDE9C56A14F87C6A9CBE37785D275249A62E49F9";
			strncpy(CharBuf80, section_str.c_str(), sizeof (CharBuf80));
			//printf ("BIND_IP_key80:%s\n", CharBuf80);

			HexStrToStr((unsigned char *)CharBuf80,80,(unsigned char *)CharBuf40);//HEX字符串转HEX 
			//printf ("BIND_IP_key80-40:%s\n", CharBuf40);
			
			btea((uint32_t *)CharBuf40,-10,KeyData);//解密
			//printf ("BIND_IP_de:%s\n", CharBuf40);

			//StrToHexStr(CharBuf40,40,CharBuf80);//HEX转HEX字符串//打印用
			//printf ("BIND_IP_de_80:%s\n", CharBuf80);

			//解密后的数据转换成ip字符串
			HexStrToStr((unsigned char *)CharBuf40,40,(unsigned char *)CharBuf20);
			//printf ("BIND_IP-20:%s\n", CharBuf20);

			//验证checksum
			if(checksum((unsigned char *)CharBuf20,16)==1)
			{
				//printf ("BIND_IP-file-err:\n", 0);
				return -1;
				//exit (1);
			}

			strncpy(CharBuf16, CharBuf20, sizeof (CharBuf16));//复制16字节ip数据
			//printf ("BIND_IP:%s\n", CharBuf16);
			//---------------------------------------------------------------------------
			strncpy (bind_port[i].ip, CharBuf16, sizeof (bind_port[i].ip));//存储绑定的IP地址
		}

		section_str = net_file.read (sect_head, "PORT");//读取端口号
		if (section_str.empty ())
		{
			printf ("config file:%s port error\n", config_file);
			return -1;
		}
		else
			bind_port[i].port = atoi(section_str.c_str());//把端口号字符串转成整形

		section_str = net_file.read (sect_head, "TYPE");//读取端口协议类型
		bind_port[i].type = (section_str == "UDP" ? 0:1);

		section_str = net_file.read (sect_head, "PROTOCOL");
		bind_port[i].protocol = atoi(section_str.c_str());
	}
	return 0;
}

void Option::print ()
{
	printf ("LOG_DIR:%s\n", ini.log_dir);
	printf ("BIND_FILE:%s\n\n", ini.bind_file);

	printf ("SEND_SEM_KEY:%d\n", ini.send_sem_key);
	printf ("RECV_SEM_KEY:%d\n\n", ini.recv_sem_key);
	printf ("SHM_QUEUE_LENGTH:%d\n\n", ini.shm_queue_length);

	printf ("LOG_PRIORITY:%d\n", ini.log_priority);
	printf ("LOG_PRENAME:%s\n\n", ini.log_prename);

	printf ("SOCKET_TIMEOUT:%d\n", ini.socket_timeout);
	printf ("SOCKET_BUFSIZE:%d\n", ini.socket_bufsize);
	printf ("WORKER_PROC_NUM:%d\n\n", ini.worker_proc_num);

	printf("s3 endPoint:%s \n\n", ini.endPoint);

	for (int i=0; i<bind_count; i++)
	{
		printf ("IP:%s\n", bind_port[i].ip);
		printf ("PORT:%d\n", bind_port[i].port);
		printf ("TYPE:%d\n", bind_port[i].type);
	}
}


//-------------------------------------------------------------------
//计算检验和
//len
//最后4个自己为校验和结果
//计算结果和最后四位不一致，返回1
int checksum(unsigned char *str,unsigned char len)
{
	unsigned char i;
	unsigned long data;
	unsigned long a;

	a=str[len];
	data=str[len+1];
	a=(a<<8)|(data&0xff);
	data=str[len+2];
	a=(a<<8)|(data&0xff);
	data=str[len+3];
	a=(a<<8)|(data&0xff);
	
	data=0;
	for(i=0;i<len;i++)
	{
		data+=str[i];
	}
	
        str[len]=(data>>24)&0xff;
	str[len+1]=(data>>16)&0xff;
	str[len+2]=(data>>8)&0xff;
	str[len+3]=data&0xff;

	if(a!=data)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
//-------------------------------------------------------------------
//十六进制字符串转字符串
//len必须是偶数
void HexStrToStr(unsigned char *hex,unsigned char len,unsigned char *str)
{
	unsigned char i;
	unsigned char a;
	len=len/2;
	for(i=0;i<len;i++)
	{
		//高四位
		if((hex[i*2]>='A')&&(hex[i*2]<='F'))//=A~F
		{
			str[i]=(hex[i*2]-0x37)&0x0f;
		}
		else if((hex[i*2]>='a')&&(hex[i*2]<='f'))//=a~f
		{
			str[i]=(hex[i*2]-0x57)&0x0f;
		}
		else if((hex[i*2]>='0')&&(hex[i*2]<='9'))//=0~9
		{
			str[i]=(hex[i*2]-0x30)&0x0f;
		}
		str[i]=str[i]<<4;//高位在前
		//低四位
		if((hex[(i*2)+1]>='A')&&(hex[(i*2)+1]<='F'))//=A~F
		{
			str[i]|=(hex[(i*2)+1]-0x37)&0x0f;
		}
		else if((hex[(i*2)+1]>='a')&&(hex[(i*2)+1]<='f'))//=A~F
		{
			str[i]|=(hex[(i*2)+1]-0x57)&0x0f;
		}
		else if((hex[(i*2)+1]>='0')&&(hex[(i*2)+1]<='9'))//=0~9
		{
			str[i]|=(hex[(i*2)+1]-0x30)&0x0f;
		}
	}
        //str[i]=0;//字符串最后补0结束
}

//-------------------------------------------------------------------
//字符串转十六进制字符串
//len
void StrToHexStr(char *str,unsigned char len,char *HexStr)
{
	unsigned char i;
	unsigned char a;
	for(i=0;i<len;i++)
	{
		a=(str[i]>>4)&0x0f;
		if((a>=0x0A)&&(a<=0x0F))//=A~F
		{
			HexStr[i*2]=a+0x37;
		}
		else if((a>=0x00)&&(a<=0x09))//=0~9
		{
			HexStr[i*2]=a+'0';
		}
		a=str[i]&0x0f;
		if((a>=0x0A)&&(a<=0x0F))//=A~F
		{
			HexStr[(i*2)+1]=a+0x37;
		}
		else if((a>=0x00)&&(a<=0x09))//=0~9
		{
			HexStr[(i*2)+1]=a+'0';
		}    
	}
        //str[(i*2)]=0;//字符串最后补0结束
}
//-------------------------------------------------------------------

//---------------------------------------------------------------
#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))
//n的绝对值表示v的长度，取正表示加密，取负表示解密  
// v为要加密的数据是两个32位无符号整数  
// k为加密解密密钥，为4个32位无符号整数，即密钥长度为128位
static void btea(uint32_t *v, int n, uint32_t const key[4])
{
    uint32_t y, z, sum;
    unsigned p, rounds, e;
    if (n > 1) {          /* Coding Part */
        rounds = 6 + 52/n;
        sum = 0;
        z = v[n-1];
        do {
            sum += DELTA;
            e = (sum >> 2) & 3;
            for (p=0; p<n-1; p++) {
                y = v[p+1];
                z = v[p] += MX;
            }
            y = v[0];
            z = v[n-1] += MX;
        } while (--rounds);
    } else if (n < -1) {  /* Decoding Part */
        n = -n;
        rounds = 6 + 52/n;
        sum = rounds*DELTA;
        y = v[0];
        do {
            e = (sum >> 2) & 3;
            for (p=n-1; p>0; p--) {
                z = v[p-1];
                y = v[p] -= MX;
            }
            z = v[n-1];
            y = v[0] -= MX;
            sum -= DELTA;
        } while (--rounds);
    }
}



