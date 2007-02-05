/** 
 * \file folders.c
 * Example program that lists all folders on a device.
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

static void dump_folder_list(LIBMTP_folder_t *folderlist, int level)
{
  int i;
  if(folderlist==NULL) {
    return;
  }

  printf("%u\t", folderlist->folder_id);
  for(i=0;i<level;i++) printf("  ");

  printf("%s\n", folderlist->name);

  dump_folder_list(folderlist->child, level+1);
  dump_folder_list(folderlist->sibling, level);
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_folder_t *folders;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get folder listing.
  folders = LIBMTP_Get_Folder_List(device);
  
  if (folders == NULL) {
    printf("No folders found\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  } else {
    dump_folder_list(folders,0);
  }

  LIBMTP_destroy_folder_t(folders);

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}
