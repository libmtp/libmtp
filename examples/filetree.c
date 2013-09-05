/**
 * \file filetree.c
 * List all files and folders of all storages recursively
 *
 * Copyright (C) 2011 Linus Walleij <triad@df.lth.se>
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

/* Clever prototype to be able to recurse */
void recursive_file_tree(LIBMTP_mtpdevice_t *,
			 LIBMTP_devicestorage_t *,
			 uint32_t,
			 int);

void recursive_file_tree(LIBMTP_mtpdevice_t *device,
			 LIBMTP_devicestorage_t *storage,
			 uint32_t leaf,
			 int depth)
{
  LIBMTP_file_t *files;
  LIBMTP_file_t *file;

  files = LIBMTP_Get_Files_And_Folders(device,
				      storage->id,
				      leaf);
  if (files == NULL) {
    return;
  }

  /* Iterate over the filelisting */
  file = files;
  while (file != NULL) {
    int i;
    LIBMTP_file_t *oldfile;

    /* Indent */
    for (i = 0; i < depth; i++) {
      printf(" ");
    }
    printf("%u %s\n", file->item_id, file->filename);
    if (file->filetype == LIBMTP_FILETYPE_FOLDER) {
      recursive_file_tree(device, storage, file->item_id, depth+2);
    }

    oldfile = file;
    file = file->next;
    LIBMTP_destroy_file_t(oldfile);
  }
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
    LIBMTP_devicestorage_t *storage;
    char *friendlyname;
    int ret;

    device = LIBMTP_Open_Raw_Device_Uncached(&rawdevices[i]);
    if (device == NULL) {
      fprintf(stderr, "Unable to open raw device %d\n", i);
      continue;
    }

    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);

    friendlyname = LIBMTP_Get_Friendlyname(device);
    if (friendlyname == NULL) {
      printf("Device: (NULL)\n");
    } else {
      printf("Device: %s\n", friendlyname);
      free(friendlyname);
    }

    /* Get all storages for this device */
    ret = LIBMTP_Get_Storage(device, LIBMTP_STORAGE_SORTBY_NOTSORTED);
    if (ret != 0) {
      perror("LIBMTP_Get_Storage()");
      goto bailout;
    }

    /* Loop over storages */
    for (storage = device->storage; storage != 0; storage = storage->next) {
      fprintf(stdout, "Storage: %s\n", storage->StorageDescription);
      recursive_file_tree(device, storage, 0, 0);
    }

  bailout:
    LIBMTP_Release_Device(device);
  } /* End For Loop */

  free(rawdevices);

  printf("OK.\n");

  return 0;
}
