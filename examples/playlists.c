/** 
 * \file playlists.c
 * Example program to list the playlists on a device.
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

static void dump_plinfo(LIBMTP_mtpdevice_t *device, LIBMTP_playlist_t *pl)
{
  uint32_t i;

  printf("Playlist ID: %d\n", pl->playlist_id);
  if (pl->name != NULL)
    printf("   Name: %s\n", pl->name);
  printf("   Parent ID: %d\n", pl->parent_id);
  printf("   Tracks:\n");

  for (i = 0; i < pl->no_tracks; i++) {
    LIBMTP_track_t *track;
    
    track = LIBMTP_Get_Trackmetadata(device, pl->tracks[i]);
    if (track != NULL) {
      printf("      %u: %s - %s\n", pl->tracks[i], track->artist, track->title);
      LIBMTP_destroy_track_t(track);
    } else {
      printf("      %u: INVALID TRACK REFERENCE!\n", pl->tracks[i]);
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
    }
  }
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_playlist_t *playlists;

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get playlist listing.
  playlists = LIBMTP_Get_Playlist_List(device);
  if (playlists == NULL) {
    printf("No playlists.\n");
  } else {
    LIBMTP_playlist_t *pl, *tmp;
    pl = playlists;
    while (pl != NULL) {
      dump_plinfo(device, pl);
      tmp = pl;
      pl = pl->next;
      LIBMTP_destroy_playlist_t(tmp);
    }
  }
    
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}
