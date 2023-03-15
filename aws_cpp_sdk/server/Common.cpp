#include "Common.h"
#include "Options.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/resource.h>

init_factory app_init;
fini_factory app_fini;
stat_factory app_stat;

handle_process_factory app_process;
handle_check_timeout_factory  app_check_timeout;
handle_close_factory app_close;
handle_input_factory app_input;
handle_check_msg_factory app_send_msg;

aws_init_factory aws_init;
handle_upload_file_factory sync_upload_file;
handle_upload_file_factory asyns_upload_file;
handle_upload_file_factory random_http_put;
aws_upload_callback_factory aws_upload_callback;

bool stopped = false;

int64_t CONNECTION_ID (int ip, unsigned short port, int fd) 
{	
	int64_t retval, tmp; 
	tmp = ip;
	retval = tmp << 32; 
	
	//start ipc:yuyang, Date: 2010-02-04
	#if 0
	tmp = port;
	retval += tmp << 16; 
	#endif
	//end ipc:yuyang, Date: 2010-02-04
	retval += fd;	
	return retval; 
}

Connect_Session_t::~Connect_Session_t ()
{
	if (recv_mb != NULL)
	{
		free (recv_mb);
		recv_mb = NULL;
	}
	if (send_mb != NULL)
	{
		free (send_mb);
		send_mb = NULL;
	}

	send_len = 0;
	recv_len = 0;
	id = 0;
}

Connect_Session_t::Connect_Session_t (int64_t key, char t , int p )
{
	id = key;
	type = t;
	protocol = p;
	gettimeofday(&stTv,NULL);
	recv_len = 0;
	send_len = 0;
	recv_mb = (char *) malloc (ini.socket_bufsize );
	send_mb = (char *) malloc (ini.socket_bufsize );

	//note here 
	send_pos = 0; //robot add it ...
}

int load_dll(const char* dll_name)
{
	void *handle;
	char *error = NULL;

	handle = dlopen (dll_name, RTLD_NOW);
	if ((error = dlerror()) != NULL)
		goto err_entry;

	app_init = (init_factory) dlsym (handle, "init");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	app_input = (handle_input_factory) dlsym (handle, "handle_input");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	app_close = (handle_close_factory) dlsym (handle, "handle_close");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	app_check_timeout =(handle_check_timeout_factory)dlsym(handle,"handle_check_timeout");
	if ((error = dlerror()) != NULL)
		goto err_entry;
	
	app_process = (handle_process_factory) dlsym (handle, "handle_process");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	app_send_msg = (handle_check_msg_factory) dlsym (handle, "handle_Check_Msg");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	aws_upload_callback = (aws_upload_callback_factory)dlsym(handle, "handle_aws_upload_callback");
	if ((error = dlerror()) != NULL)
		goto err_entry;
	//maybe no defined
	app_fini = (fini_factory) dlsym (handle, "fini");
	app_stat = (stat_factory) dlsym (handle, "handle_stat");

	/////////////////////////////////////
	handle = dlopen("../lib/libAws.so", RTLD_NOW);
	if ((error = dlerror()) != NULL)
		goto err_entry;

	printf("dlopen libAws.so success");
	aws_init = (aws_init_factory)dlsym(handle, "init");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	sync_upload_file = (handle_upload_file_factory)dlsym(handle, "handle_sync_upload_file");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	asyns_upload_file = (handle_upload_file_factory)dlsym(handle, "handle_asyns_upload_file");
	if ((error = dlerror()) != NULL)
		goto err_entry;

	random_http_put = (handle_upload_file_factory)dlsym(handle, "handle_random_http_put");
	if ((error = dlerror()) != NULL)
		goto err_entry;
	//////////////////////////////////////

	return 0;
err_entry:
	printf("open %s error: %s\n" ,dll_name, error);
	return -1;
}

static void sigterm_handler(int signo) 
{
	stopped = true;
}

void daemon_start ()
{
	struct sigaction sa;
	sigset_t sset;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	signal(SIGPIPE,SIG_IGN);	
	signal(SIGCHLD,SIG_IGN);	

	sigemptyset(&sset);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGFPE);
	//sigprocmask(SIG_UNBLOCK, &sset, &sset);

	daemon (1, 1);
}

void init_fdlimit ()
{
	//struct rlimit rlim;

	///* raise open files */
	//rlim.rlim_cur = MAXFDS;
	//rlim.rlim_max = MAXFDS;
	//setrlimit(RLIMIT_NOFILE, &rlim);
#if 0
	/* allow core dump */
	rlim.rlim_cur = RLIM_INFINITY;
	rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);
#endif	
}

