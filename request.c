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
#include "request.h"

/*
 * User requests
 */

static const struct request_info req_info[] = {
#define REQ_GROUP(help)	{ 0, NULL, 0, (help) },
#define REQ_(req, help)	{ REQ_##req, (#req), STRING_SIZE(#req), (help) }
	REQ_INFO
#undef	REQ_GROUP
#undef	REQ_
};

enum request
get_request(const char *name)
{
	int namelen = strlen(name);
	int i;

	for (i = 0; i < ARRAY_SIZE(req_info); i++)
		if (enum_equals(req_info[i], name, namelen))
			return req_info[i].request;

	return REQ_UNKNOWN;
}

bool
foreach_request(bool (*visitor)(void *data, const struct request_info *req_info, const char *group), void *data)
{
	const char *group = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(req_info); i++) {
		if (!req_info[i].request) {
			group = req_info[i].help;
			continue;
		}

		if (!visitor(data, &req_info[i], group))
			return FALSE;
	}

	return TRUE;
}


/* vim: set ts=8 sw=8 noexpandtab: */
