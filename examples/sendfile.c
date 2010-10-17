/**
 * \file sendfile.c
 * Example program to send an arbitrary file to a device.
 *
 * Copyright (C) 2005-2010 Linus Walleij <triad@df.lth.se>
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.h"
#include "libmtp.h"
#include "pathutils.h"
#include "util.h"
#include "connect.h"

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

void sendfile_usage(void)
{
  fprintf(stderr, "usage: sendfile <local filename> <remote filename>\n");
}

int sendfile_function(char * from_path, char *to_path)
{
  printf("Sending %s to %s\n",from_path,to_path);
  char *filename;
  uint64_t filesize;
  struct stat sb;
  LIBMTP_file_t *genfile;
  int ret;
  uint32_t parent_id = 0;

  if ( stat(from_path, &sb) == -1 ) {
    fprintf(stderr, "%s: ", from_path);
    perror("stat");
    return 1;
  }

  filesize = sb.st_size;
  filename = basename(from_path);
  parent_id = parse_path (to_path,files,folders);
  if (parent_id == -1) {
    printf("Parent folder could not be found, skipping\n");
    return 0;
  }

  genfile = LIBMTP_new_file_t();
  genfile->filesize = filesize;
  genfile->filename = strdup(filename);
  genfile->filetype = find_filetype (filename);
  genfile->parent_id = parent_id;
  genfile->storage_id = 0;

  printf("Sending file...\n");
  ret = LIBMTP_Send_File_From_File(device, from_path, genfile, progress, NULL);
  printf("\n");
  if (ret != 0) {
    printf("Error sending file.\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
    ret = 1;
  } else {
    printf("New file ID: %d\n", genfile->item_id);
  }

  LIBMTP_destroy_file_t(genfile);

  return ret;
}

int sendfile_command (int argc, char **argv) {
  if (argc < 3) {
    sendfile_usage();
    return 0;
  }
  checklang();
  return sendfile_function(argv[1],argv[2]);
}
