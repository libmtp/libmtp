#include "common.h"
#include "string.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void usage(void) {
  printf("Usage: newplaylist -i <fileid/trackid> -n <playlistname>\n");
  exit(0);
}

int main (int argc, char **argv) {
  int opt;
  extern int optind;
  extern char *optarg;
  LIBMTP_mtpdevice_t *device = NULL;
  int idcount = 0;
  uint32_t *ids = NULL;
  uint32_t *tmp = NULL;
  char *playlistname = NULL;
 
  while ( (opt = getopt(argc, argv, "hn:i:")) != -1 ) {
    switch (opt) {
    case 'h':
      usage();
    case 'i':
      idcount++;
      if ((tmp = realloc(ids, sizeof(uint32_t) * (idcount))) == NULL) {
        printf("realloc failed\n");
        return 1;
      }
      ids = tmp;
      ids[(idcount-1)] = atoi(strdup(optarg));
      break;
    case 'n':
      playlistname = strdup(optarg);
      break;
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;

  if ( playlistname == NULL) {
    printf("You need to supply a playlist name.\n");
    usage();
  }

  if (idcount == 0) {
    printf("You need to supply one or more track IDs\n");
    usage();
  }

    
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  LIBMTP_playlist_t *playlist = LIBMTP_new_playlist_t();
  playlist->name = playlistname;
  playlist->no_tracks = idcount;
  playlist->tracks = ids;
  int ret = LIBMTP_Create_New_Playlist(device,playlist,0);
  if (ret != 0) {
    printf("Couldn't create playlist object\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  }
  else {
    printf("Created new playlist: %u\n", playlist->playlist_id);
  }

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

