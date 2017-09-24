/*  lfmbd: linux for mobile bridge daemon 
 *  
 *  Copyright(C) 2017  Peter Bohning
 *  This program is free software : you can redistribute it and / or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <errno.h>
#include "message.h"
#include "shell.h"
 
#define SHELL_PATH		"/bin/bash"

int term_stdinout_fd;

#ifndef LFMB_CLIENT
int init_terminal(void)
{
	char ** env;
	
	//probably should load an env VAR=value file into env, just a bunch of strings
	//FIXME
	
	pid_t pid = forkpty(&term_stdinout_fd, NULL, NULL, NULL);
 	if(pid < 0)
 	{
 		error("failed to fork terminal err %d\n", errno);
 		return -1;
 	}
 	else if(pid == 0)
 	{
 		dup2(term_stdinout_fd, STDIN_FILENO);
 		dup2(term_stdinout_fd, STDOUT_FILENO);
 		dup2(term_stdinout_fd, STDERR_FILENO);
 		
 		execle(SHELL_PATH, SHELL_PATH, "-", NULL, env);
 		
 		exit(0);
 	}
 	
 	return 0;
}
#else
int open_shell(void)
{
	char * buffer;
	char * inbuffer;
	unsigned int size;
	int ret;
	
	if(send_open_shell() < 0)
		return -1;
	if(receive_ack() < 0)
		return -1;
	//do we set the connection state here to s_shell for the client or do we care ? FIXME
	
	inbuffer = (char *)malloc(MAX_BUFFER_SIZE);
	while(1)
	{	
		if(read_from_shell(&buffer, &size) < 0)
		{
			error("read from shell error\n");
		}
		if(buffer)
		{
			if(writefd(STDOUT_FILENO, buffer, size) < 0)
			{
				free(buffer);
			}
			//free(buffer);
		}
		ret = read(STDIN_FILENO, inbuffer, MAX_BUFFER_SIZE);
		if(ret > 0)
		{
			//how can we check here for exit/logout command... or do we send it?
			if(send_to_shell(inbuffer, ret) < 0)
			{
				
			}
		}
		else if(ret < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			{
				message("EAGAIN/EWOULDBLOCK/EINTR\n");
			}
			else
			{
				error("read stdin error %d\n", errno);
			}	
		}
		//SIGHUP ?  exit?
	}
	
	free(inbuffer);
	
	return 0;
}
#endif //LFMB_CLIENT

