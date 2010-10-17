/** 
 * \file newfolder.c
 * Example program to create a folder on the device.
 *
 * Copyright (C) 2006-2009 Linus Walleij <triad@df.lth.se>
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
#include <stdlib.h>
#include <libgen.h>

#include "common.h"
#include "pathutils.h"
#include "connect.h"

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

int newfolder_command (int argc, char **argv)
{
  uint32_t newid;

  if(argc != 4) {
    printf("Usage: newfolder name <parent> <storage>\n");
    printf("  parent = parent folder or 0 to create the new folder in the root dir\n");
    printf("  storage = storage id or 0 to create the new folder on the primary storage\n");
    return 0;
  }

  newid = LIBMTP_Create_Folder(device, argv[1], atol(argv[2]), atol(argv[3]));
  if (newid == 0) {
    printf("Folder creation failed.\n");
    return 1;
  } else {
    printf("New folder created with ID: %d\n", newid);
  }
  return 0;
}

int
newfolder_function(char * path)
{
  printf("Creating new folder %s\n",path);
  char * parent = dirname(path);
  char * folder = basename(path);
  int id = parse_path (parent,files,folders);
  int newid = LIBMTP_Create_Folder(device, folder, id, 0);
  if (newid == 0) {
    printf("Folder creation failed.\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
    return 1;
  } else {
    printf("New folder created with ID: %d\n", newid);
  }
  return 0;
}

