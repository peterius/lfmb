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
#include <stdarg.h>
#include "message.h"

FILE * errorlog = NULL;

void set_errorlog(const char * filename)
{
	//errorlog = fopen(filename, "w");
}

void close_errorlog(void)
{
	if(errorlog)
		fclose(errorlog);
}

//same for now FIXME
int error_message(const char * format, ...)
{
	int ret;
	va_list args;
	va_start(args, format);
	if(errorlog)
		ret = vfprintf(errorlog, format, args);
	else
		ret = vfprintf(stderr, format, args);
	va_end(args);
	return ret;
}

int message(const char * format, ...)
{
	int ret;
	va_list args;
	va_start(args, format);
	if(errorlog)
		ret = vfprintf(errorlog, format, args);
	else
		ret = vfprintf(stderr, format, args);
	va_end(args);
	return ret;
}
