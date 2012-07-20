/**
 * \file albums.c
 * Example program that lists the albums on the device.
 *
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
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

static void dump_albuminfo(LIBMTP_album_t *album)
{
  printf("Album ID: %d\n",album->album_id);
  printf("    Parent ID:   %d\n",album->parent_id);
  printf("    Name:   %s\n",album->name);
  printf("    Artist: %s\n", album->artist);
  printf("    Composer:  %s\n", album->composer);
  printf("    Genre:  %s\n", album->genre);
  printf("    Tracks: %d\n\n",album->no_tracks);
}

int main (int argc, char *argv[]) {
  LIBMTP_mtpdevice_t *device_list, *device;

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

  switch(LIBMTP_Get_Connected_Devices(&device_list))
  {
  case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
    fprintf(stdout, "mtp-albums: No Devices have been found\n");
    return 0;
  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "mtp-albums: There has been an error connecting. Exit\n");
    return 1;
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "mtp-albums: Memory Allocation Error. Exit\n");
    return 1;

  /* Unknown general errors - This should never execute */
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "mtp-albums: Unknown error, please report "
                    "this to the libmtp developers\n");
  return 1;

  /* Successfully connected at least one device, so continue */
  case LIBMTP_ERROR_NONE:
    fprintf(stdout, "mtp-albums: Successfully connected\n");
    fflush(stdout);
  }

  /* iterate through connected MTP devices */
  for(device = device_list; device != NULL; device = device->next)
  {
    char *friendlyname;
    LIBMTP_album_t *album_list, *album, *tmp;

    /* Echo the friendly name so we know which device we are working with */
    friendlyname = LIBMTP_Get_Friendlyname(device);
    if (friendlyname == NULL) {
      printf("Retrieving Albums on Device with name: (NULL)\n");
    } else {
      printf("Retrieving Albums on Device with name: %s\n", friendlyname);
      free(friendlyname);
    }

    album_list = LIBMTP_Get_Album_List(device);
    album = album_list;
    while(album != NULL)
    {
      dump_albuminfo(album);
      tmp = album;
      album = album->next;
      LIBMTP_destroy_album_t(tmp);
    }
  }

  LIBMTP_Release_Device_List(device_list);
  printf("OK.\n");
  return 0;
}

