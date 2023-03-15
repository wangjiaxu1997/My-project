#include "Pipe_Notify.h"
#include "Common.h"
#include "Log.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define READ_PIPE_BUF_MAX	4096
int pipe_handles[2];
int create_pipe ()
{
	if (pipe (pipe_handles) == -1)
	{
		fprintf (stderr, "pipe failed,%s\n", strerror (errno));
		return -1;
	}

	int val = fcntl(pipe_handles[0], F_GETFL,0); 
	val |= O_NONBLOCK; 
	fcntl(pipe_handles[0], F_SETFL, val);

	val = fcntl(pipe_handles[1], F_GETFL,0); 
	val |= O_NONBLOCK; 
	fcntl(pipe_handles[1], F_SETFL, val);

	return 0;
}

int close_wr_pipe ()
{
	if (close (pipe_handles[1]) == -1)
	{
		LOG (LOG_ERROR, "close_wr_pipe failed,%s", strerror (errno));
		return -1;
	}		

	return 0;
}

int close_rd_pipe ()
{
	if (close (pipe_handles[0]) == -1)
	{
		LOG (LOG_ERROR, "close_rd_pipe failed,%s", strerror (errno));
		return -1;
	}		

	return 0;
}

int pipe_rd_fd ()
{
	return pipe_handles[0];
}

int read_pipe ()
{
	char	buffer[READ_PIPE_BUF_MAX];
	if (read (pipe_handles[0], buffer, READ_PIPE_BUF_MAX) <= 0)
	{
//		LOG (LOG_ERROR, "read_pipe failed,%s", strerror (errno));
		return -1;
	}		

	return 0;
}

int write_pipe ()
{
	char notify_chr = '0';
	if (write (pipe_handles[1], &notify_chr, 1) <= 0)
	{
		LOG (LOG_ERROR, "write_pipe failed,%s", strerror (errno));
		return -1;
	}		
//	LOG (LOG_VERBOSE, "write_pipe ok");

	return 0;
}
