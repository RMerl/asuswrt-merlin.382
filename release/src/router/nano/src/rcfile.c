/**************************************************************************
 *   rcfile.c  --  This file is part of GNU nano.                         *
 *                                                                        *
 *   Copyright (C) 2001-2011, 2013-2019 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014 Mike Frysinger                                    *
 *   Copyright (C) 2019 Brand Huntsman                                    *
 *   Copyright (C) 2014-2019 Benno Schulenberg                            *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "proto.h"

#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <string.h>
#include <unistd.h>

#ifdef ENABLE_NANORC

#ifndef RCFILE_NAME
#define HOME_RC_NAME ".nanorc"
#define RCFILE_NAME "nanorc"
#else
#define HOME_RC_NAME RCFILE_NAME
#endif

static const rcoption rcopts[] = {
	{"boldtext", BOLD_TEXT},
#ifdef ENABLE_JUSTIFY
	{"brackets", 0},
#endif
#ifdef ENABLE_WRAPPING
	{"breaklonglines", BREAK_LONG_LINES},
#endif
	{"constantshow", CONSTANT_SHOW},
	{"emptyline", EMPTY_LINE},
#ifdef ENABLED_WRAPORJUSTIFY
	{"fill", 0},
#endif
#ifdef ENABLE_HISTORIES
	{"historylog", HISTORYLOG},
#endif
	{"jumpyscrolling", JUMPY_SCROLLING},
#ifdef ENABLE_LINENUMBERS
	{"linenumbers", LINE_NUMBERS},
#endif
	{"morespace", MORE_SPACE},  /* Deprecated; remove in 2021. */
#ifdef ENABLE_MOUSE
	{"mouse", USE_MOUSE},
#endif
#ifdef ENABLE_MULTIBUFFER
	{"multibuffer", MULTIBUFFER},
#endif
	{"nohelp", NO_HELP},
	{"nonewlines", NO_NEWLINES},
	{"nopauses", NO_PAUSES},
#ifdef ENABLE_WRAPPING
	{"nowrap", NO_WRAP},  /* Deprecated; remove in 2021. */
#endif
#ifdef ENABLE_OPERATINGDIR
	{"operatingdir", 0},
#endif
#ifdef ENABLE_HISTORIES
	{"positionlog", POSITIONLOG},
#endif
	{"preserve", PRESERVE},
#ifdef ENABLE_JUSTIFY
	{"punct", 0},
	{"quotestr", 0},
#endif
	{"quickblank", QUICK_BLANK},
	{"rawsequences", RAW_SEQUENCES},
	{"rebinddelete", REBIND_DELETE},
	{"regexp", USE_REGEXP},
#ifdef ENABLE_SPELLER
	{"speller", 0},
#endif
	{"suspend", SUSPEND},
	{"tabsize", 0},
	{"tempfile", TEMP_FILE},
	{"view", VIEW_MODE},
#ifndef NANO_TINY
	{"afterends", AFTER_ENDS},
	{"allow_insecure_backup", INSECURE_BACKUP},
	{"atblanks", AT_BLANKS},
	{"autoindent", AUTOINDENT},
	{"backup", BACKUP_FILE},
	{"backupdir", 0},
	{"casesensitive", CASE_SENSITIVE},
	{"cutfromcursor", CUT_FROM_CURSOR},
	{"guidestripe", 0},
	{"locking", LOCKING},
	{"matchbrackets", 0},
	{"noconvert", NO_CONVERT},
	{"showcursor", SHOW_CURSOR},
	{"smarthome", SMART_HOME},
	{"smooth", SMOOTH_SCROLL},  /* Deprecated; remove in 2021. */
	{"softwrap", SOFTWRAP},
	{"tabstospaces", TABS_TO_SPACES},
	{"trimblanks", TRIM_BLANKS},
	{"unix", MAKE_IT_UNIX},
	{"whitespace", 0},
	{"wordbounds", WORD_BOUNDS},
	{"wordchars", 0},
	{"zap", LET_THEM_ZAP},
#endif
#ifdef ENABLE_COLOR
	{"titlecolor", 0},
	{"numbercolor", 0},
	{"stripecolor", 0},
	{"selectedcolor", 0},
	{"statuscolor", 0},
	{"errorcolor", 0},
	{"keycolor", 0},
	{"functioncolor", 0},
#endif
	{NULL, 0}
};

static size_t lineno = 0;
		/* The line number of the last encountered error. */
static char *nanorc = NULL;
		/* The path to the rcfile we're parsing. */
#ifdef ENABLE_COLOR
static bool opensyntax = FALSE;
		/* Whether we're allowed to add to the last syntax.  When a file ends,
		 * or when a new syntax command is seen, this bool becomes FALSE. */
static syntaxtype *live_syntax;
		/* The syntax that is currently being parsed. */
static bool seen_color_command = FALSE;
		/* Whether a syntax definition contains any color commands. */
static colortype *lastcolor = NULL;
		/* The end of the color list for the current syntax. */
#endif

static linestruct *errors_head = NULL;
static linestruct *errors_tail = NULL;
		/* Beginning and end of a list of errors in rcfiles, if any. */

/* Send the gathered error messages (if any) to the terminal. */
void display_rcfile_errors()
{
	for (linestruct *error = errors_head; error != NULL; error = error->next)
		fprintf(stderr, "%s\n", error->data);
}

#define MAXSIZE  (PATH_MAX + 200)

/* Store the given error message in a linked list, to be printed upon exit. */
void jot_error(const char *msg, ...)
{
	linestruct *error = make_new_node(errors_tail);
	char textbuf[MAXSIZE];
	int length = 0;
	va_list ap;

	if (errors_head == NULL)
		errors_head = error;
	else
		errors_tail->next = error;
	errors_tail = error;

	if (rcfile_with_errors == NULL)
		rcfile_with_errors = strdup(nanorc);

	if (lineno > 0)
		length = snprintf(textbuf, MAXSIZE, _("Error in %s on line %zu: "),
											nanorc, lineno);

	va_start(ap, msg);
	length += vsnprintf(textbuf + length, MAXSIZE - length, _(msg), ap);
	va_end(ap);

	error->data = nmalloc(length + 1);
	sprintf(error->data, "%s", textbuf);
}

/* Parse the next word from the string, null-terminate it, and return
 * a pointer to the first character after the null terminator.  The
 * returned pointer will point to '\0' if we hit the end of the line. */
char *parse_next_word(char *ptr)
{
	while (!isblank((unsigned char)*ptr) && *ptr != '\0')
		ptr++;

	if (*ptr == '\0')
		return ptr;

	/* Null-terminate and advance ptr. */
	*ptr++ = '\0';

	while (isblank((unsigned char)*ptr))
		ptr++;

	return ptr;
}

/* Parse an argument, with optional quotes, after a keyword that takes
 * one.  If the next word starts with a ", we say that it ends with the
 * last " of the line.  Otherwise, we interpret it as usual, so that the
 * arguments can contain "'s too. */
char *parse_argument(char *ptr)
{
	const char *ptr_save = ptr;
	char *last_quote = NULL;

	if (*ptr != '"')
		return parse_next_word(ptr);

	while (*ptr != '\0') {
		if (*++ptr == '"')
			last_quote = ptr;
	}

	if (last_quote == NULL) {
		jot_error(N_("Argument '%s' has an unterminated \""), ptr_save);
		return NULL;
	}

	*last_quote = '\0';
	ptr = last_quote + 1;

	while (isblank((unsigned char)*ptr))
		ptr++;

	return ptr;
}

#ifdef ENABLE_COLOR
/* Pass over the current regex string in the line starting at ptr,
 * null-terminate it, and return a pointer to the /next/ word. */
char *parse_next_regex(char *ptr)
{
	/* Continue until the end of line, or until a " followed by a
	 * blank character or the end of line. */
	while (*ptr != '\0' && (*ptr != '"' ||
				(*(ptr + 1) != '\0' && !isblank((unsigned char)ptr[1]))))
		ptr++;

	if (*ptr == '\0') {
		jot_error(N_("Regex strings must begin and end with a \" character"));
		return NULL;
	}

	/* Null-terminate and advance ptr. */
	*ptr++ = '\0';

	while (isblank((unsigned char)*ptr))
		ptr++;

	return ptr;
}

/* Compile the given regular expression and store the result in packed (when
 * this pointer is not NULL).  Return TRUE when the expression is valid. */
bool compile(const char *expression, int rex_flags, regex_t **packed)
{
	regex_t *compiled = nmalloc(sizeof(regex_t));
	int outcome = regcomp(compiled, expression, rex_flags);

	if (outcome != 0) {
		size_t length = regerror(outcome, compiled, NULL, 0);
		char *message = charalloc(length);

		regerror(outcome, compiled, message, length);
		jot_error(N_("Bad regex \"%s\": %s"), expression, message);
		free(message);
	}

	if (packed == NULL || outcome != 0) {
		regfree(compiled);
		free(compiled);
	} else
		*packed = compiled;

	return (outcome == 0);
}

/* Parse the next syntax name and its possible extension regexes from the
 * line at ptr, and add it to the global linked list of color syntaxes. */
void begin_new_syntax(char *ptr)
{
	char *nameptr = ptr;

	/* Check that the syntax name is not empty. */
	if (*ptr == '\0' || (*ptr == '"' &&
						(*(ptr + 1) == '\0' || *(ptr + 1) == '"'))) {
		jot_error(N_("Missing syntax name"));
		return;
	}

	ptr = parse_next_word(ptr);

	/* Check that quotes around the name are either paired or absent. */
	if ((*nameptr == '\x22') ^ (nameptr[strlen(nameptr) - 1] == '\x22')) {
		jot_error(N_("Unpaired quote in syntax name"));
		return;
	}

	/* If the name is quoted, strip the quotes. */
	if (*nameptr == '\x22') {
		nameptr++;
		nameptr[strlen(nameptr) - 1] = '\0';
	}

	/* Redefining the "none" syntax is not allowed. */
	if (strcmp(nameptr, "none") == 0) {
		jot_error(N_("The \"none\" syntax is reserved"));
		return;
	}

	/* Initialize a new syntax struct. */
	live_syntax = (syntaxtype *)nmalloc(sizeof(syntaxtype));
	live_syntax->name = mallocstrcpy(NULL, nameptr);
	live_syntax->filename = strdup(nanorc);
	live_syntax->lineno = lineno;
	live_syntax->augmentations = NULL;
	live_syntax->extensions = NULL;
	live_syntax->headers = NULL;
	live_syntax->magics = NULL;
	live_syntax->linter = NULL;
#ifdef ENABLE_COMMENT
	live_syntax->comment = mallocstrcpy(NULL, GENERAL_COMMENT_CHARACTER);
#endif
	live_syntax->color = NULL;
	live_syntax->nmultis = 0;

	/* Hook the new syntax in at the top of the list. */
	live_syntax->next = syntaxes;
	syntaxes = live_syntax;

	opensyntax = TRUE;
	seen_color_command = FALSE;

	/* The default syntax should have no associated extensions. */
	if (strcmp(live_syntax->name, "default") == 0 && *ptr != '\0') {
		jot_error(N_("The \"default\" syntax does not accept extensions"));
		return;
	}

	/* If there seem to be extension regexes, pick them up. */
	if (*ptr != '\0')
		grab_and_store("extension", ptr, &live_syntax->extensions);
}
#endif /* ENABLE_COLOR */

/* Verify that a syntax definition contains at least one color command. */
void check_for_nonempty_syntax(void)
{
#ifdef ENABLE_COLOR
	if (opensyntax && !seen_color_command) {
		size_t current_lineno = lineno;

		lineno = live_syntax->lineno;
		jot_error(N_("Syntax \"%s\" has no color commands"), live_syntax->name);
		lineno = current_lineno;
	}

	opensyntax = FALSE;
#endif
}

/* Return TRUE when the given function is present in almost all menus. */
bool is_universal(void (*func)(void))
{
	return (func == do_left || func == do_right ||
		func == do_home || func == do_end ||
#ifndef NANO_TINY
		func == do_prev_word_void || func == do_next_word_void ||
#endif
		func == do_delete || func == do_backspace ||
		func == cut_text || func == paste_text ||
		func == do_tab || func == do_enter || func == do_verbatim_input);
}

/* Bind or unbind a key combo, to or from a function. */
void parse_binding(char *ptr, bool dobind)
{
	char *keyptr = NULL, *keycopy = NULL, *funcptr = NULL, *menuptr = NULL;
	keystruct *s, *newsc = NULL;
	int menu, mask = 0;
	funcstruct *f;

	check_for_nonempty_syntax();

	if (*ptr == '\0') {
		jot_error(N_("Missing key name"));
		return;
	}

	keyptr = ptr;
	ptr = parse_next_word(ptr);
	keycopy = mallocstrcpy(NULL, keyptr);

	if (strlen(keycopy) < 2) {
		jot_error(N_("Key name is too short"));
		goto free_things;
	}

	/* Uppercase only the first two or three characters of the key name. */
	keycopy[0] = toupper((unsigned char)keycopy[0]);
	keycopy[1] = toupper((unsigned char)keycopy[1]);
	if (keycopy[0] == 'M' && keycopy[1] == '-') {
		if (strlen(keycopy) > 2)
			keycopy[2] = toupper((unsigned char)keycopy[2]);
		else {
			jot_error(N_("Key name is too short"));
			goto free_things;
		}
	}

	/* Allow the codes for Insert and Delete to be rebound, but apart
	 * from those two only Control, Meta and Function sequences. */
	if (!strcasecmp(keycopy, "Ins") || !strcasecmp(keycopy, "Del"))
		keycopy[1] = tolower((unsigned char)keycopy[1]);
	else if (keycopy[0] != '^' && keycopy[0] != 'M' && keycopy[0] != 'F') {
		jot_error(N_("Key name must begin with \"^\", \"M\", or \"F\""));
		goto free_things;
	} else if (keycode_from_string(keycopy) < 0) {
		jot_error(N_("Key name %s is invalid"), keycopy);
		goto free_things;
	}

	if (dobind) {
		funcptr = ptr;
		ptr = parse_argument(ptr);

		if (funcptr[0] == '\0') {
			jot_error(N_("Must specify a function to bind the key to"));
			goto free_things;
		} else if (ptr == NULL)
			goto free_things;
	}

	menuptr = ptr;
	ptr = parse_next_word(ptr);

	if (menuptr[0] == '\0') {
		/* TRANSLATORS: Do not translate the word "all". */
		jot_error(N_("Must specify a menu (or \"all\") "
						"in which to bind/unbind the key"));
		goto free_things;
	}

	if (dobind) {
		/* If the thing to bind starts with a double quote, it is a string,
		 * otherwise it is the name of a function. */
		if (*funcptr == '"') {
			newsc = nmalloc(sizeof(keystruct));
			newsc->func = (functionptrtype)implant;
			newsc->expansion = mallocstrcpy(NULL, funcptr + 1);
#ifndef NANO_TINY
			newsc->toggle = 0;
#endif
		} else
			newsc = strtosc(funcptr);

		if (newsc == NULL) {
			jot_error(N_("Cannot map name \"%s\" to a function"), funcptr);
			goto free_things;
		}
	}

	menu = name_to_menu(menuptr);
	if (menu < 1) {
		jot_error(N_("Cannot map name \"%s\" to a menu"), menuptr);
		goto free_things;
	}

	/* Wipe the given shortcut from the given menu. */
	for (s = sclist; s != NULL; s = s->next)
		if ((s->menus & menu) && strcmp(s->keystr, keycopy) == 0)
			s->menus &= ~menu;

	/* When unbinding, we are done now. */
	if (!dobind)
		goto free_things;

	/* Tally up the menus where the function exists. */
	for (f = allfuncs; f != NULL; f = f->next)
		if (f->func == newsc->func)
			mask = mask | f->menus;

#ifndef NANO_TINY
	/* Handle the special case of the toggles. */
	if (newsc->func == do_toggle_void)
		mask = MMAIN;
#endif
#ifdef ENABLE_NANORC
	/* Handle the special case of a key defined as a string. */
	if (newsc->func == (functionptrtype)implant)
		mask = MMOST | MHELP;
#endif
	/* Now limit the given menu to those where the function exists. */
	menu = menu & (is_universal(newsc->func) ? MMOST : mask);

	if (!menu) {
		if (!ISSET(RESTRICTED) && !ISSET(VIEW_MODE))
			jot_error(N_("Function '%s' does not exist in menu '%s'"),
								funcptr, menuptr);
		goto free_things;
	}

	newsc->menus = menu;
	assign_keyinfo(newsc, keycopy, 0);

	/* Disallow rebinding ^[ and frequent escape-sequence starter "Esc [". */
	if ((!newsc->meta && newsc->keycode == ESC_CODE) ||
				(newsc->meta && newsc->keycode == '[')) {
		jot_error(N_("Keystroke %s may not be rebound"), keycopy);
  free_things:
		free(keycopy);
		free(newsc);
		return;
	}

#ifndef NANO_TINY
	/* If this is a toggle, find and copy its sequence number. */
	if (newsc->func == do_toggle_void) {
		for (s = sclist; s != NULL; s = s->next)
			if (s->func == do_toggle_void && s->toggle == newsc->toggle)
				newsc->ordinal = s->ordinal;
	} else
		newsc->ordinal = 0;
#endif
	/* Add the new shortcut at the start of the list. */
	newsc->next = sclist;
	sclist = newsc;
}

/* Verify that the given file exists, is not a folder nor a device. */
bool is_good_file(char *file)
{
	struct stat rcinfo;

	/* First check that the file exists and is readable. */
	if (access(file, R_OK) != 0)
		return FALSE;

	/* If the thing exists, it may be neither a directory nor a device. */
	if (stat(file, &rcinfo) != -1 && (S_ISDIR(rcinfo.st_mode) ||
				S_ISCHR(rcinfo.st_mode) || S_ISBLK(rcinfo.st_mode))) {
		jot_error(S_ISDIR(rcinfo.st_mode) ? N_("\"%s\" is a directory") :
										N_("\"%s\" is a device file"), file);
		return FALSE;
	} else
		return TRUE;
}

#ifdef ENABLE_COLOR
/* Partially parse the syntaxes in the given file, or (when syntax
 * is not NULL) fully parse one specific syntax from the file . */
void parse_one_include(char *file, syntaxtype *syntax)
{
	char *was_nanorc = nanorc;
	size_t was_lineno = lineno;
	augmentstruct *extra;
	FILE *rcstream;

	/* Don't open directories, character files, or block files. */
	if (!is_good_file(file))
		return;

	rcstream = fopen(file, "rb");

	if (rcstream == NULL) {
		jot_error(N_("Error reading %s: %s"), file, strerror(errno));
		return;
	}

	/* Use the name and line number position of the included syntax file
	 * while parsing it, so we can know where any errors in it are. */
	nanorc = file;
	lineno = 0;

	/* If this is the first pass, parse only the prologue. */
	if (syntax == NULL) {
		parse_rcfile(rcstream, TRUE, TRUE);
		nanorc = was_nanorc;
		lineno = was_lineno;
		return;
	}

	live_syntax = syntax;
	lastcolor = NULL;

	/* Fully parse the given syntax (as it is about to be used). */
	parse_rcfile(rcstream, TRUE, FALSE);

	extra = syntax->augmentations;

	/* Apply any stored extendsyntax commands. */
	while (extra != NULL) {
		char *keyword = extra->data;
		char *therest = parse_next_word(extra->data);

		nanorc = extra->filename;
		lineno = extra->lineno;

		if (!parse_syntax_commands(keyword, therest))
			jot_error(N_("Command \"%s\" not understood"), keyword);

		extra = extra->next;
	}

	free(syntax->filename);
	syntax->filename = NULL;

	nanorc = was_nanorc;
	lineno = was_lineno;
}

/* Expand globs in the passed name, and parse the resultant files. */
void parse_includes(char *ptr)
{
	char *pattern, *expanded;
	glob_t files;
	int result;

	check_for_nonempty_syntax();

	pattern = ptr;
	if (*pattern == '"')
		pattern++;
	ptr = parse_argument(ptr);

	/* Expand a tilde first, then try to match the globbing pattern. */
	expanded = real_dir_from_tilde(pattern);
	result = glob(expanded, GLOB_ERR, NULL, &files);

	/* If there are matches, process each of them.  Otherwise, only
	 * report an error if it's something other than zero matches. */
	if (result == 0) {
		for (size_t i = 0; i < files.gl_pathc; ++i)
			parse_one_include(files.gl_pathv[i], NULL);
	} else if (result != GLOB_NOMATCH)
		jot_error(N_("Error expanding %s: %s"), pattern, strerror(errno));

	globfree(&files);
	free(expanded);
}

/* Return the short value corresponding to the color named in colorname,
 * and set bright to TRUE if that color is bright. */
short color_to_short(const char *colorname, bool *bright)
{
	if (strncasecmp(colorname, "bright", 6) == 0) {
		*bright = TRUE;
		colorname += 6;
	} else
		*bright = FALSE;

	if (strcasecmp(colorname, "green") == 0)
		return COLOR_GREEN;
	else if (strcasecmp(colorname, "red") == 0)
		return COLOR_RED;
	else if (strcasecmp(colorname, "blue") == 0)
		return COLOR_BLUE;
	else if (strcasecmp(colorname, "white") == 0)
		return COLOR_WHITE;
	else if (strcasecmp(colorname, "yellow") == 0)
		return COLOR_YELLOW;
	else if (strcasecmp(colorname, "cyan") == 0)
		return COLOR_CYAN;
	else if (strcasecmp(colorname, "magenta") == 0)
		return COLOR_MAGENTA;
	else if (strcasecmp(colorname, "black") == 0)
		return COLOR_BLACK;
	else if (strcasecmp(colorname, "normal") == 0)
		return USE_THE_DEFAULT;

	jot_error(N_("Color \"%s\" not understood"), colorname);
	return BAD_COLOR;
}

/* Parse the color name (or pair of color names) in the given string.
 * Return FALSE when any color name is invalid; otherwise return TRUE. */
bool parse_color_names(char *combostr, short *fg, short *bg, int *attributes)
{
	char *comma = strchr(combostr, ',');
	bool bright;

	*attributes = A_NORMAL;

	if (comma != NULL) {
		*bg = color_to_short(comma + 1, &bright);
		if (bright) {
			jot_error(N_("A background color cannot be bright"));
			return FALSE;
		}
		if (*bg == BAD_COLOR)
			return FALSE;
		*comma = '\0';
	} else
		*bg = USE_THE_DEFAULT;

	if (comma != combostr) {
		*fg = color_to_short(combostr, &bright);
		if (*fg == BAD_COLOR)
			return FALSE;

		if (bright)
			*attributes = A_BOLD;
	} else
		*fg = USE_THE_DEFAULT;

	return TRUE;
}

/* Parse the color string in the line at ptr, and add it to the current
 * file's associated colors.  rex_flags are the regex compilation flags
 * to use, excluding or including REG_ICASE for case (in)sensitivity. */
void parse_colors(char *ptr, int rex_flags)
{
	short fg, bg;
	int attributes;
	char *item;

	if (*ptr == '\0') {
		jot_error(N_("Missing color name"));
		return;
	}

	item = ptr;
	ptr = parse_next_word(ptr);
	if (!parse_color_names(item, &fg, &bg, &attributes))
		return;

	if (*ptr == '\0') {
		jot_error(N_("Missing regex string after '%s' command"), "color");
		return;
	}

	/* Now for the fun part.  Start adding regexes to individual strings
	 * in the colorstrings array, woo! */
	while (ptr != NULL && *ptr != '\0') {
		colortype *newcolor = NULL;
			/* The container for a color plus its regexes. */
		bool goodstart;
			/* Whether the start expression was valid. */
		bool expectend = FALSE;
			/* Whether to expect an end= line. */

		if (strncasecmp(ptr, "start=", 6) == 0) {
			ptr += 6;
			expectend = TRUE;
		}

		if (*ptr != '"') {
			jot_error(N_("Regex strings must begin and end with a \" character"));
			ptr = parse_next_regex(ptr);
			continue;
		}

		item = ++ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL)
			break;

		if (*item == '\0') {
			jot_error(N_("Empty regex string"));
			goodstart = FALSE;
		} else {
			newcolor = (colortype *)nmalloc(sizeof(colortype));
			goodstart = compile(item, rex_flags, &newcolor->start);
		}

		/* If the start regex is valid, fill in the rest of the data, and
		 * hook the new color struct in at the tail of the linked list. */
		if (goodstart) {
			newcolor->fg = fg;
			newcolor->bg = bg;
			newcolor->attributes = attributes;

			newcolor->end = NULL;
			newcolor->next = NULL;

			if (lastcolor == NULL)
				live_syntax->color = newcolor;
			else
				lastcolor->next = newcolor;

			lastcolor = newcolor;
		} else
			free(newcolor);

		if (!expectend)
			continue;

		if (ptr == NULL || strncasecmp(ptr, "end=", 4) != 0) {
			jot_error(N_("\"start=\" requires a corresponding \"end=\""));
			return;
		}

		ptr += 4;
		if (*ptr != '"') {
			jot_error(N_("Regex strings must begin and end with a \" character"));
			continue;
		}

		item = ++ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL)
			break;

		if (*item == '\0') {
			jot_error(N_("Empty regex string"));
			continue;
		}

		/* If the start regex was invalid, the end regex cannot be saved. */
		if (!goodstart)
			continue;

		/* Save the compiled ending regex (when it's valid). */
		compile(item, rex_flags, &newcolor->end);

		/* Lame way to skip another static counter. */
		newcolor->id = live_syntax->nmultis;
		live_syntax->nmultis++;
	}
}

/* Parse the argument of an interface color option. */
colortype *parse_interface_color(char *combostr)
{
	colortype *trio = nmalloc(sizeof(colortype));

	if (parse_color_names(combostr, &trio->fg, &trio->bg, &trio->attributes)) {
		free(combostr);
		return trio;
	} else {
		free(combostr);
		free(trio);
		return NULL;
	}
}

/* Read regex strings enclosed in double quotes from the line pointed at
 * by ptr, and store them quoteless in the passed storage place. */
void grab_and_store(const char *kind, char *ptr, regexlisttype **storage)
{
	regexlisttype *lastthing;

	if (!opensyntax) {
		jot_error(N_("A '%s' command requires a preceding 'syntax' command"), kind);
		return;
	}

	/* The default syntax doesn't take any file matching stuff. */
	if (strcmp(live_syntax->name, "default") == 0 && *ptr != '\0') {
		jot_error(N_("The \"default\" syntax does not accept '%s' regexes"), kind);
		return;
	}

	if (*ptr == '\0') {
		jot_error(N_("Missing regex string after '%s' command"), kind);
		return;
	}

	lastthing = *storage;

	/* If there was an earlier command, go to the last of those regexes. */
	while (lastthing != NULL && lastthing->next != NULL)
		lastthing = lastthing->next;

	/* Now gather any valid regexes and add them to the linked list. */
	while (*ptr != '\0') {
		const char *regexstring;
		regexlisttype *newthing;

		if (*ptr != '"') {
			jot_error(N_("Regex strings must begin and end with a \" character"));
			return;
		}

		regexstring = ++ptr;
		ptr = parse_next_regex(ptr);
		if (ptr == NULL)
			return;

		/* If the regex string is malformed, skip it. */
		if (!compile(regexstring, NANO_REG_EXTENDED | REG_NOSUB, NULL))
			continue;

		/* Copy the regex into a struct, and hook this in at the end. */
		newthing = (regexlisttype *)nmalloc(sizeof(regexlisttype));
		newthing->full_regex = mallocstrcpy(NULL, regexstring);
		newthing->next = NULL;

		if (lastthing == NULL)
			*storage = newthing;
		else
			lastthing->next = newthing;

		lastthing = newthing;
	}
}

/* Gather and store the string after a comment/linter command. */
void pick_up_name(const char *kind, char *ptr, char **storage)
{
	if (*ptr == '\0') {
		jot_error(N_("Missing argument after '%s'"), kind);
		return;
	}

	/* If the argument starts with a quote, find the terminating quote. */
	if (*ptr == '"') {
		char *look = ptr + strlen(ptr);

		while (*look != '"') {
			if (--look == ptr) {
				jot_error(N_("Argument of '%s' lacks closing \""), kind);
				return;
			}
		}

		*look = '\0';
		ptr++;
	}

	*storage = mallocstrcpy(*storage, ptr);
}

/* Handle the four syntax-only commands. */
bool parse_syntax_commands(char *keyword, char *ptr)
{
	if (strcasecmp(keyword, "color") == 0)
		parse_colors(ptr, NANO_REG_EXTENDED);
	else if (strcasecmp(keyword, "icolor") == 0)
		parse_colors(ptr, NANO_REG_EXTENDED | REG_ICASE);
	else if (strcasecmp(keyword, "comment") == 0) {
#ifdef ENABLE_COMMENT
		pick_up_name("comment", ptr, &live_syntax->comment);
#endif
	} else if (strcasecmp(keyword, "linter") == 0)
		pick_up_name("linter", ptr, &live_syntax->linter);
	else
		return FALSE;

	return TRUE;
}
#endif /* ENABLE_COLOR */

/* Verify that the user has not unmapped every shortcut for a
 * function that we consider 'vital' (such as "Exit"). */
static void check_vitals_mapped(void)
{
	funcstruct *f;
	int v;
#define VITALS 3
	void (*vitals[VITALS])(void) = { do_exit, do_exit, do_cancel };
	int inmenus[VITALS] = { MMAIN, MHELP, MYESNO };

	for  (v = 0; v < VITALS; v++) {
		for (f = allfuncs; f != NULL; f = f->next) {
			if (f->func == vitals[v] && f->menus & inmenus[v]) {
				const keystruct *s = first_sc_for(inmenus[v], f->func);
				if (!s) {
					fprintf(stderr, _("No key is bound to function '%s' in "
										"menu '%s'.  Exiting.\n"), f->desc,
										menu_to_name(inmenus[v]));
					fprintf(stderr, _("If needed, use nano with the -I option "
										"to adjust your nanorc settings.\n"));
					exit(1);
				}
				break;
			}
		}
	}
}

/* Parse the rcfile, once it has been opened successfully at rcstream,
 * and close it afterwards.  If just_syntax is TRUE, allow the file to
 * to contain only color syntax commands. */
void parse_rcfile(FILE *rcstream, bool just_syntax, bool intros_only)
{
	char *buffer = NULL;
	ssize_t len;
	size_t size = 0;

	while ((len = getline(&buffer, &size, rcstream)) > 0) {
		char *ptr, *keyword, *option;
		int set = 0;
		size_t i;

		lineno++;

#ifdef ENABLE_COLOR
		/* If doing a full parse, skip to after the 'syntax' command. */
		if (just_syntax && !intros_only && lineno <= live_syntax->lineno)
			continue;
#endif
		/* Strip the terminating newline, if any. */
		if (buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		ptr = buffer;
		while (isblank((unsigned char)*ptr))
			ptr++;

		/* If the line is empty or a comment, skip to next line. */
		if (*ptr == '\0' || *ptr == '#')
			continue;

		/* Otherwise, skip to the next space. */
		keyword = ptr;
		ptr = parse_next_word(ptr);

#ifdef ENABLE_COLOR
		/* Handle extending first... */
		if (!just_syntax && strcasecmp(keyword, "extendsyntax") == 0) {
			augmentstruct *newitem, *extra;
			char *syntaxname = ptr;
			syntaxtype *sint;

			check_for_nonempty_syntax();

			ptr = parse_next_word(ptr);

			for (sint = syntaxes; sint != NULL; sint = sint->next)
				if (!strcmp(sint->name, syntaxname))
					break;

			if (sint == NULL) {
				jot_error(N_("Could not find syntax \"%s\" to extend"), syntaxname);
				continue;
			}

			newitem = nmalloc(sizeof(augmentstruct));;

			/* Store the content of an 'extendsyntax', for later parsing. */
			newitem->filename = strdup(nanorc);
			newitem->lineno = lineno;
			newitem->data = strdup(ptr);
			newitem->next = NULL;

			if (sint->augmentations != NULL) {
				extra = sint->augmentations;
				while (extra->next != NULL)
					extra = extra->next;
				extra->next = newitem;
			} else
				sint->augmentations = newitem;

			continue;
		}

		/* Try to parse the keyword. */
		if (strcasecmp(keyword, "syntax") == 0) {
			if (intros_only) {
				check_for_nonempty_syntax();
				begin_new_syntax(ptr);
			} else
				break;
		} else if (strcasecmp(keyword, "header") == 0) {
			if (intros_only)
				grab_and_store("header", ptr, &live_syntax->headers);
		} else if (strcasecmp(keyword, "magic") == 0) {
#ifdef HAVE_LIBMAGIC
			if (intros_only)
				grab_and_store("magic", ptr, &live_syntax->magics);
#endif
		} else if (just_syntax && (strcasecmp(keyword, "set") == 0 ||
								strcasecmp(keyword, "unset") == 0 ||
								strcasecmp(keyword, "bind") == 0 ||
								strcasecmp(keyword, "unbind") == 0 ||
								strcasecmp(keyword, "include") == 0 ||
								strcasecmp(keyword, "extendsyntax") == 0)) {
			if (intros_only)
				jot_error(N_("Command \"%s\" not allowed in included file"),
									keyword);
			else
				break;
		} else if (intros_only && (strcasecmp(keyword, "color") == 0 ||
								strcasecmp(keyword, "icolor") == 0 ||
								strcasecmp(keyword, "comment") == 0 ||
								strcasecmp(keyword, "linter") == 0)) {
			if (!opensyntax)
				jot_error(N_("A '%s' command requires a preceding "
									"'syntax' command"), keyword);
			if (strcasestr("icolor", keyword))
				seen_color_command = TRUE;
			continue;
		} else if (parse_syntax_commands(keyword, ptr))
			;
		else if (strcasecmp(keyword, "include") == 0)
			parse_includes(ptr);
		else
#endif /* ENABLE_COLOR */
		if (strcasecmp(keyword, "set") == 0)
			set = 1;
		else if (strcasecmp(keyword, "unset") == 0)
			set = -1;
		else if (strcasecmp(keyword, "bind") == 0)
			parse_binding(ptr, TRUE);
		else if (strcasecmp(keyword, "unbind") == 0)
			parse_binding(ptr, FALSE);
		else if (intros_only)
			jot_error(N_("Command \"%s\" not understood"), keyword);

		if (set == 0)
			continue;

		check_for_nonempty_syntax();

		if (*ptr == '\0') {
			jot_error(N_("Missing option"));
			continue;
		}

		option = ptr;
		ptr = parse_next_word(ptr);

		/* Find the just parsed option name among the existing names. */
		for (i = 0; rcopts[i].name != NULL; i++) {
			if (strcasecmp(option, rcopts[i].name) == 0)
				break;
		}

		if (rcopts[i].name == NULL) {
			jot_error(N_("Unknown option \"%s\""), option);
			continue;
		}

		/* If the option has a flag, set it or unset it, as requested. */
		if (rcopts[i].flag) {
			if (set == 1)
				SET(rcopts[i].flag);
			else
				UNSET(rcopts[i].flag);
			continue;
		}

		/* An option that takes an argument cannot be unset. */
		if (set == -1) {
			jot_error(N_("Cannot unset option \"%s\""), rcopts[i].name);
			continue;
		}

		if (*ptr == '\0') {
			jot_error(N_("Option \"%s\" requires an argument"), rcopts[i].name);
			continue;
		}

		option = ptr;
		if (*option == '"')
			option++;
		ptr = parse_argument(ptr);

		option = mallocstrcpy(NULL, option);

#ifdef ENABLE_UTF8
		/* When in a UTF-8 locale, ignore arguments with invalid sequences. */
		if (using_utf8() && mbstowcs(NULL, option, 0) == (size_t)-1) {
			jot_error(N_("Argument is not a valid multibyte string"));
			continue;
		}
#endif
#ifdef ENABLE_COLOR
		if (strcasecmp(rcopts[i].name, "titlecolor") == 0)
			color_combo[TITLE_BAR] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "numbercolor") == 0)
			color_combo[LINE_NUMBER] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "stripecolor") == 0)
			color_combo[GUIDE_STRIPE] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "selectedcolor") == 0)
			color_combo[SELECTED_TEXT] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "statuscolor") == 0)
			color_combo[STATUS_BAR] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "errorcolor") == 0)
			color_combo[ERROR_MESSAGE] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "keycolor") == 0)
			color_combo[KEY_COMBO] = parse_interface_color(option);
		else if (strcasecmp(rcopts[i].name, "functioncolor") == 0)
			color_combo[FUNCTION_TAG] = parse_interface_color(option);
		else
#endif
#ifdef ENABLE_OPERATINGDIR
		if (strcasecmp(rcopts[i].name, "operatingdir") == 0)
			operating_dir = option;
		else
#endif
#ifdef ENABLED_WRAPORJUSTIFY
		if (strcasecmp(rcopts[i].name, "fill") == 0) {
			if (!parse_num(option, &fill)) {
				jot_error(N_("Requested fill size \"%s\" is invalid"), option);
				fill = -COLUMNS_FROM_EOL;
			}
			free(option);
		} else
#endif
#ifndef NANO_TINY
		if (strcasecmp(rcopts[i].name, "guidestripe") == 0) {
			if (!parse_num(option, &stripe_column) || stripe_column <= 0) {
				jot_error(N_("Guide column \"%s\" is invalid"), option);
				stripe_column = 0;
			}
			free(option);
		} else if (strcasecmp(rcopts[i].name, "matchbrackets") == 0) {
			if (has_blank_char(option)) {
				jot_error(N_("Non-blank characters required"));
				free(option);
			} else if (mbstrlen(option) % 2 != 0) {
				jot_error(N_("Even number of characters required"));
				free(option);
			} else
				matchbrackets = option;
		} else if (strcasecmp(rcopts[i].name, "whitespace") == 0) {
			if (mbstrlen(option) != 2 || breadth(option) != 2) {
				jot_error(N_("Two single-column characters required"));
				free(option);
			} else {
				whitespace = option;
				whitelen[0] = char_length(whitespace);
				whitelen[1] = char_length(whitespace + whitelen[0]);
			}
		} else
#endif
#ifdef ENABLE_JUSTIFY
		if (strcasecmp(rcopts[i].name, "punct") == 0) {
			if (has_blank_char(option)) {
				jot_error(N_("Non-blank characters required"));
				free(option);
			} else
				punct = option;
		} else if (strcasecmp(rcopts[i].name, "brackets") == 0) {
			if (has_blank_char(option)) {
				jot_error(N_("Non-blank characters required"));
				free(option);
			} else
				brackets = option;
		} else if (strcasecmp(rcopts[i].name, "quotestr") == 0)
			quotestr = option;
		else
#endif
#ifndef NANO_TINY
		if (strcasecmp(rcopts[i].name, "backupdir") == 0)
			backup_dir = option;
		else
		if (strcasecmp(rcopts[i].name, "wordchars") == 0)
			word_chars = option;
		else
#endif
#ifdef ENABLE_SPELLER
		if (strcasecmp(rcopts[i].name, "speller") == 0)
			alt_speller = option;
		else
#endif
		if (strcasecmp(rcopts[i].name, "tabsize") == 0) {
			if (!parse_num(option, &tabsize) || tabsize <= 0) {
				jot_error(N_("Requested tab size \"%s\" is invalid"), option);
				tabsize = -1;
			}
			free(option);
		}
	}

	if (intros_only)
		check_for_nonempty_syntax();

	fclose(rcstream);
	free(buffer);
	lineno = 0;

	return;
}

/* Read and interpret one of the two nanorc files. */
void parse_one_nanorc(void)
{
	FILE *rcstream = fopen(nanorc, "rb");

	/* If opening the file succeeded, parse it.  Otherwise, only
	 * complain if the file actually exists. */
	if (rcstream != NULL)
		parse_rcfile(rcstream, FALSE, TRUE);
	else if (errno != ENOENT)
		jot_error(N_("Error reading %s: %s"), nanorc, strerror(errno));
}

bool have_nanorc(const char *path, char *name)
{
	if (path == NULL)
		return FALSE;

	free(nanorc);
	nanorc = concatenate(path, name);

	return is_good_file(nanorc);
}

/* First read the system-wide rcfile, then the user's rcfile. */
void do_rcfiles(void)
{
	const char *xdgconfdir;

	/* First process the system-wide nanorc, if it exists and is suitable. */
	nanorc = mallocstrcpy(nanorc, SYSCONFDIR "/nanorc");
	if (is_good_file(nanorc))
		parse_one_nanorc();

	get_homedir();
	xdgconfdir = getenv("XDG_CONFIG_HOME");

	/* Now try the to find a nanorc file in the user's home directory or in
	 * the XDG configuration directories, and process the first one found. */
	if (have_nanorc(homedir, "/" HOME_RC_NAME) ||
				have_nanorc(xdgconfdir, "/nano/" RCFILE_NAME) ||
				have_nanorc(homedir, "/.config/nano/" RCFILE_NAME))
		parse_one_nanorc();
	else if (homedir == NULL && xdgconfdir == NULL)
		jot_error(N_("I can't find my home directory!  Wah!"));

	check_vitals_mapped();

	free(nanorc);
}

#endif /* ENABLE_NANORC */
