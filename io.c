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

#include <fcntl.h>
#include <errno.h>
#include "io.h"

int high_fd;
int fds[TOTAL_FDS];

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
		message("fd %d set non block\n", fds[i]);
	}
}
