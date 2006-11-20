#include "common.h"

static void prune_empty_folders(LIBMTP_mtpdevice_t *device, LIBMTP_file_t *files, LIBMTP_folder_t *folderlist, int do_delete)
{
  if(folderlist==NULL)
    return;

  if(folderlist->child == NULL) { // this *might* be empty
    // therefore, check every file for this parent_id
    int found = 0;
    LIBMTP_file_t *file;
    file = files;
    while (file != NULL) {
      if(file->parent_id == folderlist->folder_id) { // folder has a child
        found = 1;
        break;
      }
      file = file->next;
    }

    if(found == 0) { // no files claim this as a parent
      printf("empty folder %u (%s)\n",folderlist->folder_id,folderlist->name);
      if(do_delete) {
        if (LIBMTP_Delete_Object(device,folderlist->folder_id)) {
          printf("Couldn't delete folder %u\n",folderlist->folder_id);
        }
      }
    }
  }

  prune_empty_folders(device,files,folderlist->child,do_delete); // recurse down
  prune_empty_folders(device,files,folderlist->sibling,do_delete); // recurse along
}

int main (int argc, char **argv)
{
  // check if we're doing a dummy run
  int do_delete = 0;
  int opt;
  while ( (opt = getopt(argc, argv, "d")) != -1 ) {
    switch (opt) {
    case 'd':
      do_delete = 1;
      break;
    default:
      break;
    }
  }

  if(do_delete == 0) {
    printf("This is a dummy run. No folders will be deleted.\n");
    printf("To delete folders, use the '-d' option.\n");
  }

  LIBMTP_mtpdevice_t *device;
  LIBMTP_folder_t *folders;
  LIBMTP_file_t *files;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }

  // Get file listing.
  files = LIBMTP_Get_Filelisting_With_Callback(device,NULL,NULL);

  // Get folder listing.
  folders = LIBMTP_Get_Folder_List(device);

  if(folders == NULL) {
    printf("No folders found\n");
  } else {
    prune_empty_folders(device,files,folders,do_delete);
  }

  LIBMTP_destroy_folder_t(folders);
  LIBMTP_destroy_file_t(files);

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

