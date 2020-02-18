/**************************************************************************
 *   winio.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2019 Free Software Foundation, Inc.    *
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
#include "revision.h"

#include <ctype.h>
#ifdef __linux__
#include <sys/ioctl.h>
#endif
#include <string.h>
#ifdef ENABLE_UTF8
#include <wchar.h>
#endif

#ifdef REVISION
#define BRANDING REVISION
#else
#define BRANDING PACKAGE_STRING
#endif

static int *key_buffer = NULL;
		/* A buffer for the keystrokes that haven't been handled yet. */
static size_t key_buffer_len = 0;
		/* The length of the keystroke buffer. */
static bool solitary = FALSE;
		/* Whether an Esc arrived by itself -- not as leader of a sequence. */
static int digit_count = 0;
		/* How many digits of a three-digit character code we've eaten. */
static bool waiting_mode = TRUE;
		/* Whether getting a character will wait for a key to be pressed. */
static int statusblank = 0;
		/* The number of keystrokes left before we blank the statusbar. */
#ifdef USING_OLD_NCURSES
static bool seen_wide = FALSE;
		/* Whether we've seen a multicolumn character in the current line. */
#endif
static bool reveal_cursor = FALSE;
		/* Whether the cursor should be shown when waiting for input. */
#ifndef NANO_TINY
static bool recording = FALSE;
		/* Whether we are in the process of recording a macro. */
static int *macro_buffer = NULL;
		/* A buffer where the recorded key codes are stored. */
static size_t macro_length = 0;
		/* The current length of the macro. */

/* Add the given code to the macro buffer. */
void add_to_macrobuffer(int code)
{
	macro_length++;
	macro_buffer = (int*)nrealloc(macro_buffer, macro_length * sizeof(int));
	macro_buffer[macro_length - 1] = code;
}

/* Remove the last key code plus any trailing Esc codes from macro buffer. */
void snip_last_keystroke(void)
{
	macro_length--;
	while (macro_length > 0 && macro_buffer[macro_length - 1] == '\x1b')
		macro_length--;
}

/* Start or stop the recording of keystrokes. */
void record_macro(void)
{
	recording = !recording;

	if (recording) {
		macro_length = 0;
		statusbar(_("Recording a macro..."));
	} else {
		snip_last_keystroke();
		statusbar(_("Stopped recording"));
	}
}

/* Copy the stored sequence of codes into the regular key buffer,
 * so they will be "executed" again. */
void run_macro(void)
{
	size_t i;

	if (recording) {
		statusbar(_("Cannot run macro while recording"));
		snip_last_keystroke();
		return;
	}

	if (macro_length == 0) {
		statusbar(_("Macro is empty"));
		return;
	}

	key_buffer = (int *)nrealloc(key_buffer, macro_length * sizeof(int));
	key_buffer_len = macro_length;

	for (i = 0; i < macro_length; i++)
		key_buffer[i] = macro_buffer[i];
}
#endif /* !NANO_TINY */

/* Control character compatibility:
 *
 * - Ctrl-H is Backspace under ASCII, ANSI, VT100, and VT220.
 * - Ctrl-I is Tab under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-M is Enter under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-Q is XON under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-S is XOFF under ASCII, ANSI, VT100, VT220, and VT320.
 * - Ctrl-? is Delete under ASCII, ANSI, VT100, and VT220,
 *          but is Backspace under VT320.
 *
 * Note: VT220 and VT320 also generate Esc [ 3 ~ for Delete.  By default,
 * xterm assumes it's running on a VT320 and generates Ctrl-?
 * for Backspace and Esc [ 3 ~ for Delete.  This causes problems for
 * VT100-derived terminals such as the FreeBSD console, which expect
 * Ctrl-H for Backspace and Ctrl-? for Delete, and on which the
 * VT320 sequences are translated by the keypad to KEY_DC and [nothing].
 * We work around this conflict via the REBIND_DELETE flag: if it's not set,
 * we assume VT320 compatibility, and if it is, we assume VT100 compatibility.
 *
 * Escape sequence compatibility:
 *
 * We support escape sequences for ANSI, VT100, VT220, VT320, the Linux
 * console, the FreeBSD console, the Mach console, xterm, rxvt, Eterm,
 * and Terminal, and some for iTerm2.  Among these, there are several
 * conflicts and omissions, outlined as follows:
 *
 * - Tab on ANSI == PageUp on FreeBSD console; the former is omitted.
 *   (Ctrl-I is also Tab on ANSI, which we already support.)
 * - PageDown on FreeBSD console == Center (5) on numeric keypad with
 *   NumLock off on Linux console; the latter is omitted.  (The editing
 *   keypad key is more important to have working than the numeric
 *   keypad key, because the latter has no value when NumLock is off.)
 * - F1 on FreeBSD console == the mouse key on xterm/rxvt/Eterm; the
 *   latter is omitted.  (Mouse input will only work properly if the
 *   extended keypad value KEY_MOUSE is generated on mouse events
 *   instead of the escape sequence.)
 * - F9 on FreeBSD console == PageDown on Mach console; the former is
 *   omitted.  (The editing keypad is more important to have working
 *   than the function keys, because the functions of the former are not
 *   arbitrary and the functions of the latter are.)
 * - F10 on FreeBSD console == PageUp on Mach console; the former is
 *   omitted.  (Same as above.)
 * - F13 on FreeBSD console == End on Mach console; the former is
 *   omitted.  (Same as above.)
 * - F15 on FreeBSD console == Shift-Up on rxvt/Eterm; the former is
 *   omitted.  (The arrow keys, with or without modifiers, are more
 *   important to have working than the function keys, because the
 *   functions of the former are not arbitrary and the functions of the
 *   latter are.)
 * - F16 on FreeBSD console == Shift-Down on rxvt/Eterm; the former is
 *   omitted.  (Same as above.) */

/* Read in a sequence of keystrokes from the given window and save them
 * in the keystroke buffer. */
void read_keys_from(WINDOW *win)
{
	int input = ERR;
	size_t errcount = 0;

	/* Before reading the first keycode, display any pending screen updates. */
	doupdate();

	if (reveal_cursor) {
		curs_set(1);
#ifdef USE_SLANG
		doupdate();
#endif
	}

	/* Read in the first keycode using whatever mode we're in. */
	while (input == ERR) {
		input = wgetch(win);

#ifndef NANO_TINY
		if (the_window_resized) {
			regenerate_screen();
			input = KEY_WINCH;
		}
#endif
		if (input == ERR && !waiting_mode) {
			curs_set(0);
			return;
		}

		/* If we've failed to get a keycode MAX_BUF_SIZE times in a row,
		 * assume our input source is gone and die gracefully.  We could
		 * check if errno is set to EIO ("Input/output error") and die in
		 * that case, but it's not always set properly.  Argh. */
		if (input == ERR && ++errcount == MAX_BUF_SIZE)
			die(_("Too many errors from stdin\n"));
	}

	curs_set(0);

	/* Initiate the keystroke buffer, and save the keycode in it. */
	key_buffer = (int *)nrealloc(key_buffer, sizeof(int));
	key_buffer[0] = input;
	key_buffer_len = 1;

#ifndef NANO_TINY
	/* If we got a SIGWINCH, get out as the win argument is no longer valid. */
	if (input == KEY_WINCH)
		return;
#endif

	/* Read in the remaining characters using non-blocking input. */
	nodelay(win, TRUE);

	while (TRUE) {
#ifndef NANO_TINY
		if (recording)
			add_to_macrobuffer(input);
#endif
		input = wgetch(win);

		/* If there aren't any more characters, stop reading. */
		if (input == ERR)
			break;

		/* Extend the keystroke buffer, and save the keycode at its end. */
		key_buffer_len++;
		key_buffer = (int *)nrealloc(key_buffer, key_buffer_len * sizeof(int));
		key_buffer[key_buffer_len - 1] = input;
	}

	/* Restore waiting mode if it was on. */
	if (waiting_mode)
		nodelay(win, FALSE);

#ifdef DEBUG
	fprintf(stderr, "\nSequence of hex codes:");
	for (size_t i = 0; i < key_buffer_len; i++)
		fprintf(stderr, " %3x", key_buffer[i]);
	fprintf(stderr, "\n");
#endif
}

/* Return the length of the keystroke buffer. */
size_t get_key_buffer_len(void)
{
	return key_buffer_len;
}

/* Add the given keycode to the front of the keystroke buffer. */
void put_back(int keycode)
{
	/* If the keystroke buffer is at maximum capacity, don't add anything. */
	if (key_buffer_len + 1 < key_buffer_len)
		return;

	/* Extend the keystroke buffer to make room for the extra keycode. */
	key_buffer = (int *)nrealloc(key_buffer, ++key_buffer_len * sizeof(int));

	/* If the keystroke buffer wasn't empty before, move all the
	 * existing content one step further away. */
	if (key_buffer_len > 1)
		memmove(key_buffer + 1, key_buffer, (key_buffer_len - 1) * sizeof(int));

	*key_buffer = keycode;
}

/* Put the character given in kbinput back into the input stream.  If it
 * is a Meta key, also insert an Escape character in front of it. */
void unget_kbinput(int kbinput, bool metakey)
{
	put_back(kbinput);

	if (metakey)
		put_back(ESC_CODE);
}

#ifdef ENABLE_NANORC
/* Insert the given string into the keyboard buffer. */
void implant(const char *string)
{
	for (int i = strlen(string); i > 0; i--)
		put_back((unsigned char)string[i - 1]);
}
#endif

/* Try to read input_len codes from the keystroke buffer.  If the
 * keystroke buffer is empty and win isn't NULL, try to read in more
 * codes from win and add them to the keystroke buffer before doing
 * anything else.  If the keystroke buffer is (still) empty, return NULL. */
int *get_input(WINDOW *win, size_t input_len)
{
	int *input;

	if (key_buffer_len == 0 && win != NULL)
		read_keys_from(win);

	if (key_buffer_len == 0)
		return NULL;

	/* Limit the request to the number of available codes in the buffer. */
	if (input_len > key_buffer_len)
		input_len = key_buffer_len;

	/* Copy input_len codes from the head of the keystroke buffer. */
	input = (int *)nmalloc(input_len * sizeof(int));
	memcpy(input, key_buffer, input_len * sizeof(int));
	key_buffer_len -= input_len;

	/* If the buffer still contains keystrokes, move them to the front. */
	if (key_buffer_len > 0)
		memmove(key_buffer, key_buffer + input_len, key_buffer_len *
				sizeof(int));

	return input;
}

/* Read in a single keystroke, ignoring any that are invalid. */
int get_kbinput(WINDOW *win, bool showcursor)
{
	int kbinput = ERR;

	reveal_cursor = showcursor;

	/* Extract one keystroke from the input stream. */
	while (kbinput == ERR)
		kbinput = parse_kbinput(win);

	/* If we read from the edit window, blank the statusbar if needed. */
	if (win == edit)
		check_statusblank();

	return kbinput;
}

/* Extract a single keystroke from the input stream.  Translate escape
 * sequences and extended keypad codes into their corresponding values.
 * Set meta_key to TRUE when appropriate.  Supported extended keypad values
 * are: [arrow key], Ctrl-[arrow key], Shift-[arrow key], Enter, Backspace,
 * the editing keypad (Insert, Delete, Home, End, PageUp, and PageDown),
 * the function keys (F1-F16), and the numeric keypad with NumLock off. */
int parse_kbinput(WINDOW *win)
{
	static int escapes = 0;
	static bool double_esc = FALSE;
	int *kbinput, keycode, retval = ERR;

	meta_key = FALSE;
	shift_held = FALSE;

	/* Read in a character. */
	kbinput = get_input(win, 1);

	if (kbinput == NULL && !waiting_mode)
		return ERR;

	while (kbinput == NULL)
		kbinput = get_input(win, 1);

	keycode = *kbinput;
	free(kbinput);

	if (keycode == ERR)
		return ERR;

	if (keycode == ESC_CODE) {
		/* Increment the escape counter, but trim an overabundance. */
		escapes++;
		if (escapes > 3)
			escapes = 1;
		/* Take note when an Esc arrived by itself. */
		solitary = (key_buffer_len == 0);
		return ERR;
	}

	switch (escapes) {
		case 0:
			/* One non-escape: normal input mode. */
			retval = keycode;
			break;
		case 1:
			if (keycode >= 0x80)
				retval = keycode;
			else if (keycode == TAB_CODE)
				retval = SHIFT_TAB;
			else if ((keycode != 'O' && keycode != 'o' && keycode != '[') ||
						key_buffer_len == 0 || *key_buffer == ESC_CODE) {
				/* One escape followed by a single non-escape:
				 * meta key sequence mode. */
				if (!solitary || (keycode >= 0x20 && keycode < 0x7F))
					meta_key = TRUE;
				retval = tolower(keycode);
			} else
				/* One escape followed by a non-escape, and there
				 * are more codes waiting: escape sequence mode. */
				retval = parse_escape_sequence(win, keycode);
			escapes = 0;
			break;
		case 2:
			if (double_esc) {
				/* An "ESC ESC [ X" sequence from Option+arrow, or
				 * an "ESC ESC [ x" sequence from Shift+Alt+arrow. */
				switch (keycode) {
					case 'A':
						retval = KEY_HOME;
						break;
					case 'B':
						retval = KEY_END;
						break;
					case 'C':
						retval = CONTROL_RIGHT;
						break;
					case 'D':
						retval = CONTROL_LEFT;
						break;
#ifndef NANO_TINY
					case 'a':
						retval = shiftaltup;
						break;
					case 'b':
						retval = shiftaltdown;
						break;
					case 'c':
						retval = shiftaltright;
						break;
					case 'd':
						retval = shiftaltleft;
						break;
#endif
				}
				double_esc = FALSE;
				escapes = 0;
			} else if (key_buffer_len == 0) {
				if ('0' <= keycode && ((keycode <= '2' && digit_count == 0) ||
										(keycode <= '9' && digit_count > 0))) {
					/* Two escapes followed by one or more decimal
					 * digits, and there aren't any other codes
					 * waiting: byte sequence mode.  If the range of the
					 * byte sequence is limited to 2XX, interpret it. */
					int byte = get_byte_kbinput(keycode);

					/* If the decimal byte value is complete, convert it and
					 * put the obtained byte(s) back into the input buffer. */
					if (byte != ERR) {
						char *multibyte;
						int count, onebyte, i;

						/* Convert the decimal code to one or two bytes. */
						multibyte = make_mbchar((long)byte, &count);

						/* Insert the byte(s) into the input buffer. */
						for (i = count; i > 0 ; i--) {
							onebyte = (unsigned char)multibyte[i - 1];
							put_back(onebyte);
						}

						free(multibyte);

						escapes = 0;
					}
				} else {
					if (digit_count == 0)
						/* Two escapes followed by a non-digit: meta key
						 * or control character sequence mode. */
						if (!solitary) {
							meta_key = TRUE;
							retval = tolower(keycode);
						} else
							retval = get_control_kbinput(keycode);
					else {
						/* An invalid digit in the middle of a byte
						 * sequence: reset the byte sequence counter
						 * and save the code we got as the result. */
						digit_count = 0;
						retval = keycode;
					}
					escapes = 0;
				}
			} else if (keycode == '[' && key_buffer_len > 0 &&
						(('A' <= *key_buffer && *key_buffer <= 'D') ||
						('a' <= *key_buffer && *key_buffer <= 'd'))) {
				/* An iTerm2/Eterm/rxvt sequence: ^[ ^[ [ X. */
				double_esc = TRUE;
			} else {
				/* Two escapes followed by a non-escape, and there are more
				 * codes waiting: combined meta and escape sequence mode. */
				retval = parse_escape_sequence(win, keycode);
				meta_key = TRUE;
				escapes = 0;
			}
			break;
		case 3:
			if (key_buffer_len == 0) {
				if (!solitary) {
					meta_key = TRUE;
					retval = tolower(keycode);
				} else
					/* Three escapes followed by a non-escape, and no
					 * other codes are waiting: normal input mode. */
					retval = keycode;
			} else
				/* Three escapes followed by a non-escape, and more
				 * codes are waiting: combined control character and
				 * escape sequence mode.  First interpret the escape
				 * sequence, then the result as a control sequence. */
				retval = get_control_kbinput(
						parse_escape_sequence(win, keycode));
			escapes = 0;
			break;
	}

	if (retval == ERR)
		return ERR;

	if (retval == controlleft)
		return CONTROL_LEFT;
	else if (retval == controlright)
		return CONTROL_RIGHT;
	else if (retval == controlup)
		return CONTROL_UP;
	else if (retval == controldown)
		return CONTROL_DOWN;
	else if (retval == controlhome)
		return CONTROL_HOME;
	else if (retval == controlend)
		return CONTROL_END;
#ifndef NANO_TINY
	else if (retval == controldelete)
		return CONTROL_DELETE;
	else if (retval == controlshiftdelete)
		return CONTROL_SHIFT_DELETE;
	else if (retval == shiftup) {
		shift_held = TRUE;
		return KEY_UP;
	} else if (retval == shiftdown) {
		shift_held = TRUE;
		return KEY_DOWN;
	} else if (retval == shiftcontrolleft) {
		shift_held = TRUE;
		return CONTROL_LEFT;
	} else if (retval == shiftcontrolright) {
		shift_held = TRUE;
		return CONTROL_RIGHT;
	} else if (retval == shiftcontrolup) {
		shift_held = TRUE;
		return CONTROL_UP;
	} else if (retval == shiftcontroldown) {
		shift_held = TRUE;
		return CONTROL_DOWN;
	} else if (retval == shiftcontrolhome) {
		shift_held = TRUE;
		return CONTROL_HOME;
	} else if (retval == shiftcontrolend) {
		shift_held = TRUE;
		return CONTROL_END;
	} else if (retval == altleft)
		return ALT_LEFT;
	else if (retval == altright)
		return ALT_RIGHT;
	else if (retval == altup)
		return ALT_UP;
	else if (retval == altdown)
		return ALT_DOWN;
	else if (retval == altdelete)
		return ALT_DELETE;
	else if (retval == shiftaltleft) {
		shift_held = TRUE;
		return KEY_HOME;
	} else if (retval == shiftaltright) {
		shift_held = TRUE;
		return KEY_END;
	} else if (retval == shiftaltup) {
		shift_held = TRUE;
		return KEY_PPAGE;
	} else if (retval == shiftaltdown) {
		shift_held = TRUE;
		return KEY_NPAGE;
	}
#endif

#ifdef __linux__
	/* When not running under X, check for the bare arrow keys whether
	 * Shift/Ctrl/Alt are being held together with them. */
	unsigned char modifiers = 6;

	/* Modifiers are: Alt (8), Ctrl (4), Shift (1). */
	if (on_a_vt && ioctl(0, TIOCLINUX, &modifiers) >= 0) {
#ifndef NANO_TINY
		/* Is Delete pressed together with Shift or Shift+Ctrl? */
		if (retval == KEY_DC) {
			if (modifiers == 0x01)
				return SHIFT_DELETE;
			if (modifiers == 0x05)
				return CONTROL_SHIFT_DELETE;
		}
		/* Is Shift being held? */
		if (modifiers & 0x01) {
			if (retval == TAB_CODE)
				return SHIFT_TAB;
			shift_held = TRUE;
		}
		/* Is Alt being held? */
		if (modifiers == 0x08) {
			if (retval == KEY_DC)
				return ALT_DELETE;
			if (retval == KEY_UP)
				return ALT_UP;
			if (retval == KEY_DOWN)
				return ALT_DOWN;
		}
#endif
		/* Is Ctrl being held? */
		if (modifiers & 0x04) {
			if (retval == KEY_UP)
				return CONTROL_UP;
			else if (retval == KEY_DOWN)
				return CONTROL_DOWN;
			else if (retval == KEY_LEFT)
				return CONTROL_LEFT;
			else if (retval == KEY_RIGHT)
				return CONTROL_RIGHT;
			else if (retval == KEY_HOME)
				return CONTROL_HOME;
			else if (retval == KEY_END)
				return CONTROL_END;
			else if (retval == KEY_DC)
				return CONTROL_DELETE;
		}
#ifndef NANO_TINY
		/* Are both Shift and Alt being held? */
		if ((modifiers & 0x09) == 0x09) {
			if (retval == KEY_UP)
				return KEY_PPAGE;
			else if (retval == KEY_DOWN)
				return KEY_NPAGE;
			else if (retval == KEY_LEFT)
				return KEY_HOME;
			else if (retval == KEY_RIGHT)
				return KEY_END;
		}
#endif
	}
#endif /* __linux__ */

#ifndef NANO_TINY
	/* When <Tab> is pressed while the mark is on, do an indent. */
	if (retval == TAB_CODE && openfile->mark && currmenu == MMAIN) {
		const keystruct *command = first_sc_for(MMAIN, do_indent);

		meta_key = command->meta;
		return command->keycode;
	}
#endif

	switch (retval) {
#ifdef KEY_SLEFT  /* Slang doesn't support KEY_SLEFT. */
		case KEY_SLEFT:
			shift_held = TRUE;
			return KEY_LEFT;
#endif
#ifdef KEY_SRIGHT  /* Slang doesn't support KEY_SRIGHT. */
		case KEY_SRIGHT:
			shift_held = TRUE;
			return KEY_RIGHT;
#endif
#ifdef KEY_SR
#ifdef KEY_SUP  /* ncurses and Slang don't support KEY_SUP. */
		case KEY_SUP:
#endif
		case KEY_SR:    /* Scroll backward, on Xfce4-terminal. */
			shift_held = TRUE;
			return KEY_UP;
#endif
#ifdef KEY_SF
#ifdef KEY_SDOWN  /* ncurses and Slang don't support KEY_SDOWN. */
		case KEY_SDOWN:
#endif
		case KEY_SF:    /* Scroll forward, on Xfce4-terminal. */
			shift_held = TRUE;
			return KEY_DOWN;
#endif
#ifdef KEY_SHOME  /* HP-UX 10-11 and Slang don't support KEY_SHOME. */
		case KEY_SHOME:
#endif
		case SHIFT_HOME:
			shift_held = TRUE;
		case KEY_A1:    /* Home (7) on keypad with NumLock off. */
			return KEY_HOME;
#ifdef KEY_SEND  /* HP-UX 10-11 and Slang don't support KEY_SEND. */
		case KEY_SEND:
#endif
		case SHIFT_END:
			shift_held = TRUE;
		case KEY_C1:    /* End (1) on keypad with NumLock off. */
			return KEY_END;
#ifdef KEY_EOL
		case KEY_EOL:    /* Ctrl+End on rxvt-unicode. */
			return CONTROL_END;
#endif
#ifndef NANO_TINY
#ifdef KEY_SPREVIOUS
		case KEY_SPREVIOUS:
#endif
		case SHIFT_PAGEUP:    /* Fake key, from Shift+Alt+Up. */
			shift_held = TRUE;
#endif
		case KEY_A3:    /* PageUp (9) on keypad with NumLock off. */
			return KEY_PPAGE;
#ifndef NANO_TINY
#ifdef KEY_SNEXT
		case KEY_SNEXT:
#endif
		case SHIFT_PAGEDOWN:    /* Fake key, from Shift+Alt+Down. */
			shift_held = TRUE;
#endif
		case KEY_C3:    /* PageDown (3) on keypad with NumLock off. */
			return KEY_NPAGE;
		/* When requested, swap meanings of keycodes for <Bsp> and <Del>. */
		case DEL_CODE:
		case KEY_BACKSPACE:
			return (ISSET(REBIND_DELETE) ? KEY_DC : KEY_BACKSPACE);
		case KEY_DC:
			return (ISSET(REBIND_DELETE) ? KEY_BACKSPACE : KEY_DC);
#ifdef KEY_SDC  /* Slang doesn't support KEY_SDC. */
		case KEY_SDC:
			return SHIFT_DELETE;
#endif
#ifdef KEY_SIC  /* Slang doesn't support KEY_SIC. */
		case KEY_SIC:
			return the_code_for(do_insertfile_void, KEY_IC);
#endif
#ifdef KEY_SBEG  /* Slang doesn't support KEY_SBEG. */
		case KEY_SBEG:
#endif
#ifdef KEY_BEG  /* Slang doesn't support KEY_BEG. */
		case KEY_BEG:
#endif
		case KEY_B2:    /* Center (5) on keypad with NumLock off. */
			return ERR;
#ifdef KEY_CANCEL  /* Slang doesn't support KEY_CANCEL. */
#ifdef KEY_SCANCEL  /* Slang doesn't support KEY_SCANCEL. */
		case KEY_SCANCEL:
#endif
		case KEY_CANCEL:
			return the_code_for(do_cancel, 0x03);
#endif
#ifdef KEY_SUSPEND  /* Slang doesn't support KEY_SUSPEND. */
#ifdef KEY_SSUSPEND  /* Slang doesn't support KEY_SSUSPEND. */
		case KEY_SSUSPEND:
#endif
		case KEY_SUSPEND:
			return the_code_for(do_suspend_void, KEY_SUSPEND);
#endif
#ifdef KEY_BTAB  /* Slang doesn't support KEY_BTAB. */
		case KEY_BTAB:
			return SHIFT_TAB;
#endif
#ifdef PDCURSES
		case KEY_SHIFT_L:
		case KEY_SHIFT_R:
		case KEY_CONTROL_L:
		case KEY_CONTROL_R:
		case KEY_ALT_L:
		case KEY_ALT_R:
			return ERR;
#endif
#ifdef KEY_RESIZE  /* Slang and SunOS 5.7-5.9 don't support KEY_RESIZE. */
		case KEY_RESIZE:
#endif
#if defined(USE_SLANG) && defined(ENABLE_UTF8)
		case KEY_BAD:
#endif
		case KEY_FLUSH:
			return ERR;
	}

	return retval;
}

/* Translate escape sequences, most of which correspond to extended
 * keypad values, into their corresponding key values.  These sequences
 * are generated when the keypad doesn't support the needed keys.
 * Assume that Escape has already been read in. */
int convert_sequence(const int *seq, size_t length, int *consumed)
{
	if (length > 1) {
		*consumed = 2;
		switch (seq[0]) {
			case 'O':
				switch (seq[1]) {
					case '1':
						if (length > 4  && seq[2] == ';') {
		/* <-<-<-<-<-<-<- */
		*consumed = 5;
		switch (seq[3]) {
			case '2':
				switch (seq[4]) {
					case 'A': /* Esc O 1 ; 2 A == Shift-Up on Terminal. */
					case 'B': /* Esc O 1 ; 2 B == Shift-Down on Terminal. */
					case 'C': /* Esc O 1 ; 2 C == Shift-Right on Terminal. */
					case 'D': /* Esc O 1 ; 2 D == Shift-Left on Terminal. */
						shift_held = TRUE;
						return arrow_from_abcd(seq[4]);
					case 'P': /* Esc O 1 ; 2 P == F13 on Terminal. */
						return KEY_F(13);
					case 'Q': /* Esc O 1 ; 2 Q == F14 on Terminal. */
						return KEY_F(14);
					case 'R': /* Esc O 1 ; 2 R == F15 on Terminal. */
						return KEY_F(15);
					case 'S': /* Esc O 1 ; 2 S == F16 on Terminal. */
						return KEY_F(16);
				}
				break;
			case '5':
				switch (seq[4]) {
					case 'A': /* Esc O 1 ; 5 A == Ctrl-Up on Terminal. */
						return CONTROL_UP;
					case 'B': /* Esc O 1 ; 5 B == Ctrl-Down on Terminal. */
						return CONTROL_DOWN;
					case 'C': /* Esc O 1 ; 5 C == Ctrl-Right on Terminal. */
						return CONTROL_RIGHT;
					case 'D': /* Esc O 1 ; 5 D == Ctrl-Left on Terminal. */
						return CONTROL_LEFT;
				}
				break;
		}
		/* ->->->->->->-> */
						}
						break;
					case '2':
						if (length > 2) {
							*consumed = 3;
							switch (seq[2]) {
								case 'P': /* Esc O 2 P == F13 on xterm. */
									return KEY_F(13);
								case 'Q': /* Esc O 2 Q == F14 on xterm. */
									return KEY_F(14);
								case 'R': /* Esc O 2 R == F15 on xterm. */
									return KEY_F(15);
								case 'S': /* Esc O 2 S == F16 on xterm. */
									return KEY_F(16);
							}
						}
						break;
					case '5':
						if (length > 2) {
							*consumed = 3;
							switch (seq[2]) {
								case 'A': /* Esc O 5 A == Ctrl-Up on Haiku. */
									return CONTROL_UP;
								case 'B': /* Esc O 5 B == Ctrl-Down on Haiku. */
									return CONTROL_DOWN;
								case 'C': /* Esc O 5 C == Ctrl-Right on Haiku. */
									return CONTROL_RIGHT;
								case 'D': /* Esc O 5 D == Ctrl-Left on Haiku. */
									return CONTROL_LEFT;
							}
						}
						break;
					case 'A': /* Esc O A == Up on VT100/VT320/xterm. */
					case 'B': /* Esc O B == Down on VT100/VT320/xterm. */
					case 'C': /* Esc O C == Right on VT100/VT320/xterm. */
					case 'D': /* Esc O D == Left on VT100/VT320/xterm. */
						return arrow_from_abcd(seq[1]);
					case 'E': /* Esc O E == Center (5) on numeric keypad
							   * with NumLock off on xterm. */
						return KEY_B2;
					case 'F': /* Esc O F == End on xterm/Terminal. */
						return KEY_END;
					case 'H': /* Esc O H == Home on xterm/Terminal. */
						return KEY_HOME;
					case 'M': /* Esc O M == Enter on numeric keypad with
							   * NumLock off on VT100/VT220/VT320/xterm/
							   * rxvt/Eterm. */
						return KEY_ENTER;
					case 'P': /* Esc O P == F1 on VT100/VT220/VT320/Mach console. */
						return KEY_F(1);
					case 'Q': /* Esc O Q == F2 on VT100/VT220/VT320/Mach console. */
						return KEY_F(2);
					case 'R': /* Esc O R == F3 on VT100/VT220/VT320/Mach console. */
						return KEY_F(3);
					case 'S': /* Esc O S == F4 on VT100/VT220/VT320/Mach console. */
						return KEY_F(4);
					case 'T': /* Esc O T == F5 on Mach console. */
						return KEY_F(5);
					case 'U': /* Esc O U == F6 on Mach console. */
						return KEY_F(6);
					case 'V': /* Esc O V == F7 on Mach console. */
						return KEY_F(7);
					case 'W': /* Esc O W == F8 on Mach console. */
						return KEY_F(8);
					case 'X': /* Esc O X == F9 on Mach console. */
						return KEY_F(9);
					case 'Y': /* Esc O Y == F10 on Mach console. */
						return KEY_F(10);
					case 'a': /* Esc O a == Ctrl-Up on rxvt. */
						return CONTROL_UP;
					case 'b': /* Esc O b == Ctrl-Down on rxvt. */
						return CONTROL_DOWN;
					case 'c': /* Esc O c == Ctrl-Right on rxvt. */
						return CONTROL_RIGHT;
					case 'd': /* Esc O d == Ctrl-Left on rxvt. */
						return CONTROL_LEFT;
					case 'j': /* Esc O j == '*' on numeric keypad with
							   * NumLock off on VT100/VT220/VT320/xterm/
							   * rxvt/Eterm/Terminal. */
						return '*';
					case 'k': /* Esc O k == '+' on the same. */
						return '+';
					case 'l': /* Esc O l == ',' on the same. */
						return ',';
					case 'm': /* Esc O m == '-' on the same. */
						return '-';
					case 'n': /* Esc O n == Delete (.) on the same. */
						return KEY_DC;
					case 'o': /* Esc O o == '/' on the same. */
						return '/';
					case 'p': /* Esc O p == Insert (0) on numeric keypad
							   * with NumLock off on VT100/VT220/VT320/
							   * rxvt/Eterm/Terminal. */
						return KEY_IC;
					case 'q': /* Esc O q == End (1) on the same. */
						return KEY_END;
					case 'r': /* Esc O r == Down (2) on the same. */
						return KEY_DOWN;
					case 's': /* Esc O s == PageDown (3) on the same. */
						return KEY_NPAGE;
					case 't': /* Esc O t == Left (4) on the same. */
						return KEY_LEFT;
					case 'u': /* Esc O u == Center (5) on numeric keypad
							   * with NumLock off on VT100/VT220/VT320/
							   * rxvt/Eterm. */
						return KEY_B2;
					case 'v': /* Esc O v == Right (6) on numeric keypad
							   * with NumLock off on VT100/VT220/VT320/
							   * rxvt/Eterm/Terminal. */
						return KEY_RIGHT;
					case 'w': /* Esc O w == Home (7) on the same. */
						return KEY_HOME;
					case 'x': /* Esc O x == Up (8) on the same. */
						return KEY_UP;
					case 'y': /* Esc O y == PageUp (9) on the same. */
						return KEY_PPAGE;
				}
				break;
			case 'o':
				switch (seq[1]) {
					case 'a': /* Esc o a == Ctrl-Up on Eterm. */
						return CONTROL_UP;
					case 'b': /* Esc o b == Ctrl-Down on Eterm. */
						return CONTROL_DOWN;
					case 'c': /* Esc o c == Ctrl-Right on Eterm. */
						return CONTROL_RIGHT;
					case 'd': /* Esc o d == Ctrl-Left on Eterm. */
						return CONTROL_LEFT;
				}
				break;
			case '[':
				if (seq[1] < '9')
					*consumed = 3;
				switch (seq[1]) {
					case '1':
						if (length > 2 && seq[2] == '~')
							/* Esc [ 1 ~ == Home on VT320/Linux console. */
							return KEY_HOME;
						else if (length > 3 && seq[3] == '~') {
							*consumed = 4;
							switch (seq[2]) {
								case '1': /* Esc [ 1 1 ~ == F1 on rxvt/Eterm. */
									return KEY_F(1);
								case '2': /* Esc [ 1 2 ~ == F2 on rxvt/Eterm. */
									return KEY_F(2);
								case '3': /* Esc [ 1 3 ~ == F3 on rxvt/Eterm. */
									return KEY_F(3);
								case '4': /* Esc [ 1 4 ~ == F4 on rxvt/Eterm. */
									return KEY_F(4);
								case '5': /* Esc [ 1 5 ~ == F5 on xterm/rxvt/Eterm. */
									return KEY_F(5);
								case '7': /* Esc [ 1 7 ~ == F6 on VT220/VT320/
										   * Linux console/xterm/rxvt/Eterm. */
									return KEY_F(6);
								case '8': /* Esc [ 1 8 ~ == F7 on the same. */
									return KEY_F(7);
								case '9': /* Esc [ 1 9 ~ == F8 on the same. */
									return KEY_F(8);
							}
						} else if (length > 4 && seq[2] == ';') {
		/* <-<-<-<-<-<-<- */
		*consumed = 5;
		switch (seq[3]) {
			case '2':
				switch (seq[4]) {
					case 'A': /* Esc [ 1 ; 2 A == Shift-Up on xterm. */
					case 'B': /* Esc [ 1 ; 2 B == Shift-Down on xterm. */
					case 'C': /* Esc [ 1 ; 2 C == Shift-Right on xterm. */
					case 'D': /* Esc [ 1 ; 2 D == Shift-Left on xterm. */
						shift_held = TRUE;
						return arrow_from_abcd(seq[4]);
#ifndef NANO_TINY
					case 'F': /* Esc [ 1 ; 2 F == Shift-End on xterm. */
						return SHIFT_END;
					case 'H': /* Esc [ 1 ; 2 H == Shift-Home on xterm. */
						return SHIFT_HOME;
#endif
				}
				break;
#ifndef NANO_TINY
			case '9': /* To accommodate iTerm2 in "xterm mode". */
			case '3':
				switch (seq[4]) {
					case 'A': /* Esc [ 1 ; 3 A == Alt-Up on xterm. */
						return ALT_UP;
					case 'B': /* Esc [ 1 ; 3 B == Alt-Down on xterm. */
						return ALT_DOWN;
					case 'C': /* Esc [ 1 ; 3 C == Alt-Right on xterm. */
						return ALT_RIGHT;
					case 'D': /* Esc [ 1 ; 3 D == Alt-Left on xterm. */
						return ALT_LEFT;
				}
				break;
			case '4':
				/* When the arrow keys are held together with Shift+Meta,
				 * act as if they are Home/End/PgUp/PgDown with Shift. */
				switch (seq[4]) {
					case 'A': /* Esc [ 1 ; 4 A == Shift-Alt-Up on xterm. */
						return SHIFT_PAGEUP;
					case 'B': /* Esc [ 1 ; 4 B == Shift-Alt-Down on xterm. */
						return SHIFT_PAGEDOWN;
					case 'C': /* Esc [ 1 ; 4 C == Shift-Alt-Right on xterm. */
						return SHIFT_END;
					case 'D': /* Esc [ 1 ; 4 D == Shift-Alt-Left on xterm. */
						return SHIFT_HOME;
				}
				break;
#endif
			case '5':
				switch (seq[4]) {
					case 'A': /* Esc [ 1 ; 5 A == Ctrl-Up on xterm. */
						return CONTROL_UP;
					case 'B': /* Esc [ 1 ; 5 B == Ctrl-Down on xterm. */
						return CONTROL_DOWN;
					case 'C': /* Esc [ 1 ; 5 C == Ctrl-Right on xterm. */
						return CONTROL_RIGHT;
					case 'D': /* Esc [ 1 ; 5 D == Ctrl-Left on xterm. */
						return CONTROL_LEFT;
					case 'F': /* Esc [ 1 ; 5 F == Ctrl-End on xterm. */
						return CONTROL_END;
					case 'H': /* Esc [ 1 ; 5 H == Ctrl-Home on xterm. */
						return CONTROL_HOME;
				}
				break;
#ifndef NANO_TINY
			case '6':
				switch (seq[4]) {
					case 'A': /* Esc [ 1 ; 6 A == Shift-Ctrl-Up on xterm. */
						return shiftcontrolup;
					case 'B': /* Esc [ 1 ; 6 B == Shift-Ctrl-Down on xterm. */
						return shiftcontroldown;
					case 'C': /* Esc [ 1 ; 6 C == Shift-Ctrl-Right on xterm. */
						return shiftcontrolright;
					case 'D': /* Esc [ 1 ; 6 D == Shift-Ctrl-Left on xterm. */
						return shiftcontrolleft;
					case 'F': /* Esc [ 1 ; 6 F == Shift-Ctrl-End on xterm. */
						return shiftcontrolend;
					case 'H': /* Esc [ 1 ; 6 H == Shift-Ctrl-Home on xterm. */
						return shiftcontrolhome;
				}
				break;
#endif
		}
		/* ->->->->->->-> */
						}
						break;
					case '2':
						if (length > 3 && seq[3] == '~') {
							*consumed = 4;
							switch (seq[2]) {
								case '0': /* Esc [ 2 0 ~ == F9 on VT220/VT320/
										   * Linux console/xterm/rxvt/Eterm. */
									return KEY_F(9);
								case '1': /* Esc [ 2 1 ~ == F10 on the same. */
									return KEY_F(10);
								case '3': /* Esc [ 2 3 ~ == F11 on the same. */
									return KEY_F(11);
								case '4': /* Esc [ 2 4 ~ == F12 on the same. */
									return KEY_F(12);
								case '5': /* Esc [ 2 5 ~ == F13 on VT220/VT320/
										   * Linux console/rxvt/Eterm. */
									return KEY_F(13);
								case '6': /* Esc [ 2 6 ~ == F14 on the same. */
									return KEY_F(14);
								case '8': /* Esc [ 2 8 ~ == F15 on the same. */
									return KEY_F(15);
								case '9': /* Esc [ 2 9 ~ == F16 on the same. */
									return KEY_F(16);
							}
						} else if (length > 2 && seq[2] == '~')
							/* Esc [ 2 ~ == Insert on VT220/VT320/
							 * Linux console/xterm/Terminal. */
							return KEY_IC;
						else if (length > 4 && seq[2] == ';' && seq[4] == '~')
							/* Esc [ 2 ; x ~ == modified Insert on xterm. */
							*consumed = 5;
						break;
					case '3': /* Esc [ 3 ~ == Delete on VT220/VT320/
							   * Linux console/xterm/Terminal. */
						if (length > 2 && seq[2] == '~')
							return KEY_DC;
						if (length > 4 && seq[2] == ';' && seq[4] == '~') {
							*consumed = 5;
#ifndef NANO_TINY
							if (seq[3] == '2')
								/* Esc [ 3 ; 2 ~ == Shift-Delete on xterm/Terminal. */
								return SHIFT_DELETE;
							if (seq[3] == '3')
								/* Esc [ 3 ; 3 ~ == Alt-Delete on xterm/rxvt/Eterm/Terminal. */
								return ALT_DELETE;
							if (seq[3] == '5')
								/* Esc [ 3 ; 5 ~ == Ctrl-Delete on xterm. */
								return CONTROL_DELETE;
							if (seq[3] == '6')
								/* Esc [ 3 ; 6 ~ == Ctrl-Shift-Delete on xterm. */
								return controlshiftdelete;
#endif
						}
#ifndef NANO_TINY
						if (length > 2 && seq[2] == '$')
							/* Esc [ 3 $ == Shift-Delete on urxvt. */
							return SHIFT_DELETE;
						if (length > 2 && seq[2] == '^')
							/* Esc [ 3 ^ == Ctrl-Delete on urxvt. */
							return CONTROL_DELETE;
						if (length > 2 && seq[2] == '@')
							/* Esc [ 3 @ == Ctrl-Shift-Delete on urxvt. */
							return controlshiftdelete;
#endif
						break;
					case '4': /* Esc [ 4 ~ == End on VT220/VT320/
							   * Linux console/xterm. */
						if (length > 2 && seq[2] == '~')
							return KEY_END;
						break;
					case '5': /* Esc [ 5 ~ == PageUp on VT220/VT320/
							   * Linux console/xterm/Terminal;
							   * Esc [ 5 ^ == PageUp on Eterm. */
						if (length > 2 && (seq[2] == '~' || seq[2] == '^'))
							return KEY_PPAGE;
						else if (length > 4 && seq[2] == ';' && seq[4] == '~') {
							*consumed = 5;
#ifndef NANO_TINY
							if (seq[3] == '2')
								return shiftaltup;
#endif
						}
						break;
					case '6': /* Esc [ 6 ~ == PageDown on VT220/VT320/
							   * Linux console/xterm/Terminal;
							   * Esc [ 6 ^ == PageDown on Eterm. */
						if (length > 2 && (seq[2] == '~' || seq[2] == '^'))
							return KEY_NPAGE;
						else if (length > 4 && seq[2] == ';' && seq[4] == '~') {
							*consumed = 5;
#ifndef NANO_TINY
							if (seq[3] == '2')
								return shiftaltdown;
#endif
						}
						break;
					case '7': /* Esc [ 7 ~ == Home on Eterm/rxvt;
							   * Esc [ 7 $ == Shift-Home on Eterm/rxvt;
							   * Esc [ 7 ^ == Control-Home on Eterm/rxvt;
							   * Esc [ 7 @ == Shift-Control-Home on same. */
						if (length > 2 && seq[2] == '~')
							return KEY_HOME;
						else if (length > 2 && seq[2] == '$')
							return SHIFT_HOME;
						else if (length > 2 && seq[2] == '^')
							return CONTROL_HOME;
#ifndef NANO_TINY
						else if (length > 2 && seq[2] == '@')
							return shiftcontrolhome;
#endif
						break;
					case '8': /* Esc [ 8 ~ == End on Eterm/rxvt;
							   * Esc [ 8 $ == Shift-End on Eterm/rxvt;
							   * Esc [ 8 ^ == Control-End on Eterm/rxvt;
							   * Esc [ 8 @ == Shift-Control-End on same. */
						if (length > 2 && seq[2] == '~')
							return KEY_END;
						else if (length > 2 && seq[2] == '$')
							return SHIFT_END;
						else if (length > 2 && seq[2] == '^')
							return CONTROL_END;
#ifndef NANO_TINY
						else if (length > 2 && seq[2] == '@')
							return shiftcontrolend;
#endif
						break;
					case '9': /* Esc [ 9 == Delete on Mach console. */
						return KEY_DC;
					case '@': /* Esc [ @ == Insert on Mach console. */
						return KEY_IC;
					case 'A': /* Esc [ A == Up on ANSI/VT220/Linux console/rxvt/
							   * FreeBSD console/Mach console/Eterm/Terminal. */
					case 'B': /* Esc [ B == Down on the same. */
					case 'C': /* Esc [ C == Right on the same. */
					case 'D': /* Esc [ D == Left on the same. */
						return arrow_from_abcd(seq[1]);
					case 'E': /* Esc [ E == Center (5) on numeric keypad with
							   * NumLock off on FreeBSD console/Terminal. */
						return KEY_B2;
					case 'F': /* Esc [ F == End on FreeBSD console/Eterm. */
						return KEY_END;
					case 'G': /* Esc [ G == PageDown on FreeBSD console. */
						return KEY_NPAGE;
					case 'H': /* Esc [ H == Home on ANSI/VT220/FreeBSD
							   * console/Mach console/Eterm. */
						return KEY_HOME;
					case 'I': /* Esc [ I == PageUp on FreeBSD console. */
						return KEY_PPAGE;
					case 'L': /* Esc [ L == Insert on ANSI/FreeBSD console. */
						return KEY_IC;
					case 'M': /* Esc [ M == F1 on FreeBSD console. */
						return KEY_F(1);
					case 'N': /* Esc [ N == F2 on FreeBSD console. */
						return KEY_F(2);
					case 'O':
						if (length > 2) {
							*consumed = 3;
							switch (seq[2]) {
								case 'P': /* Esc [ O P == F1 on xterm. */
									return KEY_F(1);
								case 'Q': /* Esc [ O Q == F2 on xterm. */
									return KEY_F(2);
								case 'R': /* Esc [ O R == F3 on xterm. */
									return KEY_F(3);
								case 'S': /* Esc [ O S == F4 on xterm. */
									return KEY_F(4);
							}
						} else
							/* Esc [ O == F3 on FreeBSD console. */
							return KEY_F(3);
						break;
					case 'P': /* Esc [ P == F4 on FreeBSD console. */
						return KEY_F(4);
					case 'Q': /* Esc [ Q == F5 on FreeBSD console. */
						return KEY_F(5);
					case 'R': /* Esc [ R == F6 on FreeBSD console. */
						return KEY_F(6);
					case 'S': /* Esc [ S == F7 on FreeBSD console. */
						return KEY_F(7);
					case 'T': /* Esc [ T == F8 on FreeBSD console. */
						return KEY_F(8);
					case 'U': /* Esc [ U == PageDown on Mach console. */
						return KEY_NPAGE;
					case 'V': /* Esc [ V == PageUp on Mach console. */
						return KEY_PPAGE;
					case 'W': /* Esc [ W == F11 on FreeBSD console. */
						return KEY_F(11);
					case 'X': /* Esc [ X == F12 on FreeBSD console. */
						return KEY_F(12);
					case 'Y': /* Esc [ Y == End on Mach console. */
						return KEY_END;
					case 'Z': /* Esc [ Z == Shift-Tab on ANSI/Linux console/
							   * FreeBSD console/xterm/rxvt/Terminal. */
						return SHIFT_TAB;
					case 'a': /* Esc [ a == Shift-Up on rxvt/Eterm. */
					case 'b': /* Esc [ b == Shift-Down on rxvt/Eterm. */
					case 'c': /* Esc [ c == Shift-Right on rxvt/Eterm. */
					case 'd': /* Esc [ d == Shift-Left on rxvt/Eterm. */
						shift_held = TRUE;
						return arrow_from_abcd(seq[1]);
					case '[':
						if (length > 2) {
							*consumed = 3;
							switch (seq[2]) {
								case 'A': /* Esc [ [ A == F1 on Linux console. */
									return KEY_F(1);
								case 'B': /* Esc [ [ B == F2 on Linux console. */
									return KEY_F(2);
								case 'C': /* Esc [ [ C == F3 on Linux console. */
									return KEY_F(3);
								case 'D': /* Esc [ [ D == F4 on Linux console. */
									return KEY_F(4);
								case 'E': /* Esc [ [ E == F5 on Linux console. */
									return KEY_F(5);
							}
						}
						break;
				}
				break;
		}
	}

	return ERR;
}

/* Return the equivalent arrow-key value for the first four letters
 * in the alphabet, common to many escape sequences. */
int arrow_from_abcd(int kbinput)
{
	switch (tolower(kbinput)) {
		case 'a':
			return KEY_UP;
		case 'b':
			return KEY_DOWN;
		case 'c':
			return KEY_RIGHT;
		case 'd':
			return KEY_LEFT;
		default:
			return ERR;
	}
}

/* Interpret the escape sequence in the keystroke buffer, the first
 * character of which is kbinput.  Assume that the keystroke buffer
 * isn't empty, and that the initial escape has already been read in. */
int parse_escape_sequence(WINDOW *win, int kbinput)
{
	int retval, *sequence, length, consumed;

	/* Put back the non-escape code, then grab at most five integers
	 * (the longest possible escape sequence) from the keybuffer and
	 * translate the sequence into its corresponding keycode. */
	put_back(kbinput);
	length = (key_buffer_len < 5 ? key_buffer_len : 5);
	sequence = get_input(NULL, length);
	retval = convert_sequence(sequence, length, &consumed);

	/* If not all grabbed integers were consumed, put the leftovers back. */
	for (int i = length - 1; i >= consumed; i--)
		put_back(sequence[i]);

	free(sequence);

	/* If we got an unrecognized escape sequence, notify the user. */
	if (retval == ERR && win == edit) {
		/* TRANSLATORS: This refers to a sequence of escape codes
		 * (from the keyboard) that nano does not recognize. */
		statusline(ALERT, _("Unknown sequence"));
		suppress_cursorpos = FALSE;
		lastmessage = HUSH;
		if (currmenu == MMAIN) {
			place_the_cursor();
#ifdef __NetBSD__
			wnoutrefresh(edit);  /* Needed for correct placement on NetBSD. */
#endif
			curs_set(1);
		}
	}

	return retval;
}

/* Turn a three-digit decimal number (from 000 to 255) into its corresponding
 * byte value. */
int get_byte_kbinput(int kbinput)
{
	static int byte = 0;
	int retval = ERR;

	/* Check that the given digit is within the allowed range for its position.
	 * If yes, store it.  If no, return the digit (or character) itself. */
	switch (++digit_count) {
		case 1:
			/* The first digit (the 100's position) must be from zero to two. */
			if ('0' <= kbinput && kbinput <= '2')
				byte = (kbinput - '0') * 100;
			else
				retval = kbinput;
			break;
		case 2:
			/* The second digit (the 10's position) must be from zero to five
			 * if the first was two, and may be any decimal value otherwise. */
			if ((byte < 200 && '0' <= kbinput && kbinput <= '9') ||
								('0' <= kbinput && kbinput <= '5'))
				byte += (kbinput - '0') * 10;
			else
				retval = kbinput;
			break;
		case 3:
			/* The third digit (the 1's position) must be from zero to five
			 * if the first was two and the second was five, and may be any
			 * decimal value otherwise. */
			if ((byte < 250 && '0' <= kbinput && kbinput <= '9') ||
								('0' <= kbinput && kbinput <= '5')) {
				byte += kbinput - '0';
				/* The byte sequence is complete. */
				retval = byte;
			} else
				retval = kbinput;
			break;
	}

	/* If we have a result, reset the counter and the byte holder. */
	if (retval != ERR) {
		digit_count = 0;
		byte = 0;
	}

	return retval;
}

#ifdef ENABLE_UTF8
/* If the character in kbinput is a valid hexadecimal digit, multiply it
 * by factor and add the result to uni, and return ERR to signify okay. */
long add_unicode_digit(int kbinput, long factor, long *uni)
{
	if ('0' <= kbinput && kbinput <= '9')
		*uni += (kbinput - '0') * factor;
	else if ('a' <= tolower(kbinput) && tolower(kbinput) <= 'f')
		*uni += (tolower(kbinput) - 'a' + 10) * factor;
	else
		/* The character isn't hexadecimal; give it as the result. */
		return (long)kbinput;

	return ERR;
}

/* Translate a Unicode sequence: turn a six-digit hexadecimal number
 * (from 000000 to 10FFFF, case-insensitive) into its corresponding
 * multibyte value. */
long get_unicode_kbinput(WINDOW *win, int kbinput)
{
	static int uni_digits = 0;
	static long uni = 0;
	long retval = ERR;

	/* Increment the Unicode digit counter. */
	uni_digits++;

	switch (uni_digits) {
		case 1:
			/* The first digit must be zero or one.  Put it in the
			 * 0x100000's position of the Unicode sequence holder.
			 * Otherwise, return the character itself as the result. */
			if (kbinput == '0' || kbinput == '1')
				uni = (kbinput - '0') * 0x100000;
			else
				retval = kbinput;
			break;
		case 2:
			/* The second digit must be zero if the first was one, but
			 * may be any hexadecimal value if the first was zero. */
			if (kbinput == '0' || uni == 0)
				retval = add_unicode_digit(kbinput, 0x10000, &uni);
			else
				retval = kbinput;
			break;
		case 3:
			/* Later digits may be any hexadecimal value. */
			retval = add_unicode_digit(kbinput, 0x1000, &uni);
			break;
		case 4:
			retval = add_unicode_digit(kbinput, 0x100, &uni);
			break;
		case 5:
			retval = add_unicode_digit(kbinput, 0x10, &uni);
			break;
		case 6:
			retval = add_unicode_digit(kbinput, 0x1, &uni);
			/* If also the sixth digit was a valid hexadecimal value, then
			 * the Unicode sequence is complete, so return it. */
			if (retval == ERR)
				retval = uni;
			break;
	}

	/* Show feedback only when editing, not when at a prompt. */
	if (retval == ERR && win == edit) {
		char partial[7] = "......";

		/* Construct the partial result, right-padding it with dots. */
		snprintf(partial, uni_digits + 1, "%06lX", uni);
		partial[uni_digits] = '.';

		/* TRANSLATORS: This is shown while a six-digit hexadecimal
		 * Unicode character code (%s) is being typed in. */
		statusline(HUSH, _("Unicode Input: %s"), partial);
	}

	/* If we have an end result, reset the Unicode digit counter. */
	if (retval != ERR)
		uni_digits = 0;

	return retval;
}
#endif /* ENABLE_UTF8 */

/* Translate a normal ASCII character into its corresponding control code.
 * The following groups of control keystrokes are equivalent:
 *   Ctrl-2 == Ctrl-@ == Ctrl-` == Ctrl-Space
 *   Ctrl-3 == Ctrl-[ == <Esc>
 *   Ctrl-4 == Ctrl-\ == Ctrl-|
 *   Ctrl-5 == Ctrl-]
 *   Ctrl-6 == Ctrl-^ == Ctrl-~
 *   Ctrl-7 == Ctrl-/ == Ctrl-_
 *   Ctrl-8 == Ctrl-? */
int get_control_kbinput(int kbinput)
{
	if ('@' <= kbinput && kbinput <= '_')
		return kbinput - '@';
	if ('`' <= kbinput && kbinput <= '~')
		return kbinput - '`';
	if ('3' <= kbinput && kbinput <= '7')
		return kbinput - 24;
	if (kbinput == '?' || kbinput == '8')
		return DEL_CODE;
	if (kbinput == ' ' || kbinput == '2')
		return 0;
	if (kbinput == '/')
		return 31;

	return kbinput;
}

/* Read in a stream of characters verbatim, and return the length of the
 * string in kbinput_len.  Assume nodelay(win) is FALSE. */
int *get_verbatim_kbinput(WINDOW *win, size_t *kbinput_len)
{
	int *retval;

	/* Turn off flow control characters if necessary so that we can type
	 * them in verbatim, and turn the keypad off if necessary so that we
	 * don't get extended keypad values. */
	if (ISSET(PRESERVE))
		disable_flow_control();
	if (!ISSET(RAW_SEQUENCES))
		keypad(win, FALSE);

	/* Read in one keycode, or one or two escapes. */
	retval = parse_verbatim_kbinput(win, kbinput_len);

	/* If the code is invalid in the current mode, discard it. */
	if (retval != NULL && ((*retval == '\n' && as_an_at) ||
								(*retval == '\0' && !as_an_at))) {
		*kbinput_len = 0;
		beep();
	}

	/* Turn flow control characters back on if necessary and turn the
	 * keypad back on if necessary now that we're done. */
	if (ISSET(PRESERVE))
		enable_flow_control();
	/* Use the global window pointers, because a resize may have freed
	 * the data that the win parameter points to. */
	if (!ISSET(RAW_SEQUENCES)) {
		keypad(edit, TRUE);
		keypad(bottomwin, TRUE);
	}

	return retval;
}

/* Read in one control character (or an iTerm/Eterm/rxvt double Escape),
 * or convert a series of six digits into a Unicode codepoint.  Return
 * in count either 1 (for a control character or the first byte of a
 * multibyte sequence), or 2 (for an iTerm/Eterm/rxvt double Escape). */
int *parse_verbatim_kbinput(WINDOW *win, size_t *count)
{
	int *kbinput;

	reveal_cursor = TRUE;

	/* Read in the first code. */
	while ((kbinput = get_input(win, 1)) == NULL)
		;

#ifndef NANO_TINY
	/* When the window was resized, abort and return nothing. */
	if (*kbinput == KEY_WINCH) {
		free(kbinput);
		*count = 0;
		return NULL;
	}
#endif

	*count = 1;

#ifdef ENABLE_UTF8
	if (using_utf8()) {
		/* Check whether the first code is a valid starter digit: 0 or 1. */
		long unicode = get_unicode_kbinput(win, *kbinput);

		/* If the first code isn't the digit 0 nor 1, put it back. */
		if (unicode != ERR)
			put_back(*kbinput);
		/* Otherwise, continue reading in digits until we have a complete
		 * Unicode value, and put back the corresponding byte(s). */
		else {
			char *multibyte;
			int onebyte, i;

			reveal_cursor = FALSE;

			while (unicode == ERR) {
				free(kbinput);
				while ((kbinput = get_input(win, 1)) == NULL)
					;
				unicode = get_unicode_kbinput(win, *kbinput);
			}

			/* Convert the Unicode value to a multibyte sequence. */
			multibyte = make_mbchar(unicode, (int *)count);

			/* Insert the multibyte sequence into the input buffer. */
			for (i = *count; i > 0 ; i--) {
				onebyte = (unsigned char)multibyte[i - 1];
				put_back(onebyte);
			}

			free(multibyte);
		}
	} else
#endif /* ENABLE_UTF8 */
		/* Put back the first code. */
		put_back(*kbinput);

	free(kbinput);

	/* If this is an iTerm/Eterm/rxvt double escape, take both Escapes. */
	if (key_buffer_len > 3 && *key_buffer == ESC_CODE &&
				key_buffer[1] == ESC_CODE && key_buffer[2] == '[')
		*count = 2;

	return get_input(NULL, *count);
}

#ifdef ENABLE_MOUSE
/* Handle any mouse event that may have occurred.  We currently handle
 * releases/clicks of the first mouse button.  If allow_shortcuts is
 * TRUE, releasing/clicking on a visible shortcut will put back the
 * keystroke associated with that shortcut.  If ncurses supports them,
 * we also handle presses of the fourth mouse button (upward rolls of
 * the mouse wheel) by putting back keystrokes to move up, and presses
 * of the fifth mouse button (downward rolls of the mouse wheel) by
 * putting back keystrokes to move down.  We also store the coordinates
 * of a mouse event that needs further handling in mouse_x and mouse_y.
 * Return -1 on error, 0 if the mouse event needs to be handled, 1 if it's
 * been handled by putting back keystrokes, or 2 if it's been ignored. */
int get_mouseinput(int *mouse_y, int *mouse_x, bool allow_shortcuts)
{
	MEVENT mevent;
	bool in_bottomwin;
	funcstruct *f;

	/* First, get the actual mouse event. */
	if (getmouse(&mevent) == ERR)
		return -1;

	/* Save the screen coordinates where the mouse event took place. */
	*mouse_x = mevent.x - margin;
	*mouse_y = mevent.y;

	in_bottomwin = wenclose(bottomwin, *mouse_y, *mouse_x);

	/* Handle releases/clicks of the first mouse button. */
	if (mevent.bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) {
		/* If we're allowing shortcuts, and the current shortcut list is
		 * being displayed on the last two lines of the screen, and the
		 * first mouse button was released on/clicked inside it, we need
		 * to figure out which shortcut was released on/clicked and put
		 * back the equivalent keystroke(s) for it. */
		if (allow_shortcuts && !ISSET(NO_HELP) && in_bottomwin) {
			int width;
				/* The width of each shortcut item, except the last two. */
			int index;
				/* The calculated index of the clicked item. */
			size_t number;
				/* The number of shortcut items that get displayed. */

			/* Translate the coordinates to become relative to bottomwin. */
			wmouse_trafo(bottomwin, mouse_y, mouse_x, FALSE);

			/* Clicks on the status bar are handled elsewhere, so
			 * restore the untranslated mouse-event coordinates. */
			if (*mouse_y == 0) {
				*mouse_x = mevent.x - margin;
				*mouse_y = mevent.y;
				return 0;
			}

			/* Determine how many shortcuts will be shown. */
			number = length_of_list(currmenu);
			if (number > MAIN_VISIBLE)
				number = MAIN_VISIBLE;

			/* Calculate the width of each non-rightmost shortcut item;
			 * the rightmost ones will "absorb" any remaining slack. */
			if (number < 2)
				width = COLS / (MAIN_VISIBLE / 2);
			else
				width = COLS / ((number / 2) + (number % 2));

			/* Calculate the one-based index in the shortcut list. */
			index = (*mouse_x / width) * 2 + *mouse_y;

			/* Adjust the index if we hit the last two wider ones. */
			if ((index > number) && (*mouse_x % width < COLS % width))
				index -= 2;

			/* Ignore clicks beyond the last shortcut. */
			if (index > number)
				return 2;

			/* Go through the list of functions to determine which
			 * shortcut in the current menu we clicked/released on. */
			for (f = allfuncs; f != NULL; f = f->next) {
				if ((f->menus & currmenu) == 0)
					continue;
				if (first_sc_for(currmenu, f->func) == NULL)
					continue;
				/* Tick off an actually shown shortcut. */
				if (--index == 0)
					break;
			}

			/* And put the corresponding key into the keyboard buffer. */
			if (f != NULL) {
				const keystruct *s = first_sc_for(currmenu, f->func);
				unget_kbinput(s->keycode, s->meta);
			}
			return 1;
		} else
			/* Clicks outside of bottomwin are handled elsewhere. */
			return 0;
	}
#if NCURSES_MOUSE_VERSION >= 2
	/* Handle presses of the fourth mouse button (upward rolls of the
	 * mouse wheel) and presses of the fifth mouse button (downward
	 * rolls of the mouse wheel) . */
	else if (mevent.bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED)) {
		bool in_edit = wenclose(edit, *mouse_y, *mouse_x);

		if (in_bottomwin)
			/* Translate the coordinates to become relative to bottomwin. */
			wmouse_trafo(bottomwin, mouse_y, mouse_x, FALSE);

		if (in_edit || (in_bottomwin && *mouse_y == 0)) {
			/* One roll of the mouse wheel should move three lines. */
			for (int count = 1; count <= 3; count++)
				unget_kbinput((mevent.bstate & BUTTON4_PRESSED) ?
								KEY_UP : KEY_DOWN, FALSE);
			return 1;
		} else
			/* Ignore presses of the fourth and fifth mouse buttons
			 * that aren't on the edit window or the status bar. */
			return 2;
	}
#endif
	/* Ignore all other mouse events. */
	return 2;
}
#endif /* ENABLE_MOUSE */

/* Return the shortcut that corresponds to the values of kbinput (the
 * key itself) and meta_key (whether the key is a meta sequence).  The
 * returned shortcut will be the first in the list that corresponds to
 * the given sequence. */
const keystruct *get_shortcut(int *kbinput)
{
	keystruct *s;

	/* Plain characters cannot be shortcuts, so just skip those. */
	if (!meta_key && ((*kbinput >= 0x20 && *kbinput < 0x7F) ||
						(*kbinput >= 0xA0 && *kbinput <= 0xFF)))
		return NULL;

	for (s = sclist; s != NULL; s = s->next) {
		if ((s->menus & currmenu) && *kbinput == s->keycode &&
										meta_key == s->meta)
			return s;
	}

	return NULL;
}

/* Move (in the given window) to the given row and wipe it clean. */
void blank_row(WINDOW *window, int row)
{
	wmove(window, row, 0);
	wclrtoeol(window);
}

/* Blank the first line of the top portion of the screen. */
void blank_titlebar(void)
{
	mvwprintw(topwin, 0, 0, "%*s", COLS, " ");
}

/* Blank all lines of the middle portion of the screen (the edit window). */
void blank_edit(void)
{
	for (int row = 0; row < editwinrows; row++)
		blank_row(edit, row);
}

/* Blank the first line of the bottom portion of the screen. */
void blank_statusbar(void)
{
	blank_row(bottomwin, 0);
}

/* Wipe the status bar clean and include this in the next screen update. */
void wipe_statusbar(void)
{
	blank_row(bottomwin, 0);
	wnoutrefresh(bottomwin);
}

/* Blank out the two help lines (when they are present). */
void blank_bottombars(void)
{
	if (!ISSET(NO_HELP) && LINES > 4) {
		blank_row(bottomwin, 1);
		blank_row(bottomwin, 2);
	}
}

/* Check if the number of keystrokes needed to blank the statusbar has
 * been pressed.  If so, blank the statusbar, unless constant cursor
 * position display is on and we are in the editing screen. */
void check_statusblank(void)
{
	if (statusblank == 0)
		return;

	statusblank--;

	/* When editing and 'constantshow' is active, skip the blanking. */
	if (currmenu == MMAIN && ISSET(CONSTANT_SHOW))
		return;

	if (statusblank == 0)
		wipe_statusbar();

	/* If the subwindows overlap, make sure to show the edit window now. */
	if (LINES == 1)
		edit_refresh();
}

/* Convert buf into a string that can be displayed on screen.  The caller
 * wants to display buf starting with the given column, and extending for
 * at most span columns.  column is zero-based, and span is one-based, so
 * span == 0 means you get "" returned.  The returned string is dynamically
 * allocated, and should be freed.  If isdata is TRUE, the caller might put
 * "<" at the beginning or ">" at the end of the line if it's too long.  If
 * isprompt is TRUE, the caller might put ">" at the end of the line if it's
 * too long. */
char *display_string(const char *buf, size_t column, size_t span,
						bool isdata, bool isprompt)
{
	size_t start_index = actual_x(buf, column);
		/* The index of the first character that the caller wishes to show. */
	size_t start_col = wideness(buf, start_index);
		/* The actual column where that first character starts. */
	char *converted;
		/* The expanded string we will return. */
	size_t index = 0;
		/* Current position in converted. */
	size_t beyond = column + span;
		/* The column number just beyond the last shown character. */

#ifdef USING_OLD_NCURSES
	seen_wide = FALSE;
#endif
	buf += start_index;

	/* Allocate enough space for converting the relevant part of the line. */
	converted = charalloc(strlen(buf) * (MAXCHARLEN + tabsize) + 1);

#ifndef NANO_TINY
	if (span > HIGHEST_POSITIVE) {
		statusline(ALERT, "Span has underflowed -- please report a bug");
		converted[0] = '\0';
		return converted;
	}
#endif
	/* If the first character starts before the left edge, or would be
	 * overwritten by a "<" token, then show placeholders instead. */
	if (*buf != '\0' && *buf != '\t' && (start_col < column ||
						(start_col > 0 && isdata && !ISSET(SOFTWRAP)))) {
		if (is_cntrl_mbchar(buf)) {
			if (start_col < column) {
				converted[index++] = control_mbrep(buf, isdata);
				column++;
				buf += char_length(buf);
			}
		}
#ifdef ENABLE_UTF8
		else if (mbwidth(buf) == 2) {
			if (start_col == column) {
				converted[index++] = ' ';
				column++;
			}

			/* Display the right half of a two-column character as ']'. */
			converted[index++] = ']';
			column++;
			buf += char_length(buf);
		}
#endif
	}

#ifdef ENABLE_UTF8
#define ISO8859_CHAR  FALSE
#else
#define ISO8859_CHAR  ((unsigned char)*buf > 0x9F)
#endif

	while (*buf != '\0' && (column < beyond || mbwidth(buf) == 0)) {
		/* A plain printable ASCII character is one byte, one column. */
		if (((signed char)*buf > 0x20 && *buf != DEL_CODE) || ISO8859_CHAR) {
			converted[index++] = *(buf++);
			column++;
			continue;
		}

		/* Show a space as a visible character, or as a space. */
		if (*buf == ' ') {
#ifndef NANO_TINY
			if (ISSET(WHITESPACE_DISPLAY)) {
				for (int i = whitelen[0]; i < whitelen[0] + whitelen[1];)
					converted[index++] = whitespace[i++];
			} else
#endif
				converted[index++] = ' ';
			column++;
			buf++;
			continue;
		}

		/* Show a tab as a visible character plus spaces, or as just spaces. */
		if (*buf == '\t') {
#ifndef NANO_TINY
			if (ISSET(WHITESPACE_DISPLAY) && (index > 0 || !isdata ||
						!ISSET(SOFTWRAP) || column % tabsize == 0 ||
						column == start_col)) {
				for (int i = 0; i < whitelen[0];)
					converted[index++] = whitespace[i++];
			} else
#endif
				converted[index++] = ' ';
			column++;
			/* Fill the tab up with the required number of spaces. */
			while (column % tabsize != 0 && column < beyond) {
				converted[index++] = ' ';
				column++;
			}
			buf++;
			continue;
		}

		/* Represent a control character with a leading caret. */
		if (is_cntrl_mbchar(buf)) {
			converted[index++] = '^';
			converted[index++] = control_mbrep(buf, isdata);
			buf += char_length(buf);
			column += 2;
			continue;
		}

#ifdef ENABLE_UTF8
		int charlength, charwidth;
		wchar_t wc;

		/* Convert a multibyte character to a single code. */
		charlength = mbtowc(&wc, buf, MAXCHARLEN);

		/* Represent an invalid character with the Replacement Character. */
		if (charlength < 0 || !is_valid_unicode(wc)) {
			converted[index++] = '\xEF';
			converted[index++] = '\xBF';
			converted[index++] = '\xBD';
			buf += (charlength > 0 ? charlength : 1);
			column++;
			continue;
		}

		/* For any valid character, just copy its bytes. */
		for (; charlength > 0; charlength--)
			converted[index++] = *(buf++);

		/* Determine whether the character occupies one or two columns. */
		charwidth = wcwidth(wc);

		/* If the codepoint is unassigned, assume a width of one. */
		column += (charwidth < 0 ? 1 : charwidth);

#ifdef USING_OLD_NCURSES
		if (charwidth > 1)
			seen_wide = TRUE;
#endif
#endif /* ENABLE_UTF8 */
	}

	/* If there is more text than can be shown, make room for the ">". */
	if (column > beyond || (*buf != '\0' && (isprompt ||
					(isdata && !ISSET(SOFTWRAP))))) {
		do {
			index = step_left(converted, index);
		} while (mbwidth(converted + index) == 0);

#ifdef ENABLE_UTF8
		/* Display the left half of a two-column character as '['. */
		if (mbwidth(converted + index) == 2)
			converted[index++] = '[';
#endif
	}

	is_shorter = (column < beyond);

	/* Null-terminate the converted string. */
	converted[index] = '\0';

	return converted;
}

#ifdef ENABLE_MULTIBUFFER
/* Determine the sequence number of the given buffer in the circular list. */
int buffer_number(openfilestruct *buffer)
{
	int count = 1;

	while (buffer != startfile) {
		buffer = buffer->prev;
		count++;
	}

	return count;
}
#endif

/* If path is NULL, we're in normal editing mode, so display the current
 * version of nano, the current filename, and whether the current file
 * has been modified on the titlebar.  If path isn't NULL, we're either
 * in the file browser or the help viewer, so show either the current
 * directory or the title of help text, that is: whatever is in path. */
void titlebar(const char *path)
{
	size_t verlen, prefixlen, pathlen, statelen;
		/* The width of the different titlebar elements, in columns. */
	size_t pluglen = 0;
		/* The width that "Modified" would take up. */
	size_t offset = 0;
		/* The position at which the center part of the titlebar starts. */
	const char *upperleft = "";
		/* What is shown in the top left corner. */
	const char *prefix = "";
		/* What is shown before the path -- "DIR:" or nothing. */
	const char *state = "";
		/* The state of the current buffer -- "Modified", "View", or "". */
	char *caption;
		/* The presentable form of the pathname. */
	char *indicator = NULL;
		/* The buffer sequence number plus buffer count. */

	/* If the screen is too small, there is no titlebar. */
	if (topwin == NULL)
		return;

	wattron(topwin, interface_color_pair[TITLE_BAR]);

	blank_titlebar();
	as_an_at = FALSE;

	/* Do as Pico: if there is not enough width available for all items,
	 * first sacrifice the version string, then eat up the side spaces,
	 * then sacrifice the prefix, and only then start dottifying. */

	/* Figure out the path, prefix and state strings. */
#ifdef ENABLE_COLOR
	if (currmenu == MLINTER) {
		/* TRANSLATORS: The next five are "labels" in the title bar. */
		prefix = _("Linting --");
		path = openfile->filename;
	} else
#endif
#ifdef ENABLE_BROWSER
	if (!inhelp && path != NULL)
		prefix = _("DIR:");
	else
#endif
	if (!inhelp) {
#ifdef ENABLE_MULTIBUFFER
		/* If there are/were multiple buffers, show which out of how many. */
		if (more_than_one) {
			indicator = charalloc(24);
			sprintf(indicator, "[%i/%i]", buffer_number(openfile),
										buffer_number(startfile->prev));
			upperleft = indicator;
		} else
#endif
			upperleft = BRANDING;

		if (openfile->filename[0] == '\0')
			path = _("New Buffer");
		else
			path = openfile->filename;

		if (openfile->modified)
			state = _("Modified");
		else if (ISSET(VIEW_MODE))
			state = _("View");
		else if (ISSET(RESTRICTED))
			state = _("Restricted");

		pluglen = breadth(_("Modified")) + 1;
	}

	/* Determine the widths of the four elements, including their padding. */
	verlen = breadth(upperleft) + 3;
	prefixlen = breadth(prefix);
	if (prefixlen > 0)
		prefixlen++;
	pathlen = breadth(path);
	statelen = breadth(state) + 2;
	if (statelen > 2) {
		pathlen++;
		pluglen = 0;
	}

	/* Only print the version message when there is room for it. */
	if (verlen + prefixlen + pathlen + pluglen + statelen <= COLS)
		mvwaddstr(topwin, 0, 2, upperleft);
	else {
		verlen = 2;
		/* If things don't fit yet, give up the placeholder. */
		if (verlen + prefixlen + pathlen + pluglen + statelen > COLS)
			pluglen = 0;
		/* If things still don't fit, give up the side spaces. */
		if (verlen + prefixlen + pathlen + pluglen + statelen > COLS) {
			verlen = 0;
			statelen -= 2;
		}
	}

	free(indicator);

	/* If we have side spaces left, center the path name. */
	if (verlen > 0)
		offset = verlen + (COLS - (verlen + pluglen + statelen) -
										(prefixlen + pathlen)) / 2;

	/* Only print the prefix when there is room for it. */
	if (verlen + prefixlen + pathlen + pluglen + statelen <= COLS) {
		mvwaddstr(topwin, 0, offset, prefix);
		if (prefixlen > 0)
			waddstr(topwin, " ");
	} else
		wmove(topwin, 0, offset);

	/* Print the full path if there's room; otherwise, dottify it. */
	if (pathlen + pluglen + statelen <= COLS) {
		caption = display_string(path, 0, pathlen, FALSE, FALSE);
		waddstr(topwin, caption);
		free(caption);
	} else if (5 + statelen <= COLS) {
		waddstr(topwin, "...");
		caption = display_string(path, 3 + pathlen - COLS + statelen,
										COLS - statelen, FALSE, FALSE);
		waddstr(topwin, caption);
		free(caption);
	}

	/* Right-align the state if there's room; otherwise, trim it. */
	if (statelen > 0 && statelen <= COLS)
		mvwaddstr(topwin, 0, COLS - statelen, state);
	else if (statelen > 0)
		mvwaddnstr(topwin, 0, 0, state, actual_x(state, COLS));

	wattroff(topwin, interface_color_pair[TITLE_BAR]);

	wrefresh(topwin);
}

/* Display a normal message on the statusbar, quietly. */
void statusbar(const char *msg)
{
	statusline(HUSH, msg);
}

/* Warn the user on the statusbar and pause for a moment, so that the
 * message can be noticed and read. */
void warn_and_shortly_pause(const char *msg)
{
	statusline(ALERT, msg);
	napms(1800);
}

/* Display a message on the statusbar, and set suppress_cursorpos to
 * TRUE, so that the message won't be immediately overwritten if
 * constant cursor position display is on. */
void statusline(message_type importance, const char *msg, ...)
{
	va_list ap;
	static int alerts = 0;
	int colorpair;
	char *compound, *message;
	size_t start_col;
	bool bracketed;
#ifndef NANO_TINY
	bool old_whitespace = ISSET(WHITESPACE_DISPLAY);

	UNSET(WHITESPACE_DISPLAY);
#endif

	/* Ignore a message with an importance that is lower than the last one. */
	if ((lastmessage == ALERT && importance != ALERT) ||
				(lastmessage == MILD && importance == HUSH))
		return;

#ifndef NANO_TINY
	/* Curses mode shouldn't be off when trying to write to the status bar. */
	if (!started_curses || isendwin()) {
		fprintf(stderr, "Out of curses -- please report a bug\n");
		lastmessage = HUSH;
		napms(1400);
		return;
	}
#endif

	/* If the ALERT status has been reset, reset the counter. */
	if (lastmessage == HUSH)
		alerts = 0;

	/* Shortly pause after each of the first three alert messages,
	 * to give the user time to read them. */
	if (lastmessage == ALERT && alerts < 4 && !ISSET(NO_PAUSES))
		napms(1200);

	if (importance == ALERT) {
		if (++alerts > 3 && !ISSET(NO_PAUSES))
			msg = _("Further warnings were suppressed");
		else if (alerts < 4)
			beep();
		colorpair = interface_color_pair[ERROR_MESSAGE];
	} else if (importance == NOTICE)
		colorpair = interface_color_pair[SELECTED_TEXT];
	else
		colorpair = interface_color_pair[STATUS_BAR];

	lastmessage = importance;

	blank_statusbar();

	/* Construct the message out of all the arguments. */
	compound = charalloc(MAXCHARLEN * (COLS + 1));
	va_start(ap, msg);
	vsnprintf(compound, MAXCHARLEN * (COLS + 1), msg, ap);
	va_end(ap);
	message = display_string(compound, 0, COLS, FALSE, FALSE);
	free(compound);

	start_col = (COLS - breadth(message)) / 2;
	bracketed = (start_col > 1);

	wmove(bottomwin, 0, (bracketed ? start_col - 2 : start_col));
	wattron(bottomwin, colorpair);
	if (bracketed)
		waddstr(bottomwin, "[ ");
	waddstr(bottomwin, message);
	free(message);
	if (bracketed)
		waddstr(bottomwin, " ]");
	wattroff(bottomwin, colorpair);

	/* Defeat a VTE/Konsole bug, where the cursor can go off-limits. */
	if (ISSET(CONSTANT_SHOW) && ISSET(NO_HELP))
		wmove(bottomwin, 0, 0);

	/* Push the message to the screen straightaway. */
	wrefresh(bottomwin);

	suppress_cursorpos = TRUE;

#ifndef NANO_TINY
	if (old_whitespace)
		SET(WHITESPACE_DISPLAY);
#endif

	/* If doing quick blanking, blank the statusbar after just one keystroke.
	 * Otherwise, blank it after twenty-six keystrokes, as Pico does. */
	if (ISSET(QUICK_BLANK))
		statusblank = 1;
	else
		statusblank = 26;
}

/* Display the shortcut list corresponding to menu on the last two rows
 * of the bottom portion of the window. */
void bottombars(int menu)
{
	size_t number, itemwidth, i;
	funcstruct *f;
	const keystruct *s;

	/* Set the global variable to the given menu. */
	currmenu = menu;

	if (ISSET(NO_HELP) || LINES < 5)
		return;

	/* Determine how many shortcuts there are to show. */
	number = length_of_list(menu);

	if (number > MAIN_VISIBLE)
		number = MAIN_VISIBLE;

	/* Compute the width of each keyname-plus-explanation pair. */
	itemwidth = COLS / ((number / 2) + (number % 2));

	/* If there is no room, don't print anything. */
	if (itemwidth == 0)
		return;

	blank_bottombars();

	/* Display the first number of shortcuts in the given menu that
	 * have a key combination assigned to them. */
	for (f = allfuncs, i = 0; i < number && f != NULL; f = f->next) {
		if ((f->menus & menu) == 0)
			continue;

		s = first_sc_for(menu, f->func);
		if (s == NULL)
			continue;

		wmove(bottomwin, 1 + i % 2, (i / 2) * itemwidth);

		post_one_key(s->keystr, _(f->desc), itemwidth + (COLS % itemwidth));
		i++;
	}

	/* Defeat a VTE bug by homing the cursor and forcing a screen update. */
	wmove(bottomwin, 0, 0);
	wrefresh(bottomwin);
}

/* Write a key's representation plus a minute description of its function
 * to the screen.  For example, the key could be "^C" and its tag "Cancel".
 * Key plus tag may occupy at most width columns. */
void post_one_key(const char *keystroke, const char *tag, int width)
{
	wattron(bottomwin, interface_color_pair[KEY_COMBO]);
	waddnstr(bottomwin, keystroke, actual_x(keystroke, width));
	wattroff(bottomwin, interface_color_pair[KEY_COMBO]);

	/* If the remaning space is too small, skip the description. */
	width -= breadth(keystroke);
	if (width < 2)
		return;

	waddch(bottomwin, ' ');
	wattron(bottomwin, interface_color_pair[FUNCTION_TAG]);
	waddnstr(bottomwin, tag, actual_x(tag, width - 1));
	wattroff(bottomwin, interface_color_pair[FUNCTION_TAG]);
}

/* Redetermine current_y from the position of current relative to edittop,
 * and put the cursor in the edit window at (current_y, "current_x"). */
void place_the_cursor(void)
{
	ssize_t row = 0;
	size_t col, xpt = xplustabs();

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		linestruct *line = openfile->edittop;
		size_t leftedge;

		row -= chunk_for(openfile->firstcolumn, openfile->edittop);

		/* Calculate how many rows the lines from edittop to current use. */
		while (line != NULL && line != openfile->current) {
			row += number_of_chunks_in(line) + 1;
			line = line->next;
		}

		/* Add the number of wraps in the current line before the cursor. */
		row += get_chunk_and_edge(xpt, openfile->current, &leftedge);
		col = xpt - leftedge;
	} else
#endif
	{
		row = openfile->current->lineno - openfile->edittop->lineno;
		col = xpt - get_page_start(xpt);
	}

	if (row < editwinrows)
		wmove(edit, row, margin + col);

	openfile->current_y = row;
}

/* Draw the given text on the given row of the edit window.  line is the
 * line to be drawn, and converted is the actual string to be written with
 * tabs and control characters replaced by strings of regular characters.
 * from_col is the column number of the first character of this "page". */
void draw_row(int row, const char *converted, linestruct *line, size_t from_col)
{
#if !defined(NANO_TINY) || defined(ENABLE_COLOR)
	size_t from_x = actual_x(line->data, from_col);
		/* The position in the line's data of the leftmost character
		 * that displays at least partially on the window. */
	size_t till_x = actual_x(line->data, from_col + editwincols - 1) + 1;
		/* The position in the line's data of the first character that
		 * is completely off the window to the right.  Note that till_x
		 * might be beyond the null terminator of the string. */
#endif

#ifdef ENABLE_LINENUMBERS
	/* If line numbering is switched on, put a line number in front of
	 * the text -- but only for the parts that are not softwrapped. */
	if (margin > 0) {
		wattron(edit, interface_color_pair[LINE_NUMBER]);
#ifndef NANO_TINY
		if (ISSET(SOFTWRAP) && from_col != 0)
			mvwprintw(edit, row, 0, "%*s", margin, " ");
		else
#endif
			mvwprintw(edit, row, 0, "%*zd ", margin - 1, line->lineno);
		wattroff(edit, interface_color_pair[LINE_NUMBER]);
	}
#endif

	/* First simply write the converted line -- afterward we'll add colors
	 * and the marking highlight on just the pieces that need it. */
	mvwaddstr(edit, row, margin, converted);

	/* When needed, clear the remainder of the row. */
	if (is_shorter || ISSET(SOFTWRAP))
		wclrtoeol(edit);

#ifdef USING_OLD_NCURSES
	/* Tell ncurses to really redraw the line without trying to optimize
	 * for what it thinks is already there, because it gets it wrong in
	 * the case of a wide character in column zero.  See bug #31743. */
	if (seen_wide)
		wredrawln(edit, row, 1);
#endif

#ifdef ENABLE_COLOR
	/* If color syntaxes are available and turned on, apply them. */
	if (openfile->colorstrings != NULL && !ISSET(NO_COLOR_SYNTAX)) {
		const colortype *varnish = openfile->colorstrings;

		/* If there are multiline regexes, make sure there is a cache. */
		if (openfile->syntax->nmultis > 0 && line->multidata == NULL)
			set_up_multicache(line);

		/* Iterate through all the coloring regexes. */
		for (; varnish != NULL; varnish = varnish->next) {
			size_t index = 0;
				/* Where in the line we currently begin looking for a match. */
			int start_col;
				/* The starting column of a piece to paint.  Zero-based. */
			int paintlen = 0;
				/* The number of characters to paint. */
			const char *thetext;
				/* The place in converted from where painting starts. */
			regmatch_t match;
				/* The match positions of a single-line regex. */
			const linestruct *start_line = line->prev;
				/* The first line before line that matches 'start'. */
			const linestruct *end_line = line;
				/* The line that matches 'end'. */
			regmatch_t startmatch, endmatch;
				/* The match positions of the start and end regexes. */

			/* Two notes about regexec().  A return value of zero means
			 * that there is a match.  Also, rm_eo is the first
			 * non-matching character after the match. */

			wattron(edit, varnish->attributes);

			/* First case: varnish is a single-line expression. */
			if (varnish->end == NULL) {
				/* We increment index by rm_eo, to move past the end of the
				 * last match.  Even though two matches may overlap, we
				 * want to ignore them, so that we can highlight e.g. C
				 * strings correctly. */
				while (index < till_x) {
					/* Note the fifth parameter to regexec().  It says
					 * not to match the beginning-of-line character
					 * unless index is zero.  If regexec() returns
					 * REG_NOMATCH, there are no more matches in the
					 * line. */
					if (regexec(varnish->start, &line->data[index], 1,
								&match, (index == 0) ? 0 : REG_NOTBOL) != 0)
						break;

					/* If the match is of length zero, skip it. */
					if (match.rm_so == match.rm_eo) {
						index = step_right(line->data, index + match.rm_eo);
						continue;
					}

					/* Translate the match to the beginning of the line. */
					match.rm_so += index;
					match.rm_eo += index;
					index = match.rm_eo;

					/* If the matching part is not visible, skip it. */
					if (match.rm_eo <= from_x || match.rm_so >= till_x)
						continue;

					start_col = (match.rm_so <= from_x) ?
										0 : wideness(line->data,
										match.rm_so) - from_col;

					thetext = converted + actual_x(converted, start_col);

					paintlen = actual_x(thetext, wideness(line->data,
										match.rm_eo) - from_col - start_col);

					mvwaddnstr(edit, row, margin + start_col, thetext, paintlen);
				}
				goto tail_of_loop;
			}

			/* Second case: varnish is a multiline expression. */

			/* Assume nothing gets painted until proven otherwise below. */
			line->multidata[varnish->id] = CNONE;

			/* First check the multidata of the preceding line -- it tells
			 * us about the situation so far, and thus what to do here. */
			if (start_line != NULL && start_line->multidata != NULL) {
				if (start_line->multidata[varnish->id] == CWHOLELINE ||
						start_line->multidata[varnish->id] == CENDAFTER ||
						start_line->multidata[varnish->id] == CWOULDBE)
					goto seek_an_end;
				if (start_line->multidata[varnish->id] == CNONE ||
						start_line->multidata[varnish->id] == CBEGINBEFORE ||
						start_line->multidata[varnish->id] == CSTARTENDHERE)
					goto step_two;
			}

			/* The preceding line has no precalculated multidata.  So, do
			 * some backtracking to find out what to paint. */

			/* First step: see if there is a line before current that
			 * matches 'start' and is not complemented by an 'end'. */
			while (start_line != NULL && regexec(varnish->start,
					start_line->data, 1, &startmatch, 0) == REG_NOMATCH) {
				/* There is no start on this line; but if there is an end,
				 * there is no need to look for starts on earlier lines. */
				if (regexec(varnish->end, start_line->data, 0, NULL, 0) == 0)
					goto step_two;
				start_line = start_line->prev;
			}

			/* If no start was found, skip to the next step. */
			if (start_line == NULL)
				goto step_two;

			/* If a found start has been qualified as an end earlier,
			 * believe it and skip to the next step. */
			if (start_line->multidata != NULL &&
						(start_line->multidata[varnish->id] == CBEGINBEFORE ||
						start_line->multidata[varnish->id] == CSTARTENDHERE))
				goto step_two;

			/* Is there an uncomplemented start on the found line? */
			while (TRUE) {
				/* Begin searching for an end after the start match. */
				index += startmatch.rm_eo;
				/* If there is no end after this last start, good. */
				if (regexec(varnish->end, start_line->data + index, 1, &endmatch,
								(index == 0) ? 0 : REG_NOTBOL) == REG_NOMATCH)
					break;
				/* Begin searching for a new start after the end match. */
				index += endmatch.rm_eo;
				/* If both start and end match are mere anchors, advance. */
				if (startmatch.rm_so == startmatch.rm_eo &&
								endmatch.rm_so == endmatch.rm_eo) {
					if (start_line->data[index] == '\0')
						break;
					index = step_right(start_line->data, index);
				}
				/* If there is no later start on this line, next step. */
				if (regexec(varnish->start, start_line->data + index,
								1, &startmatch, REG_NOTBOL) == REG_NOMATCH)
					goto step_two;
			}
			/* Indeed, there is a start without an end on that line. */

  seek_an_end:
			/* We've already checked that there is no end between the start
			 * and the current line.  But is there an end after the start
			 * at all?  We don't paint unterminated starts. */
			while (end_line != NULL && regexec(varnish->end, end_line->data,
								1, &endmatch, 0) == REG_NOMATCH)
				end_line = end_line->next;

			/* If there is no end, there is nothing to paint. */
			if (end_line == NULL) {
				line->multidata[varnish->id] = CWOULDBE;
				goto tail_of_loop;
			}

			/* If the end is on a later line, paint whole line, and be done. */
			if (end_line != line) {
				mvwaddnstr(edit, row, margin, converted, -1);
				line->multidata[varnish->id] = CWHOLELINE;
				goto tail_of_loop;
			}

			/* Only if it is visible, paint the part to be coloured. */
			if (endmatch.rm_eo > from_x) {
				paintlen = actual_x(converted, wideness(line->data,
												endmatch.rm_eo) - from_col);
				mvwaddnstr(edit, row, margin, converted, paintlen);
			}
			line->multidata[varnish->id] = CBEGINBEFORE;

  step_two:
			/* Second step: look for starts on this line, but begin
			 * looking only after an end match, if there is one. */
			index = (paintlen == 0) ? 0 : endmatch.rm_eo;

			while (regexec(varnish->start, line->data + index,
								1, &startmatch, (index == 0) ?
								0 : REG_NOTBOL) == 0) {
				/* Translate the match to be relative to the
				 * beginning of the line. */
				startmatch.rm_so += index;
				startmatch.rm_eo += index;

				start_col = (startmatch.rm_so <= from_x) ?
								0 : wideness(line->data,
								startmatch.rm_so) - from_col;

				thetext = converted + actual_x(converted, start_col);

				if (regexec(varnish->end, line->data + startmatch.rm_eo,
								1, &endmatch, (startmatch.rm_eo == 0) ?
								0 : REG_NOTBOL) == 0) {
					/* Translate the end match to be relative to
					 * the beginning of the line. */
					endmatch.rm_so += startmatch.rm_eo;
					endmatch.rm_eo += startmatch.rm_eo;
					/* Only paint the match if it is visible on screen and
					 * it is more than zero characters long. */
					if (endmatch.rm_eo > from_x &&
										endmatch.rm_eo > startmatch.rm_so) {
						paintlen = actual_x(thetext, wideness(line->data,
										endmatch.rm_eo) - from_col - start_col);

						mvwaddnstr(edit, row, margin + start_col,
												thetext, paintlen);

						line->multidata[varnish->id] = CSTARTENDHERE;
					}
					index = endmatch.rm_eo;
					/* If both start and end match are anchors, advance. */
					if (startmatch.rm_so == startmatch.rm_eo &&
								endmatch.rm_so == endmatch.rm_eo) {
						if (line->data[index] == '\0')
							break;
						index = step_right(line->data, index);
					}
					continue;
				}

				/* There is no end on this line.  But maybe on later lines? */
				end_line = line->next;

				while (end_line != NULL && regexec(varnish->end, end_line->data,
										0, NULL, 0) == REG_NOMATCH)
					end_line = end_line->next;

				/* If there is no end, we're done with this regex. */
				if (end_line == NULL) {
					line->multidata[varnish->id] = CWOULDBE;
					break;
				}

				/* Paint the rest of the line, and we're done. */
				mvwaddnstr(edit, row, margin + start_col, thetext, -1);
				line->multidata[varnish->id] = CENDAFTER;
				break;
			}
  tail_of_loop:
			wattroff(edit, varnish->attributes);
		}
	}
#endif /* ENABLE_COLOR */

#ifndef NANO_TINY
	if (stripe_column > from_col && !inhelp &&
						stripe_column <= from_col + editwincols) {
		ssize_t target_column = stripe_column - from_col - 1;
		size_t target_x = actual_x(converted, target_column);
		char striped_char[MAXCHARLEN];
		size_t charlen = 1;

		if (*(converted + target_x) != '\0') {
			charlen = parse_mbchar(converted + target_x, striped_char, NULL);
			target_column = wideness(converted, target_x);
		} else if (target_column + 1 == editwincols) {
			/* Defeat a VTE bug -- see https://sv.gnu.org/bugs/?55896. */
#ifdef ENABLE_UTF8
			if (using_utf8()) {
				striped_char[0] = '\xC2';
				striped_char[1] = '\xA0';
				charlen = 2;
			} else
#endif
				striped_char[0] = '.';
		} else
			striped_char[0] = ' ';

		wattron(edit, interface_color_pair[GUIDE_STRIPE]);
		mvwaddnstr(edit, row, margin + target_column, striped_char, charlen);
		wattroff(edit, interface_color_pair[GUIDE_STRIPE]);
	}

	/* If the line is at least partially selected, paint the marked part. */
	if (openfile->mark && ((line->lineno >= openfile->mark->lineno &&
						line->lineno <= openfile->current->lineno) ||
						(line->lineno <= openfile->mark->lineno &&
						line->lineno >= openfile->current->lineno))) {
		const linestruct *top, *bot;
			/* The lines where the marked region begins and ends. */
		size_t top_x, bot_x;
			/* The x positions where the marked region begins and ends. */
		int start_col;
			/* The column where painting starts.  Zero-based. */
		const char *thetext;
			/* The place in converted from where painting starts. */
		int paintlen = -1;
			/* The number of characters to paint.  Negative means "all". */

		get_region(&top, &top_x, &bot, &bot_x, NULL);

		if (top->lineno < line->lineno || top_x < from_x)
			top_x = from_x;
		if (bot->lineno > line->lineno || bot_x > till_x)
			bot_x = till_x;

		/* Only paint if the marked part of the line is on this page. */
		if (top_x < till_x && bot_x > from_x) {
			/* Compute on which screen column to start painting. */
			start_col = wideness(line->data, top_x) - from_col;

			if (start_col < 0)
				start_col = 0;

			thetext = converted + actual_x(converted, start_col);

			/* If the end of the mark is onscreen, compute how many
			 * characters to paint.  Otherwise, just paint all. */
			if (bot_x < till_x) {
				size_t end_col = wideness(line->data, bot_x) - from_col;
				paintlen = actual_x(thetext, end_col - start_col);
			}

			wattron(edit, interface_color_pair[SELECTED_TEXT]);
			mvwaddnstr(edit, row, margin + start_col, thetext, paintlen);
			wattroff(edit, interface_color_pair[SELECTED_TEXT]);
		}
	}
#endif /* !NANO_TINY */
}

/* Redraw the given line so that the character at the given index is visible
 * -- if necessary, scroll the line horizontally (when not softwrapping).
 * Return the number of rows "consumed" (relevant when softwrapping). */
int update_line(linestruct *line, size_t index)
{
	int row;
		/* The row in the edit window we will be updating. */
	char *converted;
		/* The data of the line with tabs and control characters expanded. */
	size_t from_col;
		/* From which column a horizontally scrolled line is displayed. */

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP))
		return update_softwrapped_line(line);
#endif

	row = line->lineno - openfile->edittop->lineno;
	from_col = get_page_start(wideness(line->data, index));

	/* Expand the piece to be drawn to its representable form, and draw it. */
	converted = display_string(line->data, from_col, editwincols, TRUE, FALSE);
	draw_row(row, converted, line, from_col);
	free(converted);

	if (from_col > 0) {
		wattron(edit, hilite_attribute);
		mvwaddch(edit, row, margin, '<');
		wattroff(edit, hilite_attribute);
	}
	if (breadth(line->data) > from_col + editwincols) {
		wattron(edit, hilite_attribute);
		mvwaddch(edit, row, COLS - 1, '>');
		wattroff(edit, hilite_attribute);
	}

	if (spotlighted && !inhelp)
		spotlight(light_from_col, light_to_col);

	return 1;
}

#ifndef NANO_TINY
/* Redraw all the chunks of the given line (as far as they fit onscreen),
 * unless it's edittop, which will be displayed from column firstcolumn.
 * Return the number of rows that were "consumed". */
int update_softwrapped_line(linestruct *line)
{
	int row = 0;
		/* The row in the edit window we will write to. */
	linestruct *someline = openfile->edittop;
		/* An iterator needed to find the relevant row. */
	int starting_row;
		/* The first row in the edit window that gets updated. */
	size_t from_col = 0;
		/* The starting column of the current chunk. */
	size_t to_col = 0;
		/* The end column of the current chunk. */
	char *converted;
		/* The data of the chunk with tabs and control characters expanded. */

	if (line == openfile->edittop)
		from_col = openfile->firstcolumn;
	else
		row -= chunk_for(openfile->firstcolumn, openfile->edittop);

	/* Find out on which screen row the target line should be shown. */
	while (someline != line && someline != NULL) {
		row += number_of_chunks_in(someline) + 1;
		someline = someline->next;
	}

	/* If the first chunk is offscreen, don't even try to display it. */
	if (row < 0 || row >= editwinrows) {
		statusline(ALERT, "Badness: tried to display a chunk on row %i"
								" -- please report a bug", row);
		return 0;
	}

	starting_row = row;

	while (row < editwinrows) {
		bool end_of_line = FALSE;

		to_col = get_softwrap_breakpoint(line->data, from_col, &end_of_line);

		/* Convert the chunk to its displayable form and draw it. */
		converted = display_string(line->data, from_col, to_col - from_col,
									TRUE, FALSE);
		draw_row(row++, converted, line, from_col);
		free(converted);

		if (end_of_line)
			break;

		/* If the line is softwrapped early (because of a two-column character),
		 * show a "[" placeholder, unless we're softwrapping at blanks. */
		if (!ISSET(AT_BLANKS) && to_col - from_col < editwincols)
			mvwaddch(edit, row - 1, to_col - from_col, '[');

		from_col = to_col;
	}

	if (spotlighted && !inhelp)
		spotlight_softwrapped(light_from_col, light_to_col);

	return (row - starting_row);
}
#endif

/* Check whether the mark is on, or whether old_column and new_column are on
 * different "pages" (in softwrap mode, only the former applies), which means
 * that the relevant line needs to be redrawn. */
bool line_needs_update(const size_t old_column, const size_t new_column)
{
#ifndef NANO_TINY
	if (openfile->mark)
		return TRUE;
	else
#endif
		return (get_page_start(old_column) != get_page_start(new_column));
}

/* Try to move up nrows softwrapped chunks from the given line and the
 * given column (leftedge).  After moving, leftedge will be set to the
 * starting column of the current chunk.  Return the number of chunks we
 * couldn't move up, which will be zero if we completely succeeded. */
int go_back_chunks(int nrows, linestruct **line, size_t *leftedge)
{
	int i;

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		/* Recede through the requested number of chunks. */
		for (i = nrows; i > 0; i--) {
			size_t chunk = chunk_for(*leftedge, *line);

			*leftedge = 0;

			if (chunk >= i)
				return go_forward_chunks(chunk - i, line, leftedge);

			if (*line == openfile->filetop)
				break;

			i -= chunk;
			*line = (*line)->prev;
			*leftedge = HIGHEST_POSITIVE;
		}

		if (*leftedge == HIGHEST_POSITIVE)
			*leftedge = leftedge_for(*leftedge, *line);
	} else
#endif
		for (i = nrows; i > 0 && (*line)->prev != NULL; i--)
			*line = (*line)->prev;

	return i;
}

/* Try to move down nrows softwrapped chunks from the given line and the
 * given column (leftedge).  After moving, leftedge will be set to the
 * starting column of the current chunk.  Return the number of chunks we
 * couldn't move down, which will be zero if we completely succeeded. */
int go_forward_chunks(int nrows, linestruct **line, size_t *leftedge)
{
	int i;

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		size_t current_leftedge = *leftedge;

		/* Advance through the requested number of chunks. */
		for (i = nrows; i > 0; i--) {
			bool end_of_line = FALSE;

			current_leftedge = get_softwrap_breakpoint((*line)->data,
										current_leftedge, &end_of_line);

			if (!end_of_line)
				continue;

			if (*line == openfile->filebot)
				break;

			*line = (*line)->next;
			current_leftedge = 0;
		}

		/* Only change leftedge when we actually could move. */
		if (i < nrows)
			*leftedge = current_leftedge;
	} else
#endif
		for (i = nrows; i > 0 && (*line)->next != NULL; i--)
			*line = (*line)->next;

	return i;
}

/* Return TRUE if there are fewer than a screen's worth of lines between
 * the line at line number was_lineno (and column was_leftedge, if we're
 * in softwrap mode) and the line at current[current_x]. */
bool less_than_a_screenful(size_t was_lineno, size_t was_leftedge)
{
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		linestruct *line = openfile->current;
		size_t leftedge = leftedge_for(xplustabs(), openfile->current);
		int rows_left = go_back_chunks(editwinrows - 1, &line, &leftedge);

		return (rows_left > 0 || line->lineno < was_lineno ||
				(line->lineno == was_lineno && leftedge <= was_leftedge));
	} else
#endif
		return (openfile->current->lineno - was_lineno < editwinrows);
}

/* Scroll the edit window one row in the given direction, and
 * draw the relevant content on the resultant blank row. */
void edit_scroll(bool direction)
{
	linestruct *line;
	size_t leftedge;
	int nrows = 1;

	/* Move the top line of the edit window one row up or down. */
	if (direction == BACKWARD)
		go_back_chunks(1, &openfile->edittop, &openfile->firstcolumn);
	else
		go_forward_chunks(1, &openfile->edittop, &openfile->firstcolumn);

	/* Actually scroll the text of the edit window one row up or down. */
	scrollok(edit, TRUE);
	wscrl(edit, (direction == BACKWARD) ? -1 : 1);
	scrollok(edit, FALSE);

	/* If we're not on the first "page" (when not softwrapping), or the mark
	 * is on, the row next to the scrolled region needs to be redrawn too. */
	if (line_needs_update(openfile->placewewant, 0) && nrows < editwinrows)
		nrows++;

	/* If we scrolled backward, the top row needs to be redrawn. */
	line = openfile->edittop;
	leftedge = openfile->firstcolumn;

	/* If we scrolled forward, the bottom row needs to be redrawn. */
	if (direction == FORWARD)
		go_forward_chunks(editwinrows - nrows, &line, &leftedge);

#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		/* Compensate for the earlier chunks of a softwrapped line. */
		nrows += chunk_for(leftedge, line);

		/* Don't compensate for the chunks that are offscreen. */
		if (line == openfile->edittop)
			nrows -= chunk_for(openfile->firstcolumn, line);
	}
#endif

	/* Draw new content on the blank row (and on the bordering row too
	 * when it was deemed necessary). */
	while (nrows > 0 && line != NULL) {
		nrows -= update_line(line, (line == openfile->current) ?
										openfile->current_x : 0);
		line = line->next;
	}
}

#ifndef NANO_TINY
/* Get the column number after leftedge where we can break the given text, and
 * return it.  This will always be editwincols or less after leftedge.  Set
 * end_of_line to TRUE if we reach the end of the line while searching the
 * text.  Assume leftedge is the leftmost column of a softwrapped chunk. */
size_t get_softwrap_breakpoint(const char *text, size_t leftedge,
								bool *end_of_line)
{
	size_t goal_column = leftedge + editwincols;
		/* The place at or before which text must be broken. */
	size_t breaking_col = goal_column;
		/* The column where text can be broken, when there's no better. */
	size_t column = 0;
		/* Current column position in text. */
	size_t last_blank_col = 0;
		/* The column position of the last seen whitespace character. */
	const char *farthest_blank = NULL;
		/* A pointer to the last seen whitespace character in text. */

	/* First find the place in text where the current chunk starts. */
	while (*text != '\0' && column < leftedge)
		text += parse_mbchar(text, NULL, &column);

	/* Now find the place in text where this chunk should end. */
	while (*text != '\0' && column <= goal_column) {
		/* When breaking at blanks, do it *before* the target column. */
		if (ISSET(AT_BLANKS) && is_blank_mbchar(text) && column < goal_column) {
			farthest_blank = text;
			last_blank_col = column;
		}

		breaking_col = (*text == '\t' ? goal_column : column);
		text += parse_mbchar(text, NULL, &column);
	}

	/* If we didn't overshoot the limit, we've found a breaking point;
	 * and we've reached EOL if we didn't even *reach* the limit. */
	if (column <= goal_column) {
		*end_of_line = (column < goal_column);
		return column;
	}

	/* If we're softwrapping at blanks and we found at least one blank, break
	 * after that blank -- if it doesn't overshoot the screen's edge. */
	if (farthest_blank != NULL) {
		parse_mbchar(farthest_blank, NULL, &last_blank_col);

		if (last_blank_col <= goal_column)
			return last_blank_col;

		/* If it's a tab that overshoots, break at the screen's edge. */
		if (*farthest_blank == '\t')
			breaking_col = goal_column;
	}

	/* Otherwise, break at the last character that doesn't overshoot. */
	return (editwincols > 1) ? breaking_col : column - 1;
}

/* Get the row of the softwrapped chunk of the given line that column is on,
 * relative to the first row (zero-based), and return it.  If leftedge isn't
 * NULL, return the leftmost column of the chunk in it. */
size_t get_chunk_and_edge(size_t column, linestruct *line, size_t *leftedge)
{
	size_t current_chunk = 0, start_col = 0, end_col;
	bool end_of_line = FALSE;

	while (TRUE) {
		end_col = get_softwrap_breakpoint(line->data, start_col, &end_of_line);

		/* We reached the end of the line and/or found column, so get out. */
		if (end_of_line || (column >= start_col && column < end_col)) {
			if (leftedge != NULL)
				*leftedge = start_col;
			return current_chunk;
		}

		current_chunk++;
		start_col = end_col;
	}
}

/* Return the row of the softwrapped chunk of the given line that column is on,
 * relative to the first row (zero-based). */
size_t chunk_for(size_t column, linestruct *line)
{
	if (ISSET(SOFTWRAP))
		return get_chunk_and_edge(column, line, NULL);
	else
		return 0;
}

/* Return the leftmost column of the softwrapped chunk of the given line that
 * column is on. */
size_t leftedge_for(size_t column, linestruct *line)
{
	size_t leftedge;

	if (!ISSET(SOFTWRAP))
		return 0;

	get_chunk_and_edge(column, line, &leftedge);

	return leftedge;
}

/* Return the row of the last softwrapped chunk of the given line, relative to
 * the first row (zero-based). */
size_t number_of_chunks_in(linestruct *line)
{
	return chunk_for((size_t)-1, line);
}

/* Ensure that firstcolumn is at the starting column of the softwrapped chunk
 * it's on.  We need to do this when the number of columns of the edit window
 * has changed, because then the width of softwrapped chunks has changed. */
void ensure_firstcolumn_is_aligned(void)
{
	openfile->firstcolumn = leftedge_for(openfile->firstcolumn,
												openfile->edittop);

	/* If smooth scrolling is on, make sure the viewport doesn't center. */
	focusing = FALSE;
}
#endif /* !NANO_TINY */

/* When in softwrap mode, and the given column is on or after the breakpoint of
 * a softwrapped chunk, shift it back to the last column before the breakpoint.
 * The given column is relative to the given leftedge in current.  The returned
 * column is relative to the start of the text. */
size_t actual_last_column(size_t leftedge, size_t column)
{
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		bool last_chunk = FALSE;
		size_t end_col = get_softwrap_breakpoint(openfile->current->data,
										leftedge, &last_chunk) - leftedge;

		/* If we're not on the last chunk, we're one column past the end of
		 * the row.  Shifting back one column might put us in the middle of
		 * a multi-column character, but actual_x() will fix that later. */
		if (!last_chunk)
			end_col--;

		if (column > end_col)
			column = end_col;
	}
#endif

	return leftedge + column;
}

/* Return TRUE if current[current_x] is above the top of the screen, and FALSE
 * otherwise. */
bool current_is_above_screen(void)
{
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP))
		/* The cursor is above screen when current[current_x] is before edittop
		 * at column firstcolumn. */
		return (openfile->current->lineno < openfile->edittop->lineno ||
				(openfile->current->lineno == openfile->edittop->lineno &&
				xplustabs() < openfile->firstcolumn));
	else
#endif
		return (openfile->current->lineno < openfile->edittop->lineno);
}

/* Return TRUE if current[current_x] is below the bottom of the screen, and
 * FALSE otherwise. */
bool current_is_below_screen(void)
{
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP)) {
		linestruct *line = openfile->edittop;
		size_t leftedge = openfile->firstcolumn;

		/* If current[current_x] is more than a screen's worth of lines after
		 * edittop at column firstcolumn, it's below the screen. */
		return (go_forward_chunks(editwinrows - 1, &line, &leftedge) == 0 &&
						(line->lineno < openfile->current->lineno ||
						(line->lineno == openfile->current->lineno &&
						leftedge < leftedge_for(xplustabs(),
												openfile->current))));
	} else
#endif
		return (openfile->current->lineno >=
						openfile->edittop->lineno + editwinrows);
}

/* Return TRUE if current[current_x] is offscreen relative to edittop, and
 * FALSE otherwise. */
bool current_is_offscreen(void)
{
	return (current_is_above_screen() || current_is_below_screen());
}

/* Update any lines between old_current and current that need to be
 * updated.  Use this if we've moved without changing any text. */
void edit_redraw(linestruct *old_current, update_type manner)
{
	size_t was_pww = openfile->placewewant;

	openfile->placewewant = xplustabs();

	/* If the current line is offscreen, scroll until it's onscreen. */
	if (current_is_offscreen()) {
		adjust_viewport(ISSET(JUMPY_SCROLLING) ? CENTERING : manner);
		refresh_needed = TRUE;
		return;
	}

#ifndef NANO_TINY
	/* If the mark is on, update all lines between old_current and current. */
	if (openfile->mark) {
		linestruct *line = old_current;

		while (line != openfile->current) {
			update_line(line, 0);

			line = (line->lineno > openfile->current->lineno) ?
						line->prev : line->next;
		}
	} else
#endif
		/* Otherwise, update old_current only if it differs from current
		 * and was horizontally scrolled. */
		if (old_current != openfile->current && get_page_start(was_pww) > 0)
			update_line(old_current, 0);

	/* Update current if the mark is on or it has changed "page", or if it
	 * differs from old_current and needs to be horizontally scrolled. */
	if (line_needs_update(was_pww, openfile->placewewant) ||
						(old_current != openfile->current &&
						get_page_start(openfile->placewewant) > 0))
		update_line(openfile->current, openfile->current_x);
}

/* Refresh the screen without changing the position of lines.  Use this
 * if we've moved and changed text. */
void edit_refresh(void)
{
	linestruct *line;
	int row = 0;

#ifdef ENABLE_COLOR
	/* When needed, initialize the colors for the current syntax. */
	if (!have_palette)
		color_init();
#endif

	/* If the current line is out of view, get it back on screen. */
	if (current_is_offscreen())
		adjust_viewport((focusing || ISSET(JUMPY_SCROLLING)) ? CENTERING : FLOWING);

	line = openfile->edittop;

	while (row < editwinrows && line != NULL) {
		if (line == openfile->current)
			row += update_line(line, openfile->current_x);
		else
			row += update_line(line, 0);
		line = line->next;
	}

	while (row < editwinrows)
		blank_row(edit, row++);

	place_the_cursor();
	wnoutrefresh(edit);

	refresh_needed = FALSE;
}

/* Move edittop so that current is on the screen.  manner says how:
 * STATIONARY means that the cursor should stay on the same screen row,
 * CENTERING means that current should end up in the middle of the screen,
 * and FLOWING means that it should scroll no more than needed to bring
 * current into view. */
void adjust_viewport(update_type manner)
{
	int goal = 0;

	if (manner == STATIONARY)
		goal = openfile->current_y;
	else if (manner == CENTERING)
		goal = editwinrows / 2;
	else if (!current_is_above_screen())
		goal = editwinrows - 1;

	openfile->edittop = openfile->current;
#ifndef NANO_TINY
	if (ISSET(SOFTWRAP))
		openfile->firstcolumn = leftedge_for(xplustabs(), openfile->current);
#endif

	/* Move edittop back goal rows, starting at current[current_x]. */
	go_back_chunks(goal, &openfile->edittop, &openfile->firstcolumn);
}

/* Unconditionally redraw the entire screen. */
void total_redraw(void)
{
#ifdef USE_SLANG
	/* Slang curses emulation brain damage, part 4: Slang doesn't define
	 * curscr. */
	SLsmg_touch_screen();
	SLsmg_refresh();
#else
	wrefresh(curscr);
#endif
}

/* Redraw the entire screen, then refresh the title bar and the content of
 * the edit window (when not in the file browser), and the bottom bars. */
void total_refresh(void)
{
	total_redraw();
	if (currmenu != MBROWSER && currmenu != MWHEREISFILE && currmenu != MGOTODIR)
		titlebar(title);
#ifdef ENABLE_HELP
	if (inhelp) {
		close_buffer();
		wrap_help_text_into_buffer();
	} else
#endif
	if (currmenu != MBROWSER && currmenu != MWHEREISFILE && currmenu != MGOTODIR)
		edit_refresh();
	wipe_statusbar();
	bottombars(currmenu);
}

/* Show info about the current cursor position on the statusbar.
 * Do this unconditionally when force is TRUE; otherwise, only if
 * suppress_cursorpos is FALSE.  In any case, reset the latter. */
void do_cursorpos(bool force)
{
	char saved_byte;
	size_t sum, cur_xpt = xplustabs() + 1;
	size_t cur_lenpt = breadth(openfile->current->data) + 1;
	int linepct, colpct, charpct;

	/* If the showing needs to be suppressed, don't suppress it next time. */
	if (suppress_cursorpos && !force) {
		suppress_cursorpos = FALSE;
		return;
	}

	/* Determine the size of the file up to the cursor. */
	saved_byte = openfile->current->data[openfile->current_x];
	openfile->current->data[openfile->current_x] = '\0';

	sum = get_totsize(openfile->filetop, openfile->current);

	openfile->current->data[openfile->current_x] = saved_byte;

	/* When not at EOF, subtract 1 for an overcounted newline. */
	if (openfile->current != openfile->filebot)
		sum--;

	/* Display the current cursor position on the statusbar. */
	linepct = 100 * openfile->current->lineno / openfile->filebot->lineno;
	colpct = 100 * cur_xpt / cur_lenpt;
	charpct = (openfile->totsize == 0) ? 0 : 100 * sum / openfile->totsize;

	statusline(HUSH,
		_("line %zd/%zd (%d%%), col %zu/%zu (%d%%), char %zu/%zu (%d%%)"),
		openfile->current->lineno, openfile->filebot->lineno, linepct,
		cur_xpt, cur_lenpt, colpct, sum, openfile->totsize, charpct);

	/* Displaying the cursor position should not suppress it next time. */
	suppress_cursorpos = FALSE;
}

/* Unconditionally display the current cursor position. */
void do_cursorpos_void(void)
{
	do_cursorpos(TRUE);
}

void disable_waiting(void)
{
	waiting_mode = FALSE;
	nodelay(edit, TRUE);
}

void enable_waiting(void)
{
	waiting_mode = TRUE;
	nodelay(edit, FALSE);
}

/* Highlight the text between from_col and to_col. */
void spotlight(size_t from_col, size_t to_col)
{
	size_t right_edge = get_page_start(from_col) + editwincols;
	bool overshoots = FALSE;
	char *word;

	place_the_cursor();

	/* Limit the end column to the edge of the screen. */
	if (to_col > right_edge) {
		to_col = right_edge;
		overshoots = TRUE;
	}

	/* This is so we can show zero-length matches. */
	if (to_col == from_col) {
		word = mallocstrcpy(NULL, " ");
		to_col++;
	} else
		word = display_string(openfile->current->data, from_col,
								to_col - from_col, FALSE, overshoots);

	wattron(edit, interface_color_pair[SELECTED_TEXT]);
	waddnstr(edit, word, actual_x(word, to_col));
	if (overshoots)
		mvwaddch(edit, openfile->current_y, COLS - 1, '>');
	wattroff(edit, interface_color_pair[SELECTED_TEXT]);

	free(word);

	wnoutrefresh(edit);
}

#ifndef NANO_TINY
/* Highlight the text between the given columns.  This will not highlight soft
 * line breaks, since they're not actually part of the spotlighted text. */
void spotlight_softwrapped(size_t from_col, size_t to_col)
{
	ssize_t row = openfile->current_y;
	size_t leftedge = leftedge_for(from_col, openfile->current);
	size_t break_col;
	bool end_of_line = FALSE;
	char *word;

	place_the_cursor();

	while (row < editwinrows) {
		break_col = get_softwrap_breakpoint(openfile->current->data,
												leftedge, &end_of_line);

		/* Stop after the end of the word, by pretending the end of the word is
		 * the end of the line. */
		if (break_col >= to_col) {
			end_of_line = TRUE;
			break_col = to_col;
		}

		/* This is so we can show zero-length matches. */
		if (break_col == from_col) {
			word = mallocstrcpy(NULL, " ");
			break_col++;
		} else
			word = display_string(openfile->current->data, from_col,
										break_col - from_col, FALSE, FALSE);

		wattron(edit, interface_color_pair[SELECTED_TEXT]);
		waddnstr(edit, word, actual_x(word, break_col));
		wattroff(edit, interface_color_pair[SELECTED_TEXT]);

		free(word);

		if (end_of_line)
			break;

		wmove(edit, ++row, margin);

		leftedge = break_col;
		from_col = break_col;
	}

	wnoutrefresh(edit);
}
#endif

#ifdef ENABLE_EXTRA
#define CREDIT_LEN 54
#define XLCREDIT_LEN 9

/* Easter egg: Display credits.  Assume nodelay(edit) and scrollok(edit)
 * are FALSE. */
void do_credits(void)
{
	bool with_empty_line = ISSET(EMPTY_LINE);
	bool with_help = !ISSET(NO_HELP);
	int kbinput = ERR, crpos = 0, xlpos = 0;
	const char *credits[CREDIT_LEN] = {
		NULL,                /* "The nano text editor" */
		NULL,                /* "version" */
		VERSION,
		"",
		NULL,                /* "Brought to you by:" */
		"Chris Allegretta",
		"Benno Schulenberg",
		"David Lawrence Ramsey",
		"Jordi Mallach",
		"David Benbennick",
		"Rocco Corsi",
		"Mike Frysinger",
		"Adam Rogoyski",
		"Rob Siemborski",
		"Mark Majeres",
		"Ken Tyler",
		"Sven Guckes",
		"Bill Soudan",
		"Christian Weisgerber",
		"Erik Andersen",
		"Big Gaute",
		"Joshua Jensen",
		"Ryan Krebs",
		"Albert Chin",
		"",
		NULL,                /* "Special thanks to:" */
		"Monique, Brielle & Joseph",
		"Plattsburgh State University",
		"Benet Laboratories",
		"Amy Allegretta",
		"Linda Young",
		"Jeremy Robichaud",
		"Richard Kolb II",
		NULL,                /* "The Free Software Foundation" */
		"Linus Torvalds",
		NULL,                /* "the many translators and the TP" */
		NULL,                /* "For ncurses:" */
		"Thomas Dickey",
		"Pavel Curtis",
		"Zeyd Ben-Halim",
		"Eric S. Raymond",
		NULL,                /* "and anyone else we forgot..." */
		NULL,                /* "Thank you for using nano!" */
		"",
		"",
		"",
		"",
		"(C) 2019",
		"Free Software Foundation, Inc.",
		"",
		"",
		"",
		"",
		"https://nano-editor.org/"
	};

	const char *xlcredits[XLCREDIT_LEN] = {
		N_("The nano text editor"),
		N_("version"),
		N_("Brought to you by:"),
		N_("Special thanks to:"),
		N_("The Free Software Foundation"),
		N_("the many translators and the TP"),
		N_("For ncurses:"),
		N_("and anyone else we forgot..."),
		N_("Thank you for using nano!")
	};

	if (with_empty_line || with_help) {
		UNSET(EMPTY_LINE);
		SET(NO_HELP);
		window_init();
	}

	nodelay(edit, TRUE);

	blank_titlebar();
	blank_edit();
	blank_statusbar();

	wrefresh(topwin);
	wrefresh(edit);
	wrefresh(bottomwin);
	napms(700);

	for (crpos = 0; crpos < CREDIT_LEN + editwinrows / 2; crpos++) {
		if ((kbinput = wgetch(edit)) != ERR)
			break;

		if (crpos < CREDIT_LEN) {
			const char *what;
			size_t start_col;

			if (credits[crpos] == NULL)
				what = _(xlcredits[xlpos++]);
			else
				what = credits[crpos];

			start_col = COLS / 2 - breadth(what) / 2 - 1;
			mvwaddstr(edit, editwinrows - 1 - (editwinrows % 2),
												start_col, what);
		}

		wrefresh(edit);

		if ((kbinput = wgetch(edit)) != ERR)
			break;
		napms(700);

		scrollok(edit, TRUE);
		wscrl(edit, 1);
		scrollok(edit, FALSE);
		wrefresh(edit);

		if ((kbinput = wgetch(edit)) != ERR)
			break;
		napms(700);

		scrollok(edit, TRUE);
		wscrl(edit, 1);
		scrollok(edit, FALSE);
		wrefresh(edit);
	}

	if (kbinput != ERR)
		ungetch(kbinput);

	if (with_empty_line)
		SET(EMPTY_LINE);
	if (with_help)
		UNSET(NO_HELP);
	window_init();

	nodelay(edit, FALSE);

	total_refresh();
}
#endif /* ENABLE_EXTRA */
