/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2020, paldier <paldier@hotmail.com>.
 * All Rights Reserved.
 * 
 */

#include<stdlib.h>
#include<string.h>

//unlock all languages for everyone
int get_lang_num_unlock()
{
	return 9999;
}

int check_lang_support_unlock(char *lang)
{
	if(strstr("BR CN CZ DE EN ES FR HU IT JP KR MS NL PL RU RO SL TH TR TW UK", lang))
		return 1;
	return 0;
}
/*
int change_preferred_lang_unlock(int finish)
{
	return 1;
}
*/
