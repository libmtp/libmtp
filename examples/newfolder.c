#include "common.h"

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  
  if(argc != 3) {
    printf("Usage: newfolder name id\n");
    return -1;
  }
  
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  LIBMTP_Create_Folder(device, argv[1], atol(argv[2]));

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

