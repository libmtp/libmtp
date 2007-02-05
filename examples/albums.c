/** 
 * \file albums.c
 * Example program that lists the albums on the device.
 *
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
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

static void dump_albuminfo(LIBMTP_album_t *album)
{
  printf("Album ID:%d\n",album->album_id);
  printf("    Name:%s\n",album->name);
  printf("  Tracks:%d\n\n",album->no_tracks);
}

int main () {
  LIBMTP_mtpdevice_t *device = NULL;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  LIBMTP_album_t *albums,*album,*tmp;
  albums = LIBMTP_Get_Album_List(device);
  album = albums;
  while (album != NULL) {
    dump_albuminfo(album);
    tmp = album;
    album = album->next;
    LIBMTP_destroy_album_t(tmp);
  }
     
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

