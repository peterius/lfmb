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
#include <unistd.h>
#include <errno.h>

#include "protocol.h"
#include "usb_transport.h"
#include "shell.h"
#include "io.h"
#include "file.h"
#include "message.h"
#include "packet.h"

/* FIXME all the reads and writes need to properly return errnos so that we can
 * catch ESHUTDOWN for a cable disconnect and possibly others.  We also need
 * in general better error handling and cleanup FIXME */

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
	s_put = 5,
	s_disconnect_posted = 6
} connection_state;

#ifdef LFMB_CLIENT
int send_connect(void);
int receive_accept(void);
#endif //LFMB_CLIENT

void free_packet_chain(void);

int transport_reset(void)
{
#ifdef LFMB_CLIENT
	/* no reason to, client can be restarted */
	return -1;
#endif //LFMB_CLIENT
	free_packet_chain();
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
		{ usb_cleanup(); return -1; }
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

	if(usb_write(packet, ph->length) < 0)
		return -1;
	return 0;
}

int receive_accept(void)
{
	struct packethdr * ph = (struct packethdr *)g_read_buffer;
	unsigned short bytes_received = sizeof(struct packethdr);
	if(usb_read(&bytes_received, 0) < 0)
		return -1;
	last_size = le16toh(ph->length);
	if(last_size != sizeof(struct packethdr))
		{ error_message("received bad accept message length %d\n", last_size); send_error(0); return -1; }
	if(le16toh(ph->options) != handshake_value + 1)
	{
		error_message("handshake wrong: %04x", le16toh(ph->options));
		send_error(0);
		return -1;
	}

	connection_state = s_connected;
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
	
	if(usb_write(packet, last_size) < 0)
		return -1;

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

	if(usb_write(packet, last_size) < 0)
		return -1;
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

	if(usb_write(packet, last_size) < 0)
		return -1;
	return 0;
}
#else

#endif //LFMB_CLIENT

//server and client can send:
int send_disconnect(void)
{
	if(connection_state == s_no_connection)		//already disconnected
		return 0;
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_disconnect);
	last_size = sizeof(struct packethdr);
	ph->length = htole16(last_size);

	if(usb_write(packet, last_size) < 0)
		return -1;
	connection_state = s_no_connection;
	last_size = 0;

	return 0;
}

int post_disconnect(void)
{
	if(connection_state == s_no_connection)		//already disconnected
		return 0;
	char * packet = calloc(1, sizeof(struct packethdr));
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_disconnect);
	last_size = sizeof(struct packethdr);
	ph->length = htole16(last_size);
	//post for writing when requested
	if(usb_async_write_post(packet, last_size) < 0)
		return -1;
	connection_state = s_disconnect_posted;
	last_size = 0;
	return 0;
}

/* if cable is disconnected, we don't want to do anything too serious.
 * trying to reinitialize previously initialized functionfs unfortunately
 * will cause errors.  we just want to reset the protocol and pretend
 * everything is okay. */
void clear_connection(void)
{
	free_packet_chain();
	
	connection_state = s_no_connection;
	last_size = 0;
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
	if(usb_write(packet, last_size) < 0)
		return -1;
	return 0;
}

/* FIXME why not send it separately and forget the memcpy... 
 * except that won't work with libusb_async_write_cb */
int send_filedata(char * buffer, unsigned int bytes)
{
	char * packet = calloc(1, sizeof(struct packethdr) + bytes);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_filepart);
	ph->length = htole16(sizeof(struct packethdr) + bytes);
	
	memcpy(packet + sizeof(struct packethdr), buffer, bytes);
	
	last_size = sizeof(struct packethdr) + bytes;
	if(usb_write(packet, last_size) < 0)
		return -1;
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
	if(usb_write(packet, last_size) < 0)
		return -1;
	return 0;
}

// ugly but this is where we would get a file error... 
// FIXME and actually inconsistent reads, packethdr should be +4 bigger
// across the board FIXME FIXME FIXME
int receive_filedata_to_follow(unsigned int * total_filesize)
{
	struct packethdr * ph = (struct packethdr *)g_read_buffer;
	unsigned short bytes_received = sizeof(struct packethdr);
	unsigned short b;
	
	if(usb_read(&bytes_received, 0) < 0)
		return -1;
	last_size = le16toh(ph->length);
	if(le16toh(ph->command) != c_filesize)
	{
		if(le16toh(ph->command) == c_error)
		{
			switch(le16toh(ph->options))
			{
				case EPERM:
					error_message("Received error \"operation not permitted\"\n");
					break;
				case ENOENT:
					error_message("Received error \"no such file or directory\"n");
					break;
				case EACCES:
					error_message("Received error \"permission denied\"\n");
					break;
				default:
					error_message("Received error: %d\n",  le16toh(ph->options));
					break;
			}
		}
		else
			error_message("unexpected response %d\n", le16toh(ph->command));
		return -1;
	}
	if(last_size != sizeof(struct packethdr) + 4)
		{ error_message("received bad filesize message length %d\n", last_size); return -1; }
	if(bytes_received < last_size)
	{
		b = bytes_received;
		bytes_received = last_size;		//expected
		if(usb_read(&bytes_received, b) < 0)
			return -1;
	}
	*total_filesize = le32toh(*(uint32_t *)&(g_read_buffer[sizeof(struct packethdr)]));
	return 0;
}

int receive_filedata(char ** buffer, unsigned int * bytes)
{
	struct packethdr * ph = (struct packethdr *)g_read_buffer;
	unsigned short bytes_received = sizeof(struct packethdr);
	unsigned short b;
	if(usb_read(&bytes_received, 0) < 0)
		return -1;
	last_size = le16toh(ph->length);
	*bytes = last_size - sizeof(struct packethdr);
	*buffer = g_read_buffer + sizeof(struct packethdr);
	if(bytes_received < last_size)
	{
		b = bytes_received;
		bytes_received = last_size;		//expected
		if(usb_read(&bytes_received, b) < 0)
			{ *buffer = NULL; *bytes = 0; return -1; }
	}
	*bytes = bytes_received - sizeof(struct packethdr);
	return 0;
}

int receive_filechecksum(uint32_t * received_checksum)
{
	struct packethdr * ph = (struct packethdr *)g_read_buffer;
	unsigned short bytes_received = sizeof(struct packethdr) + 4;
	if(usb_read(&bytes_received, 0) < 0)
		return -1;
	last_size = le16toh(ph->length);
	if(last_size != sizeof(struct packethdr) + 4)
		{ error_message("received bad file checksum message length %d\n", last_size); return -1; }
	*received_checksum = le32toh(*(uint32_t *)&(g_read_buffer[sizeof(struct packethdr)]));
	message("received checksum %04x\n", *received_checksum);
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

	if(usb_write(packet, sizeof(struct packethdr) + 4) < 0)
		return -1;
	last_size = 0;
	return 0;
}

int send_error(unsigned short code)
{
	char * packet = calloc(1, sizeof(struct packethdr) + 4);
	struct packethdr * ph = (struct packethdr *)packet;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_error);
	ph->length = htole16(sizeof(struct packethdr) + 4);
	ph->options = htole16(code);

	*(uint32_t *)&(packet[sizeof(struct packethdr)]) = htole32(last_size);

	error_message("sending error\n");
	if(usb_write(packet, sizeof(struct packethdr) + 4) < 0)
		return -1;

	last_size = 0;
	return 0;
}

int receive_ack(void)
{
	struct packethdr * ph = (struct packethdr *)g_read_buffer;
	unsigned short bytes_received = sizeof(struct packethdr) + 4;
	if(usb_read(&bytes_received, 0) < 0)
		return -1;
	if(le16toh(ph->command) != c_ack)
		{ error_message("did not receive acknowledgement\n"); return -1; }
	if(le16toh(ph->length) != sizeof(struct packethdr) + 4)
		{ error_message("received bad ack length %d\n", le16toh(ph->length)); return -1; }
	if(le32toh(*(uint32_t *)&(g_read_buffer[sizeof(struct packethdr)])) != last_size)
	{ 
		error_message("received ack with last size equal to %d instead of %d\n", le32toh(*(uint32_t *)&(g_read_buffer[sizeof(struct packethdr)])), last_size);
		return -1;
	}
	return 0;
}

#ifndef LFMB_CLIENT
/* many of the receives are just in line and have their own calls outside the handler... */
int handle(char * p, int len)
{
	struct packethdr *ph = (struct packethdr *)p;
	char * filename;
	char * p_reply;
	struct packet_packet * pc;
	int nl;
	int ret;
	
	if(le16toh(ph->length) != len)
	{
		error_message("received length %d does not match header specified lenth %d\n", le16toh(ph->length), len);
		send_error(0);
		goto handle_error;
	}
	
	if(le16toh(ph->command) == (enum protocol_command)c_error)
	{
		error_message("received error command packet\n");
		goto handle_error;
	}

	if(connection_state == s_no_connection && le16toh(ph->command) != (enum protocol_command)c_connect)
	{
		error_message("connection error %04x\n", le16toh(ph->command));
		send_error(0);
		goto handle_error;
	}
	else if(connection_state == s_shell && le16toh(ph->command) != (enum protocol_command)c_toshell && 
					le16toh(ph->command) != (enum protocol_command)c_disconnect)
	{
		error_message("shell protocol command error %04x\n", le16toh(ph->command));
		/* FIXME we need some kind of soft error here, so we can reconnect without writing descriptors maybe?
		 * for instance, shell dies from the host, terminal crashes or something.  device still things there's a shell connection
		 * we come back and try to reconnect, we get protocol command 0 here, but then it tries to send_error and reset
		 * and fails when it could just pretend it was already connected */
		send_error(0);
		goto handle_error;
	}
	else if(connection_state != s_no_connection && le16toh(ph->command) == (enum protocol_command)c_connect)
	{
		error_message("already connected, resetting\n");
		error_message("connection error %04x\n", le16toh(ph->command));
		send_error(0);
		goto handle_error;
	}
	else if(connection_state == s_disconnect_posted && packet_chain)
	{
		ret = usb_ffs_write(packet_chain->data, packet_chain->length);
		free(packet_chain->data);
		pc = packet_chain;
		packet_chain = packet_chain->next;
		free(pc);
		/* We could return -1 here, but I feel like we should know when the socket dies */
		if(transport_reset() < 0)
			return -1;
		connection_state = s_no_connection;
		last_size = 0;
		return 0;
	}
	last_size = le16toh(ph->length);
	switch((enum protocol_command)le16toh(ph->command))
	{
		case c_connect:	
			handshake_value = le16toh(ph->options);
			
			//reply:
			p_reply = malloc(sizeof(struct packethdr));
			memcpy(p_reply, p, sizeof(struct packethdr));
			ph = (struct packethdr *)p_reply;
			ph->command = htole16((unsigned short)c_accept);
			ph->options = htole16(handshake_value + 1);

			if(usb_write(p_reply, last_size) < 0)
				goto handle_error;
			connection_state = s_connected;
			message("Connected\n");
			break;
		case c_accept:
			send_error(0);
			goto handle_error;
			break;
		case c_getfile:
			nl = last_size - sizeof(struct packethdr);
			if(!nl || !(filename = (char *)calloc(1, nl)))
			{
				send_error(0);
				goto handle_error;
			}
			memcpy(filename, p + sizeof(struct packethdr), nl - 1);
			filename[nl - 1] = 0x00;
			connection_state = s_get;
			ret = get_file(filename);
			if(ret < 0)
			{
				send_error((unsigned short)(-ret));
				goto handle_error;
			}
			free(filename);
			connection_state = s_no_connection;		//reset connection
			transport_reset();
			break;
		case c_putfile:
			nl = last_size - sizeof(struct packethdr);
			if(!nl || !(filename = (char *)calloc(1, nl)))
			{
				send_error(0);
				goto handle_error;
			}
			memcpy(filename, p + sizeof(struct packethdr), nl - 1);
			filename[nl - 1] = 0x00;
			if(send_ack() < 0)			//a little asymmetric ?
			{
				send_error(0);
				free(filename);
				goto handle_error;
			}
			connection_state = s_put;
			if(server_receives_file(filename) < 0)
			{
				send_error(0);
				free(filename);
				goto handle_error;
			}
			free(filename);
			connection_state = s_no_connection;			//reset connection
			/* We have to reset the connection per each libusb client access */
			transport_reset();
			/* FIXME handle_error returns -1 and does this too... ... */
			break;
		case c_openshell:
			if(send_ack() < 0)
			{
				send_error(0);
				goto handle_error;
			}
			if(init_terminal() < 0)
			{
				send_error(0);
				goto handle_error;
			}
			connection_state = s_shell;
			break;
		case c_disconnect:
			// FIXME close gracefully
			//how do we kill our terminal? if we get this from client?
			connection_state = s_no_connection;
			//FIXME for now:
			goto handle_error;		//which will reset
			break;
		case c_toshell:
			if(connection_state != s_shell)
			{
				send_error(0);
				goto handle_error;
			}
			if(last_size > sizeof(struct packethdr))
			{
				if(writefd(term_stdinout_fd, p + sizeof(struct packethdr), last_size - sizeof(struct packethdr)) < 0)
				{
					send_error(0);
					goto handle_error;
				}
			}
			if(packet_chain)
			{
				ret = usb_ffs_write(packet_chain->data, packet_chain->length);
				free(packet_chain->data);
				pc = packet_chain;
				packet_chain = packet_chain->next;
				free(pc);
			}
			else
			{
				ret = send_ack();
			}
			if(ret < 0)
			{
				error_message("failed to write packet\n");
				goto handle_error;
			}
			break;
		default:
			error_message("unhandled message code!\n");
			goto handle_error;
			break;
	}
	return 0;
handle_error:
	connection_state = s_no_connection;
	return -1;
}
#endif //!LFMB_CLIENT
/* FIXME FIXME
 * clean up this define LFMB_CLIENT stuff, it's inconsistent, all over the place */
 
#ifdef LFMB_CLIENT
int send_to_shell(char * buffer, unsigned int len)
{
	struct packethdr * ph;
	if(!buffer)		//stdin neither flagged select nor returned bytes
	{
		len = 0;
		buffer = malloc(sizeof(struct packethdr));
	}
	ph = (struct packethdr *)buffer;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_toshell);
	ph->length = htole16(sizeof(struct packethdr) + len);
	last_size = sizeof(struct packethdr) + len;
	if(usb_write(buffer, last_size) < 0)
		return -1;
	return 0;
}
#else

int read_and_handle_usb(void)
{
	unsigned short length;
	unsigned short bytes_received;
	unsigned short b;
	int ret;
	
	bytes_received = sizeof(struct packethdr);
	if((ret = usb_read(&bytes_received, 0)) < 0)
		return ret;
	length = le16toh(((struct packethdr *)g_read_buffer)->length);

	if(!length || le16toh(((struct packethdr *)g_read_buffer)->magic) != PACKETHDR_MAGIC)
		{ error_message("bad packet\n"); return -1; }
	
	if(bytes_received < length)
	{
		b = bytes_received;
		bytes_received = length;
		if((ret = usb_read(&bytes_received, b)) < 0)
			return ret;
	}
	if(handle(g_read_buffer, length) < 0)
		return -1;
	return 0;
}
#endif //LFMB_CLIENT

int send_from_shell(char * buffer, unsigned int len)
{
	struct packethdr * ph = (struct packethdr *)buffer;
	
	ph->magic = htole16(PACKETHDR_MAGIC);
	ph->command = htole16((unsigned short)c_fromshell);
	ph->length = htole16(sizeof(struct packethdr) + len);
	last_size = sizeof(struct packethdr) + len;
	//post for writing when requested
	if(usb_async_write_post(buffer, last_size) < 0)
		return -1;
	return 0;
}

#ifdef LFMB_CLIENT
int read_from_shell(void)
{
	unsigned short length;
	unsigned short bytes_received;
	struct packethdr * ph = (struct packethdr *)g_read_buffer;
	enum protocol_command command;
	
	bytes_received = sizeof(struct packethdr);
	if(usb_read(&bytes_received, 0) < 0)
		return -1;
	length = le16toh(ph->length);
	if(!length || le16toh(ph->magic) != PACKETHDR_MAGIC)
		{ error_message("bad packet\n"); return -1; }
	if(bytes_received < length)
	{
		if(usb_read(&bytes_received, bytes_received) < 0)
			return -1;
	}
	command = (enum protocol_command)le16toh(ph->command);

	if(command == c_disconnect)
	{
		error_message("command disconnect\n");
		return -1;
	}
	else if(command == c_ack) {}
	else if(command == c_error)
	{
		// FIXME should be some standard error receive somewhere see filesize receive stuff
		error_message("Received error: %d\n",  le16toh(ph->options));
		return -1;
	}
	else if(command == c_fromshell)
	{
		if(length > sizeof(struct packethdr))
		{
			if(receive_shell_from_server(g_read_buffer + sizeof(struct packethdr), length - sizeof(struct packethdr)) < 0)
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

void free_packet_chain(void)
{
	struct packet_packet * p;
	if(packet_chain)
	{
		//free the terminal write post chain before resetting
		p = packet_chain;
		while(packet_chain)
		{
			p = packet_chain;
			packet_chain = p->next;
			free(p->data);
			free(p);
		}
		packet_chain = NULL;
	}
}
