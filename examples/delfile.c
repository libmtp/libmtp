/** 
 * \file delfile.c
 * Example program to delete a file off the device.
 *
 * Copyright (C) 2005-2008 Linus Walleij <triad@df.lth.se>
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
#include "string.h"
#include "pathutils.h"
#include <stdlib.h>
#include <limits.h>

void delfile_usage(void);
void delfile_function(char *);
void delfile_command(int, char **);

extern LIBMTP_mtpdevice_t *device;
extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;

void delfile_usage(void)
{
  printf("Usage: delfile [-n] <fileid/trackid> | -f <filename>\n");
}

void
delfile_function(char * path)
{
  uint32_t id = parse_path (path,files,folders);

  if (id > 0) {
    printf("Deleting %s which has item_id:%d\n",path,id);
    int ret = 1;
    ret = LIBMTP_Delete_Object(device, id);
    if (ret != 0) {
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
      printf("Failed to remove file\n");
    }
  }
}

void delfile_command(int argc, char **argv)
{
  int FILENAME = 1;
  int ITEMID = 2;
  int field_type = 0;
  int i;

  if ( argc > 2 ) {
    if (strncmp(argv[1],"-f",2) == 0) {
      field_type = FILENAME;
      strcpy(argv[1],"");
    } else if (strncmp(argv[1],"-n",2) == 0) {
      field_type = ITEMID;
      strcpy(argv[1],"0");
    } else {
      delfile_usage();
      return;
    }
  } else {
    delfile_usage();
    return;
  }

  for (i=1;i<argc;i++) {
    uint32_t id;
    char *endptr;
    int ret = 0;

    if (field_type == ITEMID) {
      // Sanity check song ID
      id = strtoul(argv[i], &endptr, 10);
      if ( *endptr != 0 ) {
        fprintf(stderr, "illegal value %s .. skipping\n", argv[i]);
        id = 0;
      }
    } else {
      if (strlen(argv[i]) > 0) {
        id = parse_path (argv[i],files,folders);
      } else {
        id = 0;
      }
    }
    if (id > 0 ) {
      printf("Deleting %s\n",argv[i]);
      ret = LIBMTP_Delete_Object(device, id);
    }
    if ( ret != 0 ) {
      printf("Failed to delete file:%s\n",argv[i]);
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
      ret = 1;
    }
  }
}

