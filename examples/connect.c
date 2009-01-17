/** 
 * \file connect.c
 * Main programs implementing several utilities in one.
 *
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
 * Copyright (C) 2008-2009 Linus Walleij <triad@df.lth.se>
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

LIBMTP_folder_t *folders;
LIBMTP_file_t *files;
LIBMTP_mtpdevice_t *device;

void usage(void);
void split_arg(char *,char **, char **);
void delfile_function(char *);
void delfile_command(int, char **);
void delfile_usage(void);
int sendtrack_function(char *, char *, char *, char *, char *, char *, char *, char *, uint16_t, uint16_t, uint16_t);
void sendtrack_command (int, char **);
void sendtrack_usage(void);
void sendfile_function(char *,char *);
void sendfile_command(int, char **);
void sendfile_usage(void);
void getfile_function(char *,char *);
void getfile_command(int, char **);
void getfile_usage(void);
void newfolder_function(char *);
void newfolder_command(int,char **);
void newfolder_usage(void);

void
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

void
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
    delfile_command(argc,argv);
  } else if ((strncmp(basename(argv[0]),"mtp-getfile",13) == 0) || (strncmp(basename(argv[0]),"getfile",9) == 0)) {
    getfile_command(argc,argv);
  } else if ((strncmp(basename(argv[0]),"mtp-newfolder",13) == 0) || (strncmp(basename(argv[0]),"newfolder",9) == 0)) {
    newfolder_command(argc,argv);
  } else if ((strncmp(basename(argv[0]),"mtp-sendfile",11) == 0) || (strncmp(basename(argv[0]),"sendfile",7) == 0)) {
    sendfile_command(argc, argv);
  } else if ((strncmp(basename(argv[0]),"mtp-sendtr",10) == 0) || (strncmp(basename(argv[0]),"sendtr",6) == 0)) {
    sendtrack_command(argc, argv);
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
        delfile_function(optarg);
        break;
  
      case 'f':
        printf("Send file %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        sendfile_function(arg1,arg2);
        break;
  
      case 'g':
        printf("Get file %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        getfile_function(arg1,arg2);
        break;
  
      case 'n':
        printf("New folder %s\n",optarg);
        newfolder_function(optarg);
        break;
  
      case 't':
        printf("Send track %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        sendtrack_function(arg1,arg2,NULL,NULL,NULL,NULL,NULL,NULL,0,0,0);
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

  exit (0);
}

