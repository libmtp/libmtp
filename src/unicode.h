#ifndef __MTP__UNICODE__H
#define __MTP__UNICODE__H

#include "config.h"
#ifdef HAVE_ICONV_H
#define USE_ICONV
#endif

void unicode_init(void);
int ucs2_strlen(uint16_t const * const);
char *ucs2le_to_utf8(const uint16_t*);
char *utf16_to_utf8(const uint16_t*);
uint16_t *utf8_to_ucs2le(unsigned char const * const);

#endif /* __MTP__UNICODE__H */
