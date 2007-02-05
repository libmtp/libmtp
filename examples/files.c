/** 
 * \file files.c
 * Example program that lists all files on a device.
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

static void dump_fileinfo(LIBMTP_file_t *file)
{
  printf("File ID: %u\n", file->item_id);
  if (file->filename != NULL)
    printf("   Filename: %s\n", file->filename);

  // This is sort of special...
  if (file->filesize == (uint32_t) -1) {
    printf("   None. (abstract file, size = -1)\n");
  } else {
    printf("   File size %llu (0x%08llX) bytes\n", file->filesize, file->filesize);
  }
  printf("   Parent ID: %u\n", file->parent_id);
  printf("   Filetype: %s\n", LIBMTP_Get_Filetype_Description(file->filetype));
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_file_t *files;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get track listing.
  files = LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);
  if (files == NULL) {
    printf("No files.\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  } else {
    LIBMTP_file_t *file, *tmp;
    file = files;
    while (file != NULL) {
      dump_fileinfo(file);
      tmp = file;
      file = file->next;
      LIBMTP_destroy_file_t(tmp);
    }
  }
    
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

