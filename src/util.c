/**
 * \file util.c
 *
 * This file contains generic utility functions such as can be
 * used for debugging for example.
 *
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* MSVC does not have these */
#ifndef _MSC_VER
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libmtp.h"
#include "util.h"

/**
 * This dumps out a number of bytes to a textual, hexadecimal
 * dump.
 *
 * @param f the file to dump to (e.g. stdout or stderr)
 * @param buf a pointer to the buffer containing the bytes to
 *            be dumped out in hex
 * @param n the number of bytes to dump from this buffer
 */
void data_dump (FILE *f, void *buf, uint32_t n)
{
  unsigned char *bp = (unsigned char *) buf;
  uint32_t i;
  
  for (i = 0; i < n; i++) {
    fprintf(f, "%02x ", *bp);
    bp++;
  }
  fprintf(f, "\n");
}

/**
 * This dumps out a number of bytes to a textual, hexadecimal
 * dump, and also prints out the string ASCII representation
 * for each line of bytes. It will also print the memory address
 * offset from a certain boundry.
 *
 * @param f the file to dump to (e.g. stdout or stderr)
 * @param buf a pointer to the buffer containing the bytes to
 *            be dumped out in hex
 * @param n the number of bytes to dump from this buffer
 * @param dump_boundry the address offset to start at (usually 0)
 */
void data_dump_ascii (FILE *f, void *buf, uint32_t n, uint32_t dump_boundry)
{
  uint32_t remain = n;
  uint32_t ln, lc;
  int i;
  unsigned char *bp = (unsigned char *) buf;
  
  lc = 0;
  while (remain) {
    fprintf(f, "\t%04x:", dump_boundry-0x10);
    
    ln = ( remain > 16 ) ? 16 : remain;
    
    for (i = 0; i < ln; i++) {
      if ( ! (i%2) ) fprintf(f, " ");
      fprintf(f, "%02x", bp[16*lc+i]);
    }
    
    if ( ln < 16 ) {
      int width = ((16-ln)/2)*5 + (2*(ln%2));
      fprintf(f, "%*.*s", width, width, "");
    }
    
    fprintf(f, "\t");
    for (i = 0; i < ln; i++) {
      unsigned char ch= bp[16*lc+i];
      fprintf(f, "%c", ( ch >= 0x20 && ch <= 0x7e ) ? 
	      ch : '.');
    }
    fprintf(f, "\n");
    
    lc++;
    remain -= ln;
    dump_boundry += ln;
  }
}
