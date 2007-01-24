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
uint16_t connect_first_device(PTPParams *params, PTP_USB *ptp_usb, uint8_t *interfaceNumber);
 LIBMTP_error_number_t connect_mtp_devices (PTPParams **params,
                                            PTP_USB **ptp_usb,
                                            uint8_t **interfaceNumber,
                                            uint8_t *numdevices);

/* connect_first_device return codes */
#define PTP_CD_RC_CONNECTED	0
#define PTP_CD_RC_NO_DEVICES	1
#define PTP_CD_RC_ERROR_CONNECTING	2
