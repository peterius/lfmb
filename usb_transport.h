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

extern int usb_control_fd, usb_bulkin_fd, usb_bulkout_fd;

int usb_init(void);
int usb_write(void * data, unsigned int len);
int usb_read(unsigned short * len, unsigned short previously_read);
int usb_ffs_write(void * data, unsigned int len);
int usb_async_write_post(void * data, unsigned int len);
void usb_reset(void);
void usb_cleanup(void);
