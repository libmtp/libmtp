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
#include "ptp.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <usb.h>

#include "libmtp.h"
#include "libusb-glue.h"
#include "util.h"

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
  // Reported by Darel on the XNJB forums
  { "Creative Zen V plus", 0x041e, 0x4152, DEVICE_FLAG_NONE },

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
   * Philipps
   */
  // From libgphoto2 source
  { "Philipps HDD6320", 0x0471, 0x01eb, DEVICE_FLAG_NONE },
  { "Philipps HDD6320 2", 0x0471, 0x014b, DEVICE_FLAG_NONE },
  // Anonymous SourceForge user
  { "Philipps HDD1630/17", 0x0471, 0x014c, DEVICE_FLAG_NONE },

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
   */
  { "iRiver Portable Media Center", 0x1006, 0x4002, DEVICE_FLAG_NONE },
  { "iRiver Portable Media Center", 0x1006, 0x4003, DEVICE_FLAG_NONE },
  // From libgphoto2 source
  { "iRiver T10", 0x4102, 0x1113, DEVICE_FLAG_NONE },
  { "iRiver T20 FM", 0x4102, 0x1114, DEVICE_FLAG_NONE },
  { "iRiver U10", 0x4102, 0x1116, DEVICE_FLAG_NONE },
  { "iRiver T10", 0x4102, 0x1117, DEVICE_FLAG_NONE },
  { "iRiver T20", 0x4102, 0x1118, DEVICE_FLAG_NONE },
  { "iRiver T30", 0x4102, 0x1119, DEVICE_FLAG_NONE },
  // Reported by David Wolpoff
  { "iRiver T10 2GB", 0x4102, 0x1120, DEVICE_FLAG_NONE },
  // Reported by Adam Torgerson
  { "iRiver Clix", 0x4102, 0x112a, DEVICE_FLAG_NONE },
  // Reported by Scott Call
  { "iRiver H10 20GB", 0x4102, 0x2101, DEVICE_FLAG_NONE },
  { "iRiver H10", 0x4102, 0x2102, DEVICE_FLAG_NONE },

  /*
   * Dell
   */
  { "Dell DJ Itty", 0x413c, 0x4500, DEVICE_FLAG_NONE },
  
  /*
   * Toshiba
   */
  { "Toshiba Gigabeat MEGF-40", 0x0930, 0x0009, DEVICE_FLAG_NONE },
  { "Toshiba Gigabeat", 0x0930, 0x000c, DEVICE_FLAG_NONE },
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
   * so special care is needed to blacklist the device in kernel
   * or similar to get it properly userspace:d.
   */
  { "Dunlop MP3 player 1GB / EGOMAN MD223AFD", 0x10d6, 0x2200, DEVICE_FLAG_UNLOAD_DRIVER}
  
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
static short ptp_write_func (unsigned char *bytes, unsigned int size, void *data);
static short ptp_read_func (unsigned char *bytes, unsigned int size, void *data, unsigned int *readbytes);
static short ptp_check_int (unsigned char *bytes, unsigned int size, void *data, unsigned int *rlen);
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


// Based on same function on library.c in libgphoto2
#define CONTEXT_BLOCK_SIZE	0x100000
static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data, unsigned int *readbytes)
{
  PTP_USB *ptp_usb = (PTP_USB *)data;
  int toread = 0;
  int result = 0;
  int curread = 0;
  /* Split into small blocks. Too large blocks (>1x MB) would
   * timeout.
   */
  while (curread < size) {
    toread = size - curread;
    if (toread > CONTEXT_BLOCK_SIZE)
      toread = CONTEXT_BLOCK_SIZE;
    
    result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)(bytes+curread), toread, ptpcam_usb_timeout);
    if (result == 0) {
      result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)(bytes+curread), toread, ptpcam_usb_timeout);
    }
    if (result < 0)
      return PTP_ERROR_IO;
    curread += result;
    if (result < toread) /* short reads are common */
      break;
  }

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
    *readbytes = curread;
    return (PTP_RC_OK);
  } else {
    return PTP_ERROR_IO;
  }
}

// Based on same function on library.c in libgphoto2
static short
ptp_write_func (unsigned char *bytes, unsigned int size, void *data)
{
  PTP_USB *ptp_usb = (PTP_USB *)data;
  int towrite = 0;
  int result = 0;
  int curwrite = 0;
  
  /*
   * gp_port_write returns (in case of success) the number of bytes
   * written. Too large blocks (>5x MB) could timeout.
   */
  while (curwrite < size) {
    towrite = size-curwrite;
    if (towrite > CONTEXT_BLOCK_SIZE)
      towrite = CONTEXT_BLOCK_SIZE;
    result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)(bytes+curwrite),towrite,ptpcam_usb_timeout);
    if (result < 0)
      return PTP_ERROR_IO;
    curwrite += result;
    if (result < towrite) /* short writes happen */
      break;
  }

  // Increase counters, call callback
  if (ptp_usb->callback_active) {
    ptp_usb->current_transfer_complete += curwrite;
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
  
  // If this is the last transfer (callbacks only active if this function called repeatedly with
  // new data, otherwise this is a single large transaction which ends here).
  // a request and the bulk send header.
  if (!ptp_usb->callback_active) {
    // Then terminate an even packet boundary write with a zero length packet
    if ((size % ptp_usb->outep_maxpacket) == 0) {
      result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)"x",0,ptpcam_usb_timeout);
    }
  } else if (ptp_usb->current_transfer_complete == ptp_usb->current_transfer_total) {
    // This is the amount actually transfered in this large transaction.
    uint64_t actual_xfer_size = ptp_usb->current_transfer_total - 2*PTP_USB_BULK_HDR_LEN;
    
    if ((actual_xfer_size % ptp_usb->outep_maxpacket) == 0) {
      result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)"x",0,ptpcam_usb_timeout);
    }
    // Set as complete and disable callback, just as good. 
    // This also blocks callbacks from the following response command.
    if (ptp_usb->current_transfer_callback != NULL) {
      (void) ptp_usb->current_transfer_callback(ptp_usb->current_transfer_total, // Both total = 100%
						ptp_usb->current_transfer_total, // Both total = 100%
						ptp_usb->current_transfer_callback_data);
    }
    ptp_usb->callback_active = 0;
  }

  if (result < 0)
    return PTP_ERROR_IO;
  return PTP_RC_OK;
}

// This is a bit hackish at the moment. I wonder if it even works.
static short ptp_check_int (unsigned char *bytes, unsigned int size, void *data, unsigned int *rlen)
{
  PTP_USB *ptp_usb = (PTP_USB *)data;
  int result;
  
  result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)bytes,size,ptpcam_usb_timeout);
  if (result==0)
    result = USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *) bytes, size, ptpcam_usb_timeout);
  if (result >= 0) {
    *rlen = result;
    return (PTP_RC_OK);
  } else {
    return PTP_ERROR_IO;
  }
}

static int init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
{
  usb_dev_handle *device_handle;
  
  params->write_func=ptp_write_func;
  params->read_func=ptp_read_func;
  params->check_int_func=ptp_check_int;
  params->check_int_fast_func=ptp_check_int;
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
	perror("usb_detach_kernel_driver_np()");
      }
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
  uint16_t status=0;
  int ret;
  
  /* check the inep status */
  ret = usb_get_endpoint_status(ptp_usb,ptp_usb->inep,&status);
  if (ret<0) perror ("inep: usb_get_endpoint_status()");
  /* and clear the HALT condition if happend */
  else if (status) {
    printf("Resetting input pipe!\n");
    ret=usb_clear_stall_feature(ptp_usb,ptp_usb->inep);
    /*usb_clear_halt(ptp_usb->handle,ptp_usb->inep); */
    if (ret<0)perror ("usb_clear_stall_feature()");
  }
  status=0;
  
  /* check the outep status */
  ret=usb_get_endpoint_status(ptp_usb,ptp_usb->outep,&status);
  if (ret<0) perror ("outep: usb_get_endpoint_status()");
  /* and clear the HALT condition if happend */
  else if (status) {
    printf("Resetting output pipe!\n");
    ret=usb_clear_stall_feature(ptp_usb,ptp_usb->outep);
    /*usb_clear_halt(ptp_usb->handle,ptp_usb->outep); */
    if (ret<0)perror ("usb_clear_stall_feature()");
  }
  
  /*usb_clear_halt(ptp_usb->handle,ptp_usb->intep); */
}

static void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber)
{
  clear_stall(ptp_usb);
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
