/** 
 * \file emptyfolders.c
 * Example program that prunes empty folders.
 *
 * Copyright (C) 2006 Andy Kelk <andy@mopoke.co.uk>
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

static void prune_empty_folders(LIBMTP_mtpdevice_t *device, LIBMTP_file_t *files, LIBMTP_folder_t *folderlist, int do_delete)
{
  if(folderlist==NULL)
    return;

  if(folderlist->child == NULL) { // this *might* be empty
    // therefore, check every file for this parent_id
    int found = 0;
    LIBMTP_file_t *file;
    file = files;
    while (file != NULL) {
      if(file->parent_id == folderlist->folder_id) { // folder has a child
        found = 1;
        break;
      }
      file = file->next;
    }

    if(found == 0) { // no files claim this as a parent
      printf("empty folder %u (%s)\n",folderlist->folder_id,folderlist->name);
      if(do_delete) {
        if (LIBMTP_Delete_Object(device,folderlist->folder_id) != 0) {
          printf("Couldn't delete folder %u\n",folderlist->folder_id);
	  LIBMTP_Dump_Errorstack(device);
	  LIBMTP_Clear_Errorstack(device);
        }
      }
    }
  }

  prune_empty_folders(device,files,folderlist->child,do_delete); // recurse down
  prune_empty_folders(device,files,folderlist->sibling,do_delete); // recurse along
}

int main (int argc, char **argv)
{
  // check if we're doing a dummy run
  int do_delete = 0;
  int opt;

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  while ( (opt = getopt(argc, argv, "d")) != -1 ) {
    switch (opt) {
    case 'd':
      do_delete = 1;
      break;
    default:
      break;
    }
  }

  if(do_delete == 0) {
    printf("This is a dummy run. No folders will be deleted.\n");
    printf("To delete folders, use the '-d' option.\n");
  }

  LIBMTP_mtpdevice_t *device;
  LIBMTP_folder_t *folders;
  LIBMTP_file_t *files;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }

  // Get file listing.
  files = LIBMTP_Get_Filelisting_With_Callback(device,NULL,NULL);

  // Get folder listing.
  folders = LIBMTP_Get_Folder_List(device);

  if(folders == NULL) {
    printf("No folders found\n");
  } else {
    prune_empty_folders(device,files,folders,do_delete);
  }

  LIBMTP_destroy_folder_t(folders);
  LIBMTP_destroy_file_t(files);

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

