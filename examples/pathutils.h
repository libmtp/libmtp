#ifndef PATHUTILS_H_INCLUSION_GUARD
#define PATHUTILS_H_INCLUSION_GUARD
int lookup_folder_id (LIBMTP_folder_t *, char *, char *);
int parse_path (char *, LIBMTP_file_t *, LIBMTP_folder_t *);
LIBMTP_filetype_t find_filetype (const char *);
int progress (u_int64_t const, u_int64_t const, void const * const); 
#ifndef HAVE_LIBGEN_H
char *basename(char *in);
#endif
#endif
