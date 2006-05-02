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
  /* Creative devices */
  { "Creative Zen Vision", 0x041e, 0x411f, NULL },
  { "Creative Portable Media Center", 0x041e, 0x4123, NULL },
  { "Creative Zen Xtra (MTP mode)", 0x041e, 0x4128, NULL },
  { "Second generation Dell DJ", 0x041e, 0x412f, NULL },
  { "Creative Zen Micro (MTP mode)", 0x041e, 0x4130, NULL },
  { "Creative Zen Touch (MTP mode)", 0x041e, 0x4131, NULL },
  { "Creative Zen Sleek (MTP mode)", 0x041e, 0x4137, NULL },
  { "Creative Zen MicroPhoto", 0x041e, 0x413c, NULL },
  { "Creative Zen Sleek Photo", 0x041e, 0x413d, NULL },
  { "Creative Zen Vision:M", 0x041e, 0x413e, NULL },
  /* Contributed by anonymous person on SourceForge */
  { "Samsung YP-T7J", 0x04e8, 0x5047, NULL },
  /* From Mark Veinot */
  { "JVC Alneo XA-HD500", 0x04f1, 0x6105, NULL },
  /* 
   * Copied in from libgphoto2's libptp2 adaption "library.c"
   * carefully trying to pick only the MTP devices.
   * Greetings to Marcus Meissner! (we should merge our
   * projects some day...)
   */
  { "Philipps HDD6320", 0x0471, 0x01eb, NULL},
  { "iRiver T10", 0x4102, 0x1113, NULL },
  { "iRiver U10", 0x4102, 0x1116, NULL },
  { "iRiver T10", 0x4102, 0x1117, NULL },
  { "iRiver T20", 0x4102, 0x1118, NULL },
  { "iRiver T30", 0x4102, 0x1119, NULL },
  { "iRiver H10", 0x4102, 0x2102, NULL },
  { "Dell DJ Itty", 0x413c, 0x4500, NULL},
  { "Toshiba Gigabeat", 0x0930, 0x000c, NULL}
};
static const int mtp_device_table_size = sizeof(mtp_device_table) / sizeof(LIBMTP_device_entry_t);

int ptpcam_usb_timeout = USB_TIMEOUT;

// Local functions
static void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber);
static struct usb_device* find_device (int busn, int devicen, short force);
static void find_endpoints(struct usb_device *dev, int* inep, int* inep_maxpacket, int* outep, int* outep_maxpacket, int* intep);
static void clear_stall(PTP_USB* ptp_usb);
static void init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev);
static short ptp_write_func (unsigned char *bytes, unsigned int size, void *data);
static short ptp_read_func (unsigned char *bytes, unsigned int size, void *data, unsigned int *readbytes);
static short ptp_check_int (unsigned char *bytes, unsigned int size, void *data, unsigned int *rlen);
static int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep);
static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);
static struct usb_bus* init_usb();

int get_device_list(LIBMTP_device_entry_t ** const devices, int * const numdevs)
{
  *devices = (LIBMTP_device_entry_t *) &mtp_device_table;
  *numdevs = mtp_device_table_size;
  return 0;
}

// Based on same function on library.c in libgphoto2
#define CONTEXT_BLOCK_SIZE	100000
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
    if (toread > 4096)
      toread = 4096;
    
    result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)(bytes+curread), toread, ptpcam_usb_timeout);
    if (result == 0) {
      result = USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)(bytes+curread), toread, ptpcam_usb_timeout);
    }
    if (result < 0)
      break;
    curread += result;
    if (result < toread) /* short reads are common */
      break;
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
   * write. Too large blocks (>5x MB) could timeout.
   */
  while (curwrite < size) {
    towrite = size-curwrite;
    if (towrite > 4096)
      towrite = 4096;
    result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)(bytes+curwrite),towrite,ptpcam_usb_timeout);
    if (result < 0)
      break;
    curwrite += result;
    if (result < towrite) /* short writes happen */
      break;
  }
  // Should load wMaxPacketsize from endpoint first. But works fine for all EPs.
  if ((size % 512) == 0)
    result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)"x",0,ptpcam_usb_timeout);
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

static void init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
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
  
  if ((device_handle=usb_open(dev))){
    if (!device_handle) {
      perror("usb_open()");
      exit(0);
    }
    ptp_usb->handle=device_handle;
    usb_claim_interface(device_handle,
			dev->config->interface->altsetting->bInterfaceNumber);
  }
}

static void clear_stall(PTP_USB* ptp_usb)
{
  uint16_t status=0;
  int ret;
  
  /* check the inep status */
  ret=usb_get_endpoint_status(ptp_usb,ptp_usb->inep,&status);
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


static struct usb_bus* init_usb()
{
  usb_init();
  usb_find_busses();
  usb_find_devices();
  return (usb_get_busses());
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
	  uint16_t ret=0;
	  int n;
	  struct usb_endpoint_descriptor *ep;
	  PTPDeviceInfo deviceinfo;
	  
	  printf("Found device \"%s\" on USB bus...\n", mtp_device->name);
	  ep = dev->config->interface->altsetting->endpoint;
	  n=dev->config->interface->altsetting->bNumEndpoints;
	  
	  find_endpoints(dev,&(ptp_usb->inep),&(ptp_usb->inep_maxpacket),&(ptp_usb->outep),&(ptp_usb->outep_maxpacket),&(ptp_usb->intep));
	  printf("Init PTP USB...\n");
	  init_ptp_usb(params, ptp_usb, dev);
	  
	  ret = ptp_opensession(params,1);
	  printf("Session open (%d)...\n", ret);
	  if (ret == PTP_RC_InvalidTransactionID) {
	    params->transaction_id += 10;
	    ret = ptp_opensession(params,1);
	  }
	  if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK) {
	    printf("Could not open session! (Return code %d)\n  Try to reset the device.\n", ret);
	    usb_release_interface(ptp_usb->handle,dev->config->interface->altsetting->bInterfaceNumber);
	    continue;
	  }
	  /* It is actually permissible to call this before opening the session */
	  ret = ptp_getdeviceinfo(params, &deviceinfo);
	  if (ret != PTP_RC_OK) {
	    printf("Could not get device info!\n");
	    usb_release_interface(ptp_usb->handle,dev->config->interface->altsetting->bInterfaceNumber);
	    return PTP_CD_RC_ERROR_CONNECTING;
	  }
	  
	  /* we're connected, return ok */
	  *interfaceNumber = dev->config->interface->altsetting->bInterfaceNumber;
	  
	  return PTP_CD_RC_CONNECTED;
	}
      }
    }
  }
  /* none found */
  return PTP_CD_RC_NO_DEVICES;
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
	  *inep_maxpacket=ep[i].wMaxPacketSize;
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
  
  init_ptp_usb(params, ptp_usb, *dev);
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
