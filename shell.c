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
#include <string.h>
#include <unistd.h>
#include <pty.h>
#include <errno.h>
#include "message.h"
#include "shell.h"
#include "io.h"
 
#define SHELL_PATH		"/bin/bash"

#define ENV_STRINGS		2
#define TERM_STRING		"TERM=linux"
#define HOME_STRING		"HOME=/home"


int term_stdinout_fd;
int terminal_pid;

struct termios terminal_state;

void terminal_set(int fd);
void terminal_restore(int fd);

#ifndef LFMB_CLIENT
int init_terminal(void)
{
	char ** env;
	int i;
	
	terminal_pid = (int)forkpty(&term_stdinout_fd, NULL, NULL, NULL);
	fds[3] = term_stdinout_fd;
	set_high_fd();
 	if(terminal_pid < 0)
 	{
 		error_message("failed to fork terminal err %d\n", errno);
 		return -1;
 	}
 	else if(terminal_pid == 0)
 	{
 		env = (char **)malloc((ENV_STRINGS + 1) * sizeof(char *));
 		env[0] = (char *)calloc(1, sizeof(TERM_STRING) + 1);
 		strcpy(env[0], TERM_STRING);
 		env[1] = (char *)calloc(1, sizeof(HOME_STRING) + 1);
 		strcpy(env[1], HOME_STRING);
		env[2] = NULL;
		 		
 		dup2(term_stdinout_fd, STDIN_FILENO);
 		dup2(term_stdinout_fd, STDOUT_FILENO);
 		dup2(term_stdinout_fd, STDERR_FILENO);
 		
 		terminal_set(term_stdinout_fd);
 		execle(SHELL_PATH, SHELL_PATH, "-", NULL, env);
 		terminal_restore(term_stdinout_fd);
 		
 		for(i = 0; i < ENV_STRINGS; i++)
 			free(env[i]);
 		free(env);
 		exit(0);
 	}
 	
 	return 0;
}
#else

int open_shell(void)
{
	terminal_set(STDIN_FILENO);
	
	if(select_loop() < 0)
		return -1;
	
	//SIGHUP ?  exit?

	terminal_restore(STDIN_FILENO);

	return 0;
}

int receive_shell_from_server(char * buffer, int len)
{
	//how can we check here for exit/logout command... or do we send it?
	if(writefd(STDOUT_FILENO, buffer, len) < 0)
	{
		return -1;
	}
	return 0;
}
#endif //LFMB_CLIENT

void terminal_set(int fd)
{
    if(tcgetattr(fd, &terminal_state))
    	return;

    struct termios tio;
    if (tcgetattr(fd, &tio))
    	return;

    cfmakeraw(&tio);

    tio.c_cc[VTIME] = 0;			//tenths of a second between bytes
    tio.c_cc[VMIN] = 1;				//minimum characters

    tcsetattr(fd, TCSAFLUSH, &tio);
}

void terminal_restore(int fd)
{
    tcsetattr(fd, TCSAFLUSH, &terminal_state);
}



