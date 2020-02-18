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
 *
 * Copyright 2014-2020 Eric Sauvageau.
 *
 */

extern int get_dns_filter(int proto, int mode, char **server);
extern int dnsfilter_support_dot(int mode);

/* DNSFilter Services */
enum {
	DNSF_SRV_UNFILTERED = 0,
	DNSF_SRV_OPENDNS,
	DNSF_SRV_NORTON1,
	DNSF_SRV_NORTON2,
	DNSF_SRV_NORTON3,
	DNSF_SRV_YANDEX_SECURE,
	DNSF_SRV_YANDEX_FAMILY,
	DNSF_SRV_OPENDNS_FAMILY,
	DNSF_SRV_CUSTOM1,
	DNSF_SRV_CUSTOM2,
	DNSF_SRV_CUSTOM3,
	DNSF_SRV_ROUTER,
	DNSF_SRV_COMODO,
	DNSF_SRV_QUAD9,
	DNSF_SRV_CLEANBROWSING_SECURITY,
	DNSF_SRV_CLEANBROWSING_ADULT,
	DNSF_SRV_CLEANBROWSING_FAMILY,
	DNSF_SRV_LAST
};
