/** 
 * \file sendfile.c
 * Example program to send an arbitrary file to a device.
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
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.h"
#include "libmtp.h"
#include "pathutils.h"

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

int sendfile_function(char *, char *);
void sendfile_command(int, char **);
void sendfile_usage(void);

void sendfile_usage(void)
{
  fprintf(stderr, "usage: sendfile <local filename> <remote filename>\n");
}

int sendfile_function(char * from_path, char *to_path)
{
  printf("Sending %s to %s\n",from_path,to_path);
  char *filename;
  uint64_t filesize;
#ifdef __USE_LARGEFILE64
  struct stat64 sb;
#else
  struct stat sb;
#endif
  LIBMTP_file_t *genfile;
  int ret;
  uint32_t parent_id = 0;

#ifdef __USE_LARGEFILE64
  if ( stat64(from_path, &sb) == -1 ) {
#else
  if ( stat(from_path, &sb) == -1 ) {
#endif
    fprintf(stderr, "%s: ", from_path);
    perror("stat");
    exit(1);
  }

#ifdef __USE_LARGEFILE64
  filesize = sb.st_size;
#else
  filesize = (uint64_t) sb.st_size;
#endif
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

  printf("Sending file...\n");
  ret = LIBMTP_Send_File_From_File(device, from_path, genfile, progress, NULL, parent_id);
  printf("\n");
  if (ret != 0) {
    printf("Error sending file.\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  } else {
    printf("New file ID: %d\n", genfile->item_id);
  }

  LIBMTP_destroy_file_t(genfile);

  return 0;
}

void sendfile_command (int argc, char **argv) {
  if (argc < 3) {
    sendfile_usage();
    return;
  }
  sendfile_function(argv[1],argv[2]);
}
