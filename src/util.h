#ifndef __MTP__UTIL__H
#define __MTP__UTIL__H

void data_dump(FILE *f, void *buf, uint32_t nbytes);
void data_dump_ascii (FILE *f, void *buf, uint32_t n, uint32_t dump_boundry);

#endif //__MTP__UTIL__H
