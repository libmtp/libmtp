#include "common.h"
#include "libmtp.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef HAVE_LIBGEN_H
#include <libgen.h> /* basename() */
#endif

/*
 * This program is derived from the exact equivalent in libnjb.
 *
 * This is an improved commandline track transfer program
 * based on Enrique Jorreto Ledesma's work on the original program by 
 * Shaun Jackman and Linus Walleij.
 */

/* Function that compensate for missing libgen.h on Windows */
#ifndef HAVE_LIBGEN_H
static char *basename(char *in) {
  char *p;
  if (in == NULL)
    return NULL;
  p = in + strlen(in) - 1;
  while (*p != '\\' && *p != '/' && *p != ':')
    { p--; }
  return ++p;
}
#endif

static int progress (uint64_t const sent, uint64_t const total, void const * const data)
{
  int percent = (sent*100)/total;
#ifdef __WIN32__
  printf("Progress: %I64u of %I64u (%d%%)\r", sent, total, percent);
#else
  printf("Progress: %llu of %llu (%d%%)\r", sent, total, percent);
#endif
  fflush(stdout);
  return 0;
}

static char *prompt (const char *prompt, char *buffer, size_t bufsz, int required)
{
  char *cp, *bp;
  
  while (1) {
    fprintf(stdout, "%s> ", prompt);
    if ( fgets(buffer, bufsz, stdin) == NULL ) {
      if (ferror(stdin)) {
	perror("fgets");
      } else {
	fprintf(stderr, "EOF on stdin\n");
      }
      return NULL;
    }
    
    cp = strrchr(buffer, '\n');
    if ( cp != NULL ) *cp = '\0';
    
    bp = buffer;
    while ( bp != cp ) {
      if ( *bp != ' ' && *bp != '\t' ) return bp;
      bp++;
    }
    
    if (! required) return bp;
  }
}

static void usage(void)
{
  fprintf(stderr, "usage: sendfile [ -D debuglvl ] [ -q ] -t type <path>\n");
  fprintf(stderr, "       -f \"Folder Name\"\n");

  exit(1);
}

static uint32_t find_folder_list(char *name, LIBMTP_folder_t *folderlist, int level)
{
  uint32_t i;

  if(folderlist==NULL) {
    return 0;
  } 

  if(!strcasecmp(name, folderlist->name))
    return folderlist->folder_id;

  if ((i = (find_folder_list(name, folderlist->child, level+1))))
    return i;
  if ((i = (find_folder_list(name, folderlist->sibling, level))))
    return i;

  return 0;
}


int main(int argc, char **argv)
{
  int opt;
  extern int optind;
  extern char *optarg;
  char *path, *filename;
  char *ptype = NULL, type[80];
  char *pfolder = NULL;
  uint64_t filesize;
  uint16_t quiet = 0;
  struct stat sb;
  char *lang;
  LIBMTP_mtpdevice_t *device;
  LIBMTP_folder_t *folders = NULL;
  LIBMTP_file_t *genfile;
  int ret;
  uint32_t parent_id = 0;

  LIBMTP_Init();
  
  while ( (opt = getopt(argc, argv, "qht:f:")) != -1 ) {
    switch (opt) {
    case 'h':
      usage();
      exit(0);
    case 'f':
      pfolder = strdup(optarg);
      break;
    case 't':
      ptype = strdup(optarg);
      break;
    case 'q':
      quiet = 1;
      break;
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;
  
  if ( argc != 1 ) {
    printf("You need to pass a filename.\n");
    usage();
  }

  /* Ask for missing parameters if not quiet */
  if (!quiet) {
    if (ptype == NULL) {
      if ( (ptype = prompt("Type", type, 80, 1)) == NULL ) {
        printf("A file type.\n");
        usage();
      }
    }
  }
  
  /*
   * Check environment variables $LANG and $LC_CTYPE
   * to see if we want to support UTF-8 unicode
   */
  lang = getenv("LANG");
  if (lang != NULL) {
    if (strlen(lang) > 5) {
      char *langsuff = &lang[strlen(lang)-5];
      if (strcmp(langsuff, "UTF-8")) {
	printf("Your system does not appear to have UTF-8 enabled ($LANG=\"%s\")\n", lang);
	printf("If you want to have support for diacritics and Unicode characters,\n");
	printf("please switch your locale to an UTF-8 locale, e.g. \"en_US.UTF-8\".\n");
      }
    }
  }
  
  path = argv[0];

  filename = basename(path);
  if (filename == NULL) {
    printf("Error: filename could not be based.\n");
    exit(1);
  }
  if ( stat(path, &sb) == -1 ) {
    fprintf(stderr, "%s: ", path);
    perror("stat");
    exit(1);
  }
  filesize = (uint64_t) sb.st_size;

  genfile = LIBMTP_new_file_t();
  genfile->filesize = filesize;
  genfile->filename = strdup(filename);

  if (!strcasecmp(ptype,"ics")) {
    genfile->filetype = LIBMTP_FILETYPE_CALENDAR;
  } else if (!strcasecmp(ptype,"jpg")) {
    genfile->filetype = LIBMTP_FILETYPE_JFIF;
  } else if (!strcasecmp(ptype,"gif")) {
    genfile->filetype = LIBMTP_FILETYPE_GIF;
  } else if (!strcasecmp(ptype,"png")) {
    genfile->filetype = LIBMTP_FILETYPE_PNG;
  } else {
    printf("Sorry, type \"%s\" is not yet supported\n", ptype);
    printf("Currently supported types: ics,jpg,gif,png\n");
    exit(1);
  }

  printf("Sending file:\n");

  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No MTP device found.\n");
    LIBMTP_destroy_file_t(genfile);
    exit(1);
  }

  if (path == NULL) {
    printf("LIBMTP_Send_File_From_File(): Bad arguments, path was NULL\n");
    return -1;
  }

  if (pfolder) {
    folders = LIBMTP_Get_Folder_List(device);
    if(folders == NULL) {
      printf("No folders found\n");
      exit(1);
    } else {
      parent_id = find_folder_list(pfolder, folders, 0);
      if  (!parent_id) {
        printf("Parent folder could not be found, exiting\n");
        exit(1);
      } else
        printf("parent_id = %d\n", parent_id);
    }
  }
  
  LIBMTP_destroy_folder_t(folders);
  
  printf("Sending file...\n");
  ret = LIBMTP_Send_File_From_File(device, path, genfile, progress, NULL, parent_id);

  printf("\n");
  
  LIBMTP_Release_Device(device);
  
  LIBMTP_destroy_file_t(genfile);
  
  printf("OK.\n");

  return 0;
}

