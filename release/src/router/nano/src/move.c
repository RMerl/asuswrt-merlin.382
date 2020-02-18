/**************************************************************************
 *   move.c  --  This file is part of GNU nano.                           *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2019 Free Software Foundation, Inc.    *
 *   Copyright (C) 2014-2018 Benno Schulenberg                            *
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

#include <string.h>

/* Move to the first line of the file. */
void to_first_line(void)
{
	openfile->current = openfile->filetop;
	openfile->current_x = 0;
	openfile->placewewant = 0;

	refresh_needed = TRUE;
}

/* Move to the last line of the file. */
void to_last_line(void)
{
	openfile->current = openfile->filebot;
	openfile->current_x = strlen(openfile->filebot->data);
	openfile->placewewant = xplustabs();

	/* Set the last line of the screen as the target for the cursor. */
	openfile->current_y = editwinrows - 1;

	refresh_needed = TRUE;
	focusing = FALSE;
}

/* Determine the actual current chunk and the target column. */
void get_edge_and_target(size_t *leftedge, size_t *target_column)
{
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		size_t shim = editwincols * (1 + (tabsize / editwincols));

		*leftedge = leftedge_for(xplustabs(), openfile->current);
		*target_column = (openfile->placewewant + shim - *leftedge) % editwincols;
	} else
#endif
	{
		*leftedge = 0;
		*target_column = openfile->placewewant;
	}
}

/* Return the index in line->data that corresponds to the given column on the
 * chunk that starts at the given leftedge.  If the target column has landed
 * on a tab, prevent the cursor from falling back a row when moving forward,
 * or from skipping a row when moving backward, by incrementing the index. */
size_t proper_x(linestruct *line, size_t *leftedge, bool forward,
				size_t column, bool *shifted)
{
	size_t index = actual_x(line->data, column);

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP) && line->data[index] == '\t' &&
				((forward && wideness(line->data, index) < *leftedge) ||
				(!forward && column / tabsize == (*leftedge - 1) / tabsize &&
				column / tabsize < (*leftedge + editwincols - 1) / tabsize))) {
		index++;

		if (shifted != NULL)
			*shifted = TRUE;
	}

	if (ISSET(SOFTWRAP))
		*leftedge = leftedge_for(wideness(line->data, index), line);
#endif

	return index;
}

/* Adjust the values for current_x and placewewant in case we have landed in
 * the middle of a tab that crosses a row boundary. */
void set_proper_index_and_pww(size_t *leftedge, size_t target, bool forward)
{
	bool shifted = FALSE;
	size_t was_edge = *leftedge;

	openfile->current_x = proper_x(openfile->current, leftedge, forward,
						actual_last_column(*leftedge, target), &shifted);

	/* If the index was incremented, try going to the target column. */
	if (shifted || *leftedge < was_edge)
		openfile->current_x = proper_x(openfile->current, leftedge, forward,
						actual_last_column(*leftedge, target), &shifted);

	openfile->placewewant = *leftedge + target;
}

/* Move up nearly one screenful. */
void do_page_up(void)
{
	int mustmove = (editwinrows < 3) ? 1 : editwinrows - 2;
	size_t leftedge, target_column;

	/* If we're not in smooth scrolling mode, put the cursor at the
	 * beginning of the top line of the edit window, as Pico does. */
	if (ISSET(JUMPY_SCROLLING)) {
		openfile->current = openfile->edittop;
		leftedge = openfile->firstcolumn;
		openfile->current_y = 0;
		target_column = 0;
	} else
		get_edge_and_target(&leftedge, &target_column);

	/* Move up the required number of lines or chunks.  If we can't, we're
	 * at the top of the file, so put the cursor there and get out. */
	if (go_back_chunks(mustmove, &openfile->current, &leftedge) > 0) {
		to_first_line();
		return;
	}

	set_proper_index_and_pww(&leftedge, target_column, FALSE);

	/* Move the viewport so that the cursor stays immobile, if possible. */
	adjust_viewport(STATIONARY);
	refresh_needed = TRUE;
}

/* Move down nearly one screenful. */
void do_page_down(void)
{
	int mustmove = (editwinrows < 3) ? 1 : editwinrows - 2;
	size_t leftedge, target_column;

	/* If we're not in smooth scrolling mode, put the cursor at the
	 * beginning of the top line of the edit window, as Pico does. */
	if (ISSET(JUMPY_SCROLLING)) {
		openfile->current = openfile->edittop;
		leftedge = openfile->firstcolumn;
		openfile->current_y = 0;
		target_column = 0;
	} else
		get_edge_and_target(&leftedge, &target_column);

	/* Move down the required number of lines or chunks.  If we can't, we're
	 * at the bottom of the file, so put the cursor there and get out. */
	if (go_forward_chunks(mustmove, &openfile->current, &leftedge) > 0) {
		to_last_line();
		return;
	}

	set_proper_index_and_pww(&leftedge, target_column, TRUE);

	/* Move the viewport so that the cursor stays immobile, if possible. */
	adjust_viewport(STATIONARY);
	refresh_needed = TRUE;
}

#ifdef ENABLE_JUSTIFY
/* Move to the first beginning of a paragraph before the current line. */
void do_para_begin(linestruct **line)
{
	if ((*line)->prev != NULL)
		*line = (*line)->prev;

	while (!begpar(*line, 0))
		*line = (*line)->prev;
}

/* Move down to the last line of the first found paragraph. */
void do_para_end(linestruct **line)
{
	while ((*line)->next != NULL && !inpar(*line))
		*line = (*line)->next;

	while ((*line)->next != NULL && inpar((*line)->next) &&
									!begpar((*line)->next, 0))
		*line = (*line)->next;
}

/* Move up to first start of a paragraph before the current line. */
void do_para_begin_void(void)
{
	linestruct *was_current = openfile->current;

	do_para_begin(&openfile->current);
	openfile->current_x = 0;

	edit_redraw(was_current, CENTERING);
}

/* Move down to just after the first found end of a paragraph. */
void do_para_end_void(void)
{
	linestruct *was_current = openfile->current;

	do_para_end(&openfile->current);

	/* Step beyond the last line of the paragraph, if possible;
	 * otherwise, move to the end of the line. */
	if (openfile->current->next != NULL) {
		openfile->current = openfile->current->next;
		openfile->current_x = 0;
	} else
		openfile->current_x = strlen(openfile->current->data);

	edit_redraw(was_current, CENTERING);
}
#endif /* ENABLE_JUSTIFY */

/* Move to the preceding block of text. */
void do_prev_block(void)
{
	linestruct *was_current = openfile->current;
	bool is_text = FALSE, seen_text = FALSE;

	/* Skip backward until first blank line after some nonblank line(s). */
	while (openfile->current->prev != NULL && (!seen_text || is_text)) {
		openfile->current = openfile->current->prev;
		is_text = !white_string(openfile->current->data);
		seen_text = seen_text || is_text;
	}

	/* Step forward one line again if this one is blank. */
	if (openfile->current->next != NULL &&
				white_string(openfile->current->data))
		openfile->current = openfile->current->next;

	openfile->current_x = 0;
	edit_redraw(was_current, CENTERING);
}

/* Move to the next block of text. */
void do_next_block(void)
{
	linestruct *was_current = openfile->current;
	bool is_white = white_string(openfile->current->data);
	bool seen_white = is_white;

	/* Skip forward until first nonblank line after some blank line(s). */
	while (openfile->current->next != NULL && (!seen_white || is_white)) {
		openfile->current = openfile->current->next;
		is_white = white_string(openfile->current->data);
		seen_white = seen_white || is_white;
	}

	openfile->current_x = 0;
	edit_redraw(was_current, CENTERING);
}

/* Move to the previous word.  If allow_punct is TRUE, treat punctuation
 * as part of a word. */
void do_prev_word(bool allow_punct)
{
	bool seen_a_word = FALSE, step_forward = FALSE;

	/* Move backward until we pass over the start of a word. */
	while (TRUE) {
		/* If at the head of a line, move to the end of the preceding one. */
		if (openfile->current_x == 0) {
			if (openfile->current->prev == NULL)
				break;
			openfile->current = openfile->current->prev;
			openfile->current_x = strlen(openfile->current->data);
		}

		/* Step back one character. */
		openfile->current_x = step_left(openfile->current->data,
												openfile->current_x);

		if (is_word_mbchar(openfile->current->data + openfile->current_x,
								allow_punct)) {
			seen_a_word = TRUE;
			/* If at the head of a line now, this surely is a word start. */
			if (openfile->current_x == 0)
				break;
		} else if (seen_a_word) {
			/* This is space now: we've overshot the start of the word. */
			step_forward = TRUE;
			break;
		}
	}

	if (step_forward)
		/* Move one character forward again to sit on the start of the word. */
		openfile->current_x = step_right(openfile->current->data,
												openfile->current_x);
}

/* Move to the next word.  If after_ends is TRUE, stop at the ends of words
 * instead of their beginnings.  If allow_punct is TRUE, treat punctuation as
 * part of a word.  Return TRUE if we started on a word, and FALSE otherwise. */
bool do_next_word(bool after_ends, bool allow_punct)
{
	bool started_on_word = is_word_mbchar(openfile->current->data +
								openfile->current_x, allow_punct);
	bool seen_space = !started_on_word;
#ifndef NANO_TINY
	bool seen_word = started_on_word;
#endif

	/* Move forward until we reach the start of a word. */
	while (TRUE) {
		/* If at the end of a line, move to the beginning of the next one. */
		if (openfile->current->data[openfile->current_x] == '\0') {
			/* When at end of file, stop. */
			if (openfile->current->next == NULL)
				break;
			openfile->current = openfile->current->next;
			openfile->current_x = 0;
			seen_space = TRUE;
		} else {
			/* Step forward one character. */
			openfile->current_x = step_right(openfile->current->data,
												openfile->current_x);
		}

#ifndef NANO_TINY
		if (after_ends) {
			/* If this is a word character, continue; else it's a separator,
			 * and if we've already seen a word, then it's a word end. */
			if (is_word_mbchar(openfile->current->data + openfile->current_x,
								allow_punct))
				seen_word = TRUE;
			else if (seen_word)
				break;
		} else
#endif
		{
			/* If this is not a word character, then it's a separator; else
			 * if we've already seen a separator, then it's a word start. */
			if (!is_word_mbchar(openfile->current->data + openfile->current_x,
								allow_punct))
				seen_space = TRUE;
			else if (seen_space)
				break;
		}
	}

	return started_on_word;
}

/* Move to the previous word in the file, treating punctuation as part of a
 * word if the WORD_BOUNDS flag is set, and update the screen afterwards. */
void do_prev_word_void(void)
{
	linestruct *was_current = openfile->current;

	do_prev_word(ISSET(WORD_BOUNDS));

	edit_redraw(was_current, FLOWING);
}

/* Move to the next word in the file.  If the AFTER_ENDS flag is set, stop
 * at word ends instead of beginnings.  If the WORD_BOUNDS flag is set, treat
 * punctuation as part of a word.  Update the screen afterwards. */
void do_next_word_void(void)
{
	linestruct *was_current = openfile->current;

	do_next_word(ISSET(AFTER_ENDS), ISSET(WORD_BOUNDS));

	edit_redraw(was_current, FLOWING);
}

/* Move to the beginning of the current line (or softwrapped chunk).
 * When enabled, do a smart home.  When softwrapping, go the beginning
 * of the full line when already at the start of a chunk. */
void do_home(void)
{
	linestruct *was_current = openfile->current;
	size_t was_column = xplustabs();
	bool moved_off_chunk = TRUE;
#ifndef NANO_TINY
	bool moved = FALSE;
	size_t leftedge = 0, leftedge_x = 0;

	if (ISSET(SOFTWRAP)) {
		leftedge = leftedge_for(was_column, openfile->current);
		leftedge_x = proper_x(openfile->current, &leftedge, FALSE, leftedge,
								NULL);
	}

	if (ISSET(SMART_HOME)) {
		size_t indent_x = indent_length(openfile->current->data);

		if (openfile->current->data[indent_x] != '\0') {
			/* If we're exactly on the indent, move fully home.  Otherwise,
			 * when not softwrapping or not after the first nonblank chunk,
			 * move to the first nonblank character. */
			if (openfile->current_x == indent_x) {
				openfile->current_x = 0;
				moved = TRUE;
			} else if (!ISSET(SOFTWRAP) || leftedge_x <= indent_x) {
				openfile->current_x = indent_x;
				moved = TRUE;
			}
		}
	}

	if (!moved && ISSET(SOFTWRAP)) {
		/* If already at the left edge of the screen, move fully home.
		 * Otherwise, move to the left edge. */
		if (openfile->current_x == leftedge_x)
			openfile->current_x = 0;
		else {
			openfile->current_x = leftedge_x;
			openfile->placewewant = leftedge;
			moved_off_chunk = FALSE;
		}
	} else if (!moved)
#endif
		openfile->current_x = 0;

	if (moved_off_chunk)
		openfile->placewewant = xplustabs();

	/* If we changed chunk, we might be offscreen.  Otherwise,
	 * update current if the mark is on or we changed "page". */
	if (ISSET(SOFTWRAP) && moved_off_chunk)
		edit_redraw(was_current, FLOWING);
	else if (line_needs_update(was_column, openfile->placewewant))
		update_line(openfile->current, openfile->current_x);
}

/* Move to the end of the current line (or softwrapped chunk).
 * When softwrapping and alredy at the end of a chunk, go to the
 * end of the full line. */
void do_end(void)
{
	linestruct *was_current = openfile->current;
	size_t was_column = xplustabs();
	size_t line_len = strlen(openfile->current->data);
	bool moved_off_chunk = TRUE;

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		bool last_chunk = FALSE;
		size_t leftedge = leftedge_for(was_column, openfile->current);
		size_t rightedge = get_softwrap_breakpoint(openfile->current->data,
												leftedge, &last_chunk);
		size_t rightedge_x;

		/* If we're on the last chunk, we're already at the end of the line.
		 * Otherwise, we're one column past the end of the line.  Shifting
		 * backwards one column might put us in the middle of a multi-column
		 * character, but actual_x() will fix that. */
		if (!last_chunk)
			rightedge--;

		rightedge_x = actual_x(openfile->current->data, rightedge);

		/* If already at the right edge of the screen, move fully to
		 * the end of the line.  Otherwise, move to the right edge. */
		if (openfile->current_x == rightedge_x)
			openfile->current_x = line_len;
		else {
			openfile->current_x = rightedge_x;
			openfile->placewewant = rightedge;
			moved_off_chunk = FALSE;
		}
	} else
#endif
		openfile->current_x = line_len;

	if (moved_off_chunk)
		openfile->placewewant = xplustabs();

	/* If we changed chunk, we might be offscreen.  Otherwise,
	 * update current if the mark is on or we changed "page". */
	if (ISSET(SOFTWRAP) && moved_off_chunk)
		edit_redraw(was_current, FLOWING);
	else if (line_needs_update(was_column, openfile->placewewant))
		update_line(openfile->current, openfile->current_x);
}

/* Move the cursor to the preceding line or chunk. */
void do_up(void)
{
	linestruct *was_current = openfile->current;
	size_t leftedge, target_column;

	get_edge_and_target(&leftedge, &target_column);

	/* If we can't move up one line or chunk, we're at top of file. */
	if (go_back_chunks(1, &openfile->current, &leftedge) > 0)
		return;

	set_proper_index_and_pww(&leftedge, target_column, FALSE);

	if (openfile->current_y == 0 && !ISSET(JUMPY_SCROLLING))
		edit_scroll(BACKWARD);
	else
		edit_redraw(was_current, FLOWING);

	/* <Up> should not change placewewant, so restore it. */
	openfile->placewewant = leftedge + target_column;
}

/* Move the cursor to next line or chunk. */
void do_down(void)
{
	linestruct *was_current = openfile->current;
	size_t leftedge, target_column;

	get_edge_and_target(&leftedge, &target_column);

	/* If we can't move down one line or chunk, we're at bottom of file. */
	if (go_forward_chunks(1, &openfile->current, &leftedge) > 0)
		return;

	set_proper_index_and_pww(&leftedge, target_column, TRUE);

	if (openfile->current_y == editwinrows - 1 && !ISSET(JUMPY_SCROLLING))
		edit_scroll(FORWARD);
	else
		edit_redraw(was_current, FLOWING);

	/* <Down> should not change placewewant, so restore it. */
	openfile->placewewant = leftedge + target_column;
}

#if !defined(NANO_TINY) || defined(ENABLE_HELP)
/* Scroll up one line or chunk without scrolling the cursor. */
void do_scroll_up(void)
{
	/* When the top of the file is onscreen, we can't scroll. */
	if (openfile->edittop->prev == NULL && openfile->firstcolumn == 0)
		return;

	if (openfile->current_y == editwinrows - 1)
		do_up();

	if (editwinrows > 1)
		edit_scroll(BACKWARD);
}

/* Scroll down one line or chunk without scrolling the cursor. */
void do_scroll_down(void)
{
	if (openfile->current_y == 0)
		do_down();

	if (editwinrows > 1 && (openfile->edittop->next != NULL
#ifndef NANO_TINY
				|| chunk_for(openfile->firstcolumn, openfile->edittop) <
					number_of_chunks_in(openfile->edittop)
#endif
										))
		edit_scroll(FORWARD);
}
#endif

/* Move left one character. */
void do_left(void)
{
	linestruct *was_current = openfile->current;

	if (openfile->current_x > 0)
		openfile->current_x = step_left(openfile->current->data,
												openfile->current_x);
	else if (openfile->current != openfile->filetop) {
		openfile->current = openfile->current->prev;
		openfile->current_x = strlen(openfile->current->data);
	}

	edit_redraw(was_current, FLOWING);
}

/* Move right one character. */
void do_right(void)
{
	linestruct *was_current = openfile->current;

	if (openfile->current->data[openfile->current_x] != '\0')
		openfile->current_x = step_right(openfile->current->data,
												openfile->current_x);
	else if (openfile->current != openfile->filebot) {
		openfile->current = openfile->current->next;
		openfile->current_x = 0;
	}

	edit_redraw(was_current, FLOWING);
}
