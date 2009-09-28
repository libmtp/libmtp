/** 
 * \file newplaylist.c
 * Example program to create a playlist on a device.
 *
 * Copyright (C) 2006 Robert Reardon <rreardon@monkshatch.vispa.com>
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
#include "string.h"
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void usage(void) {
  printf("Usage: newplaylist -i <fileid/trackid> -n <playlistname> -s <storage_id> -p <parent_id>\n");
  exit(0);
}

int main (int argc, char **argv) {
  int opt;
  extern int optind;
  extern char *optarg;
  LIBMTP_mtpdevice_t *device = NULL;
  int idcount = 0;
  uint32_t *ids = NULL;
  uint32_t *tmp = NULL;
  char *playlistname = NULL;
  char *rest;
  uint32_t storageid = 0;
  uint32_t parentid = 0;
 
  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  while ( (opt = getopt(argc, argv, "hn:i:s:p:")) != -1 ) {
    switch (opt) {
    case 'h':
      usage();
    case 'i':
      idcount++;
      if ((tmp = realloc(ids, sizeof(uint32_t) * (idcount))) == NULL) {
        printf("realloc failed\n");
        return 1;
      }
      ids = tmp;
      ids[(idcount-1)] = strtoul(optarg, &rest, 0);
      break;
    case 'n':
      playlistname = strdup(optarg);
      break;
    case 's':
      storageid = (uint32_t) strtoul(optarg, NULL, 0);
	  break;
    case 'p':
      parentid = (uint32_t) strtoul(optarg, NULL, 0);
	  break;
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;

  if ( playlistname == NULL) {
    printf("You need to supply a playlist name.\n");
    usage();
  }

  if (idcount == 0) {
    printf("You need to supply one or more track IDs\n");
    usage();
  }

    
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  LIBMTP_playlist_t *playlist = LIBMTP_new_playlist_t();
  playlist->name = playlistname;
  playlist->no_tracks = idcount;
  playlist->tracks = ids;
  playlist->parent_id = parentid;
  playlist->storage_id = storageid;
  int ret = LIBMTP_Create_New_Playlist(device,playlist);
  if (ret != 0) {
    printf("Couldn't create playlist object\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  }
  else {
    printf("Created new playlist: %u\n", playlist->playlist_id);
  }

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

