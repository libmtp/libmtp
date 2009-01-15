/** 
 * \file tracks.c
 * Example program to list the tracks on a device.
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
#include <stdlib.h>

static void dump_trackinfo(LIBMTP_track_t *track)
{
  printf("Track ID: %u\n", track->item_id);
  if (track->title != NULL)
    printf("   Title: %s\n", track->title);
  if (track->artist != NULL)
    printf("   Artist: %s\n", track->artist);
  if (track->genre != NULL)
    printf("   Genre: %s\n", track->genre);
  if (track->composer != NULL)
    printf("   Composer: %s\n", track->composer);
  if (track->album != NULL)
    printf("   Album: %s\n", track->album);
  if (track->date != NULL)
    printf("   Date: %s\n", track->date);
  if (track->filename != NULL)
    printf("   Origfilename: %s\n", track->filename);
  printf("   Track number: %d\n", track->tracknumber);
  printf("   Duration: %d milliseconds\n", track->duration);
#ifdef __WIN32__
  printf("   File size %I64u bytes\n", track->filesize);
#else
  printf("   File size %llu bytes\n", (long long unsigned int) track->filesize);
#endif
  printf("   Filetype: %s\n", LIBMTP_Get_Filetype_Description(track->filetype));
  if (track->samplerate != 0) {
    printf("   Sample rate: %u Hz\n", track->samplerate);
  }
  if (track->nochannels != 0) {
    printf("   Number of channels: %u\n", track->nochannels);
  }
  if (track->wavecodec != 0) {
    printf("   WAVE fourCC code: 0x%08X\n", track->wavecodec);
  }
  if (track->bitrate != 0) {
    printf("   Bitrate: %u bits/s\n", track->bitrate);
  }
  if (track->bitratetype != 0) {
    if (track->bitratetype == 1) {
      printf("   Bitrate type: Constant\n");
    } else if (track->bitratetype == 2) {
      printf("   Bitrate type: Variable (VBR)\n");
    } else if (track->bitratetype == 3) {
      printf("   Bitrate type: Free\n");
    } else {
      printf("   Bitrate type: Unknown/Erroneous value\n");
    }
  }
  if (track->rating != 0) {
    printf("   User rating: %u (out of 100)\n", track->rating);
  }
  if (track->usecount != 0) {
    printf("   Use count: %u times\n", track->usecount);
  }
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device_list, *iter;
  LIBMTP_track_t *tracks;

  LIBMTP_Init();
  fprintf(stdout, "Attempting to connect device(s)\n");

  switch(LIBMTP_Get_Connected_Devices(&device_list))
  {
  case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
    fprintf(stdout, "mtp-tracks: No Devices have been found\n");
    return 0;
  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "mtp-tracks: There has been an error connecting. Exit\n");
    return 1;
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "mtp-tracks: Memory Allocation Error. Exit\n");
    return 1;
 
  /* Unknown general errors - This should never execute */
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "mtp-tracks: Unknown error, please report "
                    "this to the libmtp developers\n");
  return 1;

  /* Successfully connected at least one device, so continue */
  case LIBMTP_ERROR_NONE:
    fprintf(stdout, "mtp-tracks: Successfully connected\n");
    fflush(stdout);
  }
  
  /* iterate through connected MTP devices */
  for(iter = device_list; iter != NULL; iter = iter->next)
  {
  	char *friendlyname;
    /* Echo the friendly name so we know which device we are working with */
    friendlyname = LIBMTP_Get_Friendlyname(iter);
    if (friendlyname == NULL) {
      printf("Friendly name: (NULL)\n");
    } else {
      printf("Friendly name: %s\n", friendlyname);
      free(friendlyname);
    }
  
	  // Get track listing.
	  tracks = LIBMTP_Get_Tracklisting_With_Callback(iter, NULL, NULL);
	  if (tracks == NULL) {
	    printf("No tracks.\n");
	  } else {
	    LIBMTP_track_t *track, *tmp;
	    track = tracks;
	    while (track != NULL) {
	      dump_trackinfo(track);
	      tmp = track;
	      track = track->next;
	      LIBMTP_destroy_track_t(tmp);
	    }
	  }
  }
    
  LIBMTP_Release_Device_List(device_list);
  printf("OK.\n");
  exit (0);
}

