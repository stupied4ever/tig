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

#include "keys.h"
#include "io.h"

/*
 * Keys
 */

struct keybinding {
	int alias;
	enum request request;
};

static struct keybinding default_keybindings[] = {
	/* View switching */
	{ 'm',		REQ_VIEW_MAIN },
	{ 'd',		REQ_VIEW_DIFF },
	{ 'l',		REQ_VIEW_LOG },
	{ 't',		REQ_VIEW_TREE },
	{ 'f',		REQ_VIEW_BLOB },
	{ 'B',		REQ_VIEW_BLAME },
	{ 'H',		REQ_VIEW_BRANCH },
	{ 'p',		REQ_VIEW_PAGER },
	{ 'h',		REQ_VIEW_HELP },
	{ 'S',		REQ_VIEW_STATUS },
	{ 'c',		REQ_VIEW_STAGE },
	{ 'y',		REQ_VIEW_STASH },

	/* View manipulation */
	{ 'q',		REQ_VIEW_CLOSE },
	{ KEY_TAB,	REQ_VIEW_NEXT },
	{ KEY_RETURN,	REQ_ENTER },
	{ KEY_UP,	REQ_PREVIOUS },
	{ KEY_CTL('P'),	REQ_PREVIOUS },
	{ KEY_DOWN,	REQ_NEXT },
	{ KEY_CTL('N'),	REQ_NEXT },
	{ 'R',		REQ_REFRESH },
	{ KEY_F(5),	REQ_REFRESH },
	{ 'O',		REQ_MAXIMIZE },
	{ ',',		REQ_PARENT },

	/* View specific */
	{ 'u',		REQ_STATUS_UPDATE },
	{ '!',		REQ_STATUS_REVERT },
	{ 'M',		REQ_STATUS_MERGE },
	{ '1',		REQ_STAGE_UPDATE_LINE },
	{ '@',		REQ_STAGE_NEXT },
	{ '[',		REQ_DIFF_CONTEXT_DOWN },
	{ ']',		REQ_DIFF_CONTEXT_UP },

	/* Cursor navigation */
	{ 'k',		REQ_MOVE_UP },
	{ 'j',		REQ_MOVE_DOWN },
	{ KEY_HOME,	REQ_MOVE_FIRST_LINE },
	{ KEY_END,	REQ_MOVE_LAST_LINE },
	{ KEY_NPAGE,	REQ_MOVE_PAGE_DOWN },
	{ KEY_CTL('D'),	REQ_MOVE_PAGE_DOWN },
	{ ' ',		REQ_MOVE_PAGE_DOWN },
	{ KEY_PPAGE,	REQ_MOVE_PAGE_UP },
	{ KEY_CTL('U'),	REQ_MOVE_PAGE_UP },
	{ 'b',		REQ_MOVE_PAGE_UP },
	{ '-',		REQ_MOVE_PAGE_UP },

	/* Scrolling */
	{ '|',		REQ_SCROLL_FIRST_COL },
	{ KEY_LEFT,	REQ_SCROLL_LEFT },
	{ KEY_RIGHT,	REQ_SCROLL_RIGHT },
	{ KEY_IC,	REQ_SCROLL_LINE_UP },
	{ KEY_CTL('Y'),	REQ_SCROLL_LINE_UP },
	{ KEY_DC,	REQ_SCROLL_LINE_DOWN },
	{ KEY_CTL('E'),	REQ_SCROLL_LINE_DOWN },
	{ 'w',		REQ_SCROLL_PAGE_UP },
	{ 's',		REQ_SCROLL_PAGE_DOWN },

	/* Searching */
	{ '/',		REQ_SEARCH },
	{ '?',		REQ_SEARCH_BACK },
	{ 'n',		REQ_FIND_NEXT },
	{ 'N',		REQ_FIND_PREV },

	/* Misc */
	{ 'Q',		REQ_QUIT },
	{ 'z',		REQ_STOP_LOADING },
	{ 'v',		REQ_SHOW_VERSION },
	{ 'r',		REQ_SCREEN_REDRAW },
	{ KEY_CTL('L'),	REQ_SCREEN_REDRAW },
	{ 'o',		REQ_OPTIONS },
	{ '.',		REQ_TOGGLE_LINENO },
	{ 'D',		REQ_TOGGLE_DATE },
	{ 'A',		REQ_TOGGLE_AUTHOR },
	{ 'g',		REQ_TOGGLE_REV_GRAPH },
	{ '~',		REQ_TOGGLE_GRAPHIC },
	{ '#',		REQ_TOGGLE_FILENAME },
	{ 'F',		REQ_TOGGLE_REFS },
	{ 'I',		REQ_TOGGLE_SORT_ORDER },
	{ 'i',		REQ_TOGGLE_SORT_FIELD },
	{ 'W',		REQ_TOGGLE_IGNORE_SPACE },
	{ 'X',		REQ_TOGGLE_ID },
	{ '%',		REQ_TOGGLE_FILES },
	{ '$',		REQ_TOGGLE_TITLE_OVERFLOW },
	{ ':',		REQ_PROMPT },
	{ 'e',		REQ_EDIT },
};

static struct keymap generic_keymap = { "generic" };
#define is_generic_keymap(keymap) ((keymap) == &generic_keymap)

static struct keymap *keymaps = &generic_keymap;

struct keymap *
get_keymaps(void)
{
	return keymaps;
}

void
add_keymap(struct keymap *keymap)
{
	keymap->next = keymaps;
	keymaps = keymap;
}

struct keymap *
get_keymap(const char *name)
{
	struct keymap *keymap = keymaps;

	while (keymap) {
		if (!strcasecmp(keymap->name, name))
			return keymap;
		keymap = keymap->next;
	}

	return NULL;
}


void
add_keybinding(struct keymap *table, enum request request, int key)
{
	size_t i;

	for (i = 0; i < table->size; i++) {
		if (table->data[i].alias == key) {
			table->data[i].request = request;
			return;
		}
	}

	table->data = realloc(table->data, (table->size + 1) * sizeof(*table->data));
	if (!table->data)
		return;
	table->data[table->size].alias = key;
	table->data[table->size++].request = request;

	if (request == REQ_NONE && is_generic_keymap(table)) {
		int i;

		for (i = 0; i < ARRAY_SIZE(default_keybindings); i++)
			if (default_keybindings[i].alias == key)
				default_keybindings[i].request = REQ_NONE;
	}
}

/* Looks for a key binding first in the given map, then in the generic map, and
 * lastly in the default keybindings. */
enum request
get_keybinding(struct keymap *keymap, int key)
{
	size_t i;

	for (i = 0; i < keymap->size; i++)
		if (keymap->data[i].alias == key)
			return keymap->data[i].request;

	for (i = 0; i < generic_keymap.size; i++)
		if (generic_keymap.data[i].alias == key)
			return generic_keymap.data[i].request;

	for (i = 0; i < ARRAY_SIZE(default_keybindings); i++)
		if (default_keybindings[i].alias == key)
			return default_keybindings[i].request;

	return (enum request) key;
}


struct key {
	const char *name;
	int value;
};

static const struct key key_table[] = {
	{ "Enter",	KEY_RETURN },
	{ "Space",	' ' },
	{ "Backspace",	KEY_BACKSPACE },
	{ "Tab",	KEY_TAB },
	{ "Escape",	KEY_ESC },
	{ "Left",	KEY_LEFT },
	{ "Right",	KEY_RIGHT },
	{ "Up",		KEY_UP },
	{ "Down",	KEY_DOWN },
	{ "Insert",	KEY_IC },
	{ "Delete",	KEY_DC },
	{ "Hash",	'#' },
	{ "Home",	KEY_HOME },
	{ "End",	KEY_END },
	{ "PageUp",	KEY_PPAGE },
	{ "PageDown",	KEY_NPAGE },
	{ "F1",		KEY_F(1) },
	{ "F2",		KEY_F(2) },
	{ "F3",		KEY_F(3) },
	{ "F4",		KEY_F(4) },
	{ "F5",		KEY_F(5) },
	{ "F6",		KEY_F(6) },
	{ "F7",		KEY_F(7) },
	{ "F8",		KEY_F(8) },
	{ "F9",		KEY_F(9) },
	{ "F10",	KEY_F(10) },
	{ "F11",	KEY_F(11) },
	{ "F12",	KEY_F(12) },
};

int
get_key_value(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_table); i++)
		if (!strcasecmp(key_table[i].name, name))
			return key_table[i].value;

	if (strlen(name) == 3 && name[0] == '^' && name[1] == '[' && isprint(*name))
		return (int)name[2] + 0x80;
	if (strlen(name) == 2 && name[0] == '^' && isprint(*name))
		return (int)name[1] & 0x1f;
	if (strlen(name) == 1 && isprint(*name))
		return (int) *name;
	return ERR;
}

const char *
get_key_name(int key_value)
{
	static char key_char[] = "'X'\0";
	const char *seq = NULL;
	int key;

	for (key = 0; key < ARRAY_SIZE(key_table); key++)
		if (key_table[key].value == key_value)
			seq = key_table[key].name;

	if (seq == NULL && key_value < 0x7f) {
		char *s = key_char + 1;

		if (key_value >= 0x20) {
			*s++ = key_value;
		} else {
			*s++ = '^';
			*s++ = 0x40 | (key_value & 0x1f);
		}
		*s++ = '\'';
		*s++ = '\0';
		seq = key_char;
	}

	return seq ? seq : "(no key)";
}

static bool
append_key(char *buf, size_t *pos, const struct keybinding *keybinding)
{
	const char *sep = *pos > 0 ? ", " : "";
	const char *keyname = get_key_name(keybinding->alias);

	return string_nformat(buf, BUFSIZ, pos, "%s%s", sep, keyname);
}

static bool
append_keymap_request_keys(char *buf, size_t *pos, enum request request,
			   struct keymap *keymap, bool all)
{
	int i;

	for (i = 0; i < keymap->size; i++) {
		if (keymap->data[i].request == request) {
			if (!append_key(buf, pos, &keymap->data[i]))
				return FALSE;
			if (!all)
				break;
		}
	}

	return TRUE;
}

const char *
get_keys(struct keymap *keymap, enum request request, bool all)
{
	static char buf[BUFSIZ];
	size_t pos = 0;
	int i;

	buf[pos] = 0;

	if (!append_keymap_request_keys(buf, &pos, request, keymap, all))
		return "Too many keybindings!";
	if (pos > 0 && !all)
		return buf;

	if (!is_generic_keymap(keymap)) {
		/* Only the generic keymap includes the default keybindings when
		 * listing all keys. */
		if (all)
			return buf;

		if (!append_keymap_request_keys(buf, &pos, request, &generic_keymap, all))
			return "Too many keybindings!";
		if (pos)
			return buf;
	}

	for (i = 0; i < ARRAY_SIZE(default_keybindings); i++) {
		if (default_keybindings[i].request == request) {
			if (!append_key(buf, &pos, &default_keybindings[i]))
				return "Too many keybindings!";
			if (!all)
				return buf;
		}
	}

	return buf;
}

/*
 * Run requests / external commands.
 */

static struct run_request *run_request;
static size_t run_requests;

DEFINE_ALLOCATOR(realloc_run_requests, struct run_request, 8)

bool
add_run_request(struct keymap *keymap, int key, const char **argv, enum run_request_flag flags)
{
	bool force = flags & RUN_REQUEST_FORCE;
	struct run_request *req;

	if (!force && get_keybinding(keymap, key) != key)
		return TRUE;

	if (!realloc_run_requests(&run_request, run_requests, 1))
		return FALSE;

	if (!argv_copy(&run_request[run_requests].argv, argv))
		return FALSE;

	req = &run_request[run_requests++];
	req->silent = flags & RUN_REQUEST_SILENT;
	req->confirm = flags & RUN_REQUEST_CONFIRM;
	req->exit = flags & RUN_REQUEST_EXIT;
	req->internal = flags & RUN_REQUEST_INTERNAL;
	req->keymap = keymap;
	req->key = key;

	add_keybinding(keymap, REQ_NONE + run_requests, key);
	return TRUE;
}

struct run_request *
get_run_request(enum request request)
{
	if (request <= REQ_NONE || request > REQ_NONE + run_requests)
		return NULL;
	return &run_request[request - REQ_NONE - 1];
}

void
add_builtin_run_requests(void)
{
	const char *cherry_pick[] = { "git", "cherry-pick", "%(commit)", NULL };
	const char *checkout[] = { "git", "checkout", "%(branch)", NULL };
	const char *commit[] = { "git", "commit", NULL };
	const char *gc[] = { "git", "gc", NULL };
	const char *stash_pop[] = { "git", "stash", "pop", "%(stash)", NULL };

	add_run_request(get_keymap("main"), 'C', cherry_pick, RUN_REQUEST_CONFIRM);
	add_run_request(get_keymap("status"), 'C', commit, RUN_REQUEST_DEFAULT);
	add_run_request(get_keymap("branch"), 'C', checkout, RUN_REQUEST_CONFIRM);
	add_run_request(get_keymap("generic"), 'G', gc, RUN_REQUEST_CONFIRM);
	add_run_request(get_keymap("stash"), 'P', stash_pop, RUN_REQUEST_CONFIRM);
}

/* vim: set ts=8 sw=8 noexpandtab: */
