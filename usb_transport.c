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

/* ***
 * HAS_LIBUSB code largely modified from Heimdall 
 * Copyright (c) 2010-2017 Benjamin Dobell, Glass Echidna
 * *** */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>		//for system()
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#ifdef LFMB_CLIENT
#define HAS_LIBUSB
#endif //LFMB_CLIENT
#ifdef HAS_LIBUSB
#include <libusb.h>
#include <poll.h>
#endif //HAS_LIBUSB
#include "usb_transport.h"
#include "usb_ffs.h"
#include "message.h"
#include "protocol.h"			//just for MAX_BUFFER_SIZE
#include "io.h"
#include "packet.h"

#define USB_FFS_CONTROL				"/dev/usb-ffs/lfmb/ep0"
#define USB_FFS_BULKIN				"/dev/usb-ffs/lfmb/ep1"
#define USB_FFS_BULKOUT				"/dev/usb-ffs/lfmb/ep2"

int usb_ffs;
#ifdef HAS_LIBUSB
/* All these init values are only going to work for the client... FIXME */
int interfaceIndex = -1;
int usb_libusb_detacheddriver = 0;
struct libusb_context * libusbContext = NULL;
struct libusb_device_handle * ourDeviceHandle = NULL;
struct libusb_device * ourDevice = NULL;
int libusb_inEndpoint;
int libusb_outEndpoint;
const struct libusb_pollfd ** libusb_pollfd_list = NULL;
#endif //HAS_LIBUSB

//#define LOCAL_IPC_PROTOCOL_TEST

int usb_control_fd, usb_bulkin_fd, usb_bulkout_fd;

#ifdef HAS_LIBUSB
int usb_libusb_init(void);
int find_usb_device(void);
int claim_interface(void);
void usb_libusb_cleanup(void);
int libusb_async_write(void * data, int len);
void libusb_async_write_cb(struct libusb_transfer * transfer);
#endif //HAS_LIBUSB
int local_ipc_protocol_test(void);
int usb_ffs_init(void);
int usb_ffs_read(unsigned short * len, unsigned short previously_read);
void usb_ffs_kick(void);

int usb_init(void)
{
#ifdef LOCAL_IPC_PROTOCOL_TEST
	if(local_ipc_protocol_test() < 0)
		goto usb_init_err;
	usb_ffs = 1;		//for the read/write with fd
#else
	if(access(USB_FFS_CONTROL, F_OK) == 0)
	{
		if(usb_ffs_init() < 0)
			goto usb_init_err;
		usb_ffs = 1;
	}
	else
	{
#ifdef HAS_LIBUSB
		if(usb_libusb_init() < 0)
			goto usb_init_err;
#else
		goto usb_init_err;
#endif //HAS_LIBUSB
		usb_ffs = 0;
	}
#endif //LOCAL_IP_PROTOCOL_TEST
	fds[0] = usb_control_fd;
	fds[1] = usb_bulkin_fd;
	fds[2] = usb_bulkout_fd;
	set_high_fd();
#ifndef LFMB_CLIENT
	/* This doesn't seem to do anything, server side read and write, particularly write can still hang
	 * presumably something to do with the functionfs driver... sigh... but if we don't set non blocking
	 * select won't work so... */
	set_non_blocking();
#endif //LFMB_CLIENT
	return 0;
	
usb_init_err:
	if(usb_control_fd >= 0)
	{		
		close(usb_control_fd);
		usb_control_fd = -1;
	}
	if(usb_bulkin_fd >= 0)
	{		
		close(usb_bulkin_fd);
		usb_bulkin_fd = -1;
	}
	if(usb_bulkout_fd >= 0)
	{		
		close(usb_bulkout_fd);
		usb_bulkout_fd = -1;
	}
	fds[0] = usb_control_fd;
	fds[1] = usb_bulkin_fd;
	fds[2] = usb_bulkout_fd;
	set_high_fd();
	return -1;
}

#ifdef HAS_LIBUSB
#define USBLOGLEVEL				0

int usb_libusb_init(void)
{
#ifdef LFMB_CLIENT
	int result;
	
	result = libusb_init(&libusbContext);
	if (result != LIBUSB_SUCCESS)
	{
		error_message("Failed to initialise libusb. libusb error: %d\n", result);
		return -1;
	}

	// Setup libusb log level.
	switch (USBLOGLEVEL)
	{
		case 0:
			libusb_set_debug(libusbContext, LIBUSB_LOG_LEVEL_NONE);
			break;

		case 1:
			libusb_set_debug(libusbContext, LIBUSB_LOG_LEVEL_ERROR);
			break;

		case 2:
			libusb_set_debug(libusbContext, LIBUSB_LOG_LEVEL_WARNING);
			break;

		case 3:
			libusb_set_debug(libusbContext, LIBUSB_LOG_LEVEL_INFO);
			break;

		case 4:
			libusb_set_debug(libusbContext, LIBUSB_LOG_LEVEL_DEBUG);
			break;
	}
	
	if(find_usb_device() < 0)
		return -1;

	result = libusb_open(ourDevice, &ourDeviceHandle);
	if (result != LIBUSB_SUCCESS)
	{
		error_message("Failed to access device. libusb error: %d\n", result);
		return -1;
	}

	if(claim_interface() < 0)
	{
		usb_libusb_cleanup();
		return -1;
	}

	libusb_pollfd_list = libusb_get_pollfds(libusbContext);
	if(!libusb_pollfd_list)
	{
		error_message("Failed to get libusb poll fds\n");
		usb_libusb_cleanup();
		return -1;
	}

	int i = 0;
	usb_bulkin_fd = -1;
	usb_bulkout_fd = -1;
	while(libusb_pollfd_list[i])
	{
		if(libusb_pollfd_list[i]->events & POLLIN)
		{
			message("poll in fd: %d\n",libusb_pollfd_list[i]->fd);
			///eenie meenie minie moe
			if(usb_bulkin_fd == -2)
				usb_bulkin_fd = libusb_pollfd_list[i]->fd;
			else
				usb_bulkin_fd--;
			/* FIXME no, no, but seriously, there's no way to associate fd with endpoints... just
			 * this poll thing... ... */
		}
		else if(libusb_pollfd_list[i]->events & POLLOUT)
			message("poll out fd: %d\n",libusb_pollfd_list[i]->fd);
		else
			message("unknown fd: %d\n",libusb_pollfd_list[i]->fd);
		i++;
	}
	return 0;
#else
	return -1;
#endif //LFMB_CLIENT
}

int find_usb_device(void)
{
	int result;
	struct libusb_device_descriptor descriptor;
	int deviceIndex;
	struct libusb_device **devices;
	int deviceCount = libusb_get_device_list(libusbContext, &devices);

	for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
	{
		libusb_get_device_descriptor(devices[deviceIndex], &descriptor);

		message("vendor %04x product %04x\n", descriptor.idVendor, descriptor.idProduct);
		//message("class %02x subclass %02x protocol %02x\n", descriptor.bInterfaceClass, descriptor.bInterfaceSubClass, descriptor.bInterfaceProtocol);
		message("class %02x subclass %02x protocol %02x\n", descriptor.bDeviceClass, descriptor.bDeviceSubClass, descriptor.bDeviceProtocol);

		if(descriptor.idVendor == 0x1d6b && descriptor.idProduct == 0x0161)
		{
			/* FIXME surely there are other devices ?!?  Maybe I should check each for the protocol and then just send something */
			ourDevice = devices[deviceIndex];
			libusb_ref_device(ourDevice);
			libusb_free_device_list(devices, deviceCount);			
			return 0;
		}
	}
	
	libusb_free_device_list(devices, deviceCount);
	error_message("Couldn't find device\n");
	return -1;
}

int claim_interface(void)
{
	int result;	
	struct libusb_config_descriptor *configDescriptor;
	const struct libusb_endpoint_descriptor *endpoint;
	int i, j, k;
	int verbose = 1;
	int altSettingIndex;
	struct libusb_device_descriptor descriptor;
	unsigned char stringBuffer[128];

	result = libusb_get_config_descriptor(ourDevice, 0, &configDescriptor);
	if (result != LIBUSB_SUCCESS || !configDescriptor)
	{
		error_message("Failed to retrieve config descriptor\n");
		return -1;
	}

	interfaceIndex = -1;
	altSettingIndex = -1;

	libusb_get_device_descriptor(ourDevice, &descriptor);
	if (libusb_get_string_descriptor_ascii(ourDeviceHandle, descriptor.iManufacturer,
			stringBuffer, 128) >= 0)
	{
		message("Manufacturer: \"%s\"\n", stringBuffer);
	}

	if (libusb_get_string_descriptor_ascii(ourDeviceHandle, descriptor.iProduct,
			stringBuffer, 128) >= 0)
	{
		message("Product: \"%s\"\n", stringBuffer);
	}

	if (libusb_get_string_descriptor_ascii(ourDeviceHandle, descriptor.iSerialNumber,
			stringBuffer, 128) >= 0)
	{
		message("Serial No: \"%s\"\n", stringBuffer);
	}
	//says Google Android... 
	
	for (i = 0; i < configDescriptor->bNumInterfaces; i++)
	{
		for (j = 0 ; j < configDescriptor->interface[i].num_altsetting; j++)
		{
			if (verbose)
			{
				message("\ninterface[%d].altsetting[%d]: num endpoints = %d\n",
					i, j, configDescriptor->interface[i].altsetting[j].bNumEndpoints);
				message("   Class.SubClass.Protocol: %02X.%02X.%02X\n",
					configDescriptor->interface[i].altsetting[j].bInterfaceClass,
					configDescriptor->interface[i].altsetting[j].bInterfaceSubClass,
					configDescriptor->interface[i].altsetting[j].bInterfaceProtocol);
			}

			int inEndpointAddress = -1;
			int outEndpointAddress = -1;

			for (k = 0; k < configDescriptor->interface[i].altsetting[j].bNumEndpoints; k++)
			{
				endpoint = &configDescriptor->interface[i].altsetting[j].endpoint[k];

				if (verbose)
				{
					message("       endpoint[%d].address: %02X\n", k, endpoint->bEndpointAddress);
					message("           max packet size: %04X\n", endpoint->wMaxPacketSize);
					message("          polling interval: %02X\n", endpoint->bInterval);
				}

				if((endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) && (endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_BULK))
					inEndpointAddress = endpoint->bEndpointAddress;
				else if(endpoint->bEndpointAddress&& (endpoint->bmAttributes & LIBUSB_TRANSFER_TYPE_BULK))
					outEndpointAddress = endpoint->bEndpointAddress;
			}

			if (interfaceIndex < 0
				&& configDescriptor->interface[i].altsetting[j].bNumEndpoints >= 2	/* 2 or 3? */
				&& configDescriptor->interface[i].altsetting[j].bInterfaceClass == VENDOR_SPECIFIC_CLASS
				&& (configDescriptor->interface[i].altsetting[j].bInterfaceSubClass == LFMB_SUBCLASS )
				&& inEndpointAddress != -1
				&& outEndpointAddress != -1)
			{
				interfaceIndex = i;
				altSettingIndex = j;
				libusb_inEndpoint = inEndpointAddress;
				libusb_outEndpoint = outEndpointAddress;
				message("inEndpoint %08x outEndpoint %08x\n", libusb_inEndpoint, libusb_outEndpoint);
			}
		}
	}

	libusb_free_config_descriptor(configDescriptor);

	if (interfaceIndex < 0)
	{
		error_message("Failed to find correct interface configuration\n");
		return -1;
	}
	
	result = libusb_claim_interface(ourDeviceHandle, interfaceIndex);

//#ifdef OS_LINUX

	if (result != LIBUSB_SUCCESS)
	{
		usb_libusb_detacheddriver = 1;
		message("Attempt failed. Detaching driver...\n");
		libusb_detach_kernel_driver(ourDeviceHandle, interfaceIndex);
		message("Claiming interface again...\n");
		result = libusb_claim_interface(ourDeviceHandle, interfaceIndex);
	}

//#endif

	if (result != LIBUSB_SUCCESS)
	{
		error_message("Claiming interface failed!\n");
		return -1;
	}
	
	result = libusb_set_interface_alt_setting(ourDeviceHandle, interfaceIndex, altSettingIndex);
	if (result != LIBUSB_SUCCESS)
	{
		error_message("Setting up interface failed!\n");
		return -1;
	}
	
	return 0;
}

void usb_libusb_cleanup(void)
{
	/* undefined reference:
	if(libusb_pollfd_list)
		libusb_free_pollfds(libusb_pollfd_list);
	*/
	if(interfaceIndex != -1)
	{
		libusb_release_interface(ourDeviceHandle, interfaceIndex);
//#ifdef OS_LINUX
		if (usb_libusb_detacheddriver)
		{
			message("Re-attaching kernel driver...\n");
			libusb_attach_kernel_driver(ourDeviceHandle, interfaceIndex);
		}
//#endif
	}

	if(ourDeviceHandle)
		libusb_close(ourDeviceHandle);

	if(ourDevice)
		libusb_unref_device(ourDevice);

	if(libusbContext)
		libusb_exit(libusbContext);
}
#endif //HAS_LIBUSB

int local_ipc_protocol_test(void)
{
	mknod("/tmp/lfmb_lipt_in", S_IFIFO | 0666, 0);
	mknod("/tmp/lfmb_lipt_out", S_IFIFO | 0666, 0);
#ifndef LFMB_CLIENT
	usb_bulkin_fd = open("/tmp/lfmb_lipt_in", O_RDWR);
#else
	usb_bulkin_fd = open("/tmp/lfmb_lipt_out", O_RDWR);
#endif //!LFMB_CLIENT
	if(usb_bulkin_fd < 0)
		{ error_message("Can't open local ipc protocol test bulk in err:%d\n", errno); return -1; }
#ifndef LFMB_CLIENT
	usb_bulkout_fd = open("/tmp/lfmb_lipt_out", O_RDWR);
#else
	usb_bulkout_fd = open("/tmp/lfmb_lipt_in", O_RDWR);
#endif //!LFMB_CLIENT
	if(usb_bulkout_fd < 0)
		{ error_message("Can't open local ipc protocol test bulk out err:%d\n", errno); return -1; }
	return 0;
}

int usb_ffs_init(void)
{
	static int not_first_init = 0;
	fill_descriptors_strings();
	
	usb_control_fd = open(USB_FFS_CONTROL, O_RDWR);
	if(usb_control_fd < 0)
		{ error_message("Can't open usb control err:%d\n", errno); return -1; }
		
	if(write(usb_control_fd, &descriptors, sizeof(descriptors)) < 0)
		{ error_message("Writing descriptors to usb control failed err: %d\n", errno); return -1; }
	
	if(write(usb_control_fd, &strings, sizeof(strings)) < 0)
		{ error_message("Writing strings to usb control failed err: %d\n", errno); return -1; }
	usb_bulkin_fd = open(USB_FFS_BULKIN, O_RDONLY);
	if(usb_bulkin_fd < 0)
		{ error_message("Can't open usb bulk in err:%d\n", errno); return -1; }
	usb_bulkout_fd = open(USB_FFS_BULKOUT, O_WRONLY);
	if(usb_bulkout_fd < 0)
		{ error_message("Can't open usb bulk out err:%d\n", errno); return -1; }
	
	//assumed to be created by setup by init:
	system("ls /sys/class/udc > /sys/kernel/config/usb_gadget/g1/UDC");
	message("usb_ffs_init successful\n");
	if(not_first_init)
		sleep(1);
	not_first_init = 1;
	return 0;
}

int usb_ffs_write(void * data, unsigned int len)
{
	size_t count = 0;
	int ret;

	message("usb_ffs_write called with %p %d\n", data, len);
	while (count < len)
	{
		ret = write(usb_bulkout_fd, data + count, len - count);
		if (ret < 0)
		{
			if (errno != EINTR)
				{ error_message("usb ffs write failed fd %d length %d count %d\n", usb_bulkin_fd, len, count); return -errno; }
		}
		else
			count += ret;
		message("usb_ffs_write count %d len %d\n", count, len);
	}

	return count;
}

int usb_async_write_post(void * data, unsigned int len)
{
	struct packet_packet * p, * post;
	
	if(!packet_chain)
	{
		packet_chain = malloc(sizeof(struct packet_packet));
		if(!packet_chain)
			{ error_message("failed to allocate packet chain!\n"); return -1; }
		post = packet_chain;
	}
	else
	{
		p = packet_chain;
		while(p->next)
			p = p->next;
		post = malloc(sizeof(struct packet_packet));
		if(!post)
			{ error_message("failed to allocate packet chain!\n"); return -1; }
		p->next = post;
	}
	post->data = data;
	post->length = len;
	post->next = NULL;
	return 0;
}

int usb_ffs_read(unsigned short * len, unsigned short previously_read)
{
	int ret;

	message("usb_ffs_read called %d %d\n", *len, previously_read);
	while (previously_read < *len)
	{
		ret = read(usb_bulkin_fd, g_read_buffer + previously_read, *len - previously_read);
		if (ret < 0)
		{
			if (errno != EINTR && errno != EWOULDBLOCK)
				{ error_message("usb ffs read failed fd %d length %d count %d -(%d)\n", usb_bulkout_fd, *len, previously_read, errno); return -errno; }
		}
		else
			previously_read += ret;
		//message("usb_ffs_read: count %d len %d\n", previously_read, *len);			//a lot of these
	}
	*len = previously_read;
	return 0;
}

/*  ENODEV*/
void usb_ffs_kick(void)
{
	int err;
#ifndef LOCAL_IPC_PROTOCOL_TEST
	err = ioctl(usb_bulkin_fd, FUNCTIONFS_CLEAR_HALT);
	if (err < 0)
		error_message("usb kick source fd %d clear halt failed err %d\n", usb_bulkin_fd, errno);

	err = ioctl(usb_bulkout_fd, FUNCTIONFS_CLEAR_HALT);
	if (err < 0)
		error_message("usb kick sink fd %d clear halt failed err %d\n", usb_bulkout_fd, errno);
#else
	error_message("reseting io\n");
#endif //!LOCAL_IPC_PROTOCOL_TEST

	error_message("usb_ffs_kick\n");
	// FIXME are we supposed to close control also?  barely matters but, maybe
	// there's a more graceful kind of reset... 
	if(usb_control_fd >= 0)
		close(usb_control_fd);
   	usb_control_fd = -1;
	if(usb_bulkin_fd >= 0)
		close(usb_bulkin_fd);
	usb_bulkin_fd = -1;
	if(usb_bulkout_fd >= 0)
		close(usb_bulkout_fd);
	usb_bulkout_fd = -1;
	fds[0] = usb_control_fd;
	fds[1] = usb_bulkin_fd;
	fds[2] = usb_bulkout_fd;
	set_high_fd();
	error_message("usb_ffs_kick returns...\n");
}

/* data passed here will be freed by a callback or a completion... */
int usb_write(void * data, unsigned int len)
{
	int result;
#ifdef HAS_LIBUSB
	int dataTransferred;
	
#endif //HAS_LIBUSB
	if(usb_ffs)
	{
		result = usb_ffs_write(data, len);
		free(data);
		return result;
	}
	else
	{
#ifdef HAS_LIBUSB
		/*if(libusb_async_write(data, len) < 0)
			return -1;*/
		result = libusb_bulk_transfer(ourDeviceHandle, libusb_outEndpoint, (void *)data, len, &dataTransferred, 1000);
		if (result != LIBUSB_SUCCESS)
		{
			message("libusb bulk transfer write failed: %d\n", result);
			return -1;
		}
		if(dataTransferred != len)
		{
			message("libusb bulk transfer write did not complete transfer, only %d out of %d\n", dataTransferred, len);
		}
#else
		return -1;
#endif //HAS_LIBUSB
	}
	return 0;
}

#ifdef HAS_LIBUSB
int libusb_async_write(void * data, int len)
{
	struct libusb_transfer * lut;
	
	lut = libusb_alloc_transfer(0);
	if(!lut)
		{ error_message("libusb_alloc_transfer failed\n"); free(data); return -1; }
	lut->length = len;
	lut->buffer = data;
	libusb_fill_bulk_transfer(lut, ourDeviceHandle, libusb_outEndpoint, data, len, &libusb_async_write_cb, lut, 1000);
	if(libusb_submit_transfer(lut) != 0)
		return -1;
	return 0;
}

void libusb_async_write_cb(struct libusb_transfer * transfer)
{
	if(transfer->status != LIBUSB_TRANSFER_COMPLETED)
	{
		error_message("libusb_submit_transfer error\n");
	}
	//libusb frees the data on free transfer
	libusb_free_transfer(transfer);
}
#endif //HAS_LIBUSB

int usb_read(unsigned short * len, unsigned short previously_read)
{
#ifdef HAS_LIBUSB
	int dataTransferred;
	int result;
	static char * datap = NULL;
#endif //HAS_LIBUSB
	if(usb_ffs)
		return usb_ffs_read(len, previously_read);
	else
	{
#ifdef HAS_LIBUSB
		*len = 0;
		result = libusb_bulk_transfer(ourDeviceHandle, libusb_inEndpoint, g_read_buffer + previously_read, MAX_BUFFER_SIZE - previously_read, &dataTransferred, 1000);
		if (result != LIBUSB_SUCCESS)
		{
			message("libusb bulk transfer read failed: %d\n", result);
			return -1;
		}
		*len = previously_read + dataTransferred;
#else
		return -1;
#endif //HAS_LIBUSB
	}
	return 0;
}

void usb_kick(void)
{
	if(usb_ffs)
		usb_ffs_kick();
	else
		return;
}

void usb_reset(void)
{
	usb_kick();
	if(usb_init() < 0)
	{
		error_message("usb failed to reinitialize\n");
		sleep(30);
		// probably fatal?
		//FIXME or just a disconnect and we need to wait for a connect...
	}
	error_message("usb just reset\n");
}

//I don't know... I should use this everywhere and properly clean things up... 
//FIXME FIXME FIXME at least call it from the client
void usb_cleanup(void)
{
#ifdef HAS_LIBUSB
	if(!usb_ffs)
		usb_libusb_cleanup();
#endif //HAS_LIBUSB
}

