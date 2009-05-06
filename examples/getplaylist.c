/** 
 * \file getplaylist.c
 * Example program that lists the abstract playlists on the device.
 *
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
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
#include <limits.h>

static uint32_t dump_playlist(LIBMTP_mtpdevice_t *device, LIBMTP_playlist_t *pl)
{
  uint32_t i;

  printf("Number of items: %u\n", pl->no_tracks);
  if(pl->no_tracks > 0) {
    for(i=0;i<pl->no_tracks;i++) {
      LIBMTP_track_t *track;
      
      track = LIBMTP_Get_Trackmetadata(device, pl->tracks[i]);
      if (track != NULL) {
	printf("   %u: %s - %s\n", pl->tracks[i], track->artist, track->title);
	LIBMTP_destroy_track_t(track);
      } else {
	printf("   %u: INVALID TRACK REFERENCE!\n", pl->tracks[i]);
	LIBMTP_Dump_Errorstack(device);
	LIBMTP_Clear_Errorstack(device);
      }
    }
  }
  return 0;
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_playlist_t *playlist;
  uint32_t id;
  char *endptr;

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  // We need file ID
  if ( argc != 2 ) {
    fprintf(stderr, "Just a playlist ID is required\n");
    return 1;
  }

  // Sanity check playlist ID
  id = strtoul(argv[1], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[1]);
    return 1;
  } else if ( ! id ) {
    fprintf(stderr, "bad playlist id %u\n", id);
    return 1;
 }

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices. Connect/replug device and try again.\n");
    exit (0);
  }

  playlist = LIBMTP_Get_Playlist(device,id);  

  if (playlist != NULL) {
    dump_playlist(device,playlist);
  }

  LIBMTP_destroy_playlist_t(playlist);
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

