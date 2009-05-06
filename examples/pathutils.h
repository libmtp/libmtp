/** 
 * \file pathutils.h
 *
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
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
#ifndef PATHUTILS_H_INCLUSION_GUARD
#define PATHUTILS_H_INCLUSION_GUARD
int parse_path (char *, LIBMTP_file_t *, LIBMTP_folder_t *);
LIBMTP_filetype_t find_filetype (const char *);
int progress (const uint64_t, const uint64_t, void const * const); 
#ifndef HAVE_LIBGEN_H
char *basename(char *in);
#endif
#endif
