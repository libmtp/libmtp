#include "common.h"

static void dump_albuminfo(LIBMTP_album_t *album)
{
  printf("Album ID:%d\n",album->album_id);
  printf("    Name:%s\n",album->name);
  printf("  Tracks:%d\n\n",album->no_tracks);
}

int main () {
  LIBMTP_mtpdevice_t *device = NULL;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  LIBMTP_album_t *albums,*album,*tmp;
  albums = LIBMTP_Get_Album_List(device);
  album = albums;
  while (album != NULL) {
    dump_albuminfo(album);
    tmp = album;
    album = album->next;
    LIBMTP_destroy_album_t(tmp);
  }
     
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

