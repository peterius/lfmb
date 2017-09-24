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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "protocol.h"
#include "usb_transport.h"
#include "shell.h"
#include "io.h"

unsigned short handshake_value;
unsigned int last_size;

/* playing fast and loose with the connection state, client
 * doesn't use it all */
enum _connection_state
{
	s_no_connection = 0,
	s_connected = 2,
	s_shell = 3,
	s_get = 4,
	s_put = 5	
} connection_state;

#ifdef LFMB_CLIENT
int send_connect(void);
int receive_accept(void);
#endif //LFMB_CLIENT

int transport_reset(void)
{
	if(usb_bulkin_fd >= 0)
		usb_reset();
	else 
	{
		if(usb_init() < 0)
			return -1;
	}
	return transport_init();
}

int transport_init(void)
{
	last_size = 0;
	if(fds[1] < 0)
	{
		if(usb_init() < 0)
			return -1;
	}	
#ifdef LFMB_CLIENT
	//handshake:
	if(send_connect() < 0)
		return -1;
	if(receive_accept() < 0)
		return -1;
#endif //LFMB_CLIENT
	// if we're server we can't be sending stuff yet 
	// because maybe nothing is connected
	return 0;
}

#ifdef LFMB_CLIENT
int send_connect(void)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_connect);
	handshake_value = rand() % 0xfffe;
	ph->options = htole16(handshake_value);
	ph->length = htole16(sizeof(struct packethdr));

	if(usb_ffs_write(packet, ph->length) < 0)
		{ free(packet); return -1; }

	free(packet);
	return 0;
}

int receive_accept(void)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	if(usb_ffs_read(packet, sizeof(struct packethdr)) < 0)
		{ free(packet); return -1; }
	last_size = le16toh(ph->length);
	if(last_size != sizeof(struct packethdr))
		{ error_message("received bad accept message length %d\n", last_size); free(packet); return -1; }
	if(le16toh(ph->options) != handshake_value + 1)
	{
		error_message("handshake wrong: %04x", le16toh(ph->options));
		send_error();
		free(packet);
		return -1;
	}
	message("received handshake\n");

	connection_state = s_connected;
	free(packet);
	return 0;
}

int send_get_file(char * remotefile)
{
	char * packet = calloc(1, sizeof(struct packethdr) + strlen(remotefile) + 1);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_getfile);
	last_size = sizeof(struct packethdr) + strlen(remotefile) + 1;
	ph->length = htole16(last_size);
	
	memcpy(packet + sizeof(struct packethdr), remotefile, strlen(remotefile) + 1);
	
	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	return 0;
}

int send_put_file(char * remotefile)
{
	char * packet = calloc(1, sizeof(struct packethdr) + strlen(remotefile) + 1);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_putfile);
	last_size = sizeof(struct packethdr) + strlen(remotefile) + 1;
	ph->length = htole16(last_size);
	
	memcpy(packet + sizeof(struct packethdr), remotefile, strlen(remotefile) + 1);
	
	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	return 0;
}

int send_open_shell(void)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_openshell);
	last_size = sizeof(struct packethdr);
	ph->length = htole16(last_size);

	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	return 0;
}
#else

#endif //LFMB_CLIENT

//server and client can send:
int send_disconnect(void)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_disconnect);
	last_size = sizeof(struct packethdr);
	ph->length = htole16(last_size);

	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	
	connection_state = s_no_connection;
	last_size = 0;

	return 0;
}

int send_filedata_to_follow(unsigned int filesize)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_filesize);
	ph->length = htole16(sizeof(struct packethdr) + 4);
	
	*(uint32_t *)&(packet[sizeof(struct packethdr)]) = htole32((uint32_t)filesize);
	last_size = sizeof(struct packethdr) + 4;
	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	return 0;
}

int send_filedata(char * buffer, unsigned int bytes)
{
	char * packet = calloc(1, sizeof(struct packethdr) + bytes);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_filepart);
	ph->length = htole16(sizeof(struct packethdr) + bytes);
	
	memcpy(packet + sizeof(struct packethdr), buffer, bytes);
	
	last_size = sizeof(struct packethdr) + bytes;
	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	return 0;
}

int send_filechecksum(uint32_t checksum)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_connect);
	ph->length = htole16(sizeof(struct packethdr) + 4);
	
	*(uint32_t *)&(packet[sizeof(struct packethdr)]) = htole32(checksum);
	last_size = sizeof(struct packethdr) + 4;
	if(usb_ffs_write(packet, last_size) < 0)
		{ free(packet); return -1; }
	
	free(packet);
	return 0;
}

int receive_filedata_to_follow(unsigned int * total_filesize)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	
	if(usb_ffs_read(packet, sizeof(struct packethdr) + 4) < 0)
		{ free(packet); return -1; }
	last_size = le16toh(ph->length);
	if(last_size != sizeof(struct packethdr) + 4)
		{ error_message("received bad filesize message length %d\n", last_size); free(packet); return -1; }
	*total_filesize = le32toh(*(uint32_t *)&(packet[sizeof(struct packethdr)]));
	free(packet);
	return 0;
}

int receive_filedata(char ** buffer, unsigned int * bytes)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	if(usb_ffs_read(packet, sizeof(struct packethdr)) < 1)
		{ free(packet); return -1; }
	last_size = le16toh(ph->length);
	*bytes = last_size - sizeof(struct packethdr);
	free(packet);
	*buffer = calloc(1, *bytes);
	message("Reading %d bytes into %p\n", *bytes, *buffer);
	if(usb_ffs_read(*buffer, *bytes) < 0)
		{ free(*buffer); *buffer = NULL; *bytes = 0; return -1; }
	return 0;
}

int receive_filechecksum(uint32_t * received_checksum)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	if(usb_ffs_read(packet, sizeof(struct packethdr) + 4) < 0)
		{ free(packet); return -1; }
	last_size = le16toh(ph->length);
	if(last_size != sizeof(struct packethdr) + 4)
		{ error_message("received bad file checksum message length %d\n", last_size); free(packet); return -1; }
	*received_checksum = le32toh(*(uint32_t *)&(packet[sizeof(struct packethdr)]));
	message("received checksum %04x\n", *received_checksum);
	free(packet);
	return 0;
}

int send_ack(void)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_ack);
	ph->length = htole16(sizeof(struct packethdr) + 4);
	
	*(uint32_t *)&(packet[sizeof(struct packethdr)]) = htole32(last_size);

	if(usb_ffs_write(packet, sizeof(struct packethdr) + 4) < 0)
		{ free(packet); return -1; }
	last_size = 0;
	message("sent ack\n");
	free(packet);
	return 0;
}

int send_error(void)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_error);
	ph->length = htole16(sizeof(struct packethdr) + 4);
	
	*(uint32_t *)&(packet[sizeof(struct packethdr)]) = htole32(last_size);

	if(usb_ffs_write(packet, sizeof(struct packethdr) + 4) < 0)
		{ free(packet); return -1; }
	last_size = 0;
	
	free(packet);
	return 0;
}

int receive_ack(void)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	if(usb_ffs_read(packet, sizeof(struct packethdr) + 4) < 0)
		{ free(packet); return -1; }
	if(le16toh(ph->command) != c_ack)
		{ error_message("did not receive acknowledgement\n"); free(packet); return -1; }
	if(le16toh(ph->length) != sizeof(struct packethdr) + 4)
		{ error_message("received bad ack length %d\n", le16toh(ph->length)); free(packet); return -1; }
	if(le32toh(*(uint32_t *)&(packet[sizeof(struct packethdr)])) != last_size)
	{ 
		error_message("received ack with last size equal to %d instead of %d\n", le32toh(*(uint32_t *)&(packet[sizeof(struct packethdr)])), last_size);
		free(packet);
		return -1;
	}
	free(packet);
	return 0;
}

#ifndef LFMB_CLIENT
/* many of the receives are just in line and have their own calls outside the handler... */
int handle(char * p, int len)
{
	struct packethdr *ph = (struct packethdr *)p;
	char * filename;
	int nl;

	if(le16toh(ph->length) != len)
	{
		error_message("received length %d does not match header specified lenth %d\n", le16toh(ph->length), len);
		send_error();
		goto handle_error;
	}

	if(connection_state == s_no_connection && le16toh(ph->command) != (enum protocol_command)c_connect)
	{
		error_message("connection error %04x\n", le16toh(ph->command));
		send_error();
		goto handle_error;
	}
	else if(connection_state == s_shell && le16toh(ph->command) != (enum protocol_command)c_toshell && 
					le16toh(ph->command) != (enum protocol_command)c_disconnect)
	{
		error_message("shell protocol command error %04x\n", le16toh(ph->command));
		send_error();
		goto handle_error;
	}
	last_size = le16toh(ph->length);
	switch((enum protocol_command)le16toh(ph->command))
	{
		case c_connect:			
			handshake_value = le16toh(ph->options);
	
			//reply:
			ph->command = htole16((unsigned short)c_accept);
			ph->options = htole16(handshake_value + 1);
	
			usb_ffs_write(p, last_size);
			connection_state = s_connected;
			break;
		case c_accept:
			send_error();
			goto handle_error;
			break;
		case c_getfile:
			nl = last_size - sizeof(struct packethdr);
			if(!nl || !(filename = (char *)calloc(1, nl)))
			{
				send_error();
				goto handle_error;
			}
			memcpy(filename, p + sizeof(struct packethdr), nl - 1);
			filename[nl - 1] = 0x00;
			connection_state = s_get;
			if(get_file(filename) < 0)
			{
				send_error();
				goto handle_error;
			}
			free(filename);
			break;
		case c_putfile:
			nl = last_size - sizeof(struct packethdr);
			if(!nl || !(filename = (char *)calloc(1, nl)))
			{
				send_error();
				goto handle_error;
			}
			memcpy(filename, p + sizeof(struct packethdr), nl - 1);
			filename[nl - 1] = 0x00;
			if(send_ack() < 0)			//a little asymmetric ?
			{
				send_error();
				free(filename);
				goto handle_error;
			}
			connection_state = s_put;
			if(server_receives_file(filename) < 0)
			{
				send_error();
				free(filename);
				goto handle_error;
			}
			free(filename);
			break;
		case c_openshell:
			if(send_ack() < 0)
			{
				send_error();
				goto handle_error;
			}
			if(init_terminal() < 0)
			{
				send_error();
				goto handle_error;
			}
			connection_state = s_shell;
			break;
		case c_disconnect:
			// FIXME close gracefully
			//how do we kill our terminal? if we get this from client?
			connection_state = s_no_connection;
			break;
		case c_toshell:
			if(connection_state != s_shell)
			{
				send_error();
				goto handle_error;
			}
			if(writefd(term_stdinout_fd, p + sizeof(struct packethdr), last_size - sizeof(struct packethdr)) < 0)
			{
				send_error();
				goto handle_error;
			}
			if(send_ack() < 0)
			{
				send_error();
				goto handle_error;
			}
			break;
		default:
		
			break;
	}	
	free(p);
	return 0;
handle_error:
	connection_state = s_no_connection;
	free(p);
	return -1;
}
#endif //!LFMB_CLIENT
/* FIXME FIXME
 * clean up this define LFMB_CLIENT stuff, it's inconsistent, all over the place */
 
#ifdef LFMB_CLIENT
int send_to_shell(char * buffer, int len)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_toshell);
	ph->length = htole16(sizeof(struct packethdr) + len);
	if(usb_ffs_write(packet, sizeof(struct packethdr)) < 0)
		{ free(packet); return -1; }
	free(packet);
	if(usb_ffs_write(buffer, len) < 0)
		return -1;
	last_size = sizeof(struct packethdr) + len;
	
	if(receive_ack() < 0)
		return -1;
	return 0;
}
#else

int read_and_handle_usb(void)
{
	unsigned short length;
	char * packet = malloc(sizeof(struct packethdr));
	if(usb_ffs_read(packet, sizeof(struct packethdr)) < 0)
		return -1;
	length = le16toh(((struct packethdr *)packet)->length);
	if(!length || le16toh(((struct packethdr *)packet)->magic) != PACKETHDR_MAGIC)
		{ error_message("bad packet\n"); free(packet); return -1; }
	packet = realloc(packet, length);
	if(!packet)
		{ error_message("packet reallocation failed\n"); return -1; }
	if(usb_ffs_read(packet + sizeof(struct packethdr), length - sizeof(struct packethdr)) < 0)
		return -1;
	if(handle(packet, length) < 0)
		return -1;
	return 0;
}
#endif //LFMB_CLIENT

int send_from_shell(char * buffer, int len)
{
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_fromshell);
	ph->length = htole16(sizeof(struct packethdr) + len);
	if(usb_ffs_write(packet, sizeof(struct packethdr)) < 0)
		{ free(packet); return -1; }
	free(packet);
	if(usb_ffs_write(buffer, len) < 0)
		return -1;
	last_size = sizeof(struct packethdr) + len;
	return 0;
}

#ifdef LFMB_CLIENT
int read_from_shell(void)
{
	unsigned short length;
	enum protocol_command command;
	char * packet = malloc(sizeof(struct packethdr));
	if(usb_ffs_read(packet, sizeof(struct packethdr)) < 0)
		return -1;
	length = le16toh(((struct packethdr *)packet)->length);
	if(!length || le16toh(((struct packethdr *)packet)->magic) != PACKETHDR_MAGIC)
		{ error_message("bad packet\n"); free(packet); return -1; }
	command = (enum protocol_command)le16toh(((struct packethdr *)packet)->command);
	free(packet);	
	if(command == c_disconnect)
	{
		fds[2] = -1;			//close the bulkout to signal the disconnect to caller
		return -1;
	}
	else if(command == c_fromshell)
	{
		length -= sizeof(struct packethdr);
		packet = malloc(length);
		if(usb_ffs_read(packet, length) < 0)
			return -1;
		if(receive_shell_from_server(packet, length) < 0)
		{
			return -1;
		}
	}
	else
		{ error_message("bad shell command %04x\n", command); return -1; }
	return 0;
}
#endif //LFMB_CLIENT

int readfd(int fd, char * buffer, int len)
{
	int ret;
	int _len = len;
	char * p = buffer;
	
	while(len > 0)
	{
		ret = read(fd, p, len);
		if (ret > 0) 
		{
			len -= ret;
			p += ret;
		} 
		else if (ret == -1)
			{ error_message("read error %d err %d\n", fd, errno); return -1; }
		else
      	{ message("read disconnected %d err %d\n", fd, errno); errno = 0; return 0; }
	}
	return 0;
}

int writefd(int fd, char * buffer, int len)
{
	int ret;
	int _len = len;
	char * p = buffer;
	
	while(len > 0)
	{
		ret = write(fd, p, len);
		if (ret > 0) 
		{
			len -= ret;
			p += ret;
		} 
		else if (ret == -1)
		{
			if(errno == EAGAIN)
			{
				usleep(1000);
				continue;
			}
			else if(errno == EPIPE)
				{ message("write disconnected %d err %d\n", fd); errno = 0; return 0; }
			else
				{ error_message("write error %d err %d\n", fd, errno); return -1; }
		}
		else
      	{ message("write disconnected %d err %d\n", fd, errno); errno = 0; return 0; }
	}
	return 0;
}



