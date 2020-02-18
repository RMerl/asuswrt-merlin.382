/**************************************************************************
 *   prompt.c  --  This file is part of GNU nano.                         *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2020 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016, 2018 Benno Schulenberg                           *
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

static char *prompt = NULL;
		/* The prompt string used for status-bar questions. */
static size_t typing_x = HIGHEST_POSITIVE;
		/* The cursor position in answer. */

#ifdef ENABLE_MOUSE
/* Handle a mouse click on the status-bar prompt or the shortcut list. */
int do_statusbar_mouse(void)
{
	int click_row, click_col;
	int retval = get_mouseinput(&click_row, &click_col, TRUE);

	/* We can click on the status-bar window text to move the cursor. */
	if (retval == 0 && wmouse_trafo(bottomwin, &click_row, &click_col, FALSE)) {
		size_t start_col = breadth(prompt) + 2;

		/* Move to where the click occurred. */
		if (click_row == 0 && click_col >= start_col)
			typing_x = actual_x(answer,
							get_statusbar_page_start(start_col, start_col +
							wideness(answer, typing_x)) + click_col - start_col);
	}

	return retval;
}
#endif

/* Read in a keystroke, interpret it if it is a shortcut or toggle, and
 * return it.  Set finished to TRUE if we're done after running
 * or trying to run a function associated with a shortcut key. */
int do_statusbar_input(bool *finished)
{
	int input;
		/* The character we read in. */
	static int *kbinput = NULL;
		/* The input buffer. */
	static size_t kbinput_len = 0;
		/* The length of the input buffer. */
	const keystruct *shortcut;

	*finished = FALSE;

	/* Read in a character. */
	input = get_kbinput(bottomwin, VISIBLE);

#ifndef NANO_TINY
	if (input == KEY_WINCH)
		return KEY_WINCH;
#endif

#ifdef ENABLE_MOUSE
	/* If we got a mouse click and it was on a shortcut, read in the
	 * shortcut character. */
	if (input == KEY_MOUSE) {
		if (do_statusbar_mouse() == 1)
			input = get_kbinput(bottomwin, BLIND);
		else
			return ERR;
	}
#endif

	/* Check for a shortcut in the current list. */
	shortcut = get_shortcut(&input);

	/* If we got a non-high-bit control key, a meta key sequence, or a
	 * function key, and it's not a shortcut or toggle, throw it out. */
	if (shortcut == NULL) {
		if (is_ascii_cntrl_char(input) || meta_key || !is_byte(input)) {
			beep();
			input = ERR;
		}
	}

	/* If the keystroke isn't a shortcut nor a toggle, it's a normal text
	 * character: add the it to the input buffer, when allowed. */
	if (input != ERR && shortcut == NULL) {
		/* Only accept input when not in restricted mode, or when not at
		 * the "Write File" prompt, or when there is no filename yet. */
		if (!ISSET(RESTRICTED) || currmenu != MWRITEFILE ||
						openfile->filename[0] == '\0') {
			kbinput_len++;
			kbinput = (int *)nrealloc(kbinput, kbinput_len * sizeof(int));
			kbinput[kbinput_len - 1] = input;
		}
	}

	/* If we got a shortcut, or if there aren't any other keystrokes waiting
	 * after the one we read in, we need to insert all the characters in the
	 * input buffer (if not empty) into the answer. */
	if ((shortcut || get_key_buffer_len() == 0) && kbinput != NULL) {
		/* Inject all characters in the input buffer at once, filtering out
		 * control characters. */
		inject_into_answer(kbinput, kbinput_len, TRUE);

		/* Empty the input buffer. */
		kbinput_len = 0;
		free(kbinput);
		kbinput = NULL;
	}

	if (shortcut) {
		if (shortcut->func == do_tab || shortcut->func == do_enter)
			;
#ifdef ENABLE_HISTORIES
		else if (shortcut->func == get_history_older_void ||
					shortcut->func == get_history_newer_void)
			;
#endif
		else if (shortcut->func == do_left)
			do_statusbar_left();
		else if (shortcut->func == do_right)
			do_statusbar_right();
#ifndef NANO_TINY
		else if (shortcut->func == do_prev_word_void)
			do_statusbar_prev_word();
		else if (shortcut->func == do_next_word_void)
			do_statusbar_next_word();
#endif
		else if (shortcut->func == do_home)
			do_statusbar_home();
		else if (shortcut->func == do_end)
			do_statusbar_end();
		/* When in restricted mode at the "Write File" prompt and the
		 * filename isn't blank, disallow any input and deletion. */
		else if (ISSET(RESTRICTED) && currmenu == MWRITEFILE &&
								openfile->filename[0] != '\0' &&
								(shortcut->func == do_verbatim_input ||
								shortcut->func == cut_text ||
								shortcut->func == paste_text ||
								shortcut->func == do_delete ||
								shortcut->func == do_backspace))
			;
#ifdef ENABLE_NANORC
		else if (shortcut->func == (functionptrtype)implant)
			implant(shortcut->expansion);
#endif
		else if (shortcut->func == do_verbatim_input)
			do_statusbar_verbatim_input();
		else if (shortcut->func == cut_text)
			do_statusbar_cut_text();
		else if (shortcut->func == do_delete)
			do_statusbar_delete();
		else if (shortcut->func == do_backspace)
			do_statusbar_backspace();
		else if (shortcut->func == paste_text) {
			if (cutbuffer != NULL)
				do_statusbar_uncut_text();
		} else {
			/* Handle any other shortcut in the current menu, setting finished
			 * to TRUE to indicate that we're done after running or trying to
			 * run its associated function. */
			if (!ISSET(VIEW_MODE) || okay_for_view(shortcut))
				shortcut->func();
			*finished = TRUE;
		}
	}

	return input;
}

/* The user typed input_len multibyte characters.  Add them to the answer,
 * filtering out ASCII control characters if filtering is TRUE. */
void inject_into_answer(int *the_input, size_t input_len, bool filtering)
{
	char *output = charalloc(input_len + 1);
	char onechar[MAXCHARLEN];
	size_t charlen, i, j = 0;

	/* Copy the typed stuff so it can be treated. */
	for (i = 0; i < input_len; i++)
		output[i] = (char)the_input[i];
	output[i] = '\0';

	while (j < input_len) {
		/* Encode any NUL byte as 0x0A. */
		if (output[j] == '\0')
			output[j] = '\n';

		/* Interpret the next multibyte character. */
		charlen = collect_char(output + j, onechar);

		j += charlen;

		/* When filtering, skip any ASCII control character. */
		if (filtering && is_ascii_cntrl_char(*(output + j - charlen)))
			continue;

		/* Insert the typed character into the existing answer string. */
		answer = charealloc(answer, strlen(answer) + charlen + 1);
		memmove(answer + typing_x + charlen, answer + typing_x,
								strlen(answer) - typing_x + 1);
		strncpy(answer + typing_x, onechar, charlen);

		typing_x += charlen;
	}

	free(output);
}

/* Move to the beginning of the answer. */
void do_statusbar_home(void)
{
	typing_x = 0;
}

/* Move to the end of the answer. */
void do_statusbar_end(void)
{
	typing_x = strlen(answer);
}

/* Move left one character. */
void do_statusbar_left(void)
{
	if (typing_x > 0)
		typing_x = step_left(answer, typing_x);
}

/* Move right one character. */
void do_statusbar_right(void)
{
	if (answer[typing_x] != '\0')
		typing_x = step_right(answer, typing_x);
}

/* Delete one character. */
void do_statusbar_delete(void)
{
	if (answer[typing_x] != '\0') {
		int charlen = char_length(answer + typing_x);

		memmove(answer + typing_x, answer + typing_x + charlen,
						strlen(answer) - typing_x - charlen + 1);
	}
}

/* Backspace over one character. */
void do_statusbar_backspace(void)
{
	if (typing_x > 0) {
		typing_x = step_left(answer, typing_x);
		do_statusbar_delete();
	}
}

/* Zap some or all text from the answer. */
void do_statusbar_cut_text(void)
{
	if (!ISSET(CUT_FROM_CURSOR))
		typing_x = 0;

	answer[typing_x] = '\0';
}

#ifndef NANO_TINY
/* Move to the next word in the answer. */
void do_statusbar_next_word(void)
{
	bool seen_space = !is_word_mbchar(answer + typing_x, FALSE);
	bool seen_word = !seen_space;

	/* Move forward until we reach either the end or the start of a word,
	 * depending on whether the AFTER_ENDS flag is set or not. */
	while (answer[typing_x] != '\0') {
		typing_x = step_right(answer, typing_x);

		if (ISSET(AFTER_ENDS)) {
			/* If this is a word character, continue; else it's a separator,
			 * and if we've already seen a word, then it's a word end. */
			if (is_word_mbchar(answer + typing_x, FALSE))
				seen_word = TRUE;
			else if (seen_word)
				break;
		} else {
			/* If this is not a word character, then it's a separator; else
			 * if we've already seen a separator, then it's a word start. */
			if (!is_word_mbchar(answer + typing_x, FALSE))
				seen_space = TRUE;
			else if (seen_space)
				break;
		}
	}
}

/* Move to the previous word in the answer. */
void do_statusbar_prev_word(void)
{
	bool seen_a_word = FALSE, step_forward = FALSE;

	/* Move backward until we pass over the start of a word. */
	while (typing_x != 0) {
		typing_x = step_left(answer, typing_x);

		if (is_word_mbchar(answer + typing_x, FALSE))
			seen_a_word = TRUE;
		else if (seen_a_word) {
			/* This is space now: we've overshot the start of the word. */
			step_forward = TRUE;
			break;
		}
	}

	if (step_forward)
		/* Move one character forward again to sit on the start of the word. */
		typing_x = step_right(answer, typing_x);
}
#endif /* !NANO_TINY */

/* Get verbatim input and inject it into the answer, without filtering. */
void do_statusbar_verbatim_input(void)
{
	int *kbinput;
	size_t kbinput_len;

	kbinput = get_verbatim_kbinput(bottomwin, &kbinput_len);

	inject_into_answer(kbinput, kbinput_len, FALSE);

	free(kbinput);
}

/* Paste the first line of the cutbuffer into the current answer. */
void do_statusbar_uncut_text(void)
{
	size_t pastelen = strlen(cutbuffer->data);
	char *fusion = charalloc(strlen(answer) + pastelen + 1);

	/* Concatenate: the current answer before the cursor, the first line
	 * of the cutbuffer, plus the rest of the current answer. */
	strncpy(fusion, answer, typing_x);
	strncpy(fusion + typing_x, cutbuffer->data, pastelen);
	strcpy(fusion + typing_x + pastelen, answer + typing_x);

	free(answer);
	answer = fusion;
	typing_x += pastelen;
}

/* Return the column number of the first character of the answer that is
 * displayed in the status bar when the cursor is at the given column,
 * with the available room for the answer starting at base.  Note that
 * (0 <= column - get_statusbar_page_start(column) < COLS). */
size_t get_statusbar_page_start(size_t base, size_t column)
{
	if (column == base || column < COLS - 1)
		return 0;
	else if (COLS > base + 2)
		return column - base - 1 - (column - base - 1) % (COLS - base - 2);
	else
		return column - 2;
}

/* Reinitialize the cursor position in the answer. */
void put_cursor_at_end_of_answer(void)
{
	typing_x = HIGHEST_POSITIVE;
}

/* Redraw the promptbar and place the cursor at the right spot. */
void draw_the_promptbar(void)
{
	size_t base = breadth(prompt) + 2;
	size_t the_page, end_page, column;
	char *expanded;

	the_page = get_statusbar_page_start(base, base + wideness(answer, typing_x));
	end_page = get_statusbar_page_start(base, base + breadth(answer) - 1);

	/* Color the promptbar over its full width. */
	wattron(bottomwin, interface_color_pair[TITLE_BAR]);
	mvwprintw(bottomwin, 0, 0, "%*s", COLS, " ");

	mvwaddstr(bottomwin, 0, 0, prompt);
	waddch(bottomwin, ':');
	waddch(bottomwin, (the_page == 0) ? ' ' : '<');

	expanded = display_string(answer, the_page, COLS - base, FALSE, TRUE);
	waddstr(bottomwin, expanded);
	free(expanded);

	if (base + breadth(answer) != COLS && the_page < end_page)
		mvwaddch(bottomwin, 0, COLS - 1, '>');

	wattroff(bottomwin, interface_color_pair[TITLE_BAR]);

	/* Work around a cursor-misplacement bug in VTEs. */
	wmove(bottomwin, 0, 0);
	wrefresh(bottomwin);

	/* Place the cursor at the right spot. */
	column = base + wideness(answer, typing_x);
	wmove(bottomwin, 0, column - get_statusbar_page_start(base, column));
	wnoutrefresh(bottomwin);
}

#ifndef NANO_TINY
/* Remove or add the pipe character at the answer's head. */
void add_or_remove_pipe_symbol_from_answer(void)
{
	if (answer[0] == '|') {
		memmove(answer, answer + 1, strlen(answer) + 1);
		if (typing_x > 0)
			typing_x--;
	} else {
		answer = charealloc(answer, strlen(answer) + 2);
		memmove(answer + 1, answer, strlen(answer) + 1);
		answer[0] = '|';
		typing_x++;
	}
}
#endif

/* Get a string of input at the status-bar prompt. */
functionptrtype acquire_an_answer(int *actual, bool allow_tabs,
		bool allow_files, bool *listed, linestruct **history_list,
		void (*refresh_func)(void))
{
	int kbinput = ERR;
	bool finished;
	functionptrtype func;
#ifdef ENABLE_TABCOMP
	bool tabbed = FALSE;
		/* Whether we've pressed Tab. */
#endif
#ifdef ENABLE_HISTORIES
	char *history = NULL;
		/* The current history string. */
	char *magichistory = NULL;
		/* The (partial) answer that was typed at the prompt, if any. */
#ifdef ENABLE_TABCOMP
	int last_kbinput = ERR;
		/* The key we pressed before the current key. */
	size_t complete_len = 0;
		/* The length of the original string that we're trying to
		 * tab complete, if any. */
#endif
#endif /* ENABLE_HISTORIES */

	if (typing_x > strlen(answer))
		typing_x = strlen(answer);

	while (TRUE) {
		draw_the_promptbar();

		kbinput = do_statusbar_input(&finished);

#ifndef NANO_TINY
		/* If the window size changed, go reformat the prompt string. */
		if (kbinput == KEY_WINCH) {
			refresh_func();
			*actual = KEY_WINCH;
#ifdef ENABLE_HISTORIES
			free(magichistory);
#endif
			return NULL;
		}
#endif /* !NANO_TINY */

		func = func_from_key(&kbinput);

		if (func == do_cancel || func == do_enter)
			break;

#ifdef ENABLE_TABCOMP
		if (func != do_tab)
			tabbed = FALSE;

		if (func == do_tab) {
#ifdef ENABLE_HISTORIES
			if (history_list != NULL) {
				if (last_kbinput != the_code_for(do_tab, TAB_CODE))
					complete_len = strlen(answer);

				if (complete_len > 0) {
					answer = get_history_completion(history_list,
										answer, complete_len);
					typing_x = strlen(answer);
				}
			} else
#endif
			if (allow_tabs)
				answer = input_tab(answer, allow_files, &typing_x,
										&tabbed, refresh_func, listed);
		} else
#endif /* ENABLE_TABCOMP */
#ifdef ENABLE_HISTORIES
		if (func == get_history_older_void) {
			if (history_list != NULL) {
				/* If we're scrolling up at the bottom of the history list
				 * and answer isn't blank, save answer in magichistory. */
				if ((*history_list)->next == NULL && *answer != '\0')
					magichistory = mallocstrcpy(magichistory, answer);

				/* Get the older search from the history list and save it in
				 * answer.  If there is no older search, don't do anything. */
				if ((history = get_history_older(history_list)) != NULL) {
					answer = mallocstrcpy(answer, history);
					typing_x = strlen(answer);
				}
			}
		} else if (func == get_history_newer_void) {
			if (history_list != NULL) {
				/* Get the newer search from the history list and save it in
				 * answer.  If there is no newer search, don't do anything. */
				if ((history = get_history_newer(history_list)) != NULL) {
					answer = mallocstrcpy(answer, history);
					typing_x = strlen(answer);
				}

				/* If we've reached the bottom of the history list, and answer
				 * is blank, and magichistory is set, restore the old answer. */
				if ((*history_list)->next == NULL &&
						*answer == '\0' && magichistory != NULL) {
					answer = mallocstrcpy(answer, magichistory);
					typing_x = strlen(answer);
				}
			}
		} else
#endif /* ENABLE_HISTORIES */
		if (func == do_help) {
			/* This key has a shortcut-list entry when it's used to go to
			 * the help viewer or display a message indicating that help
			 * is disabled, which means that finished has been set to TRUE.
			 * Set it back to FALSE here, so that we aren't kicked out of
			 * the status-bar prompt. */
			finished = FALSE;
		}
#ifndef NANO_TINY
		else if (func == do_nothing)
			finished = FALSE;
#endif

		/* If we have a shortcut with an associated function, break out if
		 * we're finished after running or trying to run the function. */
		if (finished)
			break;

#if defined(ENABLE_HISTORIES) && defined(ENABLE_TABCOMP)
		last_kbinput = kbinput;
#endif
	}

#ifdef ENABLE_HISTORIES
	/* Set the current position in the history list to the bottom. */
	if (history_list != NULL) {
		history_reset(*history_list);
		free(magichistory);
	}
#endif

	*actual = kbinput;

	return func;
}

/* Ask a question on the status bar.  Return 0 when text was entered,
 * -1 for a cancelled entry, -2 for a blank string, and the relevant
 * keycode when a valid shortcut key was pressed.
 *
 * The allow_tabs parameter indicates whether tab completion is allowed,
 * and allow_files indicates whether all files (and not just directories)
 * can be tab completed.  The 'provided' parameter is the default answer
 * for when simply Enter is typed. */
int do_prompt(bool allow_tabs, bool allow_files,
		int menu, const char *provided, linestruct **history_list,
		void (*refresh_func)(void), const char *msg, ...)
{
	va_list ap;
	int retval;
	functionptrtype func = NULL;
	bool listed = FALSE;
	/* Save a possible current status-bar x position and prompt. */
	size_t was_typing_x = typing_x;
	char *saved_prompt = prompt;

	bottombars(menu);

	if (answer != provided)
		answer = mallocstrcpy(answer, provided);

#ifndef NANO_TINY
  redo_theprompt:
#endif
	prompt = charalloc((COLS * MAXCHARLEN) + 1);
	va_start(ap, msg);
	vsnprintf(prompt, COLS * MAXCHARLEN, msg, ap);
	va_end(ap);
	/* Reserve five columns for colon plus angles plus answer, ":<aa>". */
	prompt[actual_x(prompt, (COLS < 5) ? 0 : COLS - 5)] = '\0';

	func = acquire_an_answer(&retval, allow_tabs, allow_files, &listed,
								history_list, refresh_func);
	free(prompt);
	prompt = saved_prompt;

#ifndef NANO_TINY
	if (retval == KEY_WINCH)
		goto redo_theprompt;
#endif

	/* If we're done with this prompt, restore the x position to what
	 * it was at a possible previous prompt. */
	if (func == do_cancel || func == do_enter)
		typing_x = was_typing_x;

	/* If we left the prompt via Cancel or Enter, set the return value
	 * properly. */
	if (func == do_cancel)
		retval = -1;
	else if (func == do_enter)
		retval = (*answer == '\0') ? -2 : 0;

	wipe_statusbar();

#ifdef ENABLE_TABCOMP
	/* If we've done tab completion, there might still be a list of
	 * filename matches on the edit window.  Clear them off. */
	if (listed)
		refresh_func();
#endif

	return retval;
}

/* Ask a simple Yes/No (and optionally All) question, specified in msg,
 * on the status bar.  Return 1 for Yes, 0 for No, 2 for All (if all is
 * TRUE when passed in), and -1 for Cancel. */
int do_yesno_prompt(bool all, const char *msg)
{
	int choice = -2, width = 16;
	/* TRANSLATORS: For the next three strings, specify the starting letters
	 * of the translations for "Yes"/"No"/"All".  The first letter of each of
	 * these strings MUST be a single-byte letter; others may be multi-byte. */
	const char *yesstr = _("Yy");
	const char *nostr = _("Nn");
	const char *allstr = _("Aa");

	while (choice == -2) {
#ifdef ENABLE_NLS
		char letter[MAXCHARLEN + 1];
		int index = 0;
#endif
		int kbinput;

		if (!ISSET(NO_HELP)) {
			char shortstr[MAXCHARLEN + 2];
				/* Temporary string for (translated) " Y", " N" and " A". */
			const keystruct *cancelshortcut = first_sc_for(MYESNO, do_cancel);
				/* The keystroke that is bound to the Cancel function. */

			if (COLS < 32)
				width = COLS / 2;

			/* Clear the shortcut list from the bottom of the screen. */
			blank_bottombars();

			/* Now show the ones for "Yes", "No", "Cancel" and maybe "All". */
			sprintf(shortstr, " %c", yesstr[0]);
			wmove(bottomwin, 1, 0);
			post_one_key(shortstr, _("Yes"), width);

			shortstr[1] = nostr[0];
			wmove(bottomwin, 2, 0);
			post_one_key(shortstr, _("No"), width);

			if (all) {
				shortstr[1] = allstr[0];
				wmove(bottomwin, 1, width);
				post_one_key(shortstr, _("All"), width);
			}

			wmove(bottomwin, 2, width);
			post_one_key(cancelshortcut->keystr, _("Cancel"), width);
		}

		/* Color the promptbar over its full width and display the question. */
		wattron(bottomwin, interface_color_pair[TITLE_BAR]);
		mvwprintw(bottomwin, 0, 0, "%*s", COLS, " ");
		mvwaddnstr(bottomwin, 0, 0, msg, actual_x(msg, COLS - 1));
		wattroff(bottomwin, interface_color_pair[TITLE_BAR]);
		wnoutrefresh(bottomwin);

		currmenu = MYESNO;

		/* When not replacing, show the cursor while waiting for a key. */
		kbinput = get_kbinput(bottomwin, !all);

#ifndef NANO_TINY
		/* Accept the first character of an external paste. */
		if (bracketed_paste && kbinput == BRACKETED_PASTE_MARKER)
			kbinput = get_kbinput(bottomwin, BLIND);
#endif

#ifdef ENABLE_NLS
		letter[index++] = (unsigned char)kbinput;
#ifdef ENABLE_UTF8
		/* If the received code is a UTF-8 starter byte, get also the
		 * continuation bytes and assemble them into one letter. */
		if (using_utf8() && 0xC0 <= kbinput && kbinput <= 0xF7) {
			int extras = (kbinput / 16) % 4 + (kbinput <= 0xCF ? 1 : 0);

			while (extras <= get_key_buffer_len() && extras-- > 0)
				letter[index++] = (unsigned char)get_kbinput(bottomwin, !all);
		}
#endif
		letter[index] = '\0';

		/* See if the typed letter is in the Yes, No, or All strings. */
		if (strstr(yesstr, letter) != NULL)
			choice = 1;
		else if (strstr(nostr, letter) != NULL)
			choice = 0;
		else if (all && strstr(allstr, letter) != NULL)
			choice = 2;
		else
#endif /* ENABLE_NLS */
		if (strchr("Yy", kbinput) != NULL)
			choice = 1;
		else if (strchr("Nn", kbinput) != NULL)
			choice = 0;
		else if (all && strchr("Aa", kbinput) != NULL)
			choice = 2;
		else if (func_from_key(&kbinput) == do_cancel)
			choice = -1;
		/* Interpret ^N and ^Q as "No", to allow exiting in anger. */
		else if (kbinput == '\x0E' || kbinput == '\x11')
			choice = 0;
		/* And interpret ^Y as "Yes". */
		else if (kbinput == '\x19')
			choice = 1;
#ifdef ENABLE_MOUSE
		else if (kbinput == KEY_MOUSE) {
			int mouse_x, mouse_y;
			/* We can click on the Yes/No/All shortcuts to select an answer. */
			if (get_mouseinput(&mouse_y, &mouse_x, FALSE) == 0 &&
						wmouse_trafo(bottomwin, &mouse_y, &mouse_x, FALSE) &&
						mouse_x < (width * 2) && mouse_y > 0) {
				int x = mouse_x / width;
				int y = mouse_y - 1;

				/* x == 0 means Yes or No, y == 0 means Yes or All. */
				choice = -2 * x * y + x - y + 1;

				if (choice == 2 && !all)
					choice = -2;
			}
		}
#endif /* ENABLE_MOUSE */
		else
			beep();

#ifndef NANO_TINY
		/* Ignore the rest of an external paste. */
		while (bracketed_paste)
			kbinput = get_kbinput(bottomwin, BLIND);
#endif
	}

	return choice;
}
