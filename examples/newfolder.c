#include "common.h"
LIBMTP_mtpdevice_t *device;

int newfolder (int argc, char **argv)
{
  uint32_t newid;
  
  if(argc != 3) {
    printf("Usage: newfolder name id\n");
    printf("(id = parent folder or 0 to create the new folder in the root dir)\n");
    return -1;
  }
  
  newid = LIBMTP_Create_Folder(device, argv[1], atol(argv[2]));
  if (newid == 0) {
    printf("Folder creation failed.\n");
  } else {
    printf("New folder created with ID: %d\n", newid);
  }

}

