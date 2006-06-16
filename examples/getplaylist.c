#include "common.h"

uint32_t dump_playlist(LIBMTP_mtpdevice_t *device, uint32_t playlist_id)
{
  uint32_t *items;
  uint32_t len, ret;
  uint32_t i;

  ret = LIBMTP_Get_Object_References (device, playlist_id, &items, &len);
  if (ret != 0) {
    printf("dump_playlist: Could not get object references\n");
    return -1;
  }

  printf("Number of items: %u\n", len);
  if(len > 0) {
    for(i=0;i<len;i++) {
      LIBMTP_track_t *track;

      track = LIBMTP_Get_Trackmetadata(device, items[i]);
      if (track != NULL) {
	printf("   %u: %s - %s\n", items[i], track->artist, track->title);
	LIBMTP_destroy_track_t(track);
      } else {
	printf("   %u: INVALID TRACK REFERENCE!\n", items[i]);
      }
    }
    free(items);
  }
  return 0;
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  u_int32_t id;
  char *endptr;

  // We need file ID
  if ( argc != 2 ) {
    fprintf(stderr, "Just a playlist ID is required\n");
    return 1;
  }

  // Sanity check playlist ID
  id = strtoul(argv[1], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[1]);
    return 1;
  } else if ( ! id ) {
    fprintf(stderr, "bad playlist id %u\n", id);
    return 1;
 }

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices. Connect/replug device and try again.\n");
    exit (0);
  }
  
  if(dump_playlist(device, id) != 0) {
    printf("Error getting playlist from MTP device.\n");
  }
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

