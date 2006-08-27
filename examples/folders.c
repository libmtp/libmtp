#include "common.h"

static void dump_folder_list(LIBMTP_folder_t *folderlist, int level)
{
  int i;
  if(folderlist==NULL) {
    return;
  }

  printf("%u\t", folderlist->folder_id);
  for(i=0;i<level;i++) printf("  ");

  printf("%s\n", folderlist->name);

  dump_folder_list(folderlist->child, level+1);
  dump_folder_list(folderlist->sibling, level);
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_folder_t *folders;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get folder listing.
  folders = LIBMTP_Get_Folder_List(device);
  
  if(folders == NULL) {
    printf("No folders found\n");
  } else {
    dump_folder_list(folders,0);
  }

  LIBMTP_destroy_folder_t(folders);

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

