/**************************************************************************
 *   help.c  --  This file is part of GNU nano.                           *
 *                                                                        *
 *   Copyright (C) 2000-2011, 2013-2019 Free Software Foundation, Inc.    *
 *   Copyright (C) 2017 Rishabh Dave                                      *
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

#include <errno.h>
#include <string.h>

#ifdef ENABLE_HELP

static char *help_text = NULL;
		/* The text displayed in the help window. */

static const char *start_of_body = NULL;
		/* The point in the help text just after the title. */

static char *end_of_intro = NULL;
		/* The point in the help text where the shortcut descriptions begin. */

static size_t location;
		/* The offset (in bytes) of the topleft of the shown help text. */

/* Hard-wrap the concatenated help text, and write it into a new buffer. */
void wrap_help_text_into_buffer()
{
	size_t sum = 0;
	const char *ptr = start_of_body;

	make_new_buffer();

	/* Copy the help text into the just-created new buffer. */
	while (*ptr != '\0') {
		int length = help_line_len(ptr);
		char *oneline = nmalloc(length + 1);

		snprintf(oneline, length + 1, "%s", ptr);
		free(openfile->current->data);
		openfile->current->data = oneline;

		ptr += length;
		if (*ptr != '\n')
			ptr--;

		/* Create a new line, and then one more for each extra \n. */
		do {
			openfile->current->next = make_new_node(openfile->current);
			openfile->current = openfile->current->next;
			openfile->current->data = mallocstrcpy(NULL, "");
		} while (*(++ptr) == '\n');
	}

	openfile->filebot = openfile->current;
	openfile->current = openfile->filetop;

	remove_magicline();
#ifdef ENABLE_COLOR
	color_update();
#endif
	prepare_for_display();

	/* Move to the position in the file where we were before. */
	while (TRUE) {
		sum += strlen(openfile->current->data);
		if (sum > location)
			break;
		openfile->current = openfile->current->next;
	}

	openfile->edittop = openfile->current;
}

/* Our main help-viewer function. */
void do_help(void)
{
	int kbinput = ERR;
	functionptrtype func;
		/* The function of the key the user typed in. */
	int oldmenu = currmenu;
		/* The menu we were called from. */
#ifdef ENABLE_LINENUMBERS
	int was_margin = margin;
#endif
	ssize_t was_tabsize = tabsize;
#ifdef ENABLE_COLOR
	char *was_syntax = syntaxstr;
#endif
	char *saved_answer = (answer != NULL) ? strdup(answer) : NULL;
		/* The current answer when the user invokes help at the prompt. */
	unsigned stash[sizeof(flags) / sizeof(flags[0])];
		/* A storage place for the current flag settings. */
	linestruct *line;
	int length;

	blank_statusbar();

	/* Save the settings of all flags. */
	memcpy(stash, flags, sizeof(flags));

	/* Ensure that the help screen's shortcut list can be displayed. */
	if (ISSET(NO_HELP) && LINES > 4) {
		UNSET(NO_HELP);
		window_init();
	}

	/* When searching, do it forward, case insensitive, and without regexes. */
	UNSET(BACKWARDS_SEARCH);
	UNSET(CASE_SENSITIVE);
	UNSET(USE_REGEXP);

	UNSET(WHITESPACE_DISPLAY);
	UNSET(NOREAD_MODE);

#ifdef ENABLE_LINENUMBERS
	UNSET(LINE_NUMBERS);
	editwincols = COLS;
	margin = 0;
#endif
	tabsize = 8;
#ifdef ENABLE_COLOR
	syntaxstr = "nanohelp";
#endif
	curs_set(0);

	/* Compose the help text from all the pieces. */
	help_init();
	inhelp = TRUE;
	location = 0;
	didfind = 0;

	bottombars(MHELP);

	/* Extract the title from the head of the help text. */
	length = break_line(help_text, MAX_BUF_SIZE, TRUE);
	title = charalloc(length * sizeof(char) + 1);
	strncpy(title, help_text, length);
	title[length] = '\0';

	titlebar(title);

	/* Skip over the title to point at the start of the body text. */
	start_of_body = help_text + length;
	while (*start_of_body == '\n')
		start_of_body++;

	wrap_help_text_into_buffer();
	edit_refresh();

	while (TRUE) {
		lastmessage = HUSH;
		focusing = TRUE;

		/* Show the cursor when we searched and found something. */
		kbinput = get_kbinput(edit, didfind == 1 || ISSET(SHOW_CURSOR));
		didfind = 0;

		func = parse_help_input(&kbinput);

		if (func == total_refresh) {
			total_redraw();
		} else if (ISSET(SHOW_CURSOR) && (func == do_left || func == do_right ||
											func == do_up || func == do_down)) {
			func();
		} else if (func == do_up || func == do_scroll_up) {
			do_scroll_up();
		} else if (func == do_down || func == do_scroll_down) {
			if (openfile->edittop->lineno + editwinrows - 1 <
								openfile->filebot->lineno)
				do_scroll_down();
		} else if (func == do_page_up || func == do_page_down ||
					func == to_first_line || func == to_last_line ||
					func == do_findprevious || func == do_findnext) {
			func();
		} else if (func == do_search_forward || func == do_search_backward) {
			func();
			bottombars(MHELP);
#ifdef ENABLE_NANORC
		} else if (func == (functionptrtype)implant) {
			implant(first_sc_for(MHELP, func)->expansion);
#endif
#ifdef ENABLE_MOUSE
		} else if (kbinput == KEY_MOUSE) {
			int dummy_row, dummy_col;
			get_mouseinput(&dummy_row, &dummy_col, TRUE);
#endif
#ifndef NANO_TINY
		} else if (kbinput == KEY_WINCH) {
			;  /* Nothing to do. */
#endif
		} else if (func == do_exit) {
			break;
		} else
			unbound_key(kbinput);

		currmenu = MHELP;
		edit_refresh();

		location = 0;
		line = openfile->filetop;

		/* Count how far (in bytes) edittop is into the file. */
		while (line != openfile->edittop) {
			location += strlen(line->data);
			line = line->next;
		}
	}

	/* Discard the help-text buffer. */
	close_buffer();

	/* Restore the settings of all flags. */
	memcpy(flags, stash, sizeof(flags));

#ifdef ENABLE_LINENUMBERS
	margin = was_margin;
	editwincols = COLS - margin;
#endif
	tabsize = was_tabsize;
#ifdef ENABLE_COLOR
	syntaxstr = was_syntax;
	have_palette = FALSE;
#endif

	free(title);
	title = NULL;
	free(answer);
	answer = saved_answer;
	free(help_text);
	inhelp = FALSE;

	curs_set(0);
	refresh_needed = TRUE;

	if (ISSET(NO_HELP)) {
		currmenu = oldmenu;
		window_init();
	} else {
		wipe_statusbar();
		bottombars(oldmenu);
	}

#ifdef ENABLE_BROWSER
	if (oldmenu == MBROWSER || oldmenu == MWHEREISFILE || oldmenu == MGOTODIR)
		browser_refresh();
	else
#endif
		titlebar(NULL);
}

/* Allocate space for the help text for the current menu, and concatenate
 * the different pieces of text into it. */
void help_init(void)
{
	size_t allocsize = 0;
		/* Space needed for help_text. */
	const char *htx[3];
		/* Untranslated help introduction.  We break it up into three chunks
		 * in case the full string is too long for the compiler to handle. */
	char *ptr;
	const funcstruct *f;
	const keystruct *s;

	/* First, set up the initial help text for the current function. */
	if (currmenu == MWHEREIS || currmenu == MREPLACE || currmenu == MREPLACEWITH) {
		htx[0] = N_("Search Command Help Text\n\n "
				"Enter the words or characters you would like to "
				"search for, and then press Enter.  If there is a "
				"match for the text you entered, the screen will be "
				"updated to the location of the nearest match for the "
				"search string.\n\n The previous search string will be "
				"shown in brackets after the search prompt.  Hitting "
				"Enter without entering any text will perform the "
				"previous search.  ");
		htx[1] = N_("If you have selected text with the mark and then "
				"search to replace, only matches in the selected text "
				"will be replaced.\n\n The following function keys are "
				"available in Search mode:\n\n");
		htx[2] = NULL;
	} else if (currmenu == MGOTOLINE) {
		htx[0] = N_("Go To Line Help Text\n\n "
				"Enter the line number that you wish to go to and hit "
				"Enter.  If there are fewer lines of text than the "
				"number you entered, you will be brought to the last "
				"line of the file.\n\n The following function keys are "
				"available in Go To Line mode:\n\n");
		htx[1] = NULL;
		htx[2] = NULL;
	} else if (currmenu == MINSERTFILE) {
		htx[0] = N_("Insert File Help Text\n\n "
				"Type in the name of a file to be inserted into the "
				"current file buffer at the current cursor "
				"location.\n\n If you have compiled nano with multiple "
				"file buffer support, and enable multiple file buffers "
				"with the -F or --multibuffer command line flags, the "
				"Meta-F toggle, or a nanorc file, inserting a file "
				"will cause it to be loaded into a separate buffer "
				"(use Meta-< and > to switch between file buffers).  ");
		htx[1] = N_("If you need another blank buffer, do not enter "
				"any filename, or type in a nonexistent filename at "
				"the prompt and press Enter.\n\n The following "
				"function keys are available in Insert File mode:\n\n");
		htx[2] = NULL;
	} else if (currmenu == MWRITEFILE) {
		htx[0] = N_("Write File Help Text\n\n "
				"Type the name that you wish to save the current file "
				"as and press Enter to save the file.\n\n If you have "
				"selected text with the mark, you will be prompted to "
				"save only the selected portion to a separate file.  To "
				"reduce the chance of overwriting the current file with "
				"just a portion of it, the current filename is not the "
				"default in this mode.\n\n The following function keys "
				"are available in Write File mode:\n\n");
		htx[1] = NULL;
		htx[2] = NULL;
	}
#ifdef ENABLE_BROWSER
	else if (currmenu == MBROWSER) {
		htx[0] = N_("File Browser Help Text\n\n "
				"The file browser is used to visually browse the "
				"directory structure to select a file for reading "
				"or writing.  You may use the arrow keys or Page Up/"
				"Down to browse through the files, and S or Enter to "
				"choose the selected file or enter the selected "
				"directory.  To move up one level, select the "
				"directory called \"..\" at the top of the file "
				"list.\n\n The following function keys are available "
				"in the file browser:\n\n");
		htx[1] = NULL;
		htx[2] = NULL;
	} else if (currmenu == MWHEREISFILE) {
		htx[0] = N_("Browser Search Command Help Text\n\n "
				"Enter the words or characters you would like to "
				"search for, and then press Enter.  If there is a "
				"match for the text you entered, the screen will be "
				"updated to the location of the nearest match for the "
				"search string.\n\n The previous search string will be "
				"shown in brackets after the search prompt.  Hitting "
				"Enter without entering any text will perform the "
				"previous search.\n\n");
		htx[1] = N_(" The following function keys are available in "
				"Browser Search mode:\n\n");
		htx[2] = NULL;
	} else if (currmenu == MGOTODIR) {
		htx[0] = N_("Browser Go To Directory Help Text\n\n "
				"Enter the name of the directory you would like to "
				"browse to.\n\n If tab completion has not been "
				"disabled, you can use the Tab key to (attempt to) "
				"automatically complete the directory name.\n\n The "
				"following function keys are available in Browser Go "
				"To Directory mode:\n\n");
		htx[1] = NULL;
		htx[2] = NULL;
	}
#endif /* ENABLE_BROWSER */
#ifdef ENABLE_SPELLER
	else if (currmenu == MSPELL) {
		htx[0] = N_("Spell Check Help Text\n\n "
				"The spell checker checks the spelling of all text in "
				"the current file.  When an unknown word is "
				"encountered, it is highlighted and a replacement can "
				"be edited.  It will then prompt to replace every "
				"instance of the given misspelled word in the current "
				"file, or, if you have selected text with the mark, in "
				"the selected text.\n\n The following function keys "
				"are available in Spell Check mode:\n\n");
		htx[1] = NULL;
		htx[2] = NULL;
	}
#endif /* ENABLE_SPELLER */
#ifndef NANO_TINY
	else if (currmenu == MEXTCMD) {
		htx[0] = N_("Execute Command Help Text\n\n "
				"This mode allows you to insert the output of a "
				"command run by the shell into the current buffer (or "
				"into a new buffer).  If the command is preceded by '|' "
				"(the pipe symbol), the current contents of the buffer "
				"(or marked region) will be piped to the command.  ");
		htx[1] = N_("If you just need another blank buffer, do not enter any "
				"command.\n\n");
		htx[2] = N_(" The following function keys are "
				"available in Execute Command mode:\n\n");
	} else if (currmenu == MLINTER) {
		htx[0] = N_("=== Linter ===\n\n "
				"In this mode, the status bar shows an error message or "
				"warning, and the cursor is put at the corresponding "
				"position in the file.  With PageUp and PageDown you "
				"can switch to earlier and later messages.\n\n");
		htx[1] = N_(" The following function keys are "
				"available in Linter mode:\n\n");
		htx[2] = NULL;
	}
#endif /* !NANO_TINY */
	else {
		/* Default to the main help list. */
		htx[0] = N_("Main nano help text\n\n "
				"The nano editor is designed to emulate the "
				"functionality and ease-of-use of the UW Pico text "
				"editor.  There are four main sections of the editor.  "
				"The top line shows the program version, the current "
				"filename being edited, and whether or not the file "
				"has been modified.  Next is the main editor window "
				"showing the file being edited.  The status line is "
				"the third line from the bottom and shows important "
				"messages.  ");
		htx[1] = N_("The bottom two lines show the most commonly used "
				"shortcuts in the editor.\n\n Shortcuts are written as "
				"follows: Control-key sequences are notated with a '^' "
				"and can be entered either by using the Ctrl key or "
				"pressing the Esc key twice.  Meta-key sequences are "
				"notated with 'M-' and can be entered using either the "
				"Alt, Cmd, or Esc key, depending on your keyboard setup.  ");
		htx[2] = N_("Also, pressing Esc twice and then typing a "
				"three-digit decimal number from 000 to 255 will enter "
				"the character with the corresponding value.  The "
				"following keystrokes are available in the main editor "
				"window.  Alternative keys are shown in "
				"parentheses:\n\n");
	}

	htx[0] = _(htx[0]);
	if (htx[1] != NULL)
		htx[1] = _(htx[1]);
	if (htx[2] != NULL)
		htx[2] = _(htx[2]);

	allocsize += strlen(htx[0]);
	if (htx[1] != NULL)
		allocsize += strlen(htx[1]);
	if (htx[2] != NULL)
		allocsize += strlen(htx[2]);

	/* Calculate the length of the shortcut help text.  Each entry has
	 * one or two keys, which fill 17 cells, plus translated text,
	 * plus one or two \n's. */
	for (f = allfuncs; f != NULL; f = f->next)
		if (f->menus & currmenu)
			allocsize += strlen(_(f->help)) + 21;

#ifndef NANO_TINY
	/* If we're on the main list, we also count the toggle help text.
	 * Each entry has "M-%c\t\t ", six chars which fill 17 cells, plus
	 * two translated texts, plus a space, plus one or two '\n's. */
	if (currmenu == MMAIN) {
		size_t onoff_len = strlen(_("enable/disable"));

		for (s = sclist; s != NULL; s = s->next)
			if (s->func == do_toggle_void)
				allocsize += strlen(_(flagtostr(s->toggle))) + onoff_len + 9;
	}
#endif

	/* Allocate memory for the help text. */
	help_text = charalloc(allocsize + 1);

	/* Now add the text we want. */
	strcpy(help_text, htx[0]);
	if (htx[1] != NULL)
		strcat(help_text, htx[1]);
	if (htx[2] != NULL)
		strcat(help_text, htx[2]);

	/* Remember this end-of-introduction, start-of-shortcuts. */
	end_of_intro = help_text + strlen(help_text);
	ptr = end_of_intro;

	/* Now add the shortcuts and their descriptions. */
	for (f = allfuncs; f != NULL; f = f->next) {
		int tally = 0;

		if ((f->menus & currmenu) == 0)
			continue;

		/* Show the first two shortcuts (if any) for each function. */
		for (s = sclist; s != NULL; s = s->next) {
			if ((s->menus & currmenu) && s->func == f->func) {
				/* Make the first column 7 cells wide and the second 10. */
				if (++tally == 1) {
					sprintf(ptr, "%s                ", s->keystr);
					/* Unicode arrows take three bytes instead of one. */
					ptr += (strstr(s->keystr, "\xE2") != NULL ? 9 : 7);
				} else {
					sprintf(ptr, "(%s)       ", s->keystr);
					ptr += (strstr(s->keystr, "\xE2") != NULL ? 12 : 10);
					break;
				}
			}
		}

		if (tally == 0)
			ptr += sprintf(ptr, "\t\t ");
		else if (tally == 1)
			ptr += 10;

		/* The shortcut's description. */
		ptr += sprintf(ptr, "%s\n", _(f->help));

		if (f->blank_after)
			ptr += sprintf(ptr, "\n");
	}

#ifndef NANO_TINY
	/* And the toggles... */
	if (currmenu == MMAIN) {
		int maximum = 0, counter = 0;

		/* First see how many toggles there are. */
		for (s = sclist; s != NULL; s = s->next)
			maximum = (s->toggle && s->ordinal > maximum) ? s->ordinal : maximum;

		/* Now show them in the original order. */
		while (counter < maximum) {
			counter++;
			for (s = sclist; s != NULL; s = s->next)
				if (s->toggle && s->ordinal == counter) {
					ptr += sprintf(ptr, "%s\t\t %s %s\n", (s->menus == MMAIN ? s->keystr : ""),
								_(flagtostr(s->toggle)), _("enable/disable"));
					if (s->toggle == NO_COLOR_SYNTAX || s->toggle == TABS_TO_SPACES)
						ptr += sprintf(ptr, "\n");
					break;
				}
		}
	}
#endif /* !NANO_TINY */
}

/* Return the function that is bound to the given key, accepting certain
 * plain characters too, for consistency with the file browser. */
functionptrtype parse_help_input(int *kbinput)
{
	if (!meta_key) {
		switch (*kbinput) {
			case BS_CODE:
			case '-':
				return do_page_up;
			case ' ':
				return do_page_down;
			case 'W':
			case 'w':
			case '/':
				return do_search_forward;
			case 'N':
				return do_findprevious;
			case 'n':
				return do_findnext;
			case 'E':
			case 'e':
			case 'Q':
			case 'q':
			case 'X':
			case 'x':
				return do_exit;
		}
	}
	return func_from_key(kbinput);
}

/* Calculate the displayable length of the help-text line starting at ptr. */
size_t help_line_len(const char *ptr)
{
	size_t wrapping_point = (COLS > 24) ? COLS - 1 : 24;
		/* The target width for wrapping long lines. */
	ssize_t wrap_location;
		/* Actual position where the line can be wrapped. */
	size_t length = 0;
		/* Full length of the line, until the first newline. */

	/* Avoid overwide paragraphs in the introductory text. */
	if (ptr < end_of_intro && COLS > 74)
		wrapping_point = 74;

	wrap_location = break_line(ptr, wrapping_point, TRUE);

	/* Get the length of the entire line up to a null or a newline. */
	while (*(ptr + length) != '\0' && *(ptr + length) != '\n')
		length = step_right(ptr, length);

	/* If the entire line will just fit the screen, don't wrap it. */
	if (wideness(ptr, length) <= wrapping_point + 1)
		return length;
	else if (wrap_location > 0)
		return wrap_location;
	else
		return 1;
}

#endif /* ENABLE_HELP */

/* Start the help viewer. */
void do_help_void(void)
{
#ifdef ENABLE_HELP
	do_help();
#else
	if (currmenu == MMAIN)
		say_there_is_no_help();
	else
		beep();
#endif
}
