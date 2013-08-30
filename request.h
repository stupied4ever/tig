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

#ifndef REQUEST_H
#define REQUEST_H

#include "tig.h"

/*
 * User requests
 */

#define VIEW_REQ(id, name, ref) REQ_(VIEW_##id, "Show " #name " view")

#define REQ_INFO \
	REQ_GROUP("View switching") \
	VIEW_INFO(VIEW_REQ), \
	\
	REQ_GROUP("View manipulation") \
	REQ_(ENTER,		"Enter current line and scroll"), \
	REQ_(NEXT,		"Move to next"), \
	REQ_(PREVIOUS,		"Move to previous"), \
	REQ_(PARENT,		"Move to parent"), \
	REQ_(VIEW_NEXT,		"Move focus to next view"), \
	REQ_(REFRESH,		"Reload and refresh"), \
	REQ_(MAXIMIZE,		"Maximize the current view"), \
	REQ_(VIEW_CLOSE,	"Close the current view"), \
	REQ_(QUIT,		"Close all views and quit"), \
	\
	REQ_GROUP("View specific requests") \
	REQ_(STATUS_UPDATE,	"Update file status"), \
	REQ_(STATUS_REVERT,	"Revert file changes"), \
	REQ_(STATUS_MERGE,	"Merge file using external tool"), \
	REQ_(STAGE_UPDATE_LINE,	"Update single line"), \
	REQ_(STAGE_NEXT,	"Find next chunk to stage"), \
	REQ_(DIFF_CONTEXT_DOWN,	"Decrease the diff context"), \
	REQ_(DIFF_CONTEXT_UP,	"Increase the diff context"), \
	\
	REQ_GROUP("Cursor navigation") \
	REQ_(MOVE_UP,		"Move cursor one line up"), \
	REQ_(MOVE_DOWN,		"Move cursor one line down"), \
	REQ_(MOVE_PAGE_DOWN,	"Move cursor one page down"), \
	REQ_(MOVE_PAGE_UP,	"Move cursor one page up"), \
	REQ_(MOVE_FIRST_LINE,	"Move cursor to first line"), \
	REQ_(MOVE_LAST_LINE,	"Move cursor to last line"), \
	\
	REQ_GROUP("Scrolling") \
	REQ_(SCROLL_FIRST_COL,	"Scroll to the first line columns"), \
	REQ_(SCROLL_LEFT,	"Scroll two columns left"), \
	REQ_(SCROLL_RIGHT,	"Scroll two columns right"), \
	REQ_(SCROLL_LINE_UP,	"Scroll one line up"), \
	REQ_(SCROLL_LINE_DOWN,	"Scroll one line down"), \
	REQ_(SCROLL_PAGE_UP,	"Scroll one page up"), \
	REQ_(SCROLL_PAGE_DOWN,	"Scroll one page down"), \
	\
	REQ_GROUP("Searching") \
	REQ_(SEARCH,		"Search the view"), \
	REQ_(SEARCH_BACK,	"Search backwards in the view"), \
	REQ_(FIND_NEXT,		"Find next search match"), \
	REQ_(FIND_PREV,		"Find previous search match"), \
	\
	REQ_GROUP("Option manipulation") \
	REQ_(OPTIONS,		"Open option menu"), \
	REQ_(TOGGLE_LINENO,	"Toggle line numbers"), \
	REQ_(TOGGLE_DATE,	"Toggle date display"), \
	REQ_(TOGGLE_AUTHOR,	"Toggle author display"), \
	REQ_(TOGGLE_REV_GRAPH,	"Toggle revision graph visualization"), \
	REQ_(TOGGLE_GRAPHIC,	"Toggle (line) graphics mode"), \
	REQ_(TOGGLE_FILENAME,	"Toggle file name display"), \
	REQ_(TOGGLE_REFS,	"Toggle reference display (tags/branches)"), \
	REQ_(TOGGLE_CHANGES,	"Toggle local changes display in the main view"), \
	REQ_(TOGGLE_SORT_ORDER,	"Toggle ascending/descending sort order"), \
	REQ_(TOGGLE_SORT_FIELD,	"Toggle field to sort by"), \
	REQ_(TOGGLE_IGNORE_SPACE,	"Toggle ignoring whitespace in diffs"), \
	REQ_(TOGGLE_COMMIT_ORDER,	"Toggle commit ordering"), \
	REQ_(TOGGLE_ID,		"Toggle commit ID display"), \
	REQ_(TOGGLE_FILES,	"Toggle file filtering"), \
	REQ_(TOGGLE_TITLE_OVERFLOW,	"Toggle highlighting of commit title overflow"), \
	REQ_(TOGGLE_FILE_SIZE,	"Toggle file size format"), \
	REQ_(TOGGLE_UNTRACKED_DIRS,	"Toggle display of files in untracked directories"), \
	\
	REQ_GROUP("Misc") \
	REQ_(PROMPT,		"Bring up the prompt"), \
	REQ_(SCREEN_REDRAW,	"Redraw the screen"), \
	REQ_(SHOW_VERSION,	"Show version information"), \
	REQ_(STOP_LOADING,	"Stop all loading views"), \
	REQ_(EDIT,		"Open in editor"), \
	REQ_(NONE,		"Do nothing")

/* User action requests. */
enum request {
#define REQ_GROUP(help)
#define REQ_(req, help) REQ_##req

	/* Offset all requests to avoid conflicts with ncurses getch values. */
	REQ_UNKNOWN = KEY_MAX + 1,
	REQ_OFFSET,
	REQ_INFO,

	/* Internal requests. */
	REQ_JUMP_COMMIT,

#undef	REQ_GROUP
#undef	REQ_
};

#endif
