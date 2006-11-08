#include "common.h"
#include "string.h"
#include "pathutils.h"

void delfile_usage(void);
void delete_item(char *);
void delete_files(int, char **);

extern LIBMTP_mtpdevice_t *device;
extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;

void delfile_usage(void)
{
  printf("Usage: delfile [-n] <fileid/trackid> | -f <filename>\n");
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

void delete_files(int argc, char **argv)
{
  int FILENAME = 1;
  int ITEMID = 2;
  int field_type = 0;
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
  int i;
  for (i=1;i<argc;i++) {
    int id;
    char *endptr;
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
    int ret = 0;
    if (id > 0 ) {
      printf("Deleting %s\n",argv[i]);
      ret = LIBMTP_Delete_Object(device, id);
    }
    if ( ret != 0 ) {
      printf("Failed to delete file:%s\n",argv[i]);
      ret = 1;
    }
  }
}

