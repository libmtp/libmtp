#include <libgen.h>
#include <getopt.h>
#include "common.h"
#include "string.h"
#include "pathutils.h"

LIBMTP_folder_t *folders;
LIBMTP_file_t *files;
LIBMTP_mtpdevice_t *device;

void usage(void);
void delete_item(char *);
void send_file(char *,char *);
int send_track(char *, char *, char *, char *, char *, char *, char *, uint16_t, uint16_t, uint16_t);
void get_file(char *,char *);
void new_folder(char *);
void split_arg(char *,char **, char **);

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

void
delete_item(char * path)
{
  int id = parse_path (path,files,folders);
  if (id > 0) {
    printf("Deleting %s which has item_id:%d\n",path,id);
    int ret = 1;
    ret = LIBMTP_Delete_Object(device, id);
    if (ret != 0) {
      printf("Failed to remove file\n");
    }
  }
}

void
get_file(char * from_path,char * to_path)
{
  int id = parse_path (from_path,files,folders);
  if (id > 0) {
    printf("Getting %s to %s\n",from_path,to_path);
    if (LIBMTP_Get_File_To_File(device, id, to_path, progress, NULL) != 0 ) {
      printf("\nError getting file from MTP device.\n");
    }
  }
}

void
new_folder(char * path)
{
  printf("Creating new folder %s\n",path);
  char * parent = dirname(path);
  char * folder = basename(path);
  int id = parse_path (parent,files,folders);
  int newid = LIBMTP_Create_Folder(device, folder, id);
  if (newid == 0) {
    printf("Folder creation failed.\n");
  } else {
    printf("New folder created with ID: %d\n", newid);
  }
}

int main (int argc, char **argv)
{
  //u_int32_t id = 0;
  int c;

  if ( argc < 2 ) {
    usage ();
    return 1;
  }

  /*
   * Check environment variables $LANG and $LC_CTYPE
   * to see if we want to support UTF-8 unicode
   */
  char * lang = getenv("LANG");
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


  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }
  files = LIBMTP_Get_Filelisting (device);
  folders = LIBMTP_Get_Folder_List (device);

  if ((strncmp(basename(argv[0]),"mtp-sendtr",10) == 0) || (strncmp(basename(argv[0]),"sendtr",6) == 0)) {
    int opt;
    extern int optind;
    extern char *optarg;
    char *partist = NULL;
    char *ptitle = NULL;
    char *pgenre = NULL;
    char *pcodec = NULL;
    char *palbum = NULL;
    char *pfolder = NULL;
    uint16_t tracknum = 0;
    uint16_t length = 0;
    uint16_t year = 0;
    uint16_t quiet = 0;
    char *lang;
      while ( (opt = getopt(argc, argv, "qD:t:a:l:c:g:n:d:y:f:")) != -1 ) {
        switch (opt) {
        case 't':
          ptitle = strdup(optarg);
          break;
        case 'a':
          partist = strdup(optarg);
          break;
        case 'l':
          palbum = strdup(optarg);
          break;
        case 'c':
          pcodec = strdup(optarg); // FIXME: DSM check for MP3, WAV or WMA
          break;
        case 'g':
          pgenre = strdup(optarg);
          break;
        case 'n':
          tracknum = atoi(optarg);
          break;
        case 'd':
          length = atoi(optarg);
          break;
        case 'y':
          year = atoi(optarg);
          break;
        case 'f':
          pfolder = strdup(optarg);
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
    
      if ( argc != 2 ) {
        printf("You need to pass a filename and destination.\n");
        usage();
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

      printf("%s,%s,%s,%s,%s,%s,%s,%d%d,%d\n",argv[0],argv[1],partist,ptitle,pgenre,palbum,pfolder, tracknum, length, year);
      send_track(argv[0],argv[1],partist,ptitle,pgenre,palbum,pfolder, tracknum, length, year);
  } else {  
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
  
      c = getopt_long (argc, argv, "d:f:g:n:t:", long_options, &option_index);
      if (c == -1)
        break;
  
      char *arg1, *arg2;
      switch (c) {
      case 'd':
        printf("Delete %s\n",optarg);
        delete_item(optarg);
        break;
  
      case 'f':
        printf("Send file %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        send_file(arg1,arg2);
        break;
  
      case 'g':
        printf("Get file %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        get_file(arg1,arg2);
        break;
  
      case 'n':
        printf("New folder %s\n",optarg);
        new_folder(optarg);
        break;
  
      case 't':
        printf("Send track %s\n",optarg);
        split_arg(optarg,&arg1,&arg2);
        send_track(arg1,arg2,NULL,NULL,NULL,NULL,NULL,0,0,0);
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

