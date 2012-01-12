/**
 * \file files.c
 * Example program that lists all files on a device.
 *
 * Copyright (C) 2005-2012 Linus Walleij <triad@df.lth.se>
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
#include <stdlib.h>

static void dump_fileinfo(LIBMTP_file_t *file)
{
  printf("File ID: %u\n", file->item_id);
  if (file->filename != NULL)
    printf("   Filename: %s\n", file->filename);

  // This is sort of special...
  if (file->filesize == (uint32_t) -1) {
    printf("   None. (abstract file, size = -1)\n");
  } else {
#ifdef __WIN32__
    printf("   File size %llu (0x%016I64X) bytes\n", file->filesize, file->filesize);
#else
    printf("   File size %llu (0x%016llX) bytes\n",
	   (long long unsigned int) file->filesize,
	   (long long unsigned int) file->filesize);
#endif
  }
  printf("   Parent ID: %u\n", file->parent_id);
  printf("   Storage ID: 0x%08X\n", file->storage_id);
  printf("   Filetype: %s\n", LIBMTP_Get_Filetype_Description(file->filetype));
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device_list, *device;
  LIBMTP_file_t *files;

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  LIBMTP_Init();

  switch(LIBMTP_Get_Connected_Devices(&device_list))
  {
  case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
    fprintf(stdout, "mtp-files: No Devices have been found\n");
    return 0;
  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "mtp-files: There has been an error connecting. Exit\n");
    return 1;
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "mtp-files: Memory Allocation Error. Exit\n");
    return 1;

  /* Unknown general errors - This should never execute */
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "mtp-files: Unknown error, please report "
                    "this to the libmtp developers\n");
  return 1;

  /* Successfully connected at least one device, so continue */
  case LIBMTP_ERROR_NONE:
    fprintf(stdout, "mtp-files: Successfully connected\n");
    fflush(stdout);
  }

  /* iterate through connected MTP devices */
  for(device = device_list; device != NULL; device = device->next)
  {
    char *friendlyname;

    /* Echo the friendly name so we know which device we are working with */
    friendlyname = LIBMTP_Get_Friendlyname(device);
    if (friendlyname == NULL) {
      printf("Listing File Information on Device with name: (NULL)\n");
    } else {
      printf("Listing File Information on Device with name: %s\n", friendlyname);
      free(friendlyname);
    }

	  /* Get track listing. */
	  files = LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);
	  if (files == NULL) {
	    printf("No files.\n");
	    LIBMTP_Dump_Errorstack(device);
	    LIBMTP_Clear_Errorstack(device);
	  } else {
	    LIBMTP_file_t *file, *tmp;
	    file = files;
	    while (file != NULL) {
	      dump_fileinfo(file);
	      tmp = file;
	      file = file->next;
	      LIBMTP_destroy_file_t(tmp);
      }
	  }
  }

  LIBMTP_Release_Device_List(device_list);
  printf("OK.\n");
  exit (0);
}
