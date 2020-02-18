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

#include <rc.h>
#include <net/ethernet.h>
#ifdef RTCONFIG_DNSFILTER
#include "dnsfilter.h"

char *server_table[][2] = {
	{ "", "" },			/* 0: Unfiltered */
	{ "208.67.222.222", "" },	/* 1: OpenDNS */
	{ "", "" },			/* 2: Discontinued Norton Connect Safe */
	{ "", "" },			/* 3: Discontinued Norton Connect Safe */
	{ "", "" },			/* 4: Discontinued Norton Connect Safe */
	{ "77.88.8.88", "" },		/* 5: Secure Mode safe.dns.yandex.ru */
	{ "77.88.8.7", "" },		/* 6: Family Mode family.dns.yandex.ru */
	{ "208.67.222.123", "" },	/* 7: OpenDNS Family Shield */
	{ "", "" },			/* 8: Custom1 */
	{ "", "" },			/* 9: Custom2 */
	{ "", "" },			/* 10: Custom3 */
	{ "", "" },			/* 11: Router */
	{ "8.26.56.26", "" },		/* 12: Comodo Secure DNS */
	{ "9.9.9.9", "" },		/* 13: Quad9 */
	{ "185.228.168.9", "" },	/* 14: CleanBrowsing Security */
	{ "185.228.168.10", "" },	/* 15: CleanBrowsing Adult */
	{ "185.228.168.168", "" }	/* 16: CleanBrowsing Family */
};

#ifdef RTCONFIG_IPV6
char *server6_table[][2] = {
	{"", ""},		/* 0: Unfiltered */
	{"", ""},		/* 1: OpenDNS */
	{"", ""},		/* 2: Discontinued Norton Connect Safe */
	{"", ""},		/* 3: Discontinued Norton Connect Safe */
	{"", ""},		/* 4: Discontinued Norton Connect Safe */
	{"2a02:6b8::feed:bad","2a02:6b8:0:1::feed:bad"},		/* 5: Secure Mode safe.dns.yandex.ru */
	{"2a02:6b8::feed:a11","2a02:6b8:0:1::feed:a11"},		/* 6: Family Mode family.dns.yandex.ru */
	{"", ""},		/* 7: OpenDNS Family Shield */
	{"", ""},		/* 8: Custom1 - not supported yet */
	{"", ""},		/* 9: Custom2 - not supported yet */
	{"", ""},		/* 10: Custom3 - not supported yet */
	{"", ""},		/* 11: Router  - semi-supported, refer dnsfilter_setup_dnsmasq() */
	{"", ""},		/* 12: Comodo Secure DNS */
	{"2620:fe::fe", "2620:fe::9"},	/* 13: Quad9 */
	{"2a0d:2a00:1::2", "2a0d:2a00:2::2"},	/* 14: CleanBrowsing Security */
	{"2a0d:2a00:1::1", "2a0d:2a00:2::1"},	/* 15: CleanBrowsing Adult */
	{"2a0d:2a00:1::", "2a0d:2a00:2::"}	/* 16: CleanBrowsing Family */
};
#endif


// Return 1 if selected mode supports DNS over TLS
int dnsfilter_support_dot(int mode)
{
	switch (mode){
		case DNSF_SRV_CUSTOM1:
		case DNSF_SRV_CUSTOM2:
		case DNSF_SRV_CUSTOM3:	// Custom 1, 2 and 3 - assume they might support it
		case DNSF_SRV_QUAD9:
		case DNSF_SRV_ROUTER:	// Router (in case end-user implements it locally)
		case DNSF_SRV_CLEANBROWSING_SECURITY:
		case DNSF_SRV_CLEANBROWSING_ADULT:
		case DNSF_SRV_CLEANBROWSING_FAMILY:
			return 1;
		default:
			return 0;
	}
}

// ARG: server must be an array of two pointers, each pointing to an array of chars
int get_dns_filter(int proto, int mode, char **server)
{
	server_table[DNSF_SRV_CUSTOM1][0] = nvram_safe_get("dnsfilter_custom1");
	server_table[DNSF_SRV_CUSTOM2][0] = nvram_safe_get("dnsfilter_custom2");
	server_table[DNSF_SRV_CUSTOM3][0] = nvram_safe_get("dnsfilter_custom3");
	server_table[DNSF_SRV_ROUTER][0] = nvram_safe_get("dhcp_dns1_x");

	int count = 0;

// Initialize
	if (mode >= (sizeof(server_table)/sizeof(server_table[0]))) mode = 0;

#ifdef RTCONFIG_IPV6
	if (proto == AF_INET6) {
		server[0] = server6_table[mode][0];
		server[1] = server6_table[mode][1];
	} else
#endif
	{
		server[0] = server_table[mode][0];
		server[1] = server_table[mode][1];
	}

// Ensure that custom and DHCP-provided DNS do contain something
	if (((mode == DNSF_SRV_CUSTOM1) || (mode == DNSF_SRV_CUSTOM2) || (mode == DNSF_SRV_CUSTOM3) || (mode == DNSF_SRV_ROUTER)) && (!strlen(server[0])) && (proto == AF_INET)) {
		server[0] = nvram_safe_get("lan_ipaddr");
	}

// Report how many non-empty server we are returning
	if (strlen(server[0])) count++;
	if (strlen(server[1])) count++;
	return count;
}


void dnsfilter_settings(FILE *fp, char *lan_ip) {
	char *name, *mac, *mode, *server[2];
	unsigned char ea[ETHER_ADDR_LEN];
	char *nv, *nvp, *rule;
	char lan_class[32];
	int dnsmode;

	if (nvram_get_int("dnsfilter_enable_x")) {
		/* Reroute all DNS requests from LAN */
		ip2class(lan_ip, nvram_safe_get("lan_netmask"), lan_class);
		fprintf(fp, "-A PREROUTING -s %s -p udp -m udp --dport 53 -j DNSFILTER\n"
			    "-A PREROUTING -s %s -p tcp -m tcp --dport 53 -j DNSFILTER\n",
			lan_class, lan_class);

		/* Protection level per client */

#ifdef HND_ROUTER
		nv = nvp = malloc(255 * 6 + 1);
		if (nv) nvram_split_get("dnsfilter_rulelist", nv, 255 * 6 + 1, 5);
#else
		nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
#endif
		while (nv && (rule = strsep(&nvp, "<")) != NULL) {
			if (vstrsep(rule, ">", &name, &mac, &mode) != 3)
				continue;
			if (!*mac || !*mode || !ether_atoe(mac, ea))
				continue;
			dnsmode = atoi(mode);
			if (dnsmode == DNSF_SRV_UNFILTERED) {
				fprintf(fp,
					"-A DNSFILTER -m mac --mac-source %s -j RETURN\n",
					mac);
			} else if (get_dns_filter(AF_INET, dnsmode, server)) {
					fprintf(fp,"-A DNSFILTER -m mac --mac-source %s -j DNAT --to-destination %s\n",
						mac, server[0]);
			}
		}

		free(nv);

		/* Send other queries to the default server */
		dnsmode = nvram_get_int("dnsfilter_mode");
		if ((dnsmode != DNSF_SRV_UNFILTERED) && get_dns_filter(AF_INET, dnsmode, server)) {
			fprintf(fp, "-A DNSFILTER -j DNAT --to-destination %s\n", server[0]);
		}
	}
}


#ifdef RTCONFIG_IPV6
void dnsfilter6_settings(FILE *fp, char *lan_if, char *lan_ip) {
	char *nv, *nvp, *rule;
	char *name, *mac, *mode, *server[2];
	unsigned char ea[ETHER_ADDR_LEN];
	int dnsmode, count;

	fprintf(fp, "-A INPUT -i %s -p udp -m udp --dport 53 -j DNSFILTERI\n"
		    "-A INPUT -i %s -p tcp -m tcp --dport 53 -j DNSFILTERI\n"
		    "-A FORWARD -i %s -p udp -m udp --dport 53 -j DNSFILTERF\n"
		    "-A FORWARD -i %s -p tcp -m tcp --dport 53 -j DNSFILTERF\n"
		    "-A FORWARD -i %s -p tcp -m tcp --dport 853 -j DNSFILTER_DOT\n",
		lan_if, lan_if, lan_if, lan_if, lan_if);

#ifdef HND_ROUTER
	nv = nvp = malloc(255 * 6 + 1);
	if (nv) nvram_split_get("dnsfilter_rulelist", nv, 255 * 6 + 1, 5);
#else
	nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
#endif
	while (nv && (rule = strsep(&nvp, "<")) != NULL) {
		if (vstrsep(rule, ">", &name, &mac, &mode) != 3)
			continue;
		dnsmode = atoi(mode);
		if (!*mac || !ether_atoe(mac, ea))
			continue;
		if (dnsmode == DNSF_SRV_UNFILTERED) {
			fprintf(fp, "-A DNSFILTERI -m mac --mac-source %s -j ACCEPT\n"
				    "-A DNSFILTERF -m mac --mac-source %s -j ACCEPT\n"
				    "-A DNSFILTER_DOT -m mac --mac-source %s -j ACCEPT\n",
					mac, mac, mac);
		} else {	// Filtered
			count = get_dns_filter(AF_INET6, dnsmode, server);
			if (count) {
				fprintf(fp, "-A DNSFILTERF -m mac --mac-source %s -d %s -j ACCEPT\n", mac, server[0]);
				if (dnsfilter_support_dot(dnsmode))
					fprintf(fp, "-A DNSFILTER_DOT -m mac --mac-source %s -d %s -j ACCEPT\n", mac, server[0]);
			}
			if (count == 2) {
				fprintf(fp, "-A DNSFILTERF -m mac --mac-source %s -d %s -j ACCEPT\n", mac, server[1]);
				if (dnsfilter_support_dot(dnsmode))
					fprintf(fp, "-A DNSFILTER_DOT -m mac --mac-source %s -d %s -j ACCEPT\n", mac, server[1]);
			}
			// Reject other servers for that client
			fprintf(fp, "-A DNSFILTERI -m mac --mac-source %s -j DROP\n"
			            "-A DNSFILTERF -m mac --mac-source %s -j DROP\n"
			            "-A DNSFILTER_DOT -m mac --mac-source %s -j DROP\n",
			            mac, mac, mac);
		}
	}
	free(nv);

	dnsmode = nvram_get_int("dnsfilter_mode");
	if (dnsmode != DNSF_SRV_UNFILTERED) {
		/* Allow other queries to the default server, and drop the rest */
		count = get_dns_filter(AF_INET6, dnsmode, server);
		if (count) {
			fprintf(fp, "-A DNSFILTERI -d %s -j ACCEPT\n"
				    "-A DNSFILTERF -d %s -j ACCEPT\n",
				server[0], server[0]);
			if (dnsfilter_support_dot(dnsmode))
				fprintf(fp, "-A DNSFILTER_DOT -d %s -j ACCEPT\n", server[0]);
		}
		if (count == 2) {
			fprintf(fp, "-A DNSFILTERI -d %s -j ACCEPT\n"
				    "-A DNSFILTERF -d %s -j ACCEPT\n",
				server[1], server[1]);
			if (dnsfilter_support_dot(dnsmode))
				fprintf(fp, "-A DNSFILTER_DOT -d %s -j ACCEPT\n", server[1]);
		}
		fprintf(fp, "-A DNSFILTERI -j %s\n"
			    "-A DNSFILTERF -j DROP\n"
		            "-A DNSFILTER_DOT -j DROP\n",
		            (dnsmode == 11 ? "ACCEPT" : "DROP"));
	}
}


void dnsfilter_setup_dnsmasq(FILE *fp) {

	unsigned char ea[ETHER_ADDR_LEN];
	char *name, *mac, *mode, *enable, *server[2];
	char *nv, *nvp, *b;
	int count, dnsmode, defmode;

	defmode = nvram_get_int("dnsfilter_mode");

	for (dnsmode = 1; dnsmode < (sizeof(server6_table)/sizeof(server6_table[0])); dnsmode++) {
		if (dnsmode == defmode)
			continue;
		count = get_dns_filter(AF_INET6, dnsmode, server);
		if (count == 0 && dnsmode == DNSF_SRV_ROUTER) {
			/* Workaround dynamic router address */
			server[0] = "::";
			count = 1;
		}
		if (count == 0)
			continue;
		fprintf(fp, "dhcp-option=dnsf%u,option6:23,[%s]", dnsmode, server[0]);
		if (count == 2)
			fprintf(fp, ",[%s]", server[1]);
		fprintf(fp, "\n");
	}

	/* DNS server per client */
	nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
	while (nv && (b = strsep(&nvp, "<")) != NULL) {
		if (vstrsep(b, ">", &name, &mac, &mode, &enable) < 3)
			continue;
		if (enable && atoi(enable) == 0)
			continue;
		if (!*mac || !*mode || !ether_atoe(mac, ea))
			continue;
		dnsmode = atoi(mode);
		/* Skip unfiltered, default, or non-IPv6 capable levels */
		if ((dnsmode == DNSF_SRV_UNFILTERED) || (dnsmode == defmode) || (get_dns_filter(AF_INET6, dnsmode, server) == 0))
			continue;
		fprintf(fp, "dhcp-host=%s,set:dnsf%u\n", mac, dnsmode);
	}
	free(nv);
}
#endif	// RTCONFIG_IPV6


// Add rules fo Filter chain to prevent the use of DOT if client is filtered, and server does not support DOT
void dnsfilter_dot_rules(FILE *fp, char *lan_if)
{
	char *name, *mac, *mode;
	unsigned char ea[ETHER_ADDR_LEN];
	char *nv, *nvp, *rule, *server[2];
	int dnsmode;

	if (nvram_get_int("dnsfilter_enable_x") == 0) return;

	fprintf(fp, "-A FORWARD -i %s -m tcp -p tcp --dport 853 -j DNSFILTER_DOT\n", lan_if);

#ifdef HND_ROUTER
	nv = nvp = malloc(255 * 6 + 1);
	if (nv) nvram_split_get("dnsfilter_rulelist", nv, 255 * 6 + 1, 5);
#else
	nv = nvp = strdup(nvram_safe_get("dnsfilter_rulelist"));
#endif
	while (nv && (rule = strsep(&nvp, "<")) != NULL) {
		if (vstrsep(rule, ">", &name, &mac, &mode) != 3)
			continue;
		if (!*mac || !*mode || !ether_atoe(mac, ea))
			continue;
		dnsmode = atoi(mode);
		if (dnsmode == DNSF_SRV_UNFILTERED)
			fprintf(fp, "-A DNSFILTER_DOT -m mac --mac-source %s -j RETURN\n", mac);
		else if (dnsfilter_support_dot(dnsmode) && get_dns_filter(AF_INET, dnsmode, server) > 0 )	// Filter supports DOT
			fprintf(fp, "-A DNSFILTER_DOT -m mac --mac-source %s ! -d %s -j REJECT\n", mac, server[0]);
		else	// Reject DOT access
			fprintf(fp, "-A DNSFILTER_DOT -m mac --mac-source %s -j REJECT\n", mac);
	}
	free(nv);

	/* Global filtering */
	dnsmode = nvram_get_int("dnsfilter_mode");
	if (dnsmode != DNSF_SRV_UNFILTERED) {
		if (dnsfilter_support_dot(dnsmode) && get_dns_filter(AF_INET, dnsmode, server) > 0 )
			fprintf(fp, "-A DNSFILTER_DOT ! -d %s -j REJECT\n", server[0]);
		else
			fprintf(fp, "-A DNSFILTER_DOT -j REJECT\n");
	}
}

#endif	// RTCONFIG_DNSFILTER
