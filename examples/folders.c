/** 
 * \file folders.c
 * Example program that lists all folders on a device.
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
  LIBMTP_mtpdevice_t *device, *iter;
  LIBMTP_folder_t *folders;

  LIBMTP_Init();
  fprintf(stdout, "Attempting to connect device(s)\n");

  switch(LIBMTP_Get_Connected_Devices(&device))
  {
  case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
    fprintf(stdout, "mtp-folders: No Devices have been found\n");
    return 0;
  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "mtp-folders: There has been an error connecting. Exit\n");
    return 1;
  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "mtp-folders: Memory Allocation Error. Exit\n");
    return 1;
 
  /* Unknown general errors - This should never execute */
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "mtp-folders: Unknown error, please report "
                    "this to the libmtp developers\n");
  return 1;

  /* Successfully connected at least one device, so continue */
  case LIBMTP_ERROR_NONE:
    fprintf(stdout, "mtp-folders: Successfully connected\n");
    fflush(stdout);
  }
  
  /* iterate through connected MTP devices */
  for(iter = device; iter != NULL; iter = iter->next)
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
    
    LIBMTP_Dump_Errorstack(iter);
    LIBMTP_Clear_Errorstack(iter);    /* Get folder listing */

    folders = LIBMTP_Get_Folder_List(iter);
    
    if (folders == NULL) {
      fprintf(stdout, "No folders found\n");
      LIBMTP_Dump_Errorstack(iter);
      LIBMTP_Clear_Errorstack(iter);
    } else {
      dump_folder_list(folders,0);
    }

    LIBMTP_destroy_folder_t(folders);
  }

  
  LIBMTP_Release_Device_List(device);
  printf("OK.\n");

  return 0;
}
