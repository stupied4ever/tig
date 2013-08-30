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
#include "io.h"
#include "request.h"
#include "keys.h"
#include "line.h"
#include "options.h"


static int
warn(const char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	fputs("tig warning: ", stderr);
	vfprintf(stderr, msg, args);
	fputs("\n", stderr);
	va_end(args);
	return ERR;
}

/*
 * Options and enum declarations.
 */

DEFINE_ENUM_MAP(filename, FILENAME_ENUM);
DEFINE_ENUM_MAP(ignore_space, IGNORE_SPACE_ENUM);
DEFINE_ENUM_MAP(commit_order, COMMIT_ORDER_ENUM);
DEFINE_ENUM_MAP(graphic, GRAPHIC_ENUM);
DEFINE_ENUM_MAP(date, DATE_ENUM);
DEFINE_ENUM_MAP(file_size, FILE_SIZE_ENUM);
DEFINE_ENUM_MAP(author, AUTHOR_ENUM);

/* Option and state variables. */
struct options opt = {
#define OPT_ENUM(type, tname, name, value)	value,
#define OPT_BOOL(name, value)		value,
#define OPT_INT(name, value)		value,
#define OPT_DOUBLE(name, value)		value,
#define OPT_STR(name, size, value)	value,
#define OPT_ARGV(name, value)		value,
#define OPT_(type, name, value)		value,
	OPTION_INFO
#undef OPT_ENUM
#undef OPT_BOOL
#undef OPT_INT
#undef OPT_DOUBLE
#undef OPT_STR
#undef OPT_ARGV
#undef OPT_
};

struct repo_info repo;

char arg_encoding[] = ENCODING_ARG;
struct encoding *opt_encoding = NULL;

/*
 * User config file handling.
 */

static const char *option_errors[] = {
#define OPT_ERR_(name, msg) msg
	OPT_ERR_INFO
#undef	OPT_ERR_
};

static const struct enum_map color_map[] = {
#define COLOR_MAP(name) ENUM_MAP(#name, COLOR_##name)
	COLOR_MAP(DEFAULT),
	COLOR_MAP(BLACK),
	COLOR_MAP(BLUE),
	COLOR_MAP(CYAN),
	COLOR_MAP(GREEN),
	COLOR_MAP(MAGENTA),
	COLOR_MAP(RED),
	COLOR_MAP(WHITE),
	COLOR_MAP(YELLOW),
};

static const struct enum_map attr_map[] = {
#define ATTR_MAP(name) ENUM_MAP(#name, A_##name)
	ATTR_MAP(NORMAL),
	ATTR_MAP(BLINK),
	ATTR_MAP(BOLD),
	ATTR_MAP(DIM),
	ATTR_MAP(REVERSE),
	ATTR_MAP(STANDOUT),
	ATTR_MAP(UNDERLINE),
};

#define set_attribute(attr, name)	map_enum(attr, attr_map, name)

static enum option_code
parse_step(double *opt, const char *arg)
{
	*opt = atoi(arg);
	if (!strchr(arg, '%'))
		return OPT_OK;

	/* "Shift down" so 100% and 1 does not conflict. */
	*opt = (*opt - 1) / 100;
	if (*opt >= 1.0) {
		*opt = 0.99;
		return OPT_ERR_INVALID_STEP_VALUE;
	}
	if (*opt < 0.0) {
		*opt = 1;
		return OPT_ERR_INVALID_STEP_VALUE;
	}
	return OPT_OK;
}

enum option_code
parse_int(int *opt, const char *arg, int min, int max)
{
	int value = atoi(arg);

	if (min <= value && value <= max) {
		*opt = value;
		return OPT_OK;
	}

	return OPT_ERR_INTEGER_VALUE_OUT_OF_BOUND;
}

#define parse_id(option, arg) \
	parse_int(option, arg, 4, SIZEOF_REV - 1)

static bool
set_color(int *color, const char *name)
{
	if (map_enum(color, color_map, name))
		return TRUE;
	if (!prefixcmp(name, "color"))
		return parse_int(color, name + 5, 0, 255) == OPT_OK;
	/* Used when reading git colors. Git expects a plain int w/o prefix.  */
	return parse_int(color, name, 0, 255) == OPT_OK;
}

/* Wants: object fgcolor bgcolor [attribute] */
static enum option_code
option_color_command(int argc, const char *argv[])
{
	struct line_info *info;

	if (argc < 3)
		return OPT_ERR_WRONG_NUMBER_OF_ARGUMENTS;

	if (*argv[0] == '"' || *argv[0] == '\'') {
		info = add_custom_color(argv[0]);
	} else {
		info = get_line_info_from_name(argv[0]);
	}
	if (!info) {
		static const struct enum_map obsolete[] = {
			ENUM_MAP("main-delim",	LINE_DELIMITER),
			ENUM_MAP("main-date",	LINE_DATE),
			ENUM_MAP("main-author",	LINE_AUTHOR),
			ENUM_MAP("blame-id",	LINE_ID),
		};
		int index;

		if (!map_enum(&index, obsolete, argv[0]))
			return OPT_ERR_UNKNOWN_COLOR_NAME;
		info = get_line_info(index);
	}

	if (!set_color(&info->fg, argv[1]) ||
	    !set_color(&info->bg, argv[2]))
		return OPT_ERR_UNKNOWN_COLOR;

	info->attr = 0;
	while (argc-- > 3) {
		int attr;

		if (!set_attribute(&attr, argv[argc]))
			return OPT_ERR_UNKNOWN_ATTRIBUTE;
		info->attr |= attr;
	}

	return OPT_OK;
}

static enum option_code
parse_bool_matched(bool *opt, const char *arg, bool *matched)
{
	*opt = (!strcmp(arg, "1") || !strcmp(arg, "true") || !strcmp(arg, "yes"))
		? TRUE : FALSE;
	if (matched)
		*matched = *opt || (!strcmp(arg, "0") || !strcmp(arg, "false") || !strcmp(arg, "no"));
	return OPT_OK;
}

#define parse_bool(option, arg) parse_bool_matched(option, arg, NULL)

static enum option_code
parse_enum_do(unsigned int *opt, const char *arg,
	      const struct enum_map *map, size_t map_size)
{
	bool is_true;

	assert(map_size > 1);

	if (map_enum_do(map, map_size, (int *) opt, arg))
		return OPT_OK;

	parse_bool(&is_true, arg);
	*opt = is_true ? map[1].value : map[0].value;
	return OPT_OK;
}

#define parse_enum(option, arg, map) \
	parse_enum_do(option, arg, map, ARRAY_SIZE(map))

static enum option_code
parse_string(char *opt, const char *arg, size_t optsize)
{
	int arglen = strlen(arg);

	switch (arg[0]) {
	case '\"':
	case '\'':
		if (arglen == 1 || arg[arglen - 1] != arg[0])
			return OPT_ERR_UNMATCHED_QUOTATION;
		arg += 1; arglen -= 2;
	default:
		string_ncopy_do(opt, optsize, arg, arglen);
		return OPT_OK;
	}
}

static enum option_code
parse_encoding(struct encoding **encoding_ref, const char *arg, bool priority)
{
	char buf[SIZEOF_STR];
	enum option_code code = parse_string(buf, arg, sizeof(buf));

	if (code == OPT_OK) {
		struct encoding *encoding = *encoding_ref;

		if (encoding && !priority)
			return code;
		encoding = encoding_open(buf);
		if (encoding)
			*encoding_ref = encoding;
	}

	return code;
}

static enum option_code
parse_args(const char ***args, const char *argv[])
{
	if (!argv_copy(args, argv))
		return OPT_ERR_OUT_OF_MEMORY;
	return OPT_OK;
}

static inline void
update_notes_arg()
{
	if (opt.show_notes) {
		string_copy(opt.notes_arg, "--show-notes");
	} else {
		/* Notes are disabled by default when passing --pretty args. */
		string_copy(opt.notes_arg, "");
	}
}

/* Wants: name = value */
static enum option_code
option_set_command(int argc, const char *argv[])
{
	if (argc < 3)
		return OPT_ERR_WRONG_NUMBER_OF_ARGUMENTS;

	if (strcmp(argv[1], "="))
		return OPT_ERR_NO_VALUE_ASSIGNED;

	if (!strcmp(argv[0], "blame-options"))
		return parse_args(&opt.blame_options, argv + 2);

	if (!strcmp(argv[0], "diff-options"))
		return parse_args(&opt.diff_options, argv + 2);

	if (argc != 3)
		return OPT_ERR_WRONG_NUMBER_OF_ARGUMENTS;

	if (!strcmp(argv[0], "show-author"))
		return parse_enum(&opt.show_author, argv[2], author_map);

	if (!strcmp(argv[0], "show-date"))
		return parse_enum(&opt.show_date, argv[2], date_map);

	if (!strcmp(argv[0], "show-rev-graph"))
		return parse_bool(&opt.show_rev_graph, argv[2]);

	if (!strcmp(argv[0], "show-refs"))
		return parse_bool(&opt.show_refs, argv[2]);

	if (!strcmp(argv[0], "show-changes"))
		return parse_bool(&opt.show_changes, argv[2]);

	if (!strcmp(argv[0], "show-notes")) {
		bool matched = FALSE;
		enum option_code res = parse_bool_matched(&opt.show_notes, argv[2], &matched);

		if (res == OPT_OK && matched) {
			update_notes_arg();
			return res;
		}

		opt.show_notes = TRUE;
		strcpy(opt.notes_arg, "--show-notes=");
		res = parse_string(opt.notes_arg + 8, argv[2],
				   sizeof(opt.notes_arg) - 8);
		if (res == OPT_OK && opt.notes_arg[8] == '\0')
			opt.notes_arg[7] = '\0';
		return res;
	}

	if (!strcmp(argv[0], "show-line-numbers"))
		return parse_bool(&opt.show_line_numbers, argv[2]);

	if (!strcmp(argv[0], "line-graphics"))
		return parse_enum(&opt.line_graphics, argv[2], graphic_map);

	if (!strcmp(argv[0], "line-number-interval"))
		return parse_int(&opt.line_number_interval, argv[2], 1, 1024);

	if (!strcmp(argv[0], "author-width"))
		return parse_int(&opt.author_width, argv[2], 0, 1024);

	if (!strcmp(argv[0], "filename-width"))
		return parse_int(&opt.filename_width, argv[2], 0, 1024);

	if (!strcmp(argv[0], "show-filename"))
		return parse_enum(&opt.show_filename, argv[2], filename_map);

	if (!strcmp(argv[0], "show-file-size"))
		return parse_enum(&opt.show_file_size, argv[2], file_size_map);

	if (!strcmp(argv[0], "horizontal-scroll"))
		return parse_step(&opt.horizontal_scroll, argv[2]);

	if (!strcmp(argv[0], "split-view-height"))
		return parse_step(&opt.split_view_height, argv[2]);

	if (!strcmp(argv[0], "vertical-split"))
		return parse_bool(&opt.vertical_split, argv[2]);

	if (!strcmp(argv[0], "tab-size"))
		return parse_int(&opt.tab_size, argv[2], 1, 1024);

	if (!strcmp(argv[0], "diff-context"))
		return parse_int(&opt.diff_context, argv[2], 0, 999999);

	if (!strcmp(argv[0], "ignore-space"))
		return parse_enum(&opt.ignore_space, argv[2], ignore_space_map);

	if (!strcmp(argv[0], "commit-order"))
		return parse_enum(&opt.commit_order, argv[2], commit_order_map);

	if (!strcmp(argv[0], "status-untracked-dirs"))
		return parse_bool(&opt.status_untracked_dirs, argv[2]);

	if (!strcmp(argv[0], "read-git-colors"))
		return parse_bool(&opt.read_git_colors, argv[2]);

	if (!strcmp(argv[0], "ignore-case"))
		return parse_bool(&opt.ignore_case, argv[2]);

	if (!strcmp(argv[0], "focus-child"))
		return parse_bool(&opt.focus_child, argv[2]);

	if (!strcmp(argv[0], "wrap-lines"))
		return parse_bool(&opt.wrap_lines, argv[2]);

	if (!strcmp(argv[0], "show-id"))
		return parse_bool(&opt.show_id, argv[2]);

	if (!strcmp(argv[0], "id-width"))
		return parse_id(&opt.id_width, argv[2]);

	if (!strcmp(argv[0], "title-overflow")) {
		bool matched;
		enum option_code code;

		/*
		 * "title-overflow" is considered a boolint.
		 * We try to parse it as a boolean (and set the value to 50 if true),
		 * otherwise we parse it as an integer and use the given value.
		 */
		code = parse_bool_matched(&opt.show_title_overflow, argv[2], &matched);
		if (code == OPT_OK && matched) {
			if (opt.show_title_overflow)
				opt.title_overflow = 50;
		} else {
			code = parse_int(&opt.title_overflow, argv[2], 2, 1024);
			if (code == OPT_OK)
				opt.show_title_overflow = TRUE;
		}

		return code;
	}

	if (!strcmp(argv[0], "editor-line-number"))
		return parse_bool(&opt.editor_line_number, argv[2]);

	return OPT_ERR_UNKNOWN_VARIABLE_NAME;
}

/* Wants: mode request key */
static enum option_code
option_bind_command(int argc, const char *argv[])
{
	enum request request;
	struct keymap *keymap;
	int key;

	if (argc < 3)
		return OPT_ERR_WRONG_NUMBER_OF_ARGUMENTS;

	if (!(keymap = get_keymap(argv[0])))
		return OPT_ERR_UNKNOWN_KEY_MAP;

	key = get_key_value(argv[1]);
	if (key == ERR)
		return OPT_ERR_UNKNOWN_KEY;

	request = get_request(argv[2]);
	if (request == REQ_UNKNOWN) {
		static const struct enum_map obsolete[] = {
			ENUM_MAP("cherry-pick",		REQ_NONE),
			ENUM_MAP("screen-resize",	REQ_NONE),
			ENUM_MAP("tree-parent",		REQ_PARENT),
		};
		int alias;

		if (map_enum(&alias, obsolete, argv[2])) {
			if (alias != REQ_NONE)
				add_keybinding(keymap, alias, key);
			return OPT_ERR_OBSOLETE_REQUEST_NAME;
		}
	}

	if (request == REQ_UNKNOWN) {
		enum run_request_flag flags = RUN_REQUEST_FORCE;

		if (strchr("!?@<", *argv[2])) {
			while (*argv[2]) {
				if (*argv[2] == '@') {
					flags |= RUN_REQUEST_SILENT;
				} else if (*argv[2] == '?') {
					flags |= RUN_REQUEST_CONFIRM;
				} else if (*argv[2] == '<') {
					flags |= RUN_REQUEST_EXIT;
				} else if (*argv[2] != '!') {
					break;
				}
				argv[2]++;
			}

		} else if (*argv[2] == ':') {
			argv[2]++;
			flags |= RUN_REQUEST_INTERNAL;

		} else {
			return OPT_ERR_UNKNOWN_REQUEST_NAME;
		}

		return add_run_request(keymap, key, argv + 2, flags)
			? OPT_OK : OPT_ERR_OUT_OF_MEMORY;
	}

	add_keybinding(keymap, request, key);

	return OPT_OK;
}


static enum option_code load_option_file(const char *path);

static enum option_code
option_source_command(int argc, const char *argv[])
{
	if (argc < 1)
		return OPT_ERR_WRONG_NUMBER_OF_ARGUMENTS;

	return load_option_file(argv[0]);
}

enum option_code
set_option(const char *opt, char *value)
{
	const char *argv[SIZEOF_ARG];
	int argc = 0;

	if (!argv_from_string(argv, &argc, value))
		return OPT_ERR_TOO_MANY_OPTION_ARGUMENTS;

	if (!strcmp(opt, "color"))
		return option_color_command(argc, argv);

	if (!strcmp(opt, "set"))
		return option_set_command(argc, argv);

	if (!strcmp(opt, "bind"))
		return option_bind_command(argc, argv);

	if (!strcmp(opt, "source"))
		return option_source_command(argc, argv);

	return OPT_ERR_UNKNOWN_OPTION_COMMAND;
}

struct config_state {
	const char *path;
	int lineno;
	bool errors;
};

static int
read_option(char *opt, size_t optlen, char *value, size_t valuelen, void *data)
{
	struct config_state *config = data;
	enum option_code status = OPT_ERR_NO_OPTION_VALUE;

	config->lineno++;

	/* Check for comment markers, since read_properties() will
	 * only ensure opt and value are split at first " \t". */
	optlen = strcspn(opt, "#");
	if (optlen == 0)
		return OK;

	if (opt[optlen] == 0) {
		/* Look for comment endings in the value. */
		size_t len = strcspn(value, "#");

		if (len < valuelen) {
			valuelen = len;
			value[valuelen] = 0;
		}

		status = set_option(opt, value);
	}

	if (status != OPT_OK) {
		warn("%s line %d: %s near '%.*s'", config->path, config->lineno,
		     option_errors[status], (int) optlen, opt);
		config->errors = TRUE;
	}

	/* Always keep going if errors are encountered. */
	return OK;
}

static enum option_code
load_option_file(const char *path)
{
	struct config_state config = { path, 0, FALSE };
	struct io io;
	char buf[SIZEOF_STR];

	/* Do not read configuration from stdin if set to "" */
	if (!path || !strlen(path))
		return OPT_OK;

	if (!prefixcmp(path, "~/")) {
		const char *home = getenv("HOME");

		if (!home || !string_format(buf, "%s/%s", home, path + 2))
			return OPT_ERR_HOME_UNRESOLVABLE;
		path = buf;
	}

	/* It's OK that the file doesn't exist. */
	if (!io_open(&io, "%s", path))
		return OPT_ERR_FILE_DOES_NOT_EXIST;

	if (io_load(&io, " \t", read_option, &config) == ERR ||
	    config.errors == TRUE)
		warn("Errors while loading %s.", path);
	return OPT_OK;
}

int
load_options(void)
{
	const char *tigrc_user = getenv("TIGRC_USER");
	const char *tigrc_system = getenv("TIGRC_SYSTEM");
	const char *tig_diff_opts = getenv("TIG_DIFF_OPTS");
	const bool diff_opts_from_args = !!opt.diff_options;

	if (!tigrc_system)
		tigrc_system = SYSCONFDIR "/tigrc";
	load_option_file(tigrc_system);

	if (!tigrc_user)
		tigrc_user = "~/.tigrc";
	load_option_file(tigrc_user);

	/* Add _after_ loading config files to avoid adding run requests
	 * that conflict with keybindings. */
	add_builtin_run_requests();

	if (!diff_opts_from_args && tig_diff_opts && *tig_diff_opts) {
		static const char *diff_opts[SIZEOF_ARG] = { NULL };
		char buf[SIZEOF_STR];
		int argc = 0;

		if (!string_format(buf, "%s", tig_diff_opts) ||
		    !argv_from_string(diff_opts, &argc, buf))
			warn("TIG_DIFF_OPTS contains too many arguments");
		else if (!argv_copy(&opt.diff_options, diff_opts))
			warn("Failed to format TIG_DIFF_OPTS arguments");
	}

	return OK;
}

/*
 * Repository properties
 */

static void
set_remote_branch(const char *name, const char *value, size_t valuelen)
{
	if (!strcmp(name, ".remote")) {
		string_ncopy(repo.remote, value, valuelen);

	} else if (*repo.remote && !strcmp(name, ".merge")) {
		size_t from = strlen(repo.remote);

		if (!prefixcmp(value, "refs/heads/"))
			value += STRING_SIZE("refs/heads/");

		if (!string_format_from(repo.remote, &from, "/%s", value))
			repo.remote[0] = 0;
	}
}

static void
set_repo_config_option(char *name, char *value, enum option_code (*cmd)(int, const char **))
{
	const char *argv[SIZEOF_ARG] = { name, "=" };
	int argc = 1 + (cmd == option_set_command);
	enum option_code error;

	if (!argv_from_string(argv, &argc, value))
		error = OPT_ERR_TOO_MANY_OPTION_ARGUMENTS;
	else
		error = cmd(argc, argv);

	if (error != OPT_OK)
		warn("Option 'tig.%s': %s", name, option_errors[error]);
}

static int
set_work_tree(const char *value)
{
	char cwd[SIZEOF_STR];

	if (!getcwd(cwd, sizeof(cwd)))
		return warn("Failed to get cwd path: %s", strerror(errno));
	if (chdir(cwd) < 0)
		return warn("Failed to chdir(%s): %s", cwd, strerror(errno));
	if (chdir(repo.git_dir) < 0)
		return warn("Failed to chdir(%s): %s", repo.git_dir, strerror(errno));
	if (!getcwd(repo.git_dir, sizeof(repo.git_dir)))
		return warn("Failed to get git path: %s", strerror(errno));
	if (chdir(value) < 0)
		return warn("Failed to chdir(%s): %s", value, strerror(errno));
	if (!getcwd(cwd, sizeof(cwd)))
		return warn("Failed to get cwd path: %s", strerror(errno));
	if (setenv("GIT_WORK_TREE", cwd, TRUE))
		return warn("Failed to set GIT_WORK_TREE to '%s'", cwd);
	if (setenv("GIT_DIR", repo.git_dir, TRUE))
		return warn("Failed to set GIT_DIR to '%s'", repo.git_dir);
	repo.is_inside_work_tree = TRUE;
	return OK;
}

static void
parse_git_color_option(enum line_type type, char *value)
{
	struct line_info *info = get_line_info(type);
	const char *argv[SIZEOF_ARG];
	int argc = 0;
	bool first_color = TRUE;
	int i;

	if (!argv_from_string(argv, &argc, value))
		return;

	info->fg = COLOR_DEFAULT;
	info->bg = COLOR_DEFAULT;
	info->attr = 0;

	for (i = 0; i < argc; i++) {
		int attr = 0;

		if (set_attribute(&attr, argv[i])) {
			info->attr |= attr;

		} else if (set_color(&attr, argv[i])) {
			if (first_color)
				info->fg = attr;
			else
				info->bg = attr;
			first_color = FALSE;
		}
	}
}

static void
set_git_color_option(const char *name, char *value)
{
	static const struct enum_map color_option_map[] = {
		ENUM_MAP("branch.current", LINE_MAIN_HEAD),
		ENUM_MAP("branch.local", LINE_MAIN_REF),
		ENUM_MAP("branch.plain", LINE_MAIN_REF),
		ENUM_MAP("branch.remote", LINE_MAIN_REMOTE),

		ENUM_MAP("diff.meta", LINE_DIFF_HEADER),
		ENUM_MAP("diff.meta", LINE_DIFF_INDEX),
		ENUM_MAP("diff.meta", LINE_DIFF_OLDMODE),
		ENUM_MAP("diff.meta", LINE_DIFF_NEWMODE),
		ENUM_MAP("diff.frag", LINE_DIFF_CHUNK),
		ENUM_MAP("diff.old", LINE_DIFF_DEL),
		ENUM_MAP("diff.new", LINE_DIFF_ADD),

		//ENUM_MAP("diff.commit", LINE_DIFF_ADD),

		ENUM_MAP("status.branch", LINE_STAT_HEAD),
		//ENUM_MAP("status.nobranch", LINE_STAT_HEAD),
		ENUM_MAP("status.added", LINE_STAT_STAGED),
		ENUM_MAP("status.updated", LINE_STAT_STAGED),
		ENUM_MAP("status.changed", LINE_STAT_UNSTAGED),
		ENUM_MAP("status.untracked", LINE_STAT_UNTRACKED),

	};
	int type = LINE_NONE;

	if (opt.read_git_colors && map_enum(&type, color_option_map, name)) {
		parse_git_color_option(type, value);
	}
}

static void
set_encoding(struct encoding **encoding_ref, const char *arg, bool priority)
{
	if (parse_encoding(encoding_ref, arg, priority) == OPT_OK)
		arg_encoding[0] = 0;
}

static int
read_repo_config_option(char *name, size_t namelen, char *value, size_t valuelen, void *data)
{
	if (!strcmp(name, "i18n.commitencoding"))
		set_encoding(&opt_encoding, value, FALSE);

	else if (!strcmp(name, "gui.encoding"))
		set_encoding(&opt_encoding, value, TRUE);

	else if (!strcmp(name, "core.editor"))
		string_ncopy(opt.editor, value, valuelen);

	else if (!strcmp(name, "core.worktree"))
		return set_work_tree(value);

	else if (!strcmp(name, "core.abbrev"))
		parse_id(&opt.id_width, value);

	else if (!prefixcmp(name, "tig.color."))
		set_repo_config_option(name + 10, value, option_color_command);

	else if (!prefixcmp(name, "tig.bind."))
		set_repo_config_option(name + 9, value, option_bind_command);

	else if (!prefixcmp(name, "tig."))
		set_repo_config_option(name + 4, value, option_set_command);

	else if (!prefixcmp(name, "color."))
		set_git_color_option(name + STRING_SIZE("color."), value);

	else if (*repo.head && !prefixcmp(name, "branch.") &&
		 !strncmp(name + 7, repo.head, strlen(repo.head)))
		set_remote_branch(name + 7 + strlen(repo.head), value, valuelen);

	return OK;
}

int
load_git_config(void)
{
	const char *config_list_argv[] = { "git", "config", "--list", NULL };

	return io_run_load(config_list_argv, "=", read_repo_config_option, NULL);
}

/* vim: set ts=8 sw=8 noexpandtab: */
