/**************************************************************************
 *   text.c  --  This file is part of GNU nano.                           *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2020 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014-2015 Mark Majeres                                 *
 *   Copyright (C) 2016 Mike Scalora                                      *
 *   Copyright (C) 2016 Sumedh Pendurkar                                  *
 *   Copyright (C) 2018 Marco Diego Aurélio Mesquita                      *
 *   Copyright (C) 2015-2019 Benno Schulenberg                            *
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
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#if defined(__APPLE__) && !defined(st_mtim)
#define st_mtim  st_mtimespec
#endif

#ifndef NANO_TINY
static pid_t pid_of_command = -1;
		/* The PID of the forked process -- needed when wanting to abort it. */
#endif
#ifdef ENABLE_WORDCOMPLETION
static int pletion_x = 0;
		/* The x position in pletion_line of the last found completion. */
static completion_word *list_of_completions;
		/* A linked list of the completions that have been attempted. */
#endif

#ifndef NANO_TINY
/* Toggle the mark. */
void do_mark(void)
{
	if (!openfile->mark) {
		openfile->mark = openfile->current;
		openfile->mark_x = openfile->current_x;
		statusbar(_("Mark Set"));
		openfile->kind_of_mark = HARDMARK;
	} else {
		openfile->mark = NULL;
		statusbar(_("Mark Unset"));
		refresh_needed = TRUE;
	}
}
#endif /* !NANO_TINY */

/* Insert a tab.  If the TABS_TO_SPACES flag is set, insert the number
 * of spaces that a tab would normally take up. */
void do_tab(void)
{
#ifdef ENABLE_COLOR
	if (openfile->syntax && openfile->syntax->tab)
		inject(openfile->syntax->tab, strlen(openfile->syntax->tab), FALSE);
	else
#endif
#ifndef NANO_TINY
	if (ISSET(TABS_TO_SPACES)) {
		char *spaces = charalloc(tabsize + 1);
		size_t length = tabsize - (xplustabs() % tabsize);

		memset(spaces, ' ', length);
		spaces[length] = '\0';

		inject(spaces, length, FALSE);

		free(spaces);
	} else
#endif
		inject((char *)"\t", 1, FALSE);
}

#ifndef NANO_TINY
/* Add an indent to the given line. */
void indent_a_line(linestruct *line, char *indentation)
{
	size_t length = strlen(line->data);
	size_t indent_len = strlen(indentation);

	/* If the requested indentation is empty, don't change the line. */
	if (indent_len == 0)
		return;

	/* Add the fabricated indentation to the beginning of the line. */
	line->data = charealloc(line->data, length + indent_len + 1);
	memmove(line->data + indent_len, line->data, length + 1);
	memcpy(line->data, indentation, indent_len);

	openfile->totsize += indent_len;

	/* Compensate for the change in the current line. */
	if (line == openfile->mark && openfile->mark_x > 0)
		openfile->mark_x += indent_len;
	if (line == openfile->current && openfile->current_x > 0) {
		openfile->current_x += indent_len;
		openfile->placewewant = xplustabs();
	}
}

/* Indent the current line (or the marked lines) by tabsize columns.
 * This inserts either a tab character or a tab's worth of spaces,
 * depending on whether --tabstospaces is in effect. */
void do_indent(void)
{
	char *indentation;
	linestruct *top, *bot, *line;

	/* Use either all the marked lines or just the current line. */
	get_range((const linestruct **)&top, (const linestruct **)&bot);

	/* Skip any leading empty lines. */
	while (top != bot->next && top->data[0] == '\0')
		top = top->next;

	/* If all lines are empty, there is nothing to do. */
	if (top == bot->next)
		return;

	indentation = charalloc(tabsize + 1);

	/* Set the indentation to either a bunch of spaces or a single tab. */
#ifdef ENABLE_COLOR
	if (openfile->syntax && openfile->syntax->tab)
		indentation = mallocstrcpy(indentation, openfile->syntax->tab);
	else
#endif
	if (ISSET(TABS_TO_SPACES)) {
		memset(indentation, ' ', tabsize);
		indentation[tabsize] = '\0';
	} else {
		indentation[0] = '\t';
		indentation[1] = '\0';
	}

	add_undo(INDENT, NULL);

	/* Go through each of the lines, adding an indent to the non-empty ones,
	 * and recording whatever was added in the undo item. */
	for (line = top; line != bot->next; line = line->next) {
		char *real_indent = (line->data[0] == '\0') ? "" : indentation;

		indent_a_line(line, real_indent);
		update_multiline_undo(line->lineno, real_indent);
	}

	free(indentation);

	set_modified();
	ensure_firstcolumn_is_aligned();
	refresh_needed = TRUE;
	shift_held = TRUE;
}

/* Return the number of bytes of whitespace at the start of the given text,
 * but at most a tab's worth. */
size_t length_of_white(const char *text)
{
	size_t white_count = 0;

#ifdef ENABLE_COLOR
	if (openfile->syntax && openfile->syntax->tab) {
		size_t thelength = strlen(openfile->syntax->tab);

		while (text[white_count] == openfile->syntax->tab[white_count])
			if (++white_count == thelength)
				return thelength;

		white_count = 0;
	}
#endif

	while (TRUE) {
		if (*text == '\t')
			return ++white_count;

		if (*text != ' ')
			return white_count;

		if (++white_count == tabsize)
			return tabsize;

		text++;
	}
}

/* Adjust the positions of mark and cursor when they are on the given line. */
void compensate_leftward(linestruct *line, size_t leftshift)
{
	if (line == openfile->mark) {
		if (openfile->mark_x < leftshift)
			openfile->mark_x = 0;
		else
			openfile->mark_x -= leftshift;
	}

	if (line == openfile->current) {
		if (openfile->current_x < leftshift)
			openfile->current_x = 0;
		else
			openfile->current_x -= leftshift;
		openfile->placewewant = xplustabs();
	}
}

/* Remove an indent from the given line. */
void unindent_a_line(linestruct *line, size_t indent_len)
{
	size_t length = strlen(line->data);

	/* If the indent is empty, don't change the line. */
	if (indent_len == 0)
		return;

	/* Remove the first tab's worth of whitespace from this line. */
	memmove(line->data, line->data + indent_len, length - indent_len + 1);

	openfile->totsize -= indent_len;

	/* Adjust the positions of mark and cursor, when they are affected. */
	compensate_leftward(line, indent_len);
}

/* Unindent the current line (or the marked lines) by tabsize columns.
 * The removed indent can be a mixture of spaces plus at most one tab. */
void do_unindent(void)
{
	linestruct *top, *bot, *line;

	/* Use either all the marked lines or just the current line. */
	get_range((const linestruct **)&top, (const linestruct **)&bot);

	/* Skip any leading lines that cannot be unindented. */
	while (top != bot->next && length_of_white(top->data) == 0)
		top = top->next;

	/* If none of the lines can be unindented, there is nothing to do. */
	if (top == bot->next)
		return;

	add_undo(UNINDENT, NULL);

	/* Go through each of the lines, removing their leading indent where
	 * possible, and saving the removed whitespace in the undo item. */
	for (line = top; line != bot->next; line = line->next) {
		size_t indent_len = length_of_white(line->data);
		char *indentation = measured_copy(line->data, indent_len + 1);

		indentation[indent_len] = '\0';

		unindent_a_line(line, indent_len);
		update_multiline_undo(line->lineno, indentation);

		free(indentation);
	}

	set_modified();
	ensure_firstcolumn_is_aligned();
	refresh_needed = TRUE;
	shift_held = TRUE;
}

/* Perform an undo or redo for an indent or unindent action. */
void handle_indent_action(undostruct *u, bool undoing, bool add_indent)
{
	groupstruct *group = u->grouping;
	linestruct *line = line_from_number(group->top_line);

	if (group->next != NULL)
		statusline(ALERT, "Multiple groups -- please report a bug");

	/* When redoing, reposition the cursor and let the indenter adjust it. */
	if (!undoing)
		goto_line_posx(u->lineno, u->begin);

	/* For each line in the group, add or remove the individual indent. */
	while (line && line->lineno <= group->bottom_line) {
		char *blanks = group->indentations[line->lineno - group->top_line];

		if (undoing ^ add_indent)
			indent_a_line(line, blanks);
		else
			unindent_a_line(line, strlen(blanks));

		line = line->next;
	}

	/* When undoing, reposition the cursor to the recorded location. */
	if (undoing)
		goto_line_posx(u->lineno, u->begin);

	refresh_needed = TRUE;
}
#endif /* !NANO_TINY */

#ifdef ENABLE_COMMENT
/* Test whether the given line can be uncommented, or add or remove a comment,
 * depending on action.  Return TRUE if the line is uncommentable, or when
 * anything was added or removed; FALSE otherwise. */
bool comment_line(undo_type action, linestruct *line, const char *comment_seq)
{
	size_t comment_seq_len = strlen(comment_seq);
	const char *post_seq = strchr(comment_seq, '|');
		/* The postfix, if this is a bracketing type comment sequence. */
	size_t pre_len = post_seq ? post_seq++ - comment_seq : comment_seq_len;
		/* Length of prefix. */
	size_t post_len = post_seq ? comment_seq_len - pre_len - 1 : 0;
		/* Length of postfix. */
	size_t line_len = strlen(line->data);

	if (!ISSET(NO_NEWLINES) && line == openfile->filebot)
		return FALSE;

	if (action == COMMENT) {
		/* Make room for the comment sequence(s), move the text right and
		 * copy them in. */
		line->data = charealloc(line->data, line_len + pre_len + post_len + 1);
		memmove(line->data + pre_len, line->data, line_len + 1);
		memmove(line->data, comment_seq, pre_len);
		if (post_len > 0)
			memmove(line->data + pre_len + line_len, post_seq, post_len + 1);

		openfile->totsize += pre_len + post_len;

		/* If needed, adjust the position of the mark and of the cursor. */
		if (line == openfile->mark && openfile->mark_x > 0)
			openfile->mark_x += pre_len;
		if (line == openfile->current && openfile->current_x > 0) {
			openfile->current_x += pre_len;
			openfile->placewewant = xplustabs();
		}

		return TRUE;
	}

	/* If the line is commented, report it as uncommentable, or uncomment it. */
	if (strncmp(line->data, comment_seq, pre_len) == 0 && (post_len == 0 ||
				strcmp(line->data + line_len - post_len, post_seq) == 0)) {

		if (action == PREFLIGHT)
			return TRUE;

		/* Erase the comment prefix by moving the non-comment part. */
		memmove(line->data, line->data + pre_len, line_len - pre_len);
		/* Truncate the postfix if there was one. */
		line->data[line_len - pre_len - post_len] = '\0';

		openfile->totsize -= pre_len + post_len;

		/* Adjust the positions of mark and cursor, when needed. */
		compensate_leftward(line, pre_len);

		return TRUE;
	}

	return FALSE;
}

/* Comment or uncomment the current line or the marked lines. */
void do_comment(void)
{
	const char *comment_seq = GENERAL_COMMENT_CHARACTER;
	undo_type action = UNCOMMENT;
	linestruct *top, *bot, *line;
	bool empty, all_empty = TRUE;

#ifdef ENABLE_COLOR
	if (openfile->syntax)
		comment_seq = openfile->syntax->comment;

	if (*comment_seq == '\0') {
		statusbar(_("Commenting is not supported for this file type"));
		return;
	}
#endif

	/* Determine which lines to work on. */
	get_range((const linestruct **)&top, (const linestruct **)&bot);

	/* If only the magic line is selected, don't do anything. */
	if (top == bot && bot == openfile->filebot && !ISSET(NO_NEWLINES)) {
		statusbar(_("Cannot comment past end of file"));
		return;
	}

	/* Figure out whether to comment or uncomment the selected line or lines. */
	for (line = top; line != bot->next; line = line->next) {
		empty = white_string(line->data);

		/* If this line is not blank and not commented, we comment all. */
		if (!empty && !comment_line(PREFLIGHT, line, comment_seq)) {
			action = COMMENT;
			break;
		}
		all_empty = all_empty && empty;
	}

	/* If all selected lines are blank, we comment them. */
	action = all_empty ? COMMENT : action;

	add_undo(action, NULL);

	/* Store the comment sequence used for the operation, because it could
	 * change when the file name changes; we need to know what it was. */
	openfile->current_undo->strdata = copy_of(comment_seq);

	/* Comment/uncomment each of the selected lines when possible, and
	 * store undo data when a line changed. */
	for (line = top; line != bot->next; line = line->next) {
		if (comment_line(action, line, comment_seq))
			update_multiline_undo(line->lineno, "");
	}

	set_modified();
	ensure_firstcolumn_is_aligned();
	refresh_needed = TRUE;
	shift_held = TRUE;
}

/* Perform an undo or redo for a comment or uncomment action. */
void handle_comment_action(undostruct *u, bool undoing, bool add_comment)
{
	groupstruct *group = u->grouping;

	/* When redoing, reposition the cursor and let the commenter adjust it. */
	if (!undoing)
		goto_line_posx(u->lineno, u->begin);

	while (group) {
		linestruct *f = line_from_number(group->top_line);

		while (f && f->lineno <= group->bottom_line) {
			comment_line(undoing ^ add_comment ?
								COMMENT : UNCOMMENT, f, u->strdata);
			f = f->next;
		}

		group = group->next;
	}

	/* When undoing, reposition the cursor to the recorded location. */
	if (undoing)
		goto_line_posx(u->lineno, u->begin);

	refresh_needed = TRUE;
}
#endif /* ENABLE_COMMENT */

#ifndef NANO_TINY
#define redo_paste undo_cut
#define undo_paste redo_cut

/* Undo a cut, or redo a paste. */
void undo_cut(undostruct *u)
{
	if (u->xflags & WAS_WHOLE_LINE)
		goto_line_posx(u->mark_begin_lineno, 0);
	else
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);

	/* If nothing was actually cut, positioning the cursor was enough. */
	if (!u->cutbuffer)
		return;

	copy_from_buffer(u->cutbuffer);

	/* If the final line was originally cut, remove the extra magic line. */
	if ((u->xflags & WAS_FINAL_LINE) && !ISSET(NO_NEWLINES) &&
			openfile->current != openfile->filebot)
		remove_magicline();

	if (!(u->xflags & WAS_MARKED_FORWARD) && u->type != PASTE)
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
}

/* Redo a cut, or undo a paste. */
void redo_cut(undostruct *u)
{
	linestruct *oldcutbuffer = cutbuffer;

	goto_line_posx(u->lineno, u->begin);

	/* If nothing was actually cut, positioning the cursor was enough. */
	if (!u->cutbuffer)
		return;

	cutbuffer = NULL;

	openfile->mark = line_from_number(u->mark_begin_lineno);
	openfile->mark_x = (u->xflags & WAS_WHOLE_LINE) ? 0 : u->mark_begin_x;

	do_snip(FALSE, TRUE, FALSE, u->type == ZAP);

	free_lines(cutbuffer);
	cutbuffer = oldcutbuffer;
}

/* Undo the last thing(s) we did. */
void do_undo(void)
{
	undostruct *u = openfile->current_undo;
	linestruct *f = NULL, *t = NULL;
	linestruct *oldcutbuffer;
	char *data, *undidmsg = NULL;
	size_t from_x, to_x;

	if (u == NULL) {
		statusbar(_("Nothing to undo"));
		return;
	}

	if (u->type <= REPLACE)
		f = line_from_number(u->mark_begin_lineno);

	openfile->current_x = u->begin;

	switch (u->type) {
	case ADD:
		/* TRANSLATORS: The next thirteen strings describe actions
		 * that are undone or redone.  They are all nouns, not verbs. */
		undidmsg = _("addition");
		if ((u->xflags & WAS_FINAL_LINE) && !ISSET(NO_NEWLINES))
			remove_magicline();
		data = charalloc(strlen(f->data) - strlen(u->strdata) + 1);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
		free(f->data);
		f->data = data;
		goto_line_posx(u->lineno, u->begin);
		break;
	case ENTER:
		undidmsg = _("line break");
		from_x = (u->begin == 0) ? 0 : u->mark_begin_x;
		to_x = (u->begin == 0) ? u->mark_begin_x : u->begin;
		f->data = charealloc(f->data, strlen(f->data) +
								strlen(&u->strdata[from_x]) + 1);
		strcat(f->data, &u->strdata[from_x]);
		unlink_node(f->next);
		renumber_from(f);
		goto_line_posx(u->lineno, to_x);
		break;
	case BACK:
	case DEL:
		undidmsg = _("deletion");
		data = charalloc(strlen(f->data) + strlen(u->strdata) + 1);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], u->strdata);
		strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
		free(f->data);
		f->data = data;
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
		break;
	case JOIN:
		undidmsg = _("line join");
		/* When the join was done by a Backspace at the tail of the file,
		 * and the nonewlines flag isn't set, do not re-add a newline that
		 * wasn't actually deleted; just position the cursor. */
		if ((u->xflags & WAS_FINAL_BACKSPACE) && !ISSET(NO_NEWLINES)) {
			goto_line_posx(openfile->filebot->lineno, 0);
			break;
		}
		t = make_new_node(f);
		t->data = copy_of(u->strdata);
		data = measured_copy(f->data, u->mark_begin_x + 1);
		data[u->mark_begin_x] = '\0';
		free(f->data);
		f->data = data;
		splice_node(f, t);
		renumber_from(t);
		goto_line_posx(u->lineno, u->begin);
		break;
	case REPLACE:
		undidmsg = _("replacement");
		goto_line_posx(u->lineno, u->begin);
		data = u->strdata;
		u->strdata = f->data;
		f->data = data;
		break;
#ifdef ENABLE_WRAPPING
	case SPLIT_END:
		goto_line_posx(u->lineno, u->begin);
		openfile->current_undo = openfile->current_undo->next;
		while (openfile->current_undo->type != SPLIT_BEGIN)
			do_undo();
		u = openfile->current_undo;
		/* Fall-through. */
	case SPLIT_BEGIN:
		undidmsg = _("addition");
		break;
#endif
	case ZAP:
		undidmsg = _("erasure");
		undo_cut(u);
		break;
	case CUT_TO_EOF:
	case CUT:
		/* TRANSLATORS: Remember: these are nouns, NOT verbs. */
		undidmsg = _("cut");
		undo_cut(u);
		break;
	case PASTE:
		undidmsg = _("paste");
		undo_paste(u);
		break;
	case INSERT:
		undidmsg = _("insertion");
		oldcutbuffer = cutbuffer;
		cutbuffer = NULL;
		openfile->mark = line_from_number(u->mark_begin_lineno);
		openfile->mark_x = u->mark_begin_x;
		goto_line_posx(u->lineno, u->begin);
		cut_marked(NULL);
		u->cutbuffer = cutbuffer;
		cutbuffer = oldcutbuffer;
		break;
	case COUPLE_BEGIN:
		undidmsg = u->strdata;
		goto_line_posx(u->lineno, u->begin);
		openfile->current_y = u->mark_begin_lineno;
		adjust_viewport(STATIONARY);
		break;
	case COUPLE_END:
		openfile->current_undo = openfile->current_undo->next;
		do_undo();
		do_undo();
		do_undo();
		return;
	case INDENT:
		handle_indent_action(u, TRUE, TRUE);
		undidmsg = _("indent");
		break;
	case UNINDENT:
		handle_indent_action(u, TRUE, FALSE);
		undidmsg = _("unindent");
		break;
#ifdef ENABLE_COMMENT
	case COMMENT:
		handle_comment_action(u, TRUE, TRUE);
		undidmsg = _("comment");
		break;
	case UNCOMMENT:
		handle_comment_action(u, TRUE, FALSE);
		undidmsg = _("uncomment");
		break;
#endif
	default:
		break;
	}

	if (undidmsg && !pletion_line)
		statusline(HUSH, _("Undid %s"), undidmsg);

	openfile->current_undo = openfile->current_undo->next;
	openfile->last_action = OTHER;
	openfile->mark = NULL;
	openfile->placewewant = xplustabs();

	openfile->totsize = u->wassize;

	/* When at the point where the file was last saved, unset "Modified". */
	if (openfile->current_undo == openfile->last_saved) {
		openfile->modified = FALSE;
		titlebar(NULL);
	} else
		set_modified();
}

/* Redo the last thing(s) we undid. */
void do_redo(void)
{
	linestruct *f = NULL, *shoveline;
	char *data, *redidmsg = NULL;
	undostruct *u = openfile->undotop;

	if (u == NULL || u == openfile->current_undo) {
		statusbar(_("Nothing to redo"));
		return;
	}

	/* Find the item before the current one in the undo stack. */
	while (u->next != openfile->current_undo)
		u = u->next;

	if (u->type <= REPLACE)
		f = line_from_number(u->mark_begin_lineno);

	switch (u->type) {
	case ADD:
		redidmsg = _("addition");
		if ((u->xflags & WAS_FINAL_LINE) && !ISSET(NO_NEWLINES))
			new_magicline();
		data = charalloc(strlen(f->data) + strlen(u->strdata) + 1);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], u->strdata);
		strcpy(&data[u->begin + strlen(u->strdata)], &f->data[u->begin]);
		free(f->data);
		f->data = data;
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
		break;
	case ENTER:
		redidmsg = _("line break");
		shoveline = make_new_node(f);
		shoveline->data = copy_of(u->strdata);
		data = measured_copy(f->data, u->begin + 1);
		data[u->begin] = '\0';
		free(f->data);
		f->data = data;
		splice_node(f, shoveline);
		renumber_from(shoveline);
		goto_line_posx(u->lineno + 1, u->mark_begin_x);
		break;
	case BACK:
	case DEL:
		redidmsg = _("deletion");
		data = charalloc(strlen(f->data) + strlen(u->strdata) + 1);
		strncpy(data, f->data, u->begin);
		strcpy(&data[u->begin], &f->data[u->begin + strlen(u->strdata)]);
		free(f->data);
		f->data = data;
		goto_line_posx(u->lineno, u->begin);
		break;
	case JOIN:
		redidmsg = _("line join");
		/* When the join was done by a Backspace at the tail of the file,
		 * and the nonewlines flag isn't set, do not join anything, as
		 * nothing was actually deleted; just position the cursor. */
		if ((u->xflags & WAS_FINAL_BACKSPACE) && !ISSET(NO_NEWLINES)) {
			goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
			break;
		}
		f->data = charealloc(f->data, strlen(f->data) + strlen(u->strdata) + 1);
		strcat(f->data, u->strdata);
		unlink_node(f->next);
		renumber_from(f);
		goto_line_posx(u->mark_begin_lineno, u->mark_begin_x);
		break;
	case REPLACE:
		redidmsg = _("replacement");
		data = u->strdata;
		u->strdata = f->data;
		f->data = data;
		goto_line_posx(u->lineno, u->begin);
		break;
#ifdef ENABLE_WRAPPING
	case SPLIT_BEGIN:
		goto_line_posx(u->lineno, u->begin);
		openfile->current_undo = u;
		while (openfile->current_undo->type != SPLIT_END)
			do_redo();
		u = openfile->current_undo;
		goto_line_posx(u->lineno, u->begin);
		/* Fall-through. */
	case SPLIT_END:
		redidmsg = _("addition");
		break;
#endif
	case ZAP:
		redidmsg = _("erasure");
		redo_cut(u);
		break;
	case CUT_TO_EOF:
	case CUT:
		redidmsg = _("cut");
		redo_cut(u);
		break;
	case PASTE:
		redidmsg = _("paste");
		redo_paste(u);
		break;
	case INSERT:
		redidmsg = _("insertion");
		goto_line_posx(u->lineno, u->begin);
		copy_from_buffer(u->cutbuffer);
		free_lines(u->cutbuffer);
		u->cutbuffer = NULL;
		break;
	case COUPLE_BEGIN:
		openfile->current_undo = u;
		do_redo();
		do_redo();
		do_redo();
		return;
	case COUPLE_END:
		redidmsg = u->strdata;
		goto_line_posx(u->lineno, u->begin);
		adjust_viewport(STATIONARY);
		break;
	case INDENT:
		handle_indent_action(u, FALSE, TRUE);
		redidmsg = _("indent");
		break;
	case UNINDENT:
		handle_indent_action(u, FALSE, FALSE);
		redidmsg = _("unindent");
		break;
#ifdef ENABLE_COMMENT
	case COMMENT:
		handle_comment_action(u, FALSE, TRUE);
		redidmsg = _("comment");
		break;
	case UNCOMMENT:
		handle_comment_action(u, FALSE, FALSE);
		redidmsg = _("uncomment");
		break;
#endif
	default:
		break;
	}

	if (redidmsg)
		statusline(HUSH, _("Redid %s"), redidmsg);

	openfile->current_undo = u;
	openfile->last_action = OTHER;
	openfile->mark = NULL;
	openfile->placewewant = xplustabs();

	openfile->totsize = u->newsize;

	/* When at the point where the file was last saved, unset "Modified". */
	if (openfile->current_undo == openfile->last_saved) {
		openfile->modified = FALSE;
		titlebar(NULL);
	} else
		set_modified();
}
#endif /* !NANO_TINY */

/* Break the current line at the cursor position. */
void do_enter(void)
{
	linestruct *newnode = make_new_node(openfile->current);
	size_t extra = 0;
#ifndef NANO_TINY
	linestruct *sampleline = openfile->current;
	bool allblanks = FALSE;

	if (ISSET(AUTOINDENT)) {
#ifdef ENABLE_JUSTIFY
		/* When doing automatic long-line wrapping and the next line is
		 * in this same paragraph, use its indentation as the model. */
		if (ISSET(BREAK_LONG_LINES) && sampleline->next != NULL &&
					inpar(sampleline->next) && !begpar(sampleline->next, 0))
			sampleline = sampleline->next;
#endif
		extra = indent_length(sampleline->data);

		/* When breaking in the indentation, limit the automatic one. */
		if (extra > openfile->current_x)
			extra = openfile->current_x;
		else if (extra == openfile->current_x)
			allblanks = TRUE;
	}
#endif /* NANO_TINY */
	newnode->data = charalloc(strlen(openfile->current->data +
										openfile->current_x) + extra + 1);
	strcpy(&newnode->data[extra], openfile->current->data +
										openfile->current_x);
#ifndef NANO_TINY
	if (ISSET(AUTOINDENT)) {
		/* Copy the whitespace from the sample line to the new one. */
		strncpy(newnode->data, sampleline->data, extra);
		/* If there were only blanks before the cursor, trim them. */
		if (allblanks)
			openfile->current_x = 0;
	}
#endif

	/* Make the current line end at the cursor position. */
	openfile->current->data[openfile->current_x] = '\0';

#ifndef NANO_TINY
	add_undo(ENTER, NULL);

	/* Adjust the mark if it was on the current line after the cursor. */
	if (openfile->mark == openfile->current &&
				openfile->mark_x > openfile->current_x) {
		openfile->mark = newnode;
		openfile->mark_x += extra - openfile->current_x;
	}
#endif

	/* Insert the newly created line after the current one and renumber. */
	splice_node(openfile->current, newnode);
	renumber_from(newnode);

	/* Put the cursor on the new line, after any automatic whitespace. */
	openfile->current = newnode;
	openfile->current_x = extra;
	openfile->placewewant = xplustabs();

	openfile->totsize++;
	set_modified();

#ifndef NANO_TINY
	if (ISSET(AUTOINDENT) && !allblanks)
		openfile->totsize += extra;
	update_undo(ENTER);
#endif

	refresh_needed = TRUE;
	focusing = FALSE;
}

#ifndef NANO_TINY
/* Send an unconditional kill signal to the running external command. */
RETSIGTYPE cancel_the_command(int signal)
{
	kill(pid_of_command, SIGKILL);
}

/* Send the text that starts at the given line to file descriptor fd. */
void send_data(const linestruct *line, int fd)
{
	FILE *tube = fdopen(fd, "w");

	if (tube == NULL)
		return;

	/* Send each line, except a final empty line. */
	while (line != NULL && (line->next != NULL || line->data[0] != '\0')) {
		fprintf(tube, "%s%s", line->data, line->next == NULL ? "" : "\n");
		line = line->next;
	}

	fclose(tube);
}

/* Execute the given command in a shell.  Return TRUE on success. */
bool execute_command(const char *command)
{
	int from_fd[2], to_fd[2];
		/* The pipes through which text will be written and read. */
	const bool should_pipe = (command[0] == '|');
	FILE *stream;
	struct sigaction oldaction, newaction;
		/* Original and temporary handlers for SIGINT. */

	/* Create a pipe to read the command's output from, and, if needed,
	 * a pipe to feed the command's input through. */
	if (pipe(from_fd) == -1 || (should_pipe && pipe(to_fd) == -1)) {
		statusline(ALERT, _("Could not create pipe"));
		return FALSE;
	}

	/* Fork a child process to run the command in. */
	if ((pid_of_command = fork()) == 0) {
		const char *theshell = getenv("SHELL");

		if (theshell == NULL)
			theshell = (char *)"/bin/sh";

		/* Child: close the unused read end of the output pipe. */
		close(from_fd[0]);

		/* Connect the write end of the output pipe to the process' output streams. */
		dup2(from_fd[1], fileno(stdout));
		dup2(from_fd[1], fileno(stderr));

		/* If the parent sends text, connect the read end of the
		 * feeding pipe to the child's input stream. */
		if (should_pipe) {
			dup2(to_fd[0], fileno(stdin));
			close(to_fd[1]);
		}

		/* Run the given command inside the preferred shell. */
		execl(theshell, tail(theshell), "-c", should_pipe ? &command[1] : command, NULL);

		/* If the exec call returns, there was an error. */
		exit(1);
	}

	/* Parent: close the unused write end of the pipe. */
	close(from_fd[1]);

	if (pid_of_command == -1) {
		statusline(ALERT, _("Could not fork"));
		close(from_fd[0]);
		return FALSE;
	}

	statusbar(_("Executing..."));

	/* If the command starts with "|", pipe buffer or region to the command. */
	if (should_pipe) {
		linestruct *was_cutbuffer = cutbuffer;
		cutbuffer = NULL;

#ifdef ENABLE_MULTIBUFFER
		if (ISSET(MULTIBUFFER)) {
			openfile = openfile->prev;
			if (openfile->mark)
				do_snip(TRUE, TRUE, FALSE, FALSE);
		} else
#endif
		{
			add_undo(COUPLE_BEGIN, "filtering");
			if (openfile->mark == NULL) {
				openfile->current = openfile->filetop;
				openfile->current_x = 0;
			}
			add_undo(CUT, NULL);
			do_snip(FALSE, openfile->mark != NULL, openfile->mark == NULL, FALSE);
			update_undo(CUT);
		}

		if (fork() == 0) {
			close(to_fd[0]);
			send_data(cutbuffer != NULL ? cutbuffer : openfile->filetop, to_fd[1]);
			close(to_fd[1]);
			exit(0);
		}

		close(to_fd[0]);
		close(to_fd[1]);

#ifdef ENABLE_MULTIBUFFER
		if (ISSET(MULTIBUFFER))
			openfile = openfile->next;
#endif
		free_lines(cutbuffer);
		cutbuffer = was_cutbuffer;
	}

	/* Re-enable interpretation of the special control keys so that we get
	 * SIGINT when Ctrl-C is pressed. */
	enable_kb_interrupt();

	/* Set up a signal handler so that ^C will terminate the forked process. */
	newaction.sa_handler = cancel_the_command;
	newaction.sa_flags = 0;
	sigaction(SIGINT, &newaction, &oldaction);

	stream = fdopen(from_fd[0], "rb");
	if (stream == NULL)
		statusline(ALERT, _("Failed to open pipe: %s"), strerror(errno));
	else
		read_file(stream, 0, "pipe", TRUE);

	if (should_pipe && !ISSET(MULTIBUFFER)) {
		/* TRANSLATORS: The next two go with Undid/Redid messages. */
		add_undo(COUPLE_END, N_("filtering"));
	}

	/* Wait for the external command (and possibly data sender) to terminate. */
	wait(NULL);
	if (should_pipe)
		wait(NULL);

	/* Restore the original handler for SIGINT. */
	sigaction(SIGINT, &oldaction, NULL);

	/* Restore the terminal to its desired state, and disable
	 * interpretation of the special control keys again. */
	terminal_init();

	return TRUE;
}

/* Discard undo items that are newer than the given one, or all if NULL.
 * When keep is TRUE, do not touch the last_saved pointer. */
void discard_until(const undostruct *thisitem, openfilestruct *thefile, bool keep)
{
	undostruct *dropit = thefile->undotop;
	groupstruct *group;

	while (dropit != NULL && dropit != thisitem) {
		thefile->undotop = dropit->next;
		free(dropit->strdata);
		free_lines(dropit->cutbuffer);
		group = dropit->grouping;
		while (group != NULL) {
			groupstruct *next = group->next;
			free_chararray(group->indentations,
								group->bottom_line - group->top_line);
			free(group);
			group = next;
		}
		free(dropit);
		dropit = thefile->undotop;
	}

	/* Adjust the pointer to the top of the undo stack. */
	thefile->current_undo = (undostruct *)thisitem;

	/* Prevent a chain of editing actions from continuing. */
	thefile->last_action = OTHER;

	/* When requested, record that the undo stack was chopped, and
	 * that thus there is no point at which the file was last saved. */
	if (!keep)
		thefile->last_saved = (undostruct *)0xbeeb;
}

/* Add a new undo item of the given type to the top of the current pile. */
void add_undo(undo_type action, const char *message)
{
	undostruct *u = nmalloc(sizeof(undostruct));

	/* Initialize the newly allocated undo item. */
	u->type = action;
	u->strdata = NULL;
	u->cutbuffer = NULL;
	u->lineno = openfile->current->lineno;
	u->begin = openfile->current_x;
	u->mark_begin_lineno = openfile->current->lineno;
	u->mark_begin_x = openfile->current_x;
	u->wassize = openfile->totsize;
	u->newsize = openfile->totsize;
	u->grouping = NULL;
	u->xflags = 0;

	/* Blow away any undone items. */
	discard_until(openfile->current_undo, openfile, TRUE);

#ifdef ENABLE_WRAPPING
	/* If some action caused automatic long-line wrapping, insert the
	 * SPLIT_BEGIN item underneath that action's undo item.  Otherwise,
	 * just add the new item to the top of the undo stack. */
	if (u->type == SPLIT_BEGIN) {
		u->next = openfile->undotop->next;
		openfile->undotop->next = u;
	} else
#endif
	{
		u->next = openfile->undotop;
		openfile->undotop = u;
		openfile->current_undo = u;
	}

	/* Record the info needed to be able to undo each possible action. */
	switch (u->type) {
	case ADD:
		/* If a new magic line will be added, an undo should remove it. */
		if (openfile->current == openfile->filebot)
			u->xflags |= WAS_FINAL_LINE;
		u->wassize--;
		break;
	case ENTER:
		break;
	case BACK:
		/* If the next line is the magic line, don't ever undo this
		 * backspace, as it won't actually have deleted anything. */
		if (openfile->current->next == openfile->filebot &&
						openfile->current->data[0] != '\0')
			u->xflags |= WAS_FINAL_BACKSPACE;
		/* Fall-through. */
	case DEL:
		/* When not at the end of a line, store the deleted character,
		 * else purposely fall into the line-joining code. */
		if (openfile->current->data[openfile->current_x] != '\0') {
			char *char_buf = charalloc(MAXCHARLEN + 1);
			int charlen = collect_char(&openfile->current->data[u->begin],
												char_buf);
			char_buf[charlen] = '\0';
			u->strdata = char_buf;
			if (u->type == BACK)
				u->mark_begin_x += charlen;
			break;
		}
		/* Fall-through. */
	case JOIN:
		if (openfile->current->next) {
			if (u->type == BACK) {
				u->lineno = openfile->current->next->lineno;
				u->begin = 0;
			}
			u->strdata = copy_of(openfile->current->next->data);
		}
		action = u->type = JOIN;
		break;
	case REPLACE:
		u->strdata = copy_of(openfile->current->data);
		break;
#ifdef ENABLE_WRAPPING
	case SPLIT_BEGIN:
		action = openfile->undotop->type;
		break;
	case SPLIT_END:
		break;
#endif
	case CUT_TO_EOF:
		u->xflags |= WAS_FINAL_LINE;
		break;
	case ZAP:
	case CUT:
		if (openfile->mark) {
			u->mark_begin_lineno = openfile->mark->lineno;
			u->mark_begin_x = openfile->mark_x;
			u->xflags |= MARK_WAS_SET;
			if (openfile->current == openfile->filebot ||
						openfile->mark == openfile->filebot)
				u->xflags |= WAS_FINAL_LINE;
		} else if (!ISSET(CUT_FROM_CURSOR)) {
			/* The entire line is being cut regardless of the cursor position. */
			u->begin = 0;
			u->xflags |= WAS_WHOLE_LINE;
		}
		break;
	case PASTE:
		u->cutbuffer = copy_buffer(cutbuffer);
		u->lineno += cutbottom->lineno - cutbuffer->lineno;
		break;
	case INSERT:
		break;
	case COUPLE_BEGIN:
		u->mark_begin_lineno = openfile->current_y;
		/* Fall-through. */
	case COUPLE_END:
		u->strdata = copy_of(_(message));
		break;
	case INDENT:
	case UNINDENT:
#ifdef ENABLE_COMMENT
	case COMMENT:
	case UNCOMMENT:
#endif
	default:
		break;
	}

	openfile->last_action = action;
}

/* Update a multiline undo item.  This should be called once for each line
 * affected by a multiple-line-altering feature.  The indentation that is
 * added or removed is saved separately for each line in the undo item. */
void update_multiline_undo(ssize_t lineno, char *indentation)
{
	undostruct *u = openfile->current_undo;

	/* If there already is a group and the current line is contiguous with it,
	 * extend the group; otherwise, create a new group. */
	if (u->grouping && u->grouping->bottom_line + 1 == lineno) {
		size_t number_of_lines;

		u->grouping->bottom_line++;

		number_of_lines = u->grouping->bottom_line - u->grouping->top_line + 1;
		u->grouping->indentations = (char **)nrealloc(u->grouping->indentations,
										number_of_lines * sizeof(char *));
		u->grouping->indentations[number_of_lines - 1] = copy_of(indentation);
	} else {
		groupstruct *born = nmalloc(sizeof(groupstruct));

		born->next = u->grouping;
		u->grouping = born;
		born->top_line = lineno;
		born->bottom_line = lineno;

		u->grouping->indentations = (char **)nmalloc(sizeof(char *));
		u->grouping->indentations[0] = copy_of(indentation);
	}

	/* Store the file size after the change, to be used when redoing. */
	u->newsize = openfile->totsize;
}

/* Update an undo item with (among other things) the file size and
 * cursor position after the given action. */
void update_undo(undo_type action)
{
	undostruct *u = openfile->undotop;
	char *char_buf;
	int charlen;

	if (u->type != action)
		statusline(ALERT, "Mismatching undo type -- please report a bug");

	u->newsize = openfile->totsize;

	switch (u->type) {
	case ADD:
		char_buf = charalloc(MAXCHARLEN);
		charlen = collect_char(&openfile->current->data[u->mark_begin_x],
								char_buf);
		u->strdata = addstrings(u->strdata, u->strdata ? strlen(u->strdata) : 0,
								char_buf, charlen);
		u->mark_begin_lineno = openfile->current->lineno;
		u->mark_begin_x = openfile->current_x;
		break;
	case ENTER:
		u->strdata = copy_of(openfile->current->data);
		u->mark_begin_x = openfile->current_x;
		break;
	case BACK:
	case DEL:
		char_buf = charalloc(MAXCHARLEN);
		charlen = collect_char(&openfile->current->data[openfile->current_x],
								char_buf);
		if (openfile->current_x == u->begin) {
			/* They deleted more: add removed character after earlier stuff. */
			u->strdata = addstrings(u->strdata, strlen(u->strdata), char_buf, charlen);
			u->mark_begin_x = openfile->current_x;
		} else if (openfile->current_x == u->begin - charlen) {
			/* They backspaced further: add removed character before earlier. */
			u->strdata = addstrings(char_buf, charlen, u->strdata, strlen(u->strdata));
			u->begin = openfile->current_x;
		} else {
			/* They deleted *elsewhere* on the line: start a new undo item. */
			free(char_buf);
			add_undo(u->type, NULL);
			return;
		}
		break;
	case JOIN:
		break;
	case REPLACE:
	case PASTE:
		u->lineno = openfile->current->lineno;
		u->begin = openfile->current_x;
		break;
#ifdef ENABLE_WRAPPING
	case SPLIT_BEGIN:
	case SPLIT_END:
		break;
#endif
	case ZAP:
	case CUT_TO_EOF:
	case CUT:
		if (!cutbuffer)
			break;
		if (u->type == ZAP)
			u->cutbuffer = cutbuffer;
		else {
			free_lines(u->cutbuffer);
			u->cutbuffer = copy_buffer(cutbuffer);
		}
		if (u->xflags & MARK_WAS_SET) {
			/* If the region was marked backwards, swap the end points. */
			if (u->lineno < u->mark_begin_lineno ||
						(u->lineno == u->mark_begin_lineno &&
						u->begin < u->mark_begin_x)) {
				ssize_t number = u->lineno;
				size_t position = u->begin;

				u->lineno = u->mark_begin_lineno;
				u->begin = u->mark_begin_x;

				u->mark_begin_lineno = number;
				u->mark_begin_x = position;
			} else
				u->xflags |= WAS_MARKED_FORWARD;
		} else {
			linestruct *bottomline = u->cutbuffer;
			size_t count = 0;

			/* Find the end of the cut for the undo/redo, using our copy. */
			while (bottomline->next != NULL) {
				bottomline = bottomline->next;
				count++;
			}
			u->lineno = u->mark_begin_lineno + count;
			if (ISSET(CUT_FROM_CURSOR) || u->type == CUT_TO_EOF) {
				u->begin = strlen(bottomline->data);
				if (u->lineno == u->mark_begin_lineno)
					u->begin += u->mark_begin_x;
			} else if (openfile->current == openfile->filebot &&
						ISSET(NO_NEWLINES))
				u->begin = strlen(bottomline->data);
		}
		break;
	case INSERT:
		u->mark_begin_lineno = openfile->current->lineno;
		u->mark_begin_x = openfile->current_x;
		break;
	case COUPLE_BEGIN:
		break;
	case COUPLE_END:
		u->lineno = openfile->current->lineno;
		u->begin = openfile->current_x;
		break;
	default:
		break;
	}
}
#endif /* !NANO_TINY */

#ifdef ENABLE_WRAPPING
/* When the current line is overlong, hard-wrap it at the furthest possible
 * whitespace character, and (if possible) prepend the remainder of the line
 * to the next line.  Return TRUE if wrapping occurred, and FALSE otherwise. */
bool do_wrap(void)
{
	linestruct *line = openfile->current;
		/* The line to be wrapped, if needed and possible. */
	size_t line_len = strlen(line->data);
		/* The length of this line. */
#ifdef ENABLE_JUSTIFY
	size_t quot_len = quote_length(line->data);
		/* The length of the quoting part of this line. */
	size_t lead_len = quot_len + indent_length(line->data + quot_len);
		/* The length of the quoting part plus subsequent whitespace. */
#else
	size_t lead_len = indent_length(line->data);
#endif
	size_t cursor_x = openfile->current_x;
		/* The current cursor position, for comparison with the wrap point. */
	ssize_t wrap_loc;
		/* The position in the line's text where we wrap. */
	const char *remainder;
		/* The text after the wrap point. */
	size_t rest_length;
		/* The length of the remainder. */

	/* First find the last blank character where we can break the line. */
	wrap_loc = break_line(line->data + lead_len,
							wrap_at - wideness(line->data, lead_len), FALSE);

	/* If no wrapping point was found before end-of-line, we don't wrap. */
	if (wrap_loc < 0 || lead_len + wrap_loc == line_len)
		return FALSE;

	/* Adjust the wrap location to its position in the full line,
	 * and step forward to the character just after the blank. */
	wrap_loc = lead_len + step_right(line->data + lead_len, wrap_loc);

	/* When now at end-of-line, no need to wrap. */
	if (line->data[wrap_loc] == '\0')
		return FALSE;

#ifndef NANO_TINY
	add_undo(SPLIT_BEGIN, NULL);
#endif
#ifdef ENABLE_JUSTIFY
	bool autowhite = ISSET(AUTOINDENT);

	if (quot_len > 0)
		UNSET(AUTOINDENT);
#endif

	/* The remainder is the text that will be wrapped to the next line. */
	remainder = line->data + wrap_loc;
	rest_length = line_len - wrap_loc;

	/* When prepending and the remainder of this line will not make the next
	 * line too long, then join the two lines, so that, after the line wrap,
	 * the remainder will effectively have been prefixed to the next line. */
	if (openfile->spillage_line && openfile->spillage_line == line->next &&
				rest_length + breadth(line->next->data) <= wrap_at) {
		/* Go to the end of this line. */
		openfile->current_x = line_len;

		/* If the remainder doesn't end in a blank, add a space. */
		if (!is_blank_mbchar(remainder + step_left(remainder, rest_length))) {
#ifndef NANO_TINY
			add_undo(ADD, NULL);
#endif
			line->data = charealloc(line->data, line_len + 2);
			line->data[line_len] = ' ';
			line->data[line_len + 1] = '\0';
			rest_length++;
			openfile->totsize++;
			openfile->current_x++;
#ifndef NANO_TINY
			update_undo(ADD);
#endif
		}

		/* Join the next line to this one. */
		do_delete();

#ifdef ENABLE_JUSTIFY
		/* If the leading part of the current line equals the leading part of
		 * what was the next line, then strip this second leading part. */
		if (strncmp(line->data, line->data + openfile->current_x, lead_len) == 0)
			for (size_t i = lead_len; i > 0; i--)
				do_delete();
#endif
		/* Remove any extra blanks. */
		while (is_blank_mbchar(&line->data[openfile->current_x]))
			do_delete();
	}

	/* Go to the wrap location. */
	openfile->current_x = wrap_loc;

	/* When requested, snip trailing blanks off the wrapped line. */
	if (ISSET(TRIM_BLANKS)) {
		size_t tail_x = step_left(line->data, wrap_loc);
		size_t typed_x = step_left(line->data, cursor_x);

		while ((tail_x != typed_x || cursor_x >= wrap_loc) &&
						is_blank_mbchar(line->data + tail_x)) {
			openfile->current_x = tail_x;
			do_delete();
			tail_x = step_left(line->data, tail_x);
		}
	}

	/* Now split the line. */
	do_enter();

#ifdef ENABLE_JUSTIFY
	/* If the original line has quoting, copy it to the spillage line. */
	if (quot_len > 0) {
		line = line->next;
		line_len = strlen(line->data);
		line->data = charealloc(line->data, lead_len + line_len + 1);

		memmove(line->data + lead_len, line->data, line_len + 1);
		strncpy(line->data, line->prev->data, lead_len);

		openfile->current_x += lead_len;
#ifndef NANO_TINY
		update_undo(ENTER);
#endif
		if (autowhite)
			SET(AUTOINDENT);
	}
#endif

	openfile->spillage_line = openfile->current;

	if (cursor_x < wrap_loc) {
		openfile->current = openfile->current->prev;
		openfile->current_x = cursor_x;
	} else
		openfile->current_x += (cursor_x - wrap_loc);

	openfile->placewewant = xplustabs();

#ifndef NANO_TINY
	add_undo(SPLIT_END, NULL);
#endif

	return TRUE;
}
#endif /* ENABLE_WRAPPING */

#if defined(ENABLE_HELP) || defined(ENABLED_WRAPORJUSTIFY)
/* We are trying to break a chunk off line.  We find the last blank such
 * that the display length to there is at most (goal + 1).  If there is
 * no such blank, then we find the first blank.  We then take the last
 * blank in that group of blanks.  The terminating '\0' counts as a
 * blank, as does a '\n' if snap_at_nl is TRUE. */
ssize_t break_line(const char *line, ssize_t goal, bool snap_at_nl)
{
	ssize_t lastblank = -1;
		/* The index of the last blank we found. */
	ssize_t index = 0;
		/* The index of the character we are looking at. */
	size_t column = 0;
		/* The column position that corresponds with index. */
	int charlen = 0;
		/* The length of the current character, in bytes. */

	/* Find the last blank that does not overshoot the target column. */
	while (*line != '\0' && ((ssize_t)column <= goal)) {
		if (is_blank_mbchar(line))
			lastblank = index;
#ifdef ENABLE_HELP
		else if (snap_at_nl && *line == '\n') {
			lastblank = index;
			break;
		}
#endif
		charlen = advance_over(line, &column);
		line += charlen;
		index += charlen;
	}

	/* If the whole line displays shorter than goal, we're done. */
	if ((ssize_t)column <= goal)
		return index;

#ifdef ENABLE_HELP
	/* If we're wrapping a help text and no blank was found, or was
	 * found only as the first character, force a line break. */
	if (snap_at_nl && lastblank < 1)
		return (index - charlen);
#endif

	/* If no blank was found within the goal width, seek one after it. */
	if (lastblank < 0) {
		while (*line != '\0') {
			if (is_blank_mbchar(line))
				lastblank = index;
			else if (lastblank > 0)
				return lastblank;

			charlen = char_length(line);
			line += charlen;
			index += charlen;
		}

		return -1;
	}

	/* Move the pointer back to the last blank, and then step beyond it. */
	line = line - index + lastblank;
	charlen = char_length(line);
	line += charlen;

	/* Skip any consecutive blanks after the last blank. */
	while (*line != '\0' && is_blank_mbchar(line)) {
		lastblank += charlen;
		charlen = char_length(line);
		line += charlen;
	}

	return lastblank;
}
#endif /* ENABLE_HELP || ENABLED_WRAPORJUSTIFY */

#if !defined(NANO_TINY) || defined(ENABLED_WRAPORJUSTIFY)
/* Return the length of the indentation part of the given line.  The
 * "indentation" of a line is the leading consecutive whitespace. */
size_t indent_length(const char *line)
{
	size_t len = 0;
	char onechar[MAXCHARLEN];
	int charlen;

	while (*line != '\0') {
		charlen = collect_char(line, onechar);

		if (!is_blank_mbchar(onechar))
			break;

		line += charlen;
		len += charlen;
	}

	return len;
}
#endif

#ifdef ENABLE_JUSTIFY
/* Copy a character from one place to another. */
void copy_character(char **from, char **to)
{
	int charlen = char_length(*from);

	if (*from == *to) {
		*from += charlen;
		*to += charlen;
	} else
		while (--charlen >= 0)
			*((*to)++) = *((*from)++);
}

/* In the given line, replace any series of blanks with a single space,
 * but keep two spaces (if there are two) after any closing punctuation,
 * and remove all blanks from the end of the line.  Leave the first skip
 * number of characters untreated. */
void squeeze(linestruct *line, size_t skip)
{
	char *start = line->data + skip;
	char *from = start, *to = start;

	/* For each character, 1) when a blank, change it to a space, and pass over
	 * all blanks after it; 2) if it is punctuation, copy it plus a possible
	 * tailing bracket, and change at most two subsequent blanks to spaces, and
	 * pass over all blanks after these; 3) leave anything else unchanged. */
	while (*from != '\0') {
		if (is_blank_mbchar(from)) {
			from += char_length(from);
			*(to++) = ' ';

			while (*from != '\0' && is_blank_mbchar(from))
				from += char_length(from);
		} else if (mbstrchr(punct, from) != NULL) {
			copy_character(&from, &to);

			if (*from != '\0' && mbstrchr(brackets, from) != NULL)
				copy_character(&from, &to);

			if (*from != '\0' && is_blank_mbchar(from)) {
				from += char_length(from);
				*(to++) = ' ';
			}
			if (*from != '\0' && is_blank_mbchar(from)) {
				from += char_length(from);
				*(to++) = ' ';
			}

			while (*from != '\0' && is_blank_mbchar(from))
				from += char_length(from);
		} else
			copy_character(&from, &to);
	}

	/* If there are spaces at the end of the line, remove them. */
	while (to > start && *(to - 1) == ' ')
		to--;

	*to = '\0';
}

/* Return the length of the quote part of the given line.  The "quote part"
 * of a line is the largest initial substring matching the quoting regex. */
size_t quote_length(const char *line)
{
	regmatch_t matches;
	int rc = regexec(&quotereg, line, 1, &matches, 0);

	if (rc == REG_NOMATCH || matches.rm_so == (regoff_t)-1)
		return 0;

	return matches.rm_eo;
}

/* The maximum depth of recursion.  This must be an even number. */
#define RECURSION_LIMIT 222

/* Return TRUE when the given line is the beginning of a paragraph (BOP). */
bool begpar(const linestruct *const line, int depth)
{
	size_t quote_len, indent_len, prev_dent_len;

	/* If this is the very first line of the buffer, it counts as a BOP
	 * even when it contains no text. */
	if (line == openfile->filetop)
		return TRUE;

	/* If recursion is going too deep, just say it's not a BOP. */
	if (depth > RECURSION_LIMIT)
		return FALSE;

	quote_len = quote_length(line->data);
	indent_len = indent_length(line->data + quote_len);

	/* If this line contains no text, it is not a BOP. */
	if (line->data[quote_len + indent_len] == '\0')
		return FALSE;

	/* If the quote part of the preceding line differs, this is a BOP. */
	if (quote_len != quote_length(line->prev->data) ||
					strncmp(line->data, line->prev->data, quote_len) != 0)
		return TRUE;

	prev_dent_len = indent_length(line->prev->data + quote_len);

	/* If the preceding line contains no text, this is a BOP. */
	if (line->prev->data[quote_len + prev_dent_len] == '\0')
		return TRUE;

	/* If indentation of this and preceding line are equal, this is not a BOP. */
	if (wideness(line->prev->data, quote_len + prev_dent_len) ==
						wideness(line->data, quote_len + indent_len))
		return FALSE;

	/* Otherwise, this is a BOP if the preceding line is not. */
	return !begpar(line->prev, depth + 1);
}

/* Return TRUE when the given line is part of a paragraph: when it
 * contains something more than quoting and leading whitespace. */
bool inpar(const linestruct *const line)
{
	size_t quote_len = quote_length(line->data);
	size_t indent_len = indent_length(line->data + quote_len);

	return (line->data[quote_len + indent_len] != '\0');
}

/* Find the first occurring paragraph in the forward direction.  Return TRUE
 * when a paragraph was found, and FALSE otherwise.  Furthermore, return the
 * first line and the length (number of lines) of the paragraph. */
bool find_paragraph(linestruct **firstline, size_t *const parlen)
{
	linestruct *line = *firstline;

	/* When not currently in a paragraph, move forward to a line that is. */
	while (!inpar(line) && line->next != NULL)
		line = line->next;

	*firstline = line;

	/* Move down to the last line of the paragraph (if any). */
	do_para_end(&line);

	/* When not in a paragraph now, there aren't any paragraphs left. */
	if (!inpar(line))
		return FALSE;

	/* We found a paragraph; determine its number of lines. */
	*parlen = line->lineno - (*firstline)->lineno + 1;

	return TRUE;
}

/* Concatenate into a single line all the lines of the paragraph that starts at
 * *line and consists of par_len lines, skipping the quoting and indentation on
 * all lines after the first. */
void concat_paragraph(linestruct **line, size_t par_len)
{
	while (par_len > 1) {
		linestruct *next_line = (*line)->next;
		size_t line_len = strlen((*line)->data);
		size_t next_line_len = strlen(next_line->data);
		size_t next_quote_len = quote_length(next_line->data);
		size_t next_lead_len = next_quote_len +
							indent_length(next_line->data + next_quote_len);

		/* We're just about to tack the next line onto this one.  If
		 * this line isn't empty, make sure it ends in a space. */
		if (line_len > 0 && (*line)->data[line_len - 1] != ' ') {
			(*line)->data = charealloc((*line)->data, line_len + 2);
			(*line)->data[line_len++] = ' ';
			(*line)->data[line_len] = '\0';
		}

		(*line)->data = charealloc((*line)->data,
								line_len + next_line_len - next_lead_len + 1);
		strcat((*line)->data, next_line->data + next_lead_len);

		unlink_node(next_line);
		par_len--;
	}
}

/* Rewrap the given line (that starts with the given lead string which is of
 * the given length), into lines that fit within the target width (wrap_at). */
void rewrap_paragraph(linestruct **line, char *lead_string, size_t lead_len)
{
	ssize_t break_pos;
		/* The x-coordinate where the current line is to be broken. */

	while (breadth((*line)->data) > wrap_at) {
		size_t line_len = strlen((*line)->data);

		/* Find a point in the line where it can be broken. */
		break_pos = break_line((*line)->data + lead_len,
						wrap_at - wideness((*line)->data, lead_len), FALSE);

		/* If we can't break the line, or don't need to, we're done. */
		if (break_pos < 0 || lead_len + break_pos == line_len)
			break;

		/* Adjust the breaking position for the leading part and
		 * move it beyond the found whitespace character. */
		break_pos += lead_len + 1;

		/* Insert a new line after the current one, and copy the leading part
		 * plus the text after the breaking point into it. */
		splice_node(*line, make_new_node(*line));
		(*line)->next->data = charalloc(lead_len + line_len - break_pos + 1);
		strncpy((*line)->next->data, lead_string, lead_len);
		strcpy((*line)->next->data + lead_len, (*line)->data + break_pos);

		/* When requested, snip the one or two trailing spaces. */
		if (ISSET(TRIM_BLANKS)) {
			while (break_pos > 0 && (*line)->data[break_pos - 1] == ' ')
				break_pos--;
		}

		/* Now actually break the current line, and go to the next. */
		(*line)->data[break_pos] = '\0';
		*line = (*line)->next;
	}

	/* When possible, go to the line after the rewrapped paragraph. */
	if ((*line)->next != NULL)
		*line = (*line)->next;
}

/* Justify the lines of the given paragraph (that starts at *line, and consists
 * of par_len lines) so they all fit within the target width (wrap_at) and have
 * their whitespace normalized. */
void justify_paragraph(linestruct **line, size_t par_len)
{
	linestruct *sampleline;
		/* The line from which the indentation is copied. */
	size_t quote_len;
		/* Length of the quote part. */
	size_t lead_len;
		/* Length of the quote part plus the indentation part. */
	char *lead_string;
		/* The quote+indent stuff that is copied from the sample line. */

	/* The sample line is either the only line or the second line. */
	sampleline = (par_len == 1 ? *line : (*line)->next);

	/* Copy the leading part (quoting + indentation) of the sample line. */
	quote_len = quote_length(sampleline->data);
	lead_len = quote_len + indent_length(sampleline->data + quote_len);
	lead_string = measured_copy(sampleline->data, lead_len + 1);
	lead_string[lead_len] = '\0';

	/* Concatenate all lines of the paragraph into a single line. */
	concat_paragraph(line, par_len);

	/* Change all blank characters to spaces and remove excess spaces. */
	squeeze(*line, quote_len + indent_length((*line)->data + quote_len));

	/* Rewrap the line into multiple lines, accounting for the leading part. */
	rewrap_paragraph(line, lead_string, lead_len);

	free(lead_string);
}

/* Justify the current paragraph, or the entire buffer when full_justify is
 * TRUE.  But if the mark is on, justify only the marked text instead. */
void do_justify(bool full_justify)
{
	size_t par_len;
		/* The number of lines in the original paragraph. */
	linestruct *first_par_line;
		/* The first line of the paragraph. */
	linestruct *last_par_line;
		/* The line after the last line of the paragraph. */
	size_t top_x;
		/* The top x-coordinate of the paragraph we justify. */
	size_t bot_x;
		/* The bottom x-coordinate of the paragraph we justify. */
	linestruct *was_cutbuffer = cutbuffer;
		/* The old cutbuffer, so we can justify in the current cutbuffer. */
	linestruct *jusline;
		/* The line that we're justifying in the current cutbuffer. */

#ifndef NANO_TINY
	/* Stash the cursor position, to be stored in the undo item. */
	ssize_t was_lineno = openfile->current->lineno;
	size_t was_current_x = openfile->current_x;

	/* We need these to restore the coordinates of the mark after justifying
	 * marked text. */
	ssize_t was_top_lineno = 0;
	size_t was_top_x = 0;
	bool right_side_up = FALSE;

	/* Whether the bottom of the mark is at the end of its line, in which case
	 * we don't need to add a new line after it. */
	bool ends_at_eol = FALSE;

	/* We need these to hold the leading part (quoting + indentation) of the
	 * line where the marked text begins, whether or not that part is covered
	 * by the mark. */
	char *the_lead = NULL;
	size_t lead_len = 0;

	/* We need these to hold the leading part of the line after the line where
	 * the marked text begins (if any). */
	char *the_second_lead = NULL;
	size_t second_lead_len = 0;
#endif

#ifndef NANO_TINY
	/* If the mark is on, do as Pico: treat all marked text as one paragraph. */
	if (openfile->mark) {
		size_t quote_len;

		get_region((const linestruct **)&first_par_line, &top_x,
					(const linestruct **)&last_par_line, &bot_x, &right_side_up);

		/* Save the starting point of the marked region. */
		was_top_lineno = first_par_line->lineno;
		was_top_x = top_x;

		par_len = last_par_line->lineno - first_par_line->lineno +
											(bot_x > 0 ? 1 : 0);

		/* Remember whether the end of the region was at the end of a line. */
		ends_at_eol = last_par_line->data[bot_x] == '\0';

		/* Copy the leading part that is to be used for the new paragraph. */
		quote_len = quote_length(first_par_line->data);
		lead_len = quote_len + indent_length(first_par_line->data + quote_len);
		the_lead = measured_copy(first_par_line->data, lead_len + 1);
		the_lead[lead_len] = '\0';

		/* Copy the leading part that is to be used for the new paragraph after
		 * its first line (if any): the quoting of the first line, plus the
		 * indentation of the second line. */
		if (first_par_line != last_par_line) {
			size_t sample_quote_len = quote_length(first_par_line->next->data);
			size_t sample_indent_len = indent_length(first_par_line->next->data +
														sample_quote_len);

			second_lead_len = quote_len + sample_indent_len;
			the_second_lead = charalloc(second_lead_len + 1);
			strncpy(the_second_lead, first_par_line->data, quote_len);
			strncpy(the_second_lead + quote_len, first_par_line->next->data +
					sample_quote_len, sample_indent_len);
			the_second_lead[second_lead_len] = '\0';
		}
	} else
#endif
	{
		size_t jus_len;
			/* The number of lines we're storing in the current cutbuffer. */

		/* When justifying the entire buffer, start at the top.  Otherwise, when
		 * in a paragraph but not at its beginning, move back to its first line. */
		if (full_justify)
			openfile->current = openfile->filetop;
		else if (inpar(openfile->current) && !begpar(openfile->current, 0))
			do_para_begin(&openfile->current);

		/* Find the first line of the paragraph(s) to be justified.  If the
		 * search fails, there is nothing to justify, and we will be on the
		 * last line of the file, so put the cursor at the end of it. */
		if (!find_paragraph(&openfile->current, &par_len)) {
			openfile->current_x = strlen(openfile->filebot->data);
			refresh_needed = TRUE;
			return;
		}

		first_par_line = openfile->current;
		top_x = 0;

		/* Set the number of lines to be pulled into the cutbuffer. */
		if (full_justify)
			jus_len = openfile->filebot->lineno - first_par_line->lineno + 1;
		else
			jus_len = par_len;

		/* Move down to the last line to be extracted. */
		for (last_par_line = openfile->current; jus_len > 1; jus_len--)
			last_par_line = last_par_line->next;

		/* When possible, step one line further; otherwise, to line's end. */
		if (last_par_line->next != NULL) {
			last_par_line = last_par_line->next;
			bot_x = 0;
		} else
			bot_x = strlen(last_par_line->data);
	}

#ifndef NANO_TINY
	add_undo(COUPLE_BEGIN, N_("justification"));

	/* Store the original cursor position, in case we unjustify. */
	openfile->undotop->lineno = was_lineno;
	openfile->undotop->begin = was_current_x;

	add_undo(CUT, NULL);
#endif

	/* Do the equivalent of a marked cut into an empty cutbuffer. */
	cutbuffer = NULL;
	extract_segment(first_par_line, top_x, last_par_line, bot_x);
#ifndef NANO_TINY
	update_undo(CUT);

	if (openfile->mark) {
		size_t line_len = strlen(cutbuffer->data), indent_len;
		size_t needed_top_extra = (top_x < lead_len ? top_x : lead_len);
		size_t needed_bot_extra = (bot_x < lead_len ? lead_len - bot_x : 0);
		linestruct *line;

		/* If the marked region starts in the middle of a line, and this line
		 * has a leading part, prepend any missing portion of this leading part
		 * to the first line of the extracted region. */
		if (needed_top_extra > 0) {
			cutbuffer->data = charealloc(cutbuffer->data,
									line_len + needed_top_extra + 1);
			memmove(cutbuffer->data + needed_top_extra, cutbuffer->data,
									line_len + 1);
			strncpy(cutbuffer->data, the_lead, needed_top_extra);
			line_len += needed_top_extra;

			/* When no portion was missing, nothing needs removing later. */
			if (top_x > lead_len)
				needed_top_extra = 0;
		}

		indent_len = indent_length(cutbuffer->data + lead_len);

		/* Remove extra whitespace after the leading part. */
		if (indent_len > 0)
			memmove(cutbuffer->data + lead_len,
						cutbuffer->data + lead_len + indent_len,
						line_len - indent_len + 1);

		/* If the marked region ends in the middle of a line, and this line
		 * has a leading part, check if the last line of the extracted region
		 * contains a missing portion of this leading part.  If it has no
		 * missing portion, we don't need to append anything below. */
		if (strncmp(cutbottom->data, the_lead, lead_len - needed_bot_extra) != 0)
			needed_bot_extra = 0;

		/* Now justify the extracted region. */
		concat_paragraph(&cutbuffer, par_len);
		squeeze(cutbuffer, lead_len);
		line = cutbuffer;
		if (the_second_lead != NULL) {
			rewrap_paragraph(&line, the_second_lead, second_lead_len);
			free(the_second_lead);
		} else
			rewrap_paragraph(&line, the_lead, lead_len);

		/* If the marked region started after the beginning of a line, insert
		 * a new line before the new paragraph.  But if the region started in
		 * the middle of the line's leading part, no new line is needed: just
		 * remove the (now-redundant) addition we made earlier. */
		if (top_x > 0) {
			if (needed_top_extra > 0)
				memmove(cutbuffer->data, cutbuffer->data + needed_top_extra,
							strlen(cutbuffer->data) - needed_top_extra + 1);
			else {
				cutbuffer->prev = make_new_node(NULL);
				cutbuffer->prev->data = copy_of("");
				cutbuffer->prev->next = cutbuffer;
				cutbuffer = cutbuffer->prev;
			}
		}

		/* If the marked region ended in the middle of a line, insert a new
		 * line after the new paragraph.  If the region ended in the middle
		 * of a line's leading part, make the new line start with the missing
		 * portion, so it will become a full leading part when the justified
		 * region is "pasted" back. */
		if (bot_x > 0 && !ends_at_eol) {
			line->next = make_new_node(line);
			line->next->data = copy_of(the_lead + needed_bot_extra);
		}

		free(the_lead);
	} else
#endif
	{
		/* Prepare to justify the text we just put in the cutbuffer. */
		jusline = cutbuffer;

		/* Justify the current paragraph. */
		justify_paragraph(&jusline, par_len);

		/* When justifying the entire buffer, find and justify all paragraphs. */
		if (full_justify) {
			while (find_paragraph(&jusline, &par_len)) {
				justify_paragraph(&jusline, par_len);

				if (jusline->next == NULL)
					break;
			}
		}
	}

#ifndef NANO_TINY
	add_undo(PASTE, NULL);
#endif
	/* Do the equivalent of a paste of the justified text. */
	ingraft_buffer(cutbuffer);
#ifndef NANO_TINY
	update_undo(PASTE);

	add_undo(COUPLE_END, "justification");

	/* If we justified marked text, restore mark or cursor position. */
	if (openfile->mark) {
		if (right_side_up) {
			openfile->mark = line_from_number(was_top_lineno);
			openfile->mark_x = was_top_x;
		} else {
			openfile->current = line_from_number(was_top_lineno);
			openfile->current_x = was_top_x;
		}
		update_undo(COUPLE_END);
	}
#endif

	/* We're done justifying.  Restore the old cutbuffer. */
	cutbuffer = was_cutbuffer;

	/* Show what we justified on the status bar. */
#ifndef NANO_TINY
	if (openfile->mark)
		statusbar(_("Justified selection"));
	else
#endif
	if (full_justify)
		statusbar(_("Justified file"));
	else
		statusbar(_("Justified paragraph"));

	/* Set the desired screen column (always zero, except at EOF). */
	openfile->placewewant = xplustabs();

	set_modified();
	refresh_needed = TRUE;
	shift_held = TRUE;
}

/* Justify the current paragraph. */
void do_justify_void(void)
{
	do_justify(FALSE);
}

/* Justify the entire file. */
void do_full_justify(void)
{
	do_justify(TRUE);
}
#endif /* ENABLE_JUSTIFY */

#if defined(ENABLE_SPELLER) || defined (ENABLE_COLOR)
/* Set up an argument list for executing the given command. */
void construct_argument_list(char ***arguments, char *command, char *filename)
{
	char *copy_of_command = copy_of(command);
	char *element = strtok(copy_of_command, " ");
	int count = 2;

	while (element != NULL) {
		*arguments = (char **)nrealloc(*arguments, ++count * sizeof(char *));
		(*arguments)[count - 3] = element;
		element = strtok(NULL, " ");
	}

	(*arguments)[count - 2] = filename;
	(*arguments)[count - 1] = NULL;
}
#endif

#ifdef ENABLE_SPELLER
/* Let the user edit the misspelled word.  Return FALSE if the user cancels. */
bool fix_spello(const char *word)
{
	char *save_search;
	size_t firstcolumn_save = openfile->firstcolumn;
	size_t current_x_save = openfile->current_x;
	linestruct *edittop_save = openfile->edittop;
	linestruct *current_save = openfile->current;
		/* Save where we are. */
	bool proceed = FALSE;
		/* The return value of this function. */
	bool result;
		/* The return value of searching for a misspelled word. */
#ifndef NANO_TINY
	bool right_side_up = FALSE;
		/* TRUE if (mark_begin, mark_begin_x) is the top of the mark,
		 * FALSE if (current, current_x) is. */
	linestruct *top, *bot;
	size_t top_x, bot_x;
#endif

	/* Save the current search string, then set it to the misspelled word. */
	save_search = last_search;
	last_search = copy_of(word);

#ifndef NANO_TINY
	/* If the mark is on, start at the beginning of the marked region. */
	if (openfile->mark) {
		get_region((const linestruct **)&top, &top_x,
					(const linestruct **)&bot, &bot_x, &right_side_up);
		/* If the region is marked normally, swap the end points, so that
		 * (current, current_x) (where searching starts) is at the top. */
		if (right_side_up) {
			openfile->current = top;
			openfile->current_x = top_x;
			openfile->mark = bot;
			openfile->mark_x = bot_x;
		}
	} else
#endif
	/* Otherwise, start from the top of the file. */
	{
		openfile->current = openfile->filetop;
		openfile->current_x = 0;
	}

	/* Find the first whole occurrence of word. */
	result = findnextstr(word, TRUE, INREGION, NULL, FALSE, NULL, 0);

	/* If the word isn't found, alert the user; if it is, allow correction. */
	if (result == 0) {
		statusline(ALERT, _("Unfindable word: %s"), word);
		lastmessage = HUSH;
		proceed = TRUE;
		napms(2800);
	} else if (result == 1) {
		spotlighted = TRUE;
		light_from_col = xplustabs();
		light_to_col = light_from_col + breadth(word);
#ifndef NANO_TINY
		linestruct *saved_mark = openfile->mark;
		openfile->mark = NULL;
#endif
		edit_refresh();

		/* Let the user supply a correctly spelled alternative. */
		proceed = (do_prompt(FALSE, FALSE, MSPELL, word, NULL,
								edit_refresh, _("Edit a replacement")) != -1);

		spotlighted = FALSE;

#ifndef NANO_TINY
		openfile->mark = saved_mark;
#endif

		/* If a replacement was given, go through all occurrences. */
		if (proceed && strcmp(word, answer) != 0) {
			do_replace_loop(word, TRUE, current_save, &current_x_save);

			/* TRANSLATORS: Shown after fixing misspellings in one word. */
			statusbar(_("Next word..."));
			napms(400);
		}
	}

#ifndef NANO_TINY
	if (openfile->mark) {
		/* Restore the (compensated) end points of the marked region. */
		if (right_side_up) {
			openfile->current = openfile->mark;
			openfile->current_x = openfile->mark_x;
			openfile->mark = top;
			openfile->mark_x = top_x;
		} else {
			openfile->current = top;
			openfile->current_x = top_x;
		}
	} else
#endif
	{
		/* Restore the (compensated) cursor position. */
		openfile->current = current_save;
		openfile->current_x = current_x_save;
	}

	/* Restore the string that was last searched for. */
	free(last_search);
	last_search = save_search;

	/* Restore the viewport to where it was. */
	openfile->edittop = edittop_save;
	openfile->firstcolumn = firstcolumn_save;

	return proceed;
}

/* Run a spell-check on the given file, using 'spell' to produce a list of all
 * misspelled words, then feeding those through 'sort' and 'uniq' to obtain an
 * alphabetical list, which words are then offered one by one to the user for
 * correction.  Return NULL when okay, and the error string otherwise. */
const char *do_int_speller(const char *tempfile_name)
{
	char *misspellings, *pointer, *oneword;
	long pipesize;
	size_t buffersize, bytesread, totalread;
	int spell_fd[2], sort_fd[2], uniq_fd[2], tempfile_fd = -1;
	pid_t pid_spell, pid_sort, pid_uniq;
	int spell_status, sort_status, uniq_status;

	/* Create all three pipes up front. */
	if (pipe(spell_fd) == -1 || pipe(sort_fd) == -1 || pipe(uniq_fd) == -1)
		return _("Could not create pipe");

	/* Fork a process to run spell in. */
	if ((pid_spell = fork()) == 0) {
		/* Child: open the temporary file that holds the text to be checked. */
		if ((tempfile_fd = open(tempfile_name, O_RDONLY)) == -1)
			exit(6);

		/* Connect standard input to the temporary file. */
		if (dup2(tempfile_fd, STDIN_FILENO) != STDIN_FILENO)
			exit(7);

		/* Connect standard output to the write end of the first pipe. */
		if (dup2(spell_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
			exit(8);

		close(tempfile_fd);
		close(spell_fd[0]);
		close(spell_fd[1]);

		/* Try to run 'hunspell'; if that fails, fall back to 'spell'. */
		execlp("hunspell", "hunspell", "-l", NULL);
		execlp("spell", "spell", NULL);

		/* Indicate failure when neither speller was found. */
		exit(9);
	}

	/* Parent: close the unused write end of the first pipe. */
	close(spell_fd[1]);

	/* Fork a process to run sort in. */
	if ((pid_sort = fork()) == 0) {
		/* Connect standard input to the read end of the first pipe. */
		if (dup2(spell_fd[0], STDIN_FILENO) != STDIN_FILENO)
			exit(7);

		/* Connect standard output to the write end of the second pipe. */
		if (dup2(sort_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
			exit(8);

		close(spell_fd[0]);
		close(sort_fd[0]);
		close(sort_fd[1]);

		/* Now run the sort program.  Use -f to mix upper and lower case. */
		execlp("sort", "sort", "-f", NULL);

		exit(9);
	}

	close(spell_fd[0]);
	close(sort_fd[1]);

	/* Fork a process to run uniq in. */
	if ((pid_uniq = fork()) == 0) {
		if (dup2(sort_fd[0], STDIN_FILENO) != STDIN_FILENO)
			exit(7);

		if (dup2(uniq_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
			exit(8);

		close(sort_fd[0]);
		close(uniq_fd[0]);
		close(uniq_fd[1]);

		execlp("uniq", "uniq", NULL);

		exit(9);
	}

	close(sort_fd[0]);
	close(uniq_fd[1]);

	/* When some child process was not forked successfully... */
	if (pid_spell < 0 || pid_sort < 0 || pid_uniq < 0) {
		close(uniq_fd[0]);
		return _("Could not fork");
	}

	/* Get the system pipe buffer size. */
	pipesize = fpathconf(uniq_fd[0], _PC_PIPE_BUF);

	if (pipesize < 1) {
		close(uniq_fd[0]);
		return _("Could not get size of pipe buffer");
	}

	/* Leave curses mode so that error messages go to the original screen. */
	endwin();

	/* Block SIGWINCHes while reading misspelled words from the third pipe. */
	block_sigwinch(TRUE);

	totalread = 0;
	buffersize = pipesize + 1;
	misspellings = charalloc(buffersize);
	pointer = misspellings;

	while ((bytesread = read(uniq_fd[0], pointer, pipesize)) > 0) {
		totalread += bytesread;
		buffersize += pipesize;
		misspellings = charealloc(misspellings, buffersize);
		pointer = misspellings + totalread;
	}

	*pointer = '\0';
	close(uniq_fd[0]);

	block_sigwinch(FALSE);

	/* Re-enter curses mode. */
	terminal_init();
	doupdate();

	/* Do any replacements case-sensitively, forward, and without regexes. */
	SET(CASE_SENSITIVE);
	UNSET(BACKWARDS_SEARCH);
	UNSET(USE_REGEXP);

	pointer = misspellings;
	oneword = misspellings;

	/* Process each of the misspelled words. */
	while (*pointer != '\0') {
		if ((*pointer == '\r') || (*pointer == '\n')) {
			*pointer = '\0';
			if (oneword != pointer) {
				if (!fix_spello(oneword)) {
					oneword = pointer;
					break;
				}
			}
			oneword = pointer + 1;
		}
		pointer++;
	}

	/* Special case: the last word doesn't end with '\r' or '\n'. */
	if (oneword != pointer)
		fix_spello(oneword);

	free(misspellings);
	refresh_needed = TRUE;

	/* Process the end of the three processes. */
	waitpid(pid_spell, &spell_status, 0);
	waitpid(pid_sort, &sort_status, 0);
	waitpid(pid_uniq, &uniq_status, 0);

	if (WIFEXITED(uniq_status) == 0 || WEXITSTATUS(uniq_status))
		return _("Error invoking \"uniq\"");

	if (WIFEXITED(sort_status) == 0 || WEXITSTATUS(sort_status))
		return _("Error invoking \"sort -f\"");

	if (WIFEXITED(spell_status) == 0 || WEXITSTATUS(spell_status))
		return _("Error invoking \"spell\"");

	/* When all went okay. */
	statusbar(_("Finished checking spelling"));
	return NULL;
}

/* Execute the given program, with the given temp file as last argument. */
const char *treat(char *tempfile_name, char *theprogram, bool spelling)
{
	ssize_t lineno_save = openfile->current->lineno;
	size_t current_x_save = openfile->current_x;
	size_t pww_save = openfile->placewewant;
	bool was_at_eol = (openfile->current->data[openfile->current_x] == '\0');
	struct stat fileinfo;
	long timestamp_sec, timestamp_nsec;
	static char **arguments = NULL;
	pid_t thepid;
	int program_status;
	bool replaced = FALSE;

	/* Get the timestamp and the size of the temporary file. */
	stat(tempfile_name, &fileinfo);
	timestamp_sec = (long)fileinfo.st_mtim.tv_sec;
	timestamp_nsec = (long)fileinfo.st_mtim.tv_nsec;

	/* If the number of bytes to check is zero, get out. */
	if (fileinfo.st_size == 0)
		return NULL;

	/* Exit from curses mode to give the program control of the terminal. */
	endwin();

	construct_argument_list(&arguments, theprogram, tempfile_name);

	/* Fork a child process and run the given program in it. */
	if ((thepid = fork()) == 0) {
		execvp(arguments[0], arguments);

		/* Terminate the child if the program is not found. */
		exit(9);
	} else if (thepid < 0)
		return _("Could not fork");

	/* Block SIGWINCHes while waiting for the program to end,
	 * so nano doesn't get pushed past the wait(). */
	block_sigwinch(TRUE);
	wait(&program_status);
	block_sigwinch(FALSE);

	/* Restore the terminal state and reenter curses mode. */
	terminal_init();
	doupdate();

	if (!WIFEXITED(program_status) || WEXITSTATUS(program_status) > 2) {
		statusline(ALERT, _("Error invoking '%s'"), arguments[0]);
		return NULL;
	} else if (WEXITSTATUS(program_status) != 0)
		statusline(ALERT, _("Program '%s' complained"), arguments[0]);

	/* Stat the temporary file again. */
	stat(tempfile_name, &fileinfo);

	/* When the temporary file wasn't touched, say so and leave. */
	if ((long)fileinfo.st_mtim.tv_sec == timestamp_sec &&
				(long)fileinfo.st_mtim.tv_nsec == timestamp_nsec) {
		statusbar(_("Nothing changed"));
		return NULL;
	}

#ifndef NANO_TINY
	/* Replace the marked text (or entire text) with the corrected text. */
	if (spelling && openfile->mark) {
		bool upright = (openfile->mark->lineno < openfile->current->lineno ||
								(openfile->mark == openfile->current &&
								openfile->mark_x < openfile->current_x));
		ssize_t was_mark_lineno = openfile->mark->lineno;

		replaced = replace_buffer(tempfile_name, CUT, TRUE, "spelling correction");

		/* Adjust the end point of the marked region for any change in
		 * length of the region's last line. */
		if (upright)
			current_x_save = openfile->current_x;
		else
			openfile->mark_x = openfile->current_x;

		/* Restore the mark. */
		openfile->mark = line_from_number(was_mark_lineno);
	} else
#endif
		replaced = replace_buffer(tempfile_name, CUT_TO_EOF, FALSE,
					/* TRANSLATORS: The next two go with Undid/Redid messages. */
					(spelling ? N_("spelling correction") : N_("formatting")));

	/* Go back to the old position. */
	goto_line_posx(lineno_save, current_x_save);
	if (was_at_eol || openfile->current_x > strlen(openfile->current->data))
		openfile->current_x = strlen(openfile->current->data);
#ifndef NANO_TINY
	if (replaced)
		update_undo(COUPLE_END);
#endif
	openfile->placewewant = pww_save;
	adjust_viewport(STATIONARY);

	if (spelling)
		statusbar(_("Finished checking spelling"));
	else
		statusbar(_("Buffer has been processed"));

	return NULL;
}

/* Spell check the current file.  If an alternate spell checker is
 * specified, use it.  Otherwise, use the internal spell checker. */
void do_spell(void)
{
	FILE *stream;
	char *temp_name;
	unsigned stash[sizeof(flags) / sizeof(flags[0])];
	const char *result_msg;
	bool okay;

	if (in_restricted_mode())
		return;

	temp_name = safe_tempfile(&stream);

	if (temp_name == NULL) {
		statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
		return;
	}

	/* Save the settings of the global flags. */
	memcpy(stash, flags, sizeof(flags));

	/* Don't add an extra newline when writing out the (selected) text. */
	SET(NO_NEWLINES);

#ifndef NANO_TINY
	if (openfile->mark)
		okay = write_marked_file(temp_name, stream, TRUE, OVERWRITE);
	else
#endif
		okay = write_file(temp_name, stream, TRUE, OVERWRITE, TRUE);

	if (!okay) {
		statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
		free(temp_name);
		return;
	}

	blank_bottombars();

	if (alt_speller)
		result_msg = treat(temp_name, alt_speller, TRUE);
	else
		result_msg = do_int_speller(temp_name);

	unlink(temp_name);
	free(temp_name);

	/* Restore the settings of the global flags. */
	memcpy(flags, stash, sizeof(flags));

	/* Ensure the help lines will be redrawn and a selection is retained. */
	currmenu = MMOST;
	shift_held = TRUE;

	if (result_msg != NULL) {
		/* Avoid giving a failure reason of "Success". */
		if (errno == 0)
			statusline(ALERT, result_msg);
		else
			statusline(ALERT, _("%s: %s"), result_msg, strerror(errno));
	}
}
#endif /* ENABLE_SPELLER */

#ifdef ENABLE_COLOR
/* Run a linting program on the current buffer. */
void do_linter(void)
{
	char *lintings, *pointer, *onelint;
	long pipesize;
	size_t buffersize, bytesread, totalread;
	bool parsesuccess = FALSE;
	int lint_status, lint_fd[2];
	pid_t pid_lint;
	bool helpless = ISSET(NO_HELP);
	static char **lintargs = NULL;
	lintstruct *lints = NULL, *tmplint = NULL, *curlint = NULL;
	time_t last_wait = 0;

	if (in_restricted_mode())
		return;

	if (!openfile->syntax || !openfile->syntax->linter) {
		statusbar(_("No linter is defined for this type of file"));
		return;
	}

#ifndef NANO_TINY
	openfile->mark = NULL;
#endif
	edit_refresh();

	if (openfile->modified) {
		int choice = do_yesno_prompt(FALSE, _("Save modified buffer before linting?"));

		if (choice == -1) {
			statusbar(_("Cancelled"));
			return;
		} else if (choice == 1 && (do_writeout(FALSE, FALSE) != 1))
			return;
	}

	/* Create a pipe up front. */
	if (pipe(lint_fd) == -1) {
		statusline(ALERT, _("Could not create pipe"));
		return;
	}

	blank_bottombars();
	currmenu = MLINTER;
	statusbar(_("Invoking linter, please wait"));

	construct_argument_list(&lintargs, openfile->syntax->linter, openfile->filename);

	/* Fork a process to run the linter in. */
	if ((pid_lint = fork()) == 0) {
		/* Redirect standard output and standard error into the pipe. */
		if (dup2(lint_fd[1], STDOUT_FILENO) != STDOUT_FILENO)
			exit(7);
		if (dup2(lint_fd[1], STDERR_FILENO) != STDERR_FILENO)
			exit(8);

		close(lint_fd[0]);
		close(lint_fd[1]);

		/* Start the linter program; we are using $PATH. */
		execvp(lintargs[0], lintargs);

		/* This is only reached when the linter is not found. */
		exit(9);
	}

	/* Parent continues here. */
	close(lint_fd[1]);

	/* If the child process was not forked successfully... */
	if (pid_lint < 0) {
		close(lint_fd[0]);
		statusline(ALERT, _("Could not fork"));
		return;
	}

	/* Get the system pipe buffer size. */
	pipesize = fpathconf(lint_fd[0], _PC_PIPE_BUF);

	if (pipesize < 1) {
		close(lint_fd[0]);
		statusline(ALERT, _("Could not get size of pipe buffer"));
		return;
	}

	/* Read in the returned syntax errors. */
	totalread = 0;
	buffersize = pipesize + 1;
	lintings = charalloc(buffersize);
	pointer = lintings;

	while ((bytesread = read(lint_fd[0], pointer, pipesize)) > 0) {
		totalread += bytesread;
		buffersize += pipesize;
		lintings = charealloc(lintings, buffersize);
		pointer = lintings + totalread;
	}

	*pointer = '\0';
	close(lint_fd[0]);

	/* Process the linter output. */
	pointer = lintings;
	onelint = lintings;

	while (*pointer != '\0') {
		if ((*pointer == '\r') || (*pointer == '\n')) {
			*pointer = '\0';
			if (onelint != pointer) {
				char *filename = NULL, *linestr = NULL, *maybecol = NULL;
				char *message = copy_of(onelint);

				/* At the moment we handle the following formats:
				 *
				 * filenameorcategory:line:column:message (e.g. splint)
				 * filenameorcategory:line,column:message (e.g. pylint)
				 * filenameorcategory:line:message        (e.g. pyflakes) */
				if (strstr(message, ": ") != NULL) {
					filename = strtok(onelint, ":");
					if ((linestr = strtok(NULL, ":")) != NULL) {
						if ((maybecol = strtok(NULL, ":")) != NULL) {
							ssize_t tmplineno = 0, tmpcolno = 0;
							char *tmplinecol;

							tmplineno = strtol(linestr, NULL, 10);
							if (tmplineno <= 0) {
								pointer++;
								free(message);
								continue;
							}

							tmpcolno = strtol(maybecol, NULL, 10);
							/* Check if the middle field is in comma format. */
							if (tmpcolno <= 0) {
								strtok(linestr, ",");
								if ((tmplinecol = strtok(NULL, ",")) != NULL)
									tmpcolno = strtol(tmplinecol, NULL, 10);
								else
									tmpcolno = 1;
							}

							/* Nice.  We have a lint message we can use. */
							parsesuccess = TRUE;
							tmplint = curlint;
							curlint = nmalloc(sizeof(lintstruct));
							curlint->next = NULL;
							curlint->prev = tmplint;
							if (curlint->prev != NULL)
								curlint->prev->next = curlint;
							curlint->msg = copy_of(message);
							curlint->lineno = tmplineno;
							curlint->colno = tmpcolno;
							curlint->filename = copy_of(filename);

							if (lints == NULL)
								lints = curlint;
						}
					}
				}
				free(message);
			}
			onelint = pointer + 1;
		}
		pointer++;
	}

	free(lintings);

	/* Process the end of the linting process. */
	waitpid(pid_lint, &lint_status, 0);

	if (!WIFEXITED(lint_status) || WEXITSTATUS(lint_status) > 2) {
		statusline(ALERT, _("Error invoking '%s'"), openfile->syntax->linter);
		return;
	}

	if (!parsesuccess) {
		statusline(HUSH, _("Got 0 parsable lines from command: %s"),
						openfile->syntax->linter);
		return;
	}

	if (helpless && LINES > 4) {
		UNSET(NO_HELP);
		window_init();
	}

	/* Show that we are in the linter now. */
	titlebar(NULL);
	bottombars(MLINTER);

	tmplint = NULL;
	curlint = lints;

	while (TRUE) {
		int kbinput;
		functionptrtype func;
		struct stat lintfileinfo;

		if (stat(curlint->filename, &lintfileinfo) != -1 &&
					(openfile->current_stat == NULL ||
					openfile->current_stat->st_ino != lintfileinfo.st_ino)) {
#ifdef ENABLE_MULTIBUFFER
			const openfilestruct *started_at = openfile;

			openfile = openfile->next;
			while (openfile != started_at && (openfile->current_stat == NULL ||
						openfile->current_stat->st_ino != lintfileinfo.st_ino))
				openfile = openfile->next;

			if (openfile->current_stat == NULL ||
						openfile->current_stat->st_ino != lintfileinfo.st_ino) {
				char *msg = charalloc(1024 + strlen(curlint->filename));
				int choice;

				sprintf(msg, _("This message is for unopened file %s,"
							" open it in a new buffer?"), curlint->filename);
				choice = do_yesno_prompt(FALSE, msg);
				currmenu = MLINTER;
				free(msg);

				if (choice == -1) {
					statusbar(_("Cancelled"));
					break;
				} else if (choice == 1) {
					open_buffer(curlint->filename, TRUE);
				} else {
#endif
					char *dontwantfile = copy_of(curlint->filename);
					lintstruct *restlint = NULL;

					while (curlint != NULL) {
						if (strcmp(curlint->filename, dontwantfile) == 0) {
							if (curlint == lints)
								lints = curlint->next;
							else
								curlint->prev->next = curlint->next;
							if (curlint->next != NULL)
								curlint->next->prev = curlint->prev;
							tmplint = curlint;
							curlint = curlint->next;
							free(tmplint->msg);
							free(tmplint->filename);
							free(tmplint);
						} else {
							if (restlint == NULL)
								restlint = curlint;
							curlint = curlint->next;
						}
					}

					free(dontwantfile);

					if (restlint == NULL) {
						statusbar(_("No messages for this file"));
						break;
					} else {
						curlint = restlint;
						continue;
					}
#ifdef ENABLE_MULTIBUFFER
				}
			}
#endif
		}

		if (tmplint != curlint) {
			goto_line_posx(curlint->lineno, curlint->colno - 1);
			titlebar(NULL);
			adjust_viewport(CENTERING);
#ifdef ENABLE_LINENUMBERS
			confirm_margin();
#endif
			edit_refresh();
			statusline(NOTICE, curlint->msg);
			bottombars(MLINTER);
		}

		/* Place the cursor to indicate the affected line. */
		place_the_cursor();
		wnoutrefresh(edit);

		kbinput = get_kbinput(bottomwin, VISIBLE);

#ifndef NANO_TINY
		if (kbinput == KEY_WINCH)
			continue;
#endif
		func = func_from_key(&kbinput);
		tmplint = curlint;

		if (func == do_cancel || func == do_enter) {
			wipe_statusbar();
			break;
		} else if (func == do_help) {
			tmplint = NULL;
			do_help();
		} else if (func == do_page_up || func == do_prev_block) {
			if (curlint->prev != NULL)
				curlint = curlint->prev;
			else if (last_wait != time(NULL)) {
				statusbar(_("At first message"));
				beep();
				napms(600);
				last_wait = time(NULL);
				statusline(NOTICE, curlint->msg);
			}
		} else if (func == do_page_down || func == do_next_block) {
			if (curlint->next != NULL)
				curlint = curlint->next;
			else if (last_wait != time(NULL)) {
				statusbar(_("At last message"));
				beep();
				napms(600);
				last_wait = time(NULL);
				statusline(NOTICE, curlint->msg);
			}
		} else
			beep();
	}

	for (curlint = lints; curlint != NULL;) {
		tmplint = curlint;
		curlint = curlint->next;
		free(tmplint->msg);
		free(tmplint->filename);
		free(tmplint);
	}

	if (helpless) {
		SET(NO_HELP);
		window_init();
		refresh_needed = TRUE;
	}

	currmenu = MMOST;
	titlebar(NULL);
}

#ifdef ENABLE_SPELLER
/* Run a manipulation program on the contents of the buffer. */
void do_formatter(void)
{
	FILE *stream;
	char *temp_name;
	bool okay = FALSE;
	const char *result_msg;

	if (in_restricted_mode())
		return;

	if (!openfile->syntax || !openfile->syntax->formatter) {
		statusbar(_("No formatter is defined for this type of file"));
		return;
	}

	temp_name = safe_tempfile(&stream);

	if (temp_name != NULL)
		okay = write_file(temp_name, stream, TRUE, OVERWRITE, TRUE);

	if (!okay) {
		statusline(ALERT, _("Error writing temp file: %s"), strerror(errno));
		free(temp_name);
		return;
	}

	result_msg = treat(temp_name, openfile->syntax->formatter, FALSE);

	if (result_msg != NULL)
		statusline(ALERT, result_msg);

	unlink(temp_name);
	free(temp_name);
}
#endif /* ENABLE_SPELLER */
#endif /* ENABLE_COLOR */

#ifndef NANO_TINY
/* Our own version of "wc".  Note that its character counts are in
 * multibyte characters instead of single-byte characters. */
void do_wordlinechar_count(void)
{
	linestruct *was_current = openfile->current;
	size_t was_x = openfile->current_x;
	size_t words = 0, chars = 0;
	ssize_t lines = 0;
	linestruct *top, *bot;
	size_t top_x, bot_x;

	/* If the mark is on, partition the buffer so that it
	 * contains only the marked text, and turn the mark off. */
	if (openfile->mark) {
		get_region((const linestruct **)&top, &top_x,
					(const linestruct **)&bot, &bot_x, NULL);
		partition_buffer(top, top_x, bot, bot_x);
	}

	/* Start at the top of the file. */
	openfile->current = openfile->filetop;
	openfile->current_x = 0;

	/* Keep moving to the next word (counting punctuation characters as
	 * part of a word, as "wc -w" does), without updating the screen,
	 * until we reach the end of the file, incrementing the total word
	 * count whenever we're on a word just before moving. */
	while (openfile->current != openfile->filebot ||
		openfile->current->data[openfile->current_x] != '\0') {
		if (do_next_word(FALSE, TRUE))
			words++;
	}

	/* Get the number of lines, similar to what "wc -l" gives. */
	lines = openfile->filebot->lineno - openfile->filetop->lineno +
					((openfile->filebot->data[0] == '\0') ? 0 : 1);

	/* Get the number of multibyte characters, similar to "wc -c". */
	if (openfile->mark) {
		chars = get_totsize(openfile->filetop, openfile->filebot);
		unpartition_buffer();
	} else
		chars = openfile->totsize;

	/* Restore where we were. */
	openfile->current = was_current;
	openfile->current_x = was_x;

	/* Display the total word, line, and character counts on the status bar. */
	statusline(HUSH, _("%sWords: %zu  Lines: %zd  Chars: %zu"), openfile->mark ?
						_("In Selection:  ") : "", words, lines, chars);
}
#endif /* !NANO_TINY */

/* Get verbatim input. */
void do_verbatim_input(void)
{
	int *kbinput;
	size_t count;
	char *keycodes;

	/* TRANSLATORS: This is displayed when the next keystroke will be
	 * inserted verbatim. */
	statusbar(_("Verbatim Input"));
	place_the_cursor();

	/* Read in all the verbatim characters. */
	kbinput = get_verbatim_kbinput(edit, &count);

	/* Unsuppress cursor-position display or blank the status bar. */
	if (ISSET(CONSTANT_SHOW))
		suppress_cursorpos = FALSE;
	else
		wipe_statusbar();

	keycodes = charalloc(count + 1);

	for (size_t i = 0; i < count; i++)
		keycodes[i] = (char)kbinput[i];
	keycodes[count] = '\0';

	/* Insert the keystroke verbatim, without filtering control characters. */
	inject(keycodes, count, FALSE);

	free(keycodes);
	free(kbinput);
}

#ifdef ENABLE_WORDCOMPLETION
/* Return a copy of the found completion candidate. */
char *copy_completion(char *text)
{
	char *word;
	size_t length = 0, index = 0;

	/* Find the end of the candidate word to get its length. */
	while (is_word_mbchar(&text[length], FALSE))
		length = step_right(text, length);

	/* Now copy this candidate to a new string. */
	word = charalloc(length + 1);
	while (index < length)
		word[index++] = *(text++);
	word[index] = '\0';

	return word;
}

/* Look at the fragment the user has typed, then search the current buffer for
 * the first word that starts with this fragment, and tentatively complete the
 * fragment.  If the user types 'Complete' again, search and paste the next
 * possible completion. */
void complete_a_word(void)
{
	char *shard, *completion = NULL;
	size_t start_of_shard, shard_length = 0;
	size_t i = 0, j = 0;
	completion_word *some_word;
#ifdef ENABLE_WRAPPING
	bool was_set_wrapping = ISSET(BREAK_LONG_LINES);
#endif

	/* If this is a fresh completion attempt... */
	if (pletion_line == NULL) {
		/* Clear the list of words of a previous completion run. */
		while (list_of_completions != NULL) {
			completion_word *dropit = list_of_completions;
			list_of_completions = list_of_completions->next;
			free(dropit->word);
			free(dropit);
		}

		/* Prevent a completion from being merged with typed text. */
		openfile->last_action = OTHER;

		/* Initialize the starting point for searching. */
		pletion_line = openfile->filetop;
		pletion_x = 0;

		/* Wipe the "No further matches" message. */
		wipe_statusbar();
	} else {
		/* Remove the attempted completion from the buffer. */
		do_undo();
	}

	/* Find the start of the fragment that the user typed. */
	start_of_shard = openfile->current_x;
	while (start_of_shard > 0) {
		size_t oneleft = step_left(openfile->current->data, start_of_shard);

		if (!is_word_mbchar(&openfile->current->data[oneleft], FALSE))
			break;
		start_of_shard = oneleft;
	}

	/* If there is no word fragment before the cursor, do nothing. */
	if (start_of_shard == openfile->current_x) {
		/* TRANSLATORS: Shown when no text is directly left of the cursor. */
		statusbar(_("No word fragment"));
		pletion_line = NULL;
		return;
	}

	shard = charalloc(openfile->current_x - start_of_shard + 1);

	/* Copy the fragment that has to be searched for. */
	while (start_of_shard < openfile->current_x)
		shard[shard_length++] = openfile->current->data[start_of_shard++];
	shard[shard_length] = '\0';

	/* Run through all of the lines in the buffer, looking for shard. */
	while (pletion_line != NULL) {
		ssize_t threshold = strlen(pletion_line->data) - shard_length - 1;
				/* The point where we can stop searching for shard. */

		/* Traverse the whole line, looking for shard. */
		for (i = pletion_x; (ssize_t)i < threshold; i++) {
			/* If the first byte doesn't match, run on. */
			if (pletion_line->data[i] != shard[0])
				continue;

			/* Compare the rest of the bytes in shard. */
			for (j = 1; j < shard_length; j++)
				if (pletion_line->data[i + j] != shard[j])
					break;

			/* If not all of the bytes matched, continue searching. */
			if (j < shard_length)
				continue;

			/* If the found match is not /longer/ than shard, skip it. */
			if (!is_word_mbchar(&pletion_line->data[i + j], FALSE))
				continue;

			/* If the match is not a separate word, skip it. */
			if (i > 0 && is_word_mbchar(&pletion_line->data[
								step_left(pletion_line->data, i)], FALSE))
				continue;

			/* If this match is the shard itself, ignore it. */
			if (pletion_line == openfile->current &&
						i == openfile->current_x - shard_length)
				continue;

			completion = copy_completion(pletion_line->data + i);

			/* Look among earlier attempted completions for a duplicate. */
			some_word = list_of_completions;
			while (some_word && strcmp(some_word->word, completion) != 0)
				some_word = some_word->next;

			/* If we've already tried this word, skip it. */
			if (some_word != NULL) {
				free(completion);
				continue;
			}

			/* Add the found word to the list of completions. */
			some_word = (completion_word *)nmalloc(sizeof(completion_word));
			some_word->word = completion;
			some_word->next = list_of_completions;
			list_of_completions = some_word;

#ifdef ENABLE_WRAPPING
			/* Temporarily disable wrapping so only one undo item is added. */
			UNSET(BREAK_LONG_LINES);
#endif
			/* Inject the completion into the buffer. */
			inject(&completion[shard_length],
						strlen(completion) - shard_length, TRUE);
#ifdef ENABLE_WRAPPING
			/* If needed, reenable wrapping and wrap the current line. */
			if (was_set_wrapping) {
				SET(BREAK_LONG_LINES);
				do_wrap();
			}
#endif
			/* Set the position for a possible next search attempt. */
			pletion_x = ++i;

			free(shard);
			return;
		}

		pletion_line = pletion_line->next;
		pletion_x = 0;
	}

	/* The search has reached the end of the file. */
	if (list_of_completions != NULL) {
		statusline(ALERT, _("No further matches"));
		refresh_needed = TRUE;
	} else
		/* TRANSLATORS: Shown when there are zero possible completions. */
		statusline(ALERT, _("No matches"));

	free(shard);
}
#endif /* ENABLE_WORDCOMPLETION */
