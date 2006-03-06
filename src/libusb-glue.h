/*
 *  mtp-utils.h
 *  XNJB
 *
 *  Created by Richard Low on 24/12/2005.
 *
 */

#include "mtp.h"
#include <usb.h>
#include "libmtp.h"

#define USB_BULK_READ usb_bulk_read
#define USB_BULK_WRITE usb_bulk_write

/* the vendor ID for creative devices that we can connect to (temporary) */

#define CREATIVE_VENDOR_ID	0x041e

int open_device (int busn, int devn, short force, PTP_USB *ptp_usb, PTPParams *params, struct usb_device **dev);
void close_device (PTP_USB *ptp_usb, PTPParams *params, uint8_t interfaceNumber);
uint16_t connect_first_device(PTPParams *params, PTP_USB *ptp_usb, uint8_t *interfaceNumber);

/* connect_first_device return codes */
#define PTP_CD_RC_CONNECTED	0
#define PTP_CD_RC_NO_DEVICES	1
#define PTP_CD_RC_ERROR_CONNECTING	2
