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
#include <fcntl.h>
#include <errno.h>

#include "usb_ffs.h"
#include "message.h"

#define USB_FFS_CONTROL				"/dev/usb-ffs/lfmb/ep0"
#define USB_FFS_BULKIN				"/dev/usb-ffs/lfmb/ep1"
#define USB_FFS_BULKOUT				"/dev/usb-ffs/lfmb/ep2"

#define LOCAL_IPC_PROTOCOL_TEST

int usb_control_fd, usb_bulkin_fd, usb_bulkout_fd;

void usb_ffs_kick(void);

int usb_init(void)
{
#ifndef LOCAL_IPC_PROTOCOL_TEST
	fill_descriptors_strings();
	
	usb_control_fd = open(USB_FFS_CONTROL, O_RDWR);
	if(usb_control_fd < 0)
		{ error("Can't open usb control err:%d\n", errno); goto usb_init_err; }
		
	if(write(usb_control_fd, &descriptors, sizeof(descriptors)) < 0)
		{ error("Writing descriptors to usb control failed err: %d\n", errno); goto usb_init_err; }
	
	if(write(usb_control_fd, &strings, sizeof(strings)) < 0)
		{ error("Writing strings to usb control failed err: %d\n", errno); goto usb_init_err; }
	usb_bulkin_fd = open(USB_FFS_BULKIN, O_RDWR);
	if(usb_bulkin_fd < 0)
		{ error("Can't open usb bulk in err:%d\n", errno); goto usb_init_err; }
	usb_bulkout_fd = open(USB_FFS_BULKOUT, O_RDWR);
	if(usb_bulkout_fd < 0)
		{ error("Can't open usb bulk out err:%d\n", errno); goto usb_init_err; }
#else
	mknod("lfmb_lipt_in", S_IFIFO | 0666, 0);
	mknod("lfmb_lipt_out", S_IFIFO | 0666, 0);
#ifndef LFMB_CLIENT
	usb_bulkin_fd = open("lfmb_lipt_in", O_RDWR);
#else
	usb_bulkin_fd = open("lfmb_lipt_out", O_RDWR);
#endif //!LFMB_CLIENT
	if(usb_bulkin_fd < 0)
		{ error("Can't open local ipc protocol test bulk in err:%d\n", errno); goto usb_init_err; }
#ifndef LFMB_CLIENT
	usb_bulkout_fd = open("lfmb_lipt_out", O_RDWR);
#else
	usb_bulkout_fd = open("lfmb_lipt_in", O_RDWR);
#endif //!LFMB_CLIENT
	if(usb_bulkout_fd < 0)
		{ error("Can't open local ipc protocol test bulk out err:%d\n", errno); goto usb_init_err; }

#endif //!LOCAL_IPC_PROTOCOL_TEST
	return 0;
usb_init_err:
	if(usb_control_fd > 0)
	{		
		close(usb_control_fd);
		usb_control_fd = -1;
	}
	if(usb_bulkin_fd > 0)
	{		
		close(usb_bulkin_fd);
		usb_bulkin_fd = -1;
	}
	if(usb_bulkout_fd > 0)
	{		
		close(usb_bulkout_fd);
		usb_bulkout_fd = -1;
	}
	return -1;
}

int usb_ffs_write(const void * data, int len)
{
	size_t count = 0;
	int ret;

	while (count < len)
	{
		ret = read(usb_bulkin_fd, data + count, len - count);
		if (ret < 0)
		{
			if (errno != EINTR)
				{ error("usb ffs write failed fd %d length %d count %d\n", usb_bulkin_fd, len, count); return ret; }
			else
				count += ret;
		}
	}

	message("usb ffs write done\n");
	return count;
}

int usb_ffs_read(void * data, int len)
{
	size_t count = 0;
	int ret;

	while (count < len)
	{
		ret = read(usb_bulkout_fd, data + count, len - count);
		if (ret < 0)
		{
			if (errno != EINTR)
				{ error("usb ffs read failed fd %d length %d count %d\n", usb_bulkout_fd, len, count); return ret; }
			else
				count += ret;
		}
	}

	return count;
}

void usb_ffs_kick(void)
{
	int err;
#ifndef LOCAL_IPC_PROTOCOL_TEST
	err = ioctl(usb_bulkin_fd, FUNCTIONFS_CLEAR_HALT);
	if (err < 0)
		error("usb kick source fd %d clear halt failed err %d", usb_bulkin_fd, errno);

	err = ioctl(usb_bulkout_fd, FUNCTIONFS_CLEAR_HALT);
	if (err < 0)
		error("usb kick sink fd %d clear halt failed err %d", usb_bulkout_fd, errno);
#else
	error("reseting io\n");
#endif //!LOCAL_IPC_PROTOCOL_TEST


	close(usb_control_fd);
   usb_control_fd = -1;
	close(usb_bulkin_fd);
	usb_bulkin_fd = -1;
	close(usb_bulkout_fd);
	usb_bulkout_fd = -1;
}

void usb_reset(void)
{
	usb_ffs_kick();
	if(usb_init() < 0)
	{
		error("usb failed to reinitialize\n");
		// probably fatal?
	}
}

