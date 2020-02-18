/**************************************************************************
 *   chars.c  --  This file is part of GNU nano.                          *
 *                                                                        *
 *   Copyright (C) 2001-2011, 2013-2019 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016-2018 Benno Schulenberg                            *
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

#ifdef ENABLE_UTF8
#include <wchar.h>
#include <wctype.h>

static bool use_utf8 = FALSE;
		/* Whether we've enabled UTF-8 support. */

/* Enable UTF-8 support. */
void utf8_init(void)
{
	use_utf8 = TRUE;
}

/* Is UTF-8 support enabled? */
bool using_utf8(void)
{
	return use_utf8;
}
#endif /* ENABLE_UTF8 */

/* Concatenate two allocated strings, and free the second. */
char *addstrings(char* str1, size_t len1, char* str2, size_t len2)
{
	str1 = charealloc(str1, len1 + len2 + 1);
	str1[len1] = '\0';

	strncat(&str1[len1], str2, len2);
	free(str2);

	return str1;
}

/* Return TRUE if the value of c is in byte range, and FALSE otherwise. */
bool is_byte(int c)
{
	return ((unsigned int)c == (unsigned char)c);
}

/* This function is equivalent to isalpha() for multibyte characters. */
bool is_alpha_mbchar(const char *c)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		wchar_t wc;

		if (mbtowc(&wc, c, MAXCHARLEN) < 0)
			return FALSE;

		return iswalpha(wc);
	} else
#endif
		return isalpha((unsigned char)*c);
}

/* This function is equivalent to isalnum() for multibyte characters. */
bool is_alnum_mbchar(const char *c)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		wchar_t wc;

		if (mbtowc(&wc, c, MAXCHARLEN) < 0)
			return FALSE;

		return iswalnum(wc);
	} else
#endif
		return isalnum((unsigned char)*c);
}

/* This function is equivalent to isblank() for multibyte characters. */
bool is_blank_mbchar(const char *c)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		wchar_t wc;

		if (mbtowc(&wc, c, MAXCHARLEN) < 0)
			return FALSE;

		return iswblank(wc);
	} else
#endif
		return isblank((unsigned char)*c);
}

/* This function is equivalent to iscntrl(), except in that it only
 * handles non-high-bit control characters. */
bool is_ascii_cntrl_char(int c)
{
	return (0 <= c && c < 32);
}

/* This function is equivalent to iscntrl() for multibyte characters,
 * except in that it also handles multibyte control characters with
 * their high bits set. */
bool is_cntrl_mbchar(const char *c)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		return ((c[0] & 0xE0) == 0 || c[0] == 127 ||
				((signed char)c[0] == -62 && (signed char)c[1] < -96));
	} else
#endif
		return (((unsigned char)*c & 0x60) == 0 || (unsigned char)*c == 127);
}

/* This function is equivalent to ispunct() for multibyte characters. */
bool is_punct_mbchar(const char *c)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		wchar_t wc;

		if (mbtowc(&wc, c, MAXCHARLEN) < 0)
			return FALSE;

		return iswpunct(wc);
	} else
#endif
		return ispunct((unsigned char)*c);
}

/* Return TRUE when the given multibyte character c is a word-forming
 * character (that is: alphanumeric, or specified in wordchars, or
 * punctuation when allow_punct is TRUE), and FALSE otherwise. */
bool is_word_mbchar(const char *c, bool allow_punct)
{
	if (*c == '\0')
		return FALSE;

	if (is_alnum_mbchar(c))
		return TRUE;

	if (word_chars != NULL && *word_chars != '\0') {
		char symbol[MAXCHARLEN + 1];
		int symlen = parse_mbchar(c, symbol, NULL);

		symbol[symlen] = '\0';
		return (strstr(word_chars, symbol) != NULL);
	}

	return (allow_punct && is_punct_mbchar(c));
}

/* Return the visible representation of control character c. */
char control_rep(const signed char c)
{
	if (c == DEL_CODE)
		return '?';
	else if (c == -97)
		return '=';
	else if (c < 0)
		return c + 224;
	else
		return c + 64;
}

/* Return the visible representation of multibyte control character c. */
char control_mbrep(const char *c, bool isdata)
{
	/* An embedded newline is an encoded NUL if it is data. */
	if (*c == '\n' && (isdata || as_an_at))
		return '@';

#ifdef ENABLE_UTF8
	if (use_utf8) {
		if ((unsigned char)c[0] < 128)
			return control_rep(c[0]);
		else
			return control_rep(c[1]);
	} else
#endif
		return control_rep(*c);
}

/* This function is equivalent to wcwidth() for multibyte characters. */
int mbwidth(const char *c)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		wchar_t wc;
		int width;

		if (mbtowc(&wc, c, MAXCHARLEN) < 0)
			return 1;

		width = wcwidth(wc);

		if (width == -1)
			return 1;

		return width;
	} else
#endif
		return 1;
}

/* Convert the Unicode value in chr to a multibyte character, if possible.
 * If the conversion succeeds, return the (dynamically allocated) multibyte
 * character and its length.  Otherwise, return an undefined (dynamically
 * allocated) multibyte character and a length of zero. */
char *make_mbchar(long chr, int *chr_mb_len)
{
	char *chr_mb;

#ifdef ENABLE_UTF8
	if (use_utf8) {
		chr_mb = charalloc(MAXCHARLEN);
		*chr_mb_len = wctomb(chr_mb, (wchar_t)chr);

		/* Reject invalid Unicode characters. */
		if (*chr_mb_len < 0 || !is_valid_unicode((wchar_t)chr)) {
			IGNORE_CALL_RESULT(wctomb(NULL, 0));
			*chr_mb_len = 0;
		}
	} else
#endif
	{
		*chr_mb_len = 1;
		chr_mb = mallocstrncpy(NULL, (char *)&chr, 1);
	}

	return chr_mb;
}

/* Return the length (in bytes) of the character located at *pointer. */
int char_length(const char *pointer)
{
#ifdef ENABLE_UTF8
	/* If possibly a multibyte character, get its length; otherwise, it's 1. */
	if ((signed char)*pointer < 0) {
		int length = mblen(pointer, MAXCHARLEN);

		return (length < 0 ? 1 : length);
	} else
#endif
		return 1;
}

/* Parse a multibyte character from buf.  Return the number of bytes
 * used.  If chr isn't NULL, store the multibyte character in it.  If
 * col isn't NULL, add the character's width (in columns) to it. */
int parse_mbchar(const char *buf, char *chr, size_t *col)
{
	int length;

#ifdef ENABLE_UTF8
	/* If this is a UTF-8 starter byte, get the number of bytes of the character. */
	if ((signed char)*buf < 0) {
		length = mblen(buf, MAXCHARLEN);

		/* When the multibyte sequence is invalid, only take the first byte. */
		if (length <= 0)
			length = 1;
	} else
#endif
		length = 1;

	/* When requested, store the multibyte character in chr. */
	if (chr != NULL)
		for (int i = 0; i < length; i++)
			chr[i] = buf[i];

	/* When requested, add the width of the character to col. */
	if (col != NULL) {
		/* If we have a tab, compute its width in columns based on the
		 * current value of col. */
		if (*buf == '\t')
			*col += tabsize - *col % tabsize;
		/* If we have a control character, it's two columns wide: one
		 * column for the "^", and one for the visible character. */
		else if (is_cntrl_mbchar(buf))
			*col += 2;
		/* If we have a normal character, get its width normally. */
		else if (length == 1)
			*col += 1;
#ifdef ENABLE_UTF8
		else
			*col += mbwidth(buf);
#endif
	}

	return length;
}

/* Return the index in buf of the beginning of the multibyte character
 * before the one at pos. */
size_t step_left(const char *buf, size_t pos)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		size_t before, charlen = 0;

		if (pos < 4)
			before = 0;
		else {
			const char *ptr = buf + pos;

			/* Probe for a valid starter byte in the preceding four bytes. */
			if ((signed char)*(--ptr) > -65)
				before = pos - 1;
			else if ((signed char)*(--ptr) > -65)
				before = pos - 2;
			else if ((signed char)*(--ptr) > -65)
				before = pos - 3;
			else if ((signed char)*(--ptr) > -65)
				before = pos - 4;
			else
				before = pos - 1;
		}

		/* Move forward again until we reach the original character,
		 * so we know the length of its preceding character. */
		while (before < pos) {
			charlen = char_length(buf + before);
			before += charlen;
		}

		return before - charlen;
	} else
#endif
	return (pos == 0 ? 0 : pos - 1);
}

/* Return the index in buf of the beginning of the multibyte character
 * after the one at pos. */
size_t step_right(const char *buf, size_t pos)
{
	return pos + char_length(buf + pos);
}

/* This function is equivalent to strcasecmp() for multibyte strings. */
int mbstrcasecmp(const char *s1, const char *s2)
{
	return mbstrncasecmp(s1, s2, HIGHEST_POSITIVE);
}

/* This function is equivalent to strncasecmp() for multibyte strings. */
int mbstrncasecmp(const char *s1, const char *s2, size_t n)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		wchar_t wc1, wc2;

		while (*s1 != '\0' && *s2 != '\0' && n > 0) {
			bool bad1 = (mbtowc(&wc1, s1, MAXCHARLEN) < 0);
			bool bad2 = (mbtowc(&wc2, s2, MAXCHARLEN) < 0);

			if (bad1 || bad2) {
				if (*s1 != *s2)
					return (unsigned char)*s1 - (unsigned char)*s2;

				if (bad1 != bad2)
					return (bad1 ? 1 : -1);
			} else {
				int difference = towlower(wc1) - towlower(wc2);

				if (difference != 0)
					return difference;
			}

			s1 += char_length(s1);
			s2 += char_length(s2);
			n--;
		}

		return (n > 0) ? ((unsigned char)*s1 - (unsigned char)*s2) : 0;
	} else
#endif
		return strncasecmp(s1, s2, n);
}

/* This function is equivalent to strcasestr() for multibyte strings. */
char *mbstrcasestr(const char *haystack, const char *needle)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		size_t needle_len = mbstrlen(needle);

		while (*haystack != '\0') {
			if (mbstrncasecmp(haystack, needle, needle_len) == 0)
				return (char *)haystack;

			haystack += char_length(haystack);
		}

		return NULL;
	} else
#endif
		return (char *)strcasestr(haystack, needle);
}

/* This function is equivalent to strstr(), except in that it scans the
 * string in reverse, starting at pointer. */
char *revstrstr(const char *haystack, const char *needle,
		const char *pointer)
{
	size_t needle_len = strlen(needle);
	size_t tail_len = strlen(pointer);

	if (tail_len < needle_len)
		pointer += tail_len - needle_len;

	while (pointer >= haystack) {
		if (strncmp(pointer, needle, needle_len) == 0)
			return (char *)pointer;
		pointer--;
	}

	return NULL;
}

/* This function is equivalent to strcasestr(), except in that it scans
 * the string in reverse, starting at pointer. */
char *revstrcasestr(const char *haystack, const char *needle,
		const char *pointer)
{
	size_t needle_len = strlen(needle);
	size_t tail_len = strlen(pointer);

	if (tail_len < needle_len)
		pointer += tail_len - needle_len;

	while (pointer >= haystack) {
		if (strncasecmp(pointer, needle, needle_len) == 0)
			return (char *)pointer;
		pointer--;
	}

	return NULL;
}

/* This function is equivalent to strcasestr() for multibyte strings,
 * except in that it scans the string in reverse, starting at pointer. */
char *mbrevstrcasestr(const char *haystack, const char *needle,
		const char *pointer)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		size_t needle_len = mbstrlen(needle);
		size_t tail_len = mbstrlen(pointer);

		if (tail_len < needle_len)
			pointer += tail_len - needle_len;

		if (pointer < haystack)
			return NULL;

		while (TRUE) {
			if (mbstrncasecmp(pointer, needle, needle_len) == 0)
				return (char *)pointer;

			if (pointer == haystack)
				return NULL;

			pointer = haystack + step_left(haystack, pointer - haystack);
		}
	} else
#endif
		return revstrcasestr(haystack, needle, pointer);
}

/* Count the number of (multibyte) characters in the given string. */
size_t mbstrlen(const char *pointer)
{
	size_t count = 0;

	while (*pointer != '\0') {
#ifdef ENABLE_UTF8
		if ((signed char)*pointer < 0) {
			int length = mblen(pointer, MAXCHARLEN);

			pointer += (length < 0 ? 1 : length);
		} else
#endif
			pointer++;

		count++;
	}

	return count;
}

#if !defined(NANO_TINY) || defined(ENABLE_JUSTIFY)
/* This function is equivalent to strchr() for multibyte strings. */
char *mbstrchr(const char *string, const char *chr)
{
#ifdef ENABLE_UTF8
	if (use_utf8) {
		bool bad_s = FALSE, bad_c = FALSE;
		wchar_t ws, wc;

		if (mbtowc(&wc, chr, MAXCHARLEN) < 0) {
			wc = (unsigned char)*chr;
			bad_c = TRUE;
		}

		while (*string != '\0') {
			int symlen = mbtowc(&ws, string, MAXCHARLEN);

			if (symlen < 0) {
				ws = (unsigned char)*string;
				bad_s = TRUE;
			}

			if (ws == wc && bad_s == bad_c)
				break;

			string += symlen;
		}

		if (*string == '\0')
			return NULL;

		return (char *)string;
	} else
#endif
		return strchr(string, *chr);
}
#endif /* !NANO_TINY || ENABLE_JUSTIFY */

#ifndef NANO_TINY
/* Locate, in the given string, the first occurrence of any of
 * the characters in accept, searching forward. */
char *mbstrpbrk(const char *string, const char *accept)
{
	while (*string != '\0') {
		if (mbstrchr(accept, string) != NULL)
			return (char *)string;

		string += char_length(string);
	}

	return NULL;
}

/* Locate, in the string that starts at head, the first occurrence of any of
 * the characters in accept, starting from pointer and searching backwards. */
char *mbrevstrpbrk(const char *head, const char *accept, const char *pointer)
{
	if (*pointer == '\0') {
		if (pointer == head)
			return NULL;
		pointer = head + step_left(head, pointer - head);
	}

	while (TRUE) {
		if (mbstrchr(accept, pointer) != NULL)
			return (char *)pointer;

		/* If we've reached the head of the string, we found nothing. */
		if (pointer == head)
			return NULL;

		pointer = head + step_left(head, pointer - head);
	}
}
#endif /* !NANO_TINY */

#if defined(ENABLE_NANORC) && (!defined(NANO_TINY) || defined(ENABLE_JUSTIFY))
/* Return TRUE if the given string contains at least one blank character,
 * and FALSE otherwise. */
bool has_blank_char(const char *string)
{
	char symbol[MAXCHARLEN];

	while (*string != '\0') {
		string += parse_mbchar(string, symbol, NULL);

		if (is_blank_mbchar(symbol))
			return TRUE;
	}

	return FALSE;
}
#endif /* ENABLE_NANORC && (!NANO_TINY || ENABLE_JUSTIFY) */

/* Return TRUE when the given string is empty or consists of only blanks. */
bool white_string(const char *string)
{
	while (*string != '\0' && (is_blank_mbchar(string) || *string == '\r'))
		string += char_length(string);

	return !*string;
}

#ifdef ENABLE_UTF8
/* Return TRUE if wc is valid Unicode, and FALSE otherwise. */
bool is_valid_unicode(wchar_t wc)
{
	return ((0 <= wc && wc <= 0xD7FF) ||
				(0xE000 <= wc && wc <= 0xFDCF) ||
				(0xFDF0 <= wc && wc <= 0xFFFD) ||
				(0xFFFF < wc && wc <= 0x10FFFF && (wc & 0xFFFF) <= 0xFFFD));
}
#endif
