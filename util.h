/* Copyright (c) 2006-2013 Jonas Fonseca <fonseca@diku.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef UTIL_H
#define UTIL_H

#include "tig.h"
#include "options.h"

struct time {
	time_t sec;
	int tz;
};

struct ident {
	const char *name;
	const char *email;
};

extern const struct ident unknown_ident;
#define author_trim(cols) (cols == 0 || cols > 10)

const char *mkdate(const struct time *time, enum date date);
const char *mkfilesize(unsigned long size, enum file_size format);
const char *mkauthor(const struct ident *ident, int cols, enum author author);
const char *mkmode(mode_t mode);

int timecmp(const struct time *t1, const struct time *t2);
int ident_compare(const struct ident *i1, const struct ident *i2);

const char *get_temp_dir(void);

#endif
