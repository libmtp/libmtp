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
 * This means the device supports both MTP and USB mass 
 * storage by dynamically reconfiguring itself if it is not
 * used with MTP before a certain timeout.
 */
#define DEVICE_FLAG_DUALMODE 0x00000001
/**
 * This means that under Linux, another kernel module may 
 * be using the MTP interface, so we need to detach it if
 * it is.
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

int open_device (int busn, int devn, short force, PTP_USB *ptp_usb, PTPParams *params, struct usb_device **dev);
void dump_usbinfo(PTP_USB *ptp_usb);
void close_device (PTP_USB *ptp_usb, PTPParams *params, uint8_t interfaceNumber);
LIBMTP_error_number_t find_usb_devices (PTPParams ***params,
                                            PTP_USB ***ptp_usb,
                                            uint8_t interfaceNumber[],
                                            uint8_t *numdevices);

/* connect_first_device return codes */
#define PTP_CD_RC_CONNECTED	0
#define PTP_CD_RC_NO_DEVICES	1
#define PTP_CD_RC_ERROR_CONNECTING	2
