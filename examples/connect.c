/**
 * \file connect.c
 * Main programs implementing several utilities in one.
 *
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
 * Copyright (C) 2008-2010 Linus Walleij <triad@df.lth.se>
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
#include <getopt.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "pathutils.h"
#include "connect.h"

LIBMTP_folder_t *folders;
LIBMTP_file_t *files;
LIBMTP_mtpdevice_t *device;

static void
split_arg(char * argument, char ** part1, char ** part2)
{
  char *sepp;
  *part1 = NULL;
  *part2 = NULL;

  sepp = argument + strcspn(argument, ",");
  sepp[0] = '\0';
  *part1 = argument;
  *part2 = sepp+1;
}

static void
usage(void)
{
  printf("Usage: connect <command1> <command2>\n");
  printf("Commands: --delete [filename]\n");
  printf("          --sendfile [source] [destination]\n");
  printf("          --sendtrack [source] [destination]\n");
  printf("          --getfile [source] [destination]\n");
  printf("          --newfolder [foldername]\n");
}


int main (int argc, char **argv)
{
  int ret = 0;

  checklang();

  LIBMTP_Init();

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }
  files = LIBMTP_Get_Filelisting_With_Callback (device, NULL, NULL);
  folders = LIBMTP_Get_Folder_List (device);

  if ((strncmp(basename(argv[0]),"mtp-delfile",11) == 0) || (strncmp(basename(argv[0]),"delfile",7) == 0)) {
    ret = delfile_command(argc,argv);
  } else if ((strncmp(basename(argv[0]),"mtp-getfile",13) == 0) || (strncmp(basename(argv[0]),"getfile",9) == 0)) {
    ret = getfile_command(argc,argv);
  } else if ((strncmp(basename(argv[0]),"mtp-newfolder",13) == 0) || (strncmp(basename(argv[0]),"newfolder",9) == 0)) {
    ret = newfolder_command(argc,argv);
  } else if ((strncmp(basename(argv[0]),"mtp-sendfile",11) == 0) || (strncmp(basename(argv[0]),"sendfile",7) == 0)) {
    ret = sendfile_command(argc, argv);
  } else if ((strncmp(basename(argv[0]),"mtp-sendtr",10) == 0) || (strncmp(basename(argv[0]),"sendtr",6) == 0)) {
    ret = sendtrack_command(argc, argv);
  } else {
    if ( argc < 2 ) {
      usage ();
      return 1;
    }

    while (1) {
      int option_index = 0;
      static struct option long_options[] = {
        {"delete", 1, 0, 'd'},
        {"sendfile", 1, 0, 'f'},
        {"getfile", 1, 0, 'g'},
        {"newfolder", 1, 0, 'n'},
        {"sendtrack", 1, 0, 't'},
        {0, 0, 0, 0}
      };

      int c = getopt_long (argc, argv, "d:f:g:n:t:", long_options, &option_index);
      if (c == -1)
        break;

      char *arg1, *arg2;
      switch (c) {
      case 'd':
        printf("Delete %s\n",optarg);
        ret = delfile_function(optarg);
        break;

      case 'f':
        printf("Send file %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        ret = sendfile_function(arg1,arg2);
        break;

      case 'g':
        printf("Get file %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        ret = getfile_function(arg1,arg2);
        break;

      case 'n':
        printf("New folder %s\n",optarg);
        ret = newfolder_function(optarg);
        break;

      case 't':
        printf("Send track %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        ret = sendtrack_function(arg1,arg2,NULL,NULL,NULL,NULL,NULL,NULL,0,0,0,0,0);
        break;
      }
    }

    if (optind < argc) {
      printf("Unknown options: ");
      while (optind < argc)
        printf("%s ", argv[optind++]);
      printf("\n");
    }
  }

  LIBMTP_Release_Device(device);

  return ret;
}
