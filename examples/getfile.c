/** 
 * \file getfile.c
 * Example program to retrieve a file off the device.
 *
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
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
#include "pathutils.h"
#include <stdlib.h>
#include <limits.h>

void getfile_function(char *,char *);
void getfile_command(int, char **);
void getfile_usage(void);

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

void getfile_usage (void)
{
  fprintf(stderr, "getfile <fileid/trackid> <filename>\n");
}

void
getfile_function(char * from_path,char * to_path)
{
  int id = parse_path (from_path,files,folders);
  if (id > 0) {
    printf("Getting %s to %s\n",from_path,to_path);
    if (LIBMTP_Get_File_To_File(device, id, to_path, progress, NULL) != 0 ) {
      printf("\nError getting file from MTP device.\n");
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
    }
  }
}


void getfile_command(int argc, char **argv)
{
  uint32_t id;
  char *endptr;
  char *file;

  // We need file ID and filename
  if ( argc != 3 ) {
    getfile_usage();
    return;
  }

  // Sanity check song ID
  id = strtoul(argv[1], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[1]);
    return;
  } else if ( ! id ) {
    fprintf(stderr, "bad file/track id %u\n", id);
    return;
 }

  // Filename, e.g. "foo.mp3"
  file = argv[2];
  printf("Getting file/track %d to local file %s\n", id, file);

  // This function will also work just as well for tracks.
  if (LIBMTP_Get_File_To_File(device, id, file, progress, NULL) != 0 ) {
    printf("\nError getting file from MTP device.\n");
  }
  // Terminate progress bar.
  printf("\n");
  
  return;
}

