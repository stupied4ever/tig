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

#include "tig.h"
#include "util.h"

int timecmp(const struct time *t1, const struct time *t2)
{
	return t1->sec - t2->sec;
}

const char *
mkdate(const struct time *time, enum date date)
{
	static char buf[DATE_WIDTH + 1];
	static const struct enum_map reldate[] = {
		{ "second", 1,			60 * 2 },
		{ "minute", 60,			60 * 60 * 2 },
		{ "hour",   60 * 60,		60 * 60 * 24 * 2 },
		{ "day",    60 * 60 * 24,	60 * 60 * 24 * 7 * 2 },
		{ "week",   60 * 60 * 24 * 7,	60 * 60 * 24 * 7 * 5 },
		{ "month",  60 * 60 * 24 * 30,	60 * 60 * 24 * 365 },
		{ "year",   60 * 60 * 24 * 365, 0 },
	};
	struct tm tm;

	if (!date || !time || !time->sec)
		return "";

	if (date == DATE_RELATIVE) {
		struct timeval now;
		time_t date = time->sec + time->tz;
		time_t seconds;
		int i;

		gettimeofday(&now, NULL);
		seconds = now.tv_sec < date ? date - now.tv_sec : now.tv_sec - date;
		for (i = 0; i < ARRAY_SIZE(reldate); i++) {
			if (seconds >= reldate[i].value && reldate[i].value)
				continue;

			seconds /= reldate[i].namelen;
			if (!string_format(buf, "%ld %s%s %s",
					   seconds, reldate[i].name,
					   seconds > 1 ? "s" : "",
					   now.tv_sec >= date ? "ago" : "ahead"))
				break;
			return buf;
		}
	}

	if (date == DATE_LOCAL) {
		time_t date = time->sec + time->tz;
		localtime_r(&date, &tm);
	}
	else {
		gmtime_r(&time->sec, &tm);
	}
	return strftime(buf, sizeof(buf), DATE_FORMAT, &tm) ? buf : NULL;
}

const char *
mkfilesize(unsigned long size, enum file_size format)
{
	static char buf[64 + 1];
	static const char relsize[] = {
		'B', 'K', 'M', 'G', 'T', 'P'
	};

	if (!format)
		return "";

	if (format == FILE_SIZE_UNITS) {
		const char *fmt = "%.0f%c";
		double rsize = size;
		int i;

		for (i = 0; i < ARRAY_SIZE(relsize); i++) {
			if (rsize > 1024.0 && i + 1 < ARRAY_SIZE(relsize)) {
				rsize /= 1024;
				continue;
			}

			size = rsize * 10;
			if (size % 10 > 0)
				fmt = "%.1f%c";

			return string_format(buf, fmt, rsize, relsize[i])
				? buf : NULL;
		}
	}

	return string_format(buf, "%ld", size) ? buf : NULL;
}

const struct ident unknown_ident = { "Unknown", "unknown@localhost" };

int
ident_compare(const struct ident *i1, const struct ident *i2)
{
	if (!i1 || !i2)
		return (!!i1) - (!!i2);
	if (!i1->name || !i2->name)
		return (!!i1->name) - (!!i2->name);
	return strcmp(i1->name, i2->name);
}

static const char *
get_author_initials(const char *author)
{
	static char initials[AUTHOR_WIDTH * 6 + 1];
	size_t pos = 0;
	const char *end = strchr(author, '\0');

#define is_initial_sep(c) (isspace(c) || ispunct(c) || (c) == '@' || (c) == '-')

	memset(initials, 0, sizeof(initials));
	while (author < end) {
		unsigned char bytes;
		size_t i;

		while (author < end && is_initial_sep(*author))
			author++;

		bytes = utf8_char_length(author, end);
		if (bytes >= sizeof(initials) - 1 - pos)
			break;
		while (bytes--) {
			initials[pos++] = *author++;
		}

		i = pos;
		while (author < end && !is_initial_sep(*author)) {
			bytes = utf8_char_length(author, end);
			if (bytes >= sizeof(initials) - 1 - i) {
				while (author < end && !is_initial_sep(*author))
					author++;
				break;
			}
			while (bytes--) {
				initials[i++] = *author++;
			}
		}

		initials[i++] = 0;
	}

	return initials;
}

static const char *
get_email_user(const char *email)
{
	static char user[AUTHOR_WIDTH * 6 + 1];
	const char *end = strchr(email, '@');
	int length = end ? end - email : strlen(email);

	string_format(user, "%.*s%c", length, email, 0);
	return user;
}

const char *
mkauthor(const struct ident *ident, int cols, enum author author)
{
	bool trim = author_trim(cols);
	bool abbreviate = author == AUTHOR_ABBREVIATED || !trim;

	if (author == AUTHOR_NO || !ident)
		return "";
	if (author == AUTHOR_EMAIL && ident->email)
		return ident->email;
	if (author == AUTHOR_EMAIL_USER && ident->email)
		return get_email_user(ident->email);
	if (abbreviate && ident->name)
		return get_author_initials(ident->name);
	return ident->name;
}

const char *
mkmode(mode_t mode)
{
	if (S_ISDIR(mode))
		return "drwxr-xr-x";
	else if (S_ISLNK(mode))
		return "lrwxrwxrwx";
	else if (S_ISGITLINK(mode))
		return "m---------";
	else if (S_ISREG(mode) && mode & S_IXUSR)
		return "-rwxr-xr-x";
	else if (S_ISREG(mode))
		return "-rw-r--r--";
	else
		return "----------";
}

const char *
get_temp_dir(void)
{
	static const char *tmp;

	if (tmp)
		return tmp;

	if (!tmp)
		tmp = getenv("TMPDIR");
	if (!tmp)
		tmp = getenv("TEMP");
	if (!tmp)
		tmp = getenv("TMP");
	if (!tmp)
		tmp = "/tmp";

	return tmp;
}
