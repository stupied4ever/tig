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

#ifndef VIEW_H
#define VIEW_H

#include "tig.h"
#include "io.h"
#include "line.h"
#include "keys.h"

enum view_flag {
	VIEW_NO_FLAGS = 0,
	VIEW_ALWAYS_LINENO	= 1 << 0,
	VIEW_CUSTOM_STATUS	= 1 << 1,
	VIEW_ADD_DESCRIBE_REF	= 1 << 2,
	VIEW_ADD_PAGER_REFS	= 1 << 3,
	VIEW_OPEN_DIFF		= 1 << 4,
	VIEW_NO_REF		= 1 << 5,
	VIEW_NO_GIT_DIR		= 1 << 6,
	VIEW_DIFF_LIKE		= 1 << 7,
	VIEW_STDIN		= 1 << 8,
	VIEW_SEND_CHILD_ENTER	= 1 << 9,
	VIEW_FILE_FILTER	= 1 << 10,
	VIEW_LOG_LIKE		= 1 << 11,
	VIEW_STATUS_LIKE	= 1 << 12,
};

#define view_has_flags(view, flag)	((view)->ops->flags & (flag))

struct position {
	unsigned long offset;	/* Offset of the window top */
	unsigned long col;	/* Offset from the window side. */
	unsigned long lineno;	/* Current line number */
};

struct line {
	enum line_type type;
	unsigned int lineno:24;

	/* State flags */
	unsigned int selected:1;
	unsigned int dirty:1;
	unsigned int cleareol:1;
	unsigned int wrapped:1;

	unsigned int user_flags:6;
	void *data;		/* User data */
};

struct view {
	const char *name;	/* View name */
	const char *id;		/* Points to either of ref_{head,commit,blob} */

	struct view_ops *ops;	/* View operations */

	char ref[SIZEOF_REF];	/* Hovered commit reference */
	char vid[SIZEOF_REF];	/* View ID. Set to id member when updating. */

	int height, width;	/* The width and height of the main window */
	WINDOW *win;		/* The main window */

	/* Navigation */
	struct position pos;	/* Current position. */
	struct position prev_pos; /* Previous position. */

	/* Searching */
	char grep[SIZEOF_STR];	/* Search string */
	regex_t *regex;		/* Pre-compiled regexp */

	/* If non-NULL, points to the view that opened this view. If this view
	 * is closed tig will switch back to the parent view. */
	struct view *parent;
	struct view *prev;

	/* Buffering */
	size_t lines;		/* Total number of lines */
	struct line *line;	/* Line index */
	unsigned int digits;	/* Number of digits in the lines member. */

	/* Number of lines with custom status, not to be counted in the
	 * view title. */
	unsigned int custom_lines;

	/* Drawing */
	struct line *curline;	/* Line currently being drawn. */
	enum line_type curtype;	/* Attribute currently used for drawing. */
	unsigned long col;	/* Column when drawing. */
	bool has_scrolled;	/* View was scrolled. */
	bool force_redraw;	/* Whether to force a redraw after reading. */

	/* Loading */
	const char **argv;	/* Shell command arguments. */
	const char *dir;	/* Directory from which to execute. */
	struct io io;
	struct io *pipe;
	time_t start_time;
	time_t update_secs;
	struct encoding *encoding;
	bool unrefreshable;

	/* Private data */
	void *private;
};

enum open_flags {
	OPEN_DEFAULT = 0,	/* Use default view switching. */
	OPEN_SPLIT = 1,		/* Split current view. */
	OPEN_RELOAD = 4,	/* Reload view even if it is the current. */
	OPEN_REFRESH = 16,	/* Refresh view using previous command. */
	OPEN_PREPARED = 32,	/* Open already prepared command. */
	OPEN_EXTRA = 64,	/* Open extra data from command. */
};

struct view_ops {
	/* What type of content being displayed. Used in the title bar. */
	const char *type;
	/* What keymap does this view have */
	struct keymap keymap;
	/* Flags to control the view behavior. */
	enum view_flag flags;
	/* Size of private data. */
	size_t private_size;
	/* Open and reads in all view content. */
	bool (*open)(struct view *view, enum open_flags flags);
	/* Read one line; updates view->line. */
	bool (*read)(struct view *view, char *data);
	/* Draw one line; @lineno must be < view->height. */
	bool (*draw)(struct view *view, struct line *line, unsigned int lineno);
	/* Depending on view handle a special requests. */
	enum request (*request)(struct view *view, enum request request, struct line *line);
	/* Search for regexp in a line. */
	bool (*grep)(struct view *view, struct line *line);
	/* Select line */
	void (*select)(struct view *view, struct line *line);
	/* Release resources when reloading the view */
	void (*done)(struct view *view);
};

#endif
