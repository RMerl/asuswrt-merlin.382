/**************************************************************************
 *   utils.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2019 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016-2017 Benno Schulenberg                            *
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

#include <errno.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <string.h>
#include <unistd.h>

/* Return the user's home directory.  We use $HOME, and if that fails,
 * we fall back on the home directory of the effective user ID. */
void get_homedir(void)
{
	if (homedir == NULL) {
		const char *homenv = getenv("HOME");

#ifdef HAVE_PWD_H
		/* When HOME isn't set, or when we're root, get the home directory
		 * from the password file instead. */
		if (homenv == NULL || geteuid() == 0) {
			const struct passwd *userage = getpwuid(geteuid());

			if (userage != NULL)
				homenv = userage->pw_dir;
		}
#endif

		/* Only set homedir if some home directory could be determined,
		 * otherwise keep homedir NULL. */
		if (homenv != NULL && *homenv != '\0')
			homedir = mallocstrcpy(NULL, homenv);
	}
}

/* Return the filename part of the given path. */
const char *tail(const char *path)
{
	const char *slash = strrchr(path, '/');

	if (slash == NULL)
		return path;
	else
		return ++slash;
}

/* Return a copy of the two given strings, welded together. */
char *concatenate(const char *path, const char *name)
{
	size_t pathlen = strlen(path);
	char *joined = charalloc(pathlen + strlen(name) + 1);

	strcpy(joined, path);
	strcpy(joined + pathlen, name);

	return joined;
}

#ifdef ENABLE_LINENUMBERS
/* Return the number of digits that the given integer n takes up. */
int digits(ssize_t n)
{
	if (n < 100000) {
		if (n < 1000) {
			if (n < 100)
				return 2;
			else
				return 3;
		} else {
			if (n < 10000)
				return 4;
			else
				return 5;
		}
	} else {
		if (n < 10000000) {
			if (n < 1000000)
				return 6;
			else
				return 7;
		} else {
			if (n < 100000000)
				return 8;
			else
				return 9;
		}
	}
}
#endif

/* Read an integer from str.  If it parses okay, store it in *result
 * and return TRUE; otherwise, return FALSE. */
bool parse_num(const char *str, ssize_t *result)
{
	char *first_error;
	ssize_t value;

	/* The manual page for strtol() says this is required. */
	errno = 0;

	value = (ssize_t)strtol(str, &first_error, 10);

	if (errno == ERANGE || *str == '\0' || *first_error != '\0')
		return FALSE;

	*result = value;

	return TRUE;
}

/* Read two numbers, separated by a comma, from str, and store them in
 * *line and *column.  Return FALSE on error, and TRUE otherwise. */
bool parse_line_column(const char *str, ssize_t *line, ssize_t *column)
{
	bool retval;
	char *firstpart;
	const char *comma;

	while (*str == ' ')
		str++;

	comma = strpbrk(str, "m,. /;");

	if (comma == NULL)
		return parse_num(str, line);

	retval = parse_num(comma + 1, column);

	if (comma == str)
		return retval;

	firstpart = mallocstrcpy(NULL, str);
	firstpart[comma - str] = '\0';

	retval = parse_num(firstpart, line) && retval;

	free(firstpart);

	return retval;
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so encode its nulls as newlines. */
void unsunder(char *str, size_t true_len)
{
	for (; true_len > 0; true_len--, str++) {
		if (*str == '\0')
			*str = '\n';
	}
}

/* For non-null-terminated lines.  A line, by definition, shouldn't
 * normally have newlines in it, so decode its newlines as nulls. */
void sunder(char *str)
{
	for (; *str != '\0'; str++) {
		if (*str == '\n')
			*str = '\0';
	}
}

#if !defined(ENABLE_TINY) || defined(ENABLE_TABCOMP) || defined(ENABLE_BROWSER)
/* Free the memory of the given array, which should contain len elements. */
void free_chararray(char **array, size_t len)
{
	if (array == NULL)
		return;

	while (len > 0)
		free(array[--len]);

	free(array);
}
#endif

#ifdef ENABLE_SPELLER
/* Is the word starting at the given position in buf and of the given length
 * a separate word?  That is: is it not part of a longer word?*/
bool is_separate_word(size_t position, size_t length, const char *buf)
{
	char before[MAXCHARLEN], after[MAXCHARLEN];
	size_t word_end = position + length;

	/* Get the characters before and after the word, if any. */
	parse_mbchar(buf + step_left(buf, position), before, NULL);
	parse_mbchar(buf + word_end, after, NULL);

	/* If the word starts at the beginning of the line OR the character before
	 * the word isn't a letter, and if the word ends at the end of the line OR
	 * the character after the word isn't a letter, we have a whole word. */
	return ((position == 0 || !is_alpha_mbchar(before)) &&
				(buf[word_end] == '\0' || !is_alpha_mbchar(after)));
}
#endif /* ENABLE_SPELLER */

/* Return the position of the needle in the haystack, or NULL if not found.
 * When searching backwards, we will find the last match that starts no later
 * than the given start; otherwise, we find the first match starting no earlier
 * than start.  If we are doing a regexp search, and we find a match, we fill
 * in the global variable regmatches with at most 9 subexpression matches. */
const char *strstrwrapper(const char *haystack, const char *needle,
		const char *start)
{
	if (ISSET(USE_REGEXP)) {
		if (ISSET(BACKWARDS_SEARCH)) {
			size_t last_find, ceiling, far_end;
			size_t floor = 0, next_rung = 0;
				/* The start of the search range, and the next start. */

			if (regexec(&search_regexp, haystack, 1, regmatches, 0) != 0)
				return NULL;

			far_end = strlen(haystack);
			ceiling = start - haystack;
			last_find = regmatches[0].rm_so;

			/* A result beyond the search range also means: no match. */
			if (last_find > ceiling)
				return NULL;

			/* Move the start-of-range forward until there is no more match;
			 * then the last match found is the first match backwards. */
			while (regmatches[0].rm_so <= ceiling) {
				floor = next_rung;
				last_find = regmatches[0].rm_so;
				/* If this is the last possible match, don't try to advance. */
				if (last_find == ceiling)
					break;
				next_rung = step_right(haystack, last_find);
				regmatches[0].rm_so = next_rung;
				regmatches[0].rm_eo = far_end;
				if (regexec(&search_regexp, haystack, 1, regmatches,
										REG_STARTEND) != 0)
					break;
			}

			/* Find the last match again, to get possible submatches. */
			regmatches[0].rm_so = floor;
			regmatches[0].rm_eo = far_end;
			if (regexec(&search_regexp, haystack, 10, regmatches,
										REG_STARTEND) != 0)
				return NULL;

			return haystack + regmatches[0].rm_so;
		}

		/* Do a forward regex search from the starting point. */
		regmatches[0].rm_so = start - haystack;
		regmatches[0].rm_eo = strlen(haystack);
		if (regexec(&search_regexp, haystack, 10, regmatches,
										REG_STARTEND) != 0)
			return NULL;
		else
			return haystack + regmatches[0].rm_so;
	}

	if (ISSET(CASE_SENSITIVE)) {
		if (ISSET(BACKWARDS_SEARCH))
			return revstrstr(haystack, needle, start);
		else
			return strstr(start, needle);
	}

	if (ISSET(BACKWARDS_SEARCH))
		return mbrevstrcasestr(haystack, needle, start);
	else
		return mbstrcasestr(start, needle);
}

/* This is a wrapper for the malloc() function that properly handles
 * things when we run out of memory. */
void *nmalloc(size_t howmuch)
{
	void *r = malloc(howmuch);

	if (r == NULL && howmuch != 0)
		die(_("Nano is out of memory!\n"));

	return r;
}

/* This is a wrapper for the realloc() function that properly handles
 * things when we run out of memory. */
void *nrealloc(void *ptr, size_t howmuch)
{
	void *r = realloc(ptr, howmuch);

	if (r == NULL && howmuch != 0)
		die(_("Nano is out of memory!\n"));

	return r;
}

/* Allocate and copy the first n characters of the given src string, after
 * freeing the destination.  Usage: "dest = mallocstrncpy(dest, src, n);". */
char *mallocstrncpy(char *dest, const char *src, size_t n)
{
	if (src == NULL)
		src = "";

	dest = charealloc(dest, n);
	strncpy(dest, src, n);

	return dest;
}

/* Free the dest string and return a malloc'ed copy of src.  Should be used as:
 * "dest = mallocstrcpy(dest, src);". */
char *mallocstrcpy(char *dest, const char *src)
{
	return mallocstrncpy(dest, src, (src == NULL) ? 1 : strlen(src) + 1);
}

/* Free the string at dest and return the string at src. */
char *free_and_assign(char *dest, char *src)
{
	free(dest);
	return src;
}

/* When not in softwrap mode, nano scrolls horizontally within a line in
 * chunks (a bit smaller than the chunks used in softwrapping).  Return the
 * column number of the first character displayed in the edit window when the
 * cursor is at the given column.  Note that (0 <= column -
 * get_page_start(column) < COLS). */
size_t get_page_start(size_t column)
{
	if (column + 2 < editwincols || ISSET(SOFTWRAP) || column == 0)
		return 0;
	else if (editwincols > 8)
		return column - 6 - (column - 6) % (editwincols - 8);
	else
		return column - (editwincols - 2);
}

/* Return the placewewant associated with current_x, i.e. the zero-based
 * column position of the cursor. */
size_t xplustabs(void)
{
	return wideness(openfile->current->data, openfile->current_x);
}

/* Return the index in text of the character that (when displayed) will
 * not overshoot the given column. */
size_t actual_x(const char *text, size_t column)
{
	const char *start = text;
		/* From where we start walking through the text. */
	size_t width = 0;
		/* The current accumulated span, in columns. */

	while (*text != '\0') {
		int charlen = parse_mbchar(text, NULL, &width);

		if (width > column)
			break;

		text += charlen;
	}

	return (text - start);
}

/* A strnlen() with tabs and multicolumn characters factored in:
 * how many columns wide are the first maxlen bytes of text? */
size_t wideness(const char *text, size_t maxlen)
{
	size_t width = 0;

	if (maxlen == 0)
		return 0;

	while (*text != '\0') {
		size_t charlen = parse_mbchar(text, NULL, &width);

		if (maxlen <= charlen)
			break;

		maxlen -= charlen;
		text += charlen;
	}

	return width;
}

/* Return the number of columns that the given text occupies. */
size_t breadth(const char *text)
{
	size_t span = 0;

	while (*text != '\0')
		text += parse_mbchar(text, NULL, &span);

	return span;
}

/* Append a new magic line to the end of the buffer. */
void new_magicline(void)
{
	openfile->filebot->next = make_new_node(openfile->filebot);
	openfile->filebot->next->data = mallocstrcpy(NULL, "");
	openfile->filebot = openfile->filebot->next;
	openfile->totsize++;
}

#if !defined(NANO_TINY) || defined(ENABLE_HELP)
/* Remove the magic line from the end of the buffer, if there is one and
 * it isn't the only line in the file. */
void remove_magicline(void)
{
	if (openfile->filebot->data[0] == '\0' &&
				openfile->filebot != openfile->filetop) {
		openfile->filebot = openfile->filebot->prev;
		delete_node(openfile->filebot->next);
		openfile->filebot->next = NULL;
		openfile->totsize--;
	}
}
#endif

#ifndef NANO_TINY
/* Return in (top, top_x) and (bot, bot_x) the start and end "coordinates"
 * of the marked region.  If right_side_up isn't NULL, set it to TRUE when
 * the mark is at the top of the marked region, and to FALSE otherwise. */
void get_region(const linestruct **top, size_t *top_x,
				const linestruct **bot, size_t *bot_x, bool *right_side_up)
{
	if (openfile->mark->lineno < openfile->current->lineno ||
				(openfile->mark == openfile->current &&
				openfile->mark_x < openfile->current_x)) {
		*top = openfile->mark;
		*top_x = openfile->mark_x;
		*bot = openfile->current;
		*bot_x = openfile->current_x;
		if (right_side_up != NULL)
			*right_side_up = TRUE;
	} else {
		*bot = openfile->mark;
		*bot_x = openfile->mark_x;
		*top = openfile->current;
		*top_x = openfile->current_x;
		if (right_side_up != NULL)
			*right_side_up = FALSE;
	}
}

/* Get the set of lines to work on -- either just the current line, or the
 * first to last lines of the marked region.  When the cursor (or mark) is
 * at the start of the last line of the region, exclude that line. */
void get_range(const linestruct **top, const linestruct **bot)
{
	if (!openfile->mark) {
		*top = openfile->current;
		*bot = openfile->current;
	} else {
		size_t top_x, bot_x;

		get_region(top, &top_x, bot, &bot_x, NULL);

		if (bot_x == 0 && *bot != *top && !also_the_last)
			*bot = (*bot)->prev;
		else
			also_the_last = TRUE;
	}
}

/* Return a pointer to the line that has the given line number. */
linestruct *line_from_number(ssize_t number)
{
	linestruct *line = openfile->current;

	if (line->lineno > number)
		while (line->lineno != number)
			line = line->prev;
	else
		while (line->lineno != number)
			line = line->next;

	return line;
}
#endif /* !NANO_TINY */

/* Count the number of characters from begin to end, and return it. */
size_t get_totsize(const linestruct *begin, const linestruct *end)
{
	const linestruct *line;
	size_t totsize = 0;

	/* Sum the number of characters (plus a newline) in each line. */
	for (line = begin; line != end->next; line = line->next)
		totsize += mbstrlen(line->data) + 1;

	/* The last line of a file doesn't have a newline -- otherwise it
	 * wouldn't be the last line -- so subtract 1 when at EOF. */
	if (line == NULL)
		totsize--;

	return totsize;
}
