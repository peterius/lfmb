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

#include <stdint.h>
 
enum connection_type
{
	ct_shell=0,
	ct_put=1,
	ct_get=2
};

enum protocol_command
{
	c_connect = 0,
	c_accept = 1,
	c_openshell = 2,
	c_disconnect = 3,
	c_getfile = 4,
	c_putfile = 5,
	c_filesize = 6,
	c_filepart = 7,
	c_filechecksum = 8,
	c_ack = 9,
	c_error = 10,
	c_fromshell = 11,
	c_toshell = 12
};

/* what about endian ness ?!?! FIXME */

#define PACKETHDR_MAGIC			0x6886

struct packethdr
{
	unsigned short magic;
	unsigned short command;
	unsigned short options;
	unsigned short length;
} __attribute__((packed));

int transport_reset(void);
int transport_init(void);
#ifdef LFMB_CLIENT
int send_get_file(char * remotefile);
int send_put_file(char * remotefile);
int send_open_shell(void);
#endif //LFMB_CLIENT
int send_disconnect(void);
int send_filedata_to_follow(unsigned int filesize);
int send_filedata(char * buffer, unsigned int bytes);
int send_filechecksum(uint32_t checksum);
int receive_filedata_to_follow(unsigned int * total_filesize);
int receive_filedata(char ** buffer, unsigned int * bytes);
int receive_filechecksum(uint32_t * received_checksum);
int send_ack(void);
int send_error(void);
int receive_ack(void);
#ifdef LFMB_CLIENT
int send_to_shell(char * buffer, int len);
int read_from_shell(void);
#endif //LFMB_CLIENT
#ifndef LFMB_CLIENT
int read_and_handle_usb(void);
#endif //!LFMB_CLIENT
int send_from_shell(char * buffer, int len);
int readfd(int fd, char * buffer, int len);
int writefd(int fd, char * buffer, int len);

