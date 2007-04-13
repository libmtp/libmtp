/*
 * \file libusb-glue.c
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
 * Created by Richard Low on 24/12/2005. (as mtp-utils.c)
 * Modified by Linus Walleij 2006-03-06
 *  (Notice that Anglo-Saxons use little-endian dates and Swedes 
 *   use big-endian dates.)
 *
 */
#include "libmtp.h"
#include "libusb-glue.h"
#include "util.h"
#include "ptp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>

#include "ptp-pack.c"

/* To enable debug prints, switch on this */
//#define ENABLE_USB_BULK_DEBUG

/* OUR APPLICATION USB URB (2MB) ;) */
#define PTPCAM_USB_URB		2097152

/* this must not be too short - the original 4000 was not long
   enough for big file transfers. I imagine the player spends a 
   bit of time gearing up to receiving lots of data. This also makes
   connecting/disconnecting more reliable */
#define USB_TIMEOUT		10000
#define USB_CAPTURE_TIMEOUT	20000

/* USB control message data phase direction */
#ifndef USB_DP_HTD
#define USB_DP_HTD		(0x00 << 7)	/* host to device */
#endif
#ifndef USB_DP_DTH
#define USB_DP_DTH		(0x01 << 7)	/* device to host */
#endif

/* USB Feature selector HALT */
#ifndef USB_FEATURE_HALT
#define USB_FEATURE_HALT	0x00
#endif

/*
 * MTP device list, trying real bad to get all devices into
 * this list by stealing from everyone I know.
 */
static const LIBMTP_device_entry_t mtp_device_table[] = {
  
  /*
   * Creative Technology
   * Initially the Creative devices was all we supported so these are
   * the most thoroughly tested devices.
   */
  { "Creative Zen Vision", 0x041e, 0x411f, DEVICE_FLAG_NONE },
  { "Creative Portable Media Center", 0x041e, 0x4123, DEVICE_FLAG_NONE },
  { "Creative Zen Xtra (MTP mode)", 0x041e, 0x4128, DEVICE_FLAG_NONE },
  { "Second generation Dell DJ", 0x041e, 0x412f, DEVICE_FLAG_NONE },
  { "Creative Zen Micro (MTP mode)", 0x041e, 0x4130, DEVICE_FLAG_NONE },
  { "Creative Zen Touch (MTP mode)", 0x041e, 0x4131, DEVICE_FLAG_NONE },
  { "Dell Pocket DJ (MTP mode)", 0x041e, 0x4132, DEVICE_FLAG_NONE },
  { "Creative Zen Sleek (MTP mode)", 0x041e, 0x4137, DEVICE_FLAG_NONE },
  { "Creative Zen MicroPhoto", 0x041e, 0x413c, DEVICE_FLAG_NONE },
  { "Creative Zen Sleek Photo", 0x041e, 0x413d, DEVICE_FLAG_NONE },
  { "Creative Zen Vision:M", 0x041e, 0x413e, DEVICE_FLAG_NONE },
  // Reported by marazm@o2.pl
  { "Creative Zen V", 0x041e, 0x4150, DEVICE_FLAG_NONE },
  // Reported by danielw@iinet.net.au
  { "Creative Zen Vision:M (DVP-HD0004)", 0x041e, 0x4151, DEVICE_FLAG_NONE },
  // Reported by Darel on the XNJB forums
  { "Creative Zen V Plus", 0x041e, 0x4152, DEVICE_FLAG_NONE },
  { "Creative Zen Vision W", 0x041e, 0x4153, DEVICE_FLAG_NONE },

  /*
   * Samsung
   * We suspect that more of these are dual mode.
   */
  // From libgphoto2
  { "Samsung YH-820", 0x04e8, 0x502e, DEVICE_FLAG_NONE },
  // Contributed by polux2001@users.sourceforge.net
  { "Samsung YH-925(-GS)", 0x04e8, 0x502f, DEVICE_FLAG_NONE },
  // Contributed by aronvanammers on SourceForge
  { "Samsung YH-925GS", 0x04e8, 0x5024, DEVICE_FLAG_NONE },
  // Contributed by anonymous person on SourceForge
  { "Samsung YP-T7J", 0x04e8, 0x5047, DEVICE_FLAG_NONE },
  // Reported by cstrickler@gmail.com
  { "Samsung YP-U2J (YP-U2JXB/XAA)", 0x04e8, 0x5054, DEVICE_FLAG_NONE },
  // Reported by Andrew Benson
  { "Samsung YP-F2J", 0x04e8, 0x5057, DEVICE_FLAG_DUALMODE },
  // Reported by Patrick <skibler@gmail.com>
  { "Samsung YP-K5", 0x04e8, 0x505a, DEVICE_FLAG_NO_ZERO_READS },
  // Reported by Matthew Wilcox <matthew@wil.cx>
  { "Samsung YP-T9", 0x04e8, 0x507f, DEVICE_FLAG_NONE },
  // From Paul Clinch
  { "Samsung YP-K3", 0x04e8, 0x5081, DEVICE_FLAG_NONE },
  // From a rouge .INF file
  { "Samsung YH-999 Portable Media Center", 0x04e8, 0x5a0f, DEVICE_FLAG_NONE },
  // From Lionel Bouton
  { "Samsung X830 Mobile Phone", 0x04e8, 0x6702, DEVICE_FLAG_NONE },
  // From XNJB user
  { "Samsung YP-Z5", 0x04e8, 0x503c, DEVICE_FLAG_NONE },

  /*
   * Intel
   */
  { "Intel Bandon Portable Media Center", 0x045e, 0x00c9, DEVICE_FLAG_NONE },

  /*
   * JVC
   */
  // From Mark Veinot
  { "JVC Alneo XA-HD500", 0x04f1, 0x6105, DEVICE_FLAG_NONE },

  /*
   * Philips
   */
  // From libgphoto2 source
  { "Philips HDD6320", 0x0471, 0x01eb, DEVICE_FLAG_NONE },
  { "Philips HDD6320/00 & HDD6330/17", 0x0471, 0x014b, DEVICE_FLAG_NONE },
  // Anonymous SourceForge user
  { "Philips HDD1630/17", 0x0471, 0x014c, DEVICE_FLAG_NONE },
  // From Gerhard Mekenkamp
  { "Philips GoGear Audio", 0x0471, 0x0165, DEVICE_FLAG_NONE },
  // from XNJB forum
  { "Philips GoGear SA9200", 0x0471, 0x014f, DEVICE_FLAG_NONE },
  // from XNJB user
  { "Philips PSA235", 0x0471, 0x7e01, DEVICE_FLAG_NONE },
  // from discussion forum
  { "Philips HDD085/00 & HDD082/17", 0x0471, 0x014d, DEVICE_FLAG_NONE },

  /*
   * SanDisk
   * several devices (c150 for sure) are definately dual-mode and must 
   * have the USB mass storage driver that hooks them unloaded first.
   */
  // Reported by Brian Robison
  { "SanDisk Sansa m230/m240", 0x0781, 0x7400, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by tangent_@users.sourceforge.net
  { "SanDisk Sansa c150", 0x0781, 0x7410, DEVICE_FLAG_UNLOAD_DRIVER },
  // From libgphoto2 source
  { "SanDisk Sansa e200", 0x0781, 0x7420, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by gonkflea@users.sourceforge.net
  { "SanDisk Sansa e260", 0x0781, 0x7420, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by anonymous user at sourceforge.net
  { "SanDisk Sansa c250", 0x0781, 0x7450, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by XNJB user
  { "SanDisk Sansa e280", 0x0781, 0x7421, DEVICE_FLAG_UNLOAD_DRIVER },
  // Reported by XNJB user
  { "SanDisk Sansa Connect", 0x0781, 0x7480, DEVICE_FLAG_UNLOAD_DRIVER },
  

  /*
   * iRiver
   * we assume that PTP_OC_MTP_GetObjPropList is essentially
   * broken on all iRiver devices, meaning it simply won't return
   * all properties for a file when asking for metadata 0xffffffff. 
   * Please test on your device if you believe it isn't broken!
   * Some devices from http://www.mtp-ums.net/viewdeviceinfo.php
   */
  { "iRiver Portable Media Center", 0x1006, 0x4002, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver Portable Media Center", 0x1006, 0x4003, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // From libgphoto2 source
  { "iRiver T10", 0x4102, 0x1113, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver T20 FM", 0x4102, 0x1114, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // This appears at the MTP-UMS site
  { "iRiver T20", 0x4102, 0x1115, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver U10", 0x4102, 0x1116, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver T10", 0x4102, 0x1117, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver T20", 0x4102, 0x1118, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver T30", 0x4102, 0x1119, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by David Wolpoff
  { "iRiver T10 2GB", 0x4102, 0x1120, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Rough guess this is the MTP device ID...
  { "iRiver N12", 0x4102, 0x1122, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by Adam Torgerson
  { "iRiver Clix", 0x4102, 0x112a, 
    DEVICE_FLAG_NO_ZERO_READS | DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  // Reported by Scott Call
  { "iRiver H10 20GB", 0x4102, 0x2101, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },
  { "iRiver H10", 0x4102, 0x2102, 
    DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST | DEVICE_FLAG_NO_ZERO_READS | 
    DEVICE_FLAG_IRIVER_OGG_ALZHEIMER },

  /*
   * Dell
   */
  { "Dell DJ Itty", 0x413c, 0x4500, DEVICE_FLAG_NONE },
  
  /*
   * Toshiba
   */
  { "Toshiba Gigabeat MEGF-40", 0x0930, 0x0009, DEVICE_FLAG_NONE },
  { "Toshiba Gigabeat", 0x0930, 0x000c, DEVICE_FLAG_NONE },
  // Reported by Nicholas Tripp
  { "Toshiba Gigabeat P20", 0x0930, 0x000f, DEVICE_FLAG_NONE },
  // From libgphoto2
  { "Toshiba Gigabeat S", 0x0930, 0x0010, DEVICE_FLAG_NONE },
  // Reported by Rob Brown
  { "Toshiba Gigabeat P10", 0x0930, 0x0011, DEVICE_FLAG_NONE },
  
  /*
   * Archos
   */
  // Reported by gudul1@users.sourceforge.net
  { "Archos 104 (MTP mode)", 0x0e79, 0x120a, DEVICE_FLAG_NONE },
  // Added by Jan Binder
  { "Archos XS202 (MTP mode)", 0x0e79, 0x1208, DEVICE_FLAG_NONE },
  // Reported by Etienne Chauchot <chauchot.etienne@free.fr>
  { "Archos 504 (MTP mode)", 0x0e79, 0x1307, DEVICE_FLAG_NONE },

  /*
   * Dunlop (OEM of EGOMAN ltd?) reported by Nanomad
   * This unit is falsely detected as USB mass storage in Linux
   * prior to kernel 2.6.19 (fixed by patch from Alan Stern)
   * so on older kernels special care is needed to remove the
   * USB mass storage driver that erroneously binds to the device
   * interface.
   */
  { "Dunlop MP3 player 1GB / EGOMAN MD223AFD", 0x10d6, 0x2200, DEVICE_FLAG_UNLOAD_DRIVER},
  
  /*
   * Microsoft
   */
  // Reported by Farooq Zaman
  { "Microsoft Zune", 0x045e, 0x0710, DEVICE_FLAG_NONE }, 
  
  /*
   * Sirius
   */
  { "Sirius Stiletto", 0x18f6, 0x0102, DEVICE_FLAG_NONE },

  /*
   * Canon
   * This is actually a camera, but it has a Microsoft device descriptor
   * and reports itself as supporting the MTP extension.
   */
  {"Canon PowerShot A640 (PTP/MTP mode)", 0x04a9, 0x3139, DEVICE_FLAG_NONE },

  /*
   * Nokia
   */
  {"Nokia Mobile Phones (MTP mode)", 0x0421, 0x04e1, DEVICE_FLAG_NONE },

  /*
   * LOGIK
   * Sold in the UK, seem to be manufactured by CCTech in China.
   */
  {"Logik LOG DAX MP3 and DAB Player", 0x13d1, 0x7002, DEVICE_FLAG_NONE },

  /*
   * Other strange stuff.
   */
  {"Isabella's prototype", 0x0b20, 0xddee, DEVICE_FLAG_NONE }
};
static const int mtp_device_table_size = sizeof(mtp_device_table) / sizeof(LIBMTP_device_entry_t);

int ptpcam_usb_timeout = USB_TIMEOUT;

// Local functions
static struct usb_bus* init_usb();
static void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber);
static void find_endpoints(struct usb_device *dev, int* inep, int* inep_maxpacket, int* outep, int* outep_maxpacket, int* intep);
static void clear_stall(PTP_USB* ptp_usb);
static int init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev);
static short ptp_write_func (unsigned long,PTPDataHandler*,void *data,unsigned long*);
static short ptp_read_func (unsigned long,PTPDataHandler*,void *data,unsigned long*,int);
static int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep);
static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);

/**
 * Get a list of the supported USB devices.
 *
 * The developers depend on users of this library to constantly
 * add in to the list of supported devices. What we need is the
 * device name, USB Vendor ID (VID) and USB Product ID (PID).
 * put this into a bug ticket at the project homepage, please.
 * The VID/PID is used to let e.g. udev lift the device to
 * console userspace access when it's plugged in.
 *
 * @param devices a pointer to a pointer that will hold a device
 *        list after the call to this function, if it was
 *        successful.
 * @param numdevs a pointer to an integer that will hold the number
 *        of devices in the device list if the call was successful.
 * @return 0 if the list was successfull retrieved, any other
 *        value means failure.
 */
int LIBMTP_Get_Supported_Devices_List(LIBMTP_device_entry_t ** const devices, int * const numdevs)
{
  *devices = (LIBMTP_device_entry_t *) &mtp_device_table;
  *numdevs = mtp_device_table_size;
  return 0;
}


static struct usb_bus* init_usb()
{
  usb_init();
  usb_find_busses();
  usb_find_devices();
  return (usb_get_busses());
}

/**
 * Small recursive function to append a new usb_device to the linked list of
 * USB MTP devices
 * @param devlist dynamic linked list of pointers to usb devices with MTP 
 * properties.
 * @param next New USB MTP device to be added to list
 * @return nothing
 */
static mtpdevice_list_t *append_to_mtpdevice_list(mtpdevice_list_t *devlist,
				     struct usb_device *newdevice)
{
  mtpdevice_list_t *new_list_entry;
  
  new_list_entry = (mtpdevice_list_t *) malloc(sizeof(mtpdevice_list_t));
  if (new_list_entry == NULL) {
    return NULL;
  }
  // Fill in USB device, if we *HAVE* to make a copy of the device do it here.
  new_list_entry->libusb_device = newdevice;
  new_list_entry->next = NULL;
  
  if (devlist == NULL) {
    return new_list_entry;
  } else {
    mtpdevice_list_t *tmp = devlist;
    while (tmp->next != NULL) {
      tmp = tmp->next;
    }
    tmp->next = new_list_entry;
  }
  return devlist;
}

/**
 * Small recursive function to free dynamic memory allocated to the linked list
 * of USB MTP devices
 * @param devlist dynamic linked list of pointers to usb devices with MTP 
 * properties.
 * @return nothing
 */
void free_mtpdevice_list(mtpdevice_list_t *devlist)
{
  mtpdevice_list_t *tmplist = devlist;

  if (devlist == NULL)
    return;
  while (tmplist != NULL) {
    mtpdevice_list_t *tmp = tmplist;
    tmplist = tmplist->next;
    // Do not free() the fields (ptp_usb, parms)! These are used elsewhere.
    free(tmp);
  }
  return;
}

/**
 * This checks if a device has an MTP descriptor.
 * @param dev a device struct from libusb.
 * @return 1 if the device is MTP compliant, 0 if not.
 */
static int device_has_descriptor(struct usb_device *dev)
{
  usb_dev_handle *devh;
  unsigned char buf[1024], cmd;
  int ret;
  
  /* Don't examine hubs (no point in that) */
  if (dev->descriptor.bDeviceClass == USB_CLASS_HUB) {
    return 0;
  }
  
  /* Attempt to open Device on this port */
  devh = usb_open(dev);
  if (devh == NULL) {
    /* Could not open this device */
    return 0;
  }
  
  /* Read the special descriptor */
  ret = usb_get_descriptor(devh, 0x03, 0xee, buf, sizeof(buf));
  
  /* Check if descriptor length is at least 10 bytes */
  if (ret < 10) {
    usb_close(devh);
    return 0;
  }
      
  /* Check if this device has a Microsoft Descriptor */
  if (!((buf[2] == 'M') && (buf[4] == 'S') &&
	(buf[6] == 'F') && (buf[8] == 'T'))) {
    usb_close(devh);
    return 0;
  }
      
  /* Check if device responds to control message 1 or if there is an error */
  cmd = buf[16];
  ret = usb_control_msg (devh,
			 USB_ENDPOINT_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR,
			 cmd,
			 0,
			 4,
			 (char *) buf,
			 sizeof(buf),
			 1000);
  
  /* If this is true, the device either isn't MTP or there was an error */
  if (ret <= 0x15) {
    /* TODO: If there was an error, flag it and let the user know somehow */
    /* if(ret == -1) {} */
    usb_close(devh);
    return 0;
  }
  
  /* Check if device is MTP or if it is something like a USB Mass Storage 
     device with Janus DRM support */
  if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
    usb_close(devh);
    return 0;
  }
      
  /* After this point we are probably dealing with an MTP device */

  /* Check if device responds to control message 2 or if there is an error*/
  ret = usb_control_msg (devh,
			 USB_ENDPOINT_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR,
			 cmd,
			 0,
			 5,
			 (char *) buf,
			 sizeof(buf),
			 1000);
  
  /* If this is true, the device errored against control message 2 */
  if (ret == -1) {
    /* TODO: Implement callback function to let managing program know there
       was a problem, along with description of the problem */
    fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
	    "ProductID:%04x encountered an error responding to "
	    "control message 2.\n"
	    "Problems may arrise but continuing\n",
	    dev->descriptor.idVendor, dev->descriptor.idProduct);
  } else if (ret <= 0x15) {
    /* TODO: Implement callback function to let managing program know there
       was a problem, along with description of the problem */
    fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
	    "ProductID:%04x responded to control message 2 with a "
	    "response that was too short. Problems may arrise but "
	    "continuing\n",
	    dev->descriptor.idVendor, dev->descriptor.idProduct);
  } else if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
    /* TODO: Implement callback function to let managing program know there
       was a problem, along with description of the problem */
    fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
	    "ProductID:%04x encountered an error responding to "
	    "control message 2\n"
	    "Problems may arrise but continuing\n",
	    dev->descriptor.idVendor, dev->descriptor.idProduct);
  }
  
  /* Close the USB device handle */
  usb_close(devh);
  return 1;
}

/**
 * This function scans through the connected usb devices on a machine and
 * if they match known Vendor and Product identifiers appends them to the
 * dynamic array mtp_device_list. Be sure to call 
 * <code>free(mtp_device_list)</code> when you are done with it, assuming it
 * is not NULL.
 * @param mtp_device_list dynamic array of pointers to usb devices with MTP 
 *        properties (if this list is not empty, new entries will be appended
 *        to the list).
 * @return LIBMTP_ERROR_NONE implies that devices have been found, scan the list
 *        appropriately. LIBMTP_ERROR_NO_DEVICE_ATTACHED implies that no devices have
 *        been found.
 */
static LIBMTP_error_number_t get_mtp_usb_device_list(mtpdevice_list_t ** mtp_device_list)
{
  struct usb_bus *bus = init_usb();

  for (; bus != NULL; bus = bus->next) {
    struct usb_device *dev = bus->devices;
    for (; dev != NULL; dev = dev->next) {
      if (device_has_descriptor(dev)) {
	/* Append this usb device to the MTP USB Device List */
	*mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, dev);
      } else {
	/* Check if it's in the known devices list then */
	int i;
	
	for(i = 0; i < mtp_device_table_size; i++) {
	  if(dev->descriptor.bDeviceClass != USB_CLASS_HUB && 
	     dev->descriptor.idVendor == mtp_device_table[i].vendor_id &&
	     dev->descriptor.idProduct == mtp_device_table[i].product_id) {
	    /* Append this usb device to the MTP device list */
	    *mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, dev);
	    break;
	  }
	}
      }
    }
  }
  
  /* If nothing was found we end up here. */
  if(*mtp_device_list == NULL) {
    return LIBMTP_ERROR_NO_DEVICE_ATTACHED;
  }
  return LIBMTP_ERROR_NONE;
}

/**
 * Detect the MTP device descriptor and return the VID and PID
 * of the first device found. This is a very low-level function
 * which is intended for use with <b>udev</b> or other hotplug
 * mechanisms. The idea is that a script may want to know if the
 * just plugged-in device was an MTP device or not.
 * 
 * @param vid the Vendor ID (VID) of the first device found.
 * @param pid the Product ID (PID) of the first device found.
 * @return the number of detected devices or -1 if the call
 *         was unsuccessful.
 */
int LIBMTP_Detect_Descriptor(uint16_t *vid, uint16_t *pid)
{
  mtpdevice_list_t *devlist;
  LIBMTP_error_number_t ret;  

  ret = get_mtp_usb_device_list(&devlist);
  if (ret != LIBMTP_ERROR_NONE) {
    *vid = *pid = 0;
    return -1;
  }
  *vid = devlist->libusb_device->descriptor.idVendor;
  *pid = devlist->libusb_device->descriptor.idProduct;
  free_mtpdevice_list(devlist);
  return 1;
}

/**
 * This routine just dumps out low-level
 * USB information about the current device.
 * @param ptp_usb the USB device to get information from.
 */
void dump_usbinfo(PTP_USB *ptp_usb)
{
  int res;
  struct usb_device *dev;

#ifdef LIBUSB_HAS_GET_DRIVER_NP
  char devname[0x10];
  
  devname[0] = '\0';
  res = usb_get_driver_np(ptp_usb->handle, ptp_usb->interface, devname, sizeof(devname));
  if (devname[0] != '\0') {
    printf("   Using kernel interface \"%s\"\n", devname);
  }
#endif
  dev = usb_device(ptp_usb->handle);
  printf("   bcdUSB: %d\n", dev->descriptor.bcdUSB);
  printf("   bDeviceClass: %d\n", dev->descriptor.bDeviceClass);
  printf("   bDeviceSubClass: %d\n", dev->descriptor.bDeviceSubClass);
  printf("   bDeviceProtocol: %d\n", dev->descriptor.bDeviceProtocol);
  printf("   idVendor: %04x\n", dev->descriptor.idVendor);
  printf("   idProduct: %04x\n", dev->descriptor.idProduct);
  printf("   IN endpoint maxpacket: %d bytes\n", ptp_usb->inep_maxpacket);
  printf("   OUT endpoint maxpacket: %d bytes\n", ptp_usb->outep_maxpacket);
  printf("   Device flags: 0x%08x\n", ptp_usb->device_flags);
  // TODO: add in string dumps for iManufacturer, iProduct, iSerialnumber...
}

static void
ptp_debug (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->debug_func!=NULL)
                params->debug_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}  

static void
ptp_error (PTPParams *params, const char *format, ...)
{  
        va_list args;

        va_start (args, format);
        if (params->error_func!=NULL)
                params->error_func (params->data, format, args);
        else
	{
                vfprintf (stderr, format, args);
		fprintf (stderr,"\n");
		fflush (stderr);
	}
        va_end (args);
}


/*
 * ptp_read_func() and ptp_write_func() are
 * based on same functions usb.c in libgphoto2.
 * Much reading packet logs and having fun with trials and errors
 * reveals that WMP / Windows is probably using an algorithm like this
 * for large transfers:
 *
 * 1. Send the command (0x0c bytes) if headers are split, else, send 
 *    command plus sizeof(endpoint) - 0x0c bytes.
 * 2. Send first packet, max size to be sizeof(endpoint) but only when using
 *    split headers. Else goto 3.
 * 3. REPEAT send 0x10000 byte chunks UNTIL remaining bytes < 0x10000
 *    We call 0x10000 CONTEXT_BLOCK_SIZE.
 * 4. Send remaining bytes MOD sizeof(endpoint)
 * 5. Send remaining bytes. If this happens to be exactly sizeof(endpoint)
 *    then also send a zero-length package.
 *
 * Further there is some special quirks to handle zero reads from the
 * device, since some devices can't do them at all due to shortcomings
 * of the USB slave controller in the device.
 */
#define CONTEXT_BLOCK_SIZE_1	0x3e00
#define CONTEXT_BLOCK_SIZE_2  0x200
#define CONTEXT_BLOCK_SIZE    CONTEXT_BLOCK_SIZE_1+CONTEXT_BLOCK_SIZE_2
static short
ptp_read_func (
	unsigned long size, PTPDataHandler *handler,void *data,
	unsigned long *readbytes,
	int readzero
) {
  PTP_USB *ptp_usb = (PTP_USB *)data;
  unsigned long toread = 0;
  int result = 0;
  unsigned long curread = 0;
  unsigned long written;
  unsigned char *bytes;
  int expect_terminator_byte = 0;

  // This is the largest block we'll need to read in.
  bytes = malloc(CONTEXT_BLOCK_SIZE);
  while (curread < size) {
    
#ifdef ENABLE_USB_BULK_DEBUG
    printf("Remaining size to read: 0x%04x bytes\n", size - curread);
#endif
    // check equal to condition here
    if (size - curread < CONTEXT_BLOCK_SIZE)
    {
      // this is the last packet
      toread = size - curread;
      // this is equivalent to zero read for these devices
      if (readzero && ptp_usb->device_flags & DEVICE_FLAG_NO_ZERO_READS && toread % 64 == 0) {
        toread += 1;
        expect_terminator_byte = 1;
      }
    }
    else if (curread == 0)
      // we are first packet, but not last packet
      toread = CONTEXT_BLOCK_SIZE_1;
    else if (toread == CONTEXT_BLOCK_SIZE_1)
      toread = CONTEXT_BLOCK_SIZE_2;
    else if (toread == CONTEXT_BLOCK_SIZE_2)
      toread = CONTEXT_BLOCK_SIZE_1;
    else
      printf("unexpected toread size 0x%04x, 0x%04x remaining bytes\n", 
	     (unsigned int) toread, (unsigned int) (size-curread));

#ifdef ENABLE_USB_BULK_DEBUG
    printf("Reading in 0x%04x bytes\n", toread);
#endif
    result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, (char*)bytes, toread, ptpcam_usb_timeout);
#ifdef ENABLE_USB_BULK_DEBUG
    printf("Result of read: 0x%04x\n", result);
#endif
        
    if (result < 0) {
      return PTP_ERROR_IO;
    }
#ifdef ENABLE_USB_BULK_DEBUG
    printf("<==USB IN\n");
    if (result == 0)
      printf("Zero Read\n");
    else
      data_dump_ascii (stdout,bytes,result,16);
#endif
    
    // want to discard extra byte
    if (expect_terminator_byte && result == toread)
    {
#ifdef ENABLE_USB_BULK_DEBUG
      printf("<==USB IN\nDiscarding extra byte\n");
#endif
      result--;
    }
    
    handler->putfunc(NULL, handler->private, result, bytes, &written);
    
    ptp_usb->current_transfer_complete += result;
    curread += result;

    // Increase counters, call callback
    if (ptp_usb->callback_active) {
      if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
	// send last update and disable callback.
	ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
	ptp_usb->callback_active = 0;
      }
      if (ptp_usb->current_transfer_callback != NULL) {
	(void) ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
						  ptp_usb->current_transfer_total,
						  ptp_usb->current_transfer_callback_data);
      }
    }  

    if (result < toread) /* short reads are common */
      break;
  }
  if (readbytes) *readbytes = curread;
  free (bytes);
  
  // there might be a zero packet waiting for us...
  if (readzero && 
      !(ptp_usb->device_flags & DEVICE_FLAG_NO_ZERO_READS) && 
      curread % ptp_usb->outep_maxpacket == 0) {
    char temp;
    int zeroresult = 0;

#ifdef ENABLE_USB_BULK_DEBUG
    printf("<==USB IN\n");
    printf("Zero Read\n");
#endif
    zeroresult = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, &temp, 0, ptpcam_usb_timeout);
    if (zeroresult != 0)
      printf("LIBMTP panic: unable to read in zero packet, response 0x%04x", zeroresult);
  }
  
  if (result > 0) {
    return (PTP_RC_OK);
  } else {
    return PTP_ERROR_IO;
  }
}

static short
ptp_write_func (
        unsigned long   size,
        PTPDataHandler  *handler,
        void            *data,
        unsigned long   *written
) {
  PTP_USB *ptp_usb = (PTP_USB *)data;
  unsigned long towrite = 0;
  int result = 0;
  unsigned long curwrite = 0;
  unsigned char *bytes;

  // This is the largest block we'll need to read in.  
  bytes = malloc(CONTEXT_BLOCK_SIZE);
  if (!bytes) {
    return PTP_ERROR_IO;
  }
  while (curwrite < size) {
    towrite = size-curwrite;
    if (towrite > CONTEXT_BLOCK_SIZE) {
      towrite = CONTEXT_BLOCK_SIZE;
    } else {
      // This magic makes packets the same size that WMP send them.
      if (towrite > ptp_usb->outep_maxpacket && towrite % ptp_usb->outep_maxpacket != 0) {
        towrite -= towrite % ptp_usb->outep_maxpacket;
      }
    }
    handler->getfunc(NULL, handler->private,towrite,bytes,&towrite);
    result = USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char*)bytes,towrite,ptpcam_usb_timeout);
#ifdef ENABLE_USB_BULK_DEBUG
    printf("USB OUT==>\n");
    data_dump_ascii (stdout,bytes,towrite,16);
#endif
    if (result < 0) {
      return PTP_ERROR_IO;
    }
    // Increase counters
    ptp_usb->current_transfer_complete += result;
    curwrite += result;

    // call callback
    if (ptp_usb->callback_active) {
      if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
	// send last update and disable callback.
	ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
	ptp_usb->callback_active = 0;
      }
      if (ptp_usb->current_transfer_callback != NULL) {
	(void) ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
						  ptp_usb->current_transfer_total,
						  ptp_usb->current_transfer_callback_data);
      }
    }
    if (result < towrite) /* short writes happen */
      break;
  }
  free (bytes);
  if (written) {
    *written = curwrite;
  }
  

  // If this is the last transfer send a zero write if required
  if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
    if ((towrite % ptp_usb->outep_maxpacket) == 0) {
#ifdef ENABLE_USB_BULK_DEBUG
      printf("USB OUT==>\n");
      printf("Zero Write\n");
#endif
      result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)"x",0,ptpcam_usb_timeout);
    }
  }
    
  if (result < 0)
    return PTP_ERROR_IO;
  return PTP_RC_OK;
}

/* memory data get/put handler */
typedef struct {
	unsigned char	*data;
	unsigned long	size, curoff;
} PTPMemHandlerPrivate;

static uint16_t
memory_getfunc(PTPParams* params, void* private,
	       unsigned long wantlen, unsigned char *data,
	       unsigned long *gotlen
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)private;
	unsigned long tocopy = wantlen;

	if (priv->curoff + tocopy > priv->size)
		tocopy = priv->size - priv->curoff;
	memcpy (data, priv->data + priv->curoff, tocopy);
	priv->curoff += tocopy;
	*gotlen = tocopy;
	return PTP_RC_OK;
}

static uint16_t
memory_putfunc(PTPParams* params, void* private,
	       unsigned long sendlen, unsigned char *data,
	       unsigned long *putlen
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)private;

	if (priv->curoff + sendlen > priv->size) {
		priv->data = realloc (priv->data, priv->curoff+sendlen);
		priv->size = priv->curoff + sendlen;
	}
	memcpy (priv->data + priv->curoff, data, sendlen);
	priv->curoff += sendlen;
	*putlen = sendlen;
	return PTP_RC_OK;
}

/* init private struct for receiving data. */
static uint16_t
ptp_init_recv_memory_handler(PTPDataHandler *handler) {
	PTPMemHandlerPrivate* priv;
	priv = malloc (sizeof(PTPMemHandlerPrivate));
	handler->private = priv;
	handler->getfunc = memory_getfunc;
	handler->putfunc = memory_putfunc;
	priv->data = NULL;
	priv->size = 0;
	priv->curoff = 0;
	return PTP_RC_OK;
}

/* init private struct and put data in for sending data.
 * data is still owned by caller.
 */
static uint16_t
ptp_init_send_memory_handler(PTPDataHandler *handler,
	unsigned char *data, unsigned long len
) {
	PTPMemHandlerPrivate* priv;
	priv = malloc (sizeof(PTPMemHandlerPrivate));
	if (!priv)
		return PTP_RC_GeneralError;
	handler->private = priv;
	handler->getfunc = memory_getfunc;
	handler->putfunc = memory_putfunc;
	priv->data = data;
	priv->size = len;
	priv->curoff = 0;
	return PTP_RC_OK;
}

/* free private struct + data */
static uint16_t
ptp_exit_send_memory_handler (PTPDataHandler *handler) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)handler->private;
	/* data is owned by caller */
	free (priv);
	return PTP_RC_OK;
}

/* hand over our internal data to caller */
static uint16_t
ptp_exit_recv_memory_handler (PTPDataHandler *handler,
	unsigned char **data, unsigned long *size
) {
	PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*)handler->private;
	*data = priv->data;
	*size = priv->size;
	free (priv);
	return PTP_RC_OK;
}

/* send / receive functions */

uint16_t
ptp_usb_sendreq (PTPParams* params, PTPContainer* req)
{
	uint16_t ret;
	PTPUSBBulkContainer usbreq;
	PTPDataHandler	memhandler;
	unsigned long written, towrite;

	/* build appropriate USB container */
	usbreq.length=htod32(PTP_USB_BULK_REQ_LEN-
		(sizeof(uint32_t)*(5-req->Nparam)));
	usbreq.type=htod16(PTP_USB_CONTAINER_COMMAND);
	usbreq.code=htod16(req->Code);
	usbreq.trans_id=htod32(req->Transaction_ID);
	usbreq.payload.params.param1=htod32(req->Param1);
	usbreq.payload.params.param2=htod32(req->Param2);
	usbreq.payload.params.param3=htod32(req->Param3);
	usbreq.payload.params.param4=htod32(req->Param4);
	usbreq.payload.params.param5=htod32(req->Param5);
	/* send it to responder */
	towrite = PTP_USB_BULK_REQ_LEN-(sizeof(uint32_t)*(5-req->Nparam));
	ptp_init_send_memory_handler (&memhandler, (unsigned char*)&usbreq, towrite);
	ret=ptp_write_func(
		towrite,
		&memhandler,
		params->data,
		&written
	);
	ptp_exit_send_memory_handler (&memhandler);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
			"PTP: request code 0x%04x sending req error 0x%04x",
			req->Code,ret); */
	}
	if (written != towrite) {
		ptp_error (params, 
			"PTP: request code 0x%04x sending req wrote only %ld bytes instead of %d",
			req->Code, written, towrite
		);
		ret = PTP_ERROR_IO;
	}
	return ret;
}

uint16_t
ptp_usb_senddata (PTPParams* params, PTPContainer* ptp,
		  unsigned long size, PTPDataHandler *handler
) {
	uint16_t ret;
	int wlen, datawlen;
	unsigned long written;
	PTPUSBBulkContainer usbdata;
	uint32_t bytes_left_to_transfer;
	PTPDataHandler memhandler;

	/* build appropriate USB container */
	usbdata.length	= htod32(PTP_USB_BULK_HDR_LEN+size);
	usbdata.type	= htod16(PTP_USB_CONTAINER_DATA);
	usbdata.code	= htod16(ptp->Code);
	usbdata.trans_id= htod32(ptp->Transaction_ID);
  
	((PTP_USB*)params->data)->current_transfer_complete = 0;
	((PTP_USB*)params->data)->current_transfer_total = size+PTP_USB_BULK_HDR_LEN;

	if (params->split_header_data) {
		datawlen = 0;
		wlen = PTP_USB_BULK_HDR_LEN;
	} else {
		unsigned long gotlen;
		/* For all camera devices. */
		datawlen = (size<PTP_USB_BULK_PAYLOAD_LEN_WRITE)?size:PTP_USB_BULK_PAYLOAD_LEN_WRITE;
		wlen = PTP_USB_BULK_HDR_LEN + datawlen;
		ret = handler->getfunc(params, handler->private, datawlen, usbdata.payload.data, &gotlen);
		if (ret != PTP_RC_OK)
			return ret;
		if (gotlen != datawlen)
			return PTP_RC_GeneralError;
	}
	ptp_init_send_memory_handler (&memhandler, (unsigned char *)&usbdata, wlen);
	/* send first part of data */
	ret = ptp_write_func(wlen, &memhandler, params->data, &written);
	ptp_exit_send_memory_handler (&memhandler);
	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
/*		ptp_error (params,
		"PTP: request code 0x%04x sending data error 0x%04x",
			ptp->Code,ret);*/
		return ret;
	}
	if (size <= datawlen) return ret;
	/* if everything OK send the rest */
	bytes_left_to_transfer = size-datawlen;
	ret = PTP_RC_OK;
	while(bytes_left_to_transfer > 0) {
		ret = ptp_write_func (bytes_left_to_transfer, handler, params->data, &written);
		if (ret != PTP_RC_OK)
			break;
		if (written == 0) {
			ret = PTP_ERROR_IO;
			break;
		}
		bytes_left_to_transfer -= written;
	}
	if (ret!=PTP_RC_OK)
		ret = PTP_ERROR_IO;
	return ret;
}

static uint16_t ptp_usb_getpacket(PTPParams *params,
		PTPUSBBulkContainer *packet, unsigned long *rlen)
{
	PTPDataHandler	memhandler;
	uint16_t	ret;
	unsigned char	*x = NULL;

	/* read the header and potentially the first data */
	if (params->response_packet_size > 0) {
		/* If there is a buffered packet, just use it. */
		memcpy(packet, params->response_packet, params->response_packet_size);
		*rlen = params->response_packet_size;
		free(params->response_packet);
		params->response_packet = NULL;
		params->response_packet_size = 0;
		/* Here this signifies a "virtual read" */
		return PTP_RC_OK;
	}
	ptp_init_recv_memory_handler (&memhandler);
	ret = ptp_read_func(PTP_USB_BULK_HS_MAX_PACKET_LEN_READ, &memhandler, params->data, rlen, 0);
	ptp_exit_recv_memory_handler (&memhandler, &x, rlen);
	if (x) {
		memcpy (packet, x, *rlen);
		free (x);
	}
	return ret;
}

uint16_t
ptp_usb_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *handler)
{
	uint16_t ret;
	PTPUSBBulkContainer usbdata;
	unsigned char	*data;
	unsigned long	written;

	memset(&usbdata,0,sizeof(usbdata));
	do {
		unsigned long len, rlen;

		ret = ptp_usb_getpacket(params, &usbdata, &rlen);
		if (ret!=PTP_RC_OK) {
			ret = PTP_ERROR_IO;
			break;
		} else
		if (dtoh16(usbdata.type)!=PTP_USB_CONTAINER_DATA) {
			ret = PTP_ERROR_DATA_EXPECTED;
			break;
		} else
		if (dtoh16(usbdata.code)!=ptp->Code) {
			ret = dtoh16(usbdata.code);
			break;
		}
		if (usbdata.length == 0xffffffffU) {
			/* stuff data directly to passed data handler */
			while (1) {
				unsigned long readdata;
				int xret;

				xret = ptp_read_func(
					PTP_USB_BULK_HS_MAX_PACKET_LEN_READ,
					handler,
					params->data,
					&readdata,
					0
				);
				if (xret == -1)
					return PTP_ERROR_IO;
				if (readdata < PTP_USB_BULK_HS_MAX_PACKET_LEN_READ)
					break;
			}
			return PTP_RC_OK;
		}
		if (rlen > dtoh32(usbdata.length)) {
			/*
			 * Buffer the surplus response packet if it is >=
			 * PTP_USB_BULK_HDR_LEN
			 * (i.e. it is probably an entire package)
			 * else discard it as erroneous surplus data.
			 * This will even work if more than 2 packets appear
			 * in the same transaction, they will just be handled
			 * iteratively.
			 *
			 * Marcus observed stray bytes on iRiver devices;
			 * these are still discarded.
			 */
			unsigned int packlen = dtoh32(usbdata.length);
			unsigned int surplen = rlen - packlen;

			if (surplen >= PTP_USB_BULK_HDR_LEN) {
				params->response_packet = malloc(surplen);
				memcpy(params->response_packet,
				       (uint8_t *) &usbdata + packlen, surplen);
				params->response_packet_size = surplen;
			/* Ignore reading one extra byte if device flags have been set */
			} else if( !(((PTP_USB *)params->data)->device_flags &
																						DEVICE_FLAG_NO_ZERO_READS &&
									rlen - dtoh32(usbdata.length) == 1)) {
				ptp_debug (params, "ptp2/ptp_usb_getdata: read %d bytes "
					   "too much, expect problems!", 
					   rlen - dtoh32(usbdata.length));
			}
			rlen = packlen;
		}

		/* For most PTP devices rlen is 512 == sizeof(usbdata)
		 * here. For MTP devices splitting header and data it might
		 * be 12.
		 */
		/* Evaluate full data length. */
		len=dtoh32(usbdata.length)-PTP_USB_BULK_HDR_LEN;

		/* autodetect split header/data MTP devices */
		if (dtoh32(usbdata.length) > 12 && (rlen==12))
			params->split_header_data = 1;

		data = malloc(PTP_USB_BULK_HS_MAX_PACKET_LEN_READ);
		/* Copy first part of data to 'data' */
		handler->putfunc(
			params, handler->private, rlen - PTP_USB_BULK_HDR_LEN, usbdata.payload.data,
			&written
		);
    
		if (((PTP_USB *)params->data)->device_flags & DEVICE_FLAG_NO_ZERO_READS && 
		    len+PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ) {
#ifdef ENABLE_USB_BULK_DEBUG
		  printf("Reading in extra terminating byte\n");
#endif
		  // need to read in extra byte and discard it
		  int result = 0;
		  char byte = 0;
		  PTP_USB *ptp_usb = (PTP_USB *)params->data;
		  result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, &byte, 1, ptpcam_usb_timeout);
		  
		  if (result != 1)
		    printf("Could not read in extra byte for PTP_USB_BULK_HS_MAX_PACKET_LEN_READ long file, return value 0x%04x\n", result);
		} else if (len+PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ && params->split_header_data == 0) {
#ifdef ENABLE_USB_BULK_DEBUG
      printf("Reading in zero packet after header\n");
#endif
      int zeroresult = 0;
      char zerobyte = 0;
      PTP_USB *ptp_usb = (PTP_USB *)params->data;
      zeroresult = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, &zerobyte, 0, ptpcam_usb_timeout);
      
      if (zeroresult != 0)
        printf("LIBMTP panic: unable to read in zero packet, response 0x%04x", zeroresult);
    }
    
		/* Is that all of data? */
		if (len+PTP_USB_BULK_HDR_LEN<=rlen) break;
		
		ret=ptp_read_func(len - (rlen - PTP_USB_BULK_HDR_LEN),
				  handler,
				  params->data, &rlen, 1);
		
		if (ret!=PTP_RC_OK) {
		  ret = PTP_ERROR_IO;
		  break;
		}
	} while (0);
	/*
	if (ret!=PTP_RC_OK) {
		ptp_error (params,
		"PTP: request code 0x%04x getting data error 0x%04x",
			ptp->Code, ret);
	}*/
	return ret;
}

uint16_t
ptp_usb_getresp (PTPParams* params, PTPContainer* resp)
{
	uint16_t ret;
	unsigned long rlen;
	PTPUSBBulkContainer usbresp;

	memset(&usbresp,0,sizeof(usbresp));
	/* read response, it should never be longer than sizeof(usbresp) */
	ret = ptp_usb_getpacket(params, &usbresp, &rlen);

	if (ret!=PTP_RC_OK) {
		ret = PTP_ERROR_IO;
	} else
	if (dtoh16(usbresp.type)!=PTP_USB_CONTAINER_RESPONSE) {
		ret = PTP_ERROR_RESP_EXPECTED;
	} else
	if (dtoh16(usbresp.code)!=resp->Code) {
		ret = dtoh16(usbresp.code);
	}
	if (ret!=PTP_RC_OK) {
/*		ptp_error (params,
		"PTP: request code 0x%04x getting resp error 0x%04x",
			resp->Code, ret);*/
		return ret;
	}
	/* build an appropriate PTPContainer */
	resp->Code=dtoh16(usbresp.code);
	resp->SessionID=params->session_id;
	resp->Transaction_ID=dtoh32(usbresp.trans_id);
	resp->Param1=dtoh32(usbresp.payload.params.param1);
	resp->Param2=dtoh32(usbresp.payload.params.param2);
	resp->Param3=dtoh32(usbresp.payload.params.param3);
	resp->Param4=dtoh32(usbresp.payload.params.param4);
	resp->Param5=dtoh32(usbresp.payload.params.param5);
	return ret;
}

/* Event handling functions */

/* PTP Events wait for or check mode */
#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */

static inline uint16_t
ptp_usb_event (PTPParams* params, PTPContainer* event, int wait)
{
	uint16_t ret;
	int result;
	unsigned long rlen;
	PTPUSBEventContainer usbevent;
	PTP_USB *ptp_usb = (PTP_USB *)(params->data);

	memset(&usbevent,0,sizeof(usbevent));

	if ((params==NULL) || (event==NULL)) 
		return PTP_ERROR_BADPARAM;
	ret = PTP_RC_OK;
	switch(wait) {
	case PTP_EVENT_CHECK:
		result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)&usbevent,sizeof(usbevent),ptpcam_usb_timeout);
		if (result==0)
			result = USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *) &usbevent, sizeof(usbevent), ptpcam_usb_timeout);
		if (result < 0) ret = PTP_ERROR_IO;
		break;
	case PTP_EVENT_CHECK_FAST:
		result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)&usbevent,sizeof(usbevent),ptpcam_usb_timeout);
		if (result==0)
			result = USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *) &usbevent, sizeof(usbevent), ptpcam_usb_timeout);
		if (result < 0) ret = PTP_ERROR_IO;
		break;
	default:
		ret=PTP_ERROR_BADPARAM;
		break;
	}
	if (ret!=PTP_RC_OK) {
		ptp_error (params,
			"PTP: reading event an error 0x%04x occurred", ret);
		return PTP_ERROR_IO;
	}
	rlen = result;
	if (rlen < 8) {
		ptp_error (params,
			"PTP: reading event an short read of %ld bytes occurred", rlen);
		return PTP_ERROR_IO;
	}
	/* if we read anything over interrupt endpoint it must be an event */
	/* build an appropriate PTPContainer */
	event->Code=dtoh16(usbevent.code);
	event->SessionID=params->session_id;
	event->Transaction_ID=dtoh32(usbevent.trans_id);
	event->Param1=dtoh32(usbevent.param1);
	event->Param2=dtoh32(usbevent.param2);
	event->Param3=dtoh32(usbevent.param3);
	return ret;
}

uint16_t
ptp_usb_event_check (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_usb_event_wait (PTPParams* params, PTPContainer* event) {

	return ptp_usb_event (params, event, PTP_EVENT_CHECK);
}


static int init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
{
  usb_dev_handle *device_handle;
  
  params->error_func=NULL;
  params->debug_func=NULL;
  params->sendreq_func=ptp_usb_sendreq;
  params->senddata_func=ptp_usb_senddata;
  params->getresp_func=ptp_usb_getresp;
  params->getdata_func=ptp_usb_getdata;
  params->data=ptp_usb;
  params->transaction_id=0;
  /*
   * This is hardcoded here since we have no devices whatsoever that are BE.
   * Change this the day we run into our first BE device (if ever).
   */
  params->byteorder = PTP_DL_LE;
  
  if ((device_handle = usb_open(dev))){
    if (!device_handle) {
      perror("usb_open()");
      return -1;
    }
    ptp_usb->handle = device_handle;
#ifdef LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
    /*
     * If this device is known to be wrongfully claimed by other kernel
     * drivers (such as mass storage), then try to unload it to make it
     * accessible from user space.
     */
    if (ptp_usb->device_flags & DEVICE_FLAG_UNLOAD_DRIVER) {
      if (usb_detach_kernel_driver_np(device_handle, dev->config->interface->altsetting->bInterfaceNumber)) {
	// Totally ignore this error!
	// perror("usb_detach_kernel_driver_np()");
      }
    }
#endif
#ifdef __WIN32__
    // Only needed on Windows, and cause problems on other platforms.
    if (usb_set_configuration(device_handle, dev->config->bConfigurationValue)) {
      perror("usb_set_configuration()");
      return -1;
    }
#endif
    if (usb_claim_interface(device_handle, dev->config->interface->altsetting->bInterfaceNumber)) {
      perror("usb_claim_interface()");
      return -1;
    }
    ptp_usb->interface = dev->config->interface->altsetting->bInterfaceNumber;
  }
  return 0;
}

static void clear_stall(PTP_USB* ptp_usb)
{
  uint16_t status;
  int ret;
  
  /* check the inep status */
  status = 0;
  ret = usb_get_endpoint_status(ptp_usb,ptp_usb->inep,&status);
  if (ret<0) {
    perror ("inep: usb_get_endpoint_status()");
  } else if (status) {
    printf("Clearing stall on IN endpoint\n");
    ret = usb_clear_stall_feature(ptp_usb,ptp_usb->inep);
    if (ret<0) {
      perror ("usb_clear_stall_feature()");
    }
  }
  
  /* check the outep status */
  status=0;
  ret = usb_get_endpoint_status(ptp_usb,ptp_usb->outep,&status);
  if (ret<0) {
    perror("outep: usb_get_endpoint_status()");
  } else if (status) {
    printf("Clearing stall on OUT endpoint\n");
    ret = usb_clear_stall_feature(ptp_usb,ptp_usb->outep);
    if (ret<0) {
      perror("usb_clear_stall_feature()");
    }
  }

  /* TODO: do we need this for INTERRUPT (ptp_usb->intep) too? */
}

static void clear_halt(PTP_USB* ptp_usb)
{
  int ret;

  ret = usb_clear_halt(ptp_usb->handle,ptp_usb->inep);
  if (ret<0) {
    perror("usb_clear_halt() on IN endpoint");
  }
  ret = usb_clear_halt(ptp_usb->handle,ptp_usb->outep);
  if (ret<0) {
    perror("usb_clear_halt() on OUT endpoint");
  }
  ret = usb_clear_halt(ptp_usb->handle,ptp_usb->intep);
  if (ret<0) {
    perror("usb_clear_halt() on INTERRUPT endpoint");
  }
}

static void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber)
{
  // Clear any stalled endpoints
  clear_stall(ptp_usb);
  // Clear halts on any endpoints
  clear_halt(ptp_usb);
  // Added to clear some stuff on the OUT endpoint
  // TODO: is this good on the Mac too?
  usb_resetep(ptp_usb->handle, ptp_usb->outep);
  usb_release_interface(ptp_usb->handle, interfaceNumber);
  // Brutally reset device
  // TODO: is this good on the Mac too?
  usb_reset(ptp_usb->handle);
  usb_close(ptp_usb->handle);
}

static LIBMTP_error_number_t prime_device_memory(mtpdevice_list_t *devlist)
{
  mtpdevice_list_t *tmplist = devlist;
 
  while (tmplist != NULL) {
    /* Allocate a parameter box */
    tmplist->params = (PTPParams *) malloc(sizeof(PTPParams));
    tmplist->ptp_usb = (PTP_USB *) malloc(sizeof(PTP_USB));
    
    /* Check for allocation Error */
    if(tmplist->params == NULL || tmplist->ptp_usb == NULL) {
      /* Error and deallocation of memory will be handled by caller. */
      return LIBMTP_ERROR_MEMORY_ALLOCATION;
    }
    
    /* Start with a blank slate (includes setting device_flags to 0) */
    memset(tmplist->params, 0, sizeof(PTPParams));
    memset(tmplist->ptp_usb, 0, sizeof(PTP_USB));
    tmplist = tmplist->next;
  }
  return LIBMTP_ERROR_NONE;
}

static void assign_known_device_flags(mtpdevice_list_t *devlist)
{
  int i;
  mtpdevice_list_t *tmplist = devlist;
  uint8_t current_device = 0;
  
  /* Search through known device list and set correct device flags */
  while (tmplist != NULL) {
    int device_known = 0;
    
    for(i = 0; i < mtp_device_table_size; i++) {
      if(tmplist->libusb_device->descriptor.idVendor == mtp_device_table[i].vendor_id &&
	 tmplist->libusb_device->descriptor.idProduct == mtp_device_table[i].product_id) {
	/* This device is known, assign the correct device flags */
	/* Note that ptp_usb[current_device] could potentially be NULL */
	if(tmplist->ptp_usb != NULL) {
	  tmplist->ptp_usb->device_flags =  mtp_device_table[i].device_flags;
	  
	  /*
	   *  TODO:
	   *	Preferable to not do this with #ifdef ENABLE_USB_BULK_DEBUG but there is 
	   *	currently no other compile time debug option
	   */
	  
	  device_known = 1;
#ifdef ENABLE_USB_BULK_DEBUG
	  /* This device is known to the developers */
	  fprintf(stderr, "Device %d (VID=%04x and PID=%04x) is a %s.\n", 
		  current_device + 1,
		  tmplist->libusb_device->descriptor.idVendor,
		  tmplist->libusb_device->descriptor.idProduct,
		  mtp_device_table[i].name);
#endif
	}
	break;
      }
    }
    if (!device_known) {
      /* This device is unknown to the developers */
      fprintf(stderr, "Device %d (VID=%04x and PID=%04x) is UNKNOWN.\n", 
	      current_device + 1,
	      tmplist->libusb_device->descriptor.idVendor,
	      tmplist->libusb_device->descriptor.idProduct);
      fprintf(stderr, "Please report this VID/PID and the device model to the "
	      "libmtp development team\n");
    }
    tmplist = tmplist->next;
    current_device++;
  }
}


static LIBMTP_error_number_t configure_usb_devices(mtpdevice_list_t *devicelist)
{
  mtpdevice_list_t *tmplist = devicelist;
  uint16_t ret = 0;
  uint8_t current_device = 0;
  
  while (tmplist != NULL) {
    /* This is erroneous, there must be a PTP_USB instance that we can initialize. */
    if(tmplist->ptp_usb == NULL) {
      return LIBMTP_ERROR_MEMORY_ALLOCATION;
    }
    
    /* TODO: Will this always be little endian? */
    tmplist->params->byteorder = PTP_DL_LE;
    tmplist->params->cd_locale_to_ucs2 = iconv_open("UCS-2LE", "UTF-8");
    tmplist->params->cd_ucs2_to_locale = iconv_open("UTF-8", "UCS-2LE");
    
    if(tmplist->params->cd_locale_to_ucs2 == (iconv_t) -1 ||
       tmplist->params->cd_ucs2_to_locale == (iconv_t) -1) {
      fprintf(stderr, "LIBMTP PANIC: Cannot open iconv() converters to/from UCS-2!\n"
	      "Too old stdlibc, glibc and libiconv?\n");
      return LIBMTP_ERROR_CONNECTING;
    }
    
    // ep = device->config->interface->altsetting->endpoint;
    // no_of_ep = device->config->interface->altsetting->bNumEndpoints;
  
    /* Assign endpoints to usbinfo... */
    find_endpoints(tmplist->libusb_device,
		   &tmplist->ptp_usb->inep,
		   &tmplist->ptp_usb->inep_maxpacket,
		   &tmplist->ptp_usb->outep,
		   &tmplist->ptp_usb->outep_maxpacket,
		   &tmplist->ptp_usb->intep);
    
    /* Attempt to initialize this device */
    if (init_ptp_usb(tmplist->params, tmplist->ptp_usb, tmplist->libusb_device) < 0) {
      fprintf(stderr, "LIBMTP PANIC: Unable to initialize device %d\n", current_device+1);
      // FIXME: perhaps use "continue" to keep trying the other devices.
      return LIBMTP_ERROR_CONNECTING;
    }
  
    /*
     * This works in situations where previous bad applications
     * have not used LIBMTP_Release_Device on exit 
     */
    if ((ret = ptp_opensession(tmplist->params, 1)) == PTP_ERROR_IO) {
      fprintf(stderr, "PTP_ERROR_IO: Trying again after resetting USB\n");
      close_usb(tmplist->ptp_usb, tmplist->libusb_device->config->interface->altsetting->bInterfaceNumber);
      
      if(init_ptp_usb(tmplist->params, tmplist->ptp_usb, tmplist->libusb_device) <0) {
	fprintf(stderr, "LIBMTP PANIC: Could not open session on device %d\n", current_device+1);
	return LIBMTP_ERROR_CONNECTING;
      }
	
      /* Device has been reset, try again */
      ret = ptp_opensession(tmplist->params, 1);
    }
  
    /* Was the transaction id invalid? Try again */
    if (ret == PTP_RC_InvalidTransactionID) {
      fprintf(stderr, "LIBMTP WARNING: Transaction ID was invalid, increment and try again\n");
      tmplist->params->transaction_id += 10;
      ret = ptp_opensession(tmplist->params, 1);
    }

    if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK) {
      fprintf(stderr, "LIBMTP PANIC: Could not open session! "
	      "(Return code %d)\n  Try to reset the device.\n",
	      ret);
      usb_release_interface(tmplist->ptp_usb->handle,
			    tmplist->libusb_device->config->interface->altsetting->bInterfaceNumber);
      return LIBMTP_ERROR_CONNECTING;
    }
  
    /* It is permissible to call this before opening the session */
    if (ptp_getdeviceinfo(tmplist->params,
			  &tmplist->params->deviceinfo) != PTP_RC_OK) {
      fprintf(stderr, "LIBMTP PANIC: Could not get device info!\n");
      usb_release_interface(tmplist->ptp_usb->handle,
			    tmplist->libusb_device->config->interface->altsetting->bInterfaceNumber);
      return LIBMTP_ERROR_CONNECTING;
    }

    tmplist = tmplist->next;
    current_device++;
  }

  /* Exit with the nice list */
  return LIBMTP_ERROR_NONE;
}

/**
 * This function scans through the results of the get_mtp_usb_device_list
 * function and attempts to connect to those devices listed using the 
 * mtp_device_table at the top of the file. Returns a LIBMTP_error_number_t.
 * 
 * @param devlist a list of devices with primed PTP_USB and params structs.
 * @return Error Codes as per the type definition
 */ 
LIBMTP_error_number_t find_usb_devices(mtpdevice_list_t **devlist)
{
  mtpdevice_list_t *mtp_device_list = NULL;
  LIBMTP_error_number_t ret;

  /*
   * Recover list of attached USB devices that match MTP criteria, i.e.
   * it either has an MTP device descriptor or it is in the known
   * devices list.
   */
  ret = get_mtp_usb_device_list (&mtp_device_list);
  if (ret != LIBMTP_ERROR_NONE) {
    return ret;
  }
  
  // Then prime them
  ret = prime_device_memory(mtp_device_list);
  if(ret) {
    fprintf(stderr, "LIBMTP PANIC: prime_device_memory() error code: %d on line %d\n", ret, __LINE__);
    goto find_usb_devices_error_exit;
  }

  /* Assign specific device flags and detect unknown devices */
  assign_known_device_flags(mtp_device_list);
  
  /* Configure the devices */
  ret = configure_usb_devices(mtp_device_list);
  if(ret) {
    fprintf(stderr, "LIBMTP PANIC: configure_usb_devices() error code: %d on line %d\n", ret, __LINE__);
    goto find_usb_devices_error_exit;
  }
  
  /* Configure interface numbers */
  {
    mtpdevice_list_t *tmplist = mtp_device_list;

    while (tmplist != NULL) {
      tmplist->interface_number = tmplist->libusb_device->config->interface->altsetting->bInterfaceNumber;
      tmplist = tmplist->next;
    }
  }
  
  /* we're connected to all devices, return the list and OK */
  *devlist = mtp_device_list;
  return LIBMTP_ERROR_NONE;
  
 find_usb_devices_error_exit:
  if(mtp_device_list != NULL) {
    free_mtpdevice_list(mtp_device_list);
    mtp_device_list = NULL;
  }
  *devlist = NULL;
  return ret;
} 

static void find_endpoints(struct usb_device *dev, int* inep, int* inep_maxpacket, int* outep, int *outep_maxpacket, int* intep)
{
  int i,n;
  struct usb_endpoint_descriptor *ep;
  
  ep = dev->config->interface->altsetting->endpoint;
  n=dev->config->interface->altsetting->bNumEndpoints;
  
  for (i=0;i<n;i++) {
    if (ep[i].bmAttributes==USB_ENDPOINT_TYPE_BULK)	{
      if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
	  USB_ENDPOINT_DIR_MASK)
	{
	  *inep=ep[i].bEndpointAddress;
	  *inep_maxpacket=ep[i].wMaxPacketSize;
	}
      if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==0)
	{
	  *outep=ep[i].bEndpointAddress;
	  *outep_maxpacket=ep[i].wMaxPacketSize;
	}
    } else if (ep[i].bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT){
      if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
	  USB_ENDPOINT_DIR_MASK)
	{
	  *intep=ep[i].bEndpointAddress;
	}
    }
  }
}

void close_device (PTP_USB *ptp_usb, PTPParams *params, uint8_t interfaceNumber)
{
  if (ptp_closesession(params)!=PTP_RC_OK)
    fprintf(stderr,"ERROR: Could not close session!\n");
  close_usb(ptp_usb, interfaceNumber);
}

static int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep)
{
  
  return (usb_control_msg(ptp_usb->handle,
			  USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, USB_FEATURE_HALT,
			  ep, NULL, 0, 3000));
}

static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status)
{
  return (usb_control_msg(ptp_usb->handle,
			  USB_DP_DTH|USB_RECIP_ENDPOINT, USB_REQ_GET_STATUS,
			  USB_FEATURE_HALT, ep, (char *)status, 2, 3000));
}
