/**************************************************************************
 *   proto.h  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 1999-2011, 2013-2020 Free Software Foundation, Inc.    *
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

#include "nano.h"

/* All external variables.  See global.c for their descriptions. */

#ifndef NANO_TINY
extern volatile sig_atomic_t the_window_resized;
#endif

extern bool on_a_vt;
extern bool shifted_metas;

extern bool meta_key;
extern bool shift_held;
extern bool mute_modifiers;
extern bool bracketed_paste;

extern bool started_curses;
extern bool we_are_running;
extern bool more_than_one;

extern bool inhelp;
extern char *title;

extern bool focusing;

extern bool as_an_at;

extern bool control_C_was_pressed;

extern bool suppress_cursorpos;

extern message_type lastmessage;

extern linestruct *pletion_line;

extern bool also_the_last;

extern char *answer;

extern char *last_search;
extern int didfind;

extern char *present_path;

extern unsigned flags[4];

extern int controlleft, controlright;
extern int controlup, controldown;
extern int controlhome, controlend;
#ifndef NANO_TINY
extern int controldelete, controlshiftdelete;
extern int shiftleft, shiftright;
extern int shiftup, shiftdown;
extern int shiftcontrolleft, shiftcontrolright;
extern int shiftcontrolup, shiftcontroldown;
extern int shiftcontrolhome, shiftcontrolend;
extern int altleft, altright;
extern int altup, altdown;
extern int altdelete;
extern int shiftaltleft, shiftaltright;
extern int shiftaltup, shiftaltdown;
#endif

#ifdef ENABLED_WRAPORJUSTIFY
extern ssize_t fill;
extern size_t wrap_at;
#endif

extern WINDOW *topwin;
extern WINDOW *edit;
extern WINDOW *bottomwin;
extern int editwinrows;
extern int editwincols;
extern int margin;
#ifndef NANO_TINY
extern ssize_t stripe_column;
#endif

extern linestruct *cutbuffer;
extern linestruct *cutbottom;
extern bool keep_cutbuffer;

extern openfilestruct *openfile;
#ifdef ENABLE_MULTIBUFFER
extern openfilestruct *startfile;
#endif

#ifndef NANO_TINY
extern char *matchbrackets;
extern char *whitespace;
extern int whitelen[2];
#endif

extern const char *exit_tag;
extern const char *close_tag;
#ifdef ENABLE_JUSTIFY
extern char *punct;
extern char *brackets;
extern char *quotestr;
extern regex_t quotereg;
#endif

extern char *word_chars;

extern ssize_t tabsize;

#ifndef NANO_TINY
extern char *backup_dir;
#endif
#ifdef ENABLE_OPERATINGDIR
extern char *operating_dir;
#endif

#ifdef ENABLE_SPELLER
extern char *alt_speller;
#endif

#ifdef ENABLE_COLOR
extern syntaxtype *syntaxes;
extern char *syntaxstr;
extern bool have_palette;
#endif

extern bool refresh_needed;

extern int currmenu;
extern keystruct *sclist;
extern funcstruct *allfuncs;
extern funcstruct *exitfunc;

extern linestruct *search_history;
extern linestruct *replace_history;
extern linestruct *execute_history;
#ifdef ENABLE_HISTORIES
extern linestruct *searchtop;
extern linestruct *searchbot;
extern linestruct *replacetop;
extern linestruct *replacebot;
extern linestruct *executetop;
extern linestruct *executebot;
#endif

extern regex_t search_regexp;
extern regmatch_t regmatches[10];

extern int hilite_attribute;
#ifdef ENABLE_COLOR
extern colortype *color_combo[NUMBER_OF_ELEMENTS];
#endif
extern int interface_color_pair[NUMBER_OF_ELEMENTS];

extern char *homedir;
extern char *statedir;
#if defined(ENABLE_NANORC) || defined(ENABLE_HISTORIES)
extern char *startup_problem;
#endif
#ifdef ENABLE_NANORC
extern char *custom_nanorc;
#endif

extern bool spotlighted;
extern size_t light_from_col;
extern size_t light_to_col;

typedef void (*functionptrtype)(void);

/* Most functions in browser.c. */
#ifdef ENABLE_BROWSER
char *do_browse_from(const char *inpath);
void read_the_list(const char *path, DIR *dir);
functionptrtype parse_browser_input(int *kbinput);
void browser_refresh(void);
void browser_select_dirname(const char *needle);
void do_filesearch(bool forwards);
void do_fileresearch(bool forwards);
void to_first_file(void);
void to_last_file(void);
char *strip_last_component(const char *path);
#endif

/* Most functions in chars.c. */
#ifdef ENABLE_UTF8
void utf8_init(void);
bool using_utf8(void);
#endif
char *addstrings(char* str1, size_t len1, char* str2, size_t len2);
bool is_byte(int c);
bool is_alpha_mbchar(const char *c);
bool is_blank_mbchar(const char *c);
bool is_ascii_cntrl_char(int c);
bool is_cntrl_mbchar(const char *c);
bool is_word_mbchar(const char *c, bool allow_punct);
char control_mbrep(const char *c, bool isdata);
#ifdef ENABLE_UTF8
int mbwidth(const char *c);
#endif
char *make_mbchar(long chr, int *chr_mb_len);
int char_length(const char *pointer);
size_t mbstrlen(const char *s);
int collect_char(const char *buf, char *chr);
int advance_over(const char *string, size_t *column);
size_t step_left(const char *buf, size_t pos);
size_t step_right(const char *buf, size_t pos);
int mbstrcasecmp(const char *s1, const char *s2);
int mbstrncasecmp(const char *s1, const char *s2, size_t n);
char *mbstrcasestr(const char *haystack, const char *needle);
char *revstrstr(const char *haystack, const char *needle, const char *index);
char *mbrevstrcasestr(const char *haystack, const char *needle, const char *index);
#if !defined(NANO_TINY) || defined(ENABLE_JUSTIFY)
char *mbstrchr(const char *s, const char *c);
#endif
#ifndef NANO_TINY
char *mbstrpbrk(const char *s, const char *accept);
char *revstrpbrk(const char *head, const char *accept, const char *index);
char *mbrevstrpbrk(const char *head, const char *accept, const char *index);
#endif
#if defined(ENABLE_NANORC) && (!defined(NANO_TINY) || defined(ENABLE_JUSTIFY))
bool has_blank_char(const char *s);
#endif
bool white_string(const char *s);
#ifdef ENABLE_UTF8
bool is_valid_unicode(wchar_t wc);
#endif

/* Most functions in color.c. */
#ifdef ENABLE_COLOR
void set_colorpairs(void);
void color_init(void);
void color_update(void);
void set_up_multicache(linestruct *line);
void check_the_multis(linestruct *line);
void precalc_multicolorinfo(void);
#endif

/* Most functions in cut.c. */
void do_delete(void);
void do_backspace(void);
#ifndef NANO_TINY
void chop_previous_word(void);
void chop_next_word(void);
#endif
void extract_segment(linestruct *top, size_t top_x,
						linestruct *bot, size_t bot_x);
void ingraft_buffer(linestruct *somebuffer);
void copy_from_buffer(linestruct *somebuffer);
#ifndef NANO_TINY
void cut_marked(bool *right_side_up);
#endif
void do_snip(bool copying, bool marked, bool until_eof, bool append);
bool is_cuttable(bool test_cliff);
void cut_text(void);
#ifndef NANO_TINY
void copy_text(void);
void cut_till_eof(void);
void zap_text(void);
#endif
void paste_text(void);

/* Most functions in files.c. */
void make_new_buffer(void);
#ifndef NANO_TINY
int delete_lockfile(const char *lockfilename);
#endif
bool open_buffer(const char *filename, bool new_buffer);
#ifdef ENABLE_SPELLER
bool replace_buffer(const char *filename, undo_type action, bool marked,
		const char *operation);
#endif
void set_modified(void);
void prepare_for_display(void);
#ifdef ENABLE_MULTIBUFFER
void mention_name_and_linecount(void);
void switch_to_prev_buffer(void);
void switch_to_next_buffer(void);
void close_buffer(void);
#endif
void read_file(FILE *f, int fd, const char *filename, bool undoable);
int open_file(const char *filename, bool newfie, FILE **f);
char *get_next_filename(const char *name, const char *suffix);
void do_insertfile_void(void);
char *get_full_path(const char *origpath);
char *safe_tempfile(FILE **f);
#ifdef ENABLE_OPERATINGDIR
void init_operating_dir(void);
bool outside_of_confinement(const char *currpath, bool allow_tabcomp);
#endif
#ifndef NANO_TINY
void init_backup_dir(void);
#endif
int copy_file(FILE *inn, FILE *out, bool close_out);
bool write_file(const char *name, FILE *f_open, bool tmp,
		kind_of_writing_type method, bool fullbuffer);
#ifndef NANO_TINY
bool write_marked_file(const char *name, FILE *f_open, bool tmp,
		kind_of_writing_type method);
#endif
int do_writeout(bool exiting, bool withprompt);
void do_writeout_void(void);
void do_savefile(void);
char *real_dir_from_tilde(const char *buf);
#if defined(ENABLE_TABCOMP) || defined(ENABLE_BROWSER)
int diralphasort(const void *va, const void *vb);
#endif
#ifdef ENABLE_TABCOMP
char *input_tab(char *buf, bool allow_files, size_t *place,
		bool *lastwastab, void (*refresh_func)(void), bool *listed);
#endif

/* Some functions in global.c. */
const keystruct *first_sc_for(int menu, void (*func)(void));
int the_code_for(void (*func)(void), int defaultval);
size_t shown_entries_for(int menu);
const keystruct *get_shortcut(int *kbinput);
functionptrtype func_from_key(int *kbinput);
#if defined(ENABLE_BROWSER) || defined(ENABLE_HELP)
functionptrtype interpret(int *keycode);
#endif
int keycode_from_string(const char *keystring);
void shortcut_init(void);
const char *flagtostr(int flag);

/* All functions in help.c. */
#ifdef ENABLE_HELP
void wrap_help_text_into_buffer(void);
void help_init(void);
functionptrtype parse_help_input(int *kbinput);
#endif
void do_help(void);

/* Most functions in history.c. */
#ifdef ENABLE_HISTORIES
void history_init(void);
void history_reset(const linestruct *h);
void update_history(linestruct **h, const char *s);
char *get_history_older(linestruct **h);
char *get_history_newer(linestruct **h);
void get_history_older_void(void);
void get_history_newer_void(void);
#ifdef ENABLE_TABCOMP
char *get_history_completion(linestruct **h, char *s, size_t len);
#endif
bool have_statedir(void);
void load_history(void);
void save_history(void);
void load_poshistory(void);
void update_poshistory(char *filename, ssize_t lineno, ssize_t xpos);
bool has_old_position(const char *file, ssize_t *line, ssize_t *column);
#endif

/* Most functions in move.c. */
void to_first_line(void);
void to_last_line(void);
void do_page_up(void);
void do_page_down(void);
#ifdef ENABLE_JUSTIFY
void do_para_begin(linestruct **line);
void do_para_end(linestruct **line);
void do_para_begin_void(void);
void do_para_end_void(void);
#endif
void do_prev_block(void);
void do_next_block(void);
void do_prev_word(bool allow_punct);
bool do_next_word(bool after_ends, bool allow_punct);
void do_prev_word_void(void);
void do_next_word_void(void);
void do_home(void);
void do_end(void);
void do_up(void);
void do_down(void);
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
void do_scroll_up(void);
void do_scroll_down(void);
#endif
void do_left(void);
void do_right(void);

/* Most functions in nano.c. */
linestruct *make_new_node(linestruct *prevnode);
void splice_node(linestruct *afterthis, linestruct *newnode);
void unlink_node(linestruct *fileptr);
void delete_node(linestruct *fileptr);
linestruct *copy_buffer(const linestruct *src);
void free_lines(linestruct *src);
void renumber_from(linestruct *line);
void partition_buffer(linestruct *top, size_t top_x,
						linestruct *bot, size_t bot_x);
void unpartition_buffer(void);
void print_view_warning(void);
bool in_restricted_mode(void);
#ifndef ENABLE_HELP
void say_there_is_no_help(void);
#endif
void finish(void);
void die(const char *msg, ...);
void emergency_save(const char *die_filename, struct stat *die_stat);
void window_init(void);
void do_exit(void);
void close_and_go(void);
void install_handler_for_Ctrl_C(void);
void restore_handler_for_Ctrl_C(void);
void reconnect_and_store_state(void);
RETSIGTYPE handle_hupterm(int signal);
#ifndef DEBUG
RETSIGTYPE handle_crash(int signal);
#endif
RETSIGTYPE do_suspend(int signal);
RETSIGTYPE do_continue(int signal);
#if !defined(NANO_TINY) || defined(ENABLE_SPELLER)
void block_sigwinch(bool blockit);
#endif
#ifndef NANO_TINY
RETSIGTYPE handle_sigwinch(int signal);
void regenerate_screen(void);
void do_toggle(int flag);
#endif
void disable_kb_interrupt(void);
void enable_kb_interrupt(void);
void disable_flow_control(void);
void enable_flow_control(void);
void terminal_init(void);
#ifdef ENABLE_LINENUMBERS
void confirm_margin(void);
#endif
void unbound_key(int code);
bool okay_for_view(const keystruct *shortcut);
void inject(char *output, size_t output_len, bool filtering);

/* Most functions in prompt.c. */
void inject_into_answer(int *the_input, size_t input_len, bool filtering);
void do_statusbar_home(void);
void do_statusbar_end(void);
void do_statusbar_left(void);
void do_statusbar_right(void);
void do_statusbar_backspace(void);
void do_statusbar_delete(void);
void do_statusbar_cut_text(void);
void do_statusbar_uncut_text(void);
#ifndef NANO_TINY
void do_statusbar_prev_word(void);
void do_statusbar_next_word(void);
#endif
void do_statusbar_verbatim_input(void);
size_t get_statusbar_page_start(size_t start_col, size_t column);
void put_cursor_at_end_of_answer(void);
void add_or_remove_pipe_symbol_from_answer(void);
int do_prompt(bool allow_tabs, bool allow_files,
		int menu, const char *curranswer, linestruct **history_list,
		void (*refresh_func)(void), const char *msg, ...);
int do_yesno_prompt(bool all, const char *msg);

/* Most functions in rcfile.c. */
#if defined(ENABLE_NANORC) || defined(ENABLE_HISTORIES)
void display_rcfile_errors(void);
void jot_error(const char *msg, ...);
#endif
#ifdef ENABLE_NANORC
#ifdef ENABLE_COLOR
void parse_one_include(char *file, syntaxtype *syntax);
void grab_and_store(const char *kind, char *ptr, regexlisttype **storage);
bool parse_syntax_commands(char *keyword, char *ptr);
#endif
void parse_rcfile(FILE *rcstream, bool just_syntax, bool intros_only);
void do_rcfiles(void);
#endif /* ENABLE_NANORC */

/* Most functions in search.c. */
bool regexp_init(const char *regexp);
void tidy_up_after_search(void);
int findnextstr(const char *needle, bool whole_word_only, int modus,
		size_t *match_len, bool skipone, const linestruct *begin, size_t begin_x);
void do_search(void);
void do_search_forward(void);
void do_search_backward(void);
void do_findprevious(void);
void do_findnext(void);
void not_found_msg(const char *str);
void go_looking(void);
ssize_t do_replace_loop(const char *needle, bool whole_word_only,
		const linestruct *real_current, size_t *real_current_x);
void do_replace(void);
void ask_for_replacement(void);
void goto_line_posx(ssize_t line, size_t pos_x);
void do_gotolinecolumn(ssize_t line, ssize_t column, bool use_answer,
		bool interactive);
void do_gotolinecolumn_void(void);
#ifndef NANO_TINY
void do_find_bracket(void);
#endif

/* Most functions in text.c. */
#ifndef NANO_TINY
void do_mark(void);
#endif
void do_tab(void);
#ifndef NANO_TINY
void do_indent(void);
void do_unindent(void);
#endif
#ifdef ENABLE_COMMENT
void do_comment(void);
#endif
void do_undo(void);
void do_redo(void);
void do_enter(void);
#ifndef NANO_TINY
RETSIGTYPE cancel_command(int signal);
bool execute_command(const char *command);
void discard_until(const undostruct *thisitem, openfilestruct *thefile, bool keep);
void add_undo(undo_type action, const char *message);
void update_multiline_undo(ssize_t lineno, char *indentation);
void update_undo(undo_type action);
#endif /* !NANO_TINY */
#ifdef ENABLE_WRAPPING
bool do_wrap(void);
#endif
#if defined(ENABLE_HELP) || defined(ENABLED_WRAPORJUSTIFY)
ssize_t break_line(const char *line, ssize_t goal, bool snap_at_nl);
#endif
#if !defined(NANO_TINY) || defined(ENABLED_WRAPORJUSTIFY)
size_t indent_length(const char *line);
#endif
#ifdef ENABLE_JUSTIFY
size_t quote_length(const char *line);
bool begpar(const linestruct *const foo, int depth);
bool inpar(const linestruct *const foo);
void do_justify(bool full_justify);
void do_justify_void(void);
void do_full_justify(void);
#endif
#ifdef ENABLE_SPELLER
void do_spell(void);
#endif
#ifdef ENABLE_COLOR
void do_linter(void);
void do_formatter(void);
#endif
#ifndef NANO_TINY
void do_wordlinechar_count(void);
#endif
void do_verbatim_input(void);
void complete_a_word(void);

/* All functions in utils.c. */
void get_homedir(void);
const char *tail(const char *path);
char *concatenate(const char *path, const char *name);
#ifdef ENABLE_LINENUMBERS
int digits(ssize_t n);
#endif
bool parse_num(const char *str, ssize_t *val);
bool parse_line_column(const char *str, ssize_t *line, ssize_t *column);
void unsunder(char *string, size_t length);
void sunder(char *string);
#if !defined(ENABLE_TINY) || defined(ENABLE_TABCOMP) || defined(ENABLE_BROWSER)
void free_chararray(char **array, size_t len);
#endif
#ifdef ENABLE_SPELLER
bool is_separate_word(size_t position, size_t length, const char *buf);
#endif
const char *strstrwrapper(const char *haystack, const char *needle,
		const char *start);
void *nmalloc(size_t howmuch);
void *nrealloc(void *ptr, size_t howmuch);
char *measured_copy(const char *src, size_t n);
char *mallocstrcpy(char *dest, const char *src);
char *copy_of(const char *string);
char *free_and_assign(char *dest, char *src);
size_t get_page_start(size_t column);
size_t xplustabs(void);
size_t actual_x(const char *text, size_t column);
size_t wideness(const char *text, size_t maxlen);
size_t breadth(const char *text);
void new_magicline(void);
#if !defined(NANO_TINY) || defined(ENABLE_HELP)
void remove_magicline(void);
#endif
#ifndef NANO_TINY
void get_region(const linestruct **top, size_t *top_x,
		const linestruct **bot, size_t *bot_x, bool *right_side_up);
void get_range(const linestruct **top, const linestruct **bot);
#endif
size_t get_totsize(const linestruct *begin, const linestruct *end);
#ifndef NANO_TINY
linestruct *line_from_number(ssize_t lineno);
#endif

/* Most functions in winio.c. */
void record_macro(void);
void run_macro(void);
size_t get_key_buffer_len(void);
#ifdef ENABLE_NANORC
void implant(const char *string);
#endif
int parse_kbinput(WINDOW *win);
int get_kbinput(WINDOW *win, bool showcursor);
int get_byte_kbinput(int kbinput);
int get_control_kbinput(int kbinput);
int *get_verbatim_kbinput(WINDOW *win, size_t *kbinput_len);
int *parse_verbatim_kbinput(WINDOW *win, size_t *count);
#ifdef ENABLE_MOUSE
int get_mouseinput(int *mouse_row, int *mouse_col, bool allow_shortcuts);
#endif
void blank_edit(void);
void blank_statusbar(void);
void wipe_statusbar(void);
void blank_bottombars(void);
void check_statusblank(void);
char *display_string(const char *buf, size_t column, size_t span,
						bool isdata, bool isprompt);
void titlebar(const char *path);
void statusline(message_type importance, const char *msg, ...);
void statusbar(const char *msg);
void warn_and_shortly_pause(const char *msg);
void bottombars(int menu);
void post_one_key(const char *keystroke, const char *tag, int width);
void place_the_cursor(void);
int update_line(linestruct *fileptr, size_t index);
#ifndef NANO_TINY
int update_softwrapped_line(linestruct *fileptr);
#endif
bool line_needs_update(const size_t old_column, const size_t new_column);
int go_back_chunks(int nrows, linestruct **line, size_t *leftedge);
int go_forward_chunks(int nrows, linestruct **line, size_t *leftedge);
bool less_than_a_screenful(size_t was_lineno, size_t was_leftedge);
void edit_scroll(bool direction);
#ifndef NANO_TINY
size_t get_softwrap_breakpoint(const char *text, size_t leftedge,
								bool *end_of_line);
size_t get_chunk_and_edge(size_t column, linestruct *line, size_t *leftedge);
size_t chunk_for(size_t column, linestruct *line);
size_t leftedge_for(size_t column, linestruct *line);
size_t number_of_chunks_in(linestruct *line);
void ensure_firstcolumn_is_aligned(void);
#endif
size_t actual_last_column(size_t leftedge, size_t column);
void edit_redraw(linestruct *old_current, update_type manner);
void edit_refresh(void);
void adjust_viewport(update_type location);
void total_redraw(void);
void total_refresh(void);
void do_cursorpos(bool force);
void do_cursorpos_void(void);
void spotlight(size_t from_col, size_t to_col);
#ifndef NANO_TINY
void spotlight_softwrapped(size_t from_col, size_t to_col);
#endif
void do_suspend_void(void);
void disable_waiting(void);
void enable_waiting(void);
#ifdef ENABLE_EXTRA
void do_credits(void);
#endif

/* These are just name definitions. */
void case_sens_void(void);
void regexp_void(void);
void backwards_void(void);
void flip_replace(void);
void flip_goto(void);
#ifdef ENABLE_BROWSER
void to_files(void);
void goto_dir(void);
#endif
#ifndef NANO_TINY
void do_nothing(void);
void do_toggle_void(void);
void dos_format_void(void);
void mac_format_void(void);
void append_void(void);
void prepend_void(void);
void backup_file_void(void);
void flip_execute(void);
void flip_pipe(void);
void flip_convert(void);
#endif
#ifdef ENABLE_MULTIBUFFER
void flip_newbuffer(void);
#endif
void discard_buffer(void);
void do_cancel(void);
