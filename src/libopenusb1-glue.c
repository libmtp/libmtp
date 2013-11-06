/*
 * \file libusb1-glue.c
 * Low-level USB interface glue towards libusb.
 *
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2005-2012 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2011 Marcus Meissner
 * Copyright (C) 2007 Ted Bullock
 * Copyright (C) 2008 Chris Bagwell <chris@cnpbagwell.com>
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
#include "../config.h"
#include "libmtp.h"
#include "libusb-glue.h"
#include "device-flags.h"
#include "util.h"
#include "ptp.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <usb.h>

#include "ptp-pack.c"

/* Aha, older libusb does not have USB_CLASS_PTP */
#ifndef USB_CLASS_PTP
#define USB_CLASS_PTP 6
#endif

/*
 * Default USB timeout length.  This can be overridden as needed
 * but should start with a reasonable value so most common
 * requests can be completed.  The original value of 4000 was
 * not long enough for large file transfer.  Also, players can
 * spend a bit of time collecting data.  Higher values also
 * make connecting/disconnecting more reliable.
 */
#define USB_TIMEOUT_DEFAULT     20000
#define USB_TIMEOUT_LONG        60000

static inline int get_timeout(PTP_USB* ptp_usb) {
    if (FLAG_LONG_TIMEOUT(ptp_usb)) {
        return USB_TIMEOUT_LONG;
    }
    return USB_TIMEOUT_DEFAULT;
}

/* USB Feature selector HALT */
#ifndef USB_FEATURE_HALT
#define USB_FEATURE_HALT	0x00
#endif

/* Internal data types */
struct mtpdevice_list_struct {
    openusb_dev_handle_t device;
    PTPParams *params;
    PTP_USB *ptp_usb;
    uint32_t bus_location;
    struct mtpdevice_list_struct *next;
};
typedef struct mtpdevice_list_struct mtpdevice_list_t;

static const LIBMTP_device_entry_t mtp_device_table[] = {
    /* We include an .h file which is shared between us and libgphoto2 */
#include "music-players.h"
};
static const int mtp_device_table_size = sizeof (mtp_device_table) / sizeof (LIBMTP_device_entry_t);

// Local functions
static void init_usb();
static void close_usb(PTP_USB* ptp_usb);
static int find_interface_and_endpoints(openusb_dev_handle_t *dev,
        uint8_t *conf,
        uint8_t *interface,
        uint8_t *altsetting,
        int* inep,
        int* inep_maxpacket,
        int* outep,
        int* outep_maxpacket,
        int* intep);
static void clear_stall(PTP_USB* ptp_usb);
static int init_ptp_usb(PTPParams* params, PTP_USB* ptp_usb, openusb_dev_handle_t * dev);
static short ptp_write_func(unsigned long, PTPDataHandler*, void *data, unsigned long*);
static short ptp_read_func(unsigned long, PTPDataHandler*, void *data, unsigned long*, int);
static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);

// Local USB handles.
static openusb_handle_t libmtp_openusb_handle;

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
int LIBMTP_Get_Supported_Devices_List(LIBMTP_device_entry_t * * const devices, int * const numdevs) {
    *devices = (LIBMTP_device_entry_t *) & mtp_device_table;
    *numdevs = mtp_device_table_size;
    return 0;
}

static void init_usb() {
    openusb_init(NULL, &libmtp_openusb_handle);
}

/**
 * Small recursive function to append a new usb_device to the linked list of
 * USB MTP devices
 * @param devlist dynamic linked list of pointers to usb devices with MTP
 *        properties, to be extended with new device.
 * @param newdevice the new device to add.
 * @param bus_location bus for this device.
 * @return an extended array or NULL on failure.
 */
static mtpdevice_list_t *append_to_mtpdevice_list(mtpdevice_list_t *devlist,
        openusb_dev_handle_t *newdevice,

        uint32_t bus_location) {
    mtpdevice_list_t *new_list_entry;

    new_list_entry = (mtpdevice_list_t *) malloc(sizeof (mtpdevice_list_t));
    if (new_list_entry == NULL) {
        return NULL;
    }
    // Fill in USB device, if we *HAVE* to make a copy of the device do it here.
    new_list_entry->device = *newdevice;
    new_list_entry->bus_location = bus_location;
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
static void free_mtpdevice_list(mtpdevice_list_t *devlist) {
    mtpdevice_list_t *tmplist = devlist;

    if (devlist == NULL)
        return;
    while (tmplist != NULL) {
        mtpdevice_list_t *tmp = tmplist;
        tmplist = tmplist->next;
        // Do not free() the fields (ptp_usb, params)! These are used elsewhere.
        free(tmp);
    }
    return;
}

/**
 * This checks if a device has an MTP descriptor. The descriptor was
 * elaborated about in gPhoto bug 1482084, and some official documentation
 * with no strings attached was published by Microsoft at
 * http://www.microsoft.com/whdc/system/bus/USB/USBFAQ_intermed.mspx#E3HAC
 *
 * @param dev a device struct from libopenusb.
 * @param dumpfile set to non-NULL to make the descriptors dump out
 *        to this file in human-readable hex so we can scruitinze them.
 * @return 1 if the device is MTP compliant, 0 if not.
 */
static int probe_device_descriptor(openusb_dev_handle_t *dev, FILE *dumpfile) {
    openusb_dev_handle_t *devh = NULL;
    unsigned char buf[1024], cmd;
    uint8_t *bufptr = (uint8_t *) &buf;
    unsigned int buffersize = sizeof(buf);
    int i;
    int ret;
    /* This is to indicate if we find some vendor interface */
    int found_vendor_spec_interface = 0;
    struct usb_device_desc desc;
    struct usb_interface_desc ifcdesc;

    ret = openusb_parse_device_desc(libmtp_openusb_handle, *dev, NULL, 0, &desc);
    if (ret != OPENUSB_SUCCESS) return 0;
    /*
     * Don't examine devices that are not likely to
     * contain any MTP interface, update this the day
     * you find some weird combination...
     */
    if (!(desc.bDeviceClass == USB_CLASS_PER_INTERFACE ||
            desc.bDeviceClass == USB_CLASS_COMM ||
            desc.bDeviceClass == USB_CLASS_PTP ||
            desc.bDeviceClass == 0xEF || /* Intf. Association Desc.*/
            desc.bDeviceClass == USB_CLASS_VENDOR_SPEC)) {
        return 0;
    }

    /* Attempt to open Device on this port */
    ret = openusb_open_device(libmtp_openusb_handle, NULL, USB_INIT_DEFAULT, devh);
    if (ret != OPENUSB_SUCCESS) {
        /* Could not open this device */
        return 0;
    }

    /*
     * This sometimes crashes on the j for loop below
     * I think it is because config is NULL yet
     * dev->descriptor.bNumConfigurations > 0
     * this check should stop this
     */
    /*
     * Loop over the device configurations and interfaces. Nokia MTP-capable
     * handsets (possibly others) typically have the string "MTP" in their
     * MTP interface descriptions, that's how they can be detected, before
     * we try the more esoteric "OS descriptors" (below).
     */
    for (i = 0; i < desc.bNumConfigurations; i++) {
        uint8_t j;
        struct usb_config_desc config;

        ret = openusb_parse_config_desc(libmtp_openusb_handle, *dev, NULL, 0, 0, &config);
        if (ret != OPENUSB_SUCCESS) {
            LIBMTP_INFO("configdescriptor %d get failed with ret %d in probe_device_descriptor yet dev->descriptor.bNumConfigurations > 0\n", i, ret);
            continue;
        }

        for (j = 0; j < config.bNumInterfaces; j++) {
            int k = 0;

            while (openusb_parse_interface_desc(libmtp_openusb_handle, *dev, NULL, 0, 0, j, k++, &ifcdesc) == 0) {
                /* Current interface descriptor */

                /*
                 * MTP interfaces have three endpoints, two bulk and one
                 * interrupt. Don't probe anything else.
                 */
                if (ifcdesc.bNumEndpoints != 3)
                    continue;

                /*
                 * We only want to probe for the OS descriptor if the
                 * device is LIBUSB_CLASS_VENDOR_SPEC or one of the interfaces
                 * in it is, so flag if we find an interface like this.
                 */
                if (ifcdesc.bInterfaceClass == USB_CLASS_VENDOR_SPEC) {
                    found_vendor_spec_interface = 1;
                }

                /*
                 * Check for Still Image Capture class with PIMA 15740 protocol,
                 * also known as PTP
                 */

                /*
                 * Next we search for the MTP substring in the interface name.
                 * For example : "RIM MS/MTP" should work.
                 */
                buf[0] = '\0';
                // FIXME: DK: Find out how to get the string descriptor for an interface?
                /*
                                ret = libusb_get_string_descriptor_ascii(devh,
                                        config->interface[j].altsetting[k].iInterface,
                                        buf,
                                        1024);
                 */
                if (ret < 3)
                    continue;
                if (strstr((char *) buf, "MTP") != NULL) {
                    if (dumpfile != NULL) {
                        fprintf(dumpfile, "Configuration %d, interface %d, altsetting %d:\n", i, j, k);
                        fprintf(dumpfile, "   Interface description contains the string \"MTP\"\n");
                        fprintf(dumpfile, "   Device recognized as MTP, no further probing.\n");
                    }
                    //libusb_free_config_descriptor(config);
                    openusb_close_device(*devh);
                    return 1;
                }
            }
        }
    }

    /*
     * Only probe for OS descriptor if the device is vendor specific
     * or one of the interfaces found is.
     */
    if (desc.bDeviceClass == USB_CLASS_VENDOR_SPEC ||
            found_vendor_spec_interface) {

        /* Read the special descriptor */
        //ret = libusb_get_descriptor(devh, 0x03, 0xee, buf, sizeof (buf));
        ret = openusb_get_raw_desc(libmtp_openusb_handle, *dev, USB_DESC_TYPE_STRING, 0xee, 0, &bufptr, (unsigned short *)&buffersize);
        /*
         * If something failed we're probably stalled to we need
         * to clear the stall off the endpoint and say this is not
         * MTP.
         */
        if (ret < 0) {
            /* EP0 is the default control endpoint */
            //libusb_clear_halt (devh, 0);
            openusb_close_device(*devh);
            openusb_free_raw_desc(buf);
            return 0;
        }

        // Dump it, if requested
        if (dumpfile != NULL && ret > 0) {
            fprintf(dumpfile, "Microsoft device descriptor 0xee:\n");
            data_dump_ascii(dumpfile, buf, ret, 16);
        }

        /* Check if descriptor length is at least 10 bytes */
        if (ret < 10) {
            openusb_close_device(*devh);
            openusb_free_raw_desc(buf);
            return 0;
        }

        /* Check if this device has a Microsoft Descriptor */
        if (!((buf[2] == 'M') && (buf[4] == 'S') &&
                (buf[6] == 'F') && (buf[8] == 'T'))) {
            openusb_close_device(*devh);
            openusb_free_raw_desc(buf);
            return 0;
        }

        /* Check if device responds to control message 1 or if there is an error */
        cmd = buf[16];

        /*
           ret = libusb_control_transfer (devh,
                                  LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_VENDOR,
                                  cmd,
                                  0,
                                  4,
                                  buf,
                                  sizeof(buf),
                                  USB_TIMEOUT_DEFAULT);
         */
        struct openusb_ctrl_request ctrl;
        ctrl.setup.bmRequestType = USB_ENDPOINT_IN | USB_RECIP_DEVICE | USB_REQ_TYPE_VENDOR;
        ctrl.setup.bRequest = cmd;
        ctrl.setup.wValue = 0;
        ctrl.setup.wIndex = 4;
        ctrl.payload = bufptr; // Out
        ctrl.length = sizeof (buf);
        ctrl.timeout = USB_TIMEOUT_DEFAULT;
        ctrl.next = NULL;
        ctrl.flags = 0;

        ret = openusb_ctrl_xfer(*devh, 0, USB_ENDPOINT_IN, &ctrl);


        // Dump it, if requested
        if (dumpfile != NULL && ctrl.result.transferred_bytes > 0) {
            fprintf(dumpfile, "Microsoft device response to control message 1, CMD 0x%02x:\n", cmd);
            data_dump_ascii(dumpfile, buf, ctrl.result.transferred_bytes, 16);
        }

        /* If this is true, the device either isn't MTP or there was an error */
        if (ctrl.result.transferred_bytes <= 0x15) {
            /* TODO: If there was an error, flag it and let the user know somehow */
            /* if(ret == -1) {} */
            openusb_close_device(*devh);
            return 0;
        }

        /* Check if device is MTP or if it is something like a USB Mass Storage
           device with Janus DRM support */
        if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
            openusb_close_device(*devh);
            return 0;
        }

        /* After this point we are probably dealing with an MTP device */

        /*
         * Check if device responds to control message 2, which is
         * the extended device parameters. Most devices will just
         * respond with a copy of the same message as for the first
         * message, some respond with zero-length (which is OK)
         * and some with pure garbage. We're not parsing the result
         * so this is not very important.
         */
        /*
            ret = libusb_control_transfer (devh,
                                   LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_DEVICE | LIBUSB_REQUEST_TYPE_VENDOR,
                                   cmd,
                                   0,
                                   5,
                                   buf,
                                   sizeof(buf),
                                   USB_TIMEOUT_DEFAULT);
         */
        //struct openusb_ctrl_request ctrl;
        ctrl.setup.bmRequestType = USB_ENDPOINT_IN | USB_RECIP_DEVICE | USB_REQ_TYPE_VENDOR;
        ctrl.setup.bRequest = cmd;
        ctrl.setup.wValue = 0;
        ctrl.setup.wIndex = 5;
        ctrl.payload = bufptr; // Out
        ctrl.length = sizeof (buf);
        ctrl.timeout = USB_TIMEOUT_DEFAULT;
        ctrl.next = NULL;
        ctrl.flags = 0;

        ret = openusb_ctrl_xfer(*devh, 0, USB_ENDPOINT_IN, &ctrl);

        // Dump it, if requested
        if (dumpfile != NULL && ctrl.result.transferred_bytes > 0) {
            fprintf(dumpfile, "Microsoft device response to control message 2, CMD 0x%02x:\n", cmd);
            data_dump_ascii(dumpfile, buf, ret, 16);
        }

        /* If this is true, the device errored against control message 2 */
        if (ctrl.result.transferred_bytes < 0) {
            /* TODO: Implement callback function to let managing program know there
               was a problem, along with description of the problem */
            LIBMTP_ERROR("Potential MTP Device with VendorID:%04x and "
                    "ProductID:%04x encountered an error responding to "
                    "control message 2.\n"
                    "Problems may arrise but continuing\n",
                    desc.idVendor, desc.idProduct);
        } else if (dumpfile != NULL && ctrl.result.transferred_bytes == 0) {
            fprintf(dumpfile, "Zero-length response to control message 2 (OK)\n");
        } else if (dumpfile != NULL) {
            fprintf(dumpfile, "Device responds to control message 2 with some data.\n");
        }
        /* Close the USB device handle */
        openusb_close_device(*devh);
        return 1;
    }

    /* Close the USB device handle */
    openusb_close_device(*devh);
    return 0;
}

/**
 * This function scans through the connected usb devices on a machine and
 * if they match known Vendor and Product identifiers appends them to the
 * dynamic array mtp_device_list. Be sure to call
 * <code>free_mtpdevice_list(mtp_device_list)</code> when you are done
 * with it, assuming it is not NULL.
 * @param mtp_device_list dynamic array of pointers to usb devices with MTP
 *        properties (if this list is not empty, new entries will be appended
 *        to the list).
 * @return LIBMTP_ERROR_NONE implies that devices have been found, scan the list
 *        appropriately. LIBMTP_ERROR_NO_DEVICE_ATTACHED implies that no
 *        devices have been found.
 */
static LIBMTP_error_number_t get_mtp_usb_device_list(mtpdevice_list_t ** mtp_device_list) {
    int nrofdevs = 0;
    openusb_devid_t *devs = NULL;
    struct usb_device_desc desc;
    int ret, i;

    init_usb();
    ret = openusb_get_devids_by_bus(libmtp_openusb_handle, 0, &devs, &nrofdevs);


    for (i = 0; i < nrofdevs; i++) {
        openusb_devid_t dev = devs[i];

        ret = openusb_parse_device_desc(libmtp_openusb_handle, dev, NULL, 0, &desc);
        if (ret != OPENUSB_SUCCESS) continue;
        
        if (desc.bDeviceClass != USB_CLASS_HUB) {
            int i;
            int found = 0;
            // First check if we know about the device already.
            // Devices well known to us will not have their descriptors
            // probed, it caused problems with some devices.
            for (i = 0; i < mtp_device_table_size; i++) {
                if (desc.idVendor == mtp_device_table[i].vendor_id &&
                        desc.idProduct == mtp_device_table[i].product_id) {
                    /* Append this usb device to the MTP device list */
                    *mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, &dev, 0);
                    found = 1;
                    break;
                }
            }
            // If we didn't know it, try probing the "OS Descriptor".
            //if (!found) {
            //   if (probe_device_descriptor(&dev, NULL)) {
                    /* Append this usb device to the MTP USB Device List */
            //        *mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, &dev, 0);
            //    }
                /*
                 * By thomas_-_s: Also append devices that are no MTP but PTP devices
                 * if this is commented out.
                 */
                /*
                else {
                  // Check whether the device is no USB hub but a PTP.
                  if ( dev->config != NULL &&dev->config->interface->altsetting->bInterfaceClass == LIBUSB_CLASS_PTP && dev->descriptor.bDeviceClass != LIBUSB_CLASS_HUB ) {
                 *mtp_device_list = append_to_mtpdevice_list(*mtp_device_list, dev, bus->location);
                  }
                }
                 */
            //}
        }
    }

    /* If nothing was found we end up here. */
    if (*mtp_device_list == NULL) {
        return LIBMTP_ERROR_NO_DEVICE_ATTACHED;
    }
    return LIBMTP_ERROR_NONE;
}

/**
 * Checks if a specific device with a certain bus and device
 * number has an MTP type device descriptor.
 *
 * @param busno the bus number of the device to check
 * @param deviceno the device number of the device to check
 * @return 1 if the device is MTP else 0
 */
int LIBMTP_Check_Specific_Device(int busno, int devno) {
    unsigned int nrofdevs;
    openusb_devid_t **devs = NULL;
    int i;

    init_usb();

    openusb_get_devids_by_bus(libmtp_openusb_handle, 0, devs, &nrofdevs);
    for (i = 0; i < nrofdevs; i++) {
        /*
            if (bus->location != busno)
              continue;
            if (dev->devnum != devno)
              continue;
         */
        if (probe_device_descriptor(devs[i], NULL))
            return 1;
    }
    return 0;
}

/**
 * Detect the raw MTP device descriptors and return a list of
 * of the devices found.
 *
 * @param devices a pointer to a variable that will hold
 *        the list of raw devices found. This may be NULL
 *        on return if the number of detected devices is zero.
 *        The user shall simply <code>free()</code> this
 *        variable when finished with the raw devices,
 *        in order to release memory.
 * @param numdevs a pointer to an integer that will hold
 *        the number of devices in the list. This may
 *        be 0.
 * @return 0 if successful, any other value means failure.
 */
LIBMTP_error_number_t LIBMTP_Detect_Raw_Devices(LIBMTP_raw_device_t ** devices,
        int * numdevs) {
    mtpdevice_list_t *devlist = NULL;
    mtpdevice_list_t *dev;
    LIBMTP_error_number_t ret;
    LIBMTP_raw_device_t *retdevs;
    int devs = 0;
    int i, j;

    ret = get_mtp_usb_device_list(&devlist);
    if (ret == LIBMTP_ERROR_NO_DEVICE_ATTACHED) {
        *devices = NULL;
        *numdevs = 0;
        return ret;
    } else if (ret != LIBMTP_ERROR_NONE) {
        LIBMTP_ERROR("LIBMTP PANIC: get_mtp_usb_device_list() "
                "error code: %d on line %d\n", ret, __LINE__);
        return ret;
    }

    // Get list size
    dev = devlist;
    while (dev != NULL) {
        devs++;
        dev = dev->next;
    }
    if (devs == 0) {
        *devices = NULL;
        *numdevs = 0;
        return LIBMTP_ERROR_NONE;
    }
    // Conjure a device list
    retdevs = (LIBMTP_raw_device_t *) malloc(sizeof (LIBMTP_raw_device_t) * devs);
    if (retdevs == NULL) {
        // Out of memory
        *devices = NULL;
        *numdevs = 0;
        return LIBMTP_ERROR_MEMORY_ALLOCATION;
    }
    dev = devlist;
    i = 0;
    while (dev != NULL) {
        int device_known = 0;
        struct usb_device_desc desc;

        openusb_parse_device_desc(libmtp_openusb_handle, dev->device, NULL, 0, &desc);
        // Assign default device info
        retdevs[i].device_entry.vendor = NULL;
        retdevs[i].device_entry.vendor_id = desc.idVendor;
        retdevs[i].device_entry.product = NULL;
        retdevs[i].device_entry.product_id = desc.idProduct;
        retdevs[i].device_entry.device_flags = 0x00000000U;
        // See if we can locate some additional vendor info and device flags
        for (j = 0; j < mtp_device_table_size; j++) {
            if (desc.idVendor == mtp_device_table[j].vendor_id &&
                    desc.idProduct == mtp_device_table[j].product_id) {
                device_known = 1;
                retdevs[i].device_entry.vendor = mtp_device_table[j].vendor;
                retdevs[i].device_entry.product = mtp_device_table[j].product;
                retdevs[i].device_entry.device_flags = mtp_device_table[j].device_flags;

                // This device is known to the developers
                LIBMTP_ERROR("Device %d (VID=%04x and PID=%04x) is a %s %s.\n",
                        i,
                        desc.idVendor,
                        desc.idProduct,
                        mtp_device_table[j].vendor,
                        mtp_device_table[j].product);
                break;
            }
        }
        if (!device_known) {
            // This device is unknown to the developers
            LIBMTP_ERROR("Device %d (VID=%04x and PID=%04x) is UNKNOWN.\n",
                    i,
                    desc.idVendor,
                    desc.idProduct);
            LIBMTP_ERROR("Please report this VID/PID and the device model to the libmtp development team\n");
            /*
             * Trying to get iManufacturer or iProduct from the device at this
             * point would require opening a device handle, that we don't want
             * to do right now. (Takes time for no good enough reason.)
             */
        }
        // Save the location on the bus
        retdevs[i].bus_location = 0;
        retdevs[i].devnum = openusb_get_devid(libmtp_openusb_handle, &dev->device);
        i++;
        dev = dev->next;
    }
    *devices = retdevs;
    *numdevs = i;
    free_mtpdevice_list(devlist);
    return LIBMTP_ERROR_NONE;
}

/**
 * This routine just dumps out low-level
 * USB information about the current device.
 * @param ptp_usb the USB device to get information from.
 */
void dump_usbinfo(PTP_USB *ptp_usb) {
    struct usb_device_desc desc;

    openusb_parse_device_desc(libmtp_openusb_handle, *ptp_usb->handle, NULL, 0, &desc);

    LIBMTP_INFO("   bcdUSB: %d\n", desc.bcdUSB);
    LIBMTP_INFO("   bDeviceClass: %d\n", desc.bDeviceClass);
    LIBMTP_INFO("   bDeviceSubClass: %d\n", desc.bDeviceSubClass);
    LIBMTP_INFO("   bDeviceProtocol: %d\n", desc.bDeviceProtocol);
    LIBMTP_INFO("   idVendor: %04x\n", desc.idVendor);
    LIBMTP_INFO("   idProduct: %04x\n", desc.idProduct);
    LIBMTP_INFO("   IN endpoint maxpacket: %d bytes\n", ptp_usb->inep_maxpacket);
    LIBMTP_INFO("   OUT endpoint maxpacket: %d bytes\n", ptp_usb->outep_maxpacket);
    LIBMTP_INFO("   Raw device info:\n");
    LIBMTP_INFO("      Bus location: %d\n", ptp_usb->rawdevice.bus_location);
    LIBMTP_INFO("      Device number: %d\n", ptp_usb->rawdevice.devnum);
    LIBMTP_INFO("      Device entry info:\n");
    LIBMTP_INFO("         Vendor: %s\n", ptp_usb->rawdevice.device_entry.vendor);
    LIBMTP_INFO("         Vendor id: 0x%04x\n", ptp_usb->rawdevice.device_entry.vendor_id);
    LIBMTP_INFO("         Product: %s\n", ptp_usb->rawdevice.device_entry.product);
    LIBMTP_INFO("         Vendor id: 0x%04x\n", ptp_usb->rawdevice.device_entry.product_id);
    LIBMTP_INFO("         Device flags: 0x%08x\n", ptp_usb->rawdevice.device_entry.device_flags);
    // TODO: (void) probe_device_descriptor(dev, stdout);
}

/**
 * Retrieve the apropriate playlist extension for this
 * device. Rather hacky at the moment. This is probably
 * desired by the managing software, but when creating
 * lists on the device itself you notice certain preferences.
 * @param ptp_usb the USB device to get suggestion for.
 * @return the suggested playlist extension.
 */
const char *get_playlist_extension(PTP_USB *ptp_usb) {
    static char creative_pl_extension[] = ".zpl";
    static char default_pl_extension[] = ".pla";
    struct usb_device_desc desc;
    openusb_parse_device_desc(libmtp_openusb_handle, *ptp_usb->handle, NULL, 0, &desc);
    if (desc.idVendor == 0x041e)
        return creative_pl_extension;
    return default_pl_extension;
}

static void
libusb_glue_debug(PTPParams *params, const char *format, ...) {
    va_list args;

    va_start(args, format);
    if (params->debug_func != NULL)
        params->debug_func(params->data, format, args);
    else {
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    va_end(args);
}

static void
libusb_glue_error(PTPParams *params, const char *format, ...) {
    va_list args;

    va_start(args, format);
    if (params->error_func != NULL)
        params->error_func(params->data, format, args);
    else {
        vfprintf(stderr, format, args);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
    va_end(args);
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
ptp_read_func(
        unsigned long size, PTPDataHandler *handler, void *data,
        unsigned long *readbytes,
        int readzero
        ) {
    PTP_USB *ptp_usb = (PTP_USB *) data;
    unsigned long toread = 0;
    int ret = 0;
    int xread;
    unsigned long curread = 0;
    unsigned long written;
    unsigned char *bytes;
    int expect_terminator_byte = 0;
    unsigned long usb_inep_maxpacket_size;
    unsigned long context_block_size_1;
    unsigned long context_block_size_2;
    uint16_t ptp_dev_vendor_id = ptp_usb->rawdevice.device_entry.vendor_id;

    //"iRiver" device special handling
    if (ptp_dev_vendor_id == 0x4102 || ptp_dev_vendor_id == 0x1006) {
	    usb_inep_maxpacket_size = ptp_usb->inep_maxpacket;
	    if (usb_inep_maxpacket_size == 0x400) {
		    context_block_size_1 = CONTEXT_BLOCK_SIZE_1 - 0x200;
		    context_block_size_2 = CONTEXT_BLOCK_SIZE_2 + 0x200;
	    }
	    else {
		    context_block_size_1 = CONTEXT_BLOCK_SIZE_1;
		    context_block_size_2 = CONTEXT_BLOCK_SIZE_2;
	    }
    }
    struct openusb_bulk_request bulk;
    // This is the largest block we'll need to read in.
    bytes = malloc(CONTEXT_BLOCK_SIZE);
    while (curread < size) {

        LIBMTP_USB_DEBUG("Remaining size to read: 0x%04lx bytes\n", size - curread);

        // check equal to condition here
        if (size - curread < CONTEXT_BLOCK_SIZE) {
            // this is the last packet
            toread = size - curread;
            // this is equivalent to zero read for these devices
            if (readzero && FLAG_NO_ZERO_READS(ptp_usb) && toread % 64 == 0) {
                toread += 1;
                expect_terminator_byte = 1;
            }
        } else if (ptp_dev_vendor_id == 0x4102 || ptp_dev_vendor_id == 0x1006) {
		//"iRiver" device special handling
		if (curread == 0)
			// we are first packet, but not last packet
			toread = context_block_size_1;
		else if (toread == context_block_size_1)
			toread = context_block_size_2;
		else if (toread == context_block_size_2)
			toread = context_block_size_1;
		else
			LIBMTP_INFO("unexpected toread size 0x%04x, 0x%04x remaining bytes\n",
				    (unsigned int) toread, (unsigned int) (size - curread));
	}
	else
		toread = CONTEXT_BLOCK_SIZE;

        LIBMTP_USB_DEBUG("Reading in 0x%04lx bytes\n", toread);

        /*
                ret = USB_BULK_READ(ptp_usb->handle,
                        ptp_usb->inep,
                        bytes,
                        toread,
                        &xread,
                        ptp_usb->timeout);
         */
        bulk.payload = bytes;
        bulk.length = toread;
        bulk.timeout = ptp_usb->timeout;
        bulk.flags = 0;
        bulk.next = NULL;
        ret = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->inep, &bulk);
        xread = bulk.result.transferred_bytes;
        LIBMTP_USB_DEBUG("Result of read: 0x%04x (%d bytes)\n", ret, xread);

        if (ret != OPENUSB_SUCCESS)
            return PTP_ERROR_IO;

        LIBMTP_USB_DEBUG("<==USB IN\n");
        if (xread == 0)
            LIBMTP_USB_DEBUG("Zero Read\n");
        else
            LIBMTP_USB_DATA(bytes, xread, 16);

        // want to discard extra byte
        if (expect_terminator_byte && xread == toread) {
            LIBMTP_USB_DEBUG("<==USB IN\nDiscarding extra byte\n");

            xread--;
        }

        int putfunc_ret = handler->putfunc(NULL, handler->priv, xread, bytes, &written);
        LIBMTP_USB_DEBUG("handler->putfunc ret = 0x%x\n", putfunc_ret);
        if (putfunc_ret != PTP_RC_OK)
            return putfunc_ret;

        ptp_usb->current_transfer_complete += xread;
        curread += xread;

        // Increase counters, call callback
        if (ptp_usb->callback_active) {
            if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
                // send last update and disable callback.
                ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
                ptp_usb->callback_active = 0;
            }
            if (ptp_usb->current_transfer_callback != NULL) {
                int ret;
                ret = ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
                        ptp_usb->current_transfer_total,
                        ptp_usb->current_transfer_callback_data);
                if (ret != 0) {
                    return PTP_ERROR_CANCEL;
                }
            }
        }

        if (xread < toread) /* short reads are common */
            break;
    }
    if (readbytes) *readbytes = curread;
    free(bytes);
    LIBMTP_USB_DEBUG("Pointer Updated\n");
    // there might be a zero packet waiting for us...
    if (readzero &&
            !FLAG_NO_ZERO_READS(ptp_usb) &&
            curread % ptp_usb->outep_maxpacket == 0) {
        unsigned char temp;
        int zeroresult = 0, xread;

        LIBMTP_USB_DEBUG("<==USB IN\n");
        LIBMTP_USB_DEBUG("Zero Read\n");

        /*
                zeroresult = USB_BULK_READ(ptp_usb->handle,
                        ptp_usb->inep,
                        &temp,
                        0,
                        &xread,
                        ptp_usb->timeout);
         */
        bulk.payload = &temp;
        bulk.length = 0;
        bulk.timeout = ptp_usb->timeout;
        bulk.flags = 0;
        bulk.next = NULL;
        ret = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->inep, &bulk);
        xread = bulk.result.transferred_bytes;
        if (zeroresult != OPENUSB_SUCCESS)
            LIBMTP_INFO("LIBMTP panic: unable to read in zero packet, response 0x%04x", zeroresult);
    }
    return PTP_RC_OK;
}

static short
ptp_write_func(
        unsigned long size,
        PTPDataHandler *handler,
        void *data,
        unsigned long *written
        ) {
    PTP_USB *ptp_usb = (PTP_USB *) data;
    unsigned long towrite = 0;
    int ret = 0;
    unsigned long curwrite = 0;
    unsigned char *bytes;

    struct openusb_bulk_request bulk;

    // This is the largest block we'll need to read in.
    bytes = malloc(CONTEXT_BLOCK_SIZE);
    if (!bytes) {
        return PTP_ERROR_IO;
    }
    while (curwrite < size) {
        unsigned long usbwritten = 0;
        int xwritten;

        towrite = size - curwrite;
        if (towrite > CONTEXT_BLOCK_SIZE) {
            towrite = CONTEXT_BLOCK_SIZE;
        } else {
            // This magic makes packets the same size that WMP send them.
            if (towrite > ptp_usb->outep_maxpacket && towrite % ptp_usb->outep_maxpacket != 0) {
                towrite -= towrite % ptp_usb->outep_maxpacket;
            }
        }
        int getfunc_ret = handler->getfunc(NULL, handler->priv, towrite, bytes, &towrite);
        if (getfunc_ret != PTP_RC_OK)
            return getfunc_ret;
        while (usbwritten < towrite) {
            /*
                        ret = USB_BULK_WRITE(ptp_usb->handle,
                                ptp_usb->outep,
                                bytes + usbwritten,
                                towrite - usbwritten,
                                &xwritten,
                                ptp_usb->timeout);
             */
            bulk.payload = bytes + usbwritten;
            bulk.length = towrite - usbwritten;
            bulk.timeout = ptp_usb->timeout;
            bulk.flags = 0;
            bulk.next = NULL;
            ret = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->outep, &bulk);
            xwritten = bulk.result.transferred_bytes;

            LIBMTP_USB_DEBUG("USB OUT==>\n");

            if (ret != OPENUSB_SUCCESS) {
                return PTP_ERROR_IO;
            }
            LIBMTP_USB_DATA(bytes + usbwritten, xwritten, 16);
            // check for result == 0 perhaps too.
            // Increase counters
            ptp_usb->current_transfer_complete += xwritten;
            curwrite += xwritten;
            usbwritten += xwritten;
        }
        // call callback
        if (ptp_usb->callback_active) {
            if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
                // send last update and disable callback.
                ptp_usb->current_transfer_complete = ptp_usb->current_transfer_total;
                ptp_usb->callback_active = 0;
            }
            if (ptp_usb->current_transfer_callback != NULL) {
                int ret;
                ret = ptp_usb->current_transfer_callback(ptp_usb->current_transfer_complete,
                        ptp_usb->current_transfer_total,
                        ptp_usb->current_transfer_callback_data);
                if (ret != 0) {
                    return PTP_ERROR_CANCEL;
                }
            }
        }
        if (xwritten < towrite) /* short writes happen */
            break;
    }
    free(bytes);
    if (written) {
        *written = curwrite;
    }

    // If this is the last transfer send a zero write if required
    if (ptp_usb->current_transfer_complete >= ptp_usb->current_transfer_total) {
        if ((towrite % ptp_usb->outep_maxpacket) == 0) {
            int xwritten;

            LIBMTP_USB_DEBUG("USB OUT==>\n");
            LIBMTP_USB_DEBUG("Zero Write\n");

            /*
                        ret = USB_BULK_WRITE(ptp_usb->handle,
                                ptp_usb->outep,
                                (unsigned char *) "x",
                                0,
                                &xwritten,
                                ptp_usb->timeout);
             */
            bulk.payload = (unsigned char *) "x";
            bulk.length = 0;
            bulk.timeout = ptp_usb->timeout;
            bulk.flags = 0;
            bulk.next = NULL;
            ret = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->outep, &bulk);
            xwritten = bulk.result.transferred_bytes;
        }
    }

    if (ret != OPENUSB_SUCCESS)
        return PTP_ERROR_IO;
    return PTP_RC_OK;
}

/* memory data get/put handler */
typedef struct {
    unsigned char *data;
    unsigned long size, curoff;
} PTPMemHandlerPrivate;

static uint16_t
memory_getfunc(PTPParams* params, void* private,
        unsigned long wantlen, unsigned char *data,
        unsigned long *gotlen
        ) {
    PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*) private;
    unsigned long tocopy = wantlen;

    if (priv->curoff + tocopy > priv->size)
        tocopy = priv->size - priv->curoff;
    memcpy(data, priv->data + priv->curoff, tocopy);
    priv->curoff += tocopy;
    *gotlen = tocopy;
    return PTP_RC_OK;
}

static uint16_t
memory_putfunc(PTPParams* params, void* private,
        unsigned long sendlen, unsigned char *data,
        unsigned long *putlen
        ) {
    PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*) private;

    if (priv->curoff + sendlen > priv->size) {
        priv->data = realloc(priv->data, priv->curoff + sendlen);
        priv->size = priv->curoff + sendlen;
    }
    memcpy(priv->data + priv->curoff, data, sendlen);
    priv->curoff += sendlen;
    *putlen = sendlen;
    return PTP_RC_OK;
}

/* init private struct for receiving data. */
static uint16_t
ptp_init_recv_memory_handler(PTPDataHandler *handler) {
    PTPMemHandlerPrivate* priv;
    priv = malloc(sizeof (PTPMemHandlerPrivate));
    handler->priv = priv;
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
    priv = malloc(sizeof (PTPMemHandlerPrivate));
    if (!priv){
        return PTP_RC_GeneralError;
    }
    handler->priv = priv;
    handler->getfunc = memory_getfunc;
    handler->putfunc = memory_putfunc;
    priv->data = data;
    priv->size = len;
    priv->curoff = 0;
    return PTP_RC_OK;
}

/* free private struct + data */
static uint16_t
ptp_exit_send_memory_handler(PTPDataHandler *handler) {
    PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*) handler->priv;
    /* data is owned by caller */
    free(priv);
    return PTP_RC_OK;
}

/* hand over our internal data to caller */
static uint16_t
ptp_exit_recv_memory_handler(PTPDataHandler *handler,
        unsigned char **data, unsigned long *size
        ) {
    PTPMemHandlerPrivate* priv = (PTPMemHandlerPrivate*) handler->priv;
    *data = priv->data;
    *size = priv->size;
    free(priv);
    return PTP_RC_OK;
}

/* send / receive functions */

uint16_t
ptp_usb_sendreq(PTPParams* params, PTPContainer* req) {
    uint16_t ret;
    PTPUSBBulkContainer usbreq;
    PTPDataHandler memhandler;
    unsigned long written = 0;
    unsigned long towrite;

    char txt[256];

    (void) ptp_render_opcode(params, req->Code, sizeof (txt), txt);
    LIBMTP_USB_DEBUG("REQUEST: 0x%04x, %s\n", req->Code, txt);

    /* build appropriate USB container */
    usbreq.length = htod32(PTP_USB_BULK_REQ_LEN -
            (sizeof (uint32_t)*(5 - req->Nparam)));
    usbreq.type = htod16(PTP_USB_CONTAINER_COMMAND);
    usbreq.code = htod16(req->Code);
    usbreq.trans_id = htod32(req->Transaction_ID);
    usbreq.payload.params.param1 = htod32(req->Param1);
    usbreq.payload.params.param2 = htod32(req->Param2);
    usbreq.payload.params.param3 = htod32(req->Param3);
    usbreq.payload.params.param4 = htod32(req->Param4);
    usbreq.payload.params.param5 = htod32(req->Param5);
    /* send it to responder */
    towrite = PTP_USB_BULK_REQ_LEN - (sizeof (uint32_t)*(5 - req->Nparam));
    ptp_init_send_memory_handler(&memhandler, (unsigned char*) &usbreq, towrite);
    ret = ptp_write_func(
            towrite,
            &memhandler,
            params->data,
            &written
            );
    ptp_exit_send_memory_handler(&memhandler);
    if (ret != PTP_RC_OK && ret != PTP_ERROR_CANCEL) {
        ret = PTP_ERROR_IO;
    }
    if (written != towrite && ret != PTP_ERROR_CANCEL && ret != PTP_ERROR_IO) {
        libusb_glue_error(params,
                "PTP: request code 0x%04x sending req wrote only %ld bytes instead of %d",
                req->Code, written, towrite
                );
        ret = PTP_ERROR_IO;
    }
    return ret;
}

uint16_t
ptp_usb_senddata(PTPParams* params, PTPContainer* ptp,
        uint64_t size, PTPDataHandler *handler
        ) {
    uint16_t ret;
    int wlen, datawlen;
    unsigned long written;
    PTPUSBBulkContainer usbdata;
    uint64_t bytes_left_to_transfer;
    PTPDataHandler memhandler;

    LIBMTP_USB_DEBUG("SEND DATA PHASE\n");

    /* build appropriate USB container */
    usbdata.length = htod32(PTP_USB_BULK_HDR_LEN + size);
    usbdata.type = htod16(PTP_USB_CONTAINER_DATA);
    usbdata.code = htod16(ptp->Code);
    usbdata.trans_id = htod32(ptp->Transaction_ID);

    ((PTP_USB*) params->data)->current_transfer_complete = 0;
    ((PTP_USB*) params->data)->current_transfer_total = size + PTP_USB_BULK_HDR_LEN;

    if (params->split_header_data) {
        datawlen = 0;
        wlen = PTP_USB_BULK_HDR_LEN;
    } else {
        unsigned long gotlen;
        /* For all camera devices. */
        datawlen = (size < PTP_USB_BULK_PAYLOAD_LEN_WRITE) ? size : PTP_USB_BULK_PAYLOAD_LEN_WRITE;
        wlen = PTP_USB_BULK_HDR_LEN + datawlen;

        ret = handler->getfunc(params, handler->priv, datawlen, usbdata.payload.data, &gotlen);
        if (ret != PTP_RC_OK){
            return ret;
        }
            
        if (gotlen != datawlen){
            return PTP_RC_GeneralError;
        }
    }
    ptp_init_send_memory_handler(&memhandler, (unsigned char *) &usbdata, wlen);
    /* send first part of data */
    ret = ptp_write_func(wlen, &memhandler, params->data, &written);
    ptp_exit_send_memory_handler(&memhandler);
    if (ret != PTP_RC_OK) {
        return ret;
    }
    if (size <= datawlen) return ret;
    /* if everything OK send the rest */
    bytes_left_to_transfer = size - datawlen;
    ret = PTP_RC_OK;
    while (bytes_left_to_transfer > 0) {
        ret = ptp_write_func(bytes_left_to_transfer, handler, params->data, &written);
        if (ret != PTP_RC_OK){
            break;
        }
        if (written == 0) {
            ret = PTP_ERROR_IO;
            break;
        }
        bytes_left_to_transfer -= written;
    }
    if (ret != PTP_RC_OK && ret != PTP_ERROR_CANCEL)
        ret = PTP_ERROR_IO;
    return ret;
}

static uint16_t ptp_usb_getpacket(PTPParams *params,
        PTPUSBBulkContainer *packet, unsigned long *rlen) {
    PTPDataHandler memhandler;
    uint16_t ret;
    unsigned char *x = NULL;
    unsigned long packet_size;
    PTP_USB *ptp_usb = (PTP_USB *) params->data;

    packet_size = ptp_usb->inep_maxpacket;

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
    ptp_init_recv_memory_handler(&memhandler);
    ret = ptp_read_func(packet_size, &memhandler, params->data, rlen, 0);
    ptp_exit_recv_memory_handler(&memhandler, &x, rlen);
    if (x) {
        memcpy(packet, x, *rlen);
        free(x);
    }
    return ret;
}

uint16_t
ptp_usb_getdata(PTPParams* params, PTPContainer* ptp, PTPDataHandler *handler) {
    uint16_t ret;
    PTPUSBBulkContainer usbdata;
    unsigned long written;
    PTP_USB *ptp_usb = (PTP_USB *) params->data;
    int putfunc_ret;

    LIBMTP_USB_DEBUG("GET DATA PHASE\n");

    struct openusb_bulk_request bulk;

    memset(&usbdata, 0, sizeof (usbdata));
    do {
        unsigned long len, rlen;

        ret = ptp_usb_getpacket(params, &usbdata, &rlen);
        if (ret != PTP_RC_OK) {
            ret = PTP_ERROR_IO;
            break;
        }
        if (dtoh16(usbdata.type) != PTP_USB_CONTAINER_DATA) {
            ret = PTP_ERROR_DATA_EXPECTED;
            break;
        }
        if (dtoh16(usbdata.code) != ptp->Code) {
            if (FLAG_IGNORE_HEADER_ERRORS(ptp_usb)) {
                libusb_glue_debug(params, "ptp2/ptp_usb_getdata: detected a broken "
                        "PTP header, code field insane, expect problems! (But continuing)");
                // Repair the header, so it won't wreak more havoc, don't just ignore it.
                // Typically these two fields will be broken.
                usbdata.code = htod16(ptp->Code);
                usbdata.trans_id = htod32(ptp->Transaction_ID);
                ret = PTP_RC_OK;
            } else {
                ret = dtoh16(usbdata.code);
                // This filters entirely insane garbage return codes, but still
                // makes it possible to return error codes in the code field when
                // getting data. It appears Windows ignores the contents of this
                // field entirely.
                if (ret < PTP_RC_Undefined || ret > PTP_RC_SpecificationOfDestinationUnsupported) {
                    libusb_glue_debug(params, "ptp2/ptp_usb_getdata: detected a broken "
                            "PTP header, code field insane.");
                    ret = PTP_ERROR_IO;
                }
                break;
            }
        }
        if (rlen == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ) {
            /* Copy first part of data to 'data' */
            putfunc_ret =
                    handler->putfunc(
                    params, handler->priv, rlen - PTP_USB_BULK_HDR_LEN, usbdata.payload.data,
                    &written
                    );
            if (putfunc_ret != PTP_RC_OK)
                return putfunc_ret;

            /* stuff data directly to passed data handler */
            while (1) {
                unsigned long readdata;
                uint16_t xret;

                xret = ptp_read_func(
                        0x20000000,
                        handler,
                        params->data,
                        &readdata,
                        0
                        );
                if (xret != PTP_RC_OK)
                    return xret;
                if (readdata < 0x20000000)
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
                        (uint8_t *) & usbdata + packlen, surplen);
                params->response_packet_size = surplen;
                /* Ignore reading one extra byte if device flags have been set */
            } else if (!FLAG_NO_ZERO_READS(ptp_usb) &&
                    (rlen - dtoh32(usbdata.length) == 1)) {
                libusb_glue_debug(params, "ptp2/ptp_usb_getdata: read %d bytes "
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
        len = dtoh32(usbdata.length) - PTP_USB_BULK_HDR_LEN;

        /* autodetect split header/data MTP devices */
        if (dtoh32(usbdata.length) > 12 && (rlen == 12))
            params->split_header_data = 1;

        /* Copy first part of data to 'data' */
        putfunc_ret =
                handler->putfunc(
                params, handler->priv, rlen - PTP_USB_BULK_HDR_LEN,
                usbdata.payload.data,
                &written
                );
        if (putfunc_ret != PTP_RC_OK)
            return putfunc_ret;

        if (FLAG_NO_ZERO_READS(ptp_usb) &&
                len + PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ) {

            LIBMTP_USB_DEBUG("Reading in extra terminating byte\n");

            // need to read in extra byte and discard it
            int result = 0, xread;
            unsigned char byte = 0;

            /*
                        result = USB_BULK_READ(ptp_usb->handle,
                                ptp_usb->inep,
                                &byte,
                                1,
                                &xread,
                                ptp_usb->timeout);
             */

            bulk.payload = &byte;
            bulk.length = 1;
            bulk.timeout = ptp_usb->timeout;
            bulk.flags = 0;
            bulk.next = NULL;
            result = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->inep, &bulk);
            xread = bulk.result.transferred_bytes;

            if (result != 1)
                LIBMTP_INFO("Could not read in extra byte for PTP_USB_BULK_HS_MAX_PACKET_LEN_READ long file, return value 0x%04x\n", result);
        } else if (len + PTP_USB_BULK_HDR_LEN == PTP_USB_BULK_HS_MAX_PACKET_LEN_READ && params->split_header_data == 0) {
            int zeroresult = 0, xread;
            unsigned char zerobyte = 0;

            LIBMTP_INFO("Reading in zero packet after header\n");
            /*
                        zeroresult = USB_BULK_READ(ptp_usb->handle,
                                ptp_usb->inep,
                                &zerobyte,
                                0,
                                &xread,
                                ptp_usb->timeout);
             */

            bulk.payload = &zerobyte;
            bulk.length = 0;
            bulk.timeout = ptp_usb->timeout;
            bulk.flags = 0;
            bulk.next = NULL;
            zeroresult = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->inep, &bulk);
            xread = bulk.result.transferred_bytes;

            if (zeroresult != 0)
                LIBMTP_INFO("LIBMTP panic: unable to read in zero packet, response 0x%04x", zeroresult);
        }

        /* Is that all of data? */
        if (len + PTP_USB_BULK_HDR_LEN <= rlen) {
            break;
        }

        ret = ptp_read_func(len - (rlen - PTP_USB_BULK_HDR_LEN),
                handler,
                params->data, &rlen, 1);

        if (ret != PTP_RC_OK) {
            break;
        }
    } while (0);
    return ret;
}

uint16_t
ptp_usb_getresp(PTPParams* params, PTPContainer* resp) {
    uint16_t ret;
    unsigned long rlen;
    PTPUSBBulkContainer usbresp;
    PTP_USB *ptp_usb = (PTP_USB *) (params->data);


    LIBMTP_USB_DEBUG("RESPONSE: ");
    memset(&usbresp, 0, sizeof (usbresp));
    /* read response, it should never be longer than sizeof(usbresp) */
    ret = ptp_usb_getpacket(params, &usbresp, &rlen);
    // Fix for bevahiour reported by Scott Snyder on Samsung YP-U3. The player
    // sends a packet containing just zeroes of length 2 (up to 4 has been seen too)
    // after a NULL packet when it should send the response. This code ignores
    // such illegal packets.
    while (ret == PTP_RC_OK && rlen < PTP_USB_BULK_HDR_LEN && usbresp.length == 0) {
        libusb_glue_debug(params, "ptp_usb_getresp: detected short response "
                "of %d bytes, expect problems! (re-reading "
                "response), rlen");
        ret = ptp_usb_getpacket(params, &usbresp, &rlen);
    }
    if (ret != PTP_RC_OK) {
        ret = PTP_ERROR_IO;
    } else
        if (dtoh16(usbresp.type) != PTP_USB_CONTAINER_RESPONSE) {
        ret = PTP_ERROR_RESP_EXPECTED;
    } else
        if (dtoh16(usbresp.code) != resp->Code) {
        ret = dtoh16(usbresp.code);
    }

    LIBMTP_USB_DEBUG("%04x\n", ret);
    if (ret != PTP_RC_OK) {
        /*		libusb_glue_error (params,
                        "PTP: request code 0x%04x getting resp error 0x%04x",
                                resp->Code, ret);*/
        return ret;
    }
    /* build an appropriate PTPContainer */
    resp->Code = dtoh16(usbresp.code);
    resp->SessionID = params->session_id;
    resp->Transaction_ID = dtoh32(usbresp.trans_id);
    if (FLAG_IGNORE_HEADER_ERRORS(ptp_usb)) {
        if (resp->Transaction_ID != params->transaction_id - 1) {
            libusb_glue_debug(params, "ptp_usb_getresp: detected a broken "
                    "PTP header, transaction ID insane, expect "
                    "problems! (But continuing)");
            // Repair the header, so it won't wreak more havoc.
            resp->Transaction_ID = params->transaction_id - 1;
        }
    }
    resp->Param1 = dtoh32(usbresp.payload.params.param1);
    resp->Param2 = dtoh32(usbresp.payload.params.param2);
    resp->Param3 = dtoh32(usbresp.payload.params.param3);
    resp->Param4 = dtoh32(usbresp.payload.params.param4);
    resp->Param5 = dtoh32(usbresp.payload.params.param5);
    return ret;
}

/* Event handling functions */

/* PTP Events wait for or check mode */
#define PTP_EVENT_CHECK			0x0000	/* waits for */
#define PTP_EVENT_CHECK_FAST		0x0001	/* checks */

static inline uint16_t
ptp_usb_event(PTPParams* params, PTPContainer* event, int wait) {
    uint16_t ret;
    int result, xread;
    unsigned long rlen;
    PTPUSBEventContainer usbevent;
    PTP_USB *ptp_usb = (PTP_USB *) (params->data);

    struct openusb_bulk_request bulk;

    memset(&usbevent, 0, sizeof (usbevent));

    if ((params == NULL) || (event == NULL))
        return PTP_ERROR_BADPARAM;
    ret = PTP_RC_OK;
    switch (wait) {
        case PTP_EVENT_CHECK:

            /*
                        result = USB_BULK_READ(ptp_usb->handle,
                                ptp_usb->intep,
                                (unsigned char *) &usbevent,
                                sizeof (usbevent),
                                &xread,
                                0);
             */
            bulk.payload = (unsigned char *) &usbevent;
            bulk.length = sizeof (usbevent);
            bulk.timeout = ptp_usb->timeout;
            bulk.flags = 0;
            bulk.next = NULL;
            result = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->intep, &bulk);
            xread = bulk.result.transferred_bytes;

            if (result == 0) {
                /*
                                result = USB_BULK_READ(ptp_usb->handle,
                                    ptp_usb->intep,
                                    (unsigned char *) &usbevent,
                                    sizeof (usbevent),
                                    &xread,
                                    0);
                 */
                bulk.payload = (unsigned char *) &usbevent;
                bulk.length = sizeof (usbevent);
                bulk.timeout = ptp_usb->timeout;
                bulk.flags = 0;
                bulk.next = NULL;
                result = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->intep, &bulk);
                xread = bulk.result.transferred_bytes;
            }
            if (result < 0) ret = PTP_ERROR_IO;
            break;
        case PTP_EVENT_CHECK_FAST:
            /*
                        result = USB_BULK_READ(ptp_usb->handle,
                                ptp_usb->intep,
                                (unsigned char *) &usbevent,
                                sizeof (usbevent),
                                &xread,
                                ptp_usb->timeout);
             */
            bulk.payload = (unsigned char *) &usbevent;
            bulk.length = sizeof (usbevent);
            bulk.timeout = ptp_usb->timeout;
            bulk.flags = 0;
            bulk.next = NULL;
            result = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->intep, &bulk);
            xread = bulk.result.transferred_bytes;

            if (result == 0) {
                /*
                                result = USB_BULK_READ(ptp_usb->handle,
                                        ptp_usb->intep,
                                        (unsigned char *) &usbevent,
                                        sizeof (usbevent),
                                        &xread,
                                        ptp_usb->timeout);
                 */
                bulk.payload = (unsigned char *) &usbevent;
                bulk.length = sizeof (usbevent);
                bulk.timeout = ptp_usb->timeout;
                bulk.flags = 0;
                bulk.next = NULL;
                result = openusb_bulk_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->intep, &bulk);
                xread = bulk.result.transferred_bytes;
            }
            if (result < 0) ret = PTP_ERROR_IO;
            break;
        default:
            ret = PTP_ERROR_BADPARAM;
            break;
    }
    if (ret != PTP_RC_OK) {
        libusb_glue_error(params,
                "PTP: reading event an error 0x%04x occurred", ret);
        return PTP_ERROR_IO;
    }
    rlen = result;
    if (rlen < 8) {
        libusb_glue_error(params,
                "PTP: reading event an short read of %ld bytes occurred", rlen);
        return PTP_ERROR_IO;
    }
    /* if we read anything over interrupt endpoint it must be an event */
    /* build an appropriate PTPContainer */
    event->Code = dtoh16(usbevent.code);
    event->SessionID = params->session_id;
    event->Transaction_ID = dtoh32(usbevent.trans_id);
    event->Param1 = dtoh32(usbevent.param1);
    event->Param2 = dtoh32(usbevent.param2);
    event->Param3 = dtoh32(usbevent.param3);
    return ret;
}

uint16_t
ptp_usb_event_check(PTPParams* params, PTPContainer* event) {

    return ptp_usb_event(params, event, PTP_EVENT_CHECK_FAST);
}

uint16_t
ptp_usb_event_wait(PTPParams* params, PTPContainer* event) {

    return ptp_usb_event(params, event, PTP_EVENT_CHECK);
}

uint16_t
ptp_usb_control_cancel_request(PTPParams *params, uint32_t transactionid) {
    PTP_USB *ptp_usb = (PTP_USB *) (params->data);
    int ret;
    unsigned char buffer[6];

    htod16a(&buffer[0], PTP_EC_CancelTransaction);
    htod32a(&buffer[2], transactionid);
    /*
            ret = libusb_control_transfer(ptp_usb->handle,
                                  LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
                                  0x64, 0x0000, 0x0000,
                                  buffer,
                                  sizeof(buffer),
                                  ptp_usb->timeout);
     */
    struct openusb_ctrl_request ctrl;
    ctrl.setup.bmRequestType = USB_REQ_TYPE_CLASS | USB_RECIP_INTERFACE;
    ctrl.setup.bRequest = 0x64;
    ctrl.setup.wValue = 0;
    ctrl.setup.wIndex = 0;
    ctrl.payload = (unsigned char *)&buffer; // Out
    ctrl.length = sizeof (buffer);
    ctrl.timeout = ptp_usb->timeout;
    ctrl.next = NULL;
    ctrl.flags = 0;

    ret = openusb_ctrl_xfer(*ptp_usb->handle, ptp_usb->interface, ptp_usb->outep, &ctrl);
    if (ctrl.result.transferred_bytes < sizeof (buffer))
        return PTP_ERROR_IO;
    return PTP_RC_OK;
}

static int init_ptp_usb(PTPParams* params, PTP_USB* ptp_usb, openusb_dev_handle_t* dev) {
    openusb_dev_handle_t device_handle;
    unsigned char buf[255];
    int ret, usbresult;

    params->sendreq_func = ptp_usb_sendreq;
    params->senddata_func = ptp_usb_senddata;
    params->getresp_func = ptp_usb_getresp;
    params->getdata_func = ptp_usb_getdata;
    params->cancelreq_func = ptp_usb_control_cancel_request;
    params->data = ptp_usb;
    params->transaction_id = 0;
    /*
     * This is hardcoded here since we have no devices whatsoever that are BE.
     * Change this the day we run into our first BE device (if ever).
     */
    params->byteorder = PTP_DL_LE;

    ptp_usb->timeout = get_timeout(ptp_usb);

    ret = openusb_open_device(libmtp_openusb_handle, *dev, USB_INIT_DEFAULT, &device_handle);
    if (ret != OPENUSB_SUCCESS) {
        perror("usb_open()");
        return -1;
    }
    ptp_usb->handle = malloc(sizeof(openusb_dev_handle_t));
    *ptp_usb->handle = device_handle;
    /*
     * If this device is known to be wrongfully claimed by other kernel
     * drivers (such as mass storage), then try to unload it to make it
     * accessible from user space.
     * Note: OpenUSB doesn't support this type of operation?
     */
    /*
      if (FLAG_UNLOAD_DRIVER(ptp_usb) &&
          libusb_kernel_driver_active (device_handle, ptp_usb->interface)
      ) {
          if (OPENUSB_SUCCESS != libusb_detach_kernel_driver (device_handle, ptp_usb->interface)) {
            return -1;
          }
      }
     */
    // It seems like on kernel 2.6.31 if we already have it open on another
    // pthread in our app, we'll get an error if we try to claim it again,
    // but that error is harmless because our process already claimed the interface
    usbresult = openusb_claim_interface(device_handle, ptp_usb->interface, USB_INIT_DEFAULT);

    if (usbresult != 0)
        fprintf(stderr, "ignoring usb_claim_interface = %d", usbresult);

    if (FLAG_SWITCH_MODE_BLACKBERRY(ptp_usb)) {
        int ret;

        // FIXME : Only for BlackBerry Storm
        // What does it mean? Maybe switch mode...
        // This first control message is absolutely necessary
        usleep(1000);
        /*
                ret = libusb_control_transfer(device_handle,
                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                        0xaa, 0x00, 0x04, buf, 0x40, 1000);
         */
        struct openusb_ctrl_request ctrl;
        ctrl.setup.bmRequestType = USB_REQ_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN;
        ctrl.setup.bRequest = 0xaa;
        ctrl.setup.wValue = 0;
        ctrl.setup.wIndex = 4;
        ctrl.payload = (unsigned char *)&buf; // Out
        ctrl.length = 0x40;
        ctrl.timeout = 1000;
        ctrl.next = NULL;
        ctrl.flags = 0;

        ret = openusb_ctrl_xfer(device_handle, ptp_usb->interface, ptp_usb->outep, &ctrl);
        LIBMTP_USB_DEBUG("BlackBerry magic part 1:\n");
        LIBMTP_USB_DATA(buf, ctrl.result.transferred_bytes, 16);

        usleep(1000);
        // This control message is unnecessary
        /*
                ret = libusb_control_transfer(device_handle,
                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                        0xa5, 0x00, 0x01, buf, 0x02, 1000);
         */
        ctrl.setup.bmRequestType = USB_REQ_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN;
        ctrl.setup.bRequest = 0xa5;
        ctrl.setup.wValue = 0;
        ctrl.setup.wIndex = 1;
        ctrl.payload = (unsigned char *)&buf; // Out
        ctrl.length = 0x02;
        ctrl.timeout = 1000;
        ctrl.next = NULL;
        ctrl.flags = 0;

        ret = openusb_ctrl_xfer(device_handle, ptp_usb->interface, ptp_usb->outep, &ctrl);
        LIBMTP_USB_DEBUG("BlackBerry magic part 2:\n");
        LIBMTP_USB_DATA(buf, ctrl.result.transferred_bytes, 16);

        usleep(1000);
        // This control message is unnecessary
        /*
                ret = libusb_control_transfer(device_handle,
                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                        0xa8, 0x00, 0x01, buf, 0x05, 1000);
         */
        ctrl.setup.bmRequestType = USB_REQ_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN;
        ctrl.setup.bRequest = 0xa8;
        ctrl.setup.wValue = 0;
        ctrl.setup.wIndex = 1;
        ctrl.payload = (unsigned char *)&buf; // Out
        ctrl.length = 0x05;
        ctrl.timeout = 1000;
        ctrl.next = NULL;
        ctrl.flags = 0;

        ret = openusb_ctrl_xfer(device_handle, ptp_usb->interface, ptp_usb->outep, &ctrl);
        LIBMTP_USB_DEBUG("BlackBerry magic part 3:\n");
        LIBMTP_USB_DATA(buf, ctrl.result.transferred_bytes, 16);

        usleep(1000);
        // This control message is unnecessary
        /*
                ret = libusb_control_transfer(device_handle,
                        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
                        0xa8, 0x00, 0x01, buf, 0x11, 1000);
         */
        ctrl.setup.bmRequestType = USB_REQ_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN;
        ctrl.setup.bRequest = 0xa8;
        ctrl.setup.wValue = 0;
        ctrl.setup.wIndex = 1;
        ctrl.payload = (unsigned char *)&buf; // Out
        ctrl.length = 0x11;
        ctrl.timeout = 1000;
        ctrl.next = NULL;
        ctrl.flags = 0;

        ret = openusb_ctrl_xfer(device_handle, ptp_usb->interface, ptp_usb->outep, &ctrl);
        LIBMTP_USB_DEBUG("BlackBerry magic part 4:\n");
        LIBMTP_USB_DATA(buf, ctrl.result.transferred_bytes, 16);

        usleep(1000);
    }
    return 0;
}

static void clear_stall(PTP_USB* ptp_usb) {
    uint16_t status;
    int ret;

    /* check the inep status */
    /*
        status = 0;
        ret = usb_get_endpoint_status(ptp_usb, ptp_usb->inep, &status);
        if (ret < 0) {
            perror("inep: usb_get_endpoint_status()");
        } else if (status) {
            LIBMTP_INFO("Clearing stall on IN endpoint\n");
            ret = libusb_clear_halt(ptp_usb->handle, ptp_usb->inep);
            if (ret != OPENUSB_SUCCESS) {
                perror("usb_clear_stall_feature()");
            }
        }

        /* check the outep status */
    /*status = 0;
    ret = usb_get_endpoint_status(ptp_usb, ptp_usb->outep, &status);
    if (ret < 0) {
        perror("outep: usb_get_endpoint_status()");
    } else if (status) {
        LIBMTP_INFO("Clearing stall on OUT endpoint\n");
        ret = libusb_clear_halt(ptp_usb->handle, ptp_usb->outep);
        if (ret != OPENUSB_SUCCESS) {
            perror("usb_clear_stall_feature()");
        }
    }
     */

    /* TODO: do we need this for INTERRUPT (ptp_usb->intep) too? */
}

static void clear_halt(PTP_USB* ptp_usb) {
    int ret;

    /*
        ret = libusb_clear_halt(ptp_usb->handle, ptp_usb->inep);
        if (ret < 0) {
            perror("usb_clear_halt() on IN endpoint");
        }
        ret = libusb_clear_halt(ptp_usb->handle, ptp_usb->outep);
        if (ret < 0) {
            perror("usb_clear_halt() on OUT endpoint");
        }
        ret = libusb_clear_halt(ptp_usb->handle, ptp_usb->intep);
        if (ret < 0) {
            perror("usb_clear_halt() on INTERRUPT endpoint");
        }
     */
}

static void close_usb(PTP_USB* ptp_usb) {
    if (!FLAG_NO_RELEASE_INTERFACE(ptp_usb)) {
        /*
         * Clear any stalled endpoints
         * On misbehaving devices designed for Windows/Mac, quote from:
         * http://www2.one-eyed-alien.net/~mdharm/linux-usb/target_offenses.txt
         * Device does Bad Things(tm) when it gets a GET_STATUS after CLEAR_HALT
         * (...) Windows, when clearing a stall, only sends the CLEAR_HALT command,
         * and presumes that the stall has cleared.  Some devices actually choke
         * if the CLEAR_HALT is followed by a GET_STATUS (used to determine if the
         * STALL is persistant or not).
         */
        clear_stall(ptp_usb);
        // Clear halts on any endpoints
        clear_halt(ptp_usb);
        // Added to clear some stuff on the OUT endpoint
        // TODO: is this good on the Mac too?
        // HINT: some devices may need that you comment these two out too.
        //libusb_clear_halt(ptp_usb->handle, ptp_usb->outep);
        //libusb_release_interface(ptp_usb->handle, (int) ptp_usb->interface);
    }
    if (FLAG_FORCE_RESET_ON_CLOSE(ptp_usb)) {
        /*
         * Some devices really love to get reset after being
         * disconnected. Again, since Windows never disconnects
         * a device closing behaviour is seldom or never exercised
         * on devices when engineered and often error prone.
         * Reset may help some.
         */
        openusb_reset(*ptp_usb->handle);
    }
    openusb_close_device(*ptp_usb->handle);
}

/**
 * Self-explanatory?
 */
static int find_interface_and_endpoints(openusb_dev_handle_t *dev,
	uint8_t *conf,
        uint8_t *interface,
        uint8_t *altsetting,
        int* inep,
        int* inep_maxpacket,
        int* outep,
        int *outep_maxpacket,
        int* intep) {
    uint8_t i;
    int ret;
    struct usb_device_desc desc;

    ret = openusb_parse_device_desc(libmtp_openusb_handle, *dev, NULL, 0, &desc);
    if (ret != OPENUSB_SUCCESS) return -1;

    // Loop over the device configurations
    for (i = 0; i < desc.bNumConfigurations; i++) {
        uint8_t j;
        struct usb_config_desc config;

        ret = openusb_parse_config_desc(libmtp_openusb_handle, *dev, NULL, 0, i, &config);
        if (ret != OPENUSB_SUCCESS) continue;
	*conf = desc.bConfigurationValue;
        // Loop over each configurations interfaces
        for (j = 0; j < config.bNumInterfaces; j++) {
            uint8_t k;
            uint8_t no_ep;
            int found_inep = 0;
            int found_outep = 0;
            int found_intep = 0;
            struct usb_endpoint_desc ep;
            struct usb_interface_desc ifcdesc;
            openusb_parse_interface_desc(libmtp_openusb_handle, *dev, NULL, 0, i, j, 0, &ifcdesc);
            // MTP devices shall have 3 endpoints, ignore those interfaces
            // that haven't.
            no_ep = ifcdesc.bNumEndpoints;
            if (no_ep != 3)
                continue;
            *interface = ifcdesc.bInterfaceNumber;
	    *altsetting = ifcdesc.bAlternateSetting;
            // Loop over the three endpoints to locate two bulk and
            // one interrupt endpoint and FAIL if we cannot, and continue.
            for (k = 0; k < no_ep; k++) {
                openusb_parse_endpoint_desc(libmtp_openusb_handle, *dev, NULL, 0, i, j, 0, k, &ep);
                if (ep.bmAttributes == USB_ENDPOINT_TYPE_BULK) {
                    if ((ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
                            USB_ENDPOINT_DIR_MASK) {
                        *inep = ep.bEndpointAddress;
                        *inep_maxpacket = ep.wMaxPacketSize;
                        found_inep = 1;
                    }
                    if ((ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == 0) {
                        *outep = ep.bEndpointAddress;
                        *outep_maxpacket = ep.wMaxPacketSize;
                        found_outep = 1;
                    }
                } else if (ep.bmAttributes == USB_ENDPOINT_TYPE_INTERRUPT) {
                    if ((ep.bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
                            USB_ENDPOINT_DIR_MASK) {
                        *intep = ep.bEndpointAddress;
                        found_intep = 1;
                    }
                }
            }
            if (found_inep && found_outep && found_intep) {
                // We assigned the endpoints so return here.
                return 0;
            }
            // Else loop to next interface/config
        }
    }
    return -1;
}

/**
 * This function assigns params and usbinfo given a raw device
 * as input.
 * @param device the device to be assigned.
 * @param usbinfo a pointer to the new usbinfo.
 * @return an error code.
 */
LIBMTP_error_number_t configure_usb_device(LIBMTP_raw_device_t *device,
        PTPParams *params,
        void **usbinfo) {
    PTP_USB *ptp_usb;
    openusb_devid_t *ldevice;
    uint16_t ret = 0;
    int err, found = 0, i;
    unsigned int nrofdevs;
    openusb_devid_t *devs = NULL;
    struct usb_device_desc desc;

    /* See if we can find this raw device again... */
    init_usb();

    openusb_get_devids_by_bus(libmtp_openusb_handle, 0, &devs, &nrofdevs);
    for (i = 0; i < nrofdevs; i++) {
        /*
                if (libusb_get_bus_number(devs[i]) != device->bus_location)
                    continue;
                if (libusb_get_device_address(devs[i]) != device->devnum)
                    continue;
         */

        ret = openusb_parse_device_desc(libmtp_openusb_handle, devs[i], NULL, 0, &desc);
        if (ret != OPENUSB_SUCCESS) continue;

        if (desc.idVendor == device->device_entry.vendor_id &&
                desc.idProduct == device->device_entry.product_id) {
            ldevice = &devs[i];
            found = 1;
            break;
        }
    }
    /* Device has gone since detecting raw devices! */
    if (!found) {
        openusb_free_devid_list(devs);
        return LIBMTP_ERROR_NO_DEVICE_ATTACHED;
    }

    /* Allocate structs */
    ptp_usb = (PTP_USB *) malloc(sizeof (PTP_USB));
    if (ptp_usb == NULL) {
        openusb_free_devid_list(devs);
        return LIBMTP_ERROR_MEMORY_ALLOCATION;
    }
    /* Start with a blank slate (includes setting device_flags to 0) */
    memset(ptp_usb, 0, sizeof (PTP_USB));

    /* Copy the raw device */
    memcpy(&ptp_usb->rawdevice, device, sizeof (LIBMTP_raw_device_t));

    /*
     * Some devices must have their "OS Descriptor" massaged in order
     * to work.
     */
    if (FLAG_ALWAYS_PROBE_DESCRIPTOR(ptp_usb)) {
        // Massage the device descriptor
        (void) probe_device_descriptor(ldevice, NULL);
    }


    /* Assign interface and endpoints to usbinfo... */
    err = find_interface_and_endpoints(ldevice,
            &ptp_usb->conf,
            &ptp_usb->interface,
            &ptp_usb->altsetting,
            &ptp_usb->inep,
            &ptp_usb->inep_maxpacket,
            &ptp_usb->outep,
            &ptp_usb->outep_maxpacket,
            &ptp_usb->intep);

    if (err) {
        openusb_free_devid_list(devs);
        LIBMTP_ERROR("LIBMTP PANIC: Unable to find interface & endpoints of device\n");
        return LIBMTP_ERROR_CONNECTING;
    }

    /* Copy USB version number */
    ptp_usb->bcdusb = desc.bcdUSB;

    /* Attempt to initialize this device */
    if (init_ptp_usb(params, ptp_usb, ldevice) < 0) {
        LIBMTP_ERROR("LIBMTP PANIC: Unable to initialize device\n");
        return LIBMTP_ERROR_CONNECTING;
    }

    /*
     * This works in situations where previous bad applications
     * have not used LIBMTP_Release_Device on exit
     */
    if ((ret = ptp_opensession(params, 1)) == PTP_ERROR_IO) {
        LIBMTP_ERROR("PTP_ERROR_IO: failed to open session, trying again after resetting USB interface\n");
        LIBMTP_ERROR("LIBMTP libusb: Attempt to reset device\n");
        openusb_reset(*ptp_usb->handle);
        close_usb(ptp_usb);

        if (init_ptp_usb(params, ptp_usb, ldevice) < 0) {
            LIBMTP_ERROR("LIBMTP PANIC: Could not init USB on second attempt\n");
            return LIBMTP_ERROR_CONNECTING;
        }

        /* Device has been reset, try again */
        if ((ret = ptp_opensession(params, 1)) == PTP_ERROR_IO) {
            LIBMTP_ERROR("LIBMTP PANIC: failed to open session on second attempt\n");
            return LIBMTP_ERROR_CONNECTING;
        }
    }

    /* Was the transaction id invalid? Try again */
    if (ret == PTP_RC_InvalidTransactionID) {
        LIBMTP_ERROR("LIBMTP WARNING: Transaction ID was invalid, increment and try again\n");
        params->transaction_id += 10;
        ret = ptp_opensession(params, 1);
    }

    if (ret != PTP_RC_SessionAlreadyOpened && ret != PTP_RC_OK) {
        LIBMTP_ERROR("LIBMTP PANIC: Could not open session! "
                "(Return code %d)\n  Try to reset the device.\n",
                ret);
        openusb_release_interface(*ptp_usb->handle, ptp_usb->interface);
        return LIBMTP_ERROR_CONNECTING;
    }

    /* OK configured properly */
    *usbinfo = (void *) ptp_usb;
    return LIBMTP_ERROR_NONE;
}

void close_device(PTP_USB *ptp_usb, PTPParams *params) {
    if (ptp_closesession(params) != PTP_RC_OK)
        LIBMTP_ERROR("ERROR: Could not close session!\n");
    close_usb(ptp_usb);
}

void set_usb_device_timeout(PTP_USB *ptp_usb, int timeout) {
    ptp_usb->timeout = timeout;
}

void get_usb_device_timeout(PTP_USB *ptp_usb, int *timeout) {
    *timeout = ptp_usb->timeout;
}

int guess_usb_speed(PTP_USB *ptp_usb) {
    int bytes_per_second;

    /*
     * We don't know the actual speeds so these are rough guesses
     * from the info you can find here:
     * http://en.wikipedia.org/wiki/USB#Transfer_rates
     * http://www.barefeats.com/usb2.html
     */
    switch (ptp_usb->bcdusb & 0xFF00) {
        case 0x0100:
            /* 1.x USB versions let's say 1MiB/s */
            bytes_per_second = 1 * 1024 * 1024;
            break;
        case 0x0200:
        case 0x0300:
            /* USB 2.0 nominal speed 18MiB/s */
            /* USB 3.0 won't be worse? */
            bytes_per_second = 18 * 1024 * 1024;
            break;
        default:
            /* Half-guess something? */
            bytes_per_second = 1 * 1024 * 1024;
            break;
    }
    return bytes_per_second;
}

static int usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status) {
    /*
      return libusb_control_transfer(ptp_usb->handle,
                              LIBUSB_ENDPOINT_IN|LIBUSB_RECIPIENT_ENDPOINT,
                              LIBUSB_REQUEST_GET_STATUS,
                              USB_FEATURE_HALT,
                              ep,
                              (unsigned char *) status,
                              2,
                              ptp_usb->timeout);
     */
    struct openusb_ctrl_request ctrl;
    ctrl.flags = 0;
    ctrl.length = 2;
    ctrl.payload = (unsigned char *)status;
    ctrl.timeout = ptp_usb->timeout;
    ctrl.next = NULL;
    ctrl.setup.bRequest = USB_REQ_GET_STATUS;
    ctrl.setup.bmRequestType = USB_ENDPOINT_IN | USB_RECIP_ENDPOINT;
    ctrl.setup.wIndex = ep;
    ctrl.setup.wValue = USB_FEATURE_HALT;
    openusb_ctrl_xfer(*ptp_usb->handle, ptp_usb->interface, ep, &ctrl);
    return ctrl.result.status;

}
