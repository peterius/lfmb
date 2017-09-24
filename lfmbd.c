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

#define FD_READABLE(x)			(x >= 0 && FD_ISSET(x, &rfd))

int main(int argc, char ** argv)
{
	char * buffer;
	int bytes;
	fd_set rfd, wfd, efd;
	int filedes;
	int n;
	
	term_stdinout_fd = -1;	
	usb_control_fd = -1;
	usb_bulkin_fd = -1;
	usb_bulkout_fd = -1;
	
	buffer = malloc(MAX_BUFFER_SIZE);
	
	if(transport_init() < 0)
		return -1;

	while(1)
	{
		FD_ZERO(&rfd);
		FD_SET(filedes, &rfd);
		FD_ZERO(&efd);
		FD_SET(filedes, &efd);
		n = select(filedes, &rfd, NULL, &efd, NULL);
		if(n == -1)
		{
			if(errno != EINTR)
			{
				error("select error: %d\n", errno);
				if(transport_init() < 0)
					{ free(buffer); return -1; }
			}
		}
		else if(n)
		{
			if(FD_READABLE(term_stdinout_fd))
			{
				bytes = read(term_stdinout_fd, buffer, MAX_BUFFER_SIZE);
				if(bytes < 0)
					error("shell stdinout read error: %d\n", errno);
				else 
				{
					if(send_from_shell(buffer, bytes) < 0)
					{
						if(transport_init() < 0)
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
			if(FD_READABLE(usb_bulkin_fd))
			{
				if(read_and_handle_usb() < 0)
				{
					if(transport_init() < 0)
						{ free(buffer); return -1; }
				}
			}
		}
	}

	//what about closing things gracefully?  SIGHUP? FIXME	
	
	free(buffer);
	
	return 0;
}
