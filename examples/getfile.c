#include "common.h"
#include "pathutils.h"

void get_file(char *,char *);
void getfile(int, char **);

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

static void getfile_usage (void)
{
  fprintf(stderr, "getfile <fileid/trackid> <filename>\n");
}

void
get_file(char * from_path,char * to_path)
{
  int id = parse_path (from_path,files,folders);
  if (id > 0) {
    printf("Getting %s to %s\n",from_path,to_path);
    if (LIBMTP_Get_File_To_File(device, id, to_path, progress, NULL) != 0 ) {
      printf("\nError getting file from MTP device.\n");
    }
  }
}


void getfile(int argc, char **argv)
{
  u_int32_t id;
  char *endptr;
  char *file;

  // We need file ID and filename
  if ( argc != 3 ) {
    getfile_usage();
    return;
  }

  // Sanity check song ID
  id = strtoul(argv[1], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[1]);
    return;
  } else if ( ! id ) {
    fprintf(stderr, "bad file/track id %u\n", id);
    return;
 }

  // Filename, e.g. "foo.mp3"
  file = argv[2];
  printf("Getting file/track %d to local file %s\n", id, file);

  // This function will also work just as well for tracks.
  if (LIBMTP_Get_File_To_File(device, id, file, progress, NULL) != 0 ) {
    printf("\nError getting file from MTP device.\n");
  }
  // Terminate progress bar.
  printf("\n");
  
  return;
}

