/* Convert wide character to multibyte character.
   Copyright (C) 2008-2019 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2008.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <wchar.h>

#include <errno.h>
#include <stdlib.h>


size_t
wcrtomb (char *s, wchar_t wc, mbstate_t *ps)
{
  /* This implementation of wcrtomb supports only stateless encodings.
     ps must be in the initial state.  */
  if (ps != NULL && !mbsinit (ps))
    {
      errno = EINVAL;
      return (size_t)(-1);
    }

  if (s == NULL)
    /* We know the NUL wide character corresponds to the NUL character.  */
    return 1;
  else
    {
#if defined __ANDROID__
      /* Implement consistently with mbrtowc(): through a 1:1 correspondence,
         as in ISO-8859-1.  */
      if (wc >= 0 && wc <= 0xff)
        {
          *s = (unsigned char) wc;
          return 1;
        }
#else
      /* Implement on top of wctomb().  */
      int ret = wctomb (s, wc);

      if (ret >= 0)
        return ret;
#endif
      else
        {
          errno = EILSEQ;
          return (size_t)(-1);
        }
    }
}
