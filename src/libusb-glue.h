/**
 * \file libusb-glue.h
 * Low-level USB interface glue towards libusb.
 *
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2007 Marcus Meissner
 * Copyright (C) 2007 Ted Bullock
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Created by Richard Low on 24/12/2005.
 * Modified by Linus Walleij
 *
 */

#include "ptp.h"
#include <usb.h>
#include "libmtp.h"

#define USB_BULK_READ usb_bulk_read
#define USB_BULK_WRITE usb_bulk_write

/**
 * These flags are used to indicate if some or other
 * device need special treatment. These should be possible
 * to concatenate using logical OR so please use one bit per
 * feature and lets pray we don't need more than 32 bits...
 */
#define DEVICE_FLAG_NONE 0x00000000
/**
 * This means that the PTP_OC_MTP_GetObjPropList is broken
 * in the sense that it won't return properly formatted metadata
 * for ALL files on the device when you request an object 
 * property list for object 0xFFFFFFFF with parameter 3 likewise
 * set to 0xFFFFFFFF. Compare to 
 * DEVICE_FLAG_BROKEN_MTPGETOBJECTPROPLIST which only signify
 * that it's broken when getting metadata for a SINGLE object.
 */
#define DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL 0x00000001
/**
 * This means that under Linux, another kernel module may 
 * be using this device's USB interface, so we need to detach 
 * it if it is. Typically this is on dual-mode devices that
 * will present both an MTP compliant interface and device
 * descriptor *and* a USB mass storage interface. If the USB
 * mass storage interface is in use, other apps (like our
 * userspace libmtp through libusb access path) cannot get in
 * and get cosy with it. So we can remove the offending 
 * application. Typically this means you have to run the program
 * as root as well.
 */
#define DEVICE_FLAG_UNLOAD_DRIVER 0x00000002
/**
 * This means that the PTP_OC_MTP_GetObjPropList is broken and
 * won't properly return all object properties if parameter 3
 * is set to 0xFFFFFFFFU.
 */
#define DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST 0x00000004
/**
 * This means the device doesn't send zero packets to indicate
 * end of transfer when the transfer boundary occurs at a 
 * multiple of 64 bytes (the USB 1.1 endpoint size). Instead, 
 * exactly one extra byte is sent at the end of the transfer 
 * if the size is an integer multiple of USB 1.1 endpoint size 
 * (64 bytes).
 *
 * This behaviour is most probably a workaround due to the fact 
 * that the hardware USB slave controller in the device cannot 
 * handle zero writes at all, and the usage of the USB 1.1 
 * endpoint size is due to the fact that the device will "gear 
 * down" on a USB 1.1 hub, and since 64 bytes is a multiple of 
 * 512 bytes, it will work with USB 1.1 and USB 2.0 alike.
 */
#define DEVICE_FLAG_NO_ZERO_READS 0x00000008
/**
 * This flag means that the device is prone to forgetting the
 * OGG container file type, so that libmtp must look at the
 * filename extensions in order to determine that a file is
 * actually OGG. This is a clear and present firmware bug, and
 * while firmware bugs should be fixed in firmware, we like
 * OGG so much that we back it by introducing this flag.
 * The error has only been seen on iriver devices. Turning this
 * flag on won't hurt anything, just that the check against
 * filename extension will be done for files of "unknown" type.
 */
#define DEVICE_FLAG_IRIVER_OGG_ALZHEIMER 0x00000010
/**
 * This flag indicates a limitation in the filenames a device
 * can accept - they must be 7 bit (all chars <= 127/0x7F).
 * It was found first on the Philips Shoqbox, and is a deviation
 * from the PTP standard which mandates that any unicode chars
 * may be used for filenames. I guess this is caused by a 7bit-only
 * filesystem being used intrinsically on the device.
 */
#define DEVICE_FLAG_ONLY_7BIT_FILENAMES 0x00000020

/**
 * Internal USB struct.
 */
typedef struct _PTP_USB PTP_USB;
struct _PTP_USB {
  usb_dev_handle* handle;
  int interface;
  int inep;
  int inep_maxpacket;
  int outep;
  int outep_maxpacket;
  int intep;
  /** File transfer callbacks and counters */
  int callback_active;
  uint64_t current_transfer_total;
  uint64_t current_transfer_complete;
  LIBMTP_progressfunc_t current_transfer_callback;
  void const * current_transfer_callback_data;
  /** Any special device flags, only used internally */
  uint32_t device_flags;
};

struct mtpdevice_list_struct {
  struct usb_device *libusb_device;
  PTPParams *params;
  PTP_USB *ptp_usb;
  uint8_t interface_number;
  struct mtpdevice_list_struct *next;
};
typedef struct mtpdevice_list_struct mtpdevice_list_t;

int open_device (int busn, int devn, short force, PTP_USB *ptp_usb, PTPParams *params, struct usb_device **dev);
void dump_usbinfo(PTP_USB *ptp_usb);
void close_device (PTP_USB *ptp_usb, PTPParams *params, uint8_t interfaceNumber);
LIBMTP_error_number_t find_usb_devices(mtpdevice_list_t **devlist);
void free_mtpdevice_list(mtpdevice_list_t *devlist);

/* connect_first_device return codes */
#define PTP_CD_RC_CONNECTED	0
#define PTP_CD_RC_NO_DEVICES	1
#define PTP_CD_RC_ERROR_CONNECTING	2
