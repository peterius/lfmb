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
#include <errno.h>
#include "file.h"
#include "protocol.h"
#include "message.h"

#define FILE_TRANSFER_CHUNK			(MAX_BUFFER_SIZE - sizeof(struct packethdr))
#define CHECKSUM_IV						0x89A437E9
#define CHECKSUM_PADDING_BYTE			0x5C

#define TMPFILENAME						"/tmp/lfmbtransfer"

/* FIXME FIXME FIXME
 * in addition to the tmp file transfer stuff
 * we need to add some syntax for specifying low level writes and reads
 * like to boot.img, etc., etc., at least for the recovery.img... I
 * guess it's not a big deal right now */

int receive_file(char * filepath);

#ifdef LFMB_CLIENT
int transfer_file_from_server(char * remotefile, char * localfile)
{
	char * tempfilename;
	char * command;
	int ret;

	if(transport_init() < 0)
		{ error_message("Failed to establish usb connection\n"); return -1; }
	
	if(send_get_file(remotefile) < 0)
		return -1;

	tempfilename = calloc(1, sizeof(TMPFILENAME));
	strcpy(tempfilename, TMPFILENAME);
	if(receive_file(TMPFILENAME) < 0)
	{
		command = calloc(1, strlen(tempfilename) + 6);
		sprintf(command, "rm -f %s", tempfilename);
	
		if((ret = system(command)) != 0)
			message("%s failed: %d\n", command, ret);

		free(command); 
		return -1;
	}
	//move the tmp file, possibly overwriting
	
	
	command = calloc(1, strlen(tempfilename) + strlen(localfile) + 6);
	sprintf(command, "mv -f %s %s", tempfilename, localfile);
	
	if((ret = system(command)) != 0)
		message("%s failed: %d\n", command, ret);
	else
		message("Successfully transferred %s\n", remotefile);
	free(command); 
	
	//close connection?
	
	return 0;
}

int transfer_file_to_server(char * localfile, char * remotefile)
{
	if(transport_init() < 0)
		return -1;
	if(send_put_file(remotefile) < 0)
		return -1;
	if(receive_ack() < 0)
		return -1;
	message("starting transfer\n");	
	if(get_file(localfile) < 0)
		return -1;
	message("transfer complete\n");		
	//close connection?
	
	return 0;
}
#else

int server_receives_file(char * filepath)
{
	char * tempfilename;
	char * command;
	int ret;
	
	tempfilename = calloc(1, sizeof(TMPFILENAME));
	strcpy(tempfilename, TMPFILENAME);
	if(receive_file(TMPFILENAME) < 0)
	{
		command = calloc(1, strlen(tempfilename) + 6);
		sprintf(command, "rm -f %s", tempfilename);
	
		if((ret = system(command)) != 0)
			message("%s failed: %d\n", command, ret);

		free(command);
		return -1;
	}
	//move the tmp file, possibly overwriting
	
	command = calloc(1, strlen(tempfilename) + strlen(filepath) + 6);
	sprintf(command, "mv -f %s %s", tempfilename, filepath);
	
	if((ret = system(command)) != 0)
		message("%s failed: %d\n", command, ret);
	message("server successfully received %s\n", filepath);
	free(command); 
	return 0;
}
#endif //LFMB_CLIENT

int get_file(char * filepath)
{
	unsigned int filesize;
	unsigned int bytes;
	unsigned char * buffer;
	uint32_t checksum = CHECKSUM_IV;
	unsigned int i;
	
	FILE * fp = fopen(filepath, "r");
	if(!fp)
		{ error_message("Can't open file %s: err %d\n", filepath, errno); return -errno; }
	
	
	fseek(fp, 0L, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	
	if(send_filedata_to_follow(filesize) < 0)
		return -1;
	if(receive_ack() < 0)
		return -1;
	
	buffer = (char *)malloc(FILE_TRANSFER_CHUNK);
	
	while(filesize > FILE_TRANSFER_CHUNK)
	{
		bytes = fread(buffer, 1, FILE_TRANSFER_CHUNK, fp);
		if(bytes != FILE_TRANSFER_CHUNK)
			{ error_message("fread error %d err %d\n", bytes, errno); return -errno; }
		filesize -= bytes;
		for(i = 0; i < bytes / 4; i++)
			checksum ^= htole32(((uint32_t *)buffer)[i]);
		if(send_filedata(buffer, bytes) < 0)
			return -1;
		if(receive_ack() < 0)
			return -1;
	}
	bytes = fread(buffer, 1, filesize, fp);
	if(bytes != filesize)
		{ error_message("fread last chunk error %d err %d\n", bytes, errno); return -errno; }

	while(bytes % 4 != 0)
	{
		buffer[bytes] = CHECKSUM_PADDING_BYTE;
		bytes++;
	}
	
	for(i = 0; i < bytes / 4; i++)
		checksum ^= htole32(((uint32_t *)buffer)[i]);
	if(send_filedata(buffer, bytes) < 0)					//with the padding
		return -1;
	if(receive_ack() < 0)
		return -1;
	if(send_filechecksum(checksum) < 0)
		return -1;
	if(receive_ack() < 0)
		return -1;
	message("file transferred, received checksum ack\n");
	free(buffer);
		
	fclose(fp);
	return 0;
}

int receive_file(char * filepath)
{
	unsigned int filesize;
	unsigned int total_filesize;
	unsigned int bytes;
	unsigned int bytes_written;
	unsigned int bytes_to_write;
	uint32_t received_checksum;
	uint32_t checksum = CHECKSUM_IV;
	unsigned int i;
	char * buffer;
	
	FILE * fp = fopen(filepath, "w");
	if(!fp)
		{ error_message("Can't open temporary file for writing %s: err %d\n", filepath, errno); return -errno; }
	
	if(receive_filedata_to_follow(&total_filesize) < 0)
		return -1;
	if(!total_filesize)
		return -1;
	if(send_ack() < 0)
		return -1;
	
	filesize = 0;
	while(filesize < total_filesize)
	{
		if(receive_filedata(&buffer, &bytes) < 0)
			return -1;
		if(bytes == 0 || buffer == NULL)				//probably won't even get here if receive_filedata fails...
			return -1;
		if(send_ack() < 0)
			return -1;
		if(filesize + bytes > total_filesize)		//don't write padding
			bytes_to_write = total_filesize - filesize;
		else
			bytes_to_write = bytes;
		bytes_written = fwrite(buffer, 1, bytes_to_write, fp);
		if(bytes_written != bytes_to_write)
			{ error_message("fwrite error %d err %d\n", bytes_written, errno); return -errno; }
		filesize += bytes_to_write;
		for(i = 0; i < bytes / 4; i ++)
			checksum ^= htole32(((uint32_t *)buffer)[i]);
	}
	
	if(receive_filechecksum(&received_checksum) < 0)
		return -1;
	if(checksum != received_checksum)
	{
		error_message("file transfer checksum failed! %08x %08x\n", checksum, received_checksum);
		fclose(fp);
		//caller will delete temporary file
		return -1;
	}
	if(send_ack() < 0)
		return -1;
		
	fclose(fp);
	return 0;
}