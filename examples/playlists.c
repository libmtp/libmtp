#include "common.h"

static void dump_plinfo(LIBMTP_playlist_t *pl)
{
  uint32_t i;

  printf("Playlist ID: %d\n", pl->playlist_id);
  if (pl->name != NULL)
    printf("   Name: %s\n", pl->name);
  printf("   Tracks: ");
  for (i = 0; i < pl->no_tracks; i++) {
    printf("%d, ", pl->tracks[i]);
  }
  printf("\n");
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_playlist_t *playlists;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get playlist listing.
  playlists = LIBMTP_Get_Playlist_List(device);
  if (playlists == NULL) {
    printf("No playlists.\n");
  } else {
    LIBMTP_playlist_t *pl, *tmp;
    pl = playlists;
    while (pl != NULL) {
      dump_plinfo(pl);
      tmp = pl;
      pl = pl->next;
      LIBMTP_destroy_playlist_t(tmp);
    }
  }
    
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}
