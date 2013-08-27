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

#ifndef KEYS_H
#define KEYS_H

#include "tig.h"
#include "request.h"

struct keymap {
	const char *name;
	struct keymap *next;
	struct keybinding *data;
	size_t size;
	bool hidden;
};

struct keymap *get_keymaps(void);
struct keymap *get_keymap(const char *name);
void add_keymap(struct keymap *keymap);

const char *get_key_name(int key_value);
int get_key_value(const char *name);

enum request get_keybinding(struct keymap *keymap, int key);
void add_keybinding(struct keymap *table, enum request request, int key);

const char *get_keys(struct keymap *keymap, enum request request, bool all);
#define get_view_key(view, request) get_keys(&(view)->ops->keymap, request, FALSE)

enum run_request_flag {
	RUN_REQUEST_DEFAULT	= 0,
	RUN_REQUEST_FORCE	= 1,
	RUN_REQUEST_SILENT	= 2,
	RUN_REQUEST_CONFIRM	= 4,
	RUN_REQUEST_EXIT	= 8,
	RUN_REQUEST_INTERNAL	= 16,
};

struct run_request {
	struct keymap *keymap;
	int key;
	const char **argv;
	bool silent;
	bool confirm;
	bool exit;
	bool internal;
};

struct run_request *get_run_request(enum request request);
bool add_run_request(struct keymap *keymap, int key, const char **argv, enum run_request_flag flags);
void add_builtin_run_requests(void);

#endif
/* vim: set ts=8 sw=8 noexpandtab: */
