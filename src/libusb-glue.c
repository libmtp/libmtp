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
  { "Samsung YH-925", 0x04e8, 0x502f, DEVICE_FLAG_NONE },
  // Contributed by aronvanammers on SourceForge
  { "Samsung YH-925GS", 0x04e8, 0x5024, DEVICE_FLAG_NONE },
  // Contributed by anonymous person on SourceForge
  { "Samsung YP-T7J", 0x04e8, 0x5047, DEVICE_FLAG_NONE },
  // Reported by cstrickler@gmail.com
  { "Samsung YP-U2J (YP-U2JXB/XAA)", 0x04e8, 0x5054, DEVICE_FLAG_NONE },
  // Reported by Andrew Benson
  { "Samsung YP-F2J", 0x04e8, 0x5057, DEVICE_FLAG_DUALMODE },
  // Reported by Patrick <skibler@gmail.com>
  { "Samsung YP-K5", 0x04e8, 0x505a, DEVICE_FLAG_NONE },
  // Reported by Matthew Wilcox <matthew@wil.cx>
  { "Samsung YP-T9", 0x04e8, 0x507f, DEVICE_FLAG_NONE },
  // From Paul Clinch
  { "Samsung YP-K3", 0x04e8, 0x5081, DEVICE_FLAG_NONE },
  // From a rouge .INF file
  { "Samsung YH-999 Portable Media Center", 0x04e8, 0x5a0f, DEVICE_FLAG_NONE },
  // From Lionel Bouton
  { "Samsung X830 Mobile Phone", 0x04e8, 0x6702, DEVICE_FLAG_NONE },

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

  /*
   * SanDisk
   */
  // Reported by Brian Robison
  { "SanDisk Sansa m240", 0x0781, 0x7400, DEVICE_FLAG_NONE },
  // Reported by tangent_@users.sourceforge.net
  { "SanDisk Sansa c150", 0x0781, 0x7410, DEVICE_FLAG_NONE },
  // From libgphoto2 source
  { "SanDisk Sansa e200", 0x0781, 0x7420, DEVICE_FLAG_NONE },
  // Reported by gonkflea@users.sourceforge.net
  { "SanDisk Sansa e260", 0x0781, 0x7420, DEVICE_FLAG_NONE },
  // Reported by anonymous user at sourceforge.net
  { "SanDisk Sansa c250", 0x0781, 0x7450, DEVICE_FLAG_NONE },

  /*
   * iRiver
   * we assume that PTP_OC_MTP_GetObjPropList is essentially
   * broken on all iRiver devices, meaning it simply won't return
   * all properties for a file when asking for metadata 0xffffffff. 
   * Please test on your device if you believe it isn't broken!
   * Some devices from http://www.mtp-ums.net/viewdeviceinfo.php
   */
  { "iRiver Portable Media Center", 0x1006, 0x4002, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver Portable Media Center", 0x1006, 0x4003, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // From libgphoto2 source
  { "iRiver T10", 0x4102, 0x1113, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver T20 FM", 0x4102, 0x1114, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // This appears at the MTP-UMS site
  { "iRiver T20", 0x4102, 0x1115, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver U10", 0x4102, 0x1116, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver T10", 0x4102, 0x1117, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver T20", 0x4102, 0x1118, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver T30", 0x4102, 0x1119, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // Reported by David Wolpoff
  { "iRiver T10 2GB", 0x4102, 0x1120, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // Rough guess this is the MTP device ID...
  { "iRiver N12", 0x4102, 0x1122, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // Reported by Adam Torgerson
  { "iRiver Clix", 0x4102, 0x112a, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  // Reported by Scott Call
  { "iRiver H10 20GB", 0x4102, 0x2101, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },
  { "iRiver H10", 0x4102, 0x2102, DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST },

  /*
   * Dell
   */
  { "Dell DJ Itty", 0x413c, 0x4500, DEVICE_FLAG_NONE },
  
  /*
   * Toshiba
   */
  { "Toshiba Gigabeat MEGF-40", 0x0930, 0x0009, DEVICE_FLAG_NONE },
  { "Toshiba Gigabeat", 0x0930, 0x000c, DEVICE_FLAG_NONE },
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
  {"Logik LOG DAX MP3 and DAB Player", 0x13d1, 0x7002, DEVICE_FLAG_NONE }
};
static const int mtp_device_table_size = sizeof(mtp_device_table) / sizeof(LIBMTP_device_entry_t);

int ptpcam_usb_timeout = USB_TIMEOUT;

// Local functions
static struct usb_bus* init_usb();
static void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber);
static struct usb_device* find_device (int busn, int devicen, short force);
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
 * @param dev dynamic linked list of pointers to usb devices with MTP 
 * properties.
 * @param next New USB MTP device to be added to list
 * @return nothing
 */
static void append_to_MTP_list(struct usb_device *dev,
                                struct usb_device *next)
{
  if(dev->next != NULL) {
    append_to_MTP_list(dev->next, next);
    return; 
  }
  
  dev->next = next;
  next->next = NULL;
  return;
}

/**
 * Small recursive function to free dynamic memory allocated to the linked list
 * of USB MTP devices
 * @param dev dynamic linked list of pointers to usb devices with MTP 
 * properties.
 * @return nothing
 */
static void free_MTP_list(struct usb_device *dev)
{
  if(dev->next != NULL) {
    free_MTP_list(dev->next);
  }
  
  free(dev);
  return;
}

/**
 * Check for the Microsoft OS device descriptor and return device struct
 * if the device is MTP-compliant. The function will only recognize
 * a single device connected to the USB bus.
 * @param MTPDeviceList dynamic array of pointers to usb devices with MTP 
 * properties. Be sure to call free(MTPDeviceList).
 * @param numdevices number of devices in MTPDeviceList Dynamic Array
 * @return an MTP-compliant USB device if one was found, else NULL.
 */
static LIBMTP_error_number_t get_mtp_usb_device_list(
		struct usb_device ** MTPDeviceList,
		uint8_t *numdevices)
{
  /* Initialize number of MTP USB devices to 0 */
  *numdevices = 0;

  struct usb_bus *bus = init_usb();
  for (; bus != NULL; bus = bus->next)
  {
    struct usb_device *dev = bus->devices;
    for (; dev != NULL; dev = dev->next)
    {
      usb_dev_handle *devh;
      unsigned char buf[1024], cmd;
      int ret;
      
      /* Attempt to open Device on this port */
      devh = usb_open(dev);
      if (devh == NULL)
      {
        /* Could not open this device, continue */
        continue;
      }

      /* Read the special descriptor */
      ret = usb_get_descriptor(devh, 0x03, 0xee, buf, sizeof(buf));

      /* Check if descriptor length is at least 10 bytes */
      if (ret < 10)
      {
        usb_close(devh);
        continue;
      }
      
      /* Check if this device has a Microsoft Descriptor */
      if (!((buf[2] == 'M') && (buf[4] == 'S') &&
            (buf[6] == 'F') && (buf[8] == 'T')))
      {
        usb_close(devh);
        continue;
      }
      
      /* Check if device responds to control message 1 or if there is an error*/      
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
      if (ret <= 0x15)
      {
        /* TODO: If there was an error, flag it and let the user know somehow */
        /* if(ret == -1) {} */
        usb_close(devh);
        continue;
      }
      
      /* Check if device is MTP or if it is something like a USB Mass Storage 
         device with Janus DRM support */
      if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P'))
      {
        usb_close(devh);
        continue;
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
      if (ret == -1)
      {
        /* TODO: Implement callback function to let managing program know there
           was a problem, along with description of the problem */
        fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
                        "ProductID:%04x encountered an error responding to "
                        "control message 2.\n"
                        "Problems may arrise but continuing\n",
                        dev->descriptor.idVendor, dev->descriptor.idProduct);
      }
      else if (ret <= 0x15)
      {
        /* TODO: Implement callback function to let managing program know there
           was a problem, along with description of the problem */
        fprintf(stderr, "Potential MTP Device with VendorID:%04x and "
                        "ProductID:%04x responded to control message 2 with a "
                        "response that was too short. Problems may arrise but "
                        "continuing\n",
                        dev->descriptor.idVendor, dev->descriptor.idProduct);
      }
      else if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P'))
      {
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
  
      /* Append this usb device to the MTP USB Device List */
      if(*MTPDeviceList == NULL)
      {
        *MTPDeviceList = (struct usb_device *)malloc(
                                                sizeof(struct usb_device));
        /* Check for allocation Error */
        if(*MTPDeviceList == NULL)
        {
          /* 
            * TODO: Implement callback function to let managing applications 
            * know there was a memory allocation problem
            */
          fprintf(stderr, "Memory Allocation Problem: unable to connect "
                          "MTP Device with VID:%04x and PID:%04x.\n",
                          dev->descriptor.idVendor,
                          dev->descriptor.idProduct);
          return LIBMTP_ERROR_MEMORY_ALLOCATION;
        }
        memcpy(*MTPDeviceList, dev, sizeof(struct usb_device));
        (*MTPDeviceList)->next = NULL;
        (*numdevices)++;
      }
      else
      {
        struct usb_device *tmp;
        tmp = (struct usb_device *)malloc(sizeof(struct usb_device));
        
        /* Check for allocation Error */
        if(tmp == NULL)
        {
          /* 
            * TODO: Implement callback function to let managing applications 
            * know there was a memory allocation problem
            */
          fprintf(stderr, "Memory Allocation Problem: unable to connect "
                          "MTP Device with VID:%04x and PID:%04x.\n",
                          dev->descriptor.idVendor,
                          dev->descriptor.idProduct);
          free_MTP_list(*MTPDeviceList);
          return LIBMTP_ERROR_MEMORY_ALLOCATION;
        }
        else
        {
          memcpy(tmp, dev, sizeof(struct usb_device));
          append_to_MTP_list(*MTPDeviceList, tmp);
          (*numdevices)++;
        }
      }
    }
  }

  /* If nothing was found we end up here. */
  if(*MTPDeviceList == NULL)
    return LIBMTP_ERROR_N0_DEVICE_ATTACHED;
  else
    return LIBMTP_ERROR_NONE;
}

/**
 * Detect the MTP device descriptor and return the VID and PID
 * of the first device found. This is a very low-level function
 * which is intended for use with <b>udev</b> or other hotplug
 * mechanisms. The idea is that a script may want to know if the
 * just plugged-in device was an MTP device or not.
 * 
 * FUNCTION STUBBED OUT FOR FURTHER EXAMINITATION LATER
 * 
 * @param vid the Vendor ID (VID) of the first device found.
 * @param pid the Product ID (PID) of the first device found.
 * @return the number of detected devices or -1 if the call
 *         was unsuccessful.
 */
int LIBMTP_Detect_Descriptor(uint16_t *vid, uint16_t *pid)
{
	/* TODO: Find a way to make this work with the multiple device code */
	*vid = *pid = 0;
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
 * based on same functions in library.c in libgphoto2.
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
 */
#define CONTEXT_BLOCK_SIZE	0x10000
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

  // This is the largest block we'll need to read in.
  bytes = malloc(CONTEXT_BLOCK_SIZE);
#ifdef ENABLE_USB_BULK_DEBUG
  printf("Total size to read: 0x%04x bytes\n", size);
#endif
  while (curread < size) {
    toread = size - curread;
    if (toread > CONTEXT_BLOCK_SIZE) {
      toread = CONTEXT_BLOCK_SIZE;
    } else if (toread > ptp_usb->outep_maxpacket) {
      toread -= toread % ptp_usb->outep_maxpacket;
    }

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
  if (readzero && curread % ptp_usb->outep_maxpacket == 0) {
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
	((PTP_USB*)params->data)->current_transfer_total = size;

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
			} else {
				ptp_debug (params, "ptp2/ptp_usb_getdata: read %d bytes too much, expect problems!", rlen - dtoh32(usbdata.length));
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

		/* Is that all of data? */
		if (len+PTP_USB_BULK_HDR_LEN<=rlen) break;

		/* If not read the rest of it. */
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


/*
 find_device() returns the pointer to a usb_device structure matching
 given busn, devicen numbers. If any or both of arguments are 0 then the
 first matching PTP device structure is returned. 
 */
static struct usb_device* find_device (int busn, int devn, short force)
{
  struct usb_bus *bus;
  struct usb_device *dev;
  
  bus=init_usb();
  for (; bus; bus = bus->next)
    for (dev = bus->devices; dev; dev = dev->next)
      /* somtimes dev->config is null, not sure why... */
      if (dev->config != NULL)
	if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB)
	  {
	    int curbusn, curdevn;
	    
	    curbusn=strtol(bus->dirname,NULL,10);
	    curdevn=strtol(dev->filename,NULL,10);
	    
	    if (devn==0) {
	      if (busn==0) return dev;
	      if (curbusn==busn) return dev;
	    } else {
	      if ((busn==0)&&(curdevn==devn)) return dev;
	      if ((curbusn==busn)&&(curdevn==devn)) return dev;
	    }
	  }
  return NULL;
}

/**
 * This function scans through the connected usb devices on a machine and
 * if they match known Vendor and Product identifiers appends them to the
 * dynamic array MTPDeviceList.  Be sure to call 
 * <code>free(MTPDeviceList)</code> when you are done with it, assuming it
 * is not NULL.
 *
 * @param MTPDeviceList dynamic array of pointers to usb devices known to
 * be valid MTP devices
 * @param numdevices pointer to the value representing the number of devices
 * being returned through MTPDeviceList
 * @return LIBMTP_ERROR_NONE implies that devices have been found, scan the list
 * appropriately. LIBMTP_ERROR_N0_DEVICE_ATTACHED implies that no devices have
 * been found. LIBMTP_ERROR_MEMORY_ALLOCATION states that there has been a
 * memory allocation error, free any dynamically allocated memory and return
 * this value.
*/
static LIBMTP_error_number_t get_mtp_usb_known_devices(
    struct usb_device ** MTPDeviceList,
    uint8_t *numdevices)
{
  /* Scan through all attached usb and devices */
  struct usb_bus *bus = init_usb();
  for(; bus != NULL; bus = bus->next)
  {
    struct usb_device *dev = bus->devices;
    for(; dev != NULL; dev = dev->next)
    {
      const LIBMTP_device_entry_t * device = mtp_device_table;
      int i = 0;
    
      /* Loop over the list of supported devices */
      while(i++ < mtp_device_table_size)
      {  	  
    	  if (dev->descriptor.bDeviceClass != USB_CLASS_HUB && 
  	      dev->descriptor.idVendor == device->vendor_id &&
  	      dev->descriptor.idProduct == device->product_id )
  	    {
          /* Append this usb device to the MTP USB Device List */
          if(*MTPDeviceList == NULL)
          {
            *MTPDeviceList = (struct usb_device *)malloc(
                                                    sizeof(struct usb_device));
            /* Check for allocation Error */
            if(*MTPDeviceList == NULL)
            {
              /* 
               * TODO: Implement callback function to let managing applications 
               * know there was a memory allocation problem
               */
              fprintf(stderr, "Memory Allocation Problem: unable to connect "
                              "MTP Device with VID:%04x and PID:%04x.",
                              dev->descriptor.idVendor,
                              dev->descriptor.idProduct);
              return LIBMTP_ERROR_MEMORY_ALLOCATION;
            }
            memcpy(*MTPDeviceList, dev, sizeof(struct usb_device));
            (*MTPDeviceList)->next = NULL;
            (*numdevices)++;
          }
          else
          {
            struct usb_device *tmp;
            tmp = (struct usb_device *)malloc(sizeof(struct usb_device));
            
            /* Check for allocation Error */
            if(tmp == NULL)
            {
              /* 
               * TODO: Implement callback function to let managing applications 
               * know there was a memory allocation problem
               */
              fprintf(stderr, "Memory Allocation Problem: unable to connect "
                              "MTP Device with VID:%04x and PID:%04x.",
                              dev->descriptor.idVendor,
                              dev->descriptor.idProduct);
              free_MTP_list(*MTPDeviceList);
              return LIBMTP_ERROR_MEMORY_ALLOCATION;
            }
            else
            {
              memcpy(tmp, dev, sizeof(struct usb_device));
              append_to_MTP_list(*MTPDeviceList, tmp);
              (*numdevices)++;
            }
          }
          
          /* Found this device, continue with search for more devices */
          device++;
          break;
  	    }  	    
  	  }
  	}
  }
  
  /* If nothing was found we end up here. */
  if(*MTPDeviceList == NULL)
    return LIBMTP_ERROR_N0_DEVICE_ATTACHED;
  else
    return LIBMTP_ERROR_NONE;
}

static LIBMTP_error_number_t prime_device_memory(PTPParams *params[],
                                            PTP_USB *ptp_usb[],
                                            uint8_t numdevices,
                                            uint8_t current_device)
{
  if(current_device < numdevices)
  {
    /* Allocate a parameter box */
    params[current_device] = (PTPParams *) malloc(sizeof(PTPParams));
    ptp_usb[current_device] = (PTP_USB *) malloc(sizeof(PTP_USB));

    /* Check for allocation Error */
    if(params[current_device] == NULL || ptp_usb[current_device] == NULL)
    {
      /* Prevent Memory Leaks */
      if(params[current_device] != NULL)
      {
        free(params[current_device]);
        params[current_device] = NULL;
      }
        
      if(ptp_usb[current_device] != NULL)
      {
        free(ptp_usb[current_device]);
        ptp_usb[current_device] = NULL;
      }
      
      /* This device has not been allocated but try to continue */
      prime_device_memory(params, ptp_usb, numdevices, current_device+1);
      return LIBMTP_ERROR_MEMORY_ALLOCATION;
    }
    
    /* Start with a blank slate (includes setting device_flags to 0) */
    memset(params[current_device], 0, sizeof(PTPParams));
    memset(ptp_usb[current_device], 0, sizeof(PTP_USB));

    /* This device has been allocated, continue with next device */
    return prime_device_memory(params, ptp_usb, numdevices, current_device+1);
  }

  /* This is the recursive exit command */  
  return LIBMTP_ERROR_NONE;
}

static void assign_known_device_flags(struct usb_device *dev,
                                       PTP_USB *ptp_usb[],
                                       uint8_t current_device)
{
  if(dev == NULL)
    return;
  
  /* Search through known device list and set correct device flags */
  int j;
  for(j = 0; j < mtp_device_table_size; j++)
  {
    if(dev->descriptor.idVendor ==
                    mtp_device_table[j].vendor_id &&
        dev->descriptor.idProduct ==
                    mtp_device_table[j].product_id)
    {
    	/* This device is known, assign the correct device flags */
    	/* Note that ptp_usb[current_device] could potentially be NULL */
    	if(ptp_usb[current_device] != NULL)
    	{
      	ptp_usb[current_device]->device_flags = 
    	                  mtp_device_table[j].device_flags;

        /* This device is known to the developers */
        fprintf(stderr, "Device %d (VID=%04x and PID=%04x) is a %s.\n", 
                        current_device + 1,
                        dev->descriptor.idVendor,
                        dev->descriptor.idProduct,
                        mtp_device_table[j].name);
    	}
      
      /* Start the next recursion */
      assign_known_device_flags(dev->next, ptp_usb, current_device + 1);
      return;
    }
  }

  /* This device is unknown to the developers */
  fprintf(stderr, "Device %d (VID=%04x and PID=%04x) is UNKNOWN.\n", 
          current_device + 1,
          dev->descriptor.idVendor,
          dev->descriptor.idProduct);
  fprintf(stderr, "Please report this VID/PID and the device model to the "
                  "libmtp development team\n");

  /* Start the next recursion */
  assign_known_device_flags(dev->next, ptp_usb, current_device + 1);

  return;
}

LIBMTP_error_number_t configure_usb_devices(struct usb_device *device,
                                            PTPParams *params[],
                                            PTP_USB *ptp_usb[],
                                            uint8_t current_device)
{
  struct usb_endpoint_descriptor *ep;
  uint16_t ret=0;
  int n;

  /* Exit Condition */
  if(device == NULL)
    return LIBMTP_ERROR_NONE;
  
  /* This device will not be configured but try to continue */
  if(ptp_usb[current_device] == NULL)
  {
    configure_usb_devices(device->next, params, ptp_usb, current_device + 1);
    return LIBMTP_ERROR_MEMORY_ALLOCATION;
  }

  /* TODO: Will this always be little endian? */
  params[current_device]->byteorder = PTP_DL_LE;
  params[current_device]->cd_locale_to_ucs2 = iconv_open("UCS-2LE", "UTF-8");
  params[current_device]->cd_ucs2_to_locale = iconv_open("UTF-8", "UCS-2LE");
  
  if(params[current_device]->cd_locale_to_ucs2 == (iconv_t) -1 ||
        params[current_device]->cd_ucs2_to_locale == (iconv_t) -1)
  {
    fprintf(stderr, "LIBMTP: Cannot open iconv() converters to/from UCS-2!\n"
                    "Too old stdlibc, glibc and libiconv?\n");

    if(params[current_device] != NULL)
    {
      free(params[current_device]);
      params[current_device] = NULL;
    }
    
    if(ptp_usb[current_device] != NULL)
    {
      free(ptp_usb[current_device]);
      ptp_usb[current_device] = NULL;
    }
 
    configure_usb_devices(device->next, params, ptp_usb, current_device + 1);
    return LIBMTP_ERROR_CONNECTING;
  }
    
  ep = device->config->interface->altsetting->endpoint;
  n = device->config->interface->altsetting->bNumEndpoints;
  
  /* Assign endpoints to usbinfo... */
  find_endpoints(device,
                &ptp_usb[current_device]->inep,
                &ptp_usb[current_device]->inep_maxpacket,
                &ptp_usb[current_device]->outep,
                &ptp_usb[current_device]->outep_maxpacket,
                &ptp_usb[current_device]->intep);
  
  fprintf(stderr, "Attempt to initialize device %d\n", current_device+1);
  /* Attempt to initialize this device, if unable, then try next device */
  if (init_ptp_usb(params[current_device], ptp_usb[current_device], device) < 0)
  {
    if(params[current_device] != NULL)
    {
      free(params[current_device]);
      params[current_device] = NULL;
    }
    
    if(ptp_usb[current_device] != NULL)
    {
      free(ptp_usb[current_device]);
      ptp_usb[current_device] = NULL;
    }
 
    fprintf(stderr, "Error: Unable to initialize device %d\n", current_device+1);
    configure_usb_devices(device->next, params, ptp_usb, current_device + 1);
    return LIBMTP_ERROR_CONNECTING;
  }
  fprintf(stderr, "Device %d initialized\n", current_device+1);
  
  /* This works in situations where previous bad applications
      have not used LIBMTP_Release_Device on exit */
  fprintf(stderr, "Try to open session for Device %d\n", current_device+1);
  if ((ret = ptp_opensession(params[current_device], 1)) == PTP_ERROR_IO)
  {
  	fprintf(stderr, "PTP ERROR IO: Trying again after resetting USB\n");
    close_usb(ptp_usb[current_device],
          device->config->interface->altsetting->bInterfaceNumber);
    
    if(init_ptp_usb(params[current_device], ptp_usb[current_device], device) <0)
    {
      if(params[current_device] != NULL)
      {
        free(params[current_device]);
        params[current_device] = NULL;
      }
      
      if(ptp_usb[current_device] != NULL)
      {
        free(ptp_usb[current_device]);
        ptp_usb[current_device] = NULL;
      }
   
      fprintf(stderr, "Error: Reinitialize device %d failed\n", current_device+1);
      configure_usb_devices(device->next, params, ptp_usb, current_device + 1);
      return LIBMTP_ERROR_CONNECTING;
    }
	
	  /* Device has been reset, try again */
	  fprintf(stderr, "Try to open session on Device %d again\n", current_device+1);
	  ret = ptp_opensession(params[current_device], 1);
	}
	
	/* Was the transaction id invalid? Try again */
  if (ret == PTP_RC_InvalidTransactionID)
  {
    fprintf(stderr, "Transaction ID was invalid, increment and try again\n");
    params[current_device]->transaction_id += 10;
    ret = ptp_opensession(params[current_device], 1);
  }

  if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK)
  {
    fprintf(stderr, "Could not open session! "
                    "(Return code %d)\n  Try to reset the device.\n",
                    ret);
    usb_release_interface(ptp_usb[current_device]->handle,
            device->config->interface->altsetting->bInterfaceNumber);
    return LIBMTP_ERROR_CONNECTING;
  }
  else
  {
    fprintf(stderr, "Session for Device %d opened\n", current_device + 1);
  }
  
  /* It is permissible to call this before opening the session */
  if (ptp_getdeviceinfo(params[current_device],
                            &params[current_device]->deviceinfo) != PTP_RC_OK)
  {
    fprintf(stderr, "Could not get device info!\n");
    usb_release_interface(ptp_usb[current_device]->handle,
        device->config->interface->altsetting->bInterfaceNumber);
    
    /* Give up on this device and try with the next */
    if(params[current_device] != NULL)
    {
      free(params[current_device]);
      params[current_device] = NULL;
    }
    
    if(ptp_usb[current_device] != NULL)
    {
      free(ptp_usb[current_device]);
      ptp_usb[current_device] = NULL;
    }
  
    configure_usb_devices(device->next, params, ptp_usb, current_device + 1);
    return LIBMTP_ERROR_CONNECTING;
  } 

  return configure_usb_devices(device->next, params, ptp_usb,current_device+1);
}

/**
 * This function scans through the results of the get_mtp_usb_device_list
 * function and attempts to connect to those devices listed using the 
 * mtp_device_table at the top of the file. Returns a LIBMTP_error_number_t.
 * 
 * @param params Dynamic array of USB parameters
 * @param ptp_usb USB information
 * @param interfaceNumber USB interface number (static size 256 bytes)
 * @param numdevices number of devices connected to the machine 
 * @return Error Codes as per the type definition
 */ 
LIBMTP_error_number_t find_usb_devices (PTPParams ***params,
                                            PTP_USB ***ptp_usb,
                                            uint8_t interfaceNumber[],
                                            uint8_t *numdevices)
{
  struct usb_device *MTPDeviceList = NULL;
  LIBMTP_error_number_t ret;
  uint8_t i;
 
  /* Recover list of attached USB devices that match MTP criteria */
  switch(get_mtp_usb_device_list (&MTPDeviceList, numdevices))
  {
  /* These values should never occur at this point */
  default:
    return LIBMTP_ERROR_GENERAL;
    
  /* Memory Allocation Error, return */
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    return LIBMTP_ERROR_MEMORY_ALLOCATION;

  /* Auto-detection did not find any MTP devices, search known device list */
  case LIBMTP_ERROR_N0_DEVICE_ATTACHED:
    switch(get_mtp_usb_known_devices (&MTPDeviceList, numdevices))
    {
    /* Memory Allocation Error, return */
    case LIBMTP_ERROR_MEMORY_ALLOCATION:
      return LIBMTP_ERROR_MEMORY_ALLOCATION;
    
    /* No devices are attached, return */
    case LIBMTP_ERROR_N0_DEVICE_ATTACHED:
      return LIBMTP_ERROR_N0_DEVICE_ATTACHED;

    /* We should never execute this */
    default:
      return LIBMTP_ERROR_GENERAL;
    
    /* Found at least one device, but don't handle this here */
    case LIBMTP_ERROR_NONE:;
    }
    
  /* Found at least one device, continue along*/
  case LIBMTP_ERROR_NONE:;
  }
  
  /* Allocate Memory Appropriately and Initialize*/
  *params = (PTPParams **)malloc(*numdevices * sizeof(void *));
  *ptp_usb = (PTP_USB **)malloc(*numdevices * sizeof(void *));
  
  /* Check for allocation Error */
  if(*params == NULL || *ptp_usb == NULL)
  {
    /* Prevent Memory Leaks */
    if(*params != NULL)
    {
      free(*params);
      *params = NULL;
    }
    
    if(*ptp_usb != NULL)
    {
      free(*ptp_usb);
      *ptp_usb = NULL;
    }

    /* TODO: Implement callback function to let managing applications know
        there was a memory allocation problem */
    fprintf(stderr, "Memory Allocation Problem: libmtp line: %d", __LINE__);
    return LIBMTP_ERROR_MEMORY_ALLOCATION;
  }

  fprintf(stderr, "Found %d device(s)\n", *numdevices);
  fprintf(stderr, "Priming USB PTP Memory\n");
  ret =  prime_device_memory(*params, *ptp_usb, *numdevices, 0);
  fprintf(stderr, "prime_device_memory error code: %d\n", ret);

  fprintf(stderr, "Assigning Device Flags to known device(s)\n");
  assign_known_device_flags(MTPDeviceList, *ptp_usb, 0);
  
  fprintf(stderr, "Configuring Device(s)\n");
  ret = configure_usb_devices(MTPDeviceList, *params, *ptp_usb, 0);
  fprintf(stderr, "configure_usb_devices error code: %d\n", ret);
  
  /* Configure interface number */
  {
    struct usb_device *tmp = MTPDeviceList;
    for(i = 0; tmp != NULL && i < sizeof(interfaceNumber); i++,tmp = tmp->next)
    {
      interfaceNumber[i] = 
          tmp->config->interface->altsetting->bInterfaceNumber;
    }
  }
      
  if(MTPDeviceList)
  {
    free_MTP_list(MTPDeviceList);
    MTPDeviceList = NULL;
  }
  /* we're connected, return OK */
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

int open_device (int busn, int devn, short force, PTP_USB *ptp_usb, PTPParams *params, struct usb_device **dev)
{
#ifdef DEBUG
  printf("dev %i\tbus %i\n",devn,busn);
#endif
  
  *dev=find_device(busn,devn,force);
  if (*dev==NULL) {
    fprintf(stderr,"could not find any device matching given "
	    "bus/dev numbers\n");
    exit(-1);
  }
  find_endpoints(*dev,&ptp_usb->inep,&ptp_usb->inep_maxpacket,&ptp_usb->outep,&ptp_usb->outep_maxpacket,&ptp_usb->intep);
  
  if (init_ptp_usb(params, ptp_usb, *dev) < 0) {
    return -1;
  }
  if (ptp_opensession(params,1)!=PTP_RC_OK) {
    fprintf(stderr,"ERROR: Could not open session!\n");
    close_usb(ptp_usb, (*dev)->config->interface->altsetting->bInterfaceNumber);
    return -1;
  }
  return 0;
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
