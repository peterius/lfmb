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
#include <signal.h>
#include <stdlib.h>
#include "io.h"
#include "message.h"

void signal_handler(int s)
{
	switch(s)
	{
		case SIGSEGV:
			error_message("Received segmentation fault\n");
			break;
		case SIGHUP:
			error_message("Received hang up\n");
			break;
		case SIGKILL:
			error_message("Received kill\n");
			break;
		default:
			error_message("Received signal %d\n", s);
			break;
	}
	//cleanup FIXME
	exit(0);
}

int main(int argc, char ** argv)
{
	signal(SIGSEGV, &signal_handler);
	signal(SIGHUP, &signal_handler);
	signal(SIGKILL, &signal_handler);

	if(select_loop() < 0)
		return -1;
	return 0;
}
