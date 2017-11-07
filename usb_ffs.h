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

#include <linux/usb/functionfs.h>

#define VENDOR_SPECIFIC_CLASS					0xff


/* What is SUBCLASS 0x6 protocol 0x50 3 endpoints, some Xiaomi thing ?!? */

/* FIXME */
#define ADB_SUBCLASS								0x42
#define ADB_PROTOCOL								0x1

#define LFMB_SUBCLASS							0x43
#define LFMB_PROTOCOL							0x6

#define STR_INTERFACE_		"LFMB Interface"
 
extern struct usb_ffs_descriptors {
    struct usb_functionfs_descs_head header;
    struct {
        struct usb_interface_descriptor intf;
        struct usb_endpoint_descriptor_no_audio source;
        struct usb_endpoint_descriptor_no_audio sink;
    } __attribute__((packed)) fs_descs, hs_descs;
} __attribute__((packed)) descriptors;

extern struct usb_ffs_strings {
    struct usb_functionfs_strings_head header;
    struct {
        __le16 code;
        char str1[sizeof(STR_INTERFACE_)];
    } __attribute__((packed)) lang0;
} __attribute__((packed)) strings;

void fill_descriptors_strings(void);
