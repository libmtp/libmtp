#include "common.h"

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  uint32_t newid;
  
  if(argc != 3) {
    printf("Usage: newfolder name id\n");
    printf("(id = parent folder or 0 to create the new folder in the root dir)\n");
    return -1;
  }
  
  LIBMTP_Init();

  device = LIBMTP_Get_First_Device();

  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  newid = LIBMTP_Create_Folder(device, argv[1], atol(argv[2]));
  if (newid == 0) {
    printf("Folder creation failed.\n");
  } else {
    printf("New folder created with ID: %d\n", newid);
  }

  LIBMTP_Release_Device(device);

  printf("OK.\n");
  exit (0);
}

