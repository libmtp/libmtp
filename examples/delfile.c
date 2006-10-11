#include "common.h"
#include "string.h"

LIBMTP_folder_t *folders;
LIBMTP_file_t *files;

static void usage(void)
{
  printf("Usage: delfile [-n] <fileid/trackid> | -f <filename>\n");
}

/* Find the folder_id of a given path
 * Runs by walking through folders structure */
static int
lookup_folder_id (LIBMTP_folder_t * folder, char * path, char * parent)
{
  int ret = -1;
  if (folder == NULL) {
    return ret;
  }
  char * current = malloc (strlen(parent) + strlen(folder->name) + 2);
  sprintf(current,"%s/%s",parent,folder->name);
  if (strcasecmp (path, current) == 0) {
    free (current);
    return folder->folder_id;
  }
  if (strncasecmp (path, current, strlen (current)) == 0) {
    ret = lookup_folder_id (folder->child, path, current);
  }
  free (current);
  if (ret >= 0) {
    return ret;
  }
  ret = lookup_folder_id (folder->sibling, path, parent);
  return ret;
}

/* Parses a string to find item_id */
static int
parse_path (char * path)
{
  // Check if path is a folder
  int item_id = lookup_folder_id(folders,path,"");
  if (item_id == -1) {
  int len = strlen(strrchr(path,'/'));
  char * filename = malloc(len);
  int index = strlen (path) - len;
  filename = strncpy (filename, &path[index+1],len);
  char * parent = malloc(index);
  parent = strncpy(parent, path, index);
  parent[index] = '\0';
  int parent_id = lookup_folder_id(folders,parent,"");
    LIBMTP_file_t * file;
    file = files;
    while (file != NULL) {
      if (file->parent_id == parent_id) {
      if (strcasecmp (files->filename, filename) == 0) {
        int item_id = files->item_id;
        return item_id;
      }
    }
      file = file->next;
    }
  } else {
    return item_id;
  }

  return 0;
}
    
  
int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  u_int32_t id = 0;
  int i;
  char *endptr;
  int ret = 1;
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
      usage();
      return 1;
    }
  } else {
    usage();
    return 1;
  }
  
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }
  if (field_type == FILENAME) {
    files = LIBMTP_Get_Filelisting (device);
    folders = LIBMTP_Get_Folder_List (device);
  }

  for (i=1;i<argc;i++) {
    if (field_type == ITEMID) {
      // Sanity check song ID
      id = strtoul(argv[i], &endptr, 10);
      if ( *endptr != 0 ) {
        fprintf(stderr, "illegal value %s .. skipping\n", argv[i]);
        id = 0;
      }
    } else {
      if (strlen(argv[i]) > 0) {
        id = parse_path (argv[i]);
      } else {
        id = 0;
      }
    }
    ret = 0;
    if (id > 0 ) {
        printf("Deleting %s\n",argv[i]);
        ret = LIBMTP_Delete_Object(device, id);
    }
  if ( ret != 0 ) {
      printf("Failed to delete file:%s\n",argv[i]);
    ret = 1;
  }
  }
  
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return ret;
}

