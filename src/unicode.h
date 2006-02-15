#ifndef __MTP__UNICODE__H
#define __MTP__UNICODE__H

int ucs2_strlen(const uint16_t*);
char *ucs2_to_utf8(const uint16_t*);
uint16_t *utf8_to_ucs2(const unsigned char *);

#endif /* __MTP__UNICODE__H */
