/*
 *  mtp-utils.c
 *
 *  Created by Richard Low on 24/12/2005.
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

#include "mtp-utils.h"
#include "ptp-pack.h"

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

int ptpcam_usb_timeout = USB_TIMEOUT;

void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber);
struct usb_device* find_device (int busn, int devicen, short force);
void find_endpoints(struct usb_device *dev, int* inep, int* outep, int* intep);
void clear_stall(PTP_USB* ptp_usb);
void init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev);
static short ptp_write_func (unsigned char *bytes, unsigned int size, void *data);
static short ptp_read_func (unsigned char *bytes, unsigned int size, void *data);
static short ptp_check_int (unsigned char *bytes, unsigned int size, void *data);
int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep);
int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);

static short ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result=-1;
	PTP_USB *ptp_usb=(PTP_USB *)data;
	int toread=0;
	signed long int rbytes=size;
	
	do {
		bytes+=toread;
		if (rbytes>PTPCAM_USB_URB) 
			toread = PTPCAM_USB_URB;
		else
			toread = rbytes;
		result=USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)bytes, toread,ptpcam_usb_timeout);
		/* sometimes retry might help */
		if (result==0)
			result=USB_BULK_READ(ptp_usb->handle, ptp_usb->inep,(char *)bytes, toread,ptpcam_usb_timeout);
		if (result < 0)
			break;
		rbytes-=PTPCAM_USB_URB;
	} while (rbytes>0);
	
	if (result >= 0) {
		return (PTP_RC_OK);
	}
	else 
	{
		return PTP_ERROR_IO;
	}
}

static short ptp_write_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;
	
	
	/* only print if size < something */
	/*int i = 0;
	 if (size < 0xff)
	 {
		 printf("-------------------------\n");
		 printf("Sending data size %d\n", size);
		 for (i = 0; i < size; i += 8)
		 {
			 int j = i;
			 for (; j<size && j<i+8; j++)
				 printf("0x%02x ", bytes[j]);
			 printf("\n");
		 }
		 printf("-------------------------\n");
	 }
	 */
	
	result=USB_BULK_WRITE(ptp_usb->handle,ptp_usb->outep,(char *)bytes,size,ptpcam_usb_timeout);
	if (result >= 0)
		return (PTP_RC_OK);
	else 
	{
		return PTP_ERROR_IO;
	}
}

static short ptp_check_int (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;
	
	result=USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *)bytes,size,ptpcam_usb_timeout);
	if (result==0)
		result = USB_BULK_READ(ptp_usb->handle, ptp_usb->intep,(char *) bytes, size, ptpcam_usb_timeout);
	if (result >= 0) {
		return (PTP_RC_OK);
	} else {
		return PTP_ERROR_IO;
	}
}

void init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
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

void clear_stall(PTP_USB* ptp_usb)
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

void close_usb(PTP_USB* ptp_usb, uint8_t interfaceNumber)
{
	clear_stall(ptp_usb);
	usb_release_interface(ptp_usb->handle, interfaceNumber);
	usb_close(ptp_usb->handle);
}


struct usb_bus*
init_usb()
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
struct usb_device* find_device (int busn, int devn, short force)
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

/* this is a temporary function to connect to the first device we can, that has vendor ID CREATIVE_VENDOR_ID */

uint16_t connect_first_device(PTPParams *params, PTP_USB *ptp_usb, uint8_t *interfaceNumber)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	
	bus=init_usb();
	for (; bus; bus = bus->next)
	{
		for (dev = bus->devices; dev; dev = dev->next)
		{
			if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB && dev->descriptor.idVendor==CREATIVE_VENDOR_ID)
			{
				uint16_t ret=0;
				int n;
				struct usb_endpoint_descriptor *ep;
				PTPDeviceInfo deviceinfo;
					
				ep = dev->config->interface->altsetting->endpoint;
				n=dev->config->interface->altsetting->bNumEndpoints;
					
				find_endpoints(dev,&(ptp_usb->inep),&(ptp_usb->outep),&(ptp_usb->intep));
				init_ptp_usb(params, ptp_usb, dev);
					
				ret = ptp_opensession(params,1);
				if (ret != PTP_RC_OK)
				{
					printf("Could not open session!\n  Try to reset the camera.\n");
					usb_release_interface(ptp_usb->handle,dev->config->interface->altsetting->bInterfaceNumber);
					continue;
				}
				
				ret = ptp_getdeviceinfo(params, &deviceinfo);
				if (ret != PTP_RC_OK)
				{
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
	/* none found */
	return PTP_CD_RC_NO_DEVICES;
}

void find_endpoints(struct usb_device *dev, int* inep, int* outep, int* intep)
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
			}
			if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==0)
			{
				*outep=ep[i].bEndpointAddress;
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
	find_endpoints(*dev,&ptp_usb->inep,&ptp_usb->outep,&ptp_usb->intep);
	
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

int usb_clear_stall_feature(PTP_USB* ptp_usb, int ep)
{
	
	return (usb_control_msg(ptp_usb->handle,
													USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, USB_FEATURE_HALT,
													ep, NULL, 0, 3000));
}

int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status)
{
	 return (usb_control_msg(ptp_usb->handle,
													 USB_DP_DTH|USB_RECIP_ENDPOINT, USB_REQ_GET_STATUS,
													 USB_FEATURE_HALT, ep, (char *)status, 2, 3000));
}
