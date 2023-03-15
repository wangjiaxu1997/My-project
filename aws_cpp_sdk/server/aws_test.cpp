#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "Log.h"
#include "Options.h"
#include "Common.h"

#include <stdlib.h>
#include <unistd.h>

char dll_file[256] = {0};
char ini_file[256] = {0};
Option option;


int parse_args (int argc, char *argv[])
{
	if (argc  < 2)
	{
		printf ("Startup parameter error: argc=%d\n" , argc);
		return -1;
	}
	else if (argc == 2)
	{
		strncpy (dll_file ,argv[1], sizeof (dll_file));
		if (load_dll (dll_file))
			return -1;
		return -1;

	}

	strncpy (ini_file ,argv[1], sizeof (ini_file));
	strncpy(dll_file, argv[2], sizeof(dll_file));

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

//int64_t GetSysTimeMicros()
//{
//#ifdef _WIN32
//	// 从1601年1月1日0:0:0:000到1970年1月1日0:0:0:000的时间(单位100ns)
//#define EPOCHFILETIME   (116444736000000000UL)
//	FILETIME ft;
//	LARGE_INTEGER li;
//	int64_t tt = 0;
//	GetSystemTimeAsFileTime(&ft);
//	li.LowPart = ft.dwLowDateTime;
//	li.HighPart = ft.dwHighDateTime;
//	// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
//	tt = (li.QuadPart - EPOCHFILETIME) / 10;
//	return tt;
//#else
//	timeval tv;
//	gettimeofday(&tv, 0);
//	return (int64_t)tv.tv_sec * 1000000 + (int64_t)tv.tv_usec;
//#endif // _WIN32
//	return 0;
//}

int UploadFileByBuff(const char *file_content, int content_len, const char *file_ext_name, char file_id[], int num)
{
	time_t nowtime;
	struct tm* p;
	time(&nowtime);
	p = localtime(&nowtime);
	char szfilename[128] = { 0 };
	//int64_t i64_curTime = GetSysTimeMicros();
	//snprintf(szfilename, sizeof(szfilename), "%s/%d%d%d/%ld.%s", ini.depositDir, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, i64_curTime, file_ext_name);
	snprintf(szfilename, sizeof(szfilename), "%s/%d%d%d/%d.%s", ini.depositDir, 1900 + p->tm_year, 1 + p->tm_mon, p->tm_mday, num, file_ext_name);
	strncpy(file_id, szfilename, strlen(szfilename));

	if (!asyns_upload_file(ini.bucket, szfilename, file_content, content_len))
	{
		LOG(LOG_DEBUG, "CAwsS3FileHandle::UploadFileByBuff upload failed! filename:%s", szfilename);
		return -1;
	}
	//LOG(LOG_DEBUG, "CAwsS3FileHandle::UploadFileByBuff upload success!");
	return 0;
}

int main(int argc, char *argv[])
{
	//配置参数初始化
	if (parse_args (argc, argv))
		return -1;

	//日志文件初始化
	Log::instance()->init_log (ini.log_dir, ini.log_priority, ini.log_prename, ini.log_size);

	if (load_dll(dll_file))
		return -1;

	aws_init(ini.endPoint, ini.region, ini.accessKey, ini.secretKey, ini.useVirtualAddressing);

	//压测
	string buf("不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不\
		不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不不");
	while (1)
	{
		time_t now = time(NULL);
		int number = 0;
		while (1)
		{
			time_t Now = time(NULL);
			char file_id[128] = { 0 };
			if(UploadFileByBuff(buf.c_str(), buf.length(), "silk", file_id, number)==0)
				number++;
			if (Now - now >= 10)
				break;
		}

		LOG( LOG_DEBUG ,"-------------------------tatol=%d ",number);
		sleep(20);
		//break;
	}

	while (1)
		sleep(60);
	delete Log::instance();
    return 0;
}


