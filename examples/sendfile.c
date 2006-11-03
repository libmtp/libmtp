#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.h"
#include "libmtp.h"
#include "pathutils.h"

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

int send_file(char *, char *);

int send_file(char * from_path, char *to_path)
{
  printf("Sending %s to %s\n",from_path,to_path);
  char *filename;
  uint64_t filesize;
  struct stat sb;
  LIBMTP_file_t *genfile;
  int ret;
  uint32_t parent_id = 0;
  char * parent;

  if ( stat(from_path, &sb) == -1 ) {
    fprintf(stderr, "%s: ", from_path);
    perror("stat");
    exit(1);
  }

  filesize = (uint64_t) sb.st_size;

  filename = basename(from_path);
  parent_id = parse_path (to_path,files,folders);
  if (parent_id == -1) {
    printf("Parent folder could not be found, skipping\n");
    return 0;
  }
  

  genfile = LIBMTP_new_file_t();
  genfile->filesize = filesize;
  genfile->filename = strdup(filename);
  genfile->filetype = find_filetype (filename);

  printf("Sending file...\n");
  ret = LIBMTP_Send_File_From_File(device, from_path, genfile, progress, NULL, parent_id);

  printf("\n");

  LIBMTP_destroy_file_t(genfile);

  return 0;
}

