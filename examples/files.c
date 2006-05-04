#include "common.h"

static void dump_fileinfo(LIBMTP_file_t *file)
{
  printf("File ID: %d\n", file->item_id);
  if (file->filename != NULL)
    printf("   Filename: %s\n", file->filename);
  printf("   File size %llu bytes\n", file->filesize);
  printf("   Parent ID: %d\n", file->parent_id);
  printf("   Filetype: %s\n", LIBMTP_Get_Filetype_Description(file->filetype));
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_file_t *files;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get track listing.
  files = LIBMTP_Get_Filelisting(device);
  if (files == NULL) {
    printf("No files.\n");
  } else {
    LIBMTP_file_t *file, *tmp;
    file = files;
    while (file != NULL) {
      dump_fileinfo(file);
      tmp = file;
      file = file->next;
      LIBMTP_destroy_file_t(tmp);
    }
  }
    
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

