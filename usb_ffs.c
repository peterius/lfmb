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

#include <endian.h>
#include <string.h>
#include "usb_ffs.h"

#define MAX_PACKET_SIZE_FS			64
#define MAX_PACKET_SIZE_HS			512

struct usb_ffs_descriptors descriptors;
struct usb_ffs_strings strings;

void fill_descriptors_strings(void)
{
	descriptors.header.magic = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC);
	descriptors.header.length = htole32(sizeof(descriptors));
	descriptors.header.fs_count = 3;
	descriptors.header.hs_count = 3;
	descriptors.fs_descs.intf.bLength = sizeof(descriptors.fs_descs.intf);
   descriptors.fs_descs.intf.bDescriptorType = USB_DT_INTERFACE;
	descriptors.fs_descs.intf.bInterfaceNumber = 0;
	descriptors.fs_descs.intf.bNumEndpoints = 2;
	descriptors.fs_descs.intf.bInterfaceClass = VENDOR_SPECIFIC_CLASS;
	descriptors.fs_descs.intf.bInterfaceSubClass = ADB_SUBCLASS;
	descriptors.fs_descs.intf.bInterfaceProtocol = ADB_PROTOCOL;
	descriptors.fs_descs.intf.iInterface = 1; /* first string from the provided table */
	descriptors.fs_descs.source.bLength = sizeof(descriptors.fs_descs.source);
	descriptors.fs_descs.source.bDescriptorType = USB_DT_ENDPOINT;
	descriptors.fs_descs.source.bEndpointAddress = 1 | USB_DIR_OUT;
	descriptors.fs_descs.source.bmAttributes = USB_ENDPOINT_XFER_BULK;
	descriptors.fs_descs.source.wMaxPacketSize = MAX_PACKET_SIZE_FS;
	descriptors.fs_descs.sink.bLength = sizeof(descriptors.fs_descs.sink);
	descriptors.fs_descs.sink.bDescriptorType = USB_DT_ENDPOINT;
	descriptors.fs_descs.sink.bEndpointAddress = 2 | USB_DIR_IN;
	descriptors.fs_descs.sink.bmAttributes = USB_ENDPOINT_XFER_BULK;
	descriptors.fs_descs.sink.wMaxPacketSize = MAX_PACKET_SIZE_FS;
	descriptors.hs_descs.intf.bLength = sizeof(descriptors.hs_descs.intf);
	descriptors.hs_descs.intf.bDescriptorType = USB_DT_INTERFACE;
	descriptors.hs_descs.intf.bInterfaceNumber = 0;
	descriptors.hs_descs.intf.bNumEndpoints = 2;
	descriptors.hs_descs.intf.bInterfaceClass = VENDOR_SPECIFIC_CLASS;
	descriptors.hs_descs.intf.bInterfaceSubClass = ADB_SUBCLASS;
	descriptors.hs_descs.intf.bInterfaceProtocol = ADB_PROTOCOL;
	descriptors.hs_descs.intf.iInterface = 1; /* first string from the provided table */
	descriptors.hs_descs.source.bLength = sizeof(descriptors.hs_descs.source);
	descriptors.hs_descs.source.bDescriptorType = USB_DT_ENDPOINT;
	descriptors.hs_descs.source.bEndpointAddress = 1 | USB_DIR_OUT;
	descriptors.hs_descs.source.bmAttributes = USB_ENDPOINT_XFER_BULK;
	descriptors.hs_descs.source.wMaxPacketSize = MAX_PACKET_SIZE_HS;
	descriptors.hs_descs.sink.bLength = sizeof(descriptors.hs_descs.sink);
	descriptors.hs_descs.sink.bDescriptorType = USB_DT_ENDPOINT;
	descriptors.hs_descs.sink.bEndpointAddress = 2 | USB_DIR_IN;
	descriptors.hs_descs.sink.bmAttributes = USB_ENDPOINT_XFER_BULK;
	descriptors.hs_descs.sink.wMaxPacketSize = MAX_PACKET_SIZE_HS;

	strings.header.magic = htole32(FUNCTIONFS_STRINGS_MAGIC);
	strings.header.length =htole32(sizeof(strings));
	strings.header.str_count = htole32(1);
	strings.header.lang_count = htole32(1);
	strings.lang0.code = htole16(0x0409);
	memcpy(strings.lang0.str1, STR_INTERFACE_, sizeof(STR_INTERFACE_));
}