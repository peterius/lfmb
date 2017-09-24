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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "protocol.h"
#include "message.h"
#include "shell.h"
#include "usb_transport.h"
#include "io.h"

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

int main(int argc, char ** argv)
{
	char * buffer;
	int bytes;
	fd_set rfd, wfd, efd;
	int n;
	
	term_stdinout_fd = -1;	
	usb_control_fd = -1;
	usb_bulkin_fd = -1;
	usb_bulkout_fd = -1;
	clear_fds();
	
	buffer = malloc(MAX_BUFFER_SIZE);
	
	if(transport_init() < 0)
		return -1;

	while(1)
	{
		set_set(&rfd);
		set_set(&efd);
		n = select(high_fd, &rfd, NULL, &efd, NULL);
		message("n %d\n", n);
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
					error_message("shell stdinout read error: %d\n", errno);
				else 
				{
					if(send_from_shell(buffer, bytes) < 0)
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
				message("bulkout is readable\n");
				if(read_and_handle_usb() < 0)
				{
					if(transport_reset() < 0)
						{ free(buffer); return -1; }
				}
			}
		}
	}

	//what about closing things gracefully?  SIGHUP? FIXME	
	
	free(buffer);
	
	return 0;
}
