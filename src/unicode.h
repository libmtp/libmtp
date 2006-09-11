#ifndef __MTP__UNICODE__H
#define __MTP__UNICODE__H

#include "config.h"

void unicode_init(LIBMTP_mtpdevice_t*);
void unicode_deinit(LIBMTP_mtpdevice_t*);
int ucs2_strlen(uint16_t const * const);
char *utf16_to_utf8(LIBMTP_mtpdevice_t*,const uint16_t*);

#endif /* __MTP__UNICODE__H */
