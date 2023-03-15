#ifndef PIPE_NOTIFY_H
#define PIPE_NOTIFY_H
int create_pipe ();
int close_wr_pipe ();
int close_rd_pipe ();
int read_pipe ();
int write_pipe (); 
int pipe_rd_fd ();
#endif
