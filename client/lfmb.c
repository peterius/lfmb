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

#include "../file.h"
#include "../shell.h"
#include "../io.h"

char * localfile;
char * remotefile;

/* FIXME FIXME sigint  for shell send_close_shell 
 * and what about for the files, to send a close ?!? */

int main(int argc, char ** argv)
{
	char * p;
	
	if(argc < 2)
	{
help:
		printf("./lfmb [shell/get/put] [file] [file]\n");
		printf("\tget [remote] ([local])\n");
		printf("\tput [local] [remote]\n");
		return 0;
	}
	
	/* FIXME FIXME FIXME
	 * we need a special syntax, "?:boot.img" or something
	 * to look up special low level read and writes and
	 * perform them so we don't have to rely on fastboot */
	
	localfile = NULL;
	remotefile = NULL;
	clear_fds();
	if(strcmp(argv[1], "shell") == 0)
	{
		if(argc > 2)
		{
			printf("shell command has no arguments\n");
			goto help;
		}
		if(open_shell() < 0)
			return -1;
	}
	else if(strcmp(argv[1], "get") == 0)
	{
		if(argc < 3 || argc > 4)
		{
			printf("get command requires at least remote file path [remote] [local]\n");
			goto help;
		}
		remotefile = (char *)calloc(1, strlen(argv[2]) + 1);
		strcpy(remotefile, argv[2]);
		if(argc == 3)
		{
			//current directory with filename:
			p = remotefile + strlen(remotefile) - 1;
			while(p > remotefile && *p != '/')
				p--;
			localfile = (char *)calloc(1, strlen(p) + 1);
			strcpy(localfile, p);
		}
		else
		{
			localfile = (char *)calloc(1, strlen(argv[3]) + 1);
			strcpy(localfile, argv[3]);
		}
		
		transfer_file_from_server(remotefile, localfile);
	}
	else if(strcmp(argv[1], "put") == 0)
	{
		if(argc != 4)
		{
			printf("put command requires local and remote file path [local] [remote]\n");
			goto help;
		}
		localfile = (char *)calloc(1, strlen(argv[2]) + 1);
		strcpy(localfile, argv[2]);
		remotefile = (char *)calloc(1, strlen(argv[3]) + 1);
		strcpy(remotefile, argv[3]);
		
		transfer_file_to_server(localfile, remotefile);
	}
	
	if(localfile)
		free(localfile);
	if(remotefile)
		free(remotefile);
	return 0;
}