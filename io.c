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
#include "packet.h"

int high_fd;
int fds[TOTAL_FDS];

char * g_read_buffer = NULL;
struct packet_packet * packet_chain = NULL;

#define FD_READABLE(x)			(x >= 0 && FD_ISSET(x, &rfd))
#define FD_WRITABLE(x)				(x >= 0 && FD_ISSET(x, &wfd))
#define FD_ERROR(x)				(x >= 0 && FD_ISSET(x, &efd))

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

/* Big mess.  select doesn't poll properly, writes and reads hang even with non blocking, libusb has it's own thing
 * so, host requests, device responds.  That's it.  */
int select_loop(void)
{
	char * b;
	int bytes;
	fd_set rfd, wfd, efd;
	int n;
	int ret;
	struct packet_packet * p;
	char * buffer;
	
	terminal_pid = 0;
	term_stdinout_fd = -1;	
	usb_control_fd = -1;
	usb_bulkin_fd = -1;
	usb_bulkout_fd = -1;
	clear_fds();
	
	if(allocate_read_buffer() < 0)
		return -1;
	packet_chain = NULL;
	
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
		buffer = NULL;
		set_set(&rfd);
		set_set(&wfd);
		//FD_CLR(usb_control_fd  &rfd);
		FD_CLR(usb_bulkin_fd, &wfd);
		FD_CLR(usb_bulkout_fd, &rfd);
		set_set(&efd);
		n = select(high_fd, &rfd, &wfd, &efd, NULL);
		if(n == -1)
		{
			if(errno != EINTR)
			{
				error_message("select error: %d\n", errno);
				if(transport_reset() < 0)
					{ free_read_buffer(); return -1; }
			}
		}
		else if(n)
		{
			if(FD_ERROR(usb_control_fd))
			{
				error_message("usb_control_fd has error condition pending\n");

			}
			if(FD_ERROR(usb_bulkout_fd))
			{
				error_message("usb_bulkout_fd has error condition pending\n");

			}
			if(FD_ERROR(usb_bulkin_fd))
			{
				error_message("usb_bulkin_fd has error condition pending\n");

			}
			if(FD_READABLE(term_stdinout_fd))
			{
				buffer = malloc(MAX_BUFFER_SIZE);
				//don't read more than we can send in a single packet (or a split header/packet...)
				bytes = read(term_stdinout_fd, buffer + sizeof(struct packethdr), MAX_BUFFER_SIZE - sizeof(struct packethdr));
				if(bytes < 0)
				{
					if(errno == EIO)
					{
#ifndef LFMB_CLIENT
						message("terminal exited\n");
						if(post_disconnect() < 0)
							error_message("error posting disconnect\n");
						fds[3] = -1;
						close(term_stdinout_fd);
						waitpid((pid_t)terminal_pid, NULL, 0);
						terminal_pid = 0;
						term_stdinout_fd = -1;
#endif //!LFMB_CLIENT
					}
					else
						error_message("shell stdinout read error: %d\n", errno);
					free(buffer);
					buffer = NULL;
#ifdef LFMB_CLIENT
					//server can reset, client can just exit
					return -1;
#endif //LFMB_CLIENT
				}
#ifndef LFMB_CLIENT
				else
				{
					//post this data for a later reply message
					if(send_from_shell(buffer, bytes) < 0)
					{
						//this would be some fatal memory error... 
						free_read_buffer();
						return -1;
					}				
				}
#endif //!LFMB_CLIENT
			}
			if(FD_READABLE(usb_control_fd))
			{
				message("usb control is readable\n");
				bytes = read(usb_control_fd, g_read_buffer, MAX_BUFFER_SIZE);
				if(bytes < 0)
				{
						error_message("usb control read failure\n");
				}
				else 
				{
					int i;
					for(i = 0; i < bytes; i++)
						message("%02x", g_read_buffer[i]);
					message("\n");
				}
			}
#ifndef LFMB_CLIENT
			if(FD_READABLE(usb_bulkin_fd))
			{
				if((ret = read_and_handle_usb()) < 0)
				{
					if(ret == -ESHUTDOWN)
					{
						error_message("probable cable disconnect\n");
						clear_connection();
					}
					else if(transport_reset() < 0)
					{
						error_message("bulkin transport reset failure\n");
						free_read_buffer(); return -1;
					}
				}
			}
#endif //!LFMB_CLIENT
			//should never:
			if(FD_READABLE(usb_bulkout_fd))
			{
				message("bulk out is readable\n");
			}
		}
#ifdef LFMB_CLIENT
		// we always send as client and we always receive, even if it's nothing and an ack
		// FIXME would we ever want to sleep this?
		if(send_to_shell(buffer, bytes) < 0)
		{
			if(transport_reset() < 0)
			{
				error_message("send from shell transport reset error\n");
				free_read_buffer(); return -1;
			}
			//but then what, we just never send this data?
			//maybe this should be fatal... FIXME
		}
		if(read_from_shell() < 0)
		{
			free_read_buffer();
			return -1;
		}
#endif //LFMB_CLIENT
	}

	//what about closing things gracefully?  SIGHUP? FIXME	
	// also configs ffs cleanup! FIXME
	
	free_read_buffer();
	return 0;
}

int allocate_read_buffer(void)
{
	if(!g_read_buffer)
	{
		g_read_buffer = malloc(MAX_BUFFER_SIZE);
		if(!g_read_buffer);
			return errno;
	}
	return 0;
}

void free_read_buffer(void)
{
	if(g_read_buffer)
		free(g_read_buffer);
}
