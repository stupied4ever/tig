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
#include "line.h"

static struct line_info line_info[] = {
#define LINE(type, line, fg, bg, attr) \
	{ #type, STRING_SIZE(#type), (line), STRING_SIZE(line), (fg), (bg), (attr) }
	LINE_INFO
#undef	LINE
};

static struct line_info **color_pair;
static size_t color_pairs;

static struct line_info *custom_color;
static size_t custom_colors;

DEFINE_ALLOCATOR(realloc_custom_color, struct line_info, 8)
DEFINE_ALLOCATOR(realloc_color_pair, struct line_info *, 8)

#define TO_CUSTOM_COLOR_TYPE(type)	(LINE_NONE + 1 + (type))
#define TO_CUSTOM_COLOR_OFFSET(type)	((type) - LINE_NONE - 1)

/* color ids must be 1 or higher. [gh #15] */
#define color_id(line_type)		((line_type) + 1)

enum line_type
get_line_type(const char *line)
{
	int linelen = strlen(line);
	enum line_type type;

	for (type = 0; type < custom_colors; type++)
		/* Case insensitive search matches Signed-off-by lines better. */
		if (linelen >= custom_color[type].linelen &&
		    !strncasecmp(custom_color[type].line, line, custom_color[type].linelen))
			return TO_CUSTOM_COLOR_TYPE(type);

	for (type = 0; type < ARRAY_SIZE(line_info); type++)
		/* Case insensitive search matches Signed-off-by lines better. */
		if (linelen >= line_info[type].linelen &&
		    !strncasecmp(line_info[type].line, line, line_info[type].linelen))
			return type;

	return LINE_DEFAULT;
}

enum line_type
get_line_type_from_ref(const struct ref *ref)
{
	if (ref->head)
		return LINE_MAIN_HEAD;
	else if (ref->ltag)
		return LINE_MAIN_LOCAL_TAG;
	else if (ref->tag)
		return LINE_MAIN_TAG;
	else if (ref->tracked)
		return LINE_MAIN_TRACKED;
	else if (ref->remote)
		return LINE_MAIN_REMOTE;
	else if (ref->replace)
		return LINE_MAIN_REPLACE;

	return LINE_MAIN_REF;
}

struct line_info *
get_line_info(enum line_type type)
{
	if (type > LINE_NONE) {
		assert(TO_CUSTOM_COLOR_OFFSET(type) < custom_colors);
		return &custom_color[TO_CUSTOM_COLOR_OFFSET(type)];
	} else {
		assert(type < ARRAY_SIZE(line_info));
		return &line_info[type];
	}
}

struct line_info *
get_line_info_from_name(const char *name)
{
	size_t namelen = strlen(name);
	enum line_type type;

	for (type = 0; type < ARRAY_SIZE(line_info); type++)
		if (enum_equals(line_info[type], name, namelen))
			return &line_info[type];

	return NULL;
}

struct line_info *
add_custom_color(const char *quoted_line)
{
	struct line_info *info;
	char *line;
	size_t linelen;

	if (!realloc_custom_color(&custom_color, custom_colors, 1))
		return NULL;

	linelen = strlen(quoted_line) - 1;
	line = malloc(linelen);
	if (!line)
		return NULL;

	strncpy(line, quoted_line + 1, linelen);
	line[linelen - 1] = 0;

	info = &custom_color[custom_colors++];
	info->name = info->line = line;
	info->namelen = info->linelen = strlen(line);

	return info;
}

static int
init_line_info_color_pair(struct line_info *info, enum line_type type,
	int default_bg, int default_fg)
{
	int bg = info->bg == COLOR_DEFAULT ? default_bg : info->bg;
	int fg = info->fg == COLOR_DEFAULT ? default_fg : info->fg;
	int i;

	for (i = 0; i < color_pairs; i++) {
		if (color_pair[i]->fg == info->fg && color_pair[i]->bg == info->bg) {
			info->color_pair = i;
			return OK;;
		}
	}

	if (!realloc_color_pair(&color_pair, color_pairs, 1))
		return ERR;

	color_pair[color_pairs] = info;
	info->color_pair = color_pairs++;
	init_pair(COLOR_ID(info->color_pair), fg, bg);
	return OK;
}

int
init_colors(void)
{
	int default_bg = line_info[LINE_DEFAULT].bg;
	int default_fg = line_info[LINE_DEFAULT].fg;
	enum line_type type;

	start_color();

	if (assume_default_colors(default_fg, default_bg) == ERR) {
		default_bg = COLOR_BLACK;
		default_fg = COLOR_WHITE;
	}

	for (type = 0; type < ARRAY_SIZE(line_info); type++) {
		struct line_info *info = &line_info[type];

		if (init_line_info_color_pair(info, type, default_bg, default_fg) == ERR)
			return ERR;
	}

	for (type = 0; type < custom_colors; type++) {
		struct line_info *info = &custom_color[type];

		if (init_line_info_color_pair(info, TO_CUSTOM_COLOR_TYPE(type),
					  default_bg, default_fg) == ERR)
			return ERR;
	}

	return OK;
}

/* vim: set ts=8 sw=8 noexpandtab: */
