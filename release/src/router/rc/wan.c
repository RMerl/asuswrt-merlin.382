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
 * Network services
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>
#include <dirent.h>
#include <linux/sockios.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <wlutils.h>
#include <rc.h>
#include <bcmutils.h>
#include <bcmparams.h>
#include <net/route.h>
#include <stdarg.h>

#include <linux/types.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#endif

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

#ifdef RTCONFIG_ALPINE
#include <alpine.h>
#endif

#ifdef RTCONFIG_LANTIQ
#include <lantiq.h>
#endif

#ifdef RTCONFIG_BCM9
#include <linux/netlink.h>
#include <ctf/ctf_cfg.h>
#endif

#ifdef RTCONFIG_BWDPI
#include <bwdpi_common.h>
#endif
#define	MAX_MAC_NUM	16
static int mac_num;
static char mac_clone[MAX_MAC_NUM][18];

void convert_wan_nvram(char *prefix, int unit);

#if defined(DSL_N55U) || defined(DSL_N55U_B)
int classATargetTable[]={
	1,
	14,
	27,
	36,
	39,
	42,
	49,
	58,
	59,
	60,
	61,
	101,
	103,
	106,
	110,
	111,
	112,
	113,
	114,
	115,
	116,
	117,
	118,
	119,
	120,
	121,
	122,
	123,
	124,
	125,
	126,
	175,
	180,
	182,
	183,
	202,
	203,
	210,
	211,
	218,
	219,
	220,
	221,
	222,
	223
};

int isTargetArea()
{
	int i;
	char *ip = get_wanip();
	int prefixA = inet_network(ip) >> 24;
_dprintf("==>%s ip: %s, prefix: %d\n", __func__, ip, prefixA);
	for(i=0; i<sizeof(classATargetTable); i++) {
		if( prefixA == classATargetTable[i] )
			return 1;
	}
	return 0;
}
#endif

#ifdef RTCONFIG_DUALWAN
void check_wan_nvram(void){
	if(nvram_match("wan1_proto", "")) nvram_set("wan1_proto", "dhcp");
}
#else
int add_multi_routes(void){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_proto[32];
	char cmd[2048];
	char wan_multi_if[WAN_UNIT_MAX][32], wan_multi_gate[WAN_UNIT_MAX][32];
	int debug = nvram_get_int("routes_debug");
	int lock;
	lock = file_lock("mt_routes");

	// clean the rules of routing table and re-build them then.
	system("ip rule flush");
	system("ip rule add from all lookup main pref 32766");
	system("ip rule add from all lookup default pref 32767");

	memset(wan_multi_if, 0, sizeof(char)*WAN_UNIT_MAX*32);
	memset(wan_multi_gate, 0, sizeof(char)*WAN_UNIT_MAX*32);

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){ // Multipath
		if(unit != wan_primary_ifunit())
			continue;

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));
		snprintf(wan_multi_if[unit], sizeof(wan_multi_if[unit]), "%s", get_wan_ifname(unit));
		snprintf(wan_multi_gate[unit], sizeof(wan_multi_gate[unit]), "%s", nvram_safe_get(strcat_r(prefix, "gateway", tmp)));

		// when wan_down().
		if(!is_wan_connect(unit))
			continue;

		snprintf(cmd, sizeof(cmd), "ip route replace %s dev %s proto kernel", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 10. cmd=%s.\n", cmd);
		system(cmd);

		// set the default gateway.
		snprintf(cmd, sizeof(cmd), "ip route replace default via %s dev %s", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 11. cmd=%s.\n", cmd);
		system(cmd);

		if(!strcmp(wan_proto, "pptp") || !strcmp(wan_proto, "l2tp")){
			snprintf(cmd, sizeof(cmd), "ip route del %s dev %s 2>/dev/null", wan_multi_gate[unit], wan_multi_if[unit]);
if(debug) printf("test 12. cmd=%s.\n", cmd);
			system(cmd);
		}
	}

if(debug) printf("test 26. route flush cache.\n");
	system("ip route flush cache");

	file_unlock(lock);
	return 0;
}
#endif

int
add_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100], *buf;

	buf = strdup(nvram_safe_get(strcat_r(prefix, var, tmp)));
	if (buf == NULL)
		return -1;

	foreach(word, buf, next) {
		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;
		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		/* Incorrect, empty and 0.0.0.0
		 * probably need to allow empty gateway to set on-link route */
		if (inet_addr_(gateway) == INADDR_ANY)
			gateway = nvram_safe_get_r(strcat_r(prefix, "xgateway", tmp), tmp, sizeof(tmp));
		route_add(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}
	free(buf);

	return 0;
}

static void
add_dhcp_routes(char *prefix, char *ifname, int metric)
{
	char *routes, *tmp;
	char nvname[sizeof("wanXXXXXXXXXX_routesXXX")];
	char *ipaddr, *gateway;
	char netmask[sizeof("255.255.255.255")];
	struct in_addr mask;
	int netsize;

	if (nvram_get_int("dr_enable_x") == 0)
		return;

	/* classful static routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes", nvname)));
	for (tmp = routes; tmp && *tmp; ) {
		ipaddr  = strsep(&tmp, "/");
		gateway = strsep(&tmp, " ");
		if (gateway && inet_addr(ipaddr) != INADDR_ANY)
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
	}
	free(routes);

	/* ms claseless static routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes_ms", nvname)));
	for (tmp = routes; tmp && *tmp; ) {
		ipaddr  = strsep(&tmp, "/");
		netsize = atoi(strsep(&tmp, " "));
		gateway = strsep(&tmp, " ");
		if (gateway && netsize > 0 && netsize <= 32 && inet_addr(ipaddr) != INADDR_ANY) {
			mask.s_addr = htonl(0xffffffff << (32 - netsize));
			strcpy(netmask, inet_ntoa(mask));
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
		}
	}
	free(routes);

	/* rfc3442 classless static routes */
	routes = strdup(nvram_safe_get(strcat_r(prefix, "routes_rfc", nvname)));
	for (tmp = routes; tmp && *tmp; ) {
		ipaddr  = strsep(&tmp, "/");
		netsize = atoi(strsep(&tmp, " "));
		gateway = strsep(&tmp, " ");
		if (gateway && netsize > 0 && netsize <= 32 && inet_addr(ipaddr) != INADDR_ANY) {
			mask.s_addr = htonl(0xffffffff << (32 - netsize));
			strcpy(netmask, inet_ntoa(mask));
			route_add(ifname, metric + 1, ipaddr, gateway, netmask);
		}
	}
	free(routes);
}

int
del_routes(char *prefix, char *var, char *ifname)
{
	char word[80], *next;
	char *ipaddr, *netmask, *gateway, *metric;
	char tmp[100], *buf;

	buf = strdup(nvram_safe_get(strcat_r(prefix, var, tmp)));
	if (buf == NULL)
		return -1;

	foreach(word, buf, next) {
		_dprintf("%s: %s\n", __FUNCTION__, word);

		netmask = word;
		ipaddr = strsep(&netmask, ":");
		if (!ipaddr || !netmask)
			continue;
		gateway = netmask;
		netmask = strsep(&gateway, ":");
		if (!netmask || !gateway)
			continue;

		metric = gateway;
		gateway = strsep(&metric, ":");
		if (!gateway || !metric)
			continue;

		if (inet_addr_(gateway) == INADDR_ANY)
			gateway = nvram_safe_get_r(strcat_r(prefix, "xgateway", tmp), tmp, sizeof(tmp));

		route_del(ifname, atoi(metric) + 1, ipaddr, gateway, netmask);
	}
	free(buf);

	return 0;
}

#if 0
#ifdef RTCONFIG_IPV6
void
stop_ecmh(void)
{
	if (pids("ecmh"))
	{
		killall_tk("ecmh");
		sleep(1);
	}
}

void
start_ecmh(const char *wan_ifname)
{
	int service = get_ipv6_service();

	stop_ecmh();

	if (!wan_ifname || (strlen(wan_ifname) <= 0))
		return;

	if (!nvram_get_int("mr_enable_x"))
		return;

	switch (service) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		eval("/bin/ecmh", "-u", nvram_safe_get("http_username"), "-i", (char*)wan_ifname);
		break;
	}
}
#endif
#endif

void
stop_igmpproxy()
{
	if (pids("udpxy"))
		killall_tk("udpxy");
#ifdef BLUECAVE
	stop_mcast_proxy();
#else
	if (pids("igmpproxy"))
		killall_tk("igmpproxy");
#endif
}

void	// oleg patch , add
start_igmpproxy(char *wan_ifname)
{
#if !defined(HND_ROUTER) && !defined(MCPD_PROXY) && !defined(BLUECAVE)
	FILE *fp;
	static char *igmpproxy_conf = "/tmp/igmpproxy.conf";
	char *altnet, buf[32];
#endif

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
	if ( nvram_match("wan0_ifname", wan_ifname)
		&& get_dualwan_primary() == WANS_DUALWAN_IF_DSL) {
		if (nvram_get_int("dslx_config_num") > 1)
			wan_ifname = "br1";
	}
#else
	if (nvram_get_int("dslx_config_num") > 1)
		wan_ifname = "br1";
#endif
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6 &&
	    nvram_match("switch_wantag", "movistar") &&
	    !nvram_match("iptv_ifname", wan_ifname))
		return;
#endif

	stop_igmpproxy();

	if (nvram_get_int("udpxy_enable_x")) {
		_dprintf("start udpxy [%s]\n", wan_ifname);
		eval("/usr/sbin/udpxy",
			"-m", wan_ifname,
			"-p", nvram_safe_get("udpxy_enable_x"),
			"-B", "65536",
			"-c", nvram_safe_get("udpxy_clients"),
			"-a", nvram_get("lan_ifname") ? : "br0");
	}

#if !defined(HND_ROUTER)
	if (!nvram_get_int("mr_enable_x"))
		return;
#endif

	_dprintf("start igmpproxy [%s]\n", wan_ifname);

#if !defined(HND_ROUTER) && !defined(MCPD_PROXY) && !defined(BLUECAVE)
	if ((fp = fopen(igmpproxy_conf, "w")) == NULL) {
		perror(igmpproxy_conf);
		return;
	}

	snprintf(buf, sizeof(buf), "%s", nvram_safe_get("mr_altnet_x"));
	altnet = buf;

	if (nvram_get_int("mr_qleave_x"))
		fprintf(fp, "quickleave\n");
	fprintf(fp,
		"phyint %s upstream ratelimit 0 threshold 1 altnet %s\n"
		"phyint %s downstream ratelimit 0 threshold 1\n",
		wan_ifname, *altnet ? altnet : "0.0.0.0/0",
		nvram_get("lan_ifname") ? : "br0");

	append_custom_config("igmpproxy.conf", fp);
	fclose(fp);
	use_custom_config("igmpproxy.conf", igmpproxy_conf);
	run_postconf("igmpproxy", igmpproxy_conf);

	eval("/usr/sbin/igmpproxy", igmpproxy_conf);
#else
	nvram_set("igmp_ifname", wan_ifname);
#ifndef BLUECAVE
	start_mcpd_proxy();
#else
	start_mcast_proxy();
#endif
#endif
}

int
wan_prefix(char *ifname, char *prefix)
{
	int unit;

	if ((unit = wan_ifunit(ifname)) < 0 &&
	    (unit = wanx_ifunit(ifname)) < 0){
		if(wan_ifunit(ifname) < 0)
			logmessage("wan", "[%s] exit [%d], ifname:[%s]", __FUNCTION__, __LINE__, ifname);
		if(wanx_ifunit(ifname) < 0)
			logmessage("wan", "[%s] exit [%d], ifname:[%s]", __FUNCTION__, __LINE__, ifname);
		return -1;
	}

	sprintf(prefix, "wan%d_", unit);

	return unit;
}

static int
add_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
		return -1;

	return add_routes(prefix, "route", wan_ifname);
}

static int
del_wan_routes(char *wan_ifname)
{
	char prefix[] = "wanXXXXXXXXXX_";

	/* Figure out nvram variable name prefix for this i/f */
	if (wan_prefix(wan_ifname, prefix) < 0)
#if 0
		return -1;
#else
		snprintf(prefix, sizeof(prefix), "wan%d_", WAN_UNIT_FIRST);
#endif

	return del_routes(prefix, "route", wan_ifname);
}

#ifdef QOS
int enable_qos()
{
#if defined (W7_LOGO) || defined (WIFI_LOGO)
	return 0;
#endif
	int qos_userspec_app_en = 0;
	int rulenum = nvram_get_int("qos_rulenum_x"), idx_class = 0;

	/* Add class for User specify, 10:20(high), 10:40(middle), 10:60(low)*/
	if (rulenum) {
		for (idx_class=0; idx_class < rulenum; idx_class++)
		{
			if (atoi(Ch_conv("qos_prio_x", idx_class)) == 1)
			{
				qos_userspec_app_en = 1;
				break;
			}
			else if (atoi(Ch_conv("qos_prio_x", idx_class)) == 6)
			{
				qos_userspec_app_en = 1;
				break;
			}
		}
	}

	if (	(nvram_match("qos_tos_prio", "1") ||
		 nvram_match("qos_pshack_prio", "1") ||
		 nvram_match("qos_service_enable", "1") ||
		 nvram_match("qos_shortpkt_prio", "1")	) ||
		(!nvram_match("qos_manual_ubw","0") && *nvram_safe_get("qos_manual_ubw")) ||
		(rulenum && qos_userspec_app_en)
	)
	{
		fprintf(stderr, "found QoS rulues\n");
		return 1;
	}
	else
	{
		fprintf(stderr, "no QoS rulues\n");
		return 0;
	}
}
#endif

/*
 * (1) wan[x]_ipaddr_x/wan[x]_netmask_x/wan[x]_gateway_x/...:
 *    static ip or ip get from dhcp
 *
 * (2) wan[x]_xipaddr/wan[x]_xnetmaskwan[x]_xgateway/...:
 *    ip get from dhcp when proto = l2tp/pptp/pppoe
 *
 * (3) wan[x]_ipaddr/wan[x]_netmask/wan[x]_gateway/...:
 *    always keeps the latest updated ip/netmask/gateway in system
 *      static: it is the same as (1)
 *      dhcp:
 *      	- before getting ip from dhcp server, it is 0.0.0.0
 *      	- after getting ip from dhcp server, it is updated
 *      l2tp/pptp/pppoe with static ip:
 *      	- before getting ip from vpn server, it is the same as (1)
 *      	- after getting ip from vpn server, it is the one from vpn server
 *      l2tp/pptp/pppoe with dhcp ip:
 *      	- before getting ip from dhcp server, it is 0.0.0.0
 *      	- before getting ip from vpn server, it is the one from vpn server
 */

void update_wan_state(char *prefix, int state, int reason)
{
	char tmp[100], tmp1[100], *ptr;
	int unit = -1;

	_dprintf("%s(%s, %d, %d)\n", __FUNCTION__, prefix, state, reason);

	if (strncmp(prefix, "wan", 3) == 0)
		unit = atoi(prefix + 3);

	nvram_set_int(strcat_r(prefix, "state_t", tmp), state);
	if(state == WAN_STATE_CONNECTED)
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), WAN_STOPPED_REASON_NONE);
	else
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);

	// 20110610, reset auxstate each time state is changed
	nvram_set_int(strcat_r(prefix, "auxstate_t", tmp), 0);

	if (state == WAN_STATE_INITIALIZING)
	{
		nvram_set(strcat_r(prefix, "proto_t", tmp), nvram_safe_get(strcat_r(prefix, "proto", tmp1)));

		/* reset wanX_* variables */
		if (!nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp))) {
			nvram_set(strcat_r(prefix, "ipaddr", tmp), nvram_safe_get(strcat_r(prefix, "ipaddr_x", tmp1)));
			nvram_set(strcat_r(prefix, "netmask", tmp), nvram_safe_get(strcat_r(prefix, "netmask_x", tmp1)));
			nvram_set(strcat_r(prefix, "gateway", tmp), nvram_safe_get(strcat_r(prefix, "gateway_x", tmp1)));
		}
		else {
			nvram_set(strcat_r(prefix, "ipaddr", tmp), "0.0.0.0");
			nvram_set(strcat_r(prefix, "netmask", tmp), "0.0.0.0");
			nvram_set(strcat_r(prefix, "gateway", tmp), "0.0.0.0");
		}
		nvram_unset(strcat_r(prefix, "domain", tmp));
		nvram_unset(strcat_r(prefix, "lease", tmp));
		nvram_unset(strcat_r(prefix, "expires", tmp));
		nvram_unset(strcat_r(prefix, "routes", tmp));
		nvram_unset(strcat_r(prefix, "routes_ms", tmp));
		nvram_unset(strcat_r(prefix, "routes_rfc", tmp));

		/* reset wanX_x* VPN variables */
		nvram_set(strcat_r(prefix, "xipaddr", tmp), nvram_safe_get(strcat_r(prefix, "ipaddr", tmp1)));
		nvram_set(strcat_r(prefix, "xnetmask", tmp), nvram_safe_get(strcat_r(prefix, "netmask", tmp1)));
		nvram_set(strcat_r(prefix, "xgateway", tmp), nvram_safe_get(strcat_r(prefix, "gateway", tmp1)));
		nvram_unset(strcat_r(prefix, "xdomain", tmp));
		nvram_unset(strcat_r(prefix, "xlease", tmp));
		nvram_unset(strcat_r(prefix, "xexpires", tmp));
		nvram_unset(strcat_r(prefix, "xroutes", tmp));
		nvram_unset(strcat_r(prefix, "xroutes_ms", tmp));
		nvram_unset(strcat_r(prefix, "xroutes_rfc", tmp));

		/* reset wanX_dns && wanX_xdns VPN */
		ptr = nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp)) ? "" :
			get_userdns_r(prefix, tmp1, sizeof(tmp1));
		nvram_set(strcat_r(prefix, "dns", tmp), ptr);
		if (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") ||
		    nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
		    nvram_match(strcat_r(prefix, "proto", tmp), "l2tp"))
			nvram_set(strcat_r(prefix, "xdns", tmp), ptr);
		else
			nvram_unset(strcat_r(prefix, "xdns", tmp));

#ifdef RTCONFIG_IPV6
		nvram_set(strcat_r(prefix, "6rd_ip4size", tmp), "");
		nvram_set(strcat_r(prefix, "6rd_router", tmp), "");
		nvram_set(strcat_r(prefix, "6rd_prefix", tmp), "");
		nvram_set(strcat_r(prefix, "6rd_prefixlen", tmp), "");
#endif
#ifdef RTCONFIG_TR069
//		nvram_unset(strcat_r(prefix, "tr_acs_url", tmp));
//		nvram_unset(strcat_r(prefix, "tr_pvgcode", tmp));
#endif
	}
#if 0
	else if (state == WAN_STATE_STOPPED) {
		// Save Stopped Reason
		// keep ip info if it is stopped from connected
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);
	}
#endif
	else if(state == WAN_STATE_STOPPING) {
		snprintf(tmp, sizeof(tmp), "/var/run/ppp-wan%d.status", unit);
		unlink(tmp);
	}

#if defined(RTCONFIG_WANRED_LED)
	switch (state) {
	case WAN_STATE_INITIALIZING:
	case WAN_STATE_STOPPED:
	case WAN_STATE_CONNECTING:
	case WAN_STATE_STOPPING:
	case WAN_STATE_CONNECTED:
		/* update WAN LED(s) as soon as possible. */
		update_wan_leds(unit);
	}
#endif

	sprintf(tmp,"%d", unit);

	switch (state) {
	case WAN_STATE_INITIALIZING:
		strcpy(tmp1, "init");
		break;
	case WAN_STATE_CONNECTING:
		strcpy(tmp1, "connecting");
		break;
	case WAN_STATE_CONNECTED:
		strcpy(tmp1, "connected");
		break;
	case WAN_STATE_DISCONNECTED:
		strcpy(tmp1, "disconnected");
		break;
	case WAN_STATE_STOPPED:
		strcpy(tmp1, "stopped");
		break;
	case WAN_STATE_DISABLED:
		strcpy(tmp1, "disabled");
		break;
	case WAN_STATE_STOPPING:
		strcpy(tmp1, "stopping");
		break;
	default:
		sprintf(tmp1, "state %d", state);
	}

	run_custom_script("wan-event", 0, tmp, tmp1);

	/* For backward/legacy compatibility */
	if (state == WAN_STATE_CONNECTED) {
		sprintf(tmp,"%c",prefix[3]);
		run_custom_script("wan-start", 0, tmp, NULL);
	}

}

#ifdef RTCONFIG_IPV6
// for one ipv6 in current stage
void update_wan6_state(char *prefix, int state, int reason)
{
	char tmp[100];

	_dprintf("%s(%s, %d, %d)\n", __FUNCTION__, prefix, state, reason);

	nvram_set_int(strcat_r(prefix, "state_t", tmp), state);
	nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), 0);

	if (state == WAN_STATE_INITIALIZING)
	{
	}
	else if (state == WAN_STATE_STOPPED) {
		// Save Stopped Reason
		// keep ip info if it is stopped from connected
		nvram_set_int(strcat_r(prefix, "sbstate_t", tmp), reason);
	}

}
#endif

// IPOA test case
// 111.235.232.137 (gateway)
// 111.235.232.138 (ip)
// 255.255.255.252 (netmask)

// cat /proc/net/arp
// arp -na

#ifdef RTCONFIG_DSL
static int start_ipoa()
{
	char tc_mac[32];
	char ip_addr[32];
	char ip_mask[32];
	char ip_gateway[32];
	int try_cnt;
	FILE* fp_dsl_mac;
	FILE* fp_log;

	int NeighborIpNum;
	int i;
	int NeiBaseIpNum;
	int LastIpNum;
	int NetMaskLastIpNum;
	char NeighborIpPrefix[32];
	int ip_addr_dot_cnt;
	char CmdBuf[128];

	// mac address is adsl mac
	for (try_cnt = 0; try_cnt < 10; try_cnt++)
	{
		fp_dsl_mac = fopen("/tmp/adsl/tc_mac.txt","r");
		if (fp_dsl_mac != NULL)
		{
			fgets(tc_mac,sizeof(tc_mac),fp_dsl_mac);
			fclose(fp_dsl_mac);
			break;
		}
		usleep(1000*1000);
	}

#ifdef RTCONFIG_DUALWAN
	if (get_dualwan_secondary()==WANS_DUALWAN_IF_DSL)
	{
		strcpy(ip_gateway, nvram_safe_get("wan1_gateway"));
		strcpy(ip_addr, nvram_safe_get("wan1_ipaddr"));
		strcpy(ip_mask, nvram_safe_get("wan1_netmask"));
	}
	else
	{
		strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
		strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
		strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
	}
#else
	strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
	strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
	strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
#endif

	// we only support maximum 256 neighbor host
	if (strncmp("255.255.255",ip_mask,11) != 0)
	{
		fp_log = fopen("/tmp/adsl/ipoa_too_many_neighbors.txt","w");
		fputs("ErrorMsg",fp_log);
		fclose(fp_log);
		return -1;
	}

//
// do not send arp to neighborhood and gateway
//

	ip_addr_dot_cnt = 0;
	for (i=0; i<sizeof(NeighborIpPrefix); i++)
	{
		if (ip_addr[i] == '.')
		{
			ip_addr_dot_cnt++;
			if (ip_addr_dot_cnt >= 3) break;
		}
		NeighborIpPrefix[i]=ip_addr[i];
	}
	NeighborIpPrefix[i] = 0;

	LastIpNum = atoi(&ip_addr[i+1]);
	NetMaskLastIpNum = atoi(&ip_mask[12]);
	NeighborIpNum = ((~NetMaskLastIpNum) + 1)&0xff;
	NeiBaseIpNum = LastIpNum & NetMaskLastIpNum;

	//
	// add gateway host
	//
#ifdef RTCONFIG_DSL_TCLINUX
	eval("arp","-i",nvram_safe_get("wan0_ifname"),"-a",ip_gateway,"-s",tc_mac);
#else
	eval("arp","-i","br0","-a",ip_gateway,"-s",tc_mac);
#endif

	// add neighbor hosts
	for (i=0; i<NeighborIpNum; i++)
	{
		snprintf(CmdBuf, sizeof(CmdBuf), "%s.%d", NeighborIpPrefix, i+NeiBaseIpNum);
#ifdef RTCONFIG_DSL_TCLINUX
		eval("arp","-i",nvram_safe_get("wan0_ifname"),"-a",CmdBuf,"-s",tc_mac);
#else
		eval("arp","-i","br0","-a",CmdBuf,"-s",tc_mac);
#endif
	}

	return 0;
}

static int stop_ipoa()
{
	char ip_addr[32];
	char ip_mask[32];
	char ip_gateway[32];
	FILE* fp_log;

	int NeighborIpNum;
	int i;
	int NeiBaseIpNum;
	int LastIpNum;
	int NetMaskLastIpNum;
	char NeighborIpPrefix[32];
	int ip_addr_dot_cnt;
	char CmdBuf[128];

#ifdef RTCONFIG_DUALWAN
		if (get_dualwan_secondary()==WANS_DUALWAN_IF_DSL)
		{
			strcpy(ip_gateway, nvram_safe_get("wan1_gateway"));
			strcpy(ip_addr, nvram_safe_get("wan1_ipaddr"));
			strcpy(ip_mask, nvram_safe_get("wan1_netmask"));
		}
		else
		{
			strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
			strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
			strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
		}
#else
		strcpy(ip_gateway, nvram_safe_get("wan0_gateway"));
		strcpy(ip_addr, nvram_safe_get("wan0_ipaddr"));
		strcpy(ip_mask, nvram_safe_get("wan0_netmask"));
#endif

	// we only support maximum 256 neighbor host
	if (strncmp("255.255.255",ip_mask,11) != 0)
	{
		fp_log = fopen("/tmp/adsl/ipoa_too_many_neighbors.txt","w");
		fputs("ErrorMsg",fp_log);
		fclose(fp_log);
		return -1;
	}

	//
	// do not send arp to neighborhood and gateway
	//

	ip_addr_dot_cnt = 0;
	for (i=0; i<sizeof(NeighborIpPrefix); i++)
	{
		if (ip_addr[i] == '.')
		{
			ip_addr_dot_cnt++;
			if (ip_addr_dot_cnt >= 3) break;
		}
		NeighborIpPrefix[i]=ip_addr[i];
	}
	NeighborIpPrefix[i] = 0;

	LastIpNum = atoi(&ip_addr[i+1]);
	NetMaskLastIpNum = atoi(&ip_mask[12]);
	NeighborIpNum = ((~NetMaskLastIpNum) + 1)&0xff;
	NeiBaseIpNum = LastIpNum & NetMaskLastIpNum;

	//
	// delete gateway host
	//
	eval("arp","-d",ip_gateway);

	// delete neighbor hosts
	for (i=0; i<NeighborIpNum; i++)
	{
		snprintf(CmdBuf, sizeof(CmdBuf), "%s.%d", NeighborIpPrefix, i+NeiBaseIpNum);
		eval("arp", "-d", CmdBuf);
	}

	return 0;
}
#endif

#ifdef DSL_AC68U	//Andy Chiu, 2015/09/15
#include "interface.h"
extern int cur_ewan_vid;

int check_wan_if(int unit)
{
	//check wan mode.
	dbG("unit=%d\n", unit);
	if(get_wans_dualwan()&WANSCAP_LAN)
	{
		//check vid
		int new_vid;

		if(nvram_match("ewan_dot1q", "1"))
			new_vid = nvram_get_int("ewan_vid");
		else
			new_vid = 4;
		dbG("cur_vid=%d, new_vid=%d\n", cur_ewan_vid, new_vid);

		if(new_vid != cur_ewan_vid)
		{
			//update port info
			char buf[32], wan[32];
			const int ports[] = { 0, 1, 2, 3, 4, 5 };
			int wan1cfg = nvram_get_int("wans_lanport") + WAN1PORT1 - 1;

			snprintf(buf, sizeof(buf), "vlan%dports", cur_ewan_vid);
			nvram_unset(buf);
			switch_gen_config(wan, ports, wan1cfg, 1, "t");
			snprintf(buf, sizeof(buf), "vlan%dports", new_vid);
			nvram_set(buf, wan);

			//update hwname info
			snprintf(buf, sizeof(buf), "vlan%dhwname", cur_ewan_vid);
			nvram_unset(buf);
			snprintf(buf, sizeof(buf), "vlan%dhwname", new_vid);
			nvram_set(buf, "et0");

			//generate vlan interfaces
			char cur_vif[16], new_vif[16];

			snprintf(cur_vif, sizeof(cur_vif), "vlan%d", cur_ewan_vid);
			snprintf(new_vif, sizeof(new_vif), "vlan%d", new_vid);

			dbG("cur_vif=%s, new_vif=%s\n", cur_vif, new_vif);

			char word[256], *next, tmp[256];

			//replace old vlan interface by new vlan interface
			memset(tmp, 0, 256);
			foreach(word, nvram_safe_get("wandevs"), next)
			{
				sprintf(tmp, "%s %s", tmp, (!strcmp(word, cur_vif))? new_vif: word);
			}
			dbG("wandevs=%s\n", tmp);
			nvram_set("wandevs", tmp);

			//replace wan_phy
			memset(tmp, 0, 256);
			foreach(word, nvram_safe_get("wan_ifnames"), next)
			{
				sprintf(tmp, "%s %s", tmp, (!strcmp(word, cur_vif))? new_vif: word);
			}
			dbG("wan_ifnames=%s\n", tmp);
			nvram_set("wan_ifnames", tmp);

			//check all wan unit.
			int unit;
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
			{
				snprintf(buf, sizeof(buf), "wan%d_ifname", unit);
				if(nvram_match(buf, cur_vif))
				{
					dbG("%s=%s\n", buf, new_vif);
					nvram_set(buf, new_vif);
				}

				snprintf(buf, sizeof(buf), "wan%d_gw_ifname", unit);
				if(nvram_match(buf, cur_vif))
				{
					dbG("%s=%s\n", buf, new_vif);
					nvram_set(buf, new_vif);
				}
			}

			//remove old vlan
			eval("ifconfig", cur_vif, "down");
			eval("vconfig", "rem", cur_vif);

			//set new vlan
			eval("vconfig", "set_name_type", "VLAN_PLUS_VID_NO_PAD");
			snprintf(buf, sizeof(buf), "%d", new_vid);
			eval("vconfig", "add", "eth0", buf);
			eval("ifconfig", new_vif, "up");
			cur_ewan_vid = new_vid;
		}
		dbG("Set switch\n");
		config_switch_dsl_set_lan();
	}
	return 0;
}
#endif

void
start_wan_if(int unit)
{
#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
	int wan_type;
#endif
#ifdef RTCONFIG_DUALWAN
	int pppoerelay_unit;
#endif
	char wan_ifname[16];
	char wan_proto[16];
	char tmp[100], prefix[32];
	char eabuf[32];
	int s;
	struct ifreq ifr;
	pid_t pid;
#ifdef RTCONFIG_USB_MODEM
	int flags, mtu = 0;
	char usb_node[32], port_path[8];
	char nvram_name[32];
	int i = 0;
	char modem_type[32];
#ifdef RTCONFIG_USB_BECEEM
	unsigned int uvid, upid;
#endif
#ifdef SET_USB_MODEM_MTU_ETH
	int modem_mtu;
#endif
	int modem_unit;
	char tmp2[100], prefix2[32];
	char env_unit[32];
#endif
	struct vlan_ioctl_args ifv;

	TRACE_PT("unit=%d.\n", unit);
	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

	/* variable should exist? */
	if (nvram_match(strcat_r(prefix, "enable", tmp), "0")) {
		update_wan_state(prefix, WAN_STATE_DISABLED, 0);
		return;
	}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	// had detected the DATA limit before.
	else if(get_wan_sbstate(unit) == WAN_STOPPED_REASON_DATALIMIT){
		TRACE_PT("start_wan_if: Data limit was detected and skip the start_wan_if().\n");
		return;
	}
#endif

	update_wan_state(prefix, WAN_STATE_INITIALIZING, 0);

#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_USB_MODEM)
	wan_type = get_dualwan_by_unit(unit);
#endif

#ifdef RTCONFIG_DUALWAN
	if (is_router_mode()) {
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			check_wan_nvram();
	}
#endif

#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(unit)) {
		FILE *fp;

#ifdef RTCONFIG_USB_MODEM
		modem_unit = get_modemunit_by_type(get_dualwan_by_unit(unit));
		usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));
		snprintf(env_unit, sizeof(env_unit), "unit=%d", modem_unit);

		if(nvram_get_int(strcat_r(prefix2, "act_scanning", tmp2)) != 0){
_dprintf("start_wan_if: USB modem is scanning...\n");
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_USBSCAN);
			return;
		}
		else
#endif
		if(is_usb_modem_ready(get_dualwan_by_unit(unit)) != 1){
			TRACE_PT("No USB Modem!\n");
			return;
		}

		TRACE_PT("3g begin.\n");
		if(nvram_match("g3err_pin", "1")){
			TRACE_PT("3g end: PIN error previously!\n");
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PINCODE_ERR);
			return;
		}

		update_wan_state(prefix, WAN_STATE_CONNECTING, WAN_STOPPED_REASON_NONE);

		snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get(strcat_r(prefix2, "act_type", tmp2)));
		if(strlen(modem_type) <= 0){
			putenv(env_unit);
			eval("/usr/sbin/find_modem_type.sh");
			unsetenv("unit");
			snprintf(modem_type, sizeof(modem_type), "%s", nvram_safe_get(strcat_r(prefix2, "act_type", tmp2)));
		}

		if(!strcmp(modem_type, "tty") || !strcmp(modem_type, "qmi") || !strcmp(modem_type, "mbim") || !strcmp(modem_type, "gobi")){
			if(strcmp(modem_type, "gobi") && !strcmp(nvram_safe_get(strcat_r(prefix2, "act_int", tmp2)), "")){
				if(!strcmp(modem_type, "qmi")){	// e.q. Huawei E398.
					TRACE_PT("Sleep 3 seconds to wait modem nodes.\n");
					sleep(3);
				}
			}

			// find the modem node at every start_wan_if() to avoid the incorrct one sometimes.
			putenv(env_unit);
			eval("/usr/sbin/find_modem_node.sh");
			unsetenv("unit");
		}

		if(nvram_get_int(strcat_r(prefix2, "act_reset", tmp2)) == 1){
			// need to execute find_modem_xxx.sh again.
			TRACE_PT("3g end: Reseting the modem...\n");
			return;
		}

		/* Stop pppd */
		stop_pppd(unit);

		/* Stop dhcp client */
		stop_udhcpc(unit);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		unsigned long long rx, tx;
		unsigned long long total, limit;

		rx = strtoull(nvram_safe_get("modem_bytes_rx"), NULL, 10);
		tx = strtoull(nvram_safe_get("modem_bytes_tx"), NULL, 10);
		limit = strtoull(nvram_safe_get("modem_bytes_data_limit"), NULL, 10);

		total = rx+tx;

		if(limit > 0 && total >= limit){
			TRACE_PT("3g end: Data limit was set: limit %llu, now %llu.\n", limit, total);
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_DATALIMIT);
			return;
		}
#endif

		if(nvram_get_int("stop_conn_3g") == 1){
			write_3g_ppp_conf(get_modemunit_by_type(wan_type));
		}
		else if(strcmp(modem_type, "wimax")){
			char *modem_argv[] = {"/usr/sbin/modem_enable.sh", NULL};
			int sim_state;

			putenv(env_unit);
			_eval(modem_argv, ">>/tmp/usb.log", 0, NULL);
			unsetenv("unit");

			if(nvram_match("g3err_pin", "1")){
				TRACE_PT("3g end: PIN error!\n");
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PINCODE_ERR);
				return;
			}

			snprintf(tmp, sizeof(tmp), "%s", nvram_safe_get(strcat_r(prefix2, "act_sim", tmp2)));
			if(strlen(tmp) > 0){
				sim_state = atoi(tmp);
				if(sim_state == 2 || sim_state == 3){
					TRACE_PT("3g end: Need to input PIN or PUK.\n");
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_PINCODE_ERR);
					return;
				}
				else if(sim_state != 1){
					TRACE_PT("3g end: SIM isn't ready.\n");
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_NONE);
					return;
				}
			}
		}

		if((!strcmp(modem_type, "tty") || !strcmp(modem_type, "mbim"))
				&& write_3g_ppp_conf(get_modemunit_by_type(wan_type))
				&& (fp = fopen(PPP_CONF_FOR_3G, "r")) != NULL){
			fclose(fp);

			// run as ppp proto.
			nvram_set(strcat_r(prefix, "proto", tmp), "pppoe");
#ifndef RTCONFIG_DUALWAN
			nvram_set(strcat_r(prefix, "dhcpenable_x", tmp), "1");
			nvram_set(strcat_r(prefix, "vpndhcp", tmp), "0");
			nvram_set(strcat_r(prefix, "dnsenable_x", tmp), "1");
#endif

			char *pppd_argv[] = { "/usr/sbin/pppd", "call", "3g", NULL};

			if(nvram_get_int("stop_conn_3g") != 1)
				_eval(pppd_argv, NULL, 0, NULL);
			else
				TRACE_PT("stop_conn_3g was set.\n");
		}
		// RNDIS, QMI interface: usbX, Beceem interface: usbbcm -> ethX, gct(mad)wimax: wimaxX.
		else{
			snprintf(wan_ifname, sizeof(wan_ifname), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
TRACE_PT("3g begin with %s.\n", wan_ifname);

			if(strlen(wan_ifname) <= 0){
#ifdef RTCONFIG_USB_BECEEM
				snprintf(usb_node, sizeof(usb_node), "%s", nvram_safe_get("usb_modem_act_path"));
				if(strlen(usb_node) <= 0)
					return;

				if(get_path_by_node(usb_node, port_path, 8) != NULL){
					snprintf(nvram_name, sizeof(nvram_name), "usb_path%s", port_path);
					TRACE_PT("RNDIS/Beceem: start_wan_if.\n");

					if(!strcmp(nvram_safe_get(nvram_name), "modem")){
						snprintf(nvram_name, sizeof(nvram_name), "usb_path%s_vid", port_path);
						uvid = strtoul(nvram_safe_get(nvram_name), NULL, 16);
						snprintf(nvram_name, sizeof(nvram_name), "usb_path%s_pid", port_path);
						upid = strtoul(nvram_safe_get(nvram_name), NULL, 16);

						if(is_samsung_dongle(1, uvid, upid)){
							modprobe("tun");
							sleep(1);

							xstart("madwimax");
						}
						else if(is_gct_dongle(1, uvid, upid)){
							modprobe("tun");
							sleep(1);

							write_gct_conf();

							xstart("gctwimax", "-C", WIMAX_CONF);
						}
					}
				}
#endif

				return;
			}

#define MAX_TRY_IFUP 3
			for (i = 0; i < MAX_TRY_IFUP; i++) {
				if (_ifconfig_get(wan_ifname, &flags, NULL, NULL, NULL, &mtu) != 0) {
					TRACE_PT("Couldn't read the flags of %s!\n", wan_ifname);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}

#ifdef SET_USB_MODEM_MTU_ETH
				modem_mtu = nvram_get_int("modem_mtu");
				mtu = (modem_mtu >= 576 && modem_mtu < mtu) ? modem_mtu : 0;
				if ((flags & IFF_UP) && !mtu)
					break;
				else if(i == (MAX_TRY_IFUP-1)){
					TRACE_PT("Interface %s couldn't be up!\n", wan_ifname);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}

				_ifconfig(wan_ifname, flags | IFUP, NULL, NULL, NULL, mtu);
#else
				if ((flags & IFF_UP))
					break;
				else if(i == (MAX_TRY_IFUP-1)){
					TRACE_PT("Interface %s couldn't be up!\n", wan_ifname);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}

				ifconfig(wan_ifname, flags | IFUP, NULL, NULL);
#endif
				if (strcmp(modem_type, "gobi") == 0)
					continue;

				TRACE_PT("%s: wait %s(%s) be up, %d second...!\n", __FUNCTION__, modem_type, wan_ifname, i+1);
				sleep(1);
			}

			// run as dhcp proto.
			nvram_set(strcat_r(prefix, "proto", tmp), "dhcp");
			nvram_set(strcat_r(prefix, "dhcpenable_x", tmp), "1");
			nvram_set(strcat_r(prefix, "dnsenable_x", tmp), "1");

			// Android phone, RNDIS, QMI interface, Gobi.
			if(!strncmp(wan_ifname, "usb", 3) || !strncmp(wan_ifname, "wwan", 4)
					// RNDIS devices should always be named "lte%d" in LTQ platform
					|| !strncmp(wan_ifname, "lte", 3)
					){
				if(nvram_get_int("stop_conn_3g") != 1){
#ifdef RTCONFIG_TCPDUMP
					char *tcpdump_argv[] = { "/usr/sbin/tcpdump", "-i", wan_ifname, "-nnXw", "/tmp/udhcpc.pcap", NULL};

					if(nvram_get_int("dhcp_dump")){
						_eval(tcpdump_argv, NULL, 0, &pid);
						sleep(2);
					}
#endif
					dbG("start udhcpc(%d): %s.\n", unit, wan_ifname);
					start_udhcpc(wan_ifname, unit, &pid);
				}
				else
					TRACE_PT("stop_conn_3g was set.\n");
			}
			// Beceem dongle, ASIX USB to RJ45 converter, Huawei E353.
			else if(!strncmp(wan_ifname, "eth", 3)){
#ifdef RTCONFIG_USB_BECEEM
				write_beceem_conf(wan_ifname);
#endif

				if(nvram_get_int("stop_conn_3g") != 1){
					snprintf(usb_node, sizeof(usb_node), "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
					if(strlen(usb_node) <= 0)
						return;

					if(get_path_by_node(usb_node, port_path, 8) == NULL)
						return;

					snprintf(nvram_name, sizeof(nvram_name), "usb_path%s_act", port_path);

					if(!strcmp(nvram_safe_get(nvram_name), wan_ifname))
						start_udhcpc(wan_ifname, unit, &pid);

#ifdef RTCONFIG_USB_BECEEM
					if(strlen(nvram_safe_get(nvram_name)) <= 0){
						char buf[128];

						snprintf(buf, sizeof(buf), "wimaxd -c %s", WIMAX_CONF);
						TRACE_PT("%s: cmd=%s.\n", __FUNCTION__, buf);
						system(buf);
						sleep(3);

						TRACE_PT("%s: cmd=wimaxc search.\n", __FUNCTION__);
						system("wimaxc search");
						TRACE_PT("%s: sleep 10 seconds.\n", __FUNCTION__);
						sleep(10);

						TRACE_PT("%s: cmd=wimaxc connect.\n", __FUNCTION__);
						system("wimaxc connect");
					}
#endif
				}
				else
					TRACE_PT("stop_conn_3g was set.\n");
			}
#ifdef RTCONFIG_USB_BECEEM
			else if(!strncmp(wan_ifname, "wimax", 5)){
				if(nvram_get_int("stop_conn_3g") != 1)
					start_udhcpc(wan_ifname, unit, &pid);
				else
					TRACE_PT("stop_conn_3g was set.\n");
			}
#endif
		}

		TRACE_PT("3g end.\n");
		return;
	}
	else
#endif
	if (dualwan_unit__nonusbif(unit)) {
		convert_wan_nvram(prefix, unit);

		/* make sure the connection exists and is enabled */
		snprintf(wan_ifname, sizeof(wan_ifname), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));

		if(strlen(wan_ifname) <= 0)
			return;

		snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));
		if(!strcmp(wan_proto, "disabled")){
			update_wan_state(prefix, WAN_STATE_DISABLED, 0);
			return;
		}

		/* Set i/f hardware address before bringing it up */
		if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
			return;
		}

		strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

		/* Since WAN interface may be already turned up (by vlan.c),
			if WAN hardware address is specified (and different than the current one),
			we need to make it down for synchronizing hwaddr. */
		if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
			close(s);
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
			return;
		}

#ifdef RTCONFIG_IPV6
		/* Enable wired IPv6 interface */
		int need_linklocal_addr = 0;
		switch (get_ipv6_service_by_unit(unit)) {
		case IPV6_NATIVE_DHCP:
		case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (!(strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
			      nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))) {
			        enable_ipv6(wan_ifname);
				need_linklocal_addr = 1;
				break;
			}
			/* fall through */
		default:
			disable_ipv6(wan_ifname);
		}
#endif

		ether_atoe((const char *) nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), (unsigned char *) eabuf);
		if ((bcmp(eabuf, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN)))
		{
			/* current hardware address is different than user specified */
			ifconfig(wan_ifname, 0, NULL, NULL);
		}

		/* Configure i/f only once, specially for wireless i/f shared by multiple connections */
		if (ioctl(s, SIOCGIFFLAGS, &ifr)) {
			close(s);
			update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
			return;
		}

		if (!(ifr.ifr_flags & IFF_UP)) {
			fprintf(stderr, "** wan_ifname: %s is NOT UP\n", wan_ifname);

			/* Sync connection nvram address and i/f hardware address */
			memset(ifr.ifr_hwaddr.sa_data, 0, ETHER_ADDR_LEN);

			if (nvram_match(strcat_r(prefix, "hwaddr", tmp), "") ||
			    !ether_atoe((const char *) nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), (unsigned char *) ifr.ifr_hwaddr.sa_data) ||
			    !memcmp(ifr.ifr_hwaddr.sa_data, "\0\0\0\0\0\0", ETHER_ADDR_LEN)) {
				if (ioctl(s, SIOCGIFHWADDR, &ifr)) {
					fprintf(stderr, "ioctl fail. continue\n");
					close(s);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}
				nvram_set(strcat_r(prefix, "hwaddr", tmp), ether_etoa((unsigned char *) ifr.ifr_hwaddr.sa_data, eabuf));
			}
#if !(defined(RTCONFIG_DETWAN) && defined(RTCONFIG_ETHBACKHAUL))		// not to change MAC
			else {
				ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
				ioctl(s, SIOCSIFHWADDR, &ifr);
			}
#endif	/* ! RTCONFIG_ETHBACKHAUL */

			/* Bring up i/f */
			ifconfig(wan_ifname, IFUP, NULL, NULL);
		}
		close(s);

#ifdef RTCONFIG_IPV6
		/* Reset linklocal address if necessary after interface is up */
		if (need_linklocal_addr && !with_ipv6_linklocal_addr(wan_ifname)) {
			reset_ipv6_linklocal_addr(wan_ifname, 0);
			enable_ipv6(wan_ifname);
		}
#endif

		/* Set initial QoS mode again now that WAN port is ready. */
#ifdef CONFIG_BCMWL5
		set_et_qos_mode();
#endif

#ifdef RTCONFIG_DUALWAN
		pppoerelay_unit = wan_primary_ifunit();
		if (nvram_match("wans_mode", "lb") && get_nr_wan_unit() > 1)
			pppoerelay_unit = nvram_get_int("pppoerelay_unit");
		if (unit == pppoerelay_unit)
			start_pppoe_relay(wan_ifname);
#else
		if (unit == wan_primary_ifunit())
			start_pppoe_relay(wan_ifname);
#endif

		enable_ip_forward();

		update_wan_state(prefix, WAN_STATE_CONNECTING, 0);

		/*
		 * Configure PPPoE connection. The PPPoE client will run
		 * ip-up/ip-down scripts upon link's connect/disconnect.
		 */
		if (strcmp(wan_proto, "pppoe") == 0 ||
		    strcmp(wan_proto, "pptp") == 0 ||
		    strcmp(wan_proto, "l2tp") == 0) 	// oleg patch
		{
			char ipaddr[16];
			char netmask[16];
			int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
			int demand = nvram_get_int(strcat_r(prefix, "pppoe_idletime", tmp)) &&
					strcmp(wan_proto, "l2tp");	/* L2TP does not support idling */
#if defined(RTCONFIG_PORT_BASED_VLAN) || defined(RTCONFIG_TAGGED_BASED_VLAN)
			char ip_mask[sizeof("192.168.100.200/255.255.255.255XXX")];
#endif

			snprintf(ipaddr, sizeof(ipaddr), "%s", nvram_safe_get(strcat_r(prefix, "xipaddr", tmp)));
			snprintf(netmask, sizeof(netmask), "%s", nvram_safe_get(strcat_r(prefix, "xnetmask", tmp)));

			/* update demand option */
			nvram_set_int(strcat_r(prefix, "pppoe_demand", tmp), demand);

			if (dhcpenable == 0 &&
			    inet_equal(ipaddr, netmask,
				       nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"))) {
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
				return;
			}

#if defined(RTCONFIG_PORT_BASED_VLAN) || defined(RTCONFIG_TAGGED_BASED_VLAN)
			/* If return value of test_and_get_free_char_network() is 1 and
			 * we got different IP/netmask from it, the WAN IP/netmask conflicts with known networks.
			 */
			if (!dhcpenable) {
				snprintf(ip_mask, sizeof(ip_mask), "%s/%s", ipaddr, netmask);
				if (test_and_get_free_char_network(7, ip_mask, EXCLUDE_NET_ALL_EXCEPT_LAN_VLAN) == 1) {
					logmessage("start_wan_if", "%s, %s/%s conflicts with known networks",
						wan_proto, ipaddr, netmask);
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
					return;
				}
			}
#endif
#if defined(RTCONFIG_COOVACHILLI)
			if (!dhcpenable) {
				restart_coovachilli_if_conflicts(ipaddr, netmask);
			}
#endif
			/* Bring up WAN interface */
			ifconfig(wan_ifname, IFUP, ipaddr, netmask);

			/* Increase WAN interface's MTU to allow pppoe MTU/MRU over 1492 (with 8 byte overhead) */
			if (strcmp(wan_proto, "pppoe") == 0) {
				/* Compute maximum required MTU by taking the maximum of the pppoe MRU and MTU values */
				int mru = nvram_get_int(strcat_r(prefix, "pppoe_mru", tmp));
				mtu = nvram_get_int(strcat_r(prefix, "pppoe_mtu", tmp));
				if (mru > mtu)
					mtu = mru;
				if (mtu > 1492) {
					if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
						/* First set parent device if vlan was configured */
						strncpy(ifv.device1, wan_ifname, IFNAMSIZ);
						ifv.cmd = GET_VLAN_REALDEV_NAME_CMD;
						if (ioctl(s, SIOCGIFVLAN, &ifv) >= 0) {
							strncpy(ifr.ifr_name, ifv.u.device2, IFNAMSIZ);
							ifr.ifr_mtu = mtu + 8;
							if (ioctl(s, SIOCSIFMTU, &ifr)) {
								perror(wan_ifname);
								logmessage("start_wan_if()", "Error setting MTU on %s to %d", ifv.u.device2, mtu);
							}
						}

						/* Set WAN device */
						strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);
						ifr.ifr_mtu = mtu + 8;
						if (ioctl(s, SIOCSIFMTU, &ifr)) {
							perror(wan_ifname);
							logmessage("start_wan_if()", "Error setting MTU on %s to %d", wan_ifname, mtu);
						}
						close(s);
					}
				}
			}

			/* launch dhcp client and wait for lease forawhile */
			if (dhcpenable) {
				start_udhcpc(wan_ifname, unit,
					(strcmp(wan_proto, "pppoe") == 0) ? &pid : NULL);
			} else {
				char gateway[16];

				snprintf(gateway, sizeof(gateway), "%s", nvram_safe_get(strcat_r(prefix, "xgateway", tmp)));

				/* start firewall */
// TODO: handle different lan_ifname
				start_firewall(unit, 0);

				/* setup static wan routes via physical device */
				add_routes(prefix, "mroute", wan_ifname);

				/* and set default route if specified with metric 1 */
				if (inet_addr_(gateway) != INADDR_ANY &&
				    !nvram_match(strcat_r(prefix, "heartbeat_x", tmp), "")) {
					in_addr_t mask = inet_addr(netmask);

					/* the gateway is out of the local network */
					if ((inet_addr(gateway) & mask) != (inet_addr(ipaddr) & mask))
						route_add(wan_ifname, 2, gateway, NULL, "255.255.255.255");

					/* default route via default gateway */
					route_add(wan_ifname, 2, "0.0.0.0", gateway, "0.0.0.0");
				}

				/* update resolv.conf */
				update_resolvconf();

				/* start multicast router on Static+VPN physical interface */
				if (unit == wan_primary_ifunit())
					start_igmpproxy(wan_ifname);
			}

#ifdef HND_ROUTER
			if (!strcmp(wan_proto, "pptp") && !module_loaded("pptp"))
				modprobe("pptp");
#endif

#if defined(RTCONFIG_TCPDUMP) && defined(RTCONFIG_SOC_IPQ40XX) && defined(RTCONFIG_PSISTLOG)
			{
				char *tcpdump_argv[] = { "/usr/sbin/tcpdump", "-i", wan_ifname, "-nnXw", "/jffs/pppoe.pcap", NULL};
				_eval(tcpdump_argv, NULL, 0, &pid);
				sleep(1);
			}
#endif	/* RTCONFIG_TCPDUMP && RTCONFIG_SOC_IPQ40XX && RTCONFIG_PSISTLOG */

			/* launch pppoe client daemon */
			start_pppd(unit);

			/* ppp interface name is referenced from this point
			 * after pppd start before ip-pre-up it will be empty */
			snprintf(wan_ifname, sizeof(wan_ifname), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)));

			/* Pretend that the WAN interface is up */
			if (demand) {
				int timeout = 5;

				/* Wait for pppx to be created */
				while (timeout--) {
					/* ppp interface name is re-referenced from this point */
					snprintf(wan_ifname, sizeof(wan_ifname), "%s", nvram_safe_get(strcat_r(prefix, "pppoe_ifname", tmp)));

					if(strlen(wan_ifname) > 0 && ifconfig(wan_ifname, IFUP, NULL, NULL) == 0)
						break;
					_dprintf("%s: wait interface %s up at %d seconds...\n", __FUNCTION__, wan_ifname, timeout);
					sleep(1);
				}

				if(strlen(wan_ifname) <= 0){
					_dprintf("%s: no interface of wan_unit %d.\n", __FUNCTION__, unit);
					return;
				}

				/* Retrieve IP info */
				if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
					update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_SYSTEM_ERR);
					return;
				}
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);

				/* Set temporary IP address */
				if (ioctl(s, SIOCGIFADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "ipaddr", tmp), inet_ntoa(sin_addr(&ifr.ifr_addr)));
				nvram_set(strcat_r(prefix, "netmask", tmp), "255.255.255.255");

				/* Set temporary P-t-P address */
				if (ioctl(s, SIOCGIFDSTADDR, &ifr))
					perror(wan_ifname);
				nvram_set(strcat_r(prefix, "gateway", tmp), inet_ntoa(sin_addr(&ifr.ifr_dstaddr)));

				close(s);

				/*
				 * Preset routes so that traffic can be sent to proper pppx even before
				 * the link is brought up.
				 */
				preset_wan_routes(wan_ifname);

				/* Trigger it up to obtain PPP DNS early */
				start_demand_ppp(unit, 0);
			}
		}
		/*
		 * Configure DHCP connection. The DHCP client will run
		 * 'udhcpc bound'/'udhcpc deconfig' upon finishing IP address
		 * renew and release.
		 */
		else if (strcmp(wan_proto, "dhcp") == 0)
		{
#if defined(RTCONFIG_AMAS) && defined(RTCONFIG_ETHOBD)
			if (nvram_get_int("x_Setting") == 0) {
				if(strcmp(wan_ifname, nvram_safe_get("eth_ifnames"))) {
					dbG("ifup:%s\n", nvram_safe_get("eth_ifnames"));
					ifconfig(nvram_safe_get("eth_ifnames"), IFUP, NULL, NULL);
				}
			}
#endif
			/* Bring up WAN interface */
			dbG("ifup:%s\n", wan_ifname);
			ifconfig(wan_ifname, IFUP, NULL, NULL);

			/* MTU */
			if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				mtu = nvram_get_int(strcat_r(prefix, "mtu", tmp));
				if (mtu < 576)
					mtu = 576;

				if (mtu > 9000)
					mtu = 9000;	// Limit to a sane value

				ifr.ifr_mtu = mtu;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);
				if (ioctl(s, SIOCSIFMTU, &ifr)) {
					perror(wan_ifname);
					logmessage("start_wan_if()","Error setting MTU on %s to %d", wan_ifname, mtu);
				}
				close(s);
			}

			/* Start pre-authenticator */
			dbG("start auth:%d\n", unit);
			start_auth(unit, 0);

#ifdef RTCONFIG_DSL
			nvram_set(strcat_r(prefix, "clientid_type", tmp), nvram_safe_get("dslx_dhcp_clientid_type"));
			nvram_set(strcat_r(prefix, "clientid", tmp), nvram_safe_get("dslx_dhcp_clientid"));
			nvram_set(strcat_r(prefix, "vendorid", tmp), nvram_safe_get("dslx_dhcp_vendorid"));
			nvram_set(strcat_r(prefix, "hostname", tmp), nvram_safe_get("dslx_dhcp_hostname"));
#endif
			/* Start dhcp daemon */
			dbG("start udhcpc:%s, %d\n", wan_ifname, unit);
			start_udhcpc(wan_ifname, unit, &pid);
		}
		/* Configure static IP connection. */
		else if (strcmp(wan_proto, "static") == 0)
		{
#if defined(RTCONFIG_PORT_BASED_VLAN) || defined(RTCONFIG_TAGGED_BASED_VLAN)
			char ip_mask[sizeof("192.168.100.200/255.255.255.255XXX")];
#endif

			if (inet_equal(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)), nvram_safe_get(strcat_r(prefix, "netmask", tmp)),
				       nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"))) {
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
				return;
			}

#if defined(RTCONFIG_PORT_BASED_VLAN) || defined(RTCONFIG_TAGGED_BASED_VLAN)
			/* If return value of test_and_get_free_char_network() is 1 and
			 * we got different IP/netmask from it, the WAN IP/netmask conflicts with known networks.
			 */
			snprintf(ip_mask, sizeof(ip_mask), "%s/%s",
				nvram_pf_safe_get(prefix, "ipaddr"), nvram_pf_safe_get(prefix, "netmask"));
			if (test_and_get_free_char_network(7, ip_mask, EXCLUDE_NET_ALL_EXCEPT_LAN_VLAN) == 1) {
				logmessage("start_wan_if", "%s, %s/%s conflicts with known networks",
					wan_proto, nvram_pf_safe_get(prefix, "ipaddr"), nvram_pf_safe_get(prefix, "netmask"));
				update_wan_state(prefix, WAN_STATE_STOPPED, WAN_STOPPED_REASON_INVALID_IPADDR);
				return;
			}
#endif
			restart_coovachilli_if_conflicts(nvram_pf_get(prefix, "ipaddr"), nvram_pf_get(prefix, "netmask"));

			/* Assign static IP address to i/f */
			ifconfig(wan_ifname, IFUP,
					nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
					nvram_safe_get(strcat_r(prefix, "netmask", tmp)));

			/* Start pre-authenticator */
			start_auth(unit, 0);

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
			if (wan_type == WANS_DUALWAN_IF_DSL)
#endif
			(
				if (nvram_match("dsl0_proto", "ipoa"))
					start_ipoa();
#endif

			/* MTU */
			if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
				mtu = nvram_get_int(strcat_r(prefix, "mtu", tmp));
				if (mtu < 576)
					mtu = 576;

				if (mtu > 9000)
					mtu = 9000;     // Limit to a sane value

				ifr.ifr_mtu = mtu;
				strncpy(ifr.ifr_name, wan_ifname, IFNAMSIZ);
				if (ioctl(s, SIOCSIFMTU, &ifr)) {
					perror(wan_ifname);
					logmessage("start_wan_if()","Error setting MTU on %s to %d", wan_ifname, mtu);
				}
				close(s);
			}

			/* We are done configuration */
			wan_up(wan_ifname);
		}
	}
	else
#ifdef RTCONFIG_DUALWAN
		_dprintf("%s(): Cound't find the type(%d) of unit(%d)!!!\n", __FUNCTION__, wan_type, unit);
#else
		_dprintf("%s(): Cound't find the wan(%d)!!!\n", __FUNCTION__, unit);
#endif

	_dprintf("%s(): End.\n", __FUNCTION__);

#ifdef RTCONFIG_IPSEC
	if(nvram_get_int("ipsec_server_enable") || nvram_get_int("ipsec_client_enable")){
		rc_ipsec_config_init();
		start_dnsmasq();
	}
#endif
}

void
stop_wan_if(int unit)
{
#if defined(RTCONFIG_DSL) && defined(RTCONFIG_DUALWAN)
	int wan_type;
#endif
	char wan_ifname[16];
	char tmp[100], prefix[32];
	char wan_proto[16], active_proto[16];
#ifdef RTCONFIG_USB_BECEEM
	int i;
	unsigned int uvid, upid;
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	char *const modem_argv[] = {"/usr/sbin/modem_stop.sh", NULL};
	int modem_unit;
	char tmp2[100], prefix2[32];
	char env_unit[32];
#endif
	int end_wan_sbstate = WAN_STOPPED_REASON_NONE;

	snprintf(prefix, sizeof(prefix), "wan%d_", unit);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	if(get_wan_sbstate(unit) == WAN_STOPPED_REASON_DATALIMIT)
		end_wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;
#endif

	update_wan_state(prefix, WAN_STATE_STOPPING, end_wan_sbstate);

	/* Backup active wan_proto for later restore, if it have been updated by ui */
	snprintf(active_proto, sizeof(active_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));

	/* Set previous wan_proto as active */
	snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto_t", tmp)));
	if(strlen(wan_proto) > 0 && strcmp(active_proto, wan_proto) != 0){
		stop_iQos(); // clean all tc rules
		_dprintf("%s %sproto_t=%s\n", __FUNCTION__, prefix, wan_proto);
		nvram_set(strcat_r(prefix, "proto", tmp), wan_proto);
		nvram_unset(strcat_r(prefix, "proto_t", tmp));
	}

	// Handel for each interface
	if(unit == wan_primary_ifunit()){
		killall_tk("stats");
#ifndef RTCONFIG_NTPD
		killall_tk("ntpclient");
#endif

		/* Shutdown and kill all possible tasks */
#if 0
		killall_tk("ip-up");
		killall_tk("ip-down");
		killall_tk("ip-pre-up");
#ifdef RTCONFIG_IPV6
		killall_tk("ipv6-up");
		killall_tk("ipv6-down");
#endif
		killall_tk("auth-fail");
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
		if (nvram_get_int("switch_stb_x") > 6 &&
		    nvram_match("switch_wantag", "movistar") && unit == WAN_UNIT_IPTV)
#endif
		stop_igmpproxy();
	}

#ifdef RTCONFIG_VPNC
	/* Stop VPN client */
	stop_vpnc();
#endif

	/* Stop l2tp */
	if (strcmp(wan_proto, "l2tp") == 0) {
		kill_pidfile_tk("/var/run/l2tpd.pid");
		usleep(1000*10000);
	}

	/* Stop pppd */
	stop_pppd(unit);

	/* Stop post-authenticator */
	stop_auth(unit, 1);

	/* Stop dhcp client */
	stop_udhcpc(unit);

	/* Stop pre-authenticator */
	stop_auth(unit, 0);

	/* Bring down WAN interfaces */
	// Does it have to?
	snprintf(wan_ifname, sizeof(wan_ifname), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
#ifdef RTCONFIG_USB_MODEM
	if(strncmp(wan_ifname, "/dev/tty", 8))
#endif
	{
		if(strlen(wan_ifname) > 0){
#ifdef RTCONFIG_SOC_IPQ40XX
			if (strcmp(wan_ifname, "eth0") == 0)
				ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
			else
#endif
			ifconfig(wan_ifname, 0, NULL, NULL);
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
			if(!strncmp(wan_ifname, "eth", 3) || !strncmp(wan_ifname, "vlan", 4))
				ifconfig(wan_ifname, IFUP, "0.0.0.0", NULL);
#endif
		}
	}

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_DUALWAN
	wan_type = get_dualwan_by_unit(unit);
	if(wan_type == WANS_DUALWAN_IF_DSL)
#endif
		if (nvram_match("dsl0_proto", "ipoa"))
		{
			stop_ipoa();
		}
#endif

#ifdef RTCONFIG_USB_MODEM
	if (dualwan_unit__usbif(unit)) {
#ifdef RTCONFIG_USB_BECEEM
		if(is_usb_modem_ready(get_dualwan_by_unit(unit)) == 1){
			if(pids("wimaxd"))
				eval("wimaxc", "disconnect");
		}

		if(pids("wimaxd")){
			killall("wimaxd", SIGTERM);
			killall("wimaxd", SIGUSR1);
		}

		uvid = nvram_get_int("usb_modem_act_vid");
		upid = nvram_get_int("usb_modem_act_pid");

		if(is_samsung_dongle(1, uvid, upid)){
			i = 0;
			while(i < 3){
				if(pids("madwimax")){
					killall_tk("madwimax");
					sleep(1);

					++i;
				}
				else
					break;
			}

			modprobe_r("tun");

			nvram_set(strcat_r(prefix, "ifname", tmp), "");
		}
		else if(is_gct_dongle(1, uvid, upid)){
			i = 0;
			while(i < 3){
				if(pids("gctwimax")){
					killall_tk("gctwimax");
					sleep(1);

					++i;
				}
				else
					break;
			}
			unlink(WIMAX_CONF);

			modprobe_r("tun");

			nvram_set(strcat_r(prefix, "ifname", tmp), "");
		}
#endif	/* RTCONFIG_USB_BECEEM */

#ifdef RTCONFIG_INTERNAL_GOBI
		modem_unit = get_modemunit_by_type(get_dualwan_by_unit(unit));
		usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));
		snprintf(env_unit, sizeof(env_unit), "unit=%d", modem_unit);

		if(!strcmp(nvram_safe_get(strcat_r(prefix2, "act_type", tmp2)), "gobi")){
			putenv(env_unit);
			_eval(modem_argv, ">>/tmp/usb.log", 0, NULL);
			unsetenv("unit");
		}
#endif
	}

#ifdef RTCONFIG_GETREALIP
#ifdef RTCONFIG_DUALWAN
	if(nvram_invmatch("wans_mode", "lb"))
#endif
	{
		nvram_set(strcat_r(prefix, "realip_state", tmp), "0");
		nvram_set(strcat_r(prefix, "realip_ip", tmp), "");
	}
#endif

	if(dualwan_unit__usbif(unit))
		update_wan_state(prefix, WAN_STATE_INITIALIZING, end_wan_sbstate);
	else
#endif // RTCONFIG_USB_MODEM
		update_wan_state(prefix, WAN_STATE_STOPPED, end_wan_sbstate);

	// wait for release finished ?
	if (!g_reboot)
		sleep(2);

	/* Restore active wan_proto value */
	_dprintf("%s %sproto=%s\n", __FUNCTION__, prefix, active_proto);
	nvram_set(strcat_r(prefix, "proto", tmp), active_proto);
}

int update_resolvconf(void)
{
	FILE *fp, *fp_servers;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char *wan_dns, *wan_domain, *next;
	char wan_dns_buf[INET6_ADDRSTRLEN*3 + 3], wan_domain_buf[256];
	char domain[64], *next_domain;
	int unit, lock;
#ifdef RTCONFIG_YANDEXDNS
	int yadns_mode = nvram_get_int("yadns_enable_x") ? nvram_get_int("yadns_mode") : YADNS_DISABLED;
#endif
#ifdef RTCONFIG_DNSPRIVACY
	int dnspriv_enable = nvram_get_int("dnspriv_enable");
#endif
#ifdef RTCONFIG_OPENVPN
        int dnsmode;
#endif
#ifdef RTCONFIG_DUALWAN
	int primary_unit = wan_primary_ifunit();
#endif

	lock = file_lock("resolv");

	if (!(fp = fopen("/tmp/resolv.conf", "w+"))) {
		perror("/tmp/resolv.conf");
		goto error;
	}
	if (!(fp_servers = fopen("/tmp/resolv.dnsmasq", "w+"))) {
		perror("/tmp/resolv.dnsmasq");
		fclose(fp);
		goto error;
	}

/* Add DNS if no VPN client is globally set to exclusive */
#ifdef RTCONFIG_OPENVPN
	dnsmode = get_max_dnsmode();
	if (dnsmode != OVPN_DNSMODE_EXCLUSIVE)
#endif
	{
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
			char *wan_xdns, *wan_xdomain;
			char wan_xdns_buf[sizeof("255.255.255.255 ")*2], wan_xdomain_buf[256];

#ifdef RTCONFIG_DUALWAN
			if (unit != primary_unit && nvram_invmatch("wans_mode", "lb"))
				continue;
#endif
			if (!is_phy_connect(unit))
				continue;

			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_dns = nvram_safe_get_r(strcat_r(prefix, "dns", tmp), wan_dns_buf, sizeof(wan_dns_buf));
			wan_xdns = nvram_safe_get_r(strcat_r(prefix, "xdns", tmp), wan_xdns_buf, sizeof(wan_xdns_buf));

			if (!*wan_dns && !*wan_xdns)
				continue;

#ifdef NORESOLV /* dnsmasq uses no resolv.conf */
			foreach(tmp, (*wan_dns ? wan_dns : wan_xdns), next)
				fprintf(fp, "nameserver %s\n", tmp);

			do {
#ifdef RTCONFIG_YANDEXDNS
				if (yadns_mode != YADNS_DISABLED)
					break;
#endif
#ifdef RTCONFIG_DNSPRIVACY
				if (dnspriv_enable)
					break;
#endif
#ifdef RTCONFIG_DUALWAN
				/* Skip not fully connected WANs in LB mode */
				if (unit != primary_unit && nvram_match("wans_mode", "lb") && !*wan_dns)
					break;
#endif
				foreach(tmp, (*wan_dns ? wan_dns : wan_xdns), next)
					fprintf(fp_servers, "server=%s\n", tmp);
			} while (0);
#endif

			wan_domain = nvram_safe_get_r(strcat_r(prefix, "domain", tmp), wan_domain_buf, sizeof(wan_domain_buf));
			foreach (tmp, wan_dns, next) {
				foreach(domain, wan_domain, next_domain)
					fprintf(fp_servers, "server=/%s/%s\n", domain, tmp);
#ifdef RTCONFIG_YANDEXDNS
				if (yadns_mode != YADNS_DISABLED)
					fprintf(fp_servers, "server=/%s/%s\n", "local", tmp);
#endif
			}

			wan_xdomain = nvram_safe_get_r(strcat_r(prefix, "xdomain", tmp), wan_xdomain_buf, sizeof(wan_xdomain_buf));
			foreach (tmp, wan_xdns, next) {
				int new = (find_word(wan_dns, tmp) == NULL);
				foreach (domain, wan_xdomain, next_domain) {
					if (new || find_word(wan_domain, domain) == NULL)
						fprintf(fp_servers, "server=/%s/%s\n", domain, tmp);
				}
#ifdef RTCONFIG_YANDEXDNS
				if (yadns_mode != YADNS_DISABLED && new)
					fprintf(fp_servers, "server=/%s/%s\n", "local", tmp);
#endif
			}
		}
	}

/* Add DNS from VPN clients - add at the end since config is read backward by dnsmasq */
#ifdef RTCONFIG_OPENVPN
	write_ovpn_resolv_dnsmasq(fp_servers);
#endif

#ifdef RTCONFIG_YANDEXDNS
	if (yadns_mode != YADNS_DISABLED) {
		char *server[2];
		int count = get_yandex_dns(AF_INET, yadns_mode, server, sizeof(server)/sizeof(server[0]));
		for (unit = 0; unit < count; unit++) {
			fprintf(fp_servers, "server=%s\n", server[unit]);
			fprintf(fp_servers, "server=%s#%u\n", server[unit], YADNS_DNSPORT);
		}
	} else
#endif
#ifdef RTCONFIG_DNSPRIVACY
	if (dnspriv_enable) {
		if (!nvram_get_int("dns_local_cache"))
			fprintf(fp, "nameserver %s\n", "127.0.1.1");
		fprintf(fp_servers, "server=%s\n", "127.0.1.1");
	} else
#endif
	;

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled() && is_routing_enabled()) {
		struct in6_addr addr;

	/* TODO: Skip unconnected wan */

		switch (get_ipv6_service()) {
		case IPV6_NATIVE_DHCP:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (nvram_get_int(ipv6_nvname("ipv6_dnsenable"))) {
				wan_dns = nvram_safe_get_r(ipv6_nvname("ipv6_get_dns"), wan_dns_buf, sizeof(wan_dns_buf));
				wan_domain = nvram_safe_get_r(ipv6_nvname("ipv6_get_domain"), wan_domain_buf, sizeof(wan_domain_buf));
				break;
			}
			/* fall through */
		default:
			wan_dns = strcpy(wan_dns_buf, "");
			wan_domain = "";
			for (unit = 1; unit <= 3; unit++) {
				snprintf(tmp, sizeof(tmp), "ipv6_dns%d", unit);
				if (*wan_dns_buf)
					strlcat(wan_dns_buf, " ", sizeof(wan_dns_buf));
				strlcat(wan_dns_buf, nvram_safe_get(ipv6_nvname(tmp)), sizeof(wan_dns_buf));
			}
		}

		foreach(tmp, wan_dns, next) {
			if (inet_pton(AF_INET6, tmp, &addr) <= 0)
				continue;
			foreach(domain, wan_domain, next_domain)
				fprintf(fp_servers, "server=/%s/%s\n", domain, tmp);
#ifdef RTCONFIG_YANDEXDNS
			if (yadns_mode != YADNS_DISABLED) {
				fprintf(fp_servers, "server=/%s/%s\n", "local", tmp);
				continue;
			}
#endif
#ifdef RTCONFIG_DNSPRIVACY
			if (dnspriv_enable)
				continue;
#endif
			fprintf(fp_servers, "server=%s\n", tmp);
		}

#ifdef RTCONFIG_YANDEXDNS
		if (yadns_mode != YADNS_DISABLED) {
			char *server[2];
			int count = get_yandex_dns(AF_INET6, yadns_mode, server, sizeof(server)/sizeof(server[0]));
			for (unit = 0; unit < count; unit++) {
				fprintf(fp_servers, "server=%s\n", server[unit]);
				fprintf(fp_servers, "server=%s#%u\n", server[unit], YADNS_DNSPORT);
			}
		}
#endif
	}
#endif

	fclose(fp);
	fclose(fp_servers);
	file_unlock(lock);

#ifdef RTCONFIG_OPENVPN
	if (dnsmode == OVPN_DNSMODE_STRICT)
		start_dnsmasq();	// add strict-order
	else
#endif
		reload_dnsmasq();

	return 0;

error:
	file_unlock(lock);
	return -1;
}

#ifdef RTCONFIG_IPV6
void wan6_up(const char *wan_ifname)
{
	char addr6[INET6_ADDRSTRLEN + 4];
	struct in_addr addr4;
	struct in6_addr addr;
	char gateway[INET6_ADDRSTRLEN];
	int mtu, service = get_ipv6_service();

	if (!wan_ifname || (strlen(wan_ifname) <= 0) ||
		(service == IPV6_DISABLED))
		return;

	switch (service) {
	case IPV6_NATIVE_DHCP:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		ipv6_sysconf(wan_ifname, "accept_ra", 1);
		ipv6_sysconf(wan_ifname, "forwarding", 0);
		break;
	case IPV6_MANUAL:
		ipv6_sysconf(wan_ifname, "accept_ra", 0);
		ipv6_sysconf(wan_ifname, "forwarding", 1);
		break;
	case IPV6_6RD:
		update_6rd_info();
		break;
	}

	set_intf_ipv6_dad(wan_ifname, 0, 1);

	switch (service) {
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
		start_6relayd();
		/* fall through */
#endif
	case IPV6_NATIVE_DHCP:
		start_dhcp6c();

		if (nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp")) {
			strlcpy(gateway, nvram_safe_get(ipv6_nvname("ipv6_llremote")), sizeof(gateway));
			if (/* gateway && */ *gateway)
				_ipv6_route_add(wan_ifname, 0, "::/0", gateway, RTF_DEFAULT | RTF_ADDRCONF);
		}

		/* propagate ipv6 mtu */
		mtu = ipv6_getconf(wan_ifname, "mtu");
		if (mtu)
			ipv6_sysconf(nvram_safe_get("lan_ifname"), "mtu", mtu);
		break;

	case IPV6_MANUAL:
		if (nvram_match(ipv6_nvname("ipv6_ipaddr"), (char*)ipv6_router_address(NULL))) {
			dbG("WAN IPv6 address is the same as LAN IPv6 address!\n");
			break;
		}
		snprintf(addr6, sizeof(addr6), "%s/%d", nvram_safe_get(ipv6_nvname("ipv6_ipaddr")), nvram_get_int(ipv6_nvname("ipv6_prefix_len_wan")));
		eval("ip", "-6", "addr", "add", addr6, "dev", (char *)wan_ifname);
		eval("ip", "-6", "route", "del", "::/0");

		strlcpy(gateway, nvram_safe_get(ipv6_nvname("ipv6_gateway")), sizeof(gateway));
		if (/* gateway && */ *gateway) {
			eval("ip", "-6", "route", "add", gateway, "dev", (char *)wan_ifname, "metric", "1");
			eval("ip", "-6", "route", "add", "::/0", "via", gateway, "dev", (char *)wan_ifname, "metric", "1");
		} else if (nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp")) {
			strlcpy(gateway, nvram_safe_get(ipv6_nvname("ipv6_llremote")), sizeof(gateway));
			if (/* gateway && */ *gateway)
				_ipv6_route_add(wan_ifname, 0, "::/0", gateway, RTF_DEFAULT | RTF_ADDRCONF);
		}

		/* propagate ipv6 mtu */
		mtu = ipv6_getconf(wan_ifname, "mtu");
		if (mtu)
			ipv6_sysconf(nvram_safe_get("lan_ifname"), "mtu", mtu);

		/* workaround to update ndp entry for now */
		char *ping6_argv[] = {"ping6", "-c", "2", "-I", (char *)wan_ifname, "ff02::1", NULL};
		char *ping6_argv2[] = {"ping6", "-c", "2", gateway, NULL};
		pid_t pid;
		_eval(ping6_argv, NULL, 0, &pid);
		_eval(ping6_argv2, NULL, 0, &pid);
		break;

	case IPV6_6TO4:
	case IPV6_6IN4:
	case IPV6_6RD:
		stop_ipv6_tunnel();
		if (service == IPV6_6TO4) {
			int prefixlen = 16;
			int mask4size = 0;

			/* prefix */
			addr4.s_addr = 0;
			memset(&addr, 0, sizeof(addr));
			inet_aton(get_wanip(), &addr4);
			addr.s6_addr16[0] = htons(0x2002);
			prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, mask4size);
			//addr4.s_addr = htonl(0x00000001);
			//prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, (32 - 16));
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_prefix"), addr6);
			nvram_set_int(ipv6_nvname("ipv6_prefix_length"), prefixlen);

			/* address */
			addr.s6_addr16[7] |= htons(0x0001);
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_rtr_addr"), addr6);
		}
		else if (service == IPV6_6RD) {
			int prefixlen = nvram_get_int(ipv6_nvname("ipv6_6rd_prefixlen"));
			int masklen = nvram_get_int(ipv6_nvname("ipv6_6rd_ip4size"));

			/* prefix */
			addr4.s_addr = 0;
			memset(&addr, 0, sizeof(addr));
			inet_aton(get_wanip(), &addr4);
			inet_pton(AF_INET6, nvram_safe_get(ipv6_nvname("ipv6_6rd_prefix")), &addr);
			prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, masklen);
			//addr4.s_addr = htonl(0x00000001);
			//prefixlen = ipv6_mapaddr4(&addr, prefixlen, &addr4, (32 - 1));
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_prefix"), addr6);
			nvram_set_int(ipv6_nvname("ipv6_prefix_length"), prefixlen);

			/* address */
			addr.s6_addr16[7] |= htons(0x0001);
			inet_ntop(AF_INET6, &addr, addr6, sizeof(addr6));
			nvram_set(ipv6_nvname("ipv6_rtr_addr"), addr6);
		}
		start_ipv6_tunnel();

		/* propagate ipv6 mtu */
		mtu = ipv6_getconf(wan_ifname, "mtu");
		if (mtu)
			ipv6_sysconf(nvram_safe_get("lan_ifname"), "mtu", mtu);
		// FIXME: give it a few seconds for DAD completion
		sleep(2);
		break;
	}

#if 0
	start_ecmh(wan_ifname);
#endif
}

void wan6_down(const char *wan_ifname)
{
	set_intf_ipv6_dad(wan_ifname, 0, 0);
#if 0
	stop_ecmh();
#endif
#ifdef RTCONFIG_6RELAYD
	stop_6relayd();
#endif
	stop_dhcp6c();
	stop_ipv6_tunnel();

	update_resolvconf();
}

void start_wan6(void)
{
	// call wan6_up directly
	char tmp[100];
	char prefix[] = "wanXXXXXXXXXX_";
	char wan_proto[16];

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_primary_ifunit_ipv6());
	snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));

	switch (get_ipv6_service()) {
	case IPV6_NATIVE_DHCP:
	case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
		    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))
			break;
		/* fall though */
	default:
		wan6_up(get_wan6face());
		break;
	}
}

void stop_wan6(void)
{
	// call wan6_down directly
	wan6_down(get_wan6face());
}

#endif

/**
 * Append netdev to bled or remove netdev from bled.
 * @action:	append or move
 * 	0:	remove
 *  otherwise:	append
 * @wan_unit:
 * @wan_ifname:
 */
#ifdef RTCONFIG_BLINK_LED
static void adjust_netdev_if_of_wan_bled(int action, int wan_unit, char *wan_ifname)
{
	char *wan_gpio = "led_wan_gpio";
	int (*func)(const char *led_gpio, const char *ifname);

	if (wan_unit < 0 || wan_unit >= WAN_UNIT_MAX || !wan_ifname)
		return;

	if (action)
		func = append_netdev_bled_if;
	else
		func = remove_netdev_bled_if;
#if defined(RTCONFIG_WANPORT2)
	if (wan_unit == 1)
		wan_gpio = "led_wan2_gpio";
#endif
	if (dualwan_unit__usbif(wan_unit)) {
		func(wan_gpio, wan_ifname);
		return;
	}

	if (!(nvram_get_int("boardflags") & 0x100))
		return;

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
	/* Nothing to do. */
#else
	if (get_dualwan_by_unit(wan_unit) == WANS_DUALWAN_IF_LAN) {
		func(wan_gpio, wan_ifname);
	}
#endif
}
#endif

void
wan_up(const char *pwan_ifname)
{
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char prefix_x[sizeof("wanXXXXXXXXX_")];
	char wan_ifname[16];
	char wan_proto[16], gateway[16], dns[PATH_MAX];
	int wan_unit;
#if defined(RTCONFIG_USB_MODEM) && defined(RTCONFIG_INTERNAL_GOBI)
	int modem_unit;
	char tmp2[100], prefix2[32];
	char env_unit[32];
#endif
#ifdef RTCONFIG_LANTIQ
	char ppa_cmd[255] = {0};
#endif
	FILE *fp;
	int i=0;

	/* Value of pwan_ifname can be modfied after do_dns_detect */
	strlcpy(wan_ifname, pwan_ifname, 16);

	/* Figure out nvram variable name prefix for this i/f */
	if ((wan_unit = wan_ifunit(wan_ifname)) < 0)
	{
		/* called for dhcp+ppp */
		if ((wan_unit = wanx_ifunit(wan_ifname)) < 0)
			return;
	_dprintf("%s_x(%s)\n", __FUNCTION__, wan_ifname);

		snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
		snprintf(prefix_x, sizeof(prefix_x), "wan%d_x", wan_unit);

#ifdef RTCONFIG_IPV6
		snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));
		if (wan_unit == wan_primary_ifunit_ipv6()) {
			switch (get_ipv6_service()) {
			case IPV6_NATIVE_DHCP:
			case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
			case IPV6_PASSTHROUGH:
#endif
				if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
				    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))
					break;
				/* fall through */
			default:
				wan6_up(get_wan6face());
				break;
			}
		}
#endif

		start_firewall(wan_unit, 0);

		/* setup static wan routes via physical device */
		add_routes(prefix, "mroute", wan_ifname);

		/* and one supplied via DHCP */
		add_dhcp_routes(prefix_x, wan_ifname, 1);

		/* and default route with metric 1 */
		snprintf(gateway, sizeof(gateway), "%s", nvram_safe_get(strcat_r(prefix_x, "gateway", tmp)));
		if (inet_addr_(gateway) != INADDR_ANY) {
			char word[100], *next;
			in_addr_t addr = inet_addr(nvram_safe_get(strcat_r(prefix_x, "ipaddr", tmp)));
			in_addr_t mask = inet_addr(nvram_safe_get(strcat_r(prefix_x, "netmask", tmp)));

			/* the gateway is out of the local network */
			if ((inet_addr(gateway) & mask) != (addr & mask))
				route_add(wan_ifname, 2, gateway, NULL, "255.255.255.255");

			/* default route via default gateway */
			route_add(wan_ifname, 2, "0.0.0.0", gateway, "0.0.0.0");

			/* ... and to dns servers as well for demand ppp to work */
			if (nvram_get_int(strcat_r(prefix, "dnsenable_x", tmp))) {
				snprintf(dns, sizeof(dns), "%s", nvram_safe_get(strcat_r(prefix_x, "dns", tmp)));
				foreach(word, dns, next) {
					if ((inet_addr(word) != inet_addr(gateway)) &&
					    (inet_addr(word) & mask) != (addr & mask))
						route_add(wan_ifname, 2, word, gateway, "255.255.255.255");
				}
			}
		}

		update_resolvconf();

		/* start multicast router on DHCP+VPN physical interface */
		if (wan_unit == wan_primary_ifunit())
			start_igmpproxy(wan_ifname);

#ifdef RTCONFIG_LANTIQ
		disable_ppa_wan(wan_ifname);
#endif
		_dprintf("%s_x(%s): done.\n", __FUNCTION__, wan_ifname);

		return;
	}

	_dprintf("%s(%s)\n", __FUNCTION__, wan_ifname);

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);

	snprintf(wan_proto, sizeof(wan_proto), "%s", nvram_safe_get(strcat_r(prefix, "proto", tmp)));
	snprintf(gateway, sizeof(gateway), "%s", nvram_safe_get(strcat_r(prefix, "gateway", tmp)));
	if (inet_addr_(gateway) == INADDR_ANY)
		memset(gateway, 0, sizeof(gateway));

	/* Set default route to gateway if specified */
	if (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0) {
		/* the gateway is in the local network */
		if (strlen(gateway) > 0 &&
		    inet_addr_(gateway) != inet_addr_(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))) {
#ifdef RTCONFIG_MULTICAST_IPTV
			/* Rawny: delete gateway route in IPTV(movistar) case to enable QUAGGA */
			if (nvram_get_int("switch_stb_x") > 6 &&
			    !nvram_match("switch_wantag", "movistar"))
#endif
			route_add(wan_ifname, 0, gateway, NULL, "255.255.255.255");
		}
		/* replaced with add_multi_routes()
		route_add(wan_ifname, 0, "0.0.0.0", gateway, "0.0.0.0"); */
	}

	/* hack: avoid routing cycles, when both peer and server has the same IP */
	if (strcmp(wan_proto, "pptp") == 0 || strcmp(wan_proto, "l2tp") == 0) {
		/* delete gateway route as it's no longer needed */
		if (strlen(gateway) > 0)
			route_del(wan_ifname, 0, gateway, "0.0.0.0", "255.255.255.255");
	}

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);

	nvram_set(strcat_r(prefix, "gw_ifname", tmp), wan_ifname);

	/* setup static wan routes via physical device */
	if (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0) {
		nvram_set(strcat_r(prefix, "xgateway", tmp), strlen(gateway) > 0 ? gateway : "0.0.0.0");
		add_routes(prefix, "mroute", wan_ifname);
	}

	/* and one supplied via DHCP */
	if (strcmp(wan_proto, "dhcp") == 0)
		add_dhcp_routes(prefix, wan_ifname, 0);

#ifdef RTCONFIG_IPV6
	if (wan_unit == wan_primary_ifunit_ipv6()) {
		switch (get_ipv6_service()) {
		case IPV6_NATIVE_DHCP:
		case IPV6_MANUAL:
#ifdef RTCONFIG_6RELAYD
		case IPV6_PASSTHROUGH:
#endif
			if (strcmp(wan_proto, "dhcp") != 0 && strcmp(wan_proto, "static") != 0 &&
			    nvram_match(ipv6_nvname("ipv6_ifdev"), "ppp"))
				break;
			/* fall through */
		default:
			wan6_up(get_wan6face());
			break;
		}
	}
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6 &&
		nvram_match("iptv_ifname", wan_ifname)) {
		if (nvram_match("switch_wantag", "maxis_fiber_iptv"))
			route_add(wan_ifname, 0, "172.17.90.1", NULL, "255.255.255.255");
		start_igmpproxy(wan_ifname);
	}
#ifdef RTCONFIG_QUAGGA
	if( (wan_unit == WAN_UNIT_IPTV) || (wan_unit == WAN_UNIT_VOIP) ){
		stop_quagga();
		start_quagga();
	}
#endif
#endif

#if defined(DSL_N55U) || defined(DSL_N55U_B)
	if(nvram_match("wl0_country_code", "GB")) {
		if(isTargetArea()) {
			system("ATE Set_RegulationDomain_2G SG");
			//system("ATE Set_RegulationDomain_5G SG");
		}
	}
#endif

	/* Set connected state */
	update_wan_state(prefix, WAN_STATE_CONNECTED, 0);

#if defined(RTCONFIG_QCA) || \
		(defined(RTCONFIG_RALINK) && !defined(RTCONFIG_DSL) && !defined(RTN13U))
	reinit_hwnat(wan_unit);
#endif

	ctrl_wan_gro(wan_unit, 0);

	// TODO: handle different lan_ifname?
	start_firewall(wan_unit, 0);
	//start_firewall(wan_ifname, nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)),
	//	nvram_safe_get("lan_ifname"), nvram_safe_get("lan_ipaddr"));

	/* Start post-authenticator */
	start_auth(wan_unit, 1);

	/* Add dns servers to resolv.conf */
	update_resolvconf();

	/* default route via default gateway */
	add_multi_routes();

#if defined(RTCONFIG_USB_MODEM) && defined(RTCONFIG_INTERNAL_GOBI)
	if(dualwan_unit__usbif(wan_unit)){
		modem_unit = get_modemunit_by_type(get_dualwan_by_unit(wan_unit));
		usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));
		snprintf(env_unit, sizeof(env_unit), "unit=%d", modem_unit);

		putenv(env_unit);
		if(nvram_match(strcat_r(prefix2, "act_type", tmp2), "gobi")){
			nvram_set("freeze_duck", "5");
			eval("/usr/sbin/modem_status.sh", "rate");
			eval("/usr/sbin/modem_status.sh", "band");
			eval("/usr/sbin/modem_status.sh", "operation");
			eval("/usr/sbin/modem_status.sh", "provider");
		}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		eval("/usr/sbin/modem_status.sh", "get_dataset");
		eval("/usr/sbin/modem_status.sh", "bytes");
#endif
		unsetenv("unit");

		char start_sec[32], *str = file2str("/proc/uptime");
		unsigned int up = atoi(str);

		free(str);
		snprintf(start_sec, sizeof(start_sec), "%u", up);
		nvram_set(strcat_r(prefix2, "act_startsec", tmp2), start_sec);
	}
#endif

#ifdef RTCONFIG_OPENVPN
	stop_ovpn_eas();
#endif

	/* Sync time if not already set, or not running a daemon */
#ifdef RTCONFIG_NTPD
	if (!nvram_get_int("ntp_ready"))
#endif
		refresh_ntpc();

#if !defined(RTCONFIG_MULTIWAN_CFG)
	if (wan_unit != wan_primary_ifunit()
#ifdef RTCONFIG_DUALWAN
			|| nvram_match("wans_mode", "lb")
#endif
			)
	{
		if (nvram_get_int("ntp_ready")) {
#ifdef RTCONFIG_OPENVPN
			start_ovpn_eas();
#endif
			stop_ddns();
			start_ddns();
		}
		return;
	}
#endif

	/* start multicast router when not VPN */
	if (wan_unit == wan_primary_ifunit() &&
	    (strcmp(wan_proto, "dhcp") == 0 || strcmp(wan_proto, "static") == 0))
		start_igmpproxy(wan_ifname);

	stop_upnp();
	start_upnp();

	if (nvram_get_int("ntp_ready")) {
		stop_ddns();
		start_ddns();
	}

#ifdef RTCONFIG_VPNC
#ifdef RTCONFIG_VPN_FUSION
	start_vpnc();
#else
	if((nvram_match("vpnc_proto", "pptp") || nvram_match("vpnc_proto", "l2tp")) && nvram_match("vpnc_auto_conn", "1"))
		start_vpnc();
#endif
#endif

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
/* TODO: still required? */
	if (nvram_get_int("pptpd_enable")) {
		stop_pptpd();
		start_pptpd();
	}
#endif

#ifdef RTCONFIG_IPSEC
	if(nvram_get_int("ipsec_server_enable") || nvram_get_int("ipsec_client_enable")) {
		rc_ipsec_config_init();
		start_dnsmasq();
	}
#endif

#ifdef RTCONFIG_BLINK_LED
	adjust_netdev_if_of_wan_bled(1, wan_unit, wan_ifname);
#endif

#ifdef RTCONFIG_BWDPI
	int enabled = check_bwdpi_nvram_setting();
	int changed = tdts_check_wan_changed();

	BWDPI_DBG("enabled = %d, changed = %d\n", enabled, changed);

	if(enabled){
		_dprintf("[%s] do dpi engine service ... \n", __FUNCTION__);
		// if Adaptive QoS or AiProtection is enabled
		int count = 0;
		int val = 0;
		while (count < 3) {
			sleep(1);
			val = found_default_route(0);
			count++;
			if ((val == 1) || (count == 3)) break;
		}

		BWDPI_DBG("found_default_route result: %d\n", val);

		if (val) {
			// if restart_wan_if, remove dpi engine related
			if ((f_exists("/dev/detector") || f_exists("/dev/idpfw")) && changed == 0)
			{
				_dprintf("[%s] stop dpi engine service - %d\n", __FUNCTION__, changed);
				stop_dpi_engine_service(0);
			}
			else if ((f_exists("/dev/detector") || f_exists("/dev/idpfw")) && changed == 1)
			{
				_dprintf("[%s] stop dpi engine service - %d\n", __FUNCTION__, changed);
				stop_dpi_engine_service(1);
			}
			_dprintf("[%s] start dpi engine service\n", __FUNCTION__);
			set_codel_patch();
			start_dpi_engine_service();
			start_firewall(wan_unit, 0);
		}

		if(nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1){
			_dprintf("[wan up] tradtional qos or bandwidth limiter start\n");
			start_iQos();
		}
	}
	else{
		if(nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") != 1){
			_dprintf("[wan up] tradtional qos or bandwidth limiter start\n");
			start_iQos();
		}
	}
#else
	start_iQos();
#endif

#if !defined(RTCONFIG_MULTIWAN_CFG)
	/* FIXME: Protect below code from 2-nd WAN temporarilly. */
	if(wan_unit == wan_primary_ifunit())
#endif
	{
#ifdef RTCONFIG_TR069
		start_tr();
#endif

#ifdef RTCONFIG_GETREALIP
		char *getip[] = {"getrealip.sh", NULL};
		pid_t pid;

		//_eval(getip, ">>/tmp/log.txt", 0, &pid);
		_eval(getip, ">>/dev/null", 0, &pid);
#endif

#ifdef RTCONFIG_TCPDUMP
		eval("killall", "tcpdump");
#endif
	}

#ifdef RTCONFIG_LANTIQ
	disable_ppa_wan(wan_ifname);

	if(ppa_support(wan_unit) == 1){
		sleep(1);
		enable_ppa_wan(wan_ifname);
	}
#endif

#if 0
	snprintf(tmp, sizeof(tmp), "arping -w 1 -I %s %s", wan_ifname, gateway);
	if((fp = popen(tmp, "r")) != NULL){
		char wan_mac[18], upper_mac[18];
		int i;
		while(fgets(tmp, sizeof(tmp), fp) != NULL){
			memset(wan_mac, 0, sizeof(wan_mac));
			if(sscanf(tmp, "Unicast reply from %*s [%s] %*s", wan_mac) == 1){
				wan_mac[17] = 0;
				memset(upper_mac, 0, sizeof(upper_mac));
				for(i = 0; wan_mac[i]; ++i)
					upper_mac[i] = toupper(wan_mac[i]);
				nvram_set(strcat_r(prefix, "gw_mac", tmp), upper_mac);
				_dprintf("%s: wan_mac=%s.\n", __func__, upper_mac);
				break;
			}
		}
		fclose(fp);
	}
#else
	snprintf(tmp, sizeof(tmp), "ip neigh show %s dev %s 2>/dev/null", gateway, wan_ifname);
	if ((fp = popen(tmp, "r")) != NULL) {
		char lladdr[18], *ptr;
		if (fscanf(fp, "%*s lladdr %17s", lladdr) == 1) {
			for (ptr = lladdr; *ptr; ptr++)
				*ptr = toupper(*ptr);
			nvram_set(strcat_r(prefix, "gw_mac", tmp), lladdr);
			_dprintf("%s: wan_mac=%s.\n", __func__, lladdr);
		}else{
			nvram_unset(strcat_r(prefix, "gw_mac", tmp));
			_dprintf("%s: no wan_mac, remove\n", __func__);
		}
		pclose(fp);
	}
#endif

#ifdef RTCONFIG_OPENVPN
	if (nvram_get_int("ntp_ready")) {
		start_ovpn_eas();
	}
#endif

_dprintf("%s(%s): done.\n", __FUNCTION__, wan_ifname);
}

void
wan_down(char *wan_ifname)
{
	int wan_unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wan_proto, wan_proto_buf[16];
	char *gateway;
	int end_wan_sbstate = WAN_STOPPED_REASON_NONE;
#ifdef RTCONFIG_INTERNAL_GOBI
	int modem_unit;
	char tmp2[100], prefix2[32];
#endif

	_dprintf("%s(%s)\n", __FUNCTION__, wan_ifname);

	/* Skip physical interface of VPN connections */
	if ((wan_unit = wan_ifunit(wan_ifname)) < 0)
		return;

	/* Figure out nvram variable name prefix for this i/f */
	if(wan_prefix(wan_ifname, prefix) < 0)
		return;

	_dprintf("%s(%s): %s.\n", __FUNCTION__, wan_ifname, nvram_safe_get(strcat_r(prefix, "dns", tmp)));

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	if(get_wan_sbstate(wan_unit) == WAN_STOPPED_REASON_DATALIMIT)
		end_wan_sbstate = WAN_STOPPED_REASON_DATALIMIT;
#endif

#ifdef RTCONFIG_INTERNAL_GOBI
	if(dualwan_unit__usbif(wan_unit)){
		modem_unit = get_modemunit_by_type(get_dualwan_by_unit(wan_unit));
		usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

		nvram_unset(strcat_r(prefix2, "act_tx", tmp2));
		nvram_unset(strcat_r(prefix2, "act_rx", tmp2));
		nvram_unset(strcat_r(prefix2, "act_band", tmp2));
		nvram_unset(strcat_r(prefix2, "act_operation", tmp2));
		nvram_unset(strcat_r(prefix2, "act_provider", tmp2));
		nvram_unset(strcat_r(prefix2, "act_startsec", tmp2));
	}
#endif

#ifdef RTCONFIG_BLINK_LED
	adjust_netdev_if_of_wan_bled(0, wan_unit, wan_ifname);
#endif

	/* Stop post-authenticator */
	stop_auth(wan_unit, 1);

	wan_proto = nvram_safe_get_r(strcat_r(prefix, "proto", tmp), wan_proto_buf, sizeof(wan_proto_buf));

	if (wan_unit == wan_primary_ifunit()) {
		/* Stop multicast router when not VPN */
		if (strcmp(wan_proto, "dhcp") == 0 ||
		    strcmp(wan_proto, "static") == 0) {
#ifdef RTCONFIG_MULTICAST_IPTV
			if (nvram_get_int("switch_stb_x") > 6 &&
				!nvram_match("switch_wantag", "movistar"))
#endif
			stop_igmpproxy();
		}

		/* Remove default route to gateway if specified */
		gateway = nvram_safe_get_r(strcat_r(prefix, "gateway", tmp), tmp, sizeof(tmp));
		if (inet_addr_(gateway) == INADDR_ANY)
			gateway = NULL;
		route_del(wan_ifname, 0, "0.0.0.0", gateway, "0.0.0.0");
	}

	/* Remove interface dependent static routes */
	del_wan_routes(wan_ifname);

	if (strcmp(wan_proto, "static") == 0)
		ifconfig(wan_ifname, IFUP, NULL, NULL);

	update_wan_state(prefix, WAN_STATE_DISCONNECTED, end_wan_sbstate);

	/* Update resolv.conf
	 * Leave as is if no dns servers left for demand to work */
	if (*nvram_safe_get(strcat_r(prefix, "xdns", tmp)))
		nvram_unset(strcat_r(prefix, "dns", tmp));
	update_resolvconf();

#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		add_multi_routes();
#endif

#ifdef RTCONFIG_GETREALIP
#ifdef RTCONFIG_DUALWAN
	if(nvram_invmatch("wans_mode", "lb"))
#endif
	{
		nvram_set(strcat_r(prefix, "realip_state", tmp), "0");
		nvram_set(strcat_r(prefix, "realip_ip", tmp), "");
	}
#endif
#ifdef RTCONFIG_LANTIQ
	disable_ppa_wan(wan_ifname);
#endif
}

int
wan_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	if ((unit = ppp_ifunit(wan_ifname)) >= 0) {
		return unit;
	} else {
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
				return unit;
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (nvram_get_int("switch_stb_x") > 6) {
			for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; unit++) {
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
				    (nvram_match(strcat_r(prefix, "proto", tmp), "dhcp") ||
				     nvram_match(strcat_r(prefix, "proto", tmp), "static")))
					return unit;
			}
		}
#endif
	}

	return -1;
}

int
wanx_ifunit(char *wan_ifname)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit ++) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
		    (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") ||
		     nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
		     nvram_match(strcat_r(prefix, "proto", tmp), "l2tp")))
			return unit;
	}
#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6) {
		for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; unit++) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			if (nvram_match(strcat_r(prefix, "ifname", tmp), wan_ifname) &&
			    (nvram_match(strcat_r(prefix, "proto", tmp), "pppoe") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "pptp") ||
			     nvram_match(strcat_r(prefix, "proto", tmp), "l2tp")))
				return unit;
		}
	}
#endif
	return -1;
}

int
preset_wan_routes(char *wan_ifname)
{
	int unit = -1;

	if((unit = wan_ifunit(wan_ifname)) < 0)
		if((unit = wanx_ifunit(wan_ifname)) < 0)
			return -1;

	/* Set default route to gateway if specified */
	if(unit == wan_primary_ifunit())
		route_add(wan_ifname, 0, "0.0.0.0", "0.0.0.0", "0.0.0.0");

	/* Install interface dependent static routes */
	add_wan_routes(wan_ifname);
	return 0;
}

char *
get_lan_ipaddr()
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in *inaddr;
	struct in_addr ip_addr;

	/* Retrieve IP info */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
#if 0
		return strdup("0.0.0.0");
#else
	{
		memset(&ip_addr, 0x0, sizeof(ip_addr));
		return inet_ntoa(ip_addr);
	}
#endif

	strncpy(ifr.ifr_name, "br0", IFNAMSIZ);
	inaddr = (struct sockaddr_in *)&ifr.ifr_addr;
	inet_aton("0.0.0.0", &inaddr->sin_addr);

	/* Get IP address */
	ioctl(s, SIOCGIFADDR, &ifr);
	close(s);

	ip_addr = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr;
//	fprintf(stderr, "current LAN IP address: %s\n", inet_ntoa(ip_addr));
	return inet_ntoa(ip_addr);
}

int
ppp0_as_default_route()
{
	int i, n, found;
	FILE *f;
	unsigned int dest, mask;
	char buf[256], device[256];

	n = 0;
	found = 0;
	mask = 0;
	device[0] = '\0';

	if ((f = fopen("/proc/net/route", "r")) != NULL)
	{
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			if (++n == 1 && strncmp(buf, "Iface", 5) == 0)
				continue;

			i = sscanf(buf, "%255s %x %*s %*s %*s %*s %*s %x",
						device, &dest, &mask);

			if (i != 3)
				break;

			if (device[0] != '\0' && dest == 0 && mask == 0)
			{
				found = 1;
				break;
			}
		}

		fclose(f);

		if (found && !strcmp("ppp0", device))
			return 1;
		else
			return 0;
	}

	return 0;
}

int
found_default_route(int wan_unit)
{
	int i, n, found;
	FILE *f;
	unsigned int dest, mask;
	char buf[256], device[256];
	char *wanif;

	if(wan_unit != wan_primary_ifunit())
		return 1;

	if(dualwan_unit__usbif(wan_unit) && nvram_get_int("modem_pdp") == 2)
		return 1;

	n = 0;
	found = 0;
	mask = 0;
	device[0] = '\0';

	if ((f = fopen("/proc/net/route", "r")) != NULL)
	{
		while (fgets(buf, sizeof(buf), f) != NULL)
		{
			if (++n == 1 && strncmp(buf, "Iface", 5) == 0)
				continue;

				i = sscanf(buf, "%255s %x %*s %*s %*s %*s %*s %x",
					device, &dest, &mask);

			if (i != 3)
			{
				break;
			}

			if (device[0] != '\0' && dest == 0 && mask == 0)
			{
				wanif = get_wan_ifname(wan_unit);
				if (!strcmp(wanif, device))
				{
					found = 1;
					break;
				}
			}
		}

		fclose(f);

		if (found)
		{
			return 1;
		}
	}

	_dprintf("\nNO default route!!!\n");

	return 0;
}

long print_num_of_connections()
{
	char buf[256];
	char entries[16], others[256];
	long num_of_entries;

	FILE *fp = fopen("/proc/net/stat/nf_conntrack", "r");
	if (!fp) {
		fprintf(stderr, "no connection!\n");
		return 0;
	}

	fgets(buf, 256, fp);
	fgets(buf, 256, fp);
	fclose(fp);

	memset(entries, 0x0, 16);
	sscanf(buf, "%15s %s", entries, others);
	num_of_entries = strtoul(entries, NULL, 16);

	fprintf(stderr, "connection count: %ld\n", num_of_entries);
	return num_of_entries;
}

#ifdef RTCONFIG_BCM9
static int
ctf_entry_cleanup(void)
{
	ctf_cfg_request_t req;
	ctf_tuple_t tuple, *tp = NULL;
	struct sockaddr_nl src_addr, dest_addr;
	struct nlmsghdr *nlh = NULL;
	struct msghdr msg;
	struct iovec iov;
	int sock_fd, ret = SUCCESS;

	memset(&req, '\0', sizeof(req));
	req.command_id = CTFCFG_CMD_IPC_CLEANUP;
	req.size = sizeof(ctf_tuple_t);
	tp = (ctf_tuple_t *)req.arg;
	/* CTFCFG_CMD_IPC_CLEANUP doesn't care about the content of tuple*/
	*tp = tuple;

	/* Create a netlink socket */
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_CTF);

	if (sock_fd < 0) {
		fprintf(stderr, "Netlink socket create failed\n");
		return FAILURE;
	}

	/* Associate a local address with the opened socket */
	memset(&src_addr, 0, sizeof(struct sockaddr_nl));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();
	src_addr.nl_groups = 0;
	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));

	/* Fill the destination address, pid of 0 indicates kernel */
	memset(&dest_addr, 0, sizeof(struct sockaddr_nl));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;
	dest_addr.nl_groups = 0;

	/* Allocate memory for sending configuration request */
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(CTFCFG_MAX_SIZE));

	if (nlh == NULL) {
		fprintf(stderr, "Out of memory allocating cfg buffer\n");
		ret = FAILURE;
		goto out;
	}

	/* Fill the netlink message header. The configuration request
	 * contains netlink header followed by data.
	 */
	nlh->nlmsg_len = NLMSG_SPACE(CTFCFG_MAX_SIZE);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;

	/* Fill the data part */
	memcpy(NLMSG_DATA(nlh), &req, sizeof(req));
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Send request to kernel module */
	ret = sendmsg(sock_fd, &msg, 0);
	if (ret < 0) {
		perror("sendmsg:");
		ret = FAILURE;
		goto out;
	}

	/* Wait for the response */
	memset(nlh, 0, NLMSG_SPACE(CTFCFG_MAX_SIZE));
	ret = recvmsg(sock_fd, &msg, 0);
	if (ret < 0) {
		perror("recvmsg:");
		ret = FAILURE;
		goto out;
	}

	/* Copy data to user buffer */
	memcpy(&req, NLMSG_DATA(nlh), sizeof(req));

out:
	if (nlh) {
		free(nlh);
	}

	if (sock_fd >= 0) {
		close(sock_fd);
	}

	return ret;
}
#endif

void
start_wan(void)
{
#if defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_MULTICAST_IPTV)
	int unit;
#endif

#ifdef RTCONFIG_DSL
	dsl_configure(1);
#endif

	if (!is_routing_enabled())
		return;

#ifdef RTCONFIG_WIFI_SON
	nvram_set("start_wan", "1");
#endif

	/* Create links */
	mkdir("/tmp/ppp", 0777);
	mkdir("/tmp/ppp/peers", 0777);
	symlink("/sbin/rc", "/tmp/ppp/ip-up");
	symlink("/sbin/rc", "/tmp/ppp/ip-down");
	symlink("/sbin/rc", "/tmp/ppp/ip-pre-up");
#ifdef RTCONFIG_IPV6
	symlink("/sbin/rc", "/tmp/ppp/ipv6-up");
	symlink("/sbin/rc", "/tmp/ppp/ipv6-down");
	symlink("/sbin/rc", "/tmp/dhcp6c");
#endif
	symlink("/sbin/rc", "/tmp/ppp/auth-fail");
#ifdef RTCONFIG_VPNC
	symlink("/sbin/rc", "/tmp/ppp/vpnc-ip-up");
	symlink("/sbin/rc", "/tmp/ppp/vpnc-ip-down");
	symlink("/sbin/rc", "/tmp/ppp/vpnc-ip-pre-up");
	symlink("/sbin/rc", "/tmp/ppp/vpnc-auth-fail");
#ifdef RTCONFIG_VPN_FUSION
	if(!d_exists("/etc/openvpn")) {
		mkdir("/etc/openvpn", 0700);
	}
	symlink("/sbin/rc", "/etc/openvpn/ovpnc-up");
	symlink("/sbin/rc", "/etc/openvpn/ovpnc-down");
	symlink("/sbin/rc", "/etc/openvpn/ovpnc-route-up");
#endif
#endif
	symlink("/sbin/rc", "/tmp/udhcpc");
	symlink("/sbin/rc", "/tmp/zcip");
#ifdef RTCONFIG_EAPOL
	symlink("/sbin/rc", "/tmp/wpa_cli");
#endif
//	symlink("/dev/null", "/tmp/ppp/connect-errors");

#if defined(RTCONFIG_QCA) || \
		(defined(RTCONFIG_RALINK) && !defined(RTCONFIG_DSL) && !defined(RTN13U))
	reinit_hwnat(-1);
#endif

#ifdef HND_ROUTER
	fc_init();
#endif

#ifdef RTCONFIG_USB
#if defined(RTCONFIG_USB_MODEM) && !defined(RTCONFIG_SOC_IPQ40XX)
	add_usb_modem_modules();
#endif
#endif

#ifdef RTCONFIG_DUALWAN
	char wans_mode[16];

	snprintf(wans_mode, sizeof(wans_mode), "%s", nvram_safe_get("wans_mode"));

	/* Start each configured and enabled wan connection and its undelying i/f */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		if(dualwan_unit__nonusbif(unit)){
			if(((!strcmp(wans_mode, "fo") || !strcmp(wans_mode, "fb")) && unit == wan_primary_ifunit())
					|| !strcmp(wans_mode, "lb")
					){
				_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, unit);
				start_wan_if(unit);
			}
		}
	}
#else // RTCONFIG_DUALWAN
	_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, WAN_UNIT_FIRST);
	start_wan_if(WAN_UNIT_FIRST);

#ifdef RTCONFIG_USB_MODEM
	if(is_usb_modem_ready(WANS_DUALWAN_IF_USB) == 1 && nvram_get_int("success_start_service") == 1){
		_dprintf("%s: start_wan_if(%d)!\n", __FUNCTION__, WAN_UNIT_SECOND);
		start_wan_if(WAN_UNIT_SECOND);
	}
#endif
#endif // RTCONFIG_DUALWAN

	sleep(1); // let wanduck's detect not be close with start_wan().
	nvram_set("wanduck_start_detect", "1");

#ifdef RTCONFIG_MULTICAST_IPTV
	/* Start each configured and enabled wan connection and its undelying i/f */
	if (nvram_get_int("switch_stb_x") > 6) {
		for (unit = WAN_UNIT_IPTV; unit < WAN_UNIT_MULTICAST_IPTV_MAX; ++unit) {
			_dprintf("%s(IPTV): start_wan_if(%d)!\n", __FUNCTION__, unit);
			start_wan_if(unit);
		}
	}
#endif

#ifndef RT4GAC68U
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
	f_write_string("/proc/sys/net/bridge/bridge-nf-call-iptables", "0", 0, 0);
	f_write_string("/proc/sys/net/bridge/bridge-nf-call-ip6tables", "0", 0, 0);
#endif
#endif

	/* Report stats */
	if (*nvram_safe_get("stats_server")) {
		char *stats_argv[] = { "stats", nvram_safe_get("stats_server"), NULL };
		_eval(stats_argv, NULL, 5, NULL);
	}
}

void
stop_wan(void)
{
	int unit;

#ifdef RTCONFIG_WIFI_SON
	if (nvram_get("start_wan") == NULL)
		return;
	nvram_unset("start_wan");
#else
	if (!is_routing_enabled())
		return;
#endif

#ifdef RTCONFIG_RALINK
	if (module_loaded("hw_nat"))
	{
#if defined (RTCONFIG_WLMODULE_MT7615E_AP)
		doSystem("iwpriv %s set hw_nat_register=%d", get_wifname(0), 0);
#ifdef RTCONFIG_HAS_5G
		doSystem("iwpriv %s set hw_nat_register=%d", get_wifname(1), 0);
#endif
#endif
		modprobe_r("hw_nat");
		if (!g_reboot)
			sleep(1);
	}
#endif

#ifdef RTCONFIG_IPV6
	stop_wan6();
#endif

	/* Start each configured and enabled wan connection and its undelying i/f */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
		stop_wan_if(unit);

#ifdef HND_ROUTER
	if (module_loaded("pptp"))
		modprobe_r("pptp");

	fc_fini();
#endif

#ifdef RTCONFIG_OPENVPN
	stop_ovpn_eas();
#endif
#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	if (nvram_get_int("pptpd_enable"))
		stop_pptpd();
#endif

	/* Remove dynamically created links */
#ifdef RTCONFIG_EAPOL
	unlink("/tmp/wpa_cli");
#endif
	unlink("/tmp/udhcpc");
	unlink("/tmp/zcip");
	unlink("/tmp/ppp/ip-up");
	unlink("/tmp/ppp/ip-down");
	unlink("/tmp/ppp/ip-pre-up");
#ifdef RTCONFIG_IPV6
	unlink("/tmp/ppp/ipv6-up");
	unlink("/tmp/ppp/ipv6-down");
	unlink("/tmp/dhcp6c");
#endif
	unlink("/tmp/ppp/auth-fail");
#ifdef RTCONFIG_VPNC
	unlink("/tmp/ppp/vpnc-ip-up");
	unlink("/tmp/ppp/vpnc-ip-down");
	unlink("/tmp/ppp/vpnc-ip-pre-up");
	unlink("/tmp/ppp/vpnc-auth-fail");
#endif
	rmdir("/tmp/ppp");

#ifdef RTCONFIG_BCM9
	/* Clean up all ipct entry */
	if (!nvram_match("ctf_disable", "1") && !nvram_match("ctf_clean_disable", "1"))
		ctf_entry_cleanup();
#endif
}

void convert_wan_nvram(char *prefix, int unit)
{
#ifdef RTCONFIG_DUALWAN
	int mac_clone = 0;
#endif
	char tmp[100];
	char macbuf[32];
#if defined(CONFIG_BCMWL5) && defined(RTCONFIG_RGMII_BRCM5301X)
	char hwaddr_5g[18];
#endif

	_dprintf("%s(%s)\n", __FUNCTION__, prefix);

	// setup hwaddr
	strcpy(macbuf, nvram_safe_get(strcat_r(prefix, "hwaddr_x", tmp)));
	if (strlen(macbuf)!=0 && strcasecmp(macbuf, "FF:FF:FF:FF:FF:FF")) {
#ifdef RTCONFIG_DUALWAN
		mac_clone = 1;
#endif
		nvram_set(strcat_r(prefix, "hwaddr", tmp), macbuf);
		logmessage("wan", "mac clone: [%s] == [%s]\n", tmp, macbuf);
	}
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_RGMII_BRCM5301X
	else{
		/* QTN */
		if(strcmp(prefix, "wan1_") == 0) {
			strcpy(hwaddr_5g, get_wan_hwaddr());
			inc_mac(hwaddr_5g, 7);
			nvram_set(strcat_r(prefix, "hwaddr", tmp), hwaddr_5g);
			logmessage("wan", "[%s] == [%s]\n", tmp, hwaddr_5g);
		} else {
			nvram_set(strcat_r(prefix, "hwaddr", tmp), nvram_safe_get("lan_hwaddr"));
			logmessage("wan", "[%s] == [%s]\n", tmp, nvram_safe_get("lan_hwaddr"));
		}
	}
#else
	else nvram_set(strcat_r(prefix, "hwaddr", tmp), nvram_safe_get("et0macaddr"));
#endif	/* RTCONFIG_RGMII_BRCM5301X */
#else
	else nvram_set(strcat_r(prefix, "hwaddr", tmp), get_wan_hwaddr());
#endif	/* CONFIG_BCMWL5 */

#if defined(RTCONFIG_DUALWAN)
	if (!mac_clone && unit > 0) {
		unsigned char eabuf[ETHER_ADDR_LEN];
		char macaddr[32];

		/* Don't use same MAC address on all WANx interfaces. */
		ether_atoe(nvram_safe_get(strcat_r(prefix, "hwaddr", tmp)), eabuf);
		eabuf[ETHER_ADDR_LEN - 1] += unit;
		ether_etoa(eabuf, macaddr);
		nvram_set(strcat_r(prefix, "hwaddr", tmp), macaddr);
	}
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6 &&
	    unit > 9) {
		unsigned char ea[6];
		ether_atoe(get_wan_hwaddr(), ea);
		ea[5] = (ea[5] & 0xf0) | ((ea[5] + unit - 9) & 0x0f);
		ether_etoa(ea, macbuf);
		nvram_set(strcat_r(prefix, "hwaddr", tmp), macbuf);
	}
#endif
	// sync proto
	if (nvram_match(strcat_r(prefix, "proto", tmp), "static"))
		nvram_set_int(strcat_r(prefix, "dhcpenable_x", tmp), 0);
	// backlink unit for ppp
	nvram_set_int(strcat_r(prefix, "unit", tmp), unit);
}

void dumparptable()
{
	char buf[256];
	char ip_entry[32], hw_type[8], flags[8], hw_address[32], mask[32], device[8];
	char macbuf[32];

	FILE *fp = fopen("/proc/net/arp", "r");
	if (!fp) {
		fprintf(stderr, "no proc fs mounted!\n");
		return;
	}

	mac_num = 0;

	while (fgets(buf, 256, fp) && (mac_num < MAX_MAC_NUM - 2)) {
		sscanf(buf, "%s %s %s %s %s %s", ip_entry, hw_type, flags, hw_address, mask, device);

		if (!strcmp(device, "br0") && strlen(hw_address)!=0)
		{
			strcpy(mac_clone[mac_num++], hw_address);
		}
	}
	fclose(fp);

	strcpy(macbuf, nvram_safe_get("wan0_hwaddr_x"));

	// try pre-set mac
	if (strlen(macbuf)!=0 && strcasecmp(macbuf, "FF:FF:FF:FF:FF:FF"))
		strcpy(mac_clone[mac_num++], macbuf);

	// try original mac
	strcpy(mac_clone[mac_num++], get_lan_hwaddr());

	if (mac_num)
	{
		fprintf(stderr, "num of mac: %d\n", mac_num);
		int i;
		for (i = 0; i < mac_num; i++)
			fprintf(stderr, "mac to clone: %s\n", mac_clone[i]);
	}
}

#ifdef RTCONFIG_QCA_PLC_UTILS
#define PLCTOOL_M	"/tmp/plctool-m"
int autodet_plc_main(int argc, char *argv[]){
	FILE *fd;
	char buf[64], *ptr1, *ptr2;
	char ifname[8], plc_mac[18], resp_mac[18];
	int found = 0, idx = 0, chk = 0, cnt = 0;

	strlcpy(ifname, nvram_safe_get("lan_ifname"), sizeof(ifname));
	strlcpy(plc_mac, nvram_safe_get("plc_macaddr"), sizeof(plc_mac));

	snprintf(buf, sizeof(buf), "/usr/local/bin/plctool -i %s -m -e > %s", ifname, PLCTOOL_M);
	system(buf);

	if (!(fd = fopen(PLCTOOL_M, "r"))) {
		_dprintf("%s: Can't open %s\n", __func__, PLCTOOL_M);
		return -1;
	}

	// parse content
	while (fgets(buf, sizeof(buf), fd) != NULL) {
		// check response MAC
		if (!found && (ptr1 = strstr(buf, ifname))) {
			ptr2 = strstr(buf, " Found ");
			if (ptr2) {
				idx++;
				ptr1 += strlen(ifname) + 1;
				strlcpy(resp_mac, ptr1, sizeof(resp_mac));
				if (!strncmp(resp_mac, plc_mac, 17))
					found = 1;
				continue;
			}
		}

		// check amount of member
		if (strstr(buf, "network->TEI")) {
			idx--;
			if (idx == 0) {
				cnt++;
				chk = 1;
			} else
				chk = 0;
			continue;
		}
		if (chk && (ptr1 = strstr(buf, "network->STATIONS"))) {
			ptr1 += strlen("network->STATIONS") + 3;
			cnt += atoi(ptr1);
		}
	}

	fclose(fd);
	unlink(PLCTOOL_M);
	nvram_set_int("autodet_plc_state" , cnt);

	return 0;
}
#endif

int autodet_main(int argc, char *argv[]){
	int i;
	int unit;
	char prefix[]="wanXXXXXX_", tmp[100];
	char prefix2[]="autodetXXXXXX_", tmp2[100];
#if 0
	char hwaddr_x[32];
#endif
	int status;

	nvram_set("autodet_proceeding", "1");//Cherry Cho added for httpd checking in 2016/4/22.
	f_write_string("/tmp/detect_wrong.log", "", 0, 0);
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		if(get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_WAN && get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_LAN && get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_WAN2)
			continue;

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if(unit == WAN_UNIT_FIRST)
			snprintf(prefix2, sizeof(prefix2), "autodet_");
		else
			snprintf(prefix2, sizeof(prefix2), "autodet%d_", unit);

		nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_INITIALIZING);
		nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_INITIALIZING);

		// it shouldnot happen, because it is only called in default mode
		if(!nvram_match(strcat_r(prefix, "proto", tmp), "dhcp")){
			status = discover_all(unit);
			if(status == -1)
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_NOLINK);
			else if(status == 2)
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_WITHPPPOE);
			else
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_OK);

			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_NODHCP);
			continue;
		}

		// TODO: need every unit of WAN to do the autodet?
		if(!get_wanports_status(unit)){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_NOLINK);
			continue;
		}

		if(get_wan_state(unit) == WAN_STATE_CONNECTED){
			i = nvram_get_int(strcat_r(prefix, "lease", tmp));

			if(i < 60 && is_private_subnet(strcat_r(prefix, "ipaddr", tmp))){
				sleep(i);
			}
			//else{
			//	nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
			//	continue;
			//}
		}

		status = discover_all(unit);

		// check for pppoe status only,
		if(get_wan_state(unit) == WAN_STATE_CONNECTED){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
			if(status == 2)
				nvram_set_int(strcat_r(prefix2, "auxstate", tmp2), AUTODET_STATE_FINISHED_WITHPPPOE);
			continue;
		}
		else if(status == 2){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_WITHPPPOE);
			continue;
		}
		else if(is_ip_conflict(unit)){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
			continue;
		}

		nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_CHECKING);

// remove the Auto MAC clone from the decision on 2018/4/11.
#if 0
		dumparptable();

		// backup hwaddr_x
		strcpy(hwaddr_x, nvram_safe_get(strcat_r(prefix, "hwaddr_x", tmp)));
		//nvram_set(strcat_r(prefix, "hwaddr_x", tmp), "");

		int waitsec = nvram_get_int(strcat_r(prefix2, "waitsec", tmp2));

#define DEF_CLONE_WAITSEC 10
		if(waitsec <= 0)
			waitsec = DEF_CLONE_WAITSEC;

		i = 0;
		while(i < mac_num && (!is_wan_connect(unit) && !is_ip_conflict(unit))){
			if(!(nvram_match("wl0_country_code", "SG")) &&
			   strncmp(nvram_safe_get("territory_code"), "SG", 2) != 0){ // Singpore do not auto clone
				_dprintf("try clone %s\n", mac_clone[i]);
				nvram_set(strcat_r(prefix, "hwaddr_x", tmp), mac_clone[i]);
			}
			char buf[32];
			snprintf(buf, sizeof(buf), "restart_wan_if %d", unit);
			notify_rc_and_wait(buf);
			_dprintf("%s: wait a IP during %d seconds...\n", __FUNCTION__, waitsec);
			int count = 0;
			while(count < waitsec && (!is_wan_connect(unit) && !is_ip_conflict(unit))){
				sleep(1);

				++count;
			}
			++i;
		}

		if(i == mac_num){
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_FAIL);
			// restore hwaddr_x
			nvram_set(strcat_r(prefix, "hwaddr_x", tmp), hwaddr_x);
		}
		else if(i == mac_num-1){ // OK in original mac
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
		}
		else{ // OK in cloned mac
			nvram_set_int(strcat_r(prefix2, "state", tmp2), AUTODET_STATE_FINISHED_OK);
		}
		nvram_commit();
#endif
	}

#ifdef RTCONFIG_ALPINE
	// detect 10G
	const char *intf_10g[] = {"eth0", "eth2", NULL};

	for(i = 0; intf_10g[i] != NULL; ++i){
		snprintf(prefix, sizeof(prefix), "link_10G%d", i+1);
		if(nvram_get_int(prefix) != 1){
			nvram_set_int(strcat_r(prefix, "_state", tmp), AUTODET_STATE_FINISHED_NOLINK);
			continue;
		}

		status = discover_interface(intf_10g[i], 0);
		if(status == -1)
			nvram_set_int(strcat_r(prefix, "_state", tmp), AUTODET_STATE_FINISHED_NOLINK);
		else if(status == 2)
			nvram_set_int(strcat_r(prefix, "_state", tmp), AUTODET_STATE_FINISHED_WITHPPPOE);
		else
			nvram_set_int(strcat_r(prefix, "_state", tmp), AUTODET_STATE_FINISHED_OK);
	}
#endif
	nvram_set("autodet_proceeding", "0");//Cherry Cho added for httpd checking in 2016/4/22.

	return 0;
}

#ifdef RTCONFIG_DETWAN
#define RETRY_COUNT	3
#define MAX_DETWAN 4

int string_remove(char *string, const char *match)
{
	char *p;
	int len;
	if(string == NULL || match == NULL)
		return 0;
	if((p = strstr(string, match)) == NULL)
		return 0;

	len = strlen(match);
	while(isspace(*(p+len)))
		len++;
	memmove(p, p + len, strlen(p)+1 - len); //including '\0'
	return 1;
}

int string_add(char *string, const char *match, int at_head)
{
	char *p;
	int len;
	if(string == NULL || match == NULL)
		return 0;
	if(strstr(string, match) != NULL)
		return 0;

	len = strlen(match);
	if(at_head) {
		memmove(string+len+1, string, strlen(string)+1);
		memcpy(string, match, len);
		string[len] = ' ';
	} else {
		p = string + strlen(string);
		sprintf(p, " %s", match);
	}
	return 1;
}

void detwan_set_net_block(int add)
{
	char *block_dhcp_argv[] = {"ebtables", "-A", "FORWARD", "-p", "IPV4", "--ip-protocol", "UDP", "--ip-dport", "67", "-j", "DROP", NULL};
	char *block_nonarp_argv[] = {"ebtables", "-A", "FORWARD", "-d", "Broadcast", "-p", "!", "ARP", "-i", "eth+", "-j", "DROP", NULL};

	if(add == 0)
	{
		block_dhcp_argv[1] = "-D";
		block_nonarp_argv[1] = "-D";
	}

	_eval(block_dhcp_argv, NULL, 0, NULL);
	_eval(block_nonarp_argv, NULL, 0, NULL);
}

void detwan_apply_wan(const char *wan_ifname, unsigned int wan_mask, unsigned int lan_mask)
{
	char lan[128];
	int max_inf;
	int modify = 0;

	max_inf = nvram_get_int("detwan_max");
	if(max_inf <= 0)
		return;

	if(nvram_match("detwan_apply", "yes"))
	{
		int retry = 20;
		while(retry-- > 0 && nvram_safe_get("rc_service")[0] != '\0')
		{
			sleep(1);
		}
		if(retry == 0)
		{
			logmessage(__func__, "1: SKIP");
			return;	//skip this result
		}
	}
	{
		int idx;
		char var_name[32];
		char ifname[32];

		strcpy(lan, nvram_safe_get("lan_ifnames"));
		for(idx = 0; idx < max_inf; idx++) {
			snprintf(var_name, sizeof(var_name), "detwan_name_%d", idx);
			snprintf(ifname, sizeof(ifname), "%s", nvram_safe_get(var_name));
			if(strlen(ifname) <= 0)
				break;

			if(strcmp(ifname, wan_ifname) == 0) {
				if(string_remove(lan, ifname)) {
					modify++;
					eval("brctl", "delif", "br0", ifname);
					eval("ifconfig", ifname, "down");
					eval("ifconfig", ifname, "hw", "ether", get_wan_hwaddr());
					eval("ifconfig", ifname, "up");
				}
			} else {
				if(string_add(lan, ifname, 1)) {
					modify++;
					eval("ifconfig", ifname, "down");
					eval("ifconfig", ifname, "hw", "ether", get_lan_hwaddr());
					eval("ifconfig", ifname, "0.0.0.0");
					eval("brctl", "addif", "br0", ifname);
					eval("ifconfig", ifname, "up");
				}
			}
		}
	}

	logmessage(__func__, "2: wan(%s) lan(%s) modify(%d)\n", wan_ifname, lan, modify);

	if(modify == 0 && nvram_match("wan0_ifname", wan_ifname))
		return;	// skip, when the same interface

	nvram_set_int("wanports_mask", wan_mask);
	nvram_set_int("lanports_mask", lan_mask);
	nvram_set_int("detwan_wan_mask", wan_mask);
	nvram_set_int("detwan_lan_mask", lan_mask);
	nvram_set("lan_ifnames", lan);
	nvram_set("wan_ifnames", wan_ifname);
	nvram_set("wan0_ifname", wan_ifname);
	nvram_set("detwan_ifname", wan_ifname);
	nvram_set("wan0_gw_ifname", wan_ifname);
	nvram_commit();

	if(nvram_match("detwan_apply", "yes"))
	{
		int retry = RETRY_COUNT;
		char buf[32];
		snprintf(buf, sizeof(buf), "restart_wan_if %d", 0);
		while(retry-- > 0 && notify_rc_and_wait(buf) != 0);
		logmessage(__func__, "3: 'restart_wan_if 0' finish\n");
	}
}

int detwan_allmask(void)
{
	char var_name[32];
	int allmask;
	int idx, max_inf;
	int value;

	max_inf = nvram_get_int("detwan_max");
	allmask = 0;

	for(idx = 0; idx < max_inf; idx++){
		snprintf(var_name, sizeof(var_name), "detwan_mask_%d", idx);
		if((value = nvram_get_int(var_name)) != 0)
			allmask |= value;
		else
			break;
	}

	return allmask;
}

int detwan_check(char *ifname, unsigned int *wan_mask)
{
	int idx;
	int max_inf;
	int state = -1;
	char var_name[32];
	char var_value[PATH_MAX];
	char wan0_ifname[32];
	int value;
	int phy;
	int inf_count;
	char inf_names_buf[MAX_DETWAN][32];
	char *inf_names[MAX_DETWAN];
	int inf_mask[MAX_DETWAN];
	int got_inf;

	if(ifname == NULL || wan_mask == NULL)
		return -1;

	max_inf = nvram_get_int("detwan_max");
	strncpy(wan0_ifname, nvram_safe_get("wan0_ifname"), sizeof(wan0_ifname)-1);
	wan0_ifname[sizeof(wan0_ifname)-1] = '\0';

	logmessage(__func__, "0: max_inf(%d) wan0_ifname(%s)\n", max_inf, wan0_ifname);
	inf_count = 0;
	got_inf = -1;
	for(idx = 0; idx < max_inf; idx++){
		snprintf(var_name, sizeof(var_name), "detwan_mask_%d", idx);
		if((value = nvram_get_int(var_name)) != 0) {
			phy = get_ports_status((unsigned int)value);
			snprintf(var_name, sizeof(var_name), "detwan_name_%d", idx);
			snprintf(var_value, sizeof(var_value), "%s", nvram_safe_get(var_name));

//			if(wan0_ifname == NULL || wan0_ifname[0] == '\0')
			{ //No WAN
				if(phy > 0 && inf_count < MAX_DETWAN && strlen(var_value) > 0) {
					strncpy(inf_names_buf[inf_count], var_value, sizeof(inf_names_buf[0])-1);
					inf_names_buf[inf_count][sizeof(inf_names_buf[0])-1] = '\0';
					inf_names[inf_count] = inf_names_buf[inf_count];
					inf_mask[inf_count] = value;
					inf_count++;
				}
			}
#if 0
			else
			{
				if(strlen(var_value) > 0 && strcmp(wan0_ifname, var_value) == 0)
				{ //Is WAN
				}
				else if(strlen(var_value) > 0)
				{ //Not WAN
				}
			}
#endif
		}
	}
	if(inf_count) {
		time_t now;
		extern int discover_interfaces(int num, const char **current_wan_ifnames, int dhcp_det, int *got_inf);
		state = discover_interfaces(inf_count, (const char **) inf_names, nvram_match("wan0_proto", "dhcp"), &got_inf);
		now = time(NULL);
		logmessage(__func__, "1: wan0_ifname(%s) inf_count(%d) state(%d) got_inf(%d) %s", wan0_ifname, inf_count, state, got_inf, ctime(&now));

		if(state <= 0 && inf_count == 1)	//set to the only phy with cable
		{
			state = 0;
			got_inf = 0;
		}
		nvram_set_int("detwan_proto", state);
		if(got_inf < 0 || got_inf >= inf_count) {
			nvram_unset("detwan_phy");
			ifname[0] = '\0';
			*wan_mask = 0;
		}
		else {
			nvram_set("detwan_phy", inf_names[got_inf]);
			strcpy(ifname, inf_names[got_inf]);
			*wan_mask = inf_mask[got_inf];
		}
	}
	else {
		time_t now = time(NULL);
		logmessage(__func__, "2: NO phy is linked %s", ctime(&now));
		nvram_set_int("detwan_proto", -1);
		nvram_unset("detwan_phy");
		ifname[0] = '\0';
		*wan_mask = 0;
	}
	return state;
}

int detwan_main(int argc, char *argv[]){
	int max_inf;
	unsigned int allmask;
	int state = -1;
	int sw_mode;
	char ifname[32];
	unsigned int wan_mask;

	sw_mode = nvram_get_int("sw_mode");
	allmask = detwan_allmask();

	logmessage(__func__, "0: sw_mode(%d) wan0_ifname(%s) allmask(%08x)", sw_mode, nvram_get("wan0_ifname"), allmask);
	if (sw_mode != SW_MODE_ROUTER || allmask == 0)
		return -1;

	if(nvram_match("detwan_apply", "yes"))
	{
		int retry = 20;
		while(retry-- > 0 && nvram_safe_get("rc_service")[0] != '\0')
		{
			sleep(1);
		}
		if(retry <= 0)
		{
			logmessage(__func__, "1: rc_service(%s) block !!", nvram_safe_get("rc_service"));
			return -1;
		}
	}

	state = detwan_check(ifname, &wan_mask);
	if(state >= 0 && ifname[0] != '\0') {
		detwan_apply_wan(ifname, wan_mask, allmask & ~wan_mask);
	}

	logmessage(__func__, "9: finish !!");
	return 0;
}
#endif	/* RTCONFIG_DETWAN */

