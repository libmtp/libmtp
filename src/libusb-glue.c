/*
 *  libusb-glue.c
 *
 *  Created by Richard Low on 24/12/2005. (as mtp-utils.c)
 *  Modified by Linus Walleij 2006-03-06
 *  (Notice that Anglo-Saxons use little-endian dates and Swedes use big-endian dates.)
 *
 * This file adds some utils (many copied from ptpcam.c from libptp2) to
 * use MTP devices. Include mtp-utils.h to use any of the ptp/mtp functions.
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
  // Contributed by anonymous person on SourceForge
  { "Samsung YP-T7J", 0x04e8, 0x5047, DEVICE_FLAG_NONE },
  // Reported by cstrickler@gmail.com
  { "Samsung YP-U2J (YP-U2JXB/XAA)", 0x04e8, 0x5054, DEVICE_FLAG_NONE },
  // Reported by Andrew Benson
  { "Samsung YP-F2J", 0x04e8, 0x5057, DEVICE_FLAG_DUALMODE },
  // Reported by Patrick <skibler@gmail.com>
  { "Samsung YP-K5", 0x04e8, 0x505a, DEVICE_FLAG_NONE },
  // Reported by Matthew Wilcox <matthew@wil.cx>
  { "Samsung Yepp T9", 0x04e8, 0x507f, DEVICE_FLAG_NONE },
  // From a rouge .INF file
  { "Samsung YH-999 Portable Media Center", 0x04e8, 0x5a0f, DEVICE_FLAG_NONE },

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
  { "Philips HDD6320/00", 0x0471, 0x014b, DEVICE_FLAG_NONE },
  // Anonymous SourceForge user
  { "Philips HDD1630/17", 0x0471, 0x014c, DEVICE_FLAG_NONE },
  // From Gerhard Mekenkamp
  { "Philips GoGear Audio", 0x0471, 0x0165, DEVICE_FLAG_NONE },
  // from XNJB forum
  { "Philips GoGear SA9200", 0x0471, 0x014f, DEVICE_FLAG_NONE },

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
  {"Canon PowerShot A640 (PTP/MTP mode)", 0x04a9, 0x3139, DEVICE_FLAG_NONE }
  
};
static const int mtp_device_table_size = sizeof(mtp_device_table) / sizeof(LIBMTP_device_entry_t);

int ptpcam_usb_timeout = USB_TIMEOUT;

// Local functions
static struct usb_bus* init_usb();
static struct usb_device *probe_usb_bus_for_mtp_devices(void);
static void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber);
static struct usb_device* find_device (int busn, int devicen, short force);
static void find_endpoints(struct usb_device *dev, int* inep, int* inep_maxpacket, int* outep, int* outep_maxpacket, int* intep);
static void clear_stall(PTP_USB* ptp_usb);
static int init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev);
static short ptp_write_func (unsigned long,PTPDataHandler*,void *data,unsigned long*);
static short ptp_read_func (unsigned long,PTPDataHandler*,void *data,unsigned long*);
static int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep);
static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);


int get_device_list(LIBMTP_device_entry_t ** const devices, int * const numdevs)
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
 * Check for the Microsoft OS device descriptor and returns device struct
 * if the device is MTP-compliant. The function will only recognize
 * a single device connected to the USB bus.
 *
 * @return an MTP-compliant USB device if one was found, else NULL.
 */
static struct usb_device *probe_usb_bus_for_mtp_devices(void)
{
  struct usb_bus *bus;
  
  bus = init_usb();
  for (; bus; bus = bus->next) {
    struct usb_device *dev;
    
    for (dev = bus->devices; dev; dev = dev->next) {
      usb_dev_handle *devh;
      unsigned char buf[1024], cmd;
      int ret;
  
      devh = usb_open(dev);
      if (devh == NULL) {
	continue;
      }

      // Read the special descripor, if possible...
      ret = usb_get_descriptor(devh, 0x03, 0xee, buf, sizeof(buf));
      
      if (ret < 10) {
	// printf("Device: VID %04x/PID %04x: no extended device property...\n",
	//       dev->descriptor.idVendor,
	//       dev->descriptor.idProduct);
	usb_close(devh);
	continue;
      }
      
      // It is atleast 10 bytes...
      if (!((buf[2] == 'M') && (buf[4]=='S') && (buf[6]=='F') && (buf[8]=='T'))) {
	printf("This is not a Microsoft MTP descriptor...\n");
	printf("Device response to read device property 0xee:\n");
	data_dump_ascii (stdout, buf, ret, 0);
	usb_close(devh);
	continue;
      }
      
      cmd = buf[16];
      ret = usb_control_msg (devh, USB_ENDPOINT_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR, 
			     cmd, 0, 4, (char *) buf, sizeof(buf), 1000);
      if (ret == -1) {
	//printf("Decice could not respond to control message 1.\n");
	usb_close(devh);
	continue;
      }
      
      if (ret > 0x15) {
	if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
	  printf("The device has a Microsoft device descriptor, but it's not MTP.\n");
	  printf("This is not an MTP device. Presumable it is USB mass storage\n");
	  printf("with some additional Janus (DRM) support.\n");
	  printf("Device response to control message 1:\n");
	  data_dump_ascii (stdout, buf, ret, 0);
	  continue;
	}
      } else {
	// Not MTP or broken
	continue;
      }

      ret = usb_control_msg (devh, USB_ENDPOINT_IN|USB_RECIP_DEVICE|USB_TYPE_VENDOR, 
			     cmd, 0, 5, (char *) buf, sizeof(buf), 1000);
      if (ret == -1) {
	//printf("Device could not respond to control message 2.\n");
	usb_close(devh);
	// Return the device anyway, it said previously it was MTP, right?
	return dev;
      }
      
      if (ret > 0x15) {
	if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
	  printf("This device does not respond with MTP characteristics on\n");
	  printf("the second device property read (0x05), but will be regarded\n");
	  printf("as MTP anyway.\n");
	  printf("Device response to control message 2:\n");
	  data_dump_ascii (stdout, buf, ret, 0);
	}
      }
      
      usb_close(devh);
      // We can return the device here, it will be the first. 
      // If it was not MTP, the loop continues before it reaches this point.
      return dev;
    }
  }
  // If nothing was found we end up here.
  return NULL;
}

/**
 * Detect the MTP device descriptor and return the VID and PID
 * of the first device found. This is a very low-level function
 * which is intended for use with <b>udev</b> or other hotplug
 * mechanisms. The idea is that a script may want to know if the
 * just plugged-in device was an MTP device or not.
 * @param vid the Vendor ID (VID) of the first device found.
 * @param pid the Product ID (PID) of the first device found.
 * @return the number of detected devices or -1 if the call
 *         was unsuccessful.
 */
int LIBMTP_Detect_Descriptor(uint16_t *vid, uint16_t *pid)
{
  struct usb_device *dev = probe_usb_bus_for_mtp_devices();
  if (dev == NULL) {
    return 0;
  }
  *vid = dev->descriptor.idVendor;
  *pid = dev->descriptor.idProduct;
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
	unsigned long *readbytes
) {
  PTP_USB *ptp_usb = (PTP_USB *)data;
  unsigned long toread = 0;
  int result = 0;
  unsigned long curread = 0;
  unsigned long written;
  unsigned char *bytes;

  // This is the largest block we'll need to read in.
  bytes = malloc(CONTEXT_BLOCK_SIZE);
  while (curread < size) {
    toread = size - curread;
    if (toread > CONTEXT_BLOCK_SIZE)
      toread = CONTEXT_BLOCK_SIZE;
    else if (toread > ptp_usb->outep_maxpacket)
      toread -= toread % ptp_usb->outep_maxpacket;

    result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, (char*)bytes, toread, ptpcam_usb_timeout);
    if (result == 0) {
      result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep, (char*)bytes, toread, ptpcam_usb_timeout);
    }
    if (result < 0)
      return PTP_ERROR_IO;
#ifdef ENABLE_USB_BULK_DEBUG
    printf("<==USB IN\n");
    data_dump_ascii (stdout,bytes,result,16);
#endif
    handler->putfunc (NULL, handler->private, result, bytes, &written);
    curread += result;
    if (result < toread) /* short reads are common */
      break;
  }
  if (readbytes) *readbytes = curread;
  free (bytes);

  // Increase counters, call callback
  if (ptp_usb->callback_active) {
    ptp_usb->current_transfer_complete += curread;
    if (ptp_usb->current_transfer_complete > ptp_usb->current_transfer_total) {
      // Fishy... but some commands have unpredictable lengths.
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
  if (!bytes) return PTP_ERROR_IO;
  while (curwrite < size) {
    towrite = size-curwrite;
    if (towrite > CONTEXT_BLOCK_SIZE)
      towrite = CONTEXT_BLOCK_SIZE;
    else
      if (towrite > ptp_usb->outep_maxpacket && towrite % ptp_usb->outep_maxpacket != 0)
        towrite -= towrite % ptp_usb->outep_maxpacket;
    handler->getfunc (NULL, handler->private,towrite,bytes,&towrite);
    result = USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char*)bytes,towrite,ptpcam_usb_timeout);
#ifdef ENABLE_USB_BULK_DEBUG
    printf("USB OUT==>\n");
    data_dump_ascii (stdout,bytes,towrite,16);
#endif
    if (result < 0)
      return PTP_ERROR_IO;
    curwrite += result;
    if (result < towrite) /* short writes happen */
      break;
  }
  free (bytes);
  if (written) *written = curwrite;

  // Increase counters
  ptp_usb->current_transfer_complete += curwrite;
  
  // call callback
  if (ptp_usb->callback_active) {
    if (ptp_usb->current_transfer_complete > ptp_usb->current_transfer_total) {
      // Fishy... but some commands have unpredictable lengths.
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
  
  if (ptp_usb->current_transfer_complete == ptp_usb->current_transfer_total)
    ptp_usb->callback_active = 0;
  
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
			written, towrite
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
		datawlen = (size<PTP_USB_BULK_PAYLOAD_LEN)?size:PTP_USB_BULK_PAYLOAD_LEN;
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
	ret = ptp_read_func( sizeof(*packet), &memhandler, params->data, rlen);
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
					PTP_USB_BULK_HS_MAX_PACKET_LEN,
					handler,
					params->data,
					&readdata
				);
				if (xret == -1)
					return PTP_ERROR_IO;
				if (readdata < PTP_USB_BULK_HS_MAX_PACKET_LEN)
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

		data = malloc(PTP_USB_BULK_HS_MAX_PACKET_LEN);
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
				      params->data, &rlen);
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
 * This function scans through the device list to see if there are
 * some devices to connect to. The table at the top of this file is
 * used to identify potential devices.
 */
uint16_t connect_first_device(PTPParams *params, PTP_USB *ptp_usb, uint8_t *interfaceNumber)
{
  struct usb_bus *bus;
  struct usb_device *dev;
  struct usb_endpoint_descriptor *ep;
  PTPDeviceInfo deviceinfo;
  uint16_t ret=0;
  int n;

  // Reset device flags
  ptp_usb->device_flags = DEVICE_FLAG_NONE;
  // First try to locate the device using the extended
  // device descriptor.
  dev = probe_usb_bus_for_mtp_devices();

  if (dev != NULL) {
    int i;

    // See if we can find the name of this beast
    for (i = 0; i < mtp_device_table_size; i++) {
      LIBMTP_device_entry_t const *mtp_device = &mtp_device_table[i];
      if (dev->descriptor.idVendor == mtp_device->vendor_id &&
	  dev->descriptor.idProduct == mtp_device->product_id ) {
	printf("Autodetected device \"%s\" (VID=%04x,PID=%04x) is known.\n", 
	       mtp_device->name, dev->descriptor.idVendor, dev->descriptor.idProduct);
	ptp_usb->device_flags = mtp_device->device_flags;
	break;
      }
    }
    if (i == mtp_device_table_size) {
      printf("Autodetected device with VID=%04x and PID=%04x is UNKNOWN.\n", 
	     dev->descriptor.idVendor, dev->descriptor.idProduct);
      printf("Please report this VID/PID and the device model name etc to the\n");
      printf("libmtp development team!\n");
    }
  }

  // If autodetection fails, scan the bus for well known devices.
  if (dev == NULL) {
    bus = init_usb();
    for (; bus; bus = bus->next) {
      for (dev = bus->devices; dev; dev = dev->next) {
	int i;
	
	// Loop over the list of supported devices
	for (i = 0; i < mtp_device_table_size; i++) {
	  LIBMTP_device_entry_t const *mtp_device = &mtp_device_table[i];
	  
	  if (dev->descriptor.bDeviceClass != USB_CLASS_HUB && 
	      dev->descriptor.idVendor == mtp_device->vendor_id &&
	      dev->descriptor.idProduct == mtp_device->product_id ) {
	    
	    printf("Found non-autodetected device \"%s\" on USB bus...\n", mtp_device->name);
	    ptp_usb->device_flags = mtp_device->device_flags;
            goto next_step;
	    
	  }
	}
      }
    }
  }

next_step:
  // Still not found any?
  if (dev == NULL) {
    return PTP_CD_RC_NO_DEVICES;
  }

  // Found a device, then assign endpoints to ptp_usb...
  ep = dev->config->interface->altsetting->endpoint;
  n = dev->config->interface->altsetting->bNumEndpoints;
  find_endpoints(dev, &ptp_usb->inep, &ptp_usb->inep_maxpacket,
		 &ptp_usb->outep, &ptp_usb->outep_maxpacket, &ptp_usb->intep);
  
  // printf("Init PTP USB...\n");
  if (init_ptp_usb(params, ptp_usb, dev) < 0) {
    return PTP_CD_RC_ERROR_CONNECTING;
  }
  
  ret = ptp_opensession(params,1);

  // This works in situations where previous bad applications have not used LIBMTP_Release_Device on exit
  if (ret == PTP_ERROR_IO) {
	printf("%s\n","PTP ERROR IO: Trying again after resetting USB");
        // printf("%s\n","Closing USB interface...");
	close_usb(ptp_usb,dev->config->interface->altsetting->bInterfaceNumber);
        // printf("%s\n","Init PTP USB...");
	if (init_ptp_usb(params, ptp_usb, dev) < 0) {
 	   return PTP_CD_RC_ERROR_CONNECTING;
  	}
	
	ret = ptp_opensession(params,1);
  }

  // printf("Session open (%d)...\n", ret);
  if (ret == PTP_RC_InvalidTransactionID) {
    params->transaction_id += 10;
    ret = ptp_opensession(params,1);
  }
  if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK) {
    printf("Could not open session! (Return code %d)\n  Try to reset the device.\n", ret);
    usb_release_interface(ptp_usb->handle,dev->config->interface->altsetting->bInterfaceNumber);
  }

  // It is actually permissible to call this before opening the session
  ret = ptp_getdeviceinfo(params, &deviceinfo);
  if (ret != PTP_RC_OK) {
    printf("Could not get device info!\n");
    usb_release_interface(ptp_usb->handle,dev->config->interface->altsetting->bInterfaceNumber);
    return PTP_CD_RC_ERROR_CONNECTING;
  }
  
  // we're connected, return OK
  *interfaceNumber = dev->config->interface->altsetting->bInterfaceNumber;  
  return PTP_CD_RC_CONNECTED;
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
