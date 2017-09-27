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

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "io.h"
#include "shell.h"
#include "usb_transport.h"
#include "message.h"
#include "protocol.h"

int high_fd;
int fds[TOTAL_FDS];

#define FD_READABLE(x)			(x >= 0 && FD_ISSET(x, &rfd))

void set_set(fd_set * s);

void set_set(fd_set * s)
{
	int i;
	FD_ZERO(s);
	for(i = 0; i < TOTAL_FDS; i++)
	{
		if(fds[i] >= 0)
			FD_SET(fds[i], s);
	}
}

void set_high_fd(void)
{
	int i;
	high_fd = 0;
	for(i = 0; i < TOTAL_FDS; i++)
	{
		if(fds[i] > high_fd)
			high_fd = fds[i];
	}
	high_fd++;
}

void clear_fds(void)
{
	int i;
	for(i = 0; i < TOTAL_FDS; i++)
		fds[i] = -1;
}

void set_non_blocking(void)
{
	int i;
	int flags;
	for(i = 0; i < TOTAL_FDS; i++)
	{
		if(fds[i] < 0)
			continue;
		flags = fcntl(fds[i], F_GETFL, 0);
		flags |= O_NONBLOCK;
		if(fcntl(fds[i], F_SETFL, flags) != 0)
			error_message("Can't set file descriptor to non blocking %d err: %d\n", fds[i], errno);			//fatal?
	}
}

int select_loop(void)
{
	char * buffer;
	int bytes;
	fd_set rfd, wfd, efd;
	int n;
	
	terminal_pid = 0;
	term_stdinout_fd = -1;	
	usb_control_fd = -1;
	usb_bulkin_fd = -1;
	usb_bulkout_fd = -1;
	clear_fds();
	
	buffer = malloc(MAX_BUFFER_SIZE);
	
	if(transport_init() < 0)
		return -1;
		
#ifdef LFMB_CLIENT
	//client only uses this for shell
	term_stdinout_fd = STDIN_FILENO;
	fds[3] = term_stdinout_fd;
	set_high_fd();
	if(send_open_shell() < 0)
		return -1;
	message("sent open shell\n");
	if(receive_ack() < 0)
		return -1;
	message("received ack\n");
	//do we set the connection state here to s_shell for the client or do we care ? FIXME
#endif //LFMB_CLIENT

	while(1)
	{
		set_set(&rfd);
		FD_CLR(fds[1], &rfd);			//we don't care about reading from bulkin, it's the write to the usb ffs
		set_set(&efd);
		n = select(high_fd, &rfd, NULL, &efd, NULL);
		if(n == -1)
		{
			if(errno != EINTR)
			{
				error_message("select error: %d\n", errno);
				if(transport_reset() < 0)
					{ free(buffer); return -1; }
			}
		}
		else if(n)
		{
			if(FD_READABLE(term_stdinout_fd))
			{
				bytes = read(term_stdinout_fd, buffer, MAX_BUFFER_SIZE);
				if(bytes < 0)
				{
					if(errno == EIO)
					{
#ifndef LFMB_CLIENT
						message("terminal exited\n");
						if(send_disconnect() < 0)
							error_message("error sending disconnect\n");
						fds[3] = -1;
						close(term_stdinout_fd);
						waitpid((pid_t)terminal_pid, NULL, 0);
						terminal_pid = 0;
						term_stdinout_fd = -1;
#endif //!LFMB_CLIENT
					}
					else
						error_message("shell stdinout read error: %d\n", errno);
				}
				else 
				{
#ifdef LFMB_CLIENT
					if(send_to_shell(buffer, bytes) < 0)
#else
					if(send_from_shell(buffer, bytes) < 0)
#endif //LFMB_CLIENT
					{
						if(transport_reset() < 0)
							{ free(buffer); return -1; }
						//but then what, we just never send this data?
						//maybe this should be fatal... FIXME
					}				
				}
			}
			if(FD_READABLE(usb_control_fd))
			{
				// what would this be, something from the driver?
				message("usb control is readable\n");
			}
			if(FD_READABLE(usb_bulkout_fd))
			{
#ifdef LFMB_CLIENT
				if(read_from_shell() < 0)
#else
				if(read_and_handle_usb() < 0)
#endif //LFMB_CLIENT
				{
					if(fds[2] == -1)
						break;
					if(transport_reset() < 0)
						{ free(buffer); return -1; }
				}
			}
			if(FD_READABLE(usb_bulkin_fd))
			{
				message("bulk in is readable\n");
			}
		}
	}

	//what about closing things gracefully?  SIGHUP? FIXME	
	
	free(buffer);
	return 0;
}

