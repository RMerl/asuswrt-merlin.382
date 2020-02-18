/**************************************************************************
 *   global.c  --  This file is part of GNU nano.                         *
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

#include <ctype.h>
#include <string.h>
#include <strings.h>

/* Global variables. */
#ifndef NANO_TINY
volatile sig_atomic_t the_window_resized = FALSE;
		/* Set to TRUE by the handler whenever a SIGWINCH occurs. */
#endif

bool on_a_vt = FALSE;
		/* Whether we're running on a Linux console (a VT). */

bool meta_key;
		/* Whether the current keystroke is a Meta key. */
bool shift_held;
		/* Whether Shift was being held together with a movement key. */
bool focusing = TRUE;
		/* Whether an update of the edit window should center the cursor. */

bool as_an_at = TRUE;
		/* Whether a 0x0A byte should be shown as a ^@ instead of a ^J. */

bool control_C_was_pressed = FALSE;
		/* Whether Ctrl+C was pressed (when a keyboard interrupt is enabled). */

bool started_curses = FALSE;

bool suppress_cursorpos = FALSE;
		/* Should we skip constant position display for current keystroke? */

message_type lastmessage = HUSH;
		/* Messages of type HUSH should not overwrite type MILD nor ALERT. */

linestruct *pletion_line = NULL;
		/* The line where the last completion was found, if any. */

bool inhelp = FALSE;
		/* Whether we are in the help viewer. */
char *title = NULL;
		/* When not NULL: the title of the current help text. */

bool more_than_one = FALSE;
		/* Whether more than one buffer is or has been open. */
bool also_the_last = FALSE;
		/* Whether indenting/commenting should include the last line of
		 * the marked region. */
bool is_shorter;
		/* Whether a row's text is narrower than the screen's width. */

int didfind = 0;
		/* Whether the last search found something. */

int controlleft, controlright, controlup, controldown;
int controlhome, controlend;
#ifndef NANO_TINY
int controldelete, controlshiftdelete;
int shiftleft, shiftright, shiftup, shiftdown;
int shiftcontrolleft, shiftcontrolright, shiftcontrolup, shiftcontroldown;
int shiftcontrolhome, shiftcontrolend;
int altleft, altright, altup, altdown;
int altdelete;
int shiftaltleft, shiftaltright, shiftaltup, shiftaltdown;
#endif

#ifdef ENABLED_WRAPORJUSTIFY
ssize_t fill = -COLUMNS_FROM_EOL;
		/* The relative column where we will wrap lines. */
ssize_t wrap_at = 0;
		/* The actual column where we will wrap lines, based on fill. */
#endif

char *last_search = NULL;
		/* The last string we searched for. */

char *present_path = NULL;
		/* The current browser directory when trying to do tab completion. */

unsigned flags[4] = {0, 0, 0, 0};
		/* Our flag containing the states of all global options. */

WINDOW *topwin = NULL;
		/* The top portion of the screen, showing the version number of nano,
		 * the name of the file, and whether the buffer was modified. */
WINDOW *edit = NULL;
		/* The middle portion of the screen: the edit window, showing the
		 * contents of the current buffer, the file we are editing. */
WINDOW *bottomwin = NULL;
		/* The bottom portion of the screen, where we display statusbar
		 * messages, the statusbar prompt, and a list of shortcuts. */
int editwinrows = 0;
		/* How many rows does the edit window take up? */
int editwincols = -1;
		/* The number of usable columns in the edit window: COLS - margin. */
int margin = 0;
		/* The amount of space reserved at the left for line numbers. */
#ifndef NANO_TINY
ssize_t stripe_column = 0;
		/* The column at which a vertical bar will be drawn. */
#endif

linestruct *cutbuffer = NULL;
		/* The buffer where we store cut text. */
linestruct *cutbottom = NULL;
		/* The last line in the cutbuffer. */
bool keep_cutbuffer = FALSE;
		/* Whether to add to the cutbuffer instead of clearing it first. */

openfilestruct *openfile = NULL;
		/* The list of all open file buffers. */
#ifdef ENABLE_MULTIBUFFER
openfilestruct *startfile = NULL;
		/* The first open buffer. */
#endif

#ifndef NANO_TINY
char *matchbrackets = NULL;
		/* The opening and closing brackets that bracket searches can find. */
char *whitespace = NULL;
		/* The characters used when visibly showing tabs and spaces. */
int whitelen[2];
		/* The length in bytes of these characters. */
#endif

#ifdef ENABLE_JUSTIFY
char *punct = NULL;
		/* The closing punctuation that can end sentences. */
char *brackets = NULL;
		/* The closing brackets that can follow closing punctuation and
		 * can end sentences. */
char *quotestr = NULL;
		/* The quoting string.  The default value is set in main(). */
regex_t quotereg;
		/* The compiled regular expression from the quoting string. */
#endif

char *word_chars = NULL;
		/* Nonalphanumeric characters that also form words. */

char *answer = NULL;
		/* The answer string used by the statusbar prompt. */

ssize_t tabsize = -1;
		/* The width of a tab in spaces.  The default is set in main(). */

#ifndef NANO_TINY
char *backup_dir = NULL;
		/* The directory where we store backup files. */

const char *locking_prefix = ".";
		/* Prefix of how to store the vim-style lock file. */
const char *locking_suffix = ".swp";
		/* Suffix of the vim-style lock file. */
#endif
#ifdef ENABLE_OPERATINGDIR
char *operating_dir = NULL;
		/* The path to our confining "operating" directory, when given. */
#endif

#ifdef ENABLE_SPELLER
char *alt_speller = NULL;
		/* The command to use for the alternate spell checker. */
#endif

#ifdef ENABLE_COLOR
syntaxtype *syntaxes = NULL;
		/* The global list of color syntaxes. */
char *syntaxstr = NULL;
		/* The color syntax name specified on the command line. */
bool have_palette = FALSE;
		/* Whether the colors for the current syntax have been initialized. */
#endif

bool refresh_needed = FALSE;
		/* Did a command mangle enough of the buffer that we should
		 * repaint the screen? */

int currmenu = MMOST;
		/* The currently active menu, initialized to a dummy value. */
keystruct *sclist = NULL;
		/* The start of the shortcuts list. */
funcstruct *allfuncs = NULL;
		/* The start of the functions list. */
funcstruct *tailfunc;
		/* The last function in the list. */
funcstruct *exitfunc;
		/* A pointer to the special Exit/Close item. */

linestruct *search_history = NULL;
		/* The current item in the list of strings that were searched for. */
linestruct *execute_history = NULL;
		/* The current item in the list of commands that were run with ^R ^X. */
linestruct *replace_history = NULL;
		/* The current item in the list of replace strings. */
#ifdef ENABLE_HISTORIES
linestruct *searchtop = NULL;
		/* The oldest item in the list of search strings. */
linestruct *searchbot = NULL;
		/* The newest item in the list of search strings. */

linestruct *replacetop = NULL;
linestruct *replacebot = NULL;

linestruct *executetop = NULL;
linestruct *executebot = NULL;
#endif

regex_t search_regexp;
		/* The compiled regular expression to use in searches. */
regmatch_t regmatches[10];
		/* The match positions for parenthetical subexpressions, 10
		 * maximum, used in regular expression searches. */

int hilite_attribute = A_REVERSE;
		/* The curses attribute we use to highlight something. */
#ifdef ENABLE_COLOR
colortype* color_combo[NUMBER_OF_ELEMENTS] = {NULL};
		/* The color combinations for interface elements given in the rcfile. */
#endif
int interface_color_pair[NUMBER_OF_ELEMENTS] = {0};
		/* The processed color pairs for the interface elements. */

char *homedir = NULL;
		/* The user's home directory, from $HOME or /etc/passwd. */
char *statedir = NULL;
		/* The directory for nano's history files. */

#ifdef ENABLE_NANORC
#define NUMBER_OF_MENUS  16
char *menunames[NUMBER_OF_MENUS] = { "main", "search", "replace", "replacewith",
									"yesno", "gotoline", "writeout", "insert",
									"extcmd", "help", "spell", "linter",
									"browser", "whereisfile", "gotodir",
									"all" };
int menusymbols[NUMBER_OF_MENUS] = { MMAIN, MWHEREIS, MREPLACE, MREPLACEWITH,
									MYESNO, MGOTOLINE, MWRITEFILE, MINSERTFILE,
									MEXTCMD, MHELP, MSPELL, MLINTER,
									MBROWSER, MWHEREISFILE, MGOTODIR,
									MMOST|MHELP|MYESNO };

char *rcfile_with_errors = NULL;
		/* The first nanorc file, if any, that produced warnings. */
#endif

bool spotlighted = FALSE;
	/* Whether any text is spotlighted. */
size_t light_from_col = 0;
	/* Where the spotlighted text starts. */
size_t light_to_col = 0;
	/* Where the spotlighted text ends. */

/* Return the number of entries in the shortcut list for a given menu. */
size_t length_of_list(int menu)
{
	funcstruct *f;
	size_t i = 0;

	for (f = allfuncs; f != NULL; f = f->next)
		if ((f->menus & menu) && first_sc_for(menu, f->func) != NULL)
			i++;

	return i;
}

/* To make the functions and shortcuts lists clearer. */
#define VIEW  TRUE    /* Is allowed in view mode. */
#define NOVIEW  FALSE
#define BLANKAFTER  TRUE    /* A blank line after this one. */
#define TOGETHER  FALSE
#ifdef ENABLE_MULTIBUFFER
#define CAN_OPEN_OTHER_BUFFER TRUE
#else
#define CAN_OPEN_OTHER_BUFFER FALSE
#endif

/* Empty functions, for the most part corresponding to toggles. */
void case_sens_void(void)
{
}
void regexp_void(void)
{
}
void backwards_void(void)
{
}
void flip_replace(void)
{
}
void flip_goto(void)
{
}
#ifdef ENABLE_BROWSER
void to_files_void(void)
{
}
void goto_dir_void(void)
{
}
#endif
#ifndef NANO_TINY
void do_toggle_void(void)
{
}
void dos_format_void(void)
{
}
void mac_format_void(void)
{
}
void append_void(void)
{
}
void prepend_void(void)
{
}
void backup_file_void(void)
{
}
void flip_execute(void)
{
}
void flip_pipe(void)
{
}
void flip_convert(void)
{
}
#endif
#ifdef ENABLE_MULTIBUFFER
void flip_newbuffer(void)
{
}
#endif
void discard_buffer(void)
{
}
void do_cancel(void)
{
}

/* Add a function to the function list. */
void add_to_funcs(void (*func)(void), int menus, const char *desc, const char *help,
	bool blank_after, bool viewok)
{
	funcstruct *f = nmalloc(sizeof(funcstruct));

	if (allfuncs == NULL)
		allfuncs = f;
	else
		tailfunc->next = f;
	tailfunc = f;

	f->next = NULL;
	f->func = func;
	f->menus = menus;
	f->desc = desc;
	f->viewok = viewok;
#ifdef ENABLE_HELP
	f->help = help;
	f->blank_after = blank_after;
#endif
}

/* Add a key combo to the shortcut list. */
void add_to_sclist(int menus, const char *scstring, const int keycode,
						void (*func)(void), int toggle)
{
	static keystruct *tailsc;
#ifndef NANO_TINY
	static int counter = 0;
#endif
	keystruct *s = nmalloc(sizeof(keystruct));

	/* Start the list, or tack on the next item. */
	if (sclist == NULL)
		sclist = s;
	else
		tailsc->next = s;
	s->next = NULL;

	/* Fill in the data. */
	s->menus = menus;
	s->func = func;
#ifndef NANO_TINY
	s->toggle = toggle;
	/* When not the same toggle as the previous one, increment the ID. */
	if (toggle)
		s->ordinal = (tailsc->toggle == toggle) ? counter : ++counter;
#endif
	assign_keyinfo(s, scstring, keycode);

	tailsc = s;
}

/* Return the first shortcut in the list of shortcuts that
 * matches the given func in the given menu. */
const keystruct *first_sc_for(int menu, void (*func)(void))
{
	const keystruct *s;

	for (s = sclist; s != NULL; s = s->next)
		if ((s->menus & menu) && s->func == func)
			return s;

	return NULL;
}

/* Return the first keycode that is bound to the given function in the
 * current menu, if any; otherwise, return the given default value. */
int the_code_for(void (*func)(void), int defaultval)
{
	const keystruct *s = first_sc_for(currmenu, func);

	if (s == NULL)
		return defaultval;

	meta_key = s->meta;
	return s->keycode;
}

/* Return a pointer to the function that is bound to the given key. */
functionptrtype func_from_key(int *kbinput)
{
	const keystruct *s = get_shortcut(kbinput);

	if (s)
		return s->func;
	else
		return NULL;
}

/* Set the string and its corresponding keycode for the given shortcut s. */
void assign_keyinfo(keystruct *s, const char *keystring, const int keycode)
{
	s->keystr = keystring;
	s->meta = (keystring[0] == 'M' && keycode == 0);

	if (keycode)
		s->keycode = keycode;
	else
		s->keycode = keycode_from_string(keystring);
}

/* Parse the given keystring and return the corresponding keycode,
 * or return -1 when the string is invalid. */
int keycode_from_string(const char *keystring)
{
	if (keystring[0] == '^') {
		if (strcasecmp(keystring, "^Space") == 0)
			return 0;
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
		if (strcasecmp(keystring, "^H") == 0)
			return KEY_BACKSPACE;
#endif
		if (keystring[1] <= '_' && strlen(keystring) == 2)
			return keystring[1] - 64;
		else
			return -1;
	} else if (keystring[0] == 'M') {
		if (strcasecmp(keystring, "M-Space") == 0)
			return (int)' ';
		if (keystring[1] == '-' && strlen(keystring) == 3)
			return tolower((unsigned char)keystring[2]);
		else
			return -1;
	} else if (keystring[0] == 'F') {
		int fn = atoi(&keystring[1]);
		if (fn < 1 || fn > 16)
			return -1;
		return KEY_F0 + fn;
	} else if (!strcasecmp(keystring, "Ins"))
		return KEY_IC;
	else if (!strcasecmp(keystring, "Del"))
		return KEY_DC;
	else
		return -1;
}

/* These two tags are used elsewhere too, so they are global. */
/* TRANSLATORS: Try to keep the next two strings at most 10 characters. */
const char *exit_tag = N_("Exit");
const char *close_tag = N_("Close");

/* Initialize the list of functions and the list of shortcuts. */
void shortcut_init(void)
{
#ifdef ENABLE_HELP
	/* TRANSLATORS: The next long series of strings are shortcut descriptions;
	 * they are best kept shorter than 56 characters, but may be longer. */
	const char *cancel_gist = N_("Cancel the current function");
	const char *help_gist = N_("Display this help text");
	const char *exit_gist =
		N_("Close the current buffer / Exit from nano");
	const char *writeout_gist =
		N_("Write the current buffer (or the marked region) to disk");
	const char *readfile_gist =
		N_("Insert another file into current buffer (or into new buffer)");
	const char *whereis_gist =
		N_("Search forward for a string or a regular expression");
	const char *wherewas_gist =
		N_("Search backward for a string or a regular expression");
#ifdef ENABLE_BROWSER
	const char *browserwhereis_gist = N_("Search forward for a string");
	const char *browserwherewas_gist = N_("Search backward for a string");
	const char *browserrefresh_gist = N_("Refresh the file list");
#ifndef NANO_TINY
	const char *browserlefthand_gist = N_("Go to lefthand column");
	const char *browserrighthand_gist = N_("Go to righthand column");
	const char *browsertoprow_gist = N_("Go to first row in this column");
	const char *browserbottomrow_gist = N_("Go to last row in this column");
#endif
#endif
	const char *prevpage_gist = N_("Go one screenful up");
	const char *nextpage_gist = N_("Go one screenful down");
	const char *cut_gist =
		N_("Cut current line (or marked region) and store it in cutbuffer");
	const char *uncut_gist =
		N_("Paste the contents of cutbuffer at current cursor position");
	const char *cursorpos_gist = N_("Display the position of the cursor");
#ifdef ENABLE_SPELLER
	const char *spell_gist = N_("Invoke the spell checker, if available");
#endif
	const char *replace_gist = N_("Replace a string or a regular expression");
	const char *gotoline_gist = N_("Go to line and column number");
#ifndef NANO_TINY
	const char *mark_gist = N_("Mark text starting from the cursor position");
	const char *copy_gist =
		N_("Copy current line (or marked region) and store it in cutbuffer");
	const char *zap_gist = N_("Throw away the current line (or marked region)");
	const char *indent_gist = N_("Indent the current line (or marked lines)");
	const char *unindent_gist = N_("Unindent the current line (or marked lines)");
	const char *undo_gist = N_("Undo the last operation");
	const char *redo_gist = N_("Redo the last undone operation");
#endif
	const char *back_gist = N_("Go back one character");
	const char *forward_gist = N_("Go forward one character");
	const char *prevword_gist = N_("Go back one word");
	const char *nextword_gist = N_("Go forward one word");
	const char *prevline_gist = N_("Go to previous line");
	const char *nextline_gist = N_("Go to next line");
	const char *home_gist = N_("Go to beginning of current line");
	const char *end_gist = N_("Go to end of current line");
	const char *prevblock_gist = N_("Go to previous block of text");
	const char *nextblock_gist = N_("Go to next block of text");
#ifdef ENABLE_JUSTIFY
	const char *parabegin_gist =
		N_("Go to beginning of paragraph; then of previous paragraph");
	const char *paraend_gist =
		N_("Go just beyond end of paragraph; then of next paragraph");
#endif
	const char *firstline_gist = N_("Go to the first line of the file");
	const char *lastline_gist = N_("Go to the last line of the file");
#ifndef NANO_TINY
	const char *bracket_gist = N_("Go to the matching bracket");
#endif
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
	const char *scrollup_gist =
		N_("Scroll up one line without moving the cursor textually");
	const char *scrolldown_gist =
		N_("Scroll down one line without moving the cursor textually");
#endif
#ifdef ENABLE_MULTIBUFFER
	const char *prevfile_gist = N_("Switch to the previous file buffer");
	const char *nextfile_gist = N_("Switch to the next file buffer");
#endif
	const char *verbatim_gist = N_("Insert the next keystroke verbatim");
	const char *tab_gist = N_("Insert a tab at the cursor position");
	const char *enter_gist = N_("Insert a newline at the cursor position");
	const char *delete_gist = N_("Delete the character under the cursor");
	const char *backspace_gist =
		N_("Delete the character to the left of the cursor");
#ifndef NANO_TINY
	const char *chopwordleft_gist =
		N_("Delete backward from cursor to word start");
	const char *chopwordright_gist =
		N_("Delete forward from cursor to next word start");
	const char *cuttilleof_gist =
		N_("Cut from the cursor position to the end of the file");
#endif
#ifdef ENABLE_JUSTIFY
	const char *justify_gist = N_("Justify the current paragraph");
	const char *fulljustify_gist = N_("Justify the entire file");
#endif
#ifndef NANO_TINY
	const char *wordcount_gist =
		N_("Count the number of words, lines, and characters");
#endif
	const char *refresh_gist =
		N_("Refresh (redraw) the current screen");
	const char *suspend_gist =
		N_("Suspend the editor (if suspension is enabled)");
#ifdef ENABLE_WORDCOMPLETION
	const char *completion_gist = N_("Try and complete the current word");
#endif
#ifdef ENABLE_COMMENT
	const char *comment_gist =
		N_("Comment/uncomment the current line (or marked lines)");
#endif
	const char *savefile_gist = N_("Save file without prompting");
	const char *findprev_gist = N_("Search next occurrence backward");
	const char *findnext_gist = N_("Search next occurrence forward");
#ifndef NANO_TINY
	const char *recordmacro_gist = N_("Start/stop recording a macro");
	const char *runmacro_gist = N_("Run the last recorded macro");
#endif
	const char *case_gist =
		N_("Toggle the case sensitivity of the search");
	const char *reverse_gist =
		N_("Reverse the direction of the search");
	const char *regexp_gist =
		N_("Toggle the use of regular expressions");
#ifdef ENABLE_HISTORIES
	const char *older_gist =
		N_("Recall the previous search/replace string");
	const char *newer_gist =
		N_("Recall the next search/replace string");
#endif
#ifndef NANO_TINY
	const char *dos_gist = N_("Toggle the use of DOS format");
	const char *mac_gist = N_("Toggle the use of Mac format");
	const char *append_gist = N_("Toggle appending");
	const char *prepend_gist = N_("Toggle prepending");
	const char *backup_gist = N_("Toggle backing up of the original file");
	const char *execute_gist = N_("Execute external command");
	const char *pipe_gist =
		N_("Pipe the current buffer (or marked region) to the command");
	const char *convert_gist = N_("Do not convert from DOS/Mac format");
#endif
#ifdef ENABLE_MULTIBUFFER
	const char *newbuffer_gist = N_("Toggle the use of a new buffer");
#endif
	const char *discardbuffer_gist = N_("Close buffer without saving it");
#ifdef ENABLE_BROWSER
	const char *tofiles_gist = N_("Go to file browser");
	const char *exitbrowser_gist = N_("Exit from the file browser");
	const char *firstfile_gist = N_("Go to the first file in the list");
	const char *lastfile_gist = N_("Go to the last file in the list");
	const char *backfile_gist = N_("Go to the previous file in the list");
	const char *forwardfile_gist = N_("Go to the next file in the list");
	const char *gotodir_gist = N_("Go to directory");
#endif
#ifdef ENABLE_COLOR
	const char *lint_gist = N_("Invoke the linter, if available");
	const char *prevlint_gist = N_("Go to previous linter msg");
	const char *nextlint_gist = N_("Go to next linter msg");
#endif
#endif /* ENABLE_HELP */

#ifdef ENABLE_HELP
#define WITHORSANS(help) help
#else
#define WITHORSANS(help) ""
#endif

	/* Start populating the different menus with functions. */

	add_to_funcs(do_help_void, (MMOST | MBROWSER) & ~MFINDINHELP,
		/* TRANSLATORS: Try to keep the next ninety strings or so at most 10
		 * characters.  Some strings may be longer -- run nano and see. */
		N_("Get Help"), WITHORSANS(help_gist), TOGETHER, VIEW);

	add_to_funcs(do_cancel, ((MMOST & ~MMAIN) | MYESNO),
		N_("Cancel"), WITHORSANS(cancel_gist), BLANKAFTER, VIEW);

	add_to_funcs(do_exit, MMAIN,
		exit_tag, WITHORSANS(exit_gist), TOGETHER, VIEW);
	/* Remember the entry for Exit, to be able to replace it with Close. */
	exitfunc = tailfunc;

#ifdef ENABLE_BROWSER
	add_to_funcs(do_exit, MBROWSER,
		close_tag, WITHORSANS(exitbrowser_gist), TOGETHER, VIEW);
#endif

	add_to_funcs(do_writeout_void, MMAIN,
		N_("Write Out"), WITHORSANS(writeout_gist), TOGETHER, NOVIEW);

#ifdef ENABLE_JUSTIFY
	/* In restricted mode, replace Insert with Justify, when possible;
	 * otherwise, show Insert anyway, to keep the help items paired. */
	if (!ISSET(RESTRICTED))
#endif
		add_to_funcs(do_insertfile_void, MMAIN,
				N_("Read File"), WITHORSANS(readfile_gist), BLANKAFTER,
				/* We allow inserting files in view mode if multibuffer mode
				 * is available, so that the user can view multiple files. */
				CAN_OPEN_OTHER_BUFFER);
#ifdef ENABLE_JUSTIFY
	else
		add_to_funcs(do_justify_void, MMAIN,
				N_("Justify"), WITHORSANS(justify_gist), BLANKAFTER, NOVIEW);
#endif

#ifdef ENABLE_HELP
	/* The description ("x") and blank_after (0) are irrelevant,
	 * because the help viewer does not have a help text. */
	add_to_funcs(total_refresh, MHELP, N_("Refresh"), "x", 0, VIEW);
	add_to_funcs(do_exit, MHELP, close_tag, "x", 0, VIEW);
#endif

	add_to_funcs(do_search_forward, MMAIN|MHELP,
		N_("Where Is"), WITHORSANS(whereis_gist), TOGETHER, VIEW);

	add_to_funcs(do_replace, MMAIN,
		N_("Replace"), WITHORSANS(replace_gist), TOGETHER, NOVIEW);

#ifdef ENABLE_BROWSER
	add_to_funcs(goto_dir_void, MBROWSER,
		N_("Go To Dir"), WITHORSANS(gotodir_gist), TOGETHER, VIEW);

	add_to_funcs(total_refresh, MBROWSER,
		N_("Refresh"), WITHORSANS(browserrefresh_gist), BLANKAFTER, VIEW);
#endif

	add_to_funcs(cut_text, MMAIN,
		N_("Cut Text"), WITHORSANS(cut_gist), TOGETHER, NOVIEW);

	add_to_funcs(paste_text, MMAIN,
		N_("Paste Text"), WITHORSANS(uncut_gist), BLANKAFTER, NOVIEW);

	if (!ISSET(RESTRICTED)) {
#ifdef ENABLE_JUSTIFY
		add_to_funcs(do_justify_void, MMAIN,
				N_("Justify"), WITHORSANS(justify_gist), TOGETHER, NOVIEW);
#endif
#ifdef ENABLE_SPELLER
		add_to_funcs(do_spell, MMAIN,
				N_("To Spell"), WITHORSANS(spell_gist), BLANKAFTER, NOVIEW);
#endif
	}

	add_to_funcs(do_cursorpos_void, MMAIN,
		N_("Cur Pos"), WITHORSANS(cursorpos_gist), TOGETHER, VIEW);

#if (defined(ENABLE_JUSTIFY) && (defined(ENABLE_SPELLER) || defined(ENABLE_COLOR)) || \
		!defined(ENABLE_JUSTIFY) && !defined(ENABLE_SPELLER) && !defined(ENABLE_COLOR))
	/* Conditionally placing this one here or further on, to keep the
	 * help items nicely paired in most conditions. */
	add_to_funcs(do_gotolinecolumn_void, MMAIN,
		N_("Go To Line"), WITHORSANS(gotoline_gist), BLANKAFTER, VIEW);
#endif

#ifndef NANO_TINY
	add_to_funcs(do_undo, MMAIN,
		N_("Undo"), WITHORSANS(undo_gist), TOGETHER, NOVIEW);
	add_to_funcs(do_redo, MMAIN,
		N_("Redo"), WITHORSANS(redo_gist), BLANKAFTER, NOVIEW);

	add_to_funcs(do_mark, MMAIN,
		N_("Mark Text"), WITHORSANS(mark_gist), TOGETHER, VIEW);
	add_to_funcs(copy_text, MMAIN,
		N_("Copy Text"), WITHORSANS(copy_gist), BLANKAFTER, NOVIEW);
#endif

	add_to_funcs(case_sens_void, MWHEREIS|MREPLACE,
		N_("Case Sens"), WITHORSANS(case_gist), TOGETHER, VIEW);
	add_to_funcs(regexp_void, MWHEREIS|MREPLACE,
		N_("Regexp"), WITHORSANS(regexp_gist), TOGETHER, VIEW);
	add_to_funcs(backwards_void, MWHEREIS|MREPLACE,
		N_("Backwards"), WITHORSANS(reverse_gist), TOGETHER, VIEW);

	add_to_funcs(flip_replace, MWHEREIS,
		N_("Replace"), WITHORSANS(replace_gist), BLANKAFTER, VIEW);

	add_to_funcs(flip_replace, MREPLACE,
		N_("No Replace"), WITHORSANS(whereis_gist), BLANKAFTER, VIEW);

#ifdef ENABLE_HISTORIES
	add_to_funcs(get_history_older_void, MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE,
		N_("Older"), WITHORSANS(older_gist), TOGETHER, VIEW);
	add_to_funcs(get_history_newer_void, MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE,
		N_("Newer"), WITHORSANS(newer_gist), BLANKAFTER, VIEW);
#endif

	add_to_funcs(flip_goto, MWHEREIS,
		N_("Go To Line"), WITHORSANS(gotoline_gist), TOGETHER, VIEW);

#ifdef ENABLE_JUSTIFY
	add_to_funcs(do_full_justify, MWHEREIS,
		N_("FullJstify"), WITHORSANS(fulljustify_gist), BLANKAFTER, NOVIEW);
#endif

#ifndef NANO_TINY
	add_to_funcs(do_find_bracket, MMAIN,
		N_("To Bracket"), WITHORSANS(bracket_gist), BLANKAFTER, VIEW);
#endif

#ifdef ENABLE_BROWSER
	add_to_funcs(do_search_forward, MBROWSER,
		N_("Where Is"), WITHORSANS(browserwhereis_gist), TOGETHER, VIEW);
	add_to_funcs(do_search_backward, MBROWSER,
		N_("Where Was"), WITHORSANS(browserwherewas_gist), TOGETHER, VIEW);
#endif

#ifndef NANO_TINY
	add_to_funcs(do_search_backward, MMAIN|MHELP,
		/* TRANSLATORS: This starts a backward search. */
		N_("Where Was"), WITHORSANS(wherewas_gist), TOGETHER, VIEW);
#endif
	add_to_funcs(do_findprevious, MMAIN|MHELP|MBROWSER,
		/* TRANSLATORS: This refers to searching the preceding occurrence. */
		N_("Previous"), WITHORSANS(findprev_gist), TOGETHER, VIEW);
#ifndef NANO_TINY
	add_to_funcs(do_findnext, MMAIN|MHELP|MBROWSER,
		N_("Next"), WITHORSANS(findnext_gist), BLANKAFTER, VIEW);
#else
	/* In the tiny version, arrange things a bit differently, so that
	 * the useful M-W and M-Q are shown in the help lines. */
	add_to_funcs(do_findnext, MMAIN|MHELP|MBROWSER,
		N_("Next"), WITHORSANS(findnext_gist), TOGETHER, VIEW);
	add_to_funcs(do_search_backward, MMAIN|MHELP,
		N_("Where Was"), WITHORSANS(wherewas_gist), BLANKAFTER, VIEW);
	add_to_funcs(total_refresh, MMAIN,
		N_("Refresh"), WITHORSANS(refresh_gist), BLANKAFTER, VIEW);
#endif

	add_to_funcs(do_left, MMAIN,
		/* TRANSLATORS: This means move the cursor one character back. */
		N_("Back"), WITHORSANS(back_gist), TOGETHER, VIEW);
	add_to_funcs(do_right, MMAIN,
		N_("Forward"), WITHORSANS(forward_gist), TOGETHER, VIEW);
#ifdef ENABLE_BROWSER
	add_to_funcs(do_left, MBROWSER,
		N_("Back"), WITHORSANS(backfile_gist), TOGETHER, VIEW);
	add_to_funcs(do_right, MBROWSER,
		N_("Forward"), WITHORSANS(forwardfile_gist), TOGETHER, VIEW);
#endif

	add_to_funcs(do_prev_word_void, MMAIN,
		N_("Prev Word"), WITHORSANS(prevword_gist), TOGETHER, VIEW);
	add_to_funcs(do_next_word_void, MMAIN,
		N_("Next Word"), WITHORSANS(nextword_gist), TOGETHER, VIEW);

	add_to_funcs(do_home, MMAIN,
		N_("Home"), WITHORSANS(home_gist), TOGETHER, VIEW);
	add_to_funcs(do_end, MMAIN,
		N_("End"), WITHORSANS(end_gist), BLANKAFTER, VIEW);

	add_to_funcs(do_up, MMAIN|MHELP|MBROWSER,
		N_("Prev Line"), WITHORSANS(prevline_gist), TOGETHER, VIEW);
	add_to_funcs(do_down, MMAIN|MHELP|MBROWSER,
		N_("Next Line"), WITHORSANS(nextline_gist), TOGETHER, VIEW);
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
	add_to_funcs(do_scroll_up, MMAIN,
		N_("Scroll Up"), WITHORSANS(scrollup_gist), TOGETHER, VIEW);
	add_to_funcs(do_scroll_down, MMAIN,
		N_("Scroll Down"), WITHORSANS(scrolldown_gist), BLANKAFTER, VIEW);
#endif

	add_to_funcs(do_prev_block, MMAIN,
		N_("Prev Block"), WITHORSANS(prevblock_gist), TOGETHER, VIEW);
	add_to_funcs(do_next_block, MMAIN,
		N_("Next Block"), WITHORSANS(nextblock_gist), TOGETHER, VIEW);
#ifdef ENABLE_JUSTIFY
	add_to_funcs(do_para_begin_void, MMAIN|MGOTOLINE,
		N_("Beg of Par"), WITHORSANS(parabegin_gist), TOGETHER, VIEW);
	add_to_funcs(do_para_end_void, MMAIN|MGOTOLINE,
		N_("End of Par"), WITHORSANS(paraend_gist), BLANKAFTER, VIEW);
#endif

	add_to_funcs(do_page_up, MMAIN|MHELP,
		N_("Prev Page"), WITHORSANS(prevpage_gist), TOGETHER, VIEW);
	add_to_funcs(do_page_down, MMAIN|MHELP,
		N_("Next Page"), WITHORSANS(nextpage_gist), TOGETHER, VIEW);

	add_to_funcs(to_first_line, MMAIN|MHELP|MGOTOLINE,
		N_("First Line"), WITHORSANS(firstline_gist), TOGETHER, VIEW);
	add_to_funcs(to_last_line, MMAIN|MHELP|MGOTOLINE,
		N_("Last Line"), WITHORSANS(lastline_gist), BLANKAFTER, VIEW);

#ifdef ENABLE_MULTIBUFFER
	add_to_funcs(switch_to_prev_buffer, MMAIN,
		N_("Prev File"), WITHORSANS(prevfile_gist), TOGETHER, VIEW);
	add_to_funcs(switch_to_next_buffer, MMAIN,
		N_("Next File"), WITHORSANS(nextfile_gist), BLANKAFTER, VIEW);
#endif

#if (!defined(ENABLE_JUSTIFY) && (defined(ENABLE_SPELLER) || defined(ENABLE_COLOR)) || \
		defined(ENABLE_JUSTIFY) && !defined(ENABLE_SPELLER) && !defined(ENABLE_COLOR))
	add_to_funcs(do_gotolinecolumn_void, MMAIN,
		N_("Go To Line"), WITHORSANS(gotoline_gist), BLANKAFTER, VIEW);
#endif

	add_to_funcs(do_tab, MMAIN,
		/* TRANSLATORS: The next four strings are names of keyboard keys. */
		N_("Tab"), WITHORSANS(tab_gist), TOGETHER, NOVIEW);
	add_to_funcs(do_enter, MMAIN,
		N_("Enter"), WITHORSANS(enter_gist), BLANKAFTER, NOVIEW);

	add_to_funcs(do_backspace, MMAIN,
		N_("Backspace"), WITHORSANS(backspace_gist), TOGETHER, NOVIEW);
	add_to_funcs(do_delete, MMAIN,
		N_("Delete"), WITHORSANS(delete_gist),
#ifndef NANO_TINY
		TOGETHER,
#else
		BLANKAFTER,
#endif
		NOVIEW);

#ifndef NANO_TINY
	add_to_funcs(chop_previous_word, MMAIN,
		/* TRANSLATORS: The next two strings refer to deleting words. */
		N_("Chop Left"), WITHORSANS(chopwordleft_gist), TOGETHER, NOVIEW);
	add_to_funcs(chop_next_word, MMAIN,
		N_("Chop Right"), WITHORSANS(chopwordright_gist), TOGETHER, NOVIEW);
	add_to_funcs(cut_till_eof, MMAIN,
		N_("CutTillEnd"), WITHORSANS(cuttilleof_gist), BLANKAFTER, NOVIEW);
#endif

#ifdef ENABLE_JUSTIFY
	add_to_funcs(do_full_justify, MMAIN,
		N_("FullJstify"), WITHORSANS(fulljustify_gist), TOGETHER, NOVIEW);
#endif

#ifndef NANO_TINY
	add_to_funcs(do_wordlinechar_count, MMAIN,
		N_("Word Count"), WITHORSANS(wordcount_gist), TOGETHER, VIEW);
#endif

	add_to_funcs(do_verbatim_input, MMAIN,
		N_("Verbatim"), WITHORSANS(verbatim_gist), BLANKAFTER, NOVIEW);

#ifndef NANO_TINY
	add_to_funcs(total_refresh, MMAIN,
		N_("Refresh"), WITHORSANS(refresh_gist), TOGETHER, VIEW);
#endif

	add_to_funcs(do_suspend_void, MMAIN,
		N_("Suspend"), WITHORSANS(suspend_gist), BLANKAFTER, VIEW);

#ifndef NANO_TINY
	add_to_funcs(do_indent, MMAIN,
		N_("Indent"), WITHORSANS(indent_gist), TOGETHER, NOVIEW);
	add_to_funcs(do_unindent, MMAIN,
		N_("Unindent"), WITHORSANS(unindent_gist), BLANKAFTER, NOVIEW);
#endif
#ifdef ENABLE_COMMENT
	add_to_funcs(do_comment, MMAIN,
		N_("Comment Lines"), WITHORSANS(comment_gist), TOGETHER, NOVIEW);
#endif
#ifdef ENABLE_WORDCOMPLETION
	add_to_funcs(complete_a_word, MMAIN,
		N_("Complete"), WITHORSANS(completion_gist), BLANKAFTER, NOVIEW);
#endif
#ifndef NANO_TINY
	add_to_funcs(record_macro, MMAIN,
		N_("Record"), WITHORSANS(recordmacro_gist), TOGETHER, VIEW);
	add_to_funcs(run_macro, MMAIN,
		N_("Run Macro"), WITHORSANS(runmacro_gist), BLANKAFTER, VIEW);

	add_to_funcs(zap_text, MMAIN,
		N_("Zap Text"), WITHORSANS(zap_gist), BLANKAFTER, NOVIEW);

#ifdef ENABLE_COLOR
	if (!ISSET(RESTRICTED))
		add_to_funcs(do_linter, MMAIN,
				N_("To Linter"), WITHORSANS(lint_gist), BLANKAFTER, NOVIEW);
#endif
#endif
	add_to_funcs(do_savefile, MMAIN,
		N_("Save"), WITHORSANS(savefile_gist), BLANKAFTER, NOVIEW);

	add_to_funcs(flip_goto, MGOTOLINE,
		N_("Go To Text"), WITHORSANS(whereis_gist), BLANKAFTER, VIEW);

#ifndef NANO_TINY
	add_to_funcs(dos_format_void, MWRITEFILE,
		N_("DOS Format"), WITHORSANS(dos_gist), TOGETHER, NOVIEW);
	add_to_funcs(mac_format_void, MWRITEFILE,
		N_("Mac Format"), WITHORSANS(mac_gist), TOGETHER, NOVIEW);

	/* If we're using restricted mode, the Append, Prepend, and Backup toggles
	 * are disabled.  The first and second are not useful as they only allow
	 * reduplicating the current file, and the third is not allowed as it
	 * would write to a file not specified on the command line. */
	if (!ISSET(RESTRICTED)) {
		add_to_funcs(append_void, MWRITEFILE,
			N_("Append"), WITHORSANS(append_gist), TOGETHER, NOVIEW);
		add_to_funcs(prepend_void, MWRITEFILE,
			N_("Prepend"), WITHORSANS(prepend_gist), TOGETHER, NOVIEW);

		add_to_funcs(backup_file_void, MWRITEFILE,
			N_("Backup File"), WITHORSANS(backup_gist), BLANKAFTER, NOVIEW);
	}
#endif /* !NANO_TINY */
#ifdef ENABLE_MULTIBUFFER
	/* Multiple buffers are only available when not in restricted mode. */
	if (!ISSET(RESTRICTED))
		add_to_funcs(flip_newbuffer, MINSERTFILE|MEXTCMD,
			N_("New Buffer"), WITHORSANS(newbuffer_gist), TOGETHER, NOVIEW);
#endif
#ifndef NANO_TINY
	add_to_funcs(flip_convert, MINSERTFILE,
		N_("No Conversion"), WITHORSANS(convert_gist), TOGETHER, NOVIEW);

	/* Command execution is only available when not in restricted mode. */
	if (!ISSET(RESTRICTED) && !ISSET(VIEW_MODE)) {
		add_to_funcs(flip_execute, MINSERTFILE,
			N_("Execute Command"), WITHORSANS(execute_gist), TOGETHER, NOVIEW);

		add_to_funcs(flip_pipe, MEXTCMD,
			N_("Pipe Text"), WITHORSANS(pipe_gist), TOGETHER, NOVIEW);

		add_to_funcs(flip_execute, MEXTCMD,
			N_("Read File"), WITHORSANS(readfile_gist), TOGETHER, NOVIEW);
	}
#endif
#ifdef ENABLE_BROWSER
	/* The file browser is only available when not in restricted mode. */
	if (!ISSET(RESTRICTED))
		add_to_funcs(to_files_void, MWRITEFILE|MINSERTFILE,
			N_("To Files"), WITHORSANS(tofiles_gist), TOGETHER, VIEW);

	add_to_funcs(do_page_up, MBROWSER,
		N_("Prev Page"), WITHORSANS(prevpage_gist), TOGETHER, VIEW);
	add_to_funcs(do_page_down, MBROWSER,
		N_("Next Page"), WITHORSANS(nextpage_gist), TOGETHER, VIEW);

	add_to_funcs(to_first_file, MBROWSER|MWHEREISFILE,
		N_("First File"), WITHORSANS(firstfile_gist), TOGETHER, VIEW);
	add_to_funcs(to_last_file, MBROWSER|MWHEREISFILE,
		N_("Last File"), WITHORSANS(lastfile_gist), BLANKAFTER, VIEW);
#ifndef NANO_TINY
	add_to_funcs(do_prev_word_void, MBROWSER,
		N_("Left Column"), WITHORSANS(browserlefthand_gist), TOGETHER, VIEW);
	add_to_funcs(do_next_word_void, MBROWSER,
		N_("Right Column"), WITHORSANS(browserrighthand_gist), TOGETHER, VIEW);
	add_to_funcs(do_prev_block, MBROWSER,
		N_("Top Row"), WITHORSANS(browsertoprow_gist), TOGETHER, VIEW);
	add_to_funcs(do_next_block, MBROWSER,
		N_("Bottom Row"), WITHORSANS(browserbottomrow_gist), BLANKAFTER, VIEW);
#endif
#endif /* ENABLE_BROWSER */

	add_to_funcs(discard_buffer, MWRITEFILE,
		N_("Discard buffer"), WITHORSANS(discardbuffer_gist), BLANKAFTER, NOVIEW);

#ifdef ENABLE_COLOR
	add_to_funcs(do_page_up, MLINTER,
		/* TRANSLATORS: Try to keep the next two strings at most 20 characters. */
		N_("Prev Lint Msg"), WITHORSANS(prevlint_gist), TOGETHER, VIEW);
	add_to_funcs(do_page_down, MLINTER,
		N_("Next Lint Msg"), WITHORSANS(nextlint_gist), TOGETHER, VIEW);
#endif

	/* Link key combos to functions in certain menus. */
	add_to_sclist(MMOST|MBROWSER, "^M", 0, do_enter, 0);
	add_to_sclist(MMOST|MBROWSER, "Enter", KEY_ENTER, do_enter, 0);
	add_to_sclist(MMOST, "^H", BS_CODE, do_backspace, 0);
	add_to_sclist(MMOST, "Bsp", KEY_BACKSPACE, do_backspace, 0);
	add_to_sclist(MMOST, "Sh-Del", SHIFT_DELETE, do_backspace, 0);
	add_to_sclist(MMOST, "^D", 0, do_delete, 0);
	add_to_sclist(MMOST, "Del", 0, do_delete, 0);
	add_to_sclist(MMOST, "^I", 0, do_tab, 0);
	add_to_sclist(MMOST, "Tab", TAB_CODE, do_tab, 0);
	add_to_sclist((MMOST|MBROWSER) & ~MFINDINHELP, "^G", 0, do_help_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^X", 0, do_exit, 0);
	if (!ISSET(PRESERVE))
		add_to_sclist(MMAIN, "^S", 0, do_savefile, 0);
	add_to_sclist(MMAIN, "^O", 0, do_writeout_void, 0);
	add_to_sclist(MMAIN, "^R", 0, do_insertfile_void, 0);
	add_to_sclist(MMAIN, "Ins", 0, do_insertfile_void, 0);
	if (!ISSET(PRESERVE))
		add_to_sclist(MMAIN|MHELP|MBROWSER, "^Q", 0, do_search_backward, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^W", 0, do_search_forward, 0);
	add_to_sclist(MMAIN, "^\\", 0, do_replace, 0);
	add_to_sclist(MMAIN, "M-R", 0, do_replace, 0);
	add_to_sclist(MMOST, "^K", 0, cut_text, 0);
	add_to_sclist(MMOST, "^U", 0, paste_text, 0);
#ifdef ENABLE_JUSTIFY
	add_to_sclist(MMAIN, "^J", 0, do_justify_void, 0);
#endif
#ifdef ENABLE_SPELLER
	add_to_sclist(MMAIN, "^T", 0, do_spell, 0);
#endif
#ifdef ENABLE_COLOR
	add_to_sclist(MMAIN, "M-B", 0, do_linter, 0);
#endif
	add_to_sclist(MMAIN, "^C", 0, do_cursorpos_void, 0);
	add_to_sclist(MMAIN, "^_", 0, do_gotolinecolumn_void, 0);
	add_to_sclist(MMAIN, "M-G", 0, do_gotolinecolumn_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "^Y", 0, do_page_up, 0);
	add_to_sclist(MHELP|MBROWSER, "Bsp", KEY_BACKSPACE, do_page_up, 0);
	add_to_sclist(MHELP|MBROWSER, "Sh-Del", SHIFT_DELETE, do_page_up, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "PgUp", KEY_PPAGE, do_page_up, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "^V", 0, do_page_down, 0);
	add_to_sclist(MHELP|MBROWSER, "Space", 0x20, do_page_down, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "PgDn", KEY_NPAGE, do_page_down, 0);
	add_to_sclist(MMAIN|MHELP, "M-\\", 0, to_first_line, 0);
	add_to_sclist(MMAIN|MHELP, "^Home", CONTROL_HOME, to_first_line, 0);
	add_to_sclist(MMAIN|MHELP, "M-/", 0, to_last_line, 0);
	add_to_sclist(MMAIN|MHELP, "^End", CONTROL_END, to_last_line, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "M-W", 0, do_findnext, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "M-Q", 0, do_findprevious, 0);
#ifndef NANO_TINY
	add_to_sclist(MMAIN, "M-]", 0, do_find_bracket, 0);
	add_to_sclist(MMAIN, "M-A", 0, do_mark, 0);
	add_to_sclist(MMAIN, "^6", 0, do_mark, 0);
	add_to_sclist(MMAIN, "^^", 0, do_mark, 0);
	add_to_sclist(MMAIN, "M-6", 0, copy_text, 0);
	add_to_sclist(MMAIN, "M-^", 0, copy_text, 0);
	add_to_sclist(MMAIN, "M-}", 0, do_indent, 0);
	add_to_sclist(MMAIN, "Tab", TAB_CODE, do_indent, 0);
	add_to_sclist(MMAIN, "M-{", 0, do_unindent, 0);
	add_to_sclist(MMAIN, "Sh-Tab", SHIFT_TAB, do_unindent, 0);
	add_to_sclist(MMAIN, "M-:", 0, record_macro, 0);
	add_to_sclist(MMAIN, "M-;", 0, run_macro, 0);
	add_to_sclist(MMAIN, "M-U", 0, do_undo, 0);
	add_to_sclist(MMAIN, "M-E", 0, do_redo, 0);
	add_to_sclist(MMAIN, "Sh-^Del", CONTROL_SHIFT_DELETE, chop_previous_word, 0);
	add_to_sclist(MMAIN, "^Del", CONTROL_DELETE, chop_next_word, 0);
	add_to_sclist(MMAIN, "M-Del", ALT_DELETE, zap_text, 0);
#endif
#ifdef ENABLE_WORDCOMPLETION
	add_to_sclist(MMAIN, "^]", 0, complete_a_word, 0);
#endif
#ifdef ENABLE_COMMENT
	add_to_sclist(MMAIN, "M-3", 0, do_comment, 0);
#endif
	add_to_sclist(MMOST|MBROWSER, "^B", 0, do_left, 0);
	add_to_sclist(MMOST|MBROWSER, "^F", 0, do_right, 0);
#ifdef ENABLE_UTF8
	if (using_utf8()) {
		add_to_sclist(MMOST|MHELP|MBROWSER, "\xE2\x97\x80", KEY_LEFT, do_left, 0);
		add_to_sclist(MMOST|MHELP|MBROWSER, "\xE2\x96\xb6", KEY_RIGHT, do_right, 0);
		add_to_sclist(MSOME, "^\xE2\x97\x80", CONTROL_LEFT, do_prev_word_void, 0);
		add_to_sclist(MSOME, "^\xE2\x96\xb6", CONTROL_RIGHT, do_next_word_void, 0);
#if !defined(NANO_TINY) && defined(ENABLE_MULTIBUFFER)
		if (!on_a_vt) {
			add_to_sclist(MMAIN, "M-\xE2\x97\x80", ALT_LEFT, switch_to_prev_buffer, 0);
			add_to_sclist(MMAIN, "M-\xE2\x96\xb6", ALT_RIGHT, switch_to_next_buffer, 0);
		}
#endif
	} else
#endif
	{
		add_to_sclist(MMOST|MHELP|MBROWSER, "Left", KEY_LEFT, do_left, 0);
		add_to_sclist(MMOST|MHELP|MBROWSER, "Right", KEY_RIGHT, do_right, 0);
		add_to_sclist(MSOME, "^Left", CONTROL_LEFT, do_prev_word_void, 0);
		add_to_sclist(MSOME, "^Right", CONTROL_RIGHT, do_next_word_void, 0);
#ifdef ENABLE_MULTIBUFFER
		if (!on_a_vt) {
			add_to_sclist(MMAIN, "M-Left", ALT_LEFT, switch_to_prev_buffer, 0);
			add_to_sclist(MMAIN, "M-Right", ALT_RIGHT, switch_to_next_buffer, 0);
		}
#endif
	}
#ifdef NANO_TINY
	add_to_sclist(MMAIN, "M-B", 0, do_prev_word_void, 0);
	add_to_sclist(MMAIN, "M-D", 0, do_prev_word_void, 0);
	add_to_sclist(MMAIN, "M-F", 0, do_next_word_void, 0);
	add_to_sclist(MMAIN, "M-N", 0, do_next_word_void, 0);
#endif
	add_to_sclist(MMOST|MBROWSER, "M-Space", 0, do_prev_word_void, 0);
	add_to_sclist(MMOST|MBROWSER, "^Space", 0, do_next_word_void, 0);
	add_to_sclist(MMOST, "^A", 0, do_home, 0);
	add_to_sclist(MMOST, "Home", KEY_HOME, do_home, 0);
	add_to_sclist(MMOST, "^E", 0, do_end, 0);
	add_to_sclist(MMOST, "End", KEY_END, do_end, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^P", 0, do_up, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "^N", 0, do_down, 0);
#ifdef ENABLE_UTF8
	if (using_utf8()) {
		add_to_sclist(MMAIN|MHELP|MBROWSER, "\xE2\x96\xb2", KEY_UP, do_up, 0);
		add_to_sclist(MMAIN|MHELP|MBROWSER, "\xE2\x96\xbc", KEY_DOWN, do_down, 0);
		add_to_sclist(MMAIN|MBROWSER|MLINTER, "^\xE2\x96\xb2", CONTROL_UP, do_prev_block, 0);
		add_to_sclist(MMAIN|MBROWSER|MLINTER, "^\xE2\x96\xbc", CONTROL_DOWN, do_next_block, 0);
	} else
#endif
	{
		add_to_sclist(MMAIN|MHELP|MBROWSER, "Up", KEY_UP, do_up, 0);
		add_to_sclist(MMAIN|MHELP|MBROWSER, "Down", KEY_DOWN, do_down, 0);
		add_to_sclist(MMAIN|MBROWSER|MLINTER, "^Up", CONTROL_UP, do_prev_block, 0);
		add_to_sclist(MMAIN|MBROWSER|MLINTER, "^Down", CONTROL_DOWN, do_next_block, 0);
	}
	add_to_sclist(MMAIN, "M-7", 0, do_prev_block, 0);
	add_to_sclist(MMAIN, "M-8", 0, do_next_block, 0);
#ifdef ENABLE_JUSTIFY
	add_to_sclist(MMAIN, "M-(", 0, do_para_begin_void, 0);
	add_to_sclist(MMAIN, "M-9", 0, do_para_begin_void, 0);
	add_to_sclist(MMAIN, "M-)", 0, do_para_end_void, 0);
	add_to_sclist(MMAIN, "M-0", 0, do_para_end_void, 0);
#endif
#ifndef NANO_TINY
#ifdef ENABLE_UTF8
	if (using_utf8()) {
		add_to_sclist(MMAIN|MHELP, "M-\xE2\x96\xb2", ALT_UP, do_scroll_up, 0);
		add_to_sclist(MMAIN|MHELP, "M-\xE2\x96\xbc", ALT_DOWN, do_scroll_down, 0);
	} else
#endif
	{
		add_to_sclist(MMAIN|MHELP, "M-Up", ALT_UP, do_scroll_up, 0);
		add_to_sclist(MMAIN|MHELP, "M-Down", ALT_DOWN, do_scroll_down, 0);
	}
#endif
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
	add_to_sclist(MMAIN|MHELP, "M--", 0, do_scroll_up, 0);
	add_to_sclist(MMAIN|MHELP, "M-_", 0, do_scroll_up, 0);
	add_to_sclist(MMAIN|MHELP, "M-+", 0, do_scroll_down, 0);
	add_to_sclist(MMAIN|MHELP, "M-=", 0, do_scroll_down, 0);
#endif
#ifdef ENABLE_MULTIBUFFER
	add_to_sclist(MMAIN, "M-<", 0, switch_to_prev_buffer, 0);
	add_to_sclist(MMAIN, "M-,", 0, switch_to_prev_buffer, 0);
	add_to_sclist(MMAIN, "M->", 0, switch_to_next_buffer, 0);
	add_to_sclist(MMAIN, "M-.", 0, switch_to_next_buffer, 0);
#endif
	add_to_sclist(MMOST, "M-V", 0, do_verbatim_input, 0);
#ifndef NANO_TINY
	add_to_sclist(MMAIN, "M-T", 0, cut_till_eof, 0);
	add_to_sclist(MMAIN, "M-D", 0, do_wordlinechar_count, 0);
#endif
#ifdef ENABLE_JUSTIFY
	add_to_sclist(MMAIN|MWHEREIS, "M-J", 0, do_full_justify, 0);
#endif
	if (!ISSET(PRESERVE))
		add_to_sclist(MMAIN|MHELP|MBROWSER, "^L", 0, total_refresh, 0);
	else
		add_to_sclist(MMAIN|MBROWSER, "^L", 0, total_refresh, 0);
	add_to_sclist(MMAIN, "^Z", 0, do_suspend_void, 0);

#ifndef NANO_TINY
	/* Group of "Appearance" toggles. */
	add_to_sclist(MMAIN, "M-X", 0, do_toggle_void, NO_HELP);
	add_to_sclist(MMAIN, "M-C", 0, do_toggle_void, CONSTANT_SHOW);
	add_to_sclist(MMAIN, "M-S", 0, do_toggle_void, SOFTWRAP);
	add_to_sclist(MMAIN, "M-$", 0, do_toggle_void, SOFTWRAP);
#ifdef ENABLE_LINENUMBERS
	add_to_sclist(MMAIN, "M-N", 0, do_toggle_void, LINE_NUMBERS);
	add_to_sclist(MMAIN, "M-#", 0, do_toggle_void, LINE_NUMBERS);
#endif
	add_to_sclist(MMAIN, "M-P", 0, do_toggle_void, WHITESPACE_DISPLAY);
#ifdef ENABLE_COLOR
	add_to_sclist(MMAIN, "M-Y", 0, do_toggle_void, NO_COLOR_SYNTAX);
#endif

	/* Group of "Editing-behavior" toggles. */
	add_to_sclist(MMAIN, "M-H", 0, do_toggle_void, SMART_HOME);
	add_to_sclist(MMAIN, "M-I", 0, do_toggle_void, AUTOINDENT);
	add_to_sclist(MMAIN, "M-K", 0, do_toggle_void, CUT_FROM_CURSOR);
#ifdef ENABLE_WRAPPING
	add_to_sclist(MMAIN, "M-L", 0, do_toggle_void, BREAK_LONG_LINES);
#endif
	add_to_sclist(MMAIN, "M-O", 0, do_toggle_void, TABS_TO_SPACES);

	/* Group of "Peripheral-feature" toggles. */
#ifdef ENABLE_MOUSE
	add_to_sclist(MMAIN, "M-M", 0, do_toggle_void, USE_MOUSE);
#endif
	add_to_sclist(MMAIN, "M-Z", 0, do_toggle_void, SUSPEND);
#endif /* !NANO_TINY */

	add_to_sclist(((MMOST & ~MMAIN) | MYESNO), "^C", 0, do_cancel, 0);

	add_to_sclist(MWHEREIS|MREPLACE, "M-C", 0, case_sens_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "M-R", 0, regexp_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "M-B", 0, backwards_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE, "^R", 0, flip_replace, 0);
	add_to_sclist(MWHEREIS|MGOTOLINE, "^T", 0, flip_goto, 0);
#ifdef ENABLE_HISTORIES
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE|MFINDINHELP|MEXTCMD, "^P", 0, get_history_older_void, 0);
	add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE|MFINDINHELP|MEXTCMD, "^N", 0, get_history_newer_void, 0);
#ifdef ENABLE_UTF8
	if (using_utf8()) {
		add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE|MFINDINHELP|MEXTCMD, "\xE2\x96\xb2", KEY_UP, get_history_older_void, 0);
		add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE|MFINDINHELP|MEXTCMD, "\xE2\x96\xbc", KEY_DOWN, get_history_newer_void, 0);
	} else
#endif
	{
		add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE|MFINDINHELP|MEXTCMD, "Up", KEY_UP, get_history_older_void, 0);
		add_to_sclist(MWHEREIS|MREPLACE|MREPLACEWITH|MWHEREISFILE|MFINDINHELP|MEXTCMD, "Down", KEY_DOWN, get_history_newer_void, 0);
	}
#endif
#ifdef ENABLE_JUSTIFY
	add_to_sclist(MGOTOLINE, "^W", 0, do_para_begin_void, 0);
	add_to_sclist(MGOTOLINE, "^O", 0, do_para_end_void, 0);
#endif
	add_to_sclist(MGOTOLINE, "^Y", 0, to_first_line, 0);
	add_to_sclist(MGOTOLINE, "^V", 0, to_last_line, 0);
	/* Some people are used to having these keystrokes in the Search menu. */
	add_to_sclist(MWHEREIS, "^Y", 0, to_first_line, 0);
	add_to_sclist(MWHEREIS, "^V", 0, to_last_line, 0);
#ifdef ENABLE_BROWSER
	add_to_sclist(MWHEREISFILE, "^Y", 0, to_first_file, 0);
	add_to_sclist(MWHEREISFILE, "^V", 0, to_last_file, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-\\", 0, to_first_file, 0);
	add_to_sclist(MBROWSER|MWHEREISFILE, "M-/", 0, to_last_file, 0);
	add_to_sclist(MBROWSER, "Home", KEY_HOME, to_first_file, 0);
	add_to_sclist(MBROWSER, "End", KEY_END, to_last_file, 0);
	add_to_sclist(MBROWSER, "^Home", CONTROL_HOME, to_first_file, 0);
	add_to_sclist(MBROWSER, "^End", CONTROL_END, to_last_file, 0);
	add_to_sclist(MBROWSER, "^_", 0, goto_dir_void, 0);
	add_to_sclist(MBROWSER, "M-G", 0, goto_dir_void, 0);
#endif
	if (ISSET(TEMP_FILE) && !ISSET(PRESERVE))
		add_to_sclist(MWRITEFILE, "^Q", 0, discard_buffer, 0);
#ifndef NANO_TINY
	add_to_sclist(MWRITEFILE, "M-D", 0, dos_format_void, 0);
	add_to_sclist(MWRITEFILE, "M-M", 0, mac_format_void, 0);
	/* Only when not in restricted mode, allow Appending, Prepending,
	 * making backups, and executing a command. */
	if (!ISSET(RESTRICTED) && !ISSET(VIEW_MODE)) {
		add_to_sclist(MWRITEFILE, "M-A", 0, append_void, 0);
		add_to_sclist(MWRITEFILE, "M-P", 0, prepend_void, 0);
		add_to_sclist(MWRITEFILE, "M-B", 0, backup_file_void, 0);
		add_to_sclist(MINSERTFILE|MEXTCMD, "^X", 0, flip_execute, 0);
	}
	add_to_sclist(MINSERTFILE, "M-N", 0, flip_convert, 0);
#endif
#ifdef ENABLE_MULTIBUFFER
	/* Only when not in restricted mode, allow multiple buffers. */
	if (!ISSET(RESTRICTED)) {
		add_to_sclist(MINSERTFILE|MEXTCMD, "M-F", 0, flip_newbuffer, 0);
#ifndef NANO_TINY
		add_to_sclist(MEXTCMD, "M-\\", 0, flip_pipe, 0);
#endif
	}
#endif
#ifdef ENABLE_BROWSER
	/* Only when not in restricted mode, allow entering the file browser. */
	if (!ISSET(RESTRICTED))
		add_to_sclist(MWRITEFILE|MINSERTFILE, "^T", 0, to_files_void, 0);
#endif
	add_to_sclist(MHELP|MBROWSER, "^C", 0, do_exit, 0);
	/* Allow exiting from the file browser and the help viewer with
	 * the same key as they were entered. */
#ifdef ENABLE_BROWSER
	add_to_sclist(MBROWSER, "^T", 0, do_exit, 0);
#endif
#ifdef ENABLE_HELP
	add_to_sclist(MHELP, "^G", 0, do_exit, 0);
	add_to_sclist(MHELP, "F1", 0, do_exit, 0);
	add_to_sclist(MHELP, "Home", KEY_HOME, to_first_line, 0);
	add_to_sclist(MHELP, "End", KEY_END, to_last_line, 0);
#endif
#ifdef ENABLE_COLOR
	add_to_sclist(MLINTER, "^X", 0, do_cancel, 0);
#endif
	add_to_sclist(MMOST & ~MFINDINHELP, "F1", 0, do_help_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "F2", 0, do_exit, 0);
	add_to_sclist(MMAIN, "F3", 0, do_writeout_void, 0);
#ifdef ENABLE_JUSTIFY
	add_to_sclist(MMAIN, "F4", 0, do_justify_void, 0);
#endif
	add_to_sclist(MMAIN, "F5", 0, do_insertfile_void, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER, "F6", 0, do_search_forward, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "F7", 0, do_page_up, 0);
	add_to_sclist(MMAIN|MHELP|MBROWSER|MLINTER, "F8", 0, do_page_down, 0);
	add_to_sclist(MMOST, "F9", 0, cut_text, 0);
	add_to_sclist(MMOST, "F10", 0, paste_text, 0);
	add_to_sclist(MMAIN, "F11", 0, do_cursorpos_void, 0);
#ifdef ENABLE_SPELLER
	add_to_sclist(MMAIN, "F12", 0, do_spell, 0);
#endif
}

#ifndef NANO_TINY
/* Return the textual description that corresponds to the given flag. */
const char *flagtostr(int flag)
{
	switch (flag) {
		case NO_HELP:
			/* TRANSLATORS: The next fourteen strings are toggle descriptions;
			 * they are best kept shorter than 40 characters, but may be longer. */
			return N_("Help mode");
		case CONSTANT_SHOW:
			return N_("Constant cursor position display");
		case SOFTWRAP:
			return N_("Soft wrapping of overlong lines");
		case WHITESPACE_DISPLAY:
			return N_("Whitespace display");
		case NO_COLOR_SYNTAX:
			return N_("Color syntax highlighting");
		case SMART_HOME:
			return N_("Smart home key");
		case AUTOINDENT:
			return N_("Auto indent");
		case CUT_FROM_CURSOR:
			return N_("Cut to end");
		case BREAK_LONG_LINES:
			return N_("Hard wrapping of overlong lines");
		case TABS_TO_SPACES:
			return N_("Conversion of typed tabs to spaces");
		case USE_MOUSE:
			return N_("Mouse support");
		case SUSPEND:
			return N_("Suspension");
		case LINE_NUMBERS:
			return N_("Line numbering");
		default:
			return "Bad toggle -- please report a bug";
	}
}
#endif /* !NANO_TINY */

#ifdef ENABLE_NANORC
/* Interpret a function string given in the rc file, and return a
 * shortcut record with the corresponding function filled in. */
keystruct *strtosc(const char *input)
{
	keystruct *s = nmalloc(sizeof(keystruct));

#ifndef NANO_TINY
	s->toggle = 0;
#endif

#ifdef ENABLE_HELP
	if (!strcasecmp(input, "help"))
		s->func = do_help_void;
	else
#endif
	if (!strcasecmp(input, "cancel"))
		s->func = do_cancel;
	else if (!strcasecmp(input, "exit"))
		s->func = do_exit;
	else if (!strcasecmp(input, "discardbuffer"))
		s->func = discard_buffer;
	else if (!strcasecmp(input, "writeout"))
		s->func = do_writeout_void;
	else if (!strcasecmp(input, "savefile"))
		s->func = do_savefile;
	else if (!strcasecmp(input, "insert"))
		s->func = do_insertfile_void;
	else if (!strcasecmp(input, "whereis"))
		s->func = do_search_forward;
	else if (!strcasecmp(input, "wherewas"))
		s->func = do_search_backward;
	else if (!strcasecmp(input, "findprevious"))
		s->func = do_findprevious;
	else if (!strcasecmp(input, "findnext"))
		s->func = do_findnext;
	else if (!strcasecmp(input, "replace"))
		s->func = do_replace;
	else if (!strcasecmp(input, "cut"))
		s->func = cut_text;
	else if (!strcasecmp(input, "paste"))
		s->func = paste_text;
#ifndef NANO_TINY
	else if (!strcasecmp(input, "cutrestoffile"))
		s->func = cut_till_eof;
	else if (!strcasecmp(input, "copy"))
		s->func = copy_text;
	else if (!strcasecmp(input, "zap"))
		s->func = zap_text;
	else if (!strcasecmp(input, "mark"))
		s->func = do_mark;
#endif
#ifdef ENABLE_SPELLER
	else if (!strcasecmp(input, "tospell") ||
			 !strcasecmp(input, "speller"))
		s->func = do_spell;
#endif
#ifdef ENABLE_COLOR
	else if (!strcasecmp(input, "linter"))
		s->func = do_linter;
#endif
	else if (!strcasecmp(input, "curpos"))
		s->func = do_cursorpos_void;
	else if (!strcasecmp(input, "gotoline"))
		s->func = do_gotolinecolumn_void;
#ifdef ENABLE_JUSTIFY
	else if (!strcasecmp(input, "justify"))
		s->func = do_justify_void;
	else if (!strcasecmp(input, "fulljustify"))
		s->func = do_full_justify;
	else if (!strcasecmp(input, "beginpara"))
		s->func = do_para_begin_void;
	else if (!strcasecmp(input, "endpara"))
		s->func = do_para_end_void;
#endif
#ifdef ENABLE_COMMENT
	else if (!strcasecmp(input, "comment"))
		s->func = do_comment;
#endif
#ifdef ENABLE_WORDCOMPLETION
	else if (!strcasecmp(input, "complete"))
		s->func = complete_a_word;
#endif
#ifndef NANO_TINY
	else if (!strcasecmp(input, "indent"))
		s->func = do_indent;
	else if (!strcasecmp(input, "unindent"))
		s->func = do_unindent;
	else if (!strcasecmp(input, "chopwordleft") ||
			 !strcasecmp(input, "cutwordleft"))  /* Deprecated; remove in 2021. */
		s->func = chop_previous_word;
	else if (!strcasecmp(input, "chopwordright") ||
			 !strcasecmp(input, "cutwordright"))  /* Deprecated; remove in 2021. */
		s->func = chop_next_word;
	else if (!strcasecmp(input, "findbracket"))
		s->func = do_find_bracket;
	else if (!strcasecmp(input, "wordcount"))
		s->func = do_wordlinechar_count;
	else if (!strcasecmp(input, "recordmacro"))
		s->func = record_macro;
	else if (!strcasecmp(input, "runmacro"))
		s->func = run_macro;
	else if (!strcasecmp(input, "undo"))
		s->func = do_undo;
	else if (!strcasecmp(input, "redo"))
		s->func = do_redo;
#endif
	else if (!strcasecmp(input, "left") ||
			 !strcasecmp(input, "back"))
		s->func = do_left;
	else if (!strcasecmp(input, "right") ||
			 !strcasecmp(input, "forward"))
		s->func = do_right;
	else if (!strcasecmp(input, "up") ||
			 !strcasecmp(input, "prevline"))
		s->func = do_up;
	else if (!strcasecmp(input, "down") ||
			 !strcasecmp(input, "nextline"))
		s->func = do_down;
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
	else if (!strcasecmp(input, "scrollup"))
		s->func = do_scroll_up;
	else if (!strcasecmp(input, "scrolldown"))
		s->func = do_scroll_down;
#endif
	else if (!strcasecmp(input, "prevword"))
		s->func = do_prev_word_void;
	else if (!strcasecmp(input, "nextword"))
		s->func = do_next_word_void;
	else if (!strcasecmp(input, "home"))
		s->func = do_home;
	else if (!strcasecmp(input, "end"))
		s->func = do_end;
	else if (!strcasecmp(input, "prevblock"))
		s->func = do_prev_block;
	else if (!strcasecmp(input, "nextblock"))
		s->func = do_next_block;
	else if (!strcasecmp(input, "pageup") ||
			 !strcasecmp(input, "prevpage"))
		s->func = do_page_up;
	else if (!strcasecmp(input, "pagedown") ||
			 !strcasecmp(input, "nextpage"))
		s->func = do_page_down;
	else if (!strcasecmp(input, "firstline"))
		s->func = to_first_line;
	else if (!strcasecmp(input, "lastline"))
		s->func = to_last_line;
#ifdef ENABLE_MULTIBUFFER
	else if (!strcasecmp(input, "prevbuf"))
		s->func = switch_to_prev_buffer;
	else if (!strcasecmp(input, "nextbuf"))
		s->func = switch_to_next_buffer;
#endif
	else if (!strcasecmp(input, "verbatim"))
		s->func = do_verbatim_input;
	else if (!strcasecmp(input, "tab"))
		s->func = do_tab;
	else if (!strcasecmp(input, "enter"))
		s->func = do_enter;
	else if (!strcasecmp(input, "delete"))
		s->func = do_delete;
	else if (!strcasecmp(input, "backspace"))
		s->func = do_backspace;
	else if (!strcasecmp(input, "refresh"))
		s->func = total_refresh;
	else if (!strcasecmp(input, "suspend"))
		s->func = do_suspend_void;
	else if (!strcasecmp(input, "casesens"))
		s->func = case_sens_void;
	else if (!strcasecmp(input, "regexp"))
		s->func = regexp_void;
	else if (!strcasecmp(input, "backwards"))
		s->func = backwards_void;
	else if (!strcasecmp(input, "flipreplace"))
		s->func = flip_replace;
	else if (!strcasecmp(input, "flipgoto"))
		s->func = flip_goto;
#ifdef ENABLE_HISTORIES
	else if (!strcasecmp(input, "older"))
		s->func = get_history_older_void;
	else if (!strcasecmp(input, "newer"))
		s->func = get_history_newer_void;
#endif
#ifndef NANO_TINY
	else if (!strcasecmp(input, "dosformat"))
		s->func = dos_format_void;
	else if (!strcasecmp(input, "macformat"))
		s->func = mac_format_void;
	else if (!strcasecmp(input, "append"))
		s->func = append_void;
	else if (!strcasecmp(input, "prepend"))
		s->func = prepend_void;
	else if (!strcasecmp(input, "backup"))
		s->func = backup_file_void;
	else if (!strcasecmp(input, "flipexecute"))
		s->func = flip_execute;
	else if (!strcasecmp(input, "flippipe"))
		s->func = flip_pipe;
	else if (!strcasecmp(input, "flipconvert"))
		s->func = flip_convert;
#endif
#ifdef ENABLE_MULTIBUFFER
	else if (!strcasecmp(input, "flipnewbuffer"))
		s->func = flip_newbuffer;
#endif
#ifdef ENABLE_BROWSER
	else if (!strcasecmp(input, "tofiles") ||
			 !strcasecmp(input, "browser"))
		s->func = to_files_void;
	else if (!strcasecmp(input, "gotodir"))
		s->func = goto_dir_void;
	else if (!strcasecmp(input, "firstfile"))
		s->func = to_first_file;
	else if (!strcasecmp(input, "lastfile"))
		s->func = to_last_file;
#endif
	else {
#ifndef NANO_TINY
		s->func = do_toggle_void;
		if (!strcasecmp(input, "nohelp"))
			s->toggle = NO_HELP;
		else if (!strcasecmp(input, "constantshow"))
			s->toggle = CONSTANT_SHOW;
		else if (!strcasecmp(input, "softwrap"))
			s->toggle = SOFTWRAP;
#ifdef ENABLE_LINENUMBERS
		else if (!strcasecmp(input, "linenumbers"))
			s->toggle = LINE_NUMBERS;
#endif
		else if (!strcasecmp(input, "whitespacedisplay"))
			s->toggle = WHITESPACE_DISPLAY;
#ifdef ENABLE_COLOR
		else if (!strcasecmp(input, "nosyntax"))
			s->toggle = NO_COLOR_SYNTAX;
#endif
		else if (!strcasecmp(input, "smarthome"))
			s->toggle = SMART_HOME;
		else if (!strcasecmp(input, "autoindent"))
			s->toggle = AUTOINDENT;
		else if (!strcasecmp(input, "cutfromcursor"))
			s->toggle = CUT_FROM_CURSOR;
#ifdef ENABLE_WRAPPING
		else if (!strcasecmp(input, "nowrap"))
			s->toggle = BREAK_LONG_LINES;
#endif
		else if (!strcasecmp(input, "tabstospaces"))
			s->toggle = TABS_TO_SPACES;
#ifdef ENABLE_MOUSE
		else if (!strcasecmp(input, "mouse"))
			s->toggle = USE_MOUSE;
#endif
		else if (!strcasecmp(input, "suspendenable"))
			s->toggle = SUSPEND;
		else
#endif /* !NANO_TINY */
		{
			free(s);
			return NULL;
		}
	}
	return s;
}

/* Return the symbol that corresponds to the given menu name. */
int name_to_menu(const char *name)
{
	int index = -1;

	while (++index < NUMBER_OF_MENUS)
		if (strcasecmp(name, menunames[index]) == 0)
			return menusymbols[index];

	return -1;
}

/* Return the name that corresponds to the given menu symbol. */
char *menu_to_name(int menu)
{
	int index = -1;

	while (++index < NUMBER_OF_MENUS)
		if (menusymbols[index] == menu)
			return menunames[index];

	return "boooo";
}
#endif /* ENABLE_NANORC */
