#include "common.h"
#include "string.h"

LIBMTP_folder_t *folders;
LIBMTP_file_t *files;

static void usage(void)
{
  printf("Usage: delfile <fileid/trackid> <filename>\n");
  printf("       if filename is set then fileid/trackid is ignored\n");
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
parse_path (const char * path)
{
  int len = strlen(strrchr(path,'/'));
  char * filename = malloc(len);
  int index = strlen (path) - len;
  filename = strncpy (filename, &path[index+1],len);
  char * parent = malloc(index);
  parent = strncpy(parent, path, index);
  parent[index] = '\0';
  int parent_id = lookup_folder_id(folders,parent,"");

  while (files != NULL) {
    if (files->parent_id == parent_id) {
      if (strcasecmp (files->filename, filename) == 0) {
        int item_id = files->item_id;
        return item_id;
      }
    }
    files = files->next;
  }

  return 0;
}
    
  
int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  u_int32_t id = 0;
  char *endptr;
  int ret = 1;

  if ( argc == 2 ) {
    // Sanity check song ID
    id = strtoul(argv[1], &endptr, 10);
    if ( *endptr != 0 ) {
      fprintf(stderr, "illegal value %s\n", argv[1]);
      usage();
      return 1;
    } else if ( ! id ) {
      fprintf(stderr, "bad file ID %u\n", id);
      usage();
      return 1;
    }
  } else if (argc != 3) {
    usage();
    return 1;
  }
  
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  if (argc == 3) {
    files = LIBMTP_Get_Filelisting (device);
    folders = LIBMTP_Get_Folder_List (device);
    id = parse_path (argv[2]);
    printf ("%d\n",id);
  }

  if (id > 0 ) ret = LIBMTP_Delete_Object(device, id);

  if ( ret != 0 ) {
    printf("Failed to delete file.\n");
    ret = 1;
  }
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return ret;
}

