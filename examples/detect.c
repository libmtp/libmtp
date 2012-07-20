/**
 * \file detect.c
 * Example program to detect a device and list capabilities.
 *
 * Copyright (C) 2005-2008 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2007 Ted Bullock <tbullock@canada.com>
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
 */
#include "common.h"
#include "util.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define XML_BUFSIZE 0x10000

static void dump_xml_fragment(uint8_t *buf, uint32_t len)
{
  static int endianness = 0; // 0 = LE, 1 = BE
  uint32_t bp = 0;

  while (bp < len) {
    if (buf[bp+0] == 0xFF && buf[bp+1] == 0xFE) {
      endianness = 0;
    } else if (buf[bp+0] == 0xFE && buf[bp+1] == 0xff) {
      endianness = 1;
    } else {
      uint16_t tmp;

      if (endianness == 0) {
	tmp = buf[bp+1] << 8 | buf[bp+0];
      } else {
	tmp = buf[bp+0] << 8 | buf[bp+1];
      }
      // Fix this some day, we only print ISO 8859-1 correctly here,
      // should atleast support UTF-8.
      printf("%c", (uint8_t) tmp);
    }
    bp += 2;
  }
  printf("\n");
}

int main (int argc, char **argv)
{
  LIBMTP_raw_device_t * rawdevices;
  int numrawdevices;
  LIBMTP_error_number_t err;
  int i;

  int opt;
  extern int optind;
  extern char *optarg;

  while ((opt = getopt(argc, argv, "d")) != -1 ) {
    switch (opt) {
    case 'd':
      LIBMTP_Set_Debug(LIBMTP_DEBUG_PTP | LIBMTP_DEBUG_DATA);
      break;
    }
  }

  argc -= optind;
  argv += optind;

  LIBMTP_Init();

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  fprintf(stdout, "Listing raw device(s)\n");
  err = LIBMTP_Detect_Raw_Devices(&rawdevices, &numrawdevices);
  switch(err) {
  case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
    fprintf(stdout, "   No raw devices found.\n");
    return 0;
  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "Detect: There has been an error connecting. Exiting\n");
    return 1;
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "Detect: Encountered a Memory Allocation Error. Exiting\n");
    return 1;
  case LIBMTP_ERROR_NONE:
    {
      int i;

      fprintf(stdout, "   Found %d device(s):\n", numrawdevices);
      for (i = 0; i < numrawdevices; i++) {
	if (rawdevices[i].device_entry.vendor != NULL ||
	    rawdevices[i].device_entry.product != NULL) {
	  fprintf(stdout, "   %s: %s (%04x:%04x) @ bus %d, dev %d\n",
		  rawdevices[i].device_entry.vendor,
		  rawdevices[i].device_entry.product,
		  rawdevices[i].device_entry.vendor_id,
		  rawdevices[i].device_entry.product_id,
		  rawdevices[i].bus_location,
		  rawdevices[i].devnum);
	} else {
	  fprintf(stdout, "   %04x:%04x @ bus %d, dev %d\n",
		  rawdevices[i].device_entry.vendor_id,
		  rawdevices[i].device_entry.product_id,
		  rawdevices[i].bus_location,
		  rawdevices[i].devnum);
	}
      }
    }
    break;
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "Unknown connection error.\n");
    return 1;
  }

  /* Iterate over connected MTP devices */
  fprintf(stdout, "Attempting to connect device(s)\n");
  for (i = 0; i < numrawdevices; i++) {
    LIBMTP_mtpdevice_t *device;
    LIBMTP_file_t *files;
    char *friendlyname;
    char *syncpartner;
    char *sectime;
    char *devcert;
    uint16_t *filetypes;
    uint16_t filetypes_len;
    uint8_t maxbattlevel;
    uint8_t currbattlevel;
    int ret;

    device = LIBMTP_Open_Raw_Device(&rawdevices[i]);
    if (device == NULL) {
      fprintf(stderr, "Unable to open raw device %d\n", i);
      continue;
    }

    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
    LIBMTP_Dump_Device_Info(device);

    printf("MTP-specific device properties:\n");
    // The friendly name
    friendlyname = LIBMTP_Get_Friendlyname(device);
    if (friendlyname == NULL) {
      fprintf(stdout, "   Friendly name: (NULL)\n");
    } else {
      fprintf(stdout, "   Friendly name: %s\n", friendlyname);
      free(friendlyname);
    }
    syncpartner = LIBMTP_Get_Syncpartner(device);
    if (syncpartner == NULL) {
      fprintf(stdout, "   Synchronization partner: (NULL)\n");
    } else {
      fprintf(stdout, "   Synchronization partner: %s\n", syncpartner);
      free(syncpartner);
    }

    // Some battery info
    ret = LIBMTP_Get_Batterylevel(device, &maxbattlevel, &currbattlevel);
    if (ret == 0) {
      fprintf(stdout, "   Battery level %d of %d (%d%%)\n",currbattlevel, maxbattlevel,
	      (int) ((float) currbattlevel/ (float) maxbattlevel * 100.0));
    } else {
      // Silently ignore. Some devices does not support getting the
      // battery level.
      LIBMTP_Clear_Errorstack(device);
    }

    ret = LIBMTP_Get_Supported_Filetypes(device, &filetypes, &filetypes_len);
    if (ret == 0) {
      uint16_t i;

      printf("libmtp supported (playable) filetypes:\n");
      for (i = 0; i < filetypes_len; i++) {
	fprintf(stdout, "   %s\n", LIBMTP_Get_Filetype_Description(filetypes[i]));
      }
    } else {
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
    }

    // Secure time XML fragment
    ret = LIBMTP_Get_Secure_Time(device, &sectime);
    if (ret == 0 && sectime != NULL) {
      fprintf(stdout, "\nSecure Time:\n%s\n", sectime);
      free(sectime);
    } else {
      // Silently ignore - there may be devices not supporting secure time.
      LIBMTP_Clear_Errorstack(device);
    }

    // Device certificate XML fragment
#if 0
    /*
     * This code is currently disabled: all devices say that
     * they support getting a device certificate but a lot of
     * them obviously doesn't, instead they crash when you try
     * to obtain it.
     */
    ret = LIBMTP_Get_Device_Certificate(device, &devcert);
    if (ret == 0 && devcert != NULL) {
      fprintf(stdout, "\nDevice Certificate:\n%s\n", devcert);
      free(devcert);
    } else {
      fprintf(stdout, "Unable to acquire device certificate, perhaps this device "
	      "does not support this\n");
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
    }
#endif

    // Try to get Media player device info XML file...
    files = LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);
    if (files != NULL) {
      LIBMTP_file_t *file, *tmp;
      file = files;
      while (file != NULL) {
	if (!strcmp(file->filename, "WMPInfo.xml") ||
	    !strcmp(file->filename, "WMPinfo.xml") ||
	    !strcmp(file->filename, "default-capabilities.xml")) {
	    if (file->item_id != 0) {
	      /* Dump this file */
	      FILE *xmltmp = tmpfile();
	      int tmpfiledescriptor = fileno(xmltmp);

	      if (tmpfiledescriptor != -1) {
		int ret = LIBMTP_Get_Track_To_File_Descriptor(device,
							      file->item_id,
							      tmpfiledescriptor,
							      NULL,
							      NULL);
		if (ret == 0) {
		  uint8_t *buf = NULL;
		  uint32_t readbytes;

		  buf = malloc(XML_BUFSIZE);
		  if (buf == NULL) {
		    printf("Could not allocate %08x bytes...\n", XML_BUFSIZE);
		    LIBMTP_Dump_Errorstack(device);
		    LIBMTP_Clear_Errorstack(device);
		    free(rawdevices);
		    return 1;
		  }

		  lseek(tmpfiledescriptor, 0, SEEK_SET);
		  readbytes = read(tmpfiledescriptor, (void*) buf, XML_BUFSIZE);

		  if (readbytes >= 2 && readbytes < XML_BUFSIZE) {
		    fprintf(stdout, "\n%s file contents:\n", file->filename);
		    dump_xml_fragment(buf, readbytes);
		  } else {
		    perror("Unable to read file");
		    LIBMTP_Dump_Errorstack(device);
		    LIBMTP_Clear_Errorstack(device);
		  }
		  free(buf);
		} else {
		  LIBMTP_Dump_Errorstack(device);
		  LIBMTP_Clear_Errorstack(device);
		}
		fclose(xmltmp);
	      }
	    }
	}
	tmp = file;
	file = file->next;
	LIBMTP_destroy_file_t(tmp);
      }
    }
    LIBMTP_Release_Device(device);
  } /* End For Loop */

  free(rawdevices);

  printf("OK.\n");

  return 0;
}
