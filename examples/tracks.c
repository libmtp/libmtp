#include "common.h"

static void dump_trackinfo(LIBMTP_track_t *track)
{
  printf("Track ID: %d\n", track->item_id);
  if (track->title != NULL)
    printf("   Title: %s\n", track->title);
  if (track->artist != NULL)
    printf("   Artist: %s\n", track->artist);
  if (track->genre != NULL)
    printf("   Genre: %s\n", track->genre);
  if (track->album != NULL)
    printf("   Album: %s\n", track->album);
  if (track->date != NULL)
    printf("   Date: %s\n", track->date);
  if (track->filename != NULL)
    printf("   Origfilename: %s\n", track->filename);
  printf("   Track number: %d\n", track->tracknumber);
  printf("   Duration: %d milliseconds\n", track->duration);
  printf("   File size %llu bytes\n", track->filesize);
  printf("   Filetype: %s\n", LIBMTP_Get_Filetype_Description(track->filetype));
  if (track->samplerate != 0) {
    printf("   Sample rate: %u Hz\n", track->samplerate);
  }
  if (track->nochannels != 0) {
    printf("   Number of channels: %u\n", track->nochannels);
  }
  if (track->wavecodec != 0) {
    printf("   WAVE fourCC code: 0x%08X\n", track->wavecodec);
  }
  if (track->bitrate != 0) {
    printf("   Bitrate: %u bits/s\n", track->bitrate);
  }
  if (track->bitratetype != 0) {
    if (track->bitratetype == 1) {
      printf("   Bitrate type: Constant\n");
    } else if (track->bitratetype == 2) {
      printf("   Bitrate type: Variable (VBR)\n");
    } else if (track->bitratetype == 3) {
      printf("   Bitrate type: Free\n");
    } else {
      printf("   Bitrate type: Unknown/Erroneous value\n");
    }
  }
  if (track->rating != 0) {
    printf("   User rating: %u (out of 100)\n", track->rating);
  }
  if (track->usecount != 0) {
    printf("   Use count: %u times\n", track->usecount);
  }
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_track_t *tracks;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get track listing.
  tracks = LIBMTP_Get_Tracklisting(device);
  if (tracks == NULL) {
    printf("No tracks.\n");
  } else {
    LIBMTP_track_t *track, *tmp;
    track = tracks;
    while (track != NULL) {
      dump_trackinfo(track);
      tmp = track;
      track = track->next;
      LIBMTP_destroy_track_t(tmp);
    }
  }
    
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

