/*
 *  libusb-glue.h
 *
 *  Created by Richard Low on 24/12/2005.
 *  Modified by Linus Walleij
 *
 */

#include "ptp.h"
#include <usb.h>
#include "libmtp.h"

#define USB_BULK_READ usb_bulk_read
#define USB_BULK_WRITE usb_bulk_write

/**
 * Internal USB struct (TODO: discard for device struct?)
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
  void *current_transfer_callback_data;
};

int get_device_list(LIBMTP_device_entry_t ** const devices, int * const numdevs);
int open_device (int busn, int devn, short force, PTP_USB *ptp_usb, PTPParams *params, struct usb_device **dev);
void dump_usbinfo(PTP_USB *ptp_usb);
void close_device (PTP_USB *ptp_usb, PTPParams *params, uint8_t interfaceNumber);
uint16_t connect_first_device(PTPParams *params, PTP_USB *ptp_usb, uint8_t *interfaceNumber);

/* connect_first_device return codes */
#define PTP_CD_RC_CONNECTED	0
#define PTP_CD_RC_NO_DEVICES	1
#define PTP_CD_RC_ERROR_CONNECTING	2
