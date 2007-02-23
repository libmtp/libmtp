/** 
 * \file detect.c
 * Example program to detect a device and list capabilities.
 *
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
  LIBMTP_mtpdevice_t *device, *iter;
  LIBMTP_file_t *files;
  uint32_t xmlfileid = 0;
  char *friendlyname;
  char *syncpartner;
  char *sectime;
  char *devcert;
  uint16_t *filetypes;
  uint16_t filetypes_len;
  uint8_t maxbattlevel;
  uint8_t currbattlevel;
  int ret;
  int probeonly = 0;

  LIBMTP_Init();

  if (argc > 1 && !strcmp(argv[1], "-p")) {
    probeonly = 1;
  }

  if (probeonly) {
//    uint16_t vid;
//    uint16_t pid;
//
//    ret = LIBMTP_Detect_Descriptor(&vid, &pid);
//    if (ret > 0) {
//      printf("DETECTED MTP DEVICE WITH VID:%04x, PID:%04X\n", vid, pid);
//      exit(0);
//    } else {
//      exit(1);
//    }
    fprintf(stdout, "LIBMTP Panic: Probing has been disabled until it has "
  								"been refactored to\nuse multiple devices\n");
  }

  fprintf(stdout, "Attempting to connect device(s)\n");

  switch(LIBMTP_Get_Connected_Devices(&device))
  {
  case LIBMTP_ERROR_N0_DEVICE_ATTACHED:
    fprintf(stdout, "Detect: No Devices have been found\n");
    return 0;
  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "Detect: There has been an error connecting. Exiting\n");
    return 1;
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "Detect: Encountered a Memory Allocation Error. Exiting\n");
    return 1;
 
  /* Unknown general errors - This should never execute */
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "Detect: There has been an unknown error, please report "
                    "this to the libmtp developers\n");
  return 1;

  /* Successfully connected at least one device, so continue */
  case LIBMTP_ERROR_NONE:
    fprintf(stdout, "Detect: Successfully connected\n");
    fflush(stdout);
  }

  /* iterate through connected MTP devices */
  for(iter = device; iter != NULL; iter = iter->next)
  {
  
  LIBMTP_Dump_Errorstack(iter);
  LIBMTP_Clear_Errorstack(iter);
  LIBMTP_Dump_Device_Info(iter);
  
  printf("MTP-specific device properties:\n");
  // The friendly name
  friendlyname = LIBMTP_Get_Friendlyname(iter);
  if (friendlyname == NULL) {
    fprintf(stdout, "   Friendly name: (NULL)\n");
  } else {
    fprintf(stdout, "   Friendly name: %s\n", friendlyname);
    free(friendlyname);
  }
  syncpartner = LIBMTP_Get_Syncpartner(iter);
  if (syncpartner == NULL) {
    fprintf(stdout, "   Synchronization partner: (NULL)\n");
  } else {
   fprintf(stdout, "   Synchronization partner: %s\n", syncpartner);
    free(syncpartner);
  }

  // Some battery info
  ret = LIBMTP_Get_Batterylevel(iter, &maxbattlevel, &currbattlevel);
  if (ret == 0) {
    fprintf(stdout, "   Battery level %d of %d (%d%%)\n",currbattlevel, maxbattlevel, 
	   (int) ((float) currbattlevel/ (float) maxbattlevel * 100.0));
  } else {
    // Silently ignore. Some devices does not support getting the 
    // battery level.
    LIBMTP_Clear_Errorstack(iter);
  }

  ret = LIBMTP_Get_Supported_Filetypes(iter, &filetypes, &filetypes_len);
  if (ret == 0) {
    uint16_t i;
    
    printf("libmtp supported (playable) filetypes:\n");
    for (i = 0; i < filetypes_len; i++) {
      fprintf(stdout, "   %s\n", LIBMTP_Get_Filetype_Description(filetypes[i]));
    }
  } else {
    LIBMTP_Dump_Errorstack(iter);
    LIBMTP_Clear_Errorstack(iter);
  }

  // Secure time XML fragment
  ret = LIBMTP_Get_Secure_Time(iter, &sectime);
  if (ret == 0 && sectime != NULL) {
    fprintf(stdout, "\nSecure Time:\n%s\n", sectime);
    free(sectime);
  } else {
    // Silently ignore - there may be devices not supporting secure time.
    LIBMTP_Clear_Errorstack(iter);
  }

  // Device certificate XML fragment
  fprintf(stdout, "Trying to acquire device certificate\n");
  ret = LIBMTP_Get_Device_Certificate(iter, &devcert);
  if (ret == 0 && devcert != NULL) {
    fprintf(stdout, "\nDevice Certificate:\n%s\n", devcert);
    free(devcert);
  } else {
    fprintf(stdout, "Unable to acquire device certificate, perhaps this device "
                    "does not support this\n");
    LIBMTP_Dump_Errorstack(iter);
    LIBMTP_Clear_Errorstack(iter);
  }

  // Try to get Media player device info XML file...
  files = LIBMTP_Get_Filelisting_With_Callback(iter, NULL, NULL);
  if (files != NULL) {
    LIBMTP_file_t *file, *tmp;
    file = files;
    while (file != NULL) {
      if (!strcmp(file->filename, "WMPInfo.xml"))
      {
        fprintf(stdout, "Found WMPInfo.xml\n");
        xmlfileid = file->item_id;
      }
      tmp = file;
      file = file->next;
      LIBMTP_destroy_file_t(tmp);
    }
  }
  if (xmlfileid != 0)
  {
    FILE *xmltmp = tmpfile();
    int tmpfile = fileno(xmltmp);
    
    if (tmpfile != -1)
    {
      int ret = LIBMTP_Get_Track_To_File_Descriptor(iter, xmlfileid, tmpfile, NULL, NULL);
      if (ret == 0)
      {
        uint8_t *buf = NULL;
        uint32_t readbytes;
        
        fprintf(stdout, "Grabbed WMPInfo.xml File Descriptor\n");

        buf = malloc(XML_BUFSIZE);
        if (buf == NULL)
        {
          printf("Could not allocate %08x bytes...\n", XML_BUFSIZE);
          exit(1);
        }
        
        lseek(tmpfile, 0, SEEK_SET);
        readbytes = read(tmpfile, (void*) buf, XML_BUFSIZE);
	
        if (readbytes >= 2 && readbytes < XML_BUFSIZE)
        {
          fprintf(stdout, "\nDevice description WMPInfo.xml file:\n");
          dump_xml_fragment(buf, readbytes);
        }
        else
        {
          fprintf(stdout, "Unable to read WMPInfo.xml for this device\n"
                          "Read %u bytes which should have been between\n"
                          "2 and %d bytes long.\n",
                          readbytes, XML_BUFSIZE);
        }
      }
      else
      {
        LIBMTP_Dump_Errorstack(iter);
        LIBMTP_Clear_Errorstack(iter);
      }
      fclose(xmltmp);
    }
  }

  } /* End For Loop */

  LIBMTP_Release_Device_List(device);
  printf("OK.\n");
  
  return 0; 
}
