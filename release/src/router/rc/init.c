/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/

#include "rc.h"

#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <time.h>
#include <errno.h>
#include <paths.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/klog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef LINUX26
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#endif
#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
#include <confmtd_utils.h>
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

#include <shutils.h>

#include <shared.h>
#include <stdio.h>
#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#include <disk_share.h>
#endif

#include <sys/utsname.h>

#ifdef RTCONFIG_REALTEK
#include "../shared/sysdeps/realtek/realtek.h"
#endif

#ifdef RTCONFIG_BCMFA
#include <sysdeps.h>
#endif

#if defined(RTCONFIG_LP5523)
#include <lp5523led.h>
#endif

#define SHELL "/bin/sh"

static int fatalsigs[] = {
	SIGILL,
	SIGABRT,
	SIGFPE,
	SIGPIPE,
	SIGBUS,
	SIGSYS,
	SIGTRAP,
	SIGPWR
};

static int initsigs[] = {
	SIGHUP,
	SIGUSR1,
	SIGUSR2,
	SIGINT,
	SIGQUIT,
	SIGALRM,
	SIGTERM
};

static char *defenv[] = {
	"TERM=vt100",
	"HOME=/",
	//"PATH=/usr/bin:/bin:/usr/sbin:/sbin",
#ifdef RTCONFIG_LANTIQ
	"PATH=/opt/usr/bin:/opt/bin:/opt/usr/sbin:/opt/sbin:/usr/bin:/bin:/usr/sbin:/sbin:/rom/opt/lantiq/bin:/rom/opt/lantiq/usr/sbin",
#else
	"PATH=/opt/usr/bin:/opt/bin:/opt/usr/sbin:/opt/sbin:/usr/bin:/bin:/usr/sbin:/sbin",
#endif
#ifdef HND_ROUTER
	"LD_LIBRARY_PATH=/lib:/usr/lib:/lib/aarch64",
#ifdef RTCONFIG_HNDMFG
	"PS1=# ",
#endif
#endif
#ifdef RTCONFIG_LANTIQ
	"LD_LIBRARY_PATH=/lib:/usr/lib:/opt/lantiq/usr/lib:/opt/lantiq/usr/sbin/:/tmp/wireless/lantiq/usr/lib/",
#endif
	"SHELL=" SHELL,
	"USER=root",
	"TMOUT=0",
#ifdef RTCONFIG_DMALLOC
/*
 * # dmalloc -DV
 * Debug Tokens:
 * none -- no functionality (0)
 * log-stats -- log general statistics (0x1)
 * log-non-free -- log non-freed pointers (0x2)
 * log-known -- log only known non-freed (0x4)
 * log-trans -- log memory transactions (0x8)
 * log-admin -- log administrative info (0x20)
 * log-bad-space -- dump space from bad pnt (0x100)
 * log-nonfree-space -- dump space from non-freed pointers (0x200)
 * log-elapsed-time -- log elapsed-time for allocated pointer (0x40000)
 * log-current-time -- log current-time for allocated pointer (0x80000)
 * check-fence -- check fence-post errors (0x400)
 * check-heap -- check heap adm structs (0x800)
 * check-blank -- check mem overwritten by alloc-blank, free-blank (0x2000)
 * check-funcs -- check functions (0x4000)
 * check-shutdown -- check heap on shutdown (0x8000)
 * catch-signals -- shutdown program on SIGHUP, SIGINT, SIGTERM (0x20000)
 * realloc-copy -- copy all re-allocations (0x100000)
 * free-blank -- overwrite freed memory with \0337 byte (0xdf) (0x200000)
 * error-abort -- abort immediately on error (0x400000)
 * alloc-blank -- overwrite allocated memory with \0332 byte (0xda) (0x800000)
 * print-messages -- write messages to stderr (0x2000000)
 * catch-null -- abort if no memory available (0x4000000)
 *
 * debug=0x4e48503 = log_stats, log-non-free, log-bad-space, log-elapsed-time, check-fence, check-shutdown, free-blank, error-abort, alloc-blank, catch-null
 * debug=0x3 = log-stats, log-non-free
 */
	"DMALLOC_OPTIONS=debug=0x3,inter=100,log=/jffs/dmalloc_%d.log",
#endif
#if defined(HND_ROUTER) && !defined(RTCONFIG_HNDMFG)
	"ENV=/etc/profile",
#endif
	NULL
};

extern int set_tcode_misc();
extern int g_upgrade;

#ifdef RTCONFIG_WPS
static void
wps_restore_defaults(void)
{
	char macstr[128];
	int i;

	/* cleanly up nvram for WPS */
	nvram_unset("wps_config_state");
	nvram_unset("wps_proc_status");
	nvram_unset("wps_proc_status_x");
	nvram_unset("wps_config_method");
	nvram_unset("wps_band");

	nvram_unset("wps_restart");
	nvram_unset("wps_proc_mac");

	nvram_unset("wps_sta_devname");
	nvram_unset("wps_sta_mac");

	nvram_unset("wps_pinfail");
	nvram_unset("wps_pinfail_mac");
	nvram_unset("wps_pinfail_name");
	nvram_unset("wps_pinfail_state");

	nvram_unset("wps_enr_ssid");
	nvram_unset("wps_enr_bssid");
	nvram_unset("wps_enr_wsec");

	nvram_set("wps_device_name", get_productid());
	nvram_set("wps_modelnum", get_productid());

	strcpy(macstr, get_lan_hwaddr());

	if (strlen(macstr))
		for (i = 0; i < strlen(macstr); i++)
			macstr[i] = tolower(macstr[i]);
	nvram_set("boardnum", nvram_get("serial_no") ? : macstr);
}
#endif /* RTCONFIG_WPS */

static void
virtual_radio_restore_defaults(void)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXXXXXXXXXXXXXXXX_mssid_";
	int i, j;

	nvram_unset("unbridged_ifnames");
	nvram_unset("ure_disable");

	/* Delete dynamically generated variables */
	for (i = 0; i < MAX_NVPARSE; i++) {
		sprintf(prefix, "wl%d_", i);
		nvram_unset(strcat_r(prefix, "vifs", tmp));
		nvram_unset(strcat_r(prefix, "ssid", tmp));
		nvram_unset(strcat_r(prefix, "guest", tmp));
		nvram_unset(strcat_r(prefix, "ure", tmp));
		nvram_unset(strcat_r(prefix, "ipconfig_index", tmp));
		nvram_unset(strcat_r(prefix, "nas_dbg", tmp));
		sprintf(prefix, "lan%d_", i);
		nvram_unset(strcat_r(prefix, "ifname", tmp));
		nvram_unset(strcat_r(prefix, "ifnames", tmp));
		nvram_unset(strcat_r(prefix, "gateway", tmp));
		nvram_unset(strcat_r(prefix, "proto", tmp));
		nvram_unset(strcat_r(prefix, "ipaddr", tmp));
		nvram_unset(strcat_r(prefix, "netmask", tmp));
		nvram_unset(strcat_r(prefix, "lease", tmp));
		nvram_unset(strcat_r(prefix, "stp", tmp));
		nvram_unset(strcat_r(prefix, "hwaddr", tmp));
		sprintf(prefix, "dhcp%d_", i);
		nvram_unset(strcat_r(prefix, "start", tmp));
		nvram_unset(strcat_r(prefix, "end", tmp));

		/* clear virtual versions */
		for (j = 0; j < 16; j++) {
			sprintf(prefix, "wl%d.%d_", i, j);
			nvram_unset(strcat_r(prefix, "ssid", tmp));
			nvram_unset(strcat_r(prefix, "ipconfig_index", tmp));
			nvram_unset(strcat_r(prefix, "guest", tmp));
			nvram_unset(strcat_r(prefix, "closed", tmp));
			nvram_unset(strcat_r(prefix, "wpa_psk", tmp));
			nvram_unset(strcat_r(prefix, "auth", tmp));
			nvram_unset(strcat_r(prefix, "wep", tmp));
			nvram_unset(strcat_r(prefix, "auth_mode", tmp));
			nvram_unset(strcat_r(prefix, "crypto", tmp));
			nvram_unset(strcat_r(prefix, "akm", tmp));
			nvram_unset(strcat_r(prefix, "hwaddr", tmp));
			nvram_unset(strcat_r(prefix, "bss_enabled", tmp));
#ifdef CONFIG_BCMWL5
			nvram_unset(strcat_r(prefix, "bss_maxassoc", tmp));
			nvram_unset(strcat_r(prefix, "wme_bss_disable", tmp));
#endif
			nvram_unset(strcat_r(prefix, "ifname", tmp));
			nvram_unset(strcat_r(prefix, "unit", tmp));
			nvram_unset(strcat_r(prefix, "ap_isolate", tmp));
			nvram_unset(strcat_r(prefix, "macmode", tmp));
#ifdef CONFIG_BCMWL5
			nvram_unset(strcat_r(prefix, "maclist", tmp));
			nvram_unset(strcat_r(prefix, "maxassoc", tmp));
#endif
			nvram_unset(strcat_r(prefix, "mode", tmp));
			nvram_unset(strcat_r(prefix, "radio", tmp));
			nvram_unset(strcat_r(prefix, "radius_ipaddr", tmp));
			nvram_unset(strcat_r(prefix, "radius_port", tmp));
			nvram_unset(strcat_r(prefix, "radius_key", tmp));
			nvram_unset(strcat_r(prefix, "key", tmp));
			nvram_unset(strcat_r(prefix, "key1", tmp));
			nvram_unset(strcat_r(prefix, "key2", tmp));
			nvram_unset(strcat_r(prefix, "key3", tmp));
			nvram_unset(strcat_r(prefix, "key4", tmp));
			nvram_unset(strcat_r(prefix, "wpa_gtk_rekey", tmp));
			nvram_unset(strcat_r(prefix, "nas_dbg", tmp));
#ifdef RTCONFIG_PSR_GUEST
			nvram_unset(strcat_r(prefix, "psr_guest", tmp));
#endif
			nvram_unset(strcat_r(prefix, "probresp_mf", tmp));

			nvram_unset(strcat_r(prefix, "bss_opmode_cap_reqd", tmp));
			nvram_unset(strcat_r(prefix, "mcast_regen_bss_enable", tmp));
			nvram_unset(strcat_r(prefix, "wmf_bss_enable", tmp));
			nvram_unset(strcat_r(prefix, "preauth", tmp));
			nvram_unset(strcat_r(prefix, "dwds", tmp));
			nvram_unset(strcat_r(prefix, "acs_dfsr_deferred", tmp));
			nvram_unset(strcat_r(prefix, "wet_tunnel", tmp));
			nvram_unset(strcat_r(prefix, "bridge", tmp));
			nvram_unset(strcat_r(prefix, "mfp", tmp));
			nvram_unset(strcat_r(prefix, "acs_dfsr_activity", tmp));
			nvram_unset(strcat_r(prefix, "acs_dfsr_immediate", tmp));
			nvram_unset(strcat_r(prefix, "wme", tmp));
			nvram_unset(strcat_r(prefix, "net_reauth", tmp));
			nvram_unset(strcat_r(prefix, "pmk_cache", tmp));
#ifdef CONFIG_BCMWL5
			nvram_unset(strcat_r(prefix, "sta_retry_time", tmp));
			nvram_unset(strcat_r(prefix, "infra", tmp));
#ifdef __CONFIG_HSPOT__
			nvram_unset(strcat_r(prefix, "hsflag", tmp));
			nvram_unset(strcat_r(prefix, "hs2cap", tmp));
			nvram_unset(strcat_r(prefix, "opercls", tmp));
			nvram_unset(strcat_r(prefix, "anonai", tmp));
			nvram_unset(strcat_r(prefix, "wanmetrics", tmp));
			nvram_unset(strcat_r(prefix, "oplist", tmp));
			nvram_unset(strcat_r(prefix, "homeqlist", tmp));
			nvram_unset(strcat_r(prefix, "osu_ssid", tmp));
			nvram_unset(strcat_r(prefix, "osu_frndname", tmp));
			nvram_unset(strcat_r(prefix, "osu_uri", tmp));
			nvram_unset(strcat_r(prefix, "osu_nai", tmp));
			nvram_unset(strcat_r(prefix, "osu_method", tmp));
			nvram_unset(strcat_r(prefix, "osu_icons", tmp));
			nvram_unset(strcat_r(prefix, "osu_servdesc", tmp));
			nvram_unset(strcat_r(prefix, "concaplist", tmp));
			nvram_unset(strcat_r(prefix, "qosmapie", tmp));
			nvram_unset(strcat_r(prefix, "gascbdel", tmp));
			nvram_unset(strcat_r(prefix, "iwnettype", tmp));
			nvram_unset(strcat_r(prefix, "hessid", tmp));
			nvram_unset(strcat_r(prefix, "ipv4addr", tmp));
			nvram_unset(strcat_r(prefix, "ipv6addr", tmp));
			nvram_unset(strcat_r(prefix, "netauthlist", tmp));
			nvram_unset(strcat_r(prefix, "venuegrp", tmp));
			nvram_unset(strcat_r(prefix, "venuetype", tmp));
			nvram_unset(strcat_r(prefix, "venuelist", tmp));
			nvram_unset(strcat_r(prefix, "ouilist", tmp));
			nvram_unset(strcat_r(prefix, "3gpplist", tmp));
			nvram_unset(strcat_r(prefix, "domainlist", tmp));
			nvram_unset(strcat_r(prefix, "realmlist", tmp));
#endif /* __CONFIG_HSPOT__ */

#if defined(__CONFIG_VISUALIZATION__) && defined(CONFIG_VISUALIZATION_ENABLED)
			nvram_unset(strcat_r(prefix, "rrm", tmp));
#endif /* __CONFIG_VISUALIZATION__ && CONFIG_VISUALIZATION_ENABLED */
#endif
		}
	}
}

#ifdef RTCONFIG_BCMARM
void
misc_ioctrl(void)
{
	/* default WAN_RED on  */
	int model = get_model();
	switch(model) {
		case MODEL_RTAC87U:
		case MODEL_RTAC68U:
		case MODEL_RTAC3200:
		case MODEL_RTAC5300:
		case MODEL_GTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC86U:
		case MODEL_RTAC3100:
#ifdef RTAC68U
			if (!is_ac66u_v2_series())
				return;
#endif

#if defined(HND_ROUTER) && defined(RTCONFIG_LAN4WAN_LED)
			setLANLedOn();
#endif

#if defined(HND_ROUTER) && defined(RTCONFIG_HNDMFG)
			led_control(LED_WAN_NORMAL, LED_ON);
			return;
#endif

			if (is_router_mode()) {
				if (strcmp(nvram_safe_get("wans_dualwan"), "wan none") != 0) {
					logmessage("DualWAN", "skip misc_ioctrl()");
					return;
				}

				led_control(LED_WAN, LED_ON);
#ifndef HND_ROUTER
				eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01fe");
				eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01fe");
#else
				led_control(LED_WAN_NORMAL, LED_OFF);
#endif
			}
#ifdef HND_ROUTER
			else
				led_control(LED_WAN_NORMAL, LED_ON);
#endif

			break;
	}
}
#endif

/* assign none-exist value */
void
wl_defaults(void)
{
	struct nvram_tuple *t;
	char prefix[]="wlXXXXXXXXXXXXX_", tmp[100], tmp2[100];
	char pprefix[]="wlXXXXXXXXXXXXX_", *pssid;
	char word[256], *next;
	int unit, subunit;
	char wlx_vifnames[64], wl_vifnames[64], lan_ifnames[128];
	char wlx_vifnames2[64], wl_vifnames2[64];
	int subunit_x = 0;
#if defined(RTCONFIG_AMAS)
	char sta_ifnames[64]={0};
#endif	
	int i;
#if !defined(RTCONFIG_FBWIFI)
	char *fbwifi_iface[3] = { "fbwifi_2g", "fbwifi_5g", "fbwifi_5g_2" };
#endif
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	int inic_vlan_id = INIC_VLAN_ID_START;
	char nic_lan_ifnames[64];
	char *pNic;
#endif
	unsigned int max_mssid;

	memset(wl_vifnames, 0, sizeof(wl_vifnames));
	memset(wlx_vifnames2, 0, sizeof(wlx_vifnames2));
	memset(wl_vifnames2, 0, sizeof(wl_vifnames2));
	memset(lan_ifnames, 0, sizeof(lan_ifnames));
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	memset(nic_lan_ifnames, 0, sizeof(nic_lan_ifnames));
	pNic = nic_lan_ifnames;
#endif

	if (!nvram_get("wl_country_code"))
		nvram_set("wl_country_code", "");

#ifdef RTCONFIG_LANTIQ
	int j, wlc_band = nvram_get_int("wlc_band");

	// only for the all SKIP_ABSENT_BAND().
	for(i = 0; i < MAX_NR_WL_IF; ++i){
		snprintf(prefix, sizeof(prefix), "wl%d_", i);

		//for(j = 0; j < MAX_NO_MSSID; ++j){
		for(j = 0; j < 1; ++j){
			snprintf(pprefix, sizeof(pprefix), "wl%d.%d_", i, j+1);

			if(!nvram_get(strcat_r(pprefix, "nband", tmp2)) || strlen(nvram_safe_get(tmp2)) <= 0)
				nvram_set(tmp2, nvram_safe_get(strcat_r(prefix, "nband", tmp)));
		}
	}
#endif

#if !defined(RTCONFIG_FBWIFI)
	/* If Facebook Wi-Fi has been enabled in another firmware, disable corresponding wlX.Y.
	 * If we don't do this, Open System VAP will be created later.
	 */
	for (i = 0; i < min(MAX_NR_WL_IF, ARRAY_SIZE(fbwifi_iface)); ++i) {
		SKIP_ABSENT_BAND(i);
		if (!nvram_get(fbwifi_iface[i]))
			continue;
		if (sscanf(nvram_safe_get(fbwifi_iface[i]), "wl%d.%d", &unit, &subunit) != 2)
			continue;

		nvram_unset(fbwifi_iface[i]);
		nvram_set("fbwifi_enable", "off");
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		if (nvram_pf_match(prefix, "bss_enabled", "0"))
			continue;
		nvram_pf_set(prefix, "bss_enabled", "0");
	}
#endif

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		for (t = router_defaults; t->name; t++) {
#ifdef CONFIG_BCMWL5
			if (!strncmp(t->name, "wl", 2) && strncmp(t->name, "wl_", 3) && strncmp(t->name, "wlc", 3) && !strcmp(&t->name[4], "nband"))
				nvram_set(t->name, t->value);
#endif
			if (strncmp(t->name, "wl_", 3)!=0) continue;
#ifdef CONFIG_BCMWL5
			if (!strcmp(&t->name[3], "nband") && nvram_match(strcat_r(prefix, &t->name[3], tmp), "-1"))
				nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
#endif
			if (!nvram_get(strcat_r(prefix, &t->name[3], tmp))) {
				/* Add special default value handle here */
#ifdef RTCONFIG_EMF
				/* Wireless IGMP Snooping */
				if (strncmp(&t->name[3], "igs", sizeof("igs")) == 0) {
					char *value = nvram_get(strcat_r(prefix, "wmf_bss_enable", tmp2));
					nvram_set(tmp, (value && *value) ? value : t->value);
				} else
#endif
				nvram_set(tmp, t->value);
			}
		}

		unit++;
	}

//	virtual_radio_restore_defaults();

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
		memset(wlx_vifnames, 0, sizeof(wlx_vifnames));
		snprintf(pprefix, sizeof(pprefix), "wl%d_", unit);
#if defined(RTCONFIG_CONCURRENTREPEATER)
		/* don't support guest network on AP mode for RP-AC66 */
		if (sw_mode() == SW_MODE_AP)
			continue;
#endif
		/* including primary ssid */
		max_mssid = num_of_mssid_support(unit);
#ifdef RTCONFIG_PSR_GUEST
		max_mssid++;
#endif

		for (subunit = 1; subunit < max_mssid+1; subunit++)
		{
			snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
#ifdef RTCONFIG_WIRELESSREPEATER
			if (sw_mode() == SW_MODE_REPEATER) {
#ifdef RTCONFIG_REALTEK
/* [MUST]: Why to do this ?  */
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
#else
				if (
#if defined(RTCONFIG_CONCURRENTREPEATER)
					(nvram_get_int("wlc_express") == 0 || (nvram_get_int("wlc_express") - 1) != unit ) && subunit==1)
#else
					nvram_get_int("wlc_band")==unit && subunit==1)
#endif
				{
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
				}
				else {
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
				}
#endif
			}
#endif

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
			if (is_psta(unit))
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
			else if (is_psr(unit) && (subunit == 1)) {
#ifdef RTCONFIG_DPSTA
#ifdef RTCONFIG_AMAS
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), ((!dpsta_mode() && !dpsr_mode()) || is_dpsta(unit) || is_dpsr(unit) || dpsta_mode()) ? "1" : "0");
#else
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), ((!dpsta_mode() && !dpsr_mode()) || is_dpsta(unit) || is_dpsr(unit)) ? "1" : "0");
#endif
#else
					nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
#endif
			}
#endif

			if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			{
				subunit_x++;
				if (strlen(wlx_vifnames)) {
					sprintf(wlx_vifnames2, "%s %s", wlx_vifnames, get_wlifname(unit, subunit, subunit_x, tmp));
					strlcpy(wlx_vifnames, wlx_vifnames2, sizeof(wlx_vifnames));
				} else
					sprintf(wlx_vifnames, "%s", get_wlifname(unit, subunit, subunit_x, tmp));
			}
			else
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");

#if !defined(RTCONFIG_REALTEK) && defined(RTCONFIG_CONCURRENTREPEATER)
			if (nvram_get_int("wlc_express") == 1 || nvram_get_int("wlc_express") == 2) {
				if (unit == (nvram_get_int("wlc_express") - 1)) {
					subunit_x++;

					if (strlen(wlx_vifnames) == 0)
						sprintf(wlx_vifnames, "%s", get_wlifname(unit, subunit, subunit_x, tmp));

				}
				else
					memset(wlx_vifnames, 0x0, 32);
			}
#endif

#ifndef RTCONFIG_LANTIQ
//			if (!nvram_get(strcat_r(prefix, "ifname", tmp)))
			{
				snprintf(tmp2, sizeof(tmp2), "wl%d.%d", unit, subunit);
//				nvram_set(strcat_r(prefix, "ifname", tmp), tmp2);
				nvram_set(strcat_r(prefix, "ifname", tmp), get_wlifname(unit, subunit, subunit_x, tmp2));
			}
#endif

			if (!nvram_get(strcat_r(prefix, "unit", tmp)))
			{
				snprintf(tmp2, sizeof(tmp2), "%d.%d", unit, subunit);
				nvram_set(strcat_r(prefix, "unit", tmp), tmp2);
			}

			if (!nvram_get(strcat_r(prefix, "bw_enabled", tmp)))
			{
				nvram_set(strcat_r(prefix, "bw_enabled", tmp), "0");
				nvram_set(strcat_r(prefix, "bw_dl", tmp), "");
				nvram_set(strcat_r(prefix, "bw_ul", tmp), "");
			}

			if (!nvram_get(strcat_r(prefix, "ssid", tmp))) {
				pssid = nvram_default_get(strcat_r(pprefix, "ssid", tmp));
				if (strlen(pssid))
#if defined(RTCONFIG_CONCURRENTREPEATER)
					sprintf(tmp2, "%s", pssid);
#else
					sprintf(tmp2, "%s_Guest%d", pssid, subunit);
#endif
				else sprintf(tmp2, "%s_Guest%d", SSID_PREFIX, subunit);
				nvram_set(strcat_r(prefix, "ssid", tmp), tmp2);
			}

			if (!nvram_get(strcat_r(prefix, "auth_mode_x", tmp)))
			{
				nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "open");
				nvram_set(strcat_r(prefix, "auth_mode", tmp), "none");
				nvram_set(strcat_r(prefix, "akm", tmp), "");
				nvram_set(strcat_r(prefix, "auth", tmp), "0");
				nvram_set(strcat_r(prefix, "wep_x", tmp), "0");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
#endif
				nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
				nvram_set(strcat_r(prefix, "key", tmp), "1");
				nvram_set(strcat_r(prefix, "key1", tmp), "");
				nvram_set(strcat_r(prefix, "key2", tmp), "");
				nvram_set(strcat_r(prefix, "key3", tmp), "");
				nvram_set(strcat_r(prefix, "key4", tmp), "");
				nvram_set(strcat_r(prefix, "wpa_psk", tmp), "");

				nvram_set(strcat_r(prefix, "ap_isolate", tmp), "0");
				nvram_set(strcat_r(prefix, "bridge", tmp), "");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#elif defined(RTCONFIG_REALTEK)
				nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), "31");
#else
				nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), "128");
#endif
				nvram_set(strcat_r(prefix, "closed", tmp), "0");
				nvram_set(strcat_r(prefix, "infra", tmp), "1");
				nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#elif defined(RTCONFIG_REALTEK)
				nvram_set(strcat_r(prefix, "maxassoc", tmp), "31");
#else
				nvram_set(strcat_r(prefix, "maxassoc", tmp), "128");
#endif
				nvram_set(strcat_r(prefix, "mode", tmp), "ap");
				//nvram_set(strcat_r(prefix, "net_reauth", tmp), "3600");
				nvram_set(strcat_r(prefix, "net_reauth", tmp), "36000");
				nvram_set(strcat_r(prefix, "preauth", tmp), "");
				nvram_set(strcat_r(prefix, "radio", tmp), "1");
				nvram_set(strcat_r(prefix, "radius_ipaddr", tmp), "");
				nvram_set(strcat_r(prefix, "radius_key", tmp), "");
				nvram_set(strcat_r(prefix, "radius_port", tmp), "1812");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#elif defined(RTCONFIG_REALTEK)
#else
				nvram_set(strcat_r(prefix, "sta_retry_time", tmp), "5");
				nvram_set(strcat_r(prefix, "wfi_enable", tmp), "0");
				nvram_set(strcat_r(prefix, "wfi_pinmode", tmp), "0");
#endif
				nvram_set(strcat_r(prefix, "wme", tmp), "auto");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "wme_bss_disable", tmp), "0");
#endif
#ifdef RTCONFIG_EMF
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp),
					nvram_get(strcat_r(pprefix, "wmf_bss_enable", tmp)));
#else
				nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), "0");
#endif
				nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), "3600");
#ifdef RTCONFIG_RALINK
#elif defined(RTCONFIG_QCA)
#else
				nvram_set(strcat_r(prefix, "wps_mode", tmp), "disabled");
#endif
				nvram_set(strcat_r(prefix, "expire", tmp), "0");
				nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");
			}
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
			if (is_router_mode())					// Only limite access of Guest network in Router mode
			if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			{
				if (strcmp(word, "rai0") == 0)
				{
					int vlan_id;
					if (nvram_match(strcat_r(prefix, "lanaccess", tmp), "off"))
						vlan_id = inic_vlan_id++;	// vlan id for no LAN access
					else
						vlan_id = 0;			// vlan id allow LAN access
					if (vlan_id)
						pNic += sprintf(pNic, "vlan%d ", vlan_id);
				}
			}
#endif
		}

		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		if (strlen(wlx_vifnames))
		{
#if defined(RTCONFIG_AMAS)
			if(strlen(sta_ifnames) == 0) 
				strncpy(sta_ifnames, wlx_vifnames, strlen(wlx_vifnames));
			else
				sprintf(sta_ifnames, "%s %s", sta_ifnames, wlx_vifnames);
#endif			
			nvram_set(strcat_r(prefix, "vifs", tmp), wlx_vifnames);
			
			if (strlen(wl_vifnames)) {
				sprintf(wl_vifnames2, "%s %s", wl_vifnames, wlx_vifnames);
				strlcpy(wl_vifnames, wl_vifnames2, sizeof(wl_vifnames));
			} else {
				sprintf(wl_vifnames, "%s", wlx_vifnames);

#if defined(RTCONFIG_CONCURRENTREPEATER)
					if (nvram_get_int("wlc_express") != 0) {
						nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
					}
#endif
			}
		}
		else {
			nvram_set(strcat_r(prefix, "vifs", tmp), "");
#if defined(RTCONFIG_CONCURRENTREPEATER)
#ifndef RTCONFIG_REALTEK
			if (nvram_get_int("wlc_express") != 0) {
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
				nvram_set(strcat_r(prefix, "ssid", tmp), "");
			}
#endif
#endif
		}

		unit++;
		subunit_x = 0;
	}
	
#if defined(RTCONFIG_AMAS)
#if defined(RTCONFIG_CONCURRENTREPEATER)
	nvram_set("sta_ifnames", sta_ifnames);
	nvram_set("sta_phy_ifnames", sta_ifnames);
#endif	
#endif
	
#if defined (RTCONFIG_WLMODULE_RT3352_INIC_MII)
	nvram_set("nic_lan_ifnames", nic_lan_ifnames); //reset value
	if (strlen(nic_lan_ifnames))
	{
		sprintf(wl_vifnames, "%s %s", wl_vifnames, nic_lan_ifnames);	//would be add to "lan_ifnames" in following code.
	}
#endif
	if (strlen(wl_vifnames))
	{
		strcpy(lan_ifnames, nvram_safe_get("lan_ifnames"));
		foreach (word, wl_vifnames, next)
			add_to_list(word, lan_ifnames, sizeof(lan_ifnames));

		nvram_set("lan_ifnames", lan_ifnames);
	}

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	reset_psr_hwaddr();
#endif
}

/* for WPS Reset */
void
wl_default_wps(int unit)
{
	struct nvram_tuple *t;
	char prefix[]="wlXXXXXX_", tmp[100];
#ifdef RTCONFIG_CONCURRENTREPEATER
	char *pssid = NULL, tmp2[100]={0};
#endif
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	for (t = router_defaults; t->name; t++) {
		if (strncmp(t->name, "wl_", 3) &&
			strncmp(t->name, prefix, 4))
			continue;

		if (strcmp(t->name, "wl_ssid") &&
		    strcmp(t->name, "wl_auth_mode_x") &&
		    strcmp(t->name, "wl_wep_x") &&
		    strcmp(t->name, "wl_key") &&
		    strcmp(t->name, "wl_key1") &&
		    strcmp(t->name, "wl_key2") &&
		    strcmp(t->name, "wl_key3") &&
		    strcmp(t->name, "wl_key4") &&
		    strcmp(t->name, "wl_phrase_x") &&
		    strcmp(t->name, "wl_crypto") &&
		    strcmp(t->name, "wl_wpa_psk"))
			continue;

		if (!strncmp(t->name, "wl_", 3)) {
			nvram_set(strcat_r(prefix, &t->name[3], tmp), t->value);
		}
		else
			nvram_set(t->name, t->value);
	}

#ifdef RTCONFIG_CONCURRENTREPEATER
				pssid = nvram_default_get(strcat_r(prefix, "ssid", tmp));
				if (strlen(pssid)){
					sprintf(tmp2, "%s", pssid);
					nvram_set(strcat_r(prefix, "ssid", tmp), tmp2);
				}
#endif
}

void
restore_defaults_wifi(int all)
{
	int rev3 = 0;
	char ssid[32];
	int defpsk = strlen(nvram_safe_get("wifi_psk")) && nvram_contains_word("rc_support", "defpsk");
	char word[256], *next;
	int unit, subunit;
	unsigned int max_mssid;
	char prefix[]="wlXXXXXX_", tmp[100];

#ifdef RTCONFIG_NEWSSID_REV2
	rev3 = 1;
#endif
	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		max_mssid = num_of_mssid_support(unit);

		strlcpy(ssid, get_default_ssid(unit, 0), sizeof(ssid));
#ifndef RTCONFIG_SSID_AMAPS
		if (defpsk
			|| (rev3
#ifdef RTAC68U
			&& is_ssid_rev3_series()
#endif
		))
#endif
			nvram_set(strcat_r(prefix, "ssid", tmp), ssid);

		if (defpsk)
		{
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
			nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wifi_psk"));
		}

		if (all)
		for (subunit = 1; subunit < max_mssid+1; subunit++) {
			snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);

			strlcpy(ssid, get_default_ssid(unit, subunit), sizeof(ssid));
#ifndef RTCONFIG_SSID_AMAPS
			if (defpsk
				|| (rev3
#ifdef RTAC68U
				&& is_ssid_rev3_series()
#endif
			))
#endif
				nvram_set(strcat_r(prefix, "ssid", tmp), ssid);

			if (defpsk)
			{
				nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "psk2");
				nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
				nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wifi_psk"));
			}
		}

		unit++;
	}

#ifdef RTCONFIG_SINGLE_SSID
	if (strlen(nvram_safe_get("wifi_psk")))
#else
	if (defpsk)
#endif
	nvram_set("w_Setting", "1");
}

void
wl_defaults_wps(void)
{
	char word[256], *next;
	int unit;

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
		wl_default_wps(unit);
		unit++;
	}

#ifdef RTCONFIG_TCODE
	restore_defaults_wifi(0);
#endif
}

void
restore_defaults_lan(int restore_defaults)
{
	//modify by sherry 2016.2.3
	if(restore_defaults){
		//if (!strcmp(nvram_safe_get("territory_code"), "US/01"))
		if (nvram_contains_word("rc_support", "defip"))
		{
			nvram_set("lan_ipaddr", "192.168.50.1");
			nvram_set("lan_ipaddr_rt", "192.168.50.1");
			nvram_set("dhcp_start", "192.168.50.2");
			nvram_set("dhcp_end", "192.168.50.254");
		}
	}
}

/* assign none-exist value or inconsist value */
void
lan_defaults(int restore_defaults)
{
	if (is_router_mode() && nvram_invmatch("lan_proto", "static"))
		nvram_set("lan_proto", "static");
#ifdef RTCONFIG_TCODE
	restore_defaults_lan(restore_defaults);
#endif
}

#ifdef RTCONFIG_TAGGED_BASED_VLAN
void
tagged_vlan_defaults(void)
{
	char *buf, *g, *p;
	char *mac, *ip, *gateway, *lan_ipaddr;
	char dhcp_staticlist[sizeof(DHCP_STATICLIST_EXAMPLE) * STATIC_MAC_IP_BINDING_PER_LAN + 8];			/* 2240 + 8 */
	char subnet_rulelist[(sizeof(SUBNET_RULE_EXAMPLE) + sizeof(SUBNET_STATICLIST_EXAMPLE) * STATIC_MAC_IP_BINDING_PER_VLAN) * (VLAN_MAX_NUM - 1) + sizeof(dhcp_staticlist)];	/* 2954 + 2240 + 8 */

	memset(subnet_rulelist, 0, sizeof(subnet_rulelist));
	memset(dhcp_staticlist, 0, sizeof(dhcp_staticlist));
	nvram_set("vlan_enable", "0");
	nvram_set("vlan_rulelist", "<1>1>0>0000>00FF00FF>007F>007F>default>1>0");
	nvram_set("vlan_if_list","00>00FF>007F>007F");
	nvram_set("vlan_pvid_list","1>1>1>1>1>1>1>1");

	g = buf = strdup(nvram_default_get("dhcp_staticlist")? : "");
	while (buf) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &mac, &ip)) != 2) continue;
		if(strlen(dhcp_staticlist) != 0)
			strcat(dhcp_staticlist, ";");
		strcat(dhcp_staticlist, mac);
		strcat(dhcp_staticlist, " ");
		strcat(dhcp_staticlist, ip);
	}
	free(buf);

	lan_ipaddr = nvram_safe_get("lan_ipaddr");
	gateway = nvram_safe_get("dhcp_gateway_x");
	gateway = (*gateway && inet_addr(gateway)) ? gateway : lan_ipaddr;

	sprintf(subnet_rulelist, "<%s>%s>%s>%s>%s>%s>%s>%s>%s>%s>%s", gateway,
		nvram_default_get("lan_netmask"), nvram_default_get("dhcp_enable_x"), nvram_default_get("dhcp_start"),
		nvram_default_get("dhcp_end"), nvram_default_get("dhcp_lease"), nvram_default_get("lan_domain"),
		nvram_default_get("dhcp_dns1_x"), nvram_default_get("dhcp_wins_x"), nvram_default_get("dhcp_static_x"), dhcp_staticlist);

	nvram_set("subnet_rulelist", subnet_rulelist);
}
#endif
/* assign none-exist value */
void
wan_defaults(void)
{
	struct nvram_tuple *t;
	char prefix[]="wanXXXXXX_", tmp[100];
	char word[256], *next;
	int unit;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		nvram_set(strcat_r(prefix, "ifname", tmp), "");

		for(t = router_defaults; t->name; ++t){
			if(strncmp(t->name, "wan_", 4)!=0) continue;

			if(!nvram_get(strcat_r(prefix, &t->name[4], tmp))){
				nvram_set(tmp, t->value);
			}
		}
	}

	unit = WAN_UNIT_FIRST;
	foreach(word, nvram_safe_get("wan_ifnames"), next){
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		if(dualwan_unit__nonusbif(unit))
			nvram_set(strcat_r(prefix, "ifname", tmp), word);

		++unit;
	}

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6) {
		unit = WAN_UNIT_IPTV;
		foreach (word, nvram_safe_get("iptv_wan_ifnames"), next) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			nvram_set(strcat_r(prefix, "ifname", tmp), word);
			if (nvram_match("switch_wantag", "singtel"))
				nvram_set(strcat_r(prefix, "vendorid", tmp),"S_iptvsys");

			for (t = router_defaults; t->name; t++) {
				if (strncmp(t->name, "wan_", 4) != 0) continue;

				if (!nvram_get(strcat_r(prefix, &t->name[4], tmp))) {
					nvram_set(tmp, t->value);
				}
			}
			++unit;
		}
	}
#endif
}

void
restore_defaults_module(char *prefix)
{
	struct nvram_tuple *t;

	if (strcmp(prefix, "wl_")==0) wl_defaults();
	else if (strcmp(prefix, "wan_")==0) wan_defaults();
	else {
		for (t = router_defaults; t->name; t++) {
			if (strncmp(t->name, prefix, strlen(prefix))!=0) continue;
			nvram_set(t->name, t->value);
		}
	}
}

int restore_defaults_g = 0;

#ifdef RTCONFIG_USB
#ifndef RTCONFIG_PERMISSION_MANAGEMENT
// increase this number each time need to reset usb related setting
// acc_num/acc_username/acc_password to acc_num/acc_list
#define USBCTRLVER 1

void usbctrl_default()
{
	char tmp[256], *ptr;
	int buf_len = 0;
	int usbctrlver;
	int acc_num, acc_num_ret, i;
	char *acc_user, *acc_password;
	char acc_nvram_username[32], acc_nvram_password[32];
	char **account_list;
	char *http_passwd;
#ifdef RTCONFIG_NVRAM_ENCRYPT
	char enc_passwd_buf[256];
#endif

	usbctrlver = nvram_get_int("usbctrlver");
	if (usbctrlver != USBCTRLVER || nvram_get("acc_username0") != NULL) {
		nvram_set_int("usbctrlver", USBCTRLVER);
		acc_num = nvram_get_int("acc_num");
		acc_num_ret = 0;

		char ascii_passwd[84];

		memset(ascii_passwd, 0, 84);

		http_passwd = nvram_safe_get("http_passwd");
#ifdef RTCONFIG_NVRAM_ENCRYPT
		int declen = pw_dec_len(http_passwd);
		char dec_passwd[declen];
		memset(dec_passwd, 0, sizeof(dec_passwd));
		pw_dec(http_passwd, dec_passwd);

		char_to_ascii_safe(ascii_passwd, dec_passwd, 84);

		int enclen = pw_enc_blen(ascii_passwd);
		char enc_passwd[enclen];
		memset(enc_passwd, 0, sizeof(enc_passwd));
		pw_enc(ascii_passwd, enc_passwd);
		memset(ascii_passwd, 0, 84);
		strlcpy(ascii_passwd, enc_passwd, sizeof(ascii_passwd));
#else
		char_to_ascii_safe(ascii_passwd, http_passwd, 64);
#endif
		buf_len = snprintf(tmp, sizeof(tmp), "%s>%s", nvram_safe_get("http_username"), ascii_passwd); // default account.

		ptr = tmp+buf_len;
		acc_num_ret = 1;

		for(i = 0; i < acc_num; i++) {
			sprintf(acc_nvram_username, "acc_username%d", i);
			if ((acc_user = nvram_get(acc_nvram_username)) == NULL) continue;

			sprintf(acc_nvram_password, "acc_password%d", i);
			if ((acc_password = nvram_get(acc_nvram_password)) == NULL) continue;
			memset(ascii_passwd, 0, 84);
			char_to_ascii_safe(ascii_passwd, acc_password, 84);
#ifdef RTCONFIG_NVRAM_ENCRYPT
			memset(enc_passwd_buf, 0, sizeof(enc_passwd_buf));
			pw_enc(ascii_passwd, enc_passwd_buf);
			 memset(ascii_passwd, 0, 84);
			strlcpy(ascii_passwd, enc_passwd_buf, sizeof(ascii_passwd));
#endif
			if (strcmp(acc_user, "admin")) {
				buf_len += snprintf(ptr, 256-buf_len, "<%s>%s", acc_user, ascii_passwd);
				ptr = tmp+buf_len;
				acc_num_ret++;
			}

			nvram_unset(acc_nvram_username);
			nvram_unset(acc_nvram_password);
		}

		nvram_set_int("acc_num", acc_num_ret);
		nvram_set("acc_list", tmp);
	}

	acc_num = get_account_list(&acc_num, &account_list);
	if (acc_num >= 0 && acc_num != nvram_get_int("acc_num"))
		nvram_set_int("acc_num", acc_num);
	free_2_dimension_list(&acc_num, &account_list);
}
#endif
#endif

#define RESTORE_DEFAULTS() \
	(!nvram_match("restore_defaults", "0") || \
	 !nvram_match("nvramver", RTCONFIG_NVRAM_VER))	// nvram version mismatch

#ifdef RTCONFIG_USB_MODEM
void clean_modem_state(int modem_unit, int flag){
	int unit;
	char tmp2[100], prefix2[32];

	for(unit = MODEM_UNIT_FIRST; unit < MODEM_UNIT_MAX; ++unit){
		if(modem_unit != -1 && modem_unit != unit)
			continue;

		usb_modem_prefix(unit, prefix2, sizeof(prefix2));

		// Need to unset after the SIM is removed.
		// auto APN
		nvram_unset(strcat_r(prefix2, "auto_lines", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_running", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_imsi", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_country", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_isp", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_apn", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_spn", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_dialnum", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_user", tmp2));
		nvram_unset(strcat_r(prefix2, "auto_pass", tmp2));

		nvram_unset(strcat_r(prefix2, "act_signal", tmp2));
		nvram_unset(strcat_r(prefix2, "act_cellid", tmp2));
		nvram_unset(strcat_r(prefix2, "act_lac", tmp2));
		nvram_unset(strcat_r(prefix2, "act_rsrq", tmp2));
		nvram_unset(strcat_r(prefix2, "act_rsrp", tmp2));
		nvram_unset(strcat_r(prefix2, "act_rssi", tmp2));
		nvram_unset(strcat_r(prefix2, "act_sinr", tmp2));
		nvram_unset(strcat_r(prefix2, "act_band", tmp2));
		nvram_unset(strcat_r(prefix2, "act_operation", tmp2));
		nvram_unset(strcat_r(prefix2, "act_provider", tmp2));
		nvram_unset(strcat_r(prefix2, "act_imsi", tmp2));
		nvram_unset(strcat_r(prefix2, "act_iccid", tmp2));
		nvram_unset(strcat_r(prefix2, "act_auth", tmp2));
		nvram_unset(strcat_r(prefix2, "act_auth_pin", tmp2));
		nvram_unset(strcat_r(prefix2, "act_auth_puk", tmp2));
		nvram_unset(strcat_r(prefix2, "act_ip", tmp2));
	}

	nvram_unset("modem_sim_order");
	nvram_unset("modem_bytes_rx");
	nvram_unset("modem_bytes_tx");
	nvram_unset("modem_bytes_rx_reset");
	nvram_unset("modem_bytes_tx_reset");

	nvram_unset("modem_sms_alert_send");
	nvram_unset("modem_sms_limit_send");

	if(flag == 2)
		return;

	for(unit = MODEM_UNIT_FIRST; unit < MODEM_UNIT_MAX; ++unit){
		if(modem_unit != -1 && modem_unit != unit)
			continue;

		usb_modem_prefix(unit, prefix2, sizeof(prefix2));

		if(flag){
			nvram_unset(strcat_r(prefix2, "act_path", tmp2));
			nvram_unset(strcat_r(prefix2, "act_type", tmp2));
			nvram_unset(strcat_r(prefix2, "act_dev", tmp2));
		}

		// modem.
		nvram_unset(strcat_r(prefix2, "act_int", tmp2));
		nvram_unset(strcat_r(prefix2, "act_bulk", tmp2));
		nvram_unset(strcat_r(prefix2, "act_vid", tmp2));
		nvram_unset(strcat_r(prefix2, "act_pid", tmp2));
		nvram_unset(strcat_r(prefix2, "act_sim", tmp2));
		nvram_unset(strcat_r(prefix2, "act_imei", tmp2));
		nvram_unset(strcat_r(prefix2, "act_tx", tmp2));
		nvram_unset(strcat_r(prefix2, "act_rx", tmp2));
		nvram_unset(strcat_r(prefix2, "act_hwver", tmp2));
		nvram_unset(strcat_r(prefix2, "act_swver", tmp2));
		nvram_unset(strcat_r(prefix2, "act_scanning", tmp2));
		nvram_unset(strcat_r(prefix2, "act_startsec", tmp2));
		nvram_unset(strcat_r(prefix2, "act_simdetect", tmp2));
		nvram_unset(strcat_r(prefix2, "act_num", tmp2));
#ifdef RTCONFIG_USB_SMS_MODEM
		nvram_unset(strcat_r(prefix2, "act_smsc", tmp2));
#endif
	}

	// modem state.
	nvram_unset("g3state_pin");
	nvram_unset("g3state_z");
	nvram_unset("g3state_q0");
	nvram_unset("g3state_cd");
	nvram_unset("g3state_class");
	nvram_unset("g3state_mode");
	nvram_unset("g3state_apn");
	nvram_unset("g3state_dial");
	nvram_unset("g3state_conn");

	// modem error.
	nvram_unset("g3err_pin");
	nvram_unset("g3err_apn");
	nvram_unset("g3err_conn");
	nvram_unset("g3err_imsi");
}
#endif

void set_default_wifi_rate(void)
{
	char *path="/etc/wifi_rate";
	char buf[128], output[128];
	char *tmp, *delim=" ";
	int offset=0;

	if (check_if_file_exist(path)) return;

	memset(buf, '\0', sizeof(buf));
	memset(output, '\0', sizeof(output));

	strncpy(buf, nvram_safe_get("wl_ifnames"), sizeof(buf));
	tmp = strtok(buf, delim);
	while (tmp!=NULL) {
		offset += snprintf(output+offset, sizeof(output)-offset, "%s:%s\n", tmp, !strncmp(tmp+3, "0", 1)?"400":"866.7");
		tmp = strtok(NULL, delim);
	}

	f_write_string(path, output, 0, 0);
}

/* Clean lanX_ifname and lanX_ifname for vlan */
void clean_vlan_ifnames(void)
{
	int i = 0;
	char nv[32];

	/* Check the ifname of lan_ifnames whether existing in other lanX_ifnames for vlan */
	for (i = 1; i <= VLAN_MAXVID; i++) {
		/* Unset lanX_ifname */
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_ifname", i);
		nvram_unset(nv);

		/* Unset lanX_ifnames */
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_ifnames", i);
		nvram_unset(nv);

		/* Unset lanX_subnet */
		memset(nv, 0x0, sizeof(nv));
		snprintf(nv, sizeof(nv), "lan%d_subnet", i);
		nvram_unset(nv);
	}

	nvram_unset("vlan_index");
	nvram_unset("lan_ifnames_t");
}

void
convert_defaults()
{
	if (nvram_get("wl_TxPower")) {
#ifdef RTCONFIG_RALINK
		nvram_set_int("wl0_txpower", nvram_get_int("wl0_TxPower"));
		nvram_set_int("wl1_txpower", nvram_get_int("wl1_TxPower"));
#else
		nvram_set_int("wl0_txpower", min(100 * nvram_get_int("wl0_TxPower") / 80, 100));
		nvram_set_int("wl1_txpower", min(100 * nvram_get_int("wl1_TxPower") / 80, 100));
#endif
		nvram_unset("wl_TxPower");
		nvram_unset("wl0_TxPower");
		nvram_unset("wl1_TxPower");

		nvram_commit();
	} else if (nvram_get("dms_rescan")) {
		nvram_unset("dms_rescan");
		nvram_commit();
	}
}

void
misc_defaults(int restore_defaults)
{
#ifdef RTCONFIG_USB
	char prefix[32];
	int i;
#if RTCONFIG_USB_MODEM
	char tmp[100];
	int modem_unit;
#endif

	// default for USB state control variables. {
	for(i = 1; i <= MAX_USB_PORT; ++i) { // MAX USB port number is 3.
		snprintf(prefix, 32, "usb_led%d", i);
		nvram_unset(prefix);
	}

	/* Unset all usb_path related nvram variables, e.g. usb_path1, usb_path2.4.3, usb_path_. */
	char buf[MAX_NVRAM_SPACE];
	char *name, *p, w;

#ifdef RTCONFIG_NVRAM_FILE
	// nvram_getall cannot read the correct nvram here.
	f_read("/tmp/nvram.txt", buf, sizeof(buf));
#else
	nvram_getall(buf, sizeof(buf));
#endif
	for(name = buf; *name; name += strlen(name)+1){
		if(strncmp(name, "usb_path", 8) && strncmp(name, "ignore_nas_service_", 19))
			continue;

		if(strstr(name, "_diskmon"))
			continue;

		if((p = strchr(name, '=')) != NULL){
			w = *p;
			*p = '\0';
		}
		else
			w = -1;

		nvram_unset(name);

		if(w != -1)
			*p = w;
	}

#ifdef RTCONFIG_USB_MODEM
#ifndef RTCONFIG_INTERNAL_GOBI
	// can't support all kinds of modem.
	nvram_set("modem_mode", "0");
#endif

#ifdef RTCONFIG_USB_MULTIMODEM
	clean_modem_state(-1, 1);
#else
	clean_modem_state(MODEM_UNIT_FIRST, 1);
#endif
	// only be unset at boot. {
	for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
		usb_modem_prefix(modem_unit, prefix, sizeof(prefix));

		nvram_unset(strcat_r(prefix, "act_reset", tmp));
		nvram_unset(strcat_r(prefix, "act_reset_path", tmp));
	}
	// only be unset at boot. }

#ifndef RTCONFIG_INTERNAL_GOBI
	int sim_num = atoi(nvram_safe_get("modem_sim_num"));
	for(i = 1; i <= sim_num; ++i){
		snprintf(prefix, 32, "modem_sim_imsi%d", i);
		nvram_unset(prefix);
	}
#endif
#endif // RTCONFIG_USB_MODEM
#endif	/* RTCONFIG_USB */
	// default for USB state control variables. }

	nvram_set_int("wlready", 0);
	nvram_set_int("init_wl_re", 0);

	/*++ Clean temporary variables of WTFast ++*/
#ifdef RTCONFIG_WTFAST
	nvram_unset("wtf_login");
	nvram_unset("wtf_account_type");
	nvram_unset("wtf_max_clients");
	nvram_unset("wtf_days_left");
	nvram_unset("wtf_game_list");
	nvram_unset("wtf_server_list");
	nvram_unset("wtf_session_hash");
#endif
	/*-- Clean temporary variables of WTFast --*/

	/* Clean lanX_ifname and lanX_ifname for vlan */
	clean_vlan_ifnames();

	/* some default state values is model deps, so handled here*/
	switch(get_model()) {
#ifdef RTCONFIG_BCMARM
#ifdef RTAC56U
		case MODEL_RTAC56U:
			nvram_set(ATE_BRCM_FACTORY_MODE_STR(), "0");
			if(chk_ate_ccode())
				fix_ate_ccode();
#endif
		case MODEL_RTAC56S:
			if (After(get_blver(nvram_safe_get("bl_version")), get_blver("1.0.2.4")))	// since 1.0.2.5
				nvram_set("reboot_time", "140");// default is 70 sec

			if (!After(get_blver(nvram_safe_get("bl_version")), get_blver("1.0.2.6"))) {	// before 1.0.2.7
				nvram_set("wl0_itxbf",	"0");
				nvram_set("wl1_itxbf",	"0");
				nvram_set("wl_itxbf",	"0");
			}

			break;

		case MODEL_GTAC5300:
		case MODEL_RTAC86U:
			nvram_set("reboot_time", "90");
			break;
		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC3100:
			nvram_set("reboot_time", "140");
			break;
		case MODEL_RTAC3200:
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
			nvram_set("reboot_time", "80");

			break;
		case MODEL_RTAC68U:
#ifdef RTCONFIG_DUAL_TRX
			nvram_set("reboot_time", "90");		// default is 70 sec
#else
			if (After(get_blver(nvram_safe_get("bl_version")), get_blver("1.0.1.6")))	// since 1.0.1.7
				nvram_set("reboot_time", "140");// default is 70 sec
			else
				nvram_set("reboot_time", "95");	// extend default to 95
#endif
			break;
		case MODEL_RTAC87U:
			nvram_set("reboot_time", "160");
			break;
		case MODEL_RTN18U:
			if (After(get_blver(nvram_safe_get("bl_version")), get_blver("2.0.0.5")))
				nvram_set("reboot_time", "140");// default is 70 sec
			break;
		case MODEL_DSLAC68U:
			nvram_set("reboot_time", "170");	// default is 70 sec
			break;
#endif
		case MODEL_RTN14UHP:
			nvram_set("reboot_time", "85");		// default is 70 sec
			break;
		case MODEL_RTN10U:
			nvram_set("reboot_time", "85");		// default is 70 sec
			break;
#ifdef RTCONFIG_RALINK
#ifdef RTCONFIG_DSL
		case MODEL_DSLN55U:
			nvram_set("reboot_time", "75");		// default is 70 sec
			break;
#else
		case MODEL_RTAC52U:
			nvram_set("reboot_time", "80");		// default is 70 sec
			break;
		case MODEL_RTN56UB1:
		case MODEL_RTN56UB2:
		case MODEL_RTAC1200GU:
		case MODEL_RTN54U:
		case MODEL_RTAC54U:
		case MODEL_RTAC1200HP:
		case MODEL_RTAC51U:
		case MODEL_RTN65U:
		case MODEL_RTAC85U:
			nvram_set("reboot_time", "90");		// default is 70 sec
			break;
#endif
#endif
#ifdef RTCONFIG_QCA
		case MODEL_RT4GAC55U:
		case MODEL_RTAC55U:
		case MODEL_RTAC55UHP:
		case MODEL_PLN12:
		case MODEL_PLAC56:
		case MODEL_PLAC66U:
			nvram_set("reboot_time", "80");		// default is 70 sec
			break;

		case MODEL_RTAC58U:
		case MODEL_RT4GAC53U:
		case MODEL_RTAC82U:
			nvram_set("reboot_time", "100");	// default is 70 sec
			break;
		case MODEL_MAPAC1300:
		case MODEL_VZWAC1300:
		case MODEL_MAPAC1750:
			nvram_set("reboot_time", "80");		// default is 70 sec
			break;
		case MODEL_MAPAC3000:
		case MODEL_MAPAC2200:
			nvram_set("reboot_time", "80");		// default is 70 sec
			break;


		case MODEL_RTAC88N:
		case MODEL_BRTAC828:
		case MODEL_RTAC88S:
		case MODEL_RTAD7200:
			nvram_set("reboot_time", "100");	// default is 70 sec
#if defined(RTCONFIG_LETSENCRYPT)
			if (nvram_match("le_acme_auth", "dns"))
				nvram_set("le_acme_auth", "http");
#endif
			break;
#endif
#ifdef RTCONFIG_REALTEK
		//case MODEL_RTRTL8198C:
		case MODEL_RPAC68U:
		case MODEL_RPAC53:
			nvram_set("reboot_time", "90");
			break;
#endif
#ifdef RTCONFIG_ALPINE
		case MODEL_GTAC9600:
			nvram_set("reboot_time", "90");
			break;
#endif
#ifdef RTCONFIG_LANTIQ
		case MODEL_BLUECAVE:
			nvram_set("reboot_time", "160");
			break;
#endif			
		default:
			break;
	}

#ifdef RTCONFIG_WEBDAV
	webdav_account_default();
#endif

	nvram_set("success_start_service", "0");

#ifdef RTCONFIG_AMAS
	nvram_set("start_service_ready", "0");
#endif

#if defined(RTAC66U) || defined(BCM4352)
	nvram_set("led_5g", "0");
#endif
#if !defined(RTCONFIG_HAS_5G)
	nvram_unset("wl1_ssid");
#endif	/* ! RTCONFIG_HAS_5G */

#if defined(RTCONFIG_QCA)
	nvram_unset("skip_gen_ath_config");
#endif

#ifdef RTCONFIG_USB
	if (nvram_match("smbd_cpage", ""))
	{
		if (nvram_match("wl0_country_code", "CN"))
			nvram_set("smbd_cpage", "936");
		else if (nvram_match("wl0_country_code", "TW"))
			nvram_set("smbd_cpage", "950");
	}
#endif
	/* reset ntp status */
	nvram_set("ntp_ready", "0");
	nvram_set("svc_ready", "0");
#ifdef RTCONFIG_QTN
	nvram_unset("qtn_ready");
#endif
	nvram_set("mfp_ip_requeue", "");
#if defined(RTAC68U) || defined(RTCONFIG_FORCE_AUTO_UPGRADE)
	nvram_set_int("auto_upgrade", 0);
	nvram_unset("fw_check_period");
#endif
#ifdef RTAC68U
	nvram_unset("fw_enc_crc");
#endif

	if (restore_defaults)
	{
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)
		nvram_set("jffs2_clean_fs", "1");
#elif defined(RTCONFIG_YAFFS)
		nvram_set("yaffs_clean_fs", "1");
#elif defined(RTCONFIG_UBIFS)
		nvram_set("ubifs_clean_fs", "1");
#endif
#ifdef RTCONFIG_QTN
		nvram_set("qtn_restore_defaults", "1");
#endif
	}
#ifdef RTCONFIG_QTN
	else nvram_unset("qtn_restore_defaults");
#endif

#ifdef WEB_REDIRECT
	nvram_set("freeze_duck", "0");
#endif
	nvram_unset("ateCommand_flag");
	nvram_unset("ateUpgarde_flag");

#ifdef RTCONFIG_VPNC
	nvram_set_int("vpnc_state_t", 0);
	nvram_set_int("vpnc_sbstate_t", 0);
#endif

#if (defined(PLN12) || defined(PLAC56))
	nvram_set("plc_ready", "0");
#elif defined(PLAC66)
	nvram_set("plc_ready", "1");
#endif

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
#if defined(RTCONFIG_BCMARM)  && (!defined(RTCONFIG_NEW_APP_ARM))
	nvram_set("apps_ipkg_old", "0");
	nvram_set("apps_new_arm", "0");//2016.7.1 sherry new oleg arm
	nvram_set("apps_install_folder", "asusware.arm");
	nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/arm/stable");
#elif defined(RTCONFIG_NEW_APP_ARM) //2016.7.1 sherry new oleg arm{
	nvram_set("apps_ipkg_old", "0");
	nvram_set("apps_new_arm", "1");
	nvram_set("apps_install_folder", "asusware.arm");
	nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/arm/stable");
	//end sherry add}
#elif defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ)
	nvram_set("apps_ipkg_old", "0");
	nvram_set("apps_new_arm", "0"); //2016.7.1 sherry new oleg arm
	nvram_set("apps_install_folder", "asusware.mipsbig");
	nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsbig/stable");
#else
	if(!strcmp(get_productid(), "VSL-N66U") || !strcmp(get_productid(), "RP-AC68U")){
		nvram_set("apps_ipkg_old", "0");
		nvram_set("apps_install_folder", "asusware.mipsbig");
		nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsbig/stable");
	}
	else{ // mipsel
		nvram_set("apps_install_folder", "asusware");
		if (nvram_match("apps_ipkg_old", "1"))
#ifdef RTCONFIG_HTTPS
			nvram_set("apps_ipkg_server", "https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT");
#else
			nvram_set("apps_ipkg_server", "http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT");
#endif
		else
			nvram_set("apps_ipkg_server", "http://nw-dlcdnet.asus.com/asusware/mipsel/stable");
	}
#endif
#endif

#if defined(BRTAC828) || defined(RTAD7200)
	/* Bonding and LAN as WAN are not compatible to each other.
	 * If LAN as WAN is enabled, turn of bonding automatically.
	 * UI will notify end-user before enabling LAN as WAN.
	 */
	if (strstr(nvram_safe_get("wans_dualwan"), "lan") != NULL) {
		nvram_set("lan_trunk_0", "0");
		nvram_set("lan_trunk_1", "0");
		nvram_set("lan_trunk_type", "0");
	}
#endif

#ifdef RTCONFIG_TUNNEL
	nvram_set("aae_support", "1");
#define AAE_ENABLE_AIHOME 2
#define AAE_EANBLE_AICLOUD 4
#ifdef RTCONFIG_AIHOME_TUNNEL
	nvram_set_int("aae_enable", (nvram_get_int("aae_enable") | AAE_ENABLE_AIHOME));
#endif
#ifdef RTCONFIG_AICLOUD_TUNNEL
	nvram_set_int("aae_enable", (nvram_get_int("aae_enable") | AAE_EANBLE_AICLOUD));
#endif
#endif

	nvram_unset("wps_reset");
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	nvram_set_int("AllLED", 1);
#endif
	nvram_unset("reload_svc_radio");
#ifdef RTCONFIG_AMAS
	nvram_unset("amesh_found_cap");
	nvram_unset("amesh_led");
#ifdef RTCONFIG_LANTIQ
	nvram_unset("amesh_hexdata");
#endif
#endif
}

/* ASUS use erase nvram to reset default only */
static void
restore_defaults(void)
{
	struct nvram_tuple *t;
	int restore_defaults;
	char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];
	int unit;
#ifdef RTCONFIG_DHDAP
	int i;
#endif
#if 1	/* TODO: define RTCONFIG_XXX for this */
	int skiplan = nvram_match("ci", "1");
#endif

	nvram_unset(ASUS_STOP_COMMIT);
	nvram_unset(LED_CTRL_HIPRIO);

	// Restore defaults if nvram version mismatch
	restore_defaults = RESTORE_DEFAULTS();

	/* Restore defaults if told to or OS has changed */
	if (!restore_defaults)
	{
		restore_defaults = !nvram_match("restore_defaults", "0");
#if defined(RTN56U)
		/* upgrade from firmware 1.x.x.x */
		if (nvram_get("HT_AutoBA") != NULL)
		{
			nvram_unset("HT_AutoBA");
			restore_defaults = 1;
		}
#endif
#if defined(RTCONFIG_REALTEK) && defined(RPAC55)
		char c;
		f_write_string("/proc/asus_ate", "restore 1", 0, 0);
		f_read("/proc/asus_ate", &c, 1);
		int restore_flag = c - '0';
		if (restore_flag == 1)
			restore_defaults = 1;
#endif
	}

#ifdef RTCONFIG_LANTIQ
	if (nvram_get_int("Ate_power_on_off_enable") != 0) {
		restore_defaults = 0;
	}
#endif

	if (restore_defaults)
		fprintf(stderr, "\n## Restoring defaults... ##\n");

	restore_defaults_g = restore_defaults;

	if (restore_defaults) {
#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
		unlink(NAND_FILE);
#endif
#ifdef RTCONFIG_WPS
		wps_restore_defaults();
#endif
		virtual_radio_restore_defaults();
	}
#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
	else
		confmtd_restore();
#endif

#ifdef RTCONFIG_USB
#ifndef RTCONFIG_PERMISSION_MANAGEMENT
	usbctrl_default();
#endif
#endif

	/* Restore defaults */
	for (t = router_defaults; t->name; t++) {
#if 1	/* TODO: define RTCONFIG_XXX for this */
		/* Skip change setting when run auto test */
		if (skiplan) {
			if (strcmp(t->name, "lan_ipaddr") == 0)
				continue;
		}
#endif

		if (restore_defaults || !nvram_get(t->name)) {
#if 0
			// add special default value handle here
			if (!strcmp(t->name, "computer_name") ||
				!strcmp(t->name, "dms_friendly_name") ||
				!strcmp(t->name, "daapd_friendly_name"))
				nvram_set(t->name, get_productid());
			else if (strcmp(t->name, "ct_max")==0) {
				// handled in init_nvram already
			}
			else
#endif
			nvram_set(t->name, t->value);
		}
	}

	wl_defaults();
	lan_defaults(restore_defaults);

	wan_defaults();

#ifdef RTCONFIG_DSL
	dsl_defaults();
#endif
#ifdef RTAC3200
	bsd_defaults();
#endif
#ifdef  __CONFIG_WBD__
	wbd_restore_defaults();
#endif /* __CONFIG_WBD__ */
#ifdef WLHOSTFBT
	fbt_restore_defaults();
#endif /* WLHOSTFBT */
#ifdef RTCONFIG_DHDAP
	for (i = 0; i < MAX_NVPARSE; i++) {
		snprintf(tmp, sizeof(tmp), "wl%d_cfg_maxassoc", i);
		nvram_unset(tmp);
	}
#endif

	if (restore_defaults) {
		uint memsize = 0;
		char pktq_thresh[8] = {0};
		char et_pktq_thresh[8] = {0};

		memsize = get_meminfo_item("MemTotal");

		if (memsize <= 32768) {
			/* Set to 512 as long as onboard memory <= 32M */
			sprintf(pktq_thresh, "512");
			sprintf(et_pktq_thresh, "512");
		}
		else if (memsize <= 65536) {
			/* We still have to set the thresh to prevent oom killer */
			sprintf(pktq_thresh, "1024");
			sprintf(et_pktq_thresh, "1536");
		}
		else {
			sprintf(pktq_thresh, "1024");
			/* Adjust the et thresh to 3300 as long as memory > 64M.
			 * The thresh value is based on benchmark test.
			 */
			sprintf(et_pktq_thresh, "3300");
		}

		nvram_set("wl_txq_thresh", pktq_thresh);
		nvram_set("et_txq_thresh", et_pktq_thresh);
#if defined(__CONFIG_USBAP__)
		nvram_set("wl_rpcq_rxthresh", pktq_thresh);
#endif
#ifdef RTCONFIG_BCMARM
#ifdef RTCONFIG_USB_XHCI
		if ((nvram_get_int("wlopmode") == 7) || factory_debug())
			nvram_set("usb_usb3", "1");
#endif
#endif
#ifdef RTCONFIG_TCODE
		restore_defaults_wifi(1);
		config_tcode(1);
#endif

#ifdef RTCONFIG_TAGGED_BASED_VLAN
		tagged_vlan_defaults();
#endif

	}

	/* Commit values */
	if (restore_defaults) {
		nvram_commit();
		fprintf(stderr, "done\n");
	}

	if (!nvram_match("buildno_org", nvram_safe_get("buildno")) ||
	    !nvram_match("rcno_org", nvram_safe_get("rcno")) ||
	    !nvram_match("extendno_org", nvram_safe_get("extendno")))
		nvram_commit();

	convert_defaults();

	/* default for state control variables */
	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);

		for (t = router_state_defaults; t->name; t++) {
			if (!strncmp(t->name, "wan_", 4))
				nvram_set(strcat_r(prefix, &t->name[4], tmp), t->value);
			else if (unit == WAN_UNIT_FIRST) // let non-wan nvram to be set one time.
				nvram_set(t->name, t->value);
		}
	}

	misc_defaults(restore_defaults);

#if defined(HND_ROUTER) && defined(RTCONFIG_HNDMFG)
	hnd_mfg_init();
#endif
}

/* Set terminal settings to reasonable defaults */
static void set_term(int fd)
{
	struct termios tty;

	tcgetattr(fd, &tty);

	/* set control chars */
	tty.c_cc[VINTR]  = 3;	/* C-c */
	tty.c_cc[VQUIT]  = 28;	/* C-\ */
	tty.c_cc[VERASE] = 127; /* C-? */
	tty.c_cc[VKILL]  = 21;	/* C-u */
	tty.c_cc[VEOF]   = 4;	/* C-d */
	tty.c_cc[VSTART] = 17;	/* C-q */
	tty.c_cc[VSTOP]  = 19;	/* C-s */
	tty.c_cc[VSUSP]  = 26;	/* C-z */

	/* use line dicipline 0 */
	tty.c_line = 0;

	/* Make it be sane */
	tty.c_cflag &= CBAUD|CBAUDEX|CSIZE|CSTOPB|PARENB|PARODD;
	tty.c_cflag |= CREAD|HUPCL|CLOCAL;


	/* input modes */
	tty.c_iflag = ICRNL | IXON | IXOFF;

	/* output modes */
	tty.c_oflag = OPOST | ONLCR;

	/* local modes */
	tty.c_lflag =
		ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;

	tcsetattr(fd, TCSANOW, &tty);
}

static int console_init(void)
{
	int fd;

	/* Clean up */
	ioctl(0, TIOCNOTTY, 0);
	close(0);
	close(1);
	close(2);
	setsid();

	/* Reopen console */
	if ((fd = open(_PATH_CONSOLE, O_RDWR)) < 0) {
		/* Avoid debug messages is redirected to socket packet if no exist a UART chip, added by honor, 2003-12-04 */
		open("/dev/null", O_RDONLY);
		open("/dev/null", O_WRONLY);
		open("/dev/null", O_WRONLY);
		perror(_PATH_CONSOLE);
		return errno;
	}
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);

	ioctl(0, TIOCSCTTY, 1);
	tcsetpgrp(0, getpgrp());
	set_term(0);

	return 0;
}

static pid_t run_shell(int timeout, int nowait)
{
	pid_t pid;
	int sig;

	/* Wait for user input */
	if (waitfor(STDIN_FILENO, timeout) <= 0) return 0;

	switch (pid = fork()) {
	case -1:
		perror("fork");
		return 0;
	case 0:
		/* Reset signal handlers set for parent process */
		for (sig = 0; sig < (_NSIG-1); sig++)
			signal(sig, SIG_DFL);

		/* Reopen console */
		console_init();

		/* Now run it.  The new program will take over this PID,
		 * so nothing further in init.c should be run. */
		execve(SHELL, (char *[]) { SHELL, NULL }, defenv);

		/* We're still here?  Some error happened. */
		perror(SHELL);
		exit(errno);
	default:
		if (nowait) {
			return pid;
		}
		else {
			waitpid(pid, NULL, 0);
			return 0;
		}
	}
}

int console_main(int argc, char *argv[])
{
	for (;;) run_shell(0, 0);

	return 0;
}

static void shutdn(int rb)
{
	int i;
	int act;
	sigset_t ss;

	_dprintf("shutdn rb=%d\n", rb);

	sigemptyset(&ss);
	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++)
		sigaddset(&ss, fatalsigs[i]);
	for (i = 0; i < sizeof(initsigs) / sizeof(initsigs[0]); i++)
		sigaddset(&ss, initsigs[i]);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	for (i = 30; i > 0; --i) {
		if (((act = check_action()) == ACT_IDLE) ||
		     (act == ACT_REBOOT) || (act == ACT_UNKNOWN)) break;
		_dprintf("Busy with %d. Waiting before shutdown... %d\n", act, i);
		sleep(1);
	}
	set_action(ACT_REBOOT);

	stop_wan();

	_dprintf("TERM\n");
	kill(-1, SIGTERM);
	sleep(3);
	sync();

	_dprintf("KILL\n");
	kill(-1, SIGKILL);
	sleep(1);
	sync();

	// TODO LED Status for LED
	setAllLedOff();

	sync(); sync(); sync();
#ifdef HND_ROUTER
	printf("\nStopping bcm_boot_launcher ...\n");
	system("bcm_boot_launcher stop");
#endif
	reboot(rb ? RB_AUTOBOOT : RB_HALT_SYSTEM);

	do {
		sleep(1);
	} while (1);
}

static void handle_fatalsigs(int sig)
{
	char *message = NULL;

	switch (sig) {
		case SIGILL: message = "Illegal instruction"; break;
		case SIGABRT: message = "Abort"; break;
		case SIGFPE: message = "Floating exception"; break;
		case SIGPIPE: message = "Broken pipe"; break;
		case SIGBUS: message = "Bus error"; break;
		case SIGSYS: message = "Bad system call"; break;
		case SIGTRAP: message = "Trace trap"; break;
		case SIGPWR: message = "Power failure"; break;
	}

	if (message)
		dbg("%s\n", message);
	else
		dbg("Caught fatal signal %d\n", sig);

	shutdn(-1);
}

/* Fixed the race condition & incorrect code by using sigwait()
 * instead of pause(). But SIGCHLD is a problem, since other
 * code: 1) messes with it and 2) depends on CHLD being caught so
 * that the pid gets immediately reaped instead of left a zombie.
 * Pidof still shows the pid, even though it's in zombie state.
 * So this SIGCHLD handler reaps and then signals the mainline by
 * raising ALRM.
 */
static void handle_reap(int sig)
{
	chld_reap(sig);
	raise(SIGALRM);
}


#ifdef RTCONFIG_SWMODE_SWITCH
/*
 * * Check
 * * 1.Phy Switch status.
 * * 2.if default, set the lan proto.
 * * */
void init_swmode()
{
#if defined(PLAC66U)
	if (!button_pressed(BTN_SWMODE_SW_ROUTER)) {
		nvram_set_int("sw_mode", SW_MODE_ROUTER);
		if (nvram_match("x_Setting", "0"))
			nvram_set("lan_proto", "dhcp");
	}
	else {
		nvram_set_int("sw_mode", SW_MODE_AP);
		if (nvram_match("x_Setting", "0"))
			nvram_set("lan_proto", "dhcp");
	}

	nvram_set_int("swmode_switch", button_pressed(BTN_SWMODE_SW_ROUTER));
#else
	if (!nvram_get_int("swmode_switch")) return;

	if (button_pressed(BTN_SWMODE_SW_REPEATER)) {
		nvram_set_int("sw_mode", SW_MODE_REPEATER);
		dbg("%s: swmode: repeater", LOGNAME);
		if (nvram_match("x_Setting", "0")) {
			nvram_set("lan_proto", "dhcp");
		}
	}else if (button_pressed(BTN_SWMODE_SW_AP)) {
		nvram_set_int("sw_mode", SW_MODE_AP);
		dbg("%s: swmode: ap", LOGNAME);
		if (nvram_match("x_Setting", "0")) {
			nvram_set("lan_proto", "dhcp");
		}
	}else if (button_pressed(BTN_SWMODE_SW_ROUTER)) {
		dbg("%s: swmode: router", LOGNAME);
		nvram_set_int("sw_mode", SW_MODE_ROUTER);
		nvram_set("lan_proto", "static");
	}else{
		dbg("%s: swmode: unknow swmode", LOGNAME);
	}
#endif	/* Model */
}
#endif	/* RTCONFIG_SWMODE_SWITCH */

#if 0
void conf_swmode_support(int model)
{
	switch (model) {
		case MODEL_RTAC66U:
		case MODEL_RTAC53U:
			nvram_set("swmode_support", "router repeater ap psta");
			dbg("%s: swmode: router repeater ap psta\n", LOGNAME);
			break;
		case MODEL_APN12HP:
			nvram_set("swmode_support", "repeater ap");
			dbg("%s: swmode: repeater ap\n", LOGNAME);
			break;
		default:
			nvram_set("swmode_support", "router repeater ap");
			dbg("%s: swmode: router repeater ap\n", LOGNAME);
			break;
	}
}
#endif
char *the_wan_phy()
{
#ifdef RTCONFIG_BCMFA
	if (FA_ON(fa_mode))
		return "vlan2";
	else
#endif
		return "eth0";
}

/**
 * Generic wan_ifnames / lan_ifnames / wl_ifnames / wl[01]_vifnames initialize helper
 * @wan:	WAN interface.
 * @lan:	LAN interface.
 * @wl_ifaces:	Wireless interfaces of each band, including 2.4G, 5G, 5G-2, and 60G.
 * @usb:	USB interface, can be NULL.
 * @ap_lan:	LAN interface in AP mode.
 * 		If model-specific code don't need to bridge WAN and LAN interface in AP mode,
 * 		customize interface name of LAN interface in this parameter.
 * 		e.g. RT-N65U:
 * 		Home gateway mode:	WAN = vlan2, LAN = vlan1.
 * 		AP mode:		WAN = N/A,   LAN = vlan1, switch is reconfigured by the "rtkswitch 8 100" command.
 * @dw_wan:	WAN interface in dual-wan mode.
 * 		e.g. RT-AC68U:
 * 		Default WAN interface of RT-AC68U is eth0.
 * 		If one of LAN ports is configured as a WAN port and WAN port is enabled,
 * 		WAN interface becomes "vlanX" (X = nvram_get("switch_wan0tagid")) or "vlan2".
 * @dw_lan:	LAN interface of LAN port that is used as a WAN interface.
 * 		In most cases, it is supposed a VLAN interface, e.g. "vlan3".
 * 		This interface is used as WAN interface (wanX_ifname) if and only if any below conditions true:
 * 		1) @force_dwlan is true.
 * 		2) WAN port is enabled too.
 * 		3) IPTV is enabled, Ralink/MTK platform only.
 * 		   Because only Ralink/MTK platform always use VLAN ID 2 as WAN and use VLAN tag swapping or similar trick
 * 		   to implement IPTV, whether IPTV is enabled or not.
 * 		So, if you really need to choose @dw_lan as WAN interface as long as LAN as WAN is enabled,
 * 		pass non-zero value to @force_dwlan.
 * @wan2:	The second WAN interface (not LAN port)
 *
 * 		e.g. RT-AC68U (1st WAN/2nd WAN):
 * 		WAN/none, WAN/usb, usb/WAN:
 * 			WAN:
 * 				(1) vlanX (X = nvram_get("switch_wan0tagid")), or
 * 				(2) the_wan_phy() = eth0 (X doesn't exist)
 *
 * 		LAN/none, LAN/usb, usb/LAN:
 * 			LAN:
 * 				the_wan_phy() = eth0
 *
 * 		WAN/LAN, LAN/WAN:
 * 			WAN:
 * 				(1) vlanX (X = nvram_get("switch_wan0tagid")), or
 * 				(2) vlan2
 * 			LAN:
 * 				vlan3
 *
 * 		char *wl_ifaces[WL_NR_BANDS];
 * 		memset(wl_ifaces, 0, sizeof(wl_ifaces));
 * 		wl_ifaces[WL_2G_BAND] = "eth1";
 * 		wl_ifaces[WL_5G_BAND] = "eth2";
 * 		set_basic_ifname_vars("eth0", "vlan1", wl_ifaces, "usb", NULL, "vlan2", "vlan3", NULL, 0);
 * @force_dwlan:If true, always choose dw_lan as LAN interface that is used as WAN, even WAN/LAN are enabled both.
 *
 * @return:
 * 	-1:	invalid parameter.
 *	0:	success
 */
#if !defined(CONFIG_BCMWL5) && !defined(RTN56U) && !defined(RTCONFIG_ALPINE) && !defined(RTCONFIG_LANTIQ)
static int set_basic_ifname_vars(char *wan, char *lan, char *wl_ifaces[WL_NR_BANDS], char *usb, char *ap_lan, char *dw_wan, char *dw_lan, char *wan2, int force_dwlan)
{
	char *wl2g, *wl5g, *wl5g2, *wl60g;
	int sw_mode = sw_mode();
	char buf[128], *wan2_orig = wan2;
#if defined(RTCONFIG_DUALWAN)
	int unit, type;
	int enable_dw_wan = 0;
#endif
	if (!wan || !lan || *lan == '\0' || !wl_ifaces ||
	    !wl_ifaces[WL_2G_BAND] || *wl_ifaces[WL_2G_BAND] == '\0') {
		dbg("%s: invalid parameter. (wan %s, lan %s, "
		    "wl_ifaces %p [%s,%s,%s,%s], ap_lan %s, sw_mode %d)\n",
			__func__, wan? wan : "NULL", lan? lan : "NULL", wl_ifaces,
			(wl_ifaces && wl_ifaces[WL_2G_BAND])? wl_ifaces[WL_2G_BAND] : "NULL",
			(wl_ifaces && wl_ifaces[WL_5G_BAND])? wl_ifaces[WL_5G_BAND] : "NULL",
			(wl_ifaces && wl_ifaces[WL_5G_2_BAND])? wl_ifaces[WL_5G_2_BAND] : "NULL",
			(wl_ifaces && wl_ifaces[WL_60G_BAND])? wl_ifaces[WL_60G_BAND] : "NULL",
			ap_lan? ap_lan : "NULL", sw_mode);
		return -1;
	}
	if (usb && *usb == '\0')
		usb = "usb";
	if (ap_lan && *ap_lan == '\0')
		ap_lan = NULL;

	wl2g  = wl_ifaces[WL_2G_BAND];
	wl5g  = wl_ifaces[WL_5G_BAND];
	wl5g2 = wl_ifaces[WL_5G_2_BAND];
	wl60g = wl_ifaces[WL_60G_BAND];

	nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
	/* wl[01]_vifnames are virtual id only for ui control */
#if defined(RTCONFIG_HAS_5G)
	if (wl5g) {
		sprintf(buf, "%s %s", wl2g, wl5g);
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
	} else {
		sprintf(buf, "%s", wl2g);
		nvram_set("wl1_vifnames", "");
	}
#else
	sprintf(buf, "%s", wl2g);
	nvram_set("wl1_vifnames", "");
#endif
#if defined(RTCONFIG_HAS_5G_2)
	if (wl5g2) {
		sprintf(buf+strlen(buf), " %s", wl5g2);
		nvram_set("wl2_vifnames", "wl2.1 wl2.2 wl2.3");
	} else {
		nvram_set("wl2_vifnames", "");
	}
#else
	nvram_set("wl2_vifnames", "");
#endif
#if defined(RTCONFIG_WIGIG)
	if (wl60g) {
#if !defined(RTCONFIG_HAS_5G_2)
		/* To keep 802.11ad Wigig at wl3 band, if 2-nd 5G is not supported,
		 * we have to pad a fake 5G-2 interface to wl_ifnames.
		 */
		if (wl5g2)
			sprintf(buf+strlen(buf), " %s", wl5g2);
#endif
		sprintf(buf+strlen(buf), " %s", wl60g);
		nvram_set("wl3_vifnames", "wl3.1 wl3.2 wl3.3");
	} else {
		nvram_set("wl3_vifnames", "");
	}
#else
	nvram_set("wl3_vifnames", "");
#endif
	nvram_set("wl_ifnames", buf);

	switch (sw_mode) {
	case SW_MODE_AP:
		if (!ap_lan) {
			sprintf(buf, "%s %s %s", lan, wan, wl2g);
			set_lan_phy(buf);
			if (wl5g)
				add_lan_phy(wl5g);
			if (wl5g2)
				add_lan_phy(wl5g2);
			if (wl60g)
				add_lan_phy(wl60g);
		} else {
			sprintf(buf, "%s %s", ap_lan, wl2g);
			set_lan_phy(buf);
			if (wl5g)
				add_lan_phy(wl5g);
			if (wl5g2)
				add_lan_phy(wl5g2);
			if (wl60g)
				add_lan_phy(wl60g);
		}
		if (wan2)
			add_lan_phy(wan2);
		set_wan_phy("");
		break;

#if defined(RTCONFIG_WIRELESSREPEATER)
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	case SW_MODE_REPEATER:
#if defined(RTCONFIG_PROXYSTA)
		if (nvram_get_int("wlc_psta") == 1)
		{
			/* media bridge mode */
			if (!ap_lan) {
				sprintf(buf, "%s %s %s", lan, wan, wl2g);
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
				if (wl5g2)
					add_lan_phy(wl5g2);
				if (wl60g)
					add_lan_phy(wl60g);
			} else {
				sprintf(buf, "%s %s", ap_lan, wl2g);
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
				if (wl5g2)
					add_lan_phy(wl5g2);
				if (wl60g)
					add_lan_phy(wl60g);
			}
			if (wan2)
				add_lan_phy(wan2);
			set_wan_phy("");
		}
		else
#endif
		{
			/* repeater mode */
			if (!ap_lan) {
				sprintf(buf, "%s %s", lan, wl2g);	/* ignore wan interface */
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
				if (wl5g2)
					add_lan_phy(wl5g2);
				if (wl60g)
					add_lan_phy(wl60g);
			} else {
				sprintf(buf, "%s %s", ap_lan, wl2g);
				set_lan_phy(buf);
				if (wl5g)
					add_lan_phy(wl5g);
				if (wl5g2)
					add_lan_phy(wl5g2);
				if (wl60g)
					add_lan_phy(wl60g);
			}
			set_wan_phy("");
		}
		break;
#elif defined(RTCONFIG_QTN)
		/* FIXME */
#else
		/* Broadcom platform */
		/* FIXME */
#endif	/* RTCONFIG_RALINK || RTCONFIG_QCA */
#endif	/* RTCONFIG_WIRELESSREPEATER */

	default:
		/* router/default */
		set_lan_phy(lan);
#if defined(RTCONFIG_DUALWAN)
		if (!dw_wan || *dw_wan == '\0')
			dw_wan = wan;
		if (!dw_lan || *dw_lan == '\0')
			dw_lan = lan;

#if defined(CONFIG_BCMWL5)
		if (!force_dwlan) {
			if ((get_wans_dualwan() & WANSCAP_WAN) && (get_wans_dualwan() & WANSCAP_LAN)) {
				sprintf(buf, "%s %s", dw_wan, dw_lan);
				nvram_set("wandevs", buf);
			} else {
				nvram_set("wandevs", "et0");	/* FIXME: et0 or eth0 */
			}
		}
#endif

		if (!(get_wans_dualwan() & WANSCAP_2G))
			add_lan_phy(wl2g);
		if (wl5g && !(get_wans_dualwan() & WANSCAP_5G))
			add_lan_phy(wl5g);
		if (wl5g2)
			add_lan_phy(wl5g2);
		if (wl60g)
			add_lan_phy(wl60g);

		set_wan_phy("");
		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			char *wphy = NULL;

			type = get_dualwan_by_unit(unit);
			switch (type) {
			case WANS_DUALWAN_IF_LAN:
#if defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2)
				/* For Realtek switch platforms.
				 * Because our IPTV code always untag frames at CPU port,
				 * unless platform-specific switch configuration code ignore CPU port from untag port mask.
				 * See config_switch() how to use __setup_vlan().
				 */
				if (nvram_get("switch_wantag") && nvram_match("switch_wantag", "movistar")) {
					sprintf(buf, "vlan%s", nvram_safe_get("switch_wan0tagid"));
					wphy = buf;
				}
				else
#endif
				if (force_dwlan || (get_wans_dualwan() & WANSCAP_WAN)
#if defined(RTCONFIG_RALINK)
				    || (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", ""))
#endif
				   )
				{
					wphy = dw_lan;
				} else
					wphy = wan;	/* NOTICE */
				break;
			case WANS_DUALWAN_IF_2G:
				wphy = wl2g;
				break;
			case WANS_DUALWAN_IF_5G:
				if (wl5g)
					wphy = wl5g;
				break;
			case WANS_DUALWAN_IF_WAN:
#if (defined(RTCONFIG_RALINK) && !defined(RTCONFIG_RALINK_MT7620) && !defined(RTCONFIG_RALINK_MT7621))
#else
				/* Broadcom, QCA, MTK (use MT7620/MT7621 ESW) platform */
				if (nvram_get("switch_wantag")
				    && !nvram_match("switch_wantag", "")
				    && !nvram_match("switch_wantag", "hinet")
				    && !nvram_match("switch_wantag", "none")
				    && nvram_get_int("switch_wan0tagid"))
				{
					sprintf(buf, "vlan%d", nvram_get_int("switch_wan0tagid"));
					wphy = buf;
				}
				else
#endif
				if (get_wans_dualwan() & WANSCAP_LAN) {
					wphy = dw_wan;
					enable_dw_wan = 1;
				} else
					wphy = wan;
				break;
			case WANS_DUALWAN_IF_WAN2:
				wphy = wan2;
				wan2_orig = NULL;
				break;
			case WANS_DUALWAN_IF_USB:
				wphy = usb;
				break;
			case WANS_DUALWAN_IF_NONE:
				break;
			default:
				_dprintf("%s: Unknown DUALWAN type %d\n", __func__, type);
			}

			if (wphy)
				add_wan_phy(wphy);
		}

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
		/* If wan2 or wan is not used as WAN, bridge it to LAN. */
		if (wan2_orig && !(get_wans_dualwan() & WANSCAP_WAN2))
			add_lan_phy(wan2_orig);
		if (wan && !(get_wans_dualwan() & WANSCAP_WAN))
			add_lan_phy(wan);
#endif
#else /* !RTCONFIG_DUALWAN */
		add_lan_phy(wl2g);
		if (wl5g)
			add_lan_phy(wl5g);
		if (wl5g2)
			add_lan_phy(wl5g2);
		if (wl60g)
			add_lan_phy(wl60g);
		set_wan_phy(wan);
#endif /* !RTCONFIG_DUALWAN */
	}

#if (defined(RTCONFIG_DUALWAN) && defined(RTCONFIG_QCA))
#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U) || defined(RTCONFIG_SOC_IPQ40XX))
#else /* RT-AC55U || 4G-AC55U */
	if(enable_dw_wan) {
		nvram_set("vlan2hwname", "et0");
	}
	else {
		nvram_unset("vlan2hwname");
	}
#endif
#endif

	_dprintf("%s: WAN %s LAN %s 2G %s 5G %s "
#if defined(RTCONFIG_HAS_5G_2)
		"5G2 %s "
#endif
		"60G %s "
		"USB %s AP_LAN %s DW_WAN %s DW_LAN %s force_dwlan %d, sw_mode %d\n",
		__func__, wan, lan, wl2g, wl5g? wl5g:"N/A",
#if defined(RTCONFIG_HAS_5G_2)
		wl5g2? wl5g2:"N/A",
#endif
		wl60g? wl60g:"N/A", usb, ap_lan? ap_lan:"N/A",
		dw_wan, dw_lan, force_dwlan, sw_mode);
	_dprintf("wan_ifnames: %s\n", nvram_safe_get("wan_ifnames"));
#if defined(CONFIG_BCMWL5)
	_dprintf("wandevs: %s\n", nvram_safe_get("wandevs"));
#endif

	return 0;
}
#endif

#ifdef RTCONFIG_BCM_7114
#define GMAC3_ENVRAM_RESTORE(name)				\
do {								\
	char *var;						\
	if ((var = cfe_nvram_get(name))) {			\
		nvram_set(name, var);				\
	}							\
} while (0)

static void
gmac3_restore_nvram()
{
	switch(get_model()) {
		case MODEL_RTAC87U:
		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
			GMAC3_ENVRAM_RESTORE("et1macaddr");
			GMAC3_ENVRAM_RESTORE("et1phyaddr");
			GMAC3_ENVRAM_RESTORE("et1mdcport");

			nvram_unset("et0macaddr");
			nvram_unset("et0mdcport");
			nvram_unset("et0phyaddr");
			break;
		default: // MODEL_RTAC3100
			GMAC3_ENVRAM_RESTORE("et0macaddr");
			GMAC3_ENVRAM_RESTORE("et0mdcport");
			GMAC3_ENVRAM_RESTORE("et0phyaddr");

			nvram_unset("et1macaddr");
			nvram_unset("et1mdcport");
			nvram_unset("et1phyaddr");
			break;
	}

	nvram_unset("et2macaddr");
	nvram_unset("et2mdcport");
	nvram_unset("et2phyaddr");
	nvram_unset("fwd_wlandevs");
	nvram_unset("fwd_cpumap");
	nvram_unset("fwddevs");
}

#ifdef RTCONFIG_GMAC3
/* Override GAMC3 nvram */
static void
gmac3_override_nvram()
{
	switch(get_model()) {
		case MODEL_RTAC87U:
		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
			nvram_set("et2mdcport", cfe_nvram_get("et1mdcport"));
			nvram_set("et2phyaddr", cfe_nvram_get("et1phyaddr"));
			nvram_set("et2macaddr", cfe_nvram_get("et1macaddr"));

			nvram_set("et1macaddr", "00:00:00:00:00:00");

			nvram_set("et0macaddr", "00:00:00:00:00:00");
			nvram_set("et0mdcport", cfe_nvram_get("et1mdcport"));
			nvram_set("et0phyaddr", cfe_nvram_get("et1phyaddr"));
			break;
		default: // MODEL_RTAC3100
			nvram_set("et2mdcport", cfe_nvram_get("et0mdcport"));
			nvram_set("et2phyaddr", cfe_nvram_get("et0phyaddr"));
			nvram_set("et2macaddr", cfe_nvram_get("et0macaddr"));

			nvram_set("et0macaddr", "00:00:00:00:00:00");

			nvram_set("et1macaddr", "00:00:00:00:00:00");
			nvram_set("et1mdcport", cfe_nvram_get("et0mdcport"));
			nvram_set("et1phyaddr", cfe_nvram_get("et0phyaddr"));
			break;
	}
	nvram_set("lan_hwaddr", nvram_safe_get("et2macaddr"));

	/* set model's fwd specific */
	switch(get_model()) {
		case MODEL_RTAC87U:

			break;
		case MODEL_RTAC5300:
			nvram_set("fwddevs", "fwd0 fwd1");
			nvram_set("fwd_cpumap", "d:l:5:163:0 d:x:2:163:0 d:u:5:169:1");
			nvram_set("fwd_wlandevs", "eth1 eth2 eth3");

			break;
		case MODEL_RTAC88U:
			nvram_set("fwddevs", "fwd1");
			if(is_router_mode()){
				if(*nvram_safe_get("new_map"))
					nvram_set("fwd_cpumap", nvram_safe_get("new_map"));
				else
					nvram_set("fwd_cpumap", "d:l:5:169:1");

				if(*nvram_safe_get("new_fwd_devs"))
					nvram_set("fwd_wlandevs", nvram_safe_get("new_fwd_devs"));
				else
					nvram_set("fwd_wlandevs", "eth2");
			}
			else if(is_psta(nvram_get_int("wlc_band")) || is_psr(nvram_get_int("wlc_band"))) {
				if(nvram_get_int("wlc_band") == 0) {
					/* 2G */
					nvram_set("fwd_cpumap", "d:x:2:163:1");
					nvram_set("fwd_wlandevs", "eth1");
				} else {
					/* 5G */
					nvram_set("fwd_cpumap", "d:l:5:169:1");
					nvram_set("fwd_wlandevs", "eth2");
				}
			} else {
				nvram_set("fwd_cpumap", "d:l:5:169:1");
				nvram_set("fwd_wlandevs", "eth2");
			}
			break;
		case MODEL_RTAC3100:
			nvram_set("fwddevs", "fwd0 fwd1");
			nvram_set("fwd_cpumap", "d:x:2:163:0 d:l:5:169:1");
			nvram_set("fwd_wlandevs", "eth1 eth2");

			break;
		case MODEL_RTAC3200:
			nvram_set("fwddevs", "fwd0 fwd1");
			nvram_set("fwd_cpumap", "d:u:5:163:0 d:x:2:169:1 d:l:5:169:1");
			nvram_set("fwd_wlandevs", "eth1 eth2 eth3");

			break;
		default:
			nvram_set("fwddevs", "fwd0 fwd1");
			nvram_set("fwd_wlandevs", "eth1 eth2 eth3");

			break;
	}

	/* vlan ports/hw are left in init_switch */
}

void chk_gmac3_excludes()
{

	if (nvram_get_int("gmac3_enable") == 0	// disable gmac3
	//|| (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none"))
#if defined(RTAC88U) || defined(RTAC3100)
	//|| is_router_mode()
#endif
	)
		gmac3_restore_nvram();
	else
		gmac3_override_nvram();
}
#endif
#endif

// intialized in this area
//  lan_ifnames
//  wan_ifnames
//  wl_ifnames
//  btn_xxxx_gpio
//  led_xxxx_gpio

int init_nvram(void)
{
	int model = get_model();
	char *wl_ifaces[WL_NR_BANDS];
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_DUALWAN) || defined(RTCONFIG_QCA)
#if !defined(RTCONFIG_LANTIQ)
	int unit = 0;
#endif
#endif
#if defined(RTCONFIG_DUALWAN) && defined(CONFIG_BCMWL5)
	char wan_if[10];
#endif
#if defined(BRTAC828) || defined(RTAC88S) || defined(RTAD7200)
	char *wan0, *wan1, *lan_1, *lan_2, lan_ifs[32];
#endif
#ifdef RTCONFIG_GMAC3
	char *hw_name = "et0";
#endif
#ifdef RTCONFIG_DPSTA
	char word[256], *next;
	char list[128];
	int idx;
	char uif_list[128];	/* for upstream all ifnames */
#endif

#if defined (CONFIG_BCMWL5) && defined(RTCONFIG_TCODE)
	refresh_cfe_nvram();
#endif

	TRACE_PT("init_nvram for model(%d)\n", model);

	/* set default value */
	memset(wl_ifaces, 0, sizeof(wl_ifaces));
	nvram_set("rc_support", "");
	nvram_set_int("btn_rst_gpio", 0xff);
	nvram_set_int("btn_wps_gpio", 0xff);
	nvram_set_int("btn_radio_gpio", 0xff);
	nvram_set_int("led_pwr_gpio", 0xff);
	nvram_set_int("led_wps_gpio", 0xff);
	nvram_set_int("pwr_usb_gpio", 0xff);
	nvram_set_int("pwr_usb_gpio2", 0xff);
	nvram_set_int("led_usb_gpio", 0xff);
	nvram_set_int("led_lan_gpio", 0xff);
	nvram_set_int("led_lan1_gpio", 0xff);
	nvram_set_int("led_lan2_gpio", 0xff);
	nvram_set_int("led_lan3_gpio", 0xff);
	nvram_set_int("led_lan4_gpio", 0xff);
	nvram_set_int("led_wan_gpio", 0xff);
#if defined(RTCONFIG_WANPORT2)
	nvram_unset("led_wan2_gpio");
#endif
#ifdef HND_ROUTER
	nvram_set_int("led_wan_normal_gpio", 0xff);
#endif
	nvram_set_int("led_2g_gpio", 0xff);
	nvram_set_int("led_5g_gpio", 0xff);
	nvram_set_int("led_all_gpio", 0xff);
	nvram_set_int("led_logo_gpio", 0xff);
	nvram_set_int("led_usb3_gpio", 0xff);
	nvram_set_int("fan_gpio", 0xff);
	nvram_set_int("have_fan_gpio", 0xff);
#ifdef RTCONFIG_SWMODE_SWITCH
#if defined(PLAC66U)
	nvram_set_int("btn_swmode1_gpio", 0xff);
#else
	nvram_set_int("btn_swmode1_gpio", 0xff);
	nvram_set_int("btn_swmode2_gpio", 0xff);
	nvram_set_int("btn_swmode3_gpio", 0xff);
	nvram_set_int("swmode_switch", 0);
#endif	/* Model */
#endif	/* RTCONFIG_SWMODE_SWITCH */
#ifdef RTCONFIG_WIRELESS_SWITCH
	nvram_set_int("btn_wifi_gpio", 0xff);
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
	nvram_set_int("btn_wltog_gpio", 0xff);
#endif
#ifdef RTCONFIG_LED_BTN
	nvram_set_int("btn_led_gpio", 0xff);
#endif
#ifdef RTCONFIG_EJUSB_BTN
	nvram_set_int("btn_ejusb1_gpio", 0xff);
	nvram_set_int("btn_ejusb2_gpio", 0xff);
#endif
#if defined(RTCONFIG_WANRED_LED)
	nvram_unset("led_wan_red_gpio");
#if defined(RTCONFIG_WANPORT2)
	nvram_unset("led_wan2_red_gpio");
#endif
#endif
#if defined(RTCONFIG_PWRRED_LED)
	nvram_unset("led_pwr_red_gpio");
#endif
#if defined(RTCONFIG_FAILOVER_LED)
	nvram_unset("led_failover_gpio");
#endif
#if defined(RTCONFIG_M2_SSD)
	nvram_unset("led_sata_gpio");
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	nvram_unset("btn_lte_gpio");
	nvram_unset("led_3g_gpio");
	nvram_unset("led_lte_gpio");
	nvram_unset("led_sig1_gpio");
	nvram_unset("led_sig2_gpio");
	nvram_unset("led_sig3_gpio");
#endif
#ifdef BLUECAVE
	nvram_unset("led_ctl_sig1_gpio");
	nvram_unset("led_ctl_sig2_gpio");
	nvram_unset("led_ctl_sig3_gpio");
	nvram_unset("led_idr_sig1_gpio");
	nvram_unset("led_idr_sig1_gpio");
#endif
	/* In the order of physical placement */
	nvram_set("ehci_ports", "");
	nvram_set("ohci_ports", "");
	nvram_unset("xhci_ports");

#if 0
	conf_swmode_support(model);
#endif

#ifdef RTCONFIG_DSL_TCLINUX
	nvram_set("dsllog_opmode", "");
	nvram_set("dsllog_adsltype", "");
	nvram_set("dsllog_snrmargindown", "0");
	nvram_set("dsllog_snrmarginup", "0");
	nvram_set("dsllog_attendown", "0");
	nvram_set("dsllog_attenup", "0");
	nvram_set("dsllog_wanlistmode", "0");
	nvram_set("dsllog_dataratedown", "0");
	nvram_set("dsllog_datarateup", "0");
	nvram_set("dsllog_attaindown", "0");
	nvram_set("dsllog_attainup", "0");
	nvram_set("dsllog_powerdown", "0");
	nvram_set("dsllog_powerup", "0");
	nvram_set("dsllog_crcdown", "0");
	nvram_set("dsllog_crcup", "0");
	nvram_set("dsllog_farendvendorid", "");//far end vendor
	nvram_set("dsllog_tcm", "");//TCM, Trellis Coded Modulation
	nvram_set("dsllog_pathmodedown", "");//downstream path mode
	nvram_set("dsllog_interleavedepthdown", "");//downstream interleave depth
	nvram_set("dsllog_pathmodeup", "");//upstream path mode
	nvram_set("dsllog_interleavedepthup", "");//upstream interleave depth
	nvram_set("dsllog_vdslcurrentprofile", "");//VDSL current profile
#endif

#ifdef RTCONFIG_PUSH_EMAIL
	nvram_set("fb_state", "");
#endif
	nvram_unset("usb_buildin");

#ifdef RTCONFIG_WIRELESSREPEATER
	if (sw_mode() != SW_MODE_REPEATER)
		nvram_set("ure_disable", "1");
#endif

#ifdef RTCONFIG_DPSTA
	memset(list, 0, sizeof(list));
	memset(uif_list, 0, sizeof(uif_list));

	if (dpsta_mode()) {
		idx = 0;
		foreach (word, nvram_safe_get("wl_ifnames"), next) {
			if ((num_of_wl_if() == 2) || !idx || idx == nvram_get_int("dpsta_band"))
				add_to_list(word, list, sizeof(list));
			add_to_list(word, uif_list, sizeof(uif_list));
			idx++;
		}
	}

	nvram_set("dpsta_ifnames", list);
	nvram_set("dpsta_all_ifnames", uif_list);
#endif

	/* initialize this value to check fw upgrade status */
	nvram_set_int("upgrade_fw_status", FW_INIT);

#ifdef RTCONFIG_DEFAULT_AP_MODE
	if (sw_mode() == SW_MODE_AP && nvram_match("x_Setting", "0")) {
		nvram_set("lan_ipaddr", nvram_safe_get("lan_ipaddr_rt"));
		nvram_set("lan_netmask", nvram_safe_get("lan_netmask_rt"));
	}
#endif

	nvram_unset("wanduck_start_detect");

	switch (model) {
#ifdef RTCONFIG_RALINK
	case MODEL_EAN66:
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "eth2 ra0 rai0");
		nvram_set("wan_ifnames", "");
		nvram_set("wl_ifnames","ra0 rai0");
		nvram_set("wl0_vifnames", "");
		nvram_set("wl1_vifnames", "");
		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		//if (!nvram_get("ct_max"))
		//	nvram_set("ct_max", "30000");
		if (is_router_mode())
			nvram_set_int("sw_mode", SW_MODE_REPEATER);
		add_rc_support("mssid 2.4G 5G update");
		break;

#ifdef RTN65U
	case MODEL_RTN65U:
		nvram_set("boardflags", "0x100");	// although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");	// vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");	// vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "rai0";
		wl_ifaces[WL_5G_BAND] = "ra0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_LED_ALL
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		nvram_set("wl0_HT_TxStream", "2");	// for 300 Mbps RT3352 2T2R 2.4G
		nvram_set("wl0_HT_RxStream", "2");	// for 300 Mbps RT3352 2T2R 2.4G
		nvram_set("wl1_HT_TxStream", "3");	// for 450 Mbps RT3353 3T3R 2.4G/5G
		nvram_set("wl1_HT_RxStream", "3");	// for 450 Mbps RT3883 3T3R 2.4G/5G
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
#endif	/* RTN65U */

#if defined(RTN67U)
	case MODEL_RTN67U:
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		set_basic_ifname_vars("eth3", "eth2", wl_ifaces, "usb", NULL, NULL, NULL, NULL, 0);

		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb2_gpio", 28|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_LED_ALL
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
#endif
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		nvram_set("wl0_HT_TxStream", "3");	// for 450 Mbps RT3353 3T3R 2.4G
		nvram_set("wl0_HT_RxStream", "3");	// for 450 Mbps RT3883 3T3R 2.4G
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
#endif	/* RTN67U */

#if defined(RTN36U3)
	case MODEL_RTN36U3:
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		set_basic_ifname_vars("eth3", "eth2", wl_ifaces, "usb", NULL, NULL, NULL, NULL, 0);

		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		break;
#endif	/* RTN36U3 */

#if defined(RTN56U)
	case MODEL_RTN56U:
		nvram_set("lan_ifname", "br0");

		if (sw_mode()==SW_MODE_AP) {
			nvram_set("lan_ifnames", "eth2 eth3 rai0 ra0");
			nvram_set("wan_ifnames", "");
		}
		else {  // router/default
			nvram_set("lan_ifnames", "eth2 rai0 ra0");
			nvram_set("wan_ifnames", "eth3");
		}
		nvram_set("wl_ifnames","rai0 ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

#ifdef RTCONFIG_N56U_SR2
		nvram_set_int("btn_rst_gpio", 25|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("btn_rst_gpio", 13|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 24|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_N56U_SR2
		nvram_set_int("led_lan_gpio", 31|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("led_lan_gpio", 19|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("led_wan_gpio", 27|GPIO_ACTIVE_LOW);
#ifndef RTCONFIG_N56U_SR2
		eval("rtkswitch", "11");		// for SR3 LAN LED
#endif
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("no_phddns");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif	/* RTN56U */

#if defined(RTN14U)
	case MODEL_RTN14U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 41|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 40|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
//		if (!nvram_get("ct_max"))
		nvram_set("ct_max", "300000"); // force

		if (nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl"); //for hwnat only
		add_rc_support("manual_stb");
		add_rc_support("meoVoda");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		break;
#endif

#if defined(RTN11P) || defined(RTN300)
	case MODEL_RTN11P:
	case MODEL_RTN300:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, NULL, "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 44|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 39|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 72|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);

		nvram_set("ct_max", "300000"); // force

		if (nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update");
		add_rc_support("rawifi");
		add_rc_support("switchctrl"); //for hwnat only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		break;
#endif

#if defined(RTAC52U)
	case MODEL_RTAC52U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wltog_gpio",13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio",  8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif

#if defined(RTAC51U)
	case MODEL_RTAC51U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif	/* RTAC51U */

#if defined(RTN56UB1) || defined(RTN56UB2)
	case MODEL_RTN56UB1:
	case MODEL_RTN56UB2:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface

		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("eth3", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 14|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 7|GPIO_ACTIVE_LOW);
		eval("rtkswitch", "11");

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ra0");
		config_netdev_bled("led_5g_gpio", "rai0");
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		//add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");

		break;

#endif	/* RTN56UB1 RTN56UB2 */

#if defined(RTN54U) || defined(RTAC54U)
	case MODEL_RTN54U:
	case MODEL_RTAC54U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
#if defined(RTAC54U)
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
#endif
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTN54U or RTAC54U*/

#if defined(RTAC1200HP)
	case MODEL_RTAC1200HP:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  62|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  61|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 67|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  65|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  65|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 70|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 72|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 69|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 68|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 66|GPIO_ACTIVE_LOW);
#endif
		eval("rtkswitch", "11");

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ra0");
		config_netdev_bled("led_5g_gpio", "rai0");
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC1200HP */

#if defined(RTAC1200)
	case MODEL_RTAC1200:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  5|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 6);
		nvram_set_int("led_pwr_gpio",  37|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  37|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 4|GPIO_ACTIVE_LOW);

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		//add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC1200 */

#if defined(RTAC1200GU)
	case MODEL_RTAC1200GU:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("eth3", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  41|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 47|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 48|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  48|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 46|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 16|GPIO_ACTIVE_LOW);
		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-2");
		nvram_set("ohci_ports", "2-2");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC1200GU */

#if defined(RTAC1200GA1)
	case MODEL_RTAC1200GA1:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("eth3", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  41|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 47|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 48|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  48|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 46|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 16|GPIO_ACTIVE_LOW);
		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-2");
		nvram_set("ohci_ports", "2-2");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC1200GA1 */

#if defined(RTAC51UP)
	case MODEL_RTAC51UP:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		eval("rtkswitch", "11");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "15000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif	/* RTAC51UP */

#if defined(RTAC53)
	case MODEL_RTAC53:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "NULL", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_usbbus_bled("led_usb_gpio", "1 2");

		eval("rtkswitch", "11");

		nvram_set("ct_max", "15000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif	/* RTAC53 */

#if defined(RPAC87)
	case MODEL_RPAC87:
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("eth2", "eth2", wl_ifaces, "NULL", "eth2", NULL, "NULL", NULL, 0);

		nvram_set_int("btn_rst_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 4|GPIO_ACTIVE_LOW);

		nvram_set_int("led_2g_green_gpio1", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio2", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio3", 41|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio4", 44|GPIO_ACTIVE_LOW);

		nvram_set_int("led_5g_green_gpio1", 45|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_green_gpio2", 46|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_green_gpio3", 47|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_green_gpio4", 48|GPIO_ACTIVE_LOW);

		add_rc_support("2.4G 5G update");
		add_rc_support("rawifi");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		nvram_set("wl_time", "2");
		break;
#endif	/* RP-AC87 */

#if defined(RTN800HP)
	case MODEL_RTN800HP:

		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";

		set_basic_ifname_vars("eth3", "vlan1", wl_ifaces, NULL, "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  16|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio",  13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio",   14|GPIO_ACTIVE_LOW);

		eval("rtkswitch", "11");

		config_swports_bled("led_wan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ra0");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");

		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");

		//nvram_set("wl_time", "15");
		break;
#endif	/* RT-N800HP */

#if defined(RTAC85U)
	case MODEL_RTAC85U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		wl_ifaces[WL_5G_BAND] = "rai0";
		set_basic_ifname_vars("eth3", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio",  16|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio",  18|GPIO_ACTIVE_LOW);
//		nvram_set_int("btn_wltog_gpio",13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio",  13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio",  6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio",  12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 4|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_all_gpio", 10|GPIO_ACTIVE_LOW);

		eval("rtkswitch", "11");

		/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ra0");
		config_netdev_bled("led_5g_gpio", "rai0");
		config_usbbus_bled("led_usb_gpio", "1 2");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("rawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		//add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
#if defined(RTAC65U)
		nvram_set("wl0_HT_TxStream", "3");
		nvram_set("wl0_HT_RxStream", "3");
		nvram_set("wl1_HT_TxStream", "3");
		nvram_set("wl1_HT_RxStream", "3");
		add_rc_support("noaidisk nodm noftp");
#endif
		break;
#endif /*  RTAC85U  */

#if defined(RTN11P_B1)
	case MODEL_RTN11P_B1:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ra0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, NULL, "vlan1", NULL, "vlan3", NULL, 0);
		nvram_set_int("btn_rst_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 44|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 37|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 44|GPIO_ACTIVE_LOW);

	/* enable bled */
		config_swports_bled("led_wan_gpio", 0);
		config_swports_bled("led_lan_gpio", 0);
		//eval("rtkswitch", "11");

		nvram_set("ct_max", "1024"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update");
		add_rc_support("rawifi");
		//add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		//add_rc_support("11AC");
		//either txpower or singlesku supports rc.
		add_rc_support("pwrctrl");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		//nvram_set("wl1_HT_TxStream", "2");
		//nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTN11P_B1 */

#ifdef RTCONFIG_DSL
	case MODEL_DSLN55U:
		nvram_set("lan_ifname", "br0");

		if (sw_mode()==SW_MODE_AP) {
			// switch need to well-set for this config
			nvram_set("lan_ifnames", "eth2.2 rai0 ra0");
			nvram_set("wan_ifnames", "");
		}
		else { // router/default
			nvram_set("lan_ifnames", "eth2.2 rai0 ra0");
			nvram_set("wan_ifnames", "eth2.1.1");
		}

#ifdef RTCONFIG_DUALWAN
		// Support dsl, lan, dsl/usb, lan/usb, dsl/lan, usb/lan only, so far
		if (get_dualwan_secondary()==WANS_DUALWAN_IF_NONE) {
			// dsl, lan
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1");
			}
			else if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN) {
				nvram_set("wan_ifnames", "eth2.4");
			}
		}
		else if (get_dualwan_secondary()==WANS_DUALWAN_IF_LAN) {
			//dsl/lan
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1 eth2.4");
			}
			/* Paul add 2013/1/24 */
			//usb/lan
			else {
				nvram_set("wan_ifnames", "usb eth2.4");
			}
		}
		else {
			//dsl/usb, lan/usb
			if (get_dualwan_primary()==WANS_DUALWAN_IF_DSL) {
				nvram_set("wan_ifnames", "eth2.1.1 usb");
			}
			else if (get_dualwan_primary()==WANS_DUALWAN_IF_LAN) {
				nvram_set("wan_ifnames", "eth2.4 usb");
			}
		}
#endif

		nvram_set("wl_ifnames","rai0 ra0");
		// it is virtual id only for ui control
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 26|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 25|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIRELESS_SWITCH
		nvram_set_int("btn_wifi_gpio", 1|GPIO_ACTIVE_LOW);
#endif

		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "2-1 2-2");

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
		add_rc_support("mssid");
		add_rc_support("2.4G 5G usbX2");
		add_rc_support("rawifi");
		add_rc_support("dsl");
#if defined(RTCONFIG_WIRELESS_SWITCH) || defined(RTCONFIG_WIFI_TOG_BTN)
		add_rc_support("wifi_hw_sw");
#endif
		add_rc_support("pwrctrl");
		add_rc_support("no_phddns");

		//Ren: set "spectrum_supported" in /router/dsl_drv_tool/tp_init/tp_init_main.c
		if (nvram_match("spectrum_supported", "1"))
		{
			add_rc_support("spectrum"); //Ren
		}

		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif // RTCONFIG_DSL

#if defined(RTN13U)
	case MODEL_RTN13U:
		nvram_set("boardflags", "0x310"); // although it is not used in ralink driver
		nvram_set("lan_ifname", "br0");
		nvram_set("lan_ifnames", "vlan0 ra0");
		nvram_set("wan_ifnames", "vlan1");
		nvram_set("wl_ifnames","ra0");
		nvram_set("wan_ports", "4");
		nvram_set("vlan0hwname", "et0"); // although it is not used in ralink driver
		nvram_set("vlan1hwname", "et0");

		nvram_set_int("btn_rst_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7|GPIO_ACTIVE_LOW);
		break;
#endif	/* RTN13U */
#endif // RTCONFIG_RALINK

#if defined(MAPAC1300)
	case MODEL_MAPAC1300:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
#ifdef RTCONFIG_DETWAN
		if(IPTV_ports_cnt() > 0) {
			const char *wan = "eth1";
			const char *wan_mask = "16";
			const char *lan_mask = "32";
			nvram_set("detwan_ifname", wan);
			nvram_set("detwan_phy", wan);
			nvram_set("wan0_ifname", wan);
			nvram_set("wan_ifnames", wan);
			nvram_set("wan0_gw_ifname", wan);
			nvram_set("wanports_mask", wan_mask);
			nvram_set("lanports_mask", lan_mask);
			nvram_set("detwan_wan_mask", wan_mask);
			nvram_set("detwan_lan_mask", lan_mask);
		}

		if(nvram_safe_get("wan0_ifname")[0] == '\0' || sw_mode() != SW_MODE_ROUTER)
		{
#ifdef RTCONFIG_CFGSYNC
			nvram_unset("wired_ifnames");
#endif
			if(sw_mode() == SW_MODE_ROUTER || nvram_match("cfg_master", "1"))
				set_basic_ifname_vars("", "eth0 eth1", wl_ifaces, "usb", "eth0 eth1", "vlan2", "vlan3", NULL, 0);
			else {
#ifdef RTCONFIG_DUAL_BACKHAUL
				set_basic_ifname_vars("", "eth0 eth1 sta0 sta1", wl_ifaces, "usb", "eth0 eth1 sta0 sta1", "vlan2", "vlan3", NULL, 0);
#else
				set_basic_ifname_vars("", "eth0 eth1 sta1", wl_ifaces, "usb", "eth0 eth1 sta1", "vlan2", "vlan3", NULL, 0);
#endif
#ifdef RTCONFIG_CFGSYNC
				nvram_set("wired_ifnames", "eth0 eth1");
#endif
			}
			nvram_set("detwan_proto", "-1");
			nvram_set("wanports_mask", "0");
			nvram_set("lanports_mask", "48");
		}
		else if(sw_mode() == SW_MODE_ROUTER)
		{
			char wantmp[10], lantmp[128];
			strcpy(wantmp, nvram_safe_get("wan0_ifname"));
			strcpy(lantmp, "eth0 eth1");
			if (string_remove(lantmp, wantmp)) {
				if(lantmp[strlen(lantmp)-1]==' ')
					lantmp[strlen(lantmp)-1] = '\0';
				set_basic_ifname_vars(wantmp, lantmp, wl_ifaces, "usb", "eth0 eth1", "vlan2", "vlan3", NULL, 0);
			}
		}

		nvram_set("detwan_max", "2");
		nvram_set("detwan_name_0", "eth0");
		nvram_set("detwan_mask_0", "32");
		nvram_set("detwan_name_1", "eth1");
		nvram_set("detwan_mask_1", "16");
#else
#ifdef RTCONFIG_DUAL_BACKHAUL
		set_basic_ifname_vars("eth0", "eth1 sta0 sta1", wl_ifaces, "usb", "eth1 sta0 sta1", "vlan2", "vlan3", NULL, 0);
#else
		set_basic_ifname_vars("eth0", "eth1 sta1", wl_ifaces, "usb", "eth1 sta1", "vlan2", "vlan3", NULL, 0);
#endif
#endif	/* RTCONFIG_DETWAN */
#ifdef RTCONFIG_LP5523
		nvram_set_int("prelink_pap_status", -1);
		nvram_set("lp55xx_lp5523_col", "");
		nvram_set("lp55xx_lp5523_beh", "");
		if (nvram_match("lp55xx_lp5523_sch_enable", "2"))
			nvram_set_int("lp55xx_lp5523_sch_enable", 1);
#endif
		nvram_set_int("btn_rst_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 63|GPIO_ACTIVE_LOW);

		if(nvram_match("wl1_country_code", "GB"))
			nvram_set("dfs_check_period","1");
		else
			nvram_set("dfs_check_period","0");

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-1");
		nvram_set("ehci_ports", "1-1 3-1");
		nvram_set("ohci_ports", "1-1 4-1");
#else
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
#endif

		nvram_set("ct_max", "300000"); // force

		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("11AC");
		add_rc_support("noaidisk");
		add_rc_support("noitunes");
		add_rc_support("nodm");
		add_rc_support("manual_stb");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");

		nvram_set("wl0_vifnames", "wl0.1 wl0.2");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif //MAPAC1300

#if defined(MAPAC2200)
	case MODEL_MAPAC2200:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		wl_ifaces[WL_5G_2_BAND] = "ath2";
#ifdef RTCONFIG_DETWAN
		if(IPTV_ports_cnt() > 0) {
			const char *wan = "eth0";
			const char *wan_mask = "32";
			const char *lan_mask = "16";
			nvram_set("detwan_ifname", wan);
			nvram_set("detwan_phy", wan);
			nvram_set("wan0_ifname", wan);
			nvram_set("wan_ifnames", wan);
			nvram_set("wan0_gw_ifname", wan);
			nvram_set("wanports_mask", wan_mask);
			nvram_set("lanports_mask", lan_mask);
			nvram_set("detwan_wan_mask", wan_mask);
			nvram_set("detwan_lan_mask", lan_mask);
		}

		if(nvram_safe_get("wan0_ifname")[0] == '\0' || sw_mode() != SW_MODE_ROUTER)
		{
#ifdef RTCONFIG_CFGSYNC
			nvram_unset("wired_ifnames");
#endif
			if(sw_mode() == SW_MODE_ROUTER || nvram_match("cfg_master", "1"))
				set_basic_ifname_vars("", "eth0 eth1", wl_ifaces, "usb", "eth0 eth1", "vlan2", "vlan3", NULL, 0);
			else {
#ifdef RTCONFIG_DUAL_BACKHAUL
				set_basic_ifname_vars("", "eth0 eth1 sta0 sta1", wl_ifaces, "usb", "eth0 eth1 sta0 sta1", "vlan2", "vlan3", NULL, 0);
#else
				set_basic_ifname_vars("", "eth0 eth1 sta1", wl_ifaces, "usb", "eth0 eth1 sta1", "vlan2", "vlan3", NULL, 0);
#endif
#ifdef RTCONFIG_CFGSYNC
				nvram_set("wired_ifnames", "eth0 eth1");
#endif
			}
			nvram_set("detwan_proto", "-1");
			nvram_set("wanports_mask", "0");
			nvram_set("lanports_mask", "48");
		}
		else if(sw_mode() == SW_MODE_ROUTER)
		{
			char wantmp[10], lantmp[128];
			strcpy(wantmp, nvram_safe_get("wan0_ifname"));
			strcpy(lantmp, "eth0 eth1");
			if (string_remove(lantmp, wantmp)) {
				if(lantmp[strlen(lantmp)-1]==' ')
					lantmp[strlen(lantmp)-1] = '\0';
				set_basic_ifname_vars(wantmp, lantmp, wl_ifaces, "usb", "eth0 eth1", "vlan2", "vlan3", NULL, 0);
			}
		}

		nvram_set("detwan_max", "2");
		nvram_set("detwan_name_0", "eth0");
		nvram_set("detwan_mask_0", "32");
		nvram_set("detwan_name_1", "eth1");
		nvram_set("detwan_mask_1", "16");
#else
#ifdef RTCONFIG_DUAL_BACKHAUL
		set_basic_ifname_vars("eth0", "eth1 sta0 sta1", wl_ifaces, "usb", "eth1 sta0 sta1", "vlan2", "vlan3", NULL, 0);
#else
		set_basic_ifname_vars("eth0", "eth1  sta1", wl_ifaces, "usb", "eth1 sta1", "vlan2", "vlan3", NULL, 0);
#endif
#endif	/* RTCONFIG_DETWAN */
#ifdef RTCONFIG_LP5523
		nvram_set_int("prelink_pap_status", -1);
		nvram_set("lp55xx_lp5523_col", "");
		nvram_set("lp55xx_lp5523_beh", "");
#endif
		/* set DFS check time */
		if(nvram_match("wl1_country_code", "GB"))
			nvram_set("dfs_check_period","1");
		else
			nvram_set("dfs_check_period","0");

		/* 2.4G tx power switch */
		gpio_dir(39, GPIO_DIR_OUT);
		if(nvram_match("wl0_country_code", "GB") || nvram_match("wl0_country_code", "CN"))
			set_gpio(39, 1);	// 3.3v: echo 1 > /sys/class/gpio/gpio39/value
		else
			set_gpio(39, 0);	// 5v  : echo 0 > /sys/class/gpio/gpio39/value

		/* set DPDT */
		gpio_dir(44, GPIO_DIR_OUT);
		set_gpio(44, 1);
		gpio_dir(45, GPIO_DIR_OUT);
		set_gpio(45, 0);
		gpio_dir(46, GPIO_DIR_OUT);
		set_gpio(46, 1);
		gpio_dir(47, GPIO_DIR_OUT);
		set_gpio(47, 0);

		nvram_set_int("btn_rst_gpio", 34|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 18|GPIO_ACTIVE_LOW);

		nvram_set("ct_max", "300000"); // force

		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("11AC");
		add_rc_support("noaidisk");
		add_rc_support("noitunes");
		add_rc_support("nodm");
		add_rc_support("manual_stb");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2");
		break;
#endif //MAPAC2200

#ifdef VZWAC1300
	case MODEL_VZWAC1300:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
#ifdef RTCONFIG_CFGSYNC
		nvram_unset("wired_ifnames");
#endif
		if(sw_mode() == SW_MODE_ROUTER || nvram_match("cfg_master", "1"))
			set_basic_ifname_vars("eth0", "eth1", wl_ifaces, "usb", "eth0 eth1", "vlan2", "vlan3", NULL, 0);
		else {
#ifdef RTCONFIG_DUAL_BACKHAUL
				set_basic_ifname_vars("", "eth0 eth1 sta0 sta1", wl_ifaces, "usb", "eth0 eth1 sta0 sta1", "vlan2", "vlan3", NULL, 0);
#else
				set_basic_ifname_vars("", "eth0 eth1 sta1", wl_ifaces, "usb", "eth0 eth1 sta1", "vlan2", "vlan3", NULL, 0);
#endif
#ifdef RTCONFIG_CFGSYNC
			nvram_set("wired_ifnames", "eth0 eth1");
#endif
		}

#ifdef RTCONFIG_LP5523
		nvram_set_int("prelink_pap_status", -1);
		nvram_set("lp55xx_lp5523_col", "");
		nvram_set("lp55xx_lp5523_beh", "");
		if (nvram_match("lp55xx_lp5523_sch_enable", "2"))
			nvram_set_int("lp55xx_lp5523_sch_enable", 1);
#endif
		nvram_set_int("btn_rst_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 63|GPIO_ACTIVE_LOW);

		if(nvram_match("wl1_country_code", "GB"))
			nvram_set("dfs_check_period","1");
		else
			nvram_set("dfs_check_period","0");

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-1");
		nvram_set("ehci_ports", "1-1 3-1");
		nvram_set("ohci_ports", "1-1 4-1");
#else
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
#endif

		nvram_set("ct_max", "300000"); // force

		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("11AC");
		add_rc_support("noaidisk");
		add_rc_support("noitunes");
		add_rc_support("nodm");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");

		nvram_set("wl0_vifnames", "wl0.1 wl0.2");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		set_default_wifi_rate();
		break;
#endif

#if defined(MAPAC1750)
	case MODEL_MAPAC1750:	/* fall through */
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
#ifdef RTCONFIG_DETWAN
		if(nvram_safe_get("wan0_ifname")[0] == '\0' || sw_mode() != SW_MODE_ROUTER)
		{
#ifdef RTCONFIG_CFGSYNC
			nvram_unset("wired_ifnames");
#endif
			if(sw_mode() == SW_MODE_ROUTER || nvram_get("cfg_master") != NULL)
				set_basic_ifname_vars("", "vlan1 vlan2", wl_ifaces, "usb", "vlan1 vlan2", NULL, "vlan3", NULL, 0);
			else {
#ifdef RTCONFIG_DUAL_BACKHAUL
				set_basic_ifname_vars("", "vlan1 vlan2 sta0 sta1", wl_ifaces, "usb", "vlan1 vlan2 sta0 sta1", NULL, "vlan3", NULL, 0);
#else
				set_basic_ifname_vars("", "vlan1 vlan2 sta1", wl_ifaces, "usb", "vlan1 vlan2 sta1", NULL, "vlan3", NULL, 0);
#endif
#ifdef RTCONFIG_CFGSYNC
				nvram_set("wired_ifnames", "vlan1 vlan2");
#endif
			}
			nvram_set("detwan_proto", "-1");
			nvram_set("wanports_mask", "0");
			nvram_set("lanports_mask", "12");
		}
		else if(sw_mode() == SW_MODE_ROUTER)
		{
			char wantmp[10], lantmp[128];
			strcpy(wantmp, nvram_safe_get("wan0_ifname"));
			strcpy(lantmp, "vlan1 vlan2");
			if (string_remove(lantmp, wantmp)) {
				if(lantmp[strlen(lantmp)-1]==' ')
					lantmp[strlen(lantmp)-1] = '\0';
				set_basic_ifname_vars(wantmp, lantmp, wl_ifaces, "usb", "vlan1 vlan2", NULL, "vlan3", NULL, 0);
			}
		}

		nvram_set("detwan_max", "2");
		nvram_set("detwan_name_0", "vlan2");
		nvram_set("detwan_mask_0", "4");
		nvram_set("detwan_name_1", "vlan1");
		nvram_set("detwan_mask_1", "8");
#else
#ifdef RTCONFIG_DUAL_BACKHAUL
		set_basic_ifname_vars("vlan2", "vlan1 sta0 sta1", wl_ifaces, "usb", "vlan1 vlan2 sta0 sta1", NULL, "vlan3", NULL, 0);
#else
		set_basic_ifname_vars("vlan2", "vlan1 sta1", wl_ifaces, "usb", "vlan1 vlan2 sta1", NULL, "vlan3", NULL, 0);
#endif
#endif	/* RTCONFIG_DETWAN */
		nvram_set_int("prelink_pap_status", -1);

		nvram_set_int("btn_rst_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_blue_gpio", 14);
		nvram_set_int("led_green_gpio", 15);
		nvram_set_int("led_red_gpio", 16);

		/* enable Bluetooth functionality */
		gpio_dir(7, GPIO_DIR_OUT);
		set_gpio(7, 1);

		/* enable bled */
		config_netdev_bled("led_blue_gpio", "ath0");
		add_gpio_to_bled("led_blue_gpio", "led_green_gpio");
		add_gpio_to_bled("led_blue_gpio", "led_red_gpio");
		set_rgbled(RGBLED_BLUE_3ON3OFF);

		/* Etron xhci host:
		 *	USB2 bus: 1-1
		 *	USB3 bus: 2-1
		 * Internal ehci host:
		 * 	USB2 bus: 3-1
		 */
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("11AC");
		add_rc_support("manual_stb");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "3");
		nvram_set("wl0_HT_RxStream", "3");
		nvram_set("wl1_HT_TxStream", "3");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif	/* MAPAC1750 */

#if defined(MAPAC3000)
	case MODEL_MAPAC3000:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		wl_ifaces[WL_5G_2_BAND] = "ath2";
#ifdef RTCONFIG_DUAL_BACKHAUL
		set_basic_ifname_vars("eth0", "eth1 sta0 sta1", wl_ifaces, "usb", "eth1 sta0 sta1", "vlan2", "vlan3", NULL, 0);
#else
		set_basic_ifname_vars("eth0", "eth1 sta1", wl_ifaces, "usb", "eth1 sta1", "vlan2", "vlan3", NULL, 0);
#endif
		/* set DFS check time */
		if(nvram_match("wl1_country_code", "GB"))
			nvram_set("dfs_check_period","1");
		else
			nvram_set("dfs_check_period","0");

		/* temp for other LED pin*/
		gpio_dir(52, GPIO_DIR_OUT);
		set_gpio(52, 0);
		gpio_dir(61, GPIO_DIR_OUT);
		set_gpio(61, 0);
		gpio_dir(54, GPIO_DIR_OUT);
		set_gpio(54, 1);
		gpio_dir(45, GPIO_DIR_OUT);
		set_gpio(45, 1);

		nvram_set_int("btn_rst_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 68);

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-1");
		nvram_set("ehci_ports", "1-1 3-1");
		nvram_set("ohci_ports", "1-1 4-1");
#else
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
#endif
		nvram_set("ct_max", "300000"); // force

		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		add_rc_support("noitunes");
		add_rc_support("nodm");

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		nvram_set("wl2_HT_TxStream", "4");
		nvram_set("wl2_HT_RxStream", "4");
		break;
#endif //MAPAC3000

#if defined(RTAC55U) || defined(RTAC55UHP)
	case MODEL_RTAC55U:	/* fall through */
	case MODEL_RTAC55UHP:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth0", "eth1", wl_ifaces, "usb", "eth1", "vlan2", "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio",  4);
		nvram_set_int("led_lan_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 15);
		nvram_set_int("led_2g_gpio", 13);
#if defined(RTCONFIG_ETRON_XHCI_USB3_LED)
		/* SR1 */
		nvram_set("led_usb3_gpio", "etron");
#else
		nvram_set_int("led_5g_gpio", 0);
		nvram_set_int("led_usb3_gpio", 1);
#endif
		nvram_set_int("led_pwr_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_red_gpio", 14);
#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 21);
#endif

		/* enable bled */
		config_netdev_bled("led_wan_gpio", "eth0");
		config_swports_bled_sleep("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");

#if defined(RTCONFIG_ETRON_XHCI_USB3_LED)
		/* SR1 */
#else
		config_netdev_bled("led_5g_gpio", "ath1");
#endif

		config_usbbus_bled("led_usb_gpio", "3 4");
		config_usbbus_bled("led_usb3_gpio", "1 2");

		/* Etron xhci host:
		 *	USB2 bus: 1-1
		 *	USB3 bus: 2-1
		 * Internal ehci host:
		 * 	USB2 bus: 3-1
		 */
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		break;
#endif	/* RTAC55U | RTAC55UHP */

#ifdef RT4GAC55U
	case MODEL_RT4GAC55U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth0", "eth1", wl_ifaces, "usb", "eth1", "vlan2", "vlan3", NULL, 0);

		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_lte_gpio", 21|GPIO_ACTIVE_LOW);
#ifdef RTAC55U_SR1
		nvram_set_int("led_usb_gpio",  4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio" , 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio" , 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lte_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig1_gpio",18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig2_gpio",23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig3_gpio",22|GPIO_ACTIVE_LOW);
#else	/* RTAC55U_SR2 */
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lte_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio" ,  4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio" , 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig1_gpio",19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig2_gpio",20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig3_gpio",15|GPIO_ACTIVE_LOW);
#endif	/* RTAC55U_SR */

#ifdef RTCONFIG_WIRELESS_SWITCH
		nvram_set_int("btn_wifi_gpio", 1|GPIO_ACTIVE_LOW);
		add_rc_support("wifi_hw_sw");
#endif	/* RTCONFIG_WIRELESS_SWITCH */

		/* enable bled */
		config_netdev_bled("led_wan_gpio", "eth0");
		config_swports_bled_sleep("led_lan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		config_usbbus_bled("led_usb_gpio", "1 2");
		nvram_set("ehci_ports", "1-1 2-1");
		nvram_set("usb_buildin", "2");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		break;
#endif	/* RT4GAC55U */

#if defined(PLN12)
	case MODEL_PLN12:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, NULL, "vlan1", NULL, "vlan3", NULL, 0);

		/* Enable 1st 2G guest network only. */
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 15);
		nvram_set_int("btn_wps_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_red_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_orange_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_red_gpio", 17|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled_sleep("led_lan_gpio", 0);
		//config_netdev_bled("led_2g_green_gpio", "ath0");
		//config_netdev_bled("led_2g_orange_gpio", "ath0");
		config_netdev_bled("led_2g_red_gpio", "ath0");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G update");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("swmode_switch");
#ifdef RTCONFIG_DHCP_OVERRIDE
		add_rc_support("dhcp_override");

		if (nvram_match("dhcp_enable_x", "1") || nvram_match("x_Setting", "0"))
			nvram_set("dnsqmode", "2");
		else
			nvram_set("dnsqmode", "1");
#endif
		add_rc_support("plc");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		/* avoid inserting unnecessary kernel modules */
		nvram_set("nf_ftp", "0");
		nvram_set("nf_pptp", "0");
		break;
#endif	/* PLN12 */

#if defined(PLAC56)
	case MODEL_PLAC56:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, NULL, "vlan1", NULL, "vlan3", NULL, 0);
		nvram_set("wl0_vifnames", "wl0.1 wl0.2");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2");

		nvram_set_int("btn_rst_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("plc_wake_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_red_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio", 19|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_red_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_green_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_red_gpio", 7|GPIO_ACTIVE_LOW);

		/* enable bled */
		config_swports_bled_sleep("led_lan_gpio", 0);
		config_netdev_bled("led_2g_green_gpio", "ath0");
		//config_netdev_bled("led_2g_red_gpio", "ath0");
		config_netdev_bled("led_5g_green_gpio", "ath1");
		//config_netdev_bled("led_5g_red_gpio", "ath1");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("swmode_switch");
		add_rc_support("11AC");
#ifdef RTCONFIG_DHCP_OVERRIDE
		add_rc_support("dhcp_override");

		if (nvram_match("dhcp_enable_x", "1") || nvram_match("x_Setting", "0"))
			nvram_set("dnsqmode", "2");
		else
			nvram_set("dnsqmode", "1");
#endif
		add_rc_support("plc");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");
		/* avoid inserting unnecessary kernel modules */
		nvram_set("nf_ftp", "0");
		nvram_set("nf_pptp", "0");
		break;
#endif	/* PLAC56 */

#if defined(PLAC66U)
	case MODEL_PLAC66U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		nvram_set("vlan1hwname", "et1");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("vlan2hwname", "et1");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("vlan2", "vlan1", wl_ifaces, "usb", "vlan1", NULL, "vlan3", NULL, 0);

		nvram_set_int("btn_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 7|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_SWMODE_SWITCH
		nvram_set_int("btn_swmode1_gpio", 19|GPIO_ACTIVE_LOW);
		add_rc_support("swmode_switch");
#endif

		/* enable bled */
		config_netdev_bled("led_lan_gpio", "eth0");
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "3");
		nvram_set("wl0_HT_RxStream", "3");
		nvram_set("wl1_HT_TxStream", "3");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif	/* PLAC66U */

#if defined(RPAC51)
	case MODEL_RPAC51:
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth0", "eth1", wl_ifaces, NULL, "eth1", NULL, NULL, NULL, 0);
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "wl1.1");

		nvram_set_int("btn_rst_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_red_gpio", 15|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_lan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_single_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_far_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_near_gpio", 16|GPIO_ACTIVE_LOW);

		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "1");
		nvram_set("wl1_HT_RxStream", "1");
		break;
#endif	/* RP-AC51 */

#if defined(RPAC66)
	case MODEL_RPAC66:
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth0", "eth0", wl_ifaces, NULL, "eth0", NULL, NULL, NULL, 0);
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "wl1.1");

		nvram_set_int("btn_rst_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6);
		nvram_set_int("led_pwr_orange_gpio", 1);
		//nvram_set_int("led_lan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_blue_gpio", 7);
		nvram_set_int("led_2g_green_gpio", 8);
		nvram_set_int("led_2g_red_gpio", 9);
		nvram_set_int("led_5g_blue_gpio", 14);
		nvram_set_int("led_5g_green_gpio", 15);
		nvram_set_int("led_5g_red_gpio", 16);

		add_rc_support("2.4G 5G update");
		add_rc_support("qcawifi");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "3");
		nvram_set("wl0_HT_RxStream", "3");
		nvram_set("wl1_HT_TxStream", "3");
		nvram_set("wl1_HT_RxStream", "3");
		break;
#endif	/* RP-AC66 */

#if defined(RTAC58U)
	case MODEL_RTAC58U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth0", "eth1", wl_ifaces, "usb", "eth1", NULL, "eth2", NULL, 0);

		nvram_set_int("btn_rst_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 63|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 0);
		nvram_set_int("led_usb3_gpio", 0);
		nvram_set_int("led_lan_gpio", 2);
		nvram_set_int("led_wan_gpio", 1);
		nvram_set_int("led_2g_gpio", 58);

		nvram_set_int("led_5g_gpio", 5);

		nvram_set_int("led_wps_gpio", 3);
		nvram_set_int("led_pwr_gpio", 3);

		/* Etron xhci host:
		 *	USB2 bus: 1-1
		 *	USB3 bus: 2-1
		 * Internal ehci host:
		 * 	USB2 bus: 3-1
		 */
#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-1");
		nvram_set("ehci_ports", "1-1 3-1");
		nvram_set("ohci_ports", "1-1 4-1");
#else
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
#endif

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		add_rc_support("nodm");
		if (!strncmp(nvram_safe_get("territory_code"), "CX", 2))
			add_rc_support("nz_isp");
		else if (!strncmp(nvram_safe_get("territory_code"), "SP", 2))
			add_rc_support("spirit");
		if (get_meminfo_item("MemTotal") > 131072) {
			add_rc_support("repeater");
			add_rc_support("psta");
		}
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");

		break;
#endif //RTAC58U

#if defined(RT4GAC53U)
	case MODEL_RT4GAC53U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth2", "eth1", wl_ifaces, "usb", "eth1", NULL, NULL, NULL, 0);
		
		nvram_set_int("btn_rst_gpio", 63|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 5|GPIO_ACTIVE_LOW);

		nvram_set_int("led_wps_gpio", 0);
		nvram_set_int("led_pwr_gpio", 0);
		nvram_set_int("led_pwr_red_gpio", 1);
		nvram_set_int("led_2g_gpio", 8);
		nvram_set_int("led_5g_gpio", 9);
		nvram_set_int("led_lan_gpio", 10);
		nvram_set_int("led_usb_gpio", 11);
		nvram_set_int("led_usb3_gpio", 11);

		nvram_set_int("led_lteoff_gpio", 50|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig4_gpio", 15);
		nvram_set_int("led_sig3_gpio", 14);
		nvram_set_int("led_sig2_gpio", 13);
		nvram_set_int("led_sig1_gpio", 12);

		nvram_set("ehci_ports", "3-1 1-1");
		nvram_set("ohci_ports", "4-1 1-1");
		nvram_set("usb_buildin", "2");

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		add_rc_support("nodm");
		add_rc_support("nowan");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "2");
		nvram_set("wl1_HT_RxStream", "2");

		break;
#endif //RT4GAC53U

#if defined(RTAC82U)
	case MODEL_RTAC82U:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		set_basic_ifname_vars("eth0", "eth1", wl_ifaces, "usb", "eth1", "vlan2", "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 40|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 40|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 52|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 61);
		nvram_set_int("led_wan_red_gpio", 68);

#ifdef RTCONFIG_LAN4WAN_LED
		nvram_set_int("led_lan1_gpio", 45|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan2_gpio", 43|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan3_gpio", 42|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan4_gpio", 49|GPIO_ACTIVE_LOW);
#endif
#if 0
		//nvram_set_int("led_usb_gpio", 0);
		//nvram_set_int("led_usb3_gpio", 0);
		//nvram_set_int("led_lan_gpio", 49);
#endif

		/* Etron xhci host:
		 *	USB2 bus: 1-1
		 *	USB3 bus: 2-1
		 * Internal ehci host:
		 * 	USB2 bus: 3-1
		 */
#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-1");
		nvram_set("ehci_ports", "1-1 3-1");
		nvram_set("ohci_ports", "1-1 4-1");
#else
		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "1-1 4-1");
		}
#endif

		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX1");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("pwrctrl");
		add_rc_support("noitunes");
		add_rc_support("nodm");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "2");
		nvram_set("wl0_HT_RxStream", "2");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif //RTAC82U

#if defined(RTAC88N)
	case MODEL_RTAC88N:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");
		wl_ifaces[WL_2G_BAND] = "ath1";
		wl_ifaces[WL_5G_BAND] = "ath0";
		set_basic_ifname_vars("eth0", "eth1 eth2", wl_ifaces, "usb", "eth0", "vlan2", "vlan3", NULL, 0);

		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);	/* SW_SWITCH */
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990)
		nvram_set_int("btn_wps_gpio", 65|GPIO_ACTIVE_LOW);	/* AP148-030, WPS_SWITCH */
#elif defined(RTCONFIG_WIFI_QCA9994_QCA9994)
		nvram_set_int("btn_wps_gpio", 68|GPIO_ACTIVE_LOW);	/* AP161, WPS_SWITCH */
#endif
		nvram_set_int("led_usb_gpio", 7);			/* LED_USB1 */
		nvram_set_int("led_usb3_gpio", 8);			/* LED_USB3 */
		nvram_set_int("led_2g_gpio", 9);			/* STATUS_LED_FAIL/GREEN; MiniCard controls another LED */
		nvram_set_int("led_5g_gpio", 53);			/* STATUS_LED_PASS/RED;   MiniCard controls another LED */
		nvram_set_int("led_pwr_gpio", 26);			/* SATA LED */
		nvram_set_int("led_wps_gpio", 26);			/* SATA LED */

		/* enable bled */
		config_netdev_bled("led_2g_gpio", "ath1");
		config_netdev_bled("led_5g_gpio", "ath0");
		config_usbbus_bled("led_usb_gpio", "1 3");
		config_usbbus_bled("led_usb3_gpio", "2 4");

		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "2-1 4-1");
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "");
		}
		else{
			nvram_set("ehci_ports", "1-1 3-1");
			nvram_set("ohci_ports", "");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif	/* RTAC88N */

#if defined(BRTAC828) || defined(RTAC88S)
	case MODEL_BRTAC828:
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
	case MODEL_RTAC88S:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2)
		/* BRT-AC828 SR1~SR3, REV 1.00 ~ 1.20 */
		wan0 = "eth2";
		wan1 = "eth3";
		lan_1 = "eth0";
		lan_2 = "eth1";
#else
		/* BRT-AC828 SR4 or above, REV 1.30+ */
		wan0 = "eth0";
		wan1 = "eth3";
		lan_1 = "eth1";
		lan_2 = "eth2";
#endif

		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		if(get_wans_dualwan() & WANSCAP_LAN) {
			int wans = get_wans_dualwan();

			strcpy(lan_ifs, lan_2);
			if (wans & WANSCAP_WAN) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			} else if (wans & WANSCAP_WAN2) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
			} else {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			}
			nvram_unset("bond0_ifnames");
			set_basic_ifname_vars(wan0, lan_ifs, wl_ifaces, "usb", NULL, NULL, lan_1, wan1, 1);
		} else {
			snprintf(lan_ifs, sizeof(lan_ifs), "%s %s", lan_1, lan_2);
			nvram_set("bond0_ifnames", lan_ifs);
			set_basic_ifname_vars(wan0, "bond0", wl_ifaces, "usb", NULL, "vlan2", "vlan3", wan1, 0);
		}

		/* Override default number of guest networks.
		 * Number of guest network of each band is 6.
		 */
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3 wl0.4 wl0.5 wl0.6");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3 wl1.4 wl1.5 wl1.6");

#if defined(BRTAC828_SR1)
		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio",  7);
		nvram_set_int("led_usb3_gpio", 8);
		nvram_set_int("led_2g_gpio", 68);
		nvram_set_int("led_5g_gpio", 67);
		nvram_set_int("led_pwr_gpio", 53);
		nvram_set_int("led_wps_gpio", 53);
#else
		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_ejusb1_gpio", 17|GPIO_ACTIVE_LOW | GPIO_EJUSB_PORT(2));
		nvram_set_int("btn_ejusb2_gpio", 24|GPIO_ACTIVE_LOW | GPIO_EJUSB_PORT(1));
		nvram_set_int("led_usb_gpio",  7);
		nvram_set_int("led_usb3_gpio", 15);
		nvram_set_int("led_2g_gpio", 68);
		nvram_set_int("led_5g_gpio", 67);
		nvram_set_int("led_wps_gpio", 53);
		nvram_set_int("led_pwr_gpio", 53);
		nvram_set_int("led_pwr_red_gpio", 57);
		nvram_set_int("led_wan_gpio", 9);
		nvram_set_int("led_wan2_gpio", 6);
		nvram_set_int("led_wan_red_gpio", 56);
		nvram_set_int("led_wan2_red_gpio", 55);	/* RTCONFIG_WANRED_LED && RTCONFIG_WANPORT2 */
		nvram_set_int("led_failover_gpio", 26);
#if !defined(BRTAC828_SR2)
		nvram_set_int("led_sata_gpio", 25);
#endif
#endif

		/* enable bled */
		config_swports_bled_sleep("led_wan_gpio", 0);
		config_swports_bled_sleep("led_wan2_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");
		config_usbbus_bled("led_usb_gpio", "1 2");
		config_usbbus_bled("led_usb3_gpio", "3 4");
		config_interrupt_bled("led_sata_gpio", "241");

		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			int dualwan_if = get_dualwan_by_unit(unit);
			char nvram_ports[20] = "wanports_mask", led_wan_gpio[20] = "led_wan_gpio";

			if (access_point_mode()) {
				if (unit) {
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (unit == WAN_UNIT_FIRST)? wan0 : wan1);
			} else {
				if (dualwan_if != WANS_DUALWAN_IF_WAN &&
				    dualwan_if != WANS_DUALWAN_IF_WAN2)
					continue;
				if (unit) {
					snprintf(nvram_ports, sizeof(nvram_ports), "wan%dports_mask", unit);
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (dualwan_if == WANS_DUALWAN_IF_WAN)? wan0 : wan1);
			}
		}

		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "4-1 2-1");
#if defined(RTCONFIG_M2_SSD)
			nvram_set("ehci_ports", "3-1 1-1 ata1");
#else
			nvram_set("ehci_ports", "3-1 1-1");
#endif
			nvram_set("ohci_ports", "");
		}
		else{
#if defined(RTCONFIG_M2_SSD)
			nvram_set("ehci_ports", "3-1 1-1 ata1");
#else
			nvram_set("ehci_ports", "3-1 1-1");
#endif
			nvram_set("ohci_ports", "");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
#if defined(RTCONFIG_M2_SSD)
		add_rc_support("usbX3");
#else
		add_rc_support("usbX2");
#endif
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("nodm");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif	/* BRTAC828 || RTAC88S */

#if defined(RTAD7200)
	case MODEL_RTAD7200:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");

#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2)
		/* BRT-AC828 SR1~SR3, REV 1.00 ~ 1.20 */
		wan0 = "eth2";
		wan1 = "eth3";
		lan_1 = "eth0";
		lan_2 = "eth1";
#else
		/* BRT-AC828 SR4 or above, REV 1.30+ */
		wan0 = "eth0";
		wan1 = "eth3";
		lan_1 = "eth1";
		lan_2 = "eth2";
#endif

		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		wl_ifaces[WL_5G_2_BAND] = "FAKE5G2";	/* Padding, make sure we can get right wl_unit when enumerating wl_ifnames. */
		wl_ifaces[WL_60G_BAND] = "wlan0";
		if(get_wans_dualwan() & WANSCAP_LAN) {
			int wans = get_wans_dualwan();

			strcpy(lan_ifs, lan_2);
			if (wans & WANSCAP_WAN) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			} else if (wans & WANSCAP_WAN2) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
			} else {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			}
			nvram_unset("bond0_ifnames");
			set_basic_ifname_vars(wan0, lan_ifs, wl_ifaces, "usb", NULL, NULL, lan_1, wan1, 1);
		} else {
			snprintf(lan_ifs, sizeof(lan_ifs), "%s %s", lan_1, lan_2);
			nvram_set("bond0_ifnames", lan_ifs);
			set_basic_ifname_vars(wan0, "bond0", wl_ifaces, "usb", NULL, "vlan2", "vlan3", wan1, 0);
		}

		/* Override default number of guest networks of 802.11ad Wigig.
		 * Because 802.11ad driver for kernel v3.4 doesn't support multiple SSID.
		 */
		nvram_set("wl3_vifnames", "");

		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_ejusb1_gpio", 17|GPIO_ACTIVE_LOW | GPIO_EJUSB_PORT(2));
		nvram_set_int("btn_ejusb2_gpio", 24|GPIO_ACTIVE_LOW | GPIO_EJUSB_PORT(1));
		nvram_set_int("led_usb_gpio",  7);
		nvram_set_int("led_usb3_gpio", 15);
		nvram_set_int("led_2g_gpio", 68);
		nvram_set_int("led_5g_gpio", 67);
		nvram_set_int("led_wps_gpio", 53);
		nvram_set_int("led_pwr_gpio", 53);
		nvram_set_int("led_pwr_red_gpio", 57);
		nvram_set_int("led_wan_gpio", 9);
		nvram_set_int("led_wan2_gpio", 6);
		nvram_set_int("led_wan_red_gpio", 56);
		nvram_set_int("led_wan2_red_gpio", 55);	/* RTCONFIG_WANRED_LED && RTCONFIG_WANPORT2 */
		nvram_set_int("led_failover_gpio", 26);
#if defined(RTCONFIG_SATA_LED)
		nvram_set_int("led_sata_gpio", 25);
#else
		nvram_set_int("led_60g_gpio", 25);
#endif

		/* enable bled */
		config_swports_bled_sleep("led_wan_gpio", 0);
		config_swports_bled_sleep("led_wan2_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");
		config_usbbus_bled("led_usb_gpio", "1 2");
		config_usbbus_bled("led_usb3_gpio", "3 4");
#if defined(RTCONFIG_SATA_LED)
		config_interrupt_bled("led_sata_gpio", "241");
#else
		config_netdev_bled("led_60g_gpio", "wlan0");
#endif

		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			int dualwan_if = get_dualwan_by_unit(unit);
			char nvram_ports[20] = "wanports_mask", led_wan_gpio[20] = "led_wan_gpio";

			if (access_point_mode()) {
				if (unit) {
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (unit == WAN_UNIT_FIRST)? wan0 : wan1);
			} else {
				if (dualwan_if != WANS_DUALWAN_IF_WAN &&
				    dualwan_if != WANS_DUALWAN_IF_WAN2)
					continue;
				if (unit) {
					snprintf(nvram_ports, sizeof(nvram_ports), "wan%dports_mask", unit);
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (dualwan_if == WANS_DUALWAN_IF_WAN)? wan0 : wan1);
			}
		}

		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "4-1 2-1");
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		else{
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		add_rc_support("nodm");
		add_rc_support("meoVoda");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif	/* RTAD7200 */

#if defined(GTAX6000)
	case MODEL_GTAX6000:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");

		wan0 = "eth0";
		wan1 = "eth3";
		lan_1 = "eth1";
		lan_2 = "eth2";

		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		if(get_wans_dualwan() & WANSCAP_LAN) {
			int wans = get_wans_dualwan();

			strcpy(lan_ifs, lan_2);
			if (wans & WANSCAP_WAN) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			} else if (wans & WANSCAP_WAN2) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
			} else {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			}
			nvram_unset("bond0_ifnames");
			set_basic_ifname_vars(wan0, lan_ifs, wl_ifaces, "usb", NULL, NULL, lan_1, wan1, 1);
		} else {
			snprintf(lan_ifs, sizeof(lan_ifs), "%s %s", lan_1, lan_2);
			nvram_set("bond0_ifnames", lan_ifs);
			set_basic_ifname_vars(wan0, "bond0", wl_ifaces, "usb", NULL, "vlan2", "vlan3", wan1, 0);
		}

		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 68);
		nvram_set_int("led_5g_gpio", 67);
		nvram_set_int("led_wps_gpio", 53);
		nvram_set_int("led_pwr_gpio", 53);
		nvram_set_int("led_wan_gpio", 9);

		/* enable bled */
		config_swports_bled_sleep("led_wan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			int dualwan_if = get_dualwan_by_unit(unit);
			char nvram_ports[20] = "wanports_mask", led_wan_gpio[20] = "led_wan_gpio";

			if (access_point_mode()) {
				if (unit) {
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (unit == WAN_UNIT_FIRST)? wan0 : wan1);
			} else {
				if (dualwan_if != WANS_DUALWAN_IF_WAN &&
				    dualwan_if != WANS_DUALWAN_IF_WAN2)
					continue;
				if (unit) {
					snprintf(nvram_ports, sizeof(nvram_ports), "wan%dports_mask", unit);
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (dualwan_if == WANS_DUALWAN_IF_WAN)? wan0 : wan1);
			}
		}

		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "4-1 2-1");
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		else{
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif	/* GTAX6000 */

#if defined(GTAX6000N)
	case MODEL_GTAX6000N:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");

		wan0 = "eth0";
		wan1 = "eth3";
		lan_1 = "eth1";
		lan_2 = "eth2";

		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		if(get_wans_dualwan() & WANSCAP_LAN) {
			int wans = get_wans_dualwan();

			strcpy(lan_ifs, lan_2);
			if (wans & WANSCAP_WAN) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			} else if (wans & WANSCAP_WAN2) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
			} else {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			}
			nvram_unset("bond0_ifnames");
			set_basic_ifname_vars(wan0, lan_ifs, wl_ifaces, "usb", NULL, NULL, lan_1, wan1, 1);
		} else {
			snprintf(lan_ifs, sizeof(lan_ifs), "%s %s", lan_1, lan_2);
			nvram_set("bond0_ifnames", lan_ifs);
			set_basic_ifname_vars(wan0, "bond0", wl_ifaces, "usb", NULL, "vlan2", "vlan3", wan1, 0);
		}

		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 68);
		nvram_set_int("led_5g_gpio", 67);
		nvram_set_int("led_wps_gpio", 53);
		nvram_set_int("led_pwr_gpio", 53);
		nvram_set_int("led_wan_gpio", 9);

		/* enable bled */
		config_swports_bled_sleep("led_wan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			int dualwan_if = get_dualwan_by_unit(unit);
			char nvram_ports[20] = "wanports_mask", led_wan_gpio[20] = "led_wan_gpio";

			if (access_point_mode()) {
				if (unit) {
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (unit == WAN_UNIT_FIRST)? wan0 : wan1);
			} else {
				if (dualwan_if != WANS_DUALWAN_IF_WAN &&
				    dualwan_if != WANS_DUALWAN_IF_WAN2)
					continue;
				if (unit) {
					snprintf(nvram_ports, sizeof(nvram_ports), "wan%dports_mask", unit);
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (dualwan_if == WANS_DUALWAN_IF_WAN)? wan0 : wan1);
			}
		}

		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "4-1 2-1");
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		else{
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif	/* GTAX6000N */

#if defined(GTAX6000S)
	case MODEL_GTAX6000S:
		nvram_set("boardflags", "0x100"); // although it is not used in ralink driver, set for vlan
		//nvram_set("vlan1hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		//nvram_set("vlan2hwname", "et0");  // vlan. used to get "%smacaddr" for compare and find parent interface.
		nvram_set("lan_ifname", "br0");

		wan0 = "eth0";
		wan1 = "eth3";
		lan_1 = "eth1";
		lan_2 = "eth2";

		wl_ifaces[WL_2G_BAND] = "ath0";
		wl_ifaces[WL_5G_BAND] = "ath1";
		if(get_wans_dualwan() & WANSCAP_LAN) {
			int wans = get_wans_dualwan();

			strcpy(lan_ifs, lan_2);
			if (wans & WANSCAP_WAN) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			} else if (wans & WANSCAP_WAN2) {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
			} else {
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan0);
				strcat(lan_ifs, " ");
				strcat(lan_ifs, wan1);
			}
			nvram_unset("bond0_ifnames");
			set_basic_ifname_vars(wan0, lan_ifs, wl_ifaces, "usb", NULL, NULL, lan_1, wan1, 1);
		} else {
			snprintf(lan_ifs, sizeof(lan_ifs), "%s %s", lan_1, lan_2);
			nvram_set("bond0_ifnames", lan_ifs);
			set_basic_ifname_vars(wan0, "bond0", wl_ifaces, "usb", NULL, "vlan2", "vlan3", wan1, 0);
		}

		nvram_set_int("btn_rst_gpio", 54|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 68);
		nvram_set_int("led_5g_gpio", 67);
		nvram_set_int("led_wps_gpio", 53);
		nvram_set_int("led_pwr_gpio", 53);
		nvram_set_int("led_wan_gpio", 9);

		/* enable bled */
		config_swports_bled_sleep("led_wan_gpio", 0);
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");

		for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
			int dualwan_if = get_dualwan_by_unit(unit);
			char nvram_ports[20] = "wanports_mask", led_wan_gpio[20] = "led_wan_gpio";

			if (access_point_mode()) {
				if (unit) {
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (unit == WAN_UNIT_FIRST)? wan0 : wan1);
			} else {
				if (dualwan_if != WANS_DUALWAN_IF_WAN &&
				    dualwan_if != WANS_DUALWAN_IF_WAN2)
					continue;
				if (unit) {
					snprintf(nvram_ports, sizeof(nvram_ports), "wan%dports_mask", unit);
					snprintf(led_wan_gpio, sizeof(led_wan_gpio), "led_wan%d_gpio", unit + 1);
				}
				append_netdev_bled_if(led_wan_gpio, (dualwan_if == WANS_DUALWAN_IF_WAN)? wan0 : wan1);
			}
		}

		if(nvram_get_int("usb_usb3") == 1){
			nvram_set("xhci_ports", "4-1 2-1");
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		else{
			nvram_set("ehci_ports", "3-1 1-1");
			nvram_set("ohci_ports", "");
		}
		nvram_set("ct_max", "300000"); // force

		if (nvram_get("wl_mssid") && nvram_match("wl_mssid", "1"))
			add_rc_support("mssid");
		add_rc_support("2.4G 5G update");
		add_rc_support("usbX2");
		add_rc_support("qcawifi");
		add_rc_support("switchctrl");
		add_rc_support("manual_stb");
		add_rc_support("11AC");
		// the following values is model dep. so move it from default.c to here
		nvram_set("wl0_HT_TxStream", "4");
		nvram_set("wl0_HT_RxStream", "4");
		nvram_set("wl1_HT_TxStream", "4");
		nvram_set("wl1_HT_RxStream", "4");
		break;
#endif	/* GTAX6000S */

#ifdef CONFIG_BCMWL5
#ifndef RTCONFIG_BCMWL6
#ifdef RTN10U
	case MODEL_RTN10U:
#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			set_lan_phy("vlan0");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan1");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("lan_ifnames", "vlan0 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set_int("led_usb_gpio", 8);
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "2048");
		if (nvram_match("wl0_country_code", "XU")){
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}
		add_rc_support("2.4G mssid usbX1 nomedia small_fw");
		break;
#endif

#if defined(RTN10P) || defined(RTN10D1) || defined(RTN10PV2)
	case MODEL_RTN10P:
	case MODEL_RTN10D1:
	case MODEL_RTN10PV2:
#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			set_lan_phy("vlan0");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan1");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("lan_ifnames", "vlan0 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 7);
		nvram_set("sb/1/maxp2ga0", "0x52");
		nvram_set("sb/1/maxp2ga1", "0x52");
		if (!nvram_get("ct_max")) {
			if (model == MODEL_RTN10D1)
				nvram_set_int("ct_max", 1024);
		}
		if (nvram_match("wl0_country_code", "XU")){
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}

		add_rc_support("2.4G mssid");
#ifdef RTCONFIG_KYIVSTAR
		add_rc_support("kyivstar");
#endif
		break;
#endif

	case MODEL_APN12HP:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3 wl0.4 wl0.5 wl0.6 wl0.7");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set("sb/1/ledbh0", "11");
		nvram_set("sb/1/ledbh1", "11");
		nvram_set("sb/1/ledbh2", "11");
		nvram_set("sb/1/ledbh3", "2");
		nvram_set("sb/1/ledbh5", "11");
		nvram_set("sb/1/ledbh6", "11");
		add_rc_support("pwrctrl");
		nvram_set_int("et_swleds", 0);
		nvram_set("productid", "AP-N12");
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12HP:
	case MODEL_RTN12HP_B1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid update");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);	/* does HP have it */
		nvram_set_int("sb/1/ledbh5", 7);			/* is active_high? set 7 then */
		if (!nvram_match("hardware_version", "RTN12HP_B1-2.0.1.5"))	//6691 PA
			add_rc_support("pwrctrl");
		if (nvram_match("wl0_country_code", "US"))
		{
			if (nvram_match("wl0_country_rev", "37"))
			{
				nvram_set("wl0_country_rev", "16");
			}

			if (nvram_match("sb/1/regrev", "37"))
			{
				nvram_set("sb/1/regrev", "16");
			}
		}
		if (nvram_match("wl0_country_code", "EU"))
		{
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12D1:
	case MODEL_RTN12VP:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh5", 7);
		nvram_set("sb/1/maxp2ga0", "0x52");
		nvram_set("sb/1/maxp2ga1", "0x52");
		nvram_set("sb/1/cck2gpo", "0x0");
		nvram_set("sb/1/ofdm2gpo0", "0x2000");
		nvram_set("sb/1/ofdm2gpo1", "0x6442");
		nvram_set("sb/1/ofdm2gpo", "0x64422000");
		nvram_set("sb/1/mcs2gpo0", "0x2200");
		nvram_set("sb/1/mcs2gpo1", "0x6644");
		nvram_set("sb/1/mcs2gpo2", "0x2200");
		nvram_set("sb/1/mcs2gpo3", "0x6644");
		nvram_set("sb/1/mcs2gpo4", "0x4422");
		nvram_set("sb/1/mcs2gpo5", "0x8866");
		nvram_set("sb/1/mcs2gpo6", "0x4422");
		nvram_set("sb/1/mcs2gpo7", "0x8866");
		add_rc_support("update");
		if (model == MODEL_RTN12D1) {
			if (nvram_match("regulation_domain", "US") && nvram_match("sb/1/regrev", "0")) {
				nvram_set("sb/1/regrev", "2");
			}
		}
		if(nvram_match("wl0_country_code", "XU")){
			nvram_set("sb/1/eu_edthresh2g", "-69"); //for CE adaptivity certification
		}
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12C1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("et_swleds", 0x0f);
		nvram_set_int("sb/1/ledbh4", 2);
		nvram_set_int("sb/1/ledbh5", 11);
		nvram_set_int("sb/1/ledbh6", 11);
#ifdef RTCONFIG_SWMODE_SWITCH
		nvram_set_int("btn_swmode1_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode2_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode3_gpio", 8|GPIO_ACTIVE_LOW);
		add_rc_support("swmode_switch");
		nvram_set_int("swmode_switch", 1);
#endif
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12B1:
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		add_rc_support("2.4G mssid");
		nvram_set_int("btn_rst_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh5", 2);
#ifdef RTCONFIG_SWMODE_SWITCH
		nvram_set_int("btn_swmode1_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode2_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_swmode3_gpio", 8|GPIO_ACTIVE_LOW);
		add_rc_support("swmode_switch");
		nvram_set_int("swmode_switch", 1);
#endif
		/* go to common N12* init */
		goto case_MODEL_RTN12X;

	case MODEL_RTN12:
		nvram_set_int("btn_rst_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set("boardflags", "0x310");
		/* fall through to common N12* init */
	case_MODEL_RTN12X:
		//nvram_set("lan_ifnames", "vlan0 eth1");
		//nvram_set("wan_ifnames", "eth0");
		//nvram_set("wandevs", "et0");
		nvram_set("wl_ifnames", "eth1");

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan1 vlan2");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan0");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan1");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan0 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wandevs", "et0");
#endif
		break;

#ifdef RTN14UHP
	case MODEL_RTN14UHP:
		nvram_unset("odmpid");
		nvram_unset("et_dispatch_mode");
		nvram_unset("wl_dispatch_mode");
		add_rc_support("pwrctrl");
#ifdef RTCONFIG_LAN4WAN_LED
		if (nvram_match("odmpid", "TW")) {
			add_rc_support("lanwan_led2");
		}
#endif

#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan0");

		if (!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if (nvram_get("wans_dualwan")) {
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					if (get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
						int wan_vid = nvram_get_int("switch_wan0tagid");
						if (wan_vid)
							sprintf(wan_if, "vlan%d", wan_vid);
						else
							sprintf(wan_if, "eth0");
						add_wan_phy(wan_if);
					}
					else if (get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan1");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
					add_wan_phy("usb2");
#endif
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan0 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 24|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 25|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 6);
#ifdef RTCONFIG_LAN4WAN_LED
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan1_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan2_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan3_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan4_gpio", 3|GPIO_ACTIVE_LOW);
#endif
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "2048");
		add_rc_support("2.4G mssid media usbX1 update");
		break;
#endif

#ifdef RTN15U
	case MODEL_RTN15U:
#ifdef RTCONFIG_DUALWAN
		set_lan_phy("vlan1");

		if (!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth1");

		if (nvram_get("wans_dualwan")) {
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					if (get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth1");
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
						int wan_vid = nvram_get_int("switch_wan0tagid");
						if (wan_vid)
							sprintf(wan_if, "vlan%d", wan_vid);
						else
							sprintf(wan_if, "eth0");
						add_wan_phy(wan_if);
					}
					else if (get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy("eth0");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
					add_wan_phy("usb2");
#endif
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "");

		nvram_set_int("btn_rst_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 9);
		nvram_set_int("led_lan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("sb/1/ledbh0", 0x82);
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "15000");
		add_rc_support("2.4G mssid usbX1 nomedia");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		break;
#endif

#ifdef RTN16
	case MODEL_RTN16:
#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "");

		nvram_set("wl0_vifnames", "wl0.1");	/* Only one gueset network? */

		nvram_set_int("btn_rst_gpio", 6|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-2 1-1");
		nvram_set("ohci_ports", "2-2 2-1");
		nvram_set("boardflags", "0x310");
		nvram_set("sb/1/boardflags", "0x310");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("2.4G update usbX2 mssid");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		break;
#endif

#ifdef RTN53
	case MODEL_RTN53:
		nvram_set("lan_ifnames", "vlan0 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 1|GPIO_ACTIVE_LOW);
		/* change lan interface to vlan0 */
		nvram_set("vlan0hwname", "et0");
		nvram_set("landevs", "vlan0 wl0 wl1");
		nvram_unset("vlan2ports");
		nvram_unset("vlan2hwname");
		/* end */
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "2048");

		add_rc_support("2.4G 5G mssid no5gmssid small_fw");
#ifdef RTCONFIG_WLAN_LED
		add_rc_support("led_2g");
		nvram_set("led_5g", "1");
#endif
		break;
#endif
#endif	// RTCONFIG_BCMWL6

#ifdef RTN18U
	case MODEL_RTN18U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("landevs", "vlan1 wl0");

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 13|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 13);
#endif
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 7|GPIO_ACTIVE_LOW);

		if (nvram_match("bl_version", "3.0.0.7")) {
			nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_lan_gpio", 6|GPIO_ACTIVE_LOW);
		}
		else if (nvram_match("bl_version", "1.0.0.0")) {
			nvram_set_int("led_usb_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 6|GPIO_ACTIVE_LOW);
			nvram_set_int("led_lan_gpio", 9|GPIO_ACTIVE_LOW);
			nvram_set_int("led_2g_gpio", 10|GPIO_ACTIVE_LOW);
		}
		else{
			nvram_set_int("led_usb_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_usb3_gpio", 14|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 6|GPIO_ACTIVE_LOW);
			nvram_set_int("led_lan_gpio", 9|GPIO_ACTIVE_LOW);
		}

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G update usbX2");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");

		if (nvram_match("bl_version", "1.0.0.0"))
			add_rc_support("led_2g");

		break;
#endif

#ifdef DSL_AC68U
	case MODEL_DSLAC68U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("landevs", "vlan1 wl0 wl1");
#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			//Andy Chiu, 2015/09/11
			int ewan_dot1q = nvram_get_int("ewan_dot1q");	//Andy Chiu, 2015/09/08
			int ewan_vid = nvram_get_int("ewan_vid");		//Andy Chiu, 2015/09/08
			char vlanif[16];
			if(ewan_dot1q && ewan_vid> 0 && !(ewan_vid >= DSL_WAN_VID && ewan_vid<= (DSL_WAN_VID + 7)))
				sprintf(vlanif, "vlan%d", ewan_vid);
			else
				strcpy(vlanif, "vlan4");

			if (get_wans_dualwan()&WANSCAP_DSL && get_wans_dualwan()&WANSCAP_LAN)
			{
				//Andy Chiu, 2015/09/11
				char buf[64];
				sprintf(buf, "%s %s", DSL_WAN_VIF, vlanif);
				nvram_set("wandevs", buf);
			}
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN)
						add_wan_phy(vlanif);	//Andy Chiu, 2015/09/11
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_DSL)
						add_wan_phy(DSL_WAN_VIF);
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
			{
				char buf[64];
				snprintf(buf, sizeof(buf), "%s usb", DSL_WAN_VIF);
				nvram_set("wan_ifnames", buf);
			}
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", DSL_WAN_VIF);
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", DSL_WAN_VIF);
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_wan_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);	// 4360's fake led 5g
		nvram_set_int("led_usb3_gpio", 14|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1");
		nvram_set("ohci_ports", "3-1");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1");
			nvram_set("ohci_ports", "3-1");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1");
			nvram_set("ohci_ports", "2-1");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX1");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("dsl");
		add_rc_support("vdsl");
		add_rc_support("spectrum");

		if (	nvram_match("wl1_country_code", "EU") &&
				nvram_match("wl1_country_rev", "13")) {
			nvram_set("0:ed_thresh2g", "-70");
			nvram_set("0:ed_thresh5g", "-70");
			nvram_set("1:ed_thresh2g", "-72");
			nvram_set("1:ed_thresh5g", "-72");
		}

		if (nvram_match("x_Setting", "0")) {
			char ver[64];
			snprintf(ver, sizeof(ver), "%s.%s_%s", nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno"));
			nvram_set("innerver", ver);
		}
		break;
#endif

#ifdef RTAC3200
	case MODEL_RTAC3200:
		nvram_set("vlan1hwname", "et0");
		nvram_set("landevs", "vlan1 wl0 wl1 wl2");

#ifdef RTCONFIG_GMAC3
		if (nvram_match("gmac3_enable", "1"))
			hw_name = "et2";
		else {
			nvram_set("fwd_wlandevs", "");
			nvram_set("fwddevs", "");
		}
#endif

#ifdef RTCONFIG_DUALWAN
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
			nvram_set("wandevs", "vlan2 vlan3");
#ifdef RTCONFIG_GMAC3
		else
			nvram_set("wandevs", hw_name);
#endif
		set_lan_phy("vlan1");

		if (!(get_wans_dualwan()&WANSCAP_2G))
			add_lan_phy("eth2");
		if (!(get_wans_dualwan()&WANSCAP_5G)) {
			add_lan_phy("eth1");
			add_lan_phy("eth3");
		}

		if (nvram_get("wans_dualwan")) {
			set_wan_phy("");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					if (get_wans_dualwan()&WANSCAP_WAN)
						add_wan_phy("vlan3");
					else
						add_wan_phy(the_wan_phy());
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
					add_wan_phy("eth2");
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G) {
					add_wan_phy("eth1");
					add_wan_phy("eth3");
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
						int wan_vid = nvram_get_int("switch_wan0tagid");
						if (wan_vid) {
							sprintf(wan_if, "vlan%d", wan_vid);
							add_wan_phy(wan_if);
						}
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_wans_dualwan()&WANSCAP_LAN)
						add_wan_phy("vlan2");
					else
						add_wan_phy(the_wan_phy());
				}
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
					add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
				else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
					add_wan_phy("usb2");
#endif
			}
		}
		else
			nvram_set("wan_ifnames", "eth0 usb");
#else
		nvram_set("lan_ifnames", "vlan1 eth2 eth1 eth3");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth2 eth1 eth3");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set("wl2_vifnames", "wl2.1 wl2.2 wl2.3");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 4|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("btn_led_gpio", 15);			// active high
#endif
		nvram_set_int("rst_hw_gpio", 17|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("smart_connect");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		add_rc_support("app");

		break;
#endif

#ifdef RTAC68U
	case MODEL_RTAC68U:
		check_cfe_ac68u();
		nvram_set("vlan1hwname", "et0");
		nvram_set("landevs", "vlan1 wl0 wl1");	

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid) {
								sprintf(wan_if, "vlan%d", wan_vid);
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");	
#if defined(RTCONFIG_AMAS)
#ifdef RTCONFIG_DPSTA		
		if (dpsta_mode()) {
			nvram_set("sta_phy_ifnames", "dpsta");
		}
#endif
		else if (dpsr_mode()) {
			nvram_set("sta_phy_ifnames", "eth1 eth2");
		}
		else
		{
			nvram_set("sta_phy_ifnames", "eth1 eth2");		
		}
		
		if(nvram_get_int("re_mode") == 1) {
			nvram_set("lan_ifnames", "vlan1 vlan2 eth1 eth2");
			nvram_set("eth_ifnames", "vlan2");
			nvram_set("wait_band", "10");
			nvram_set("wait_wifi", "15");
		}
		nvram_set("sta_ifnames", "eth1 eth2");
#endif
#ifdef RT4GAC68U
		nvram_set_int("led_lan_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_3g_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lte_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig1_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig2_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_sig3_gpio", 8|GPIO_ACTIVE_LOW);

		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
#else // RT4GAC68U
		if (!is_ac66u_v2_series()) {
			nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);// 4360's fake led 5g
#ifdef RTCONFIG_LED_BTN
			nvram_set_int("btn_led_gpio", 5);		// active high
#endif
#ifdef RTCONFIG_LOGO_LED
			nvram_set_int("led_logo_gpio", 4|GPIO_ACTIVE_LOW);
#endif
			nvram_set_int("led_usb_gpio", 0|GPIO_ACTIVE_LOW);

			nvram_set_int("led_wps_gpio", 3|GPIO_ACTIVE_LOW);
		}
		else {
			nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		}
#endif  // RT4GAC68U
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		if (!is_ac66u_v2_series()) {
			nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
			nvram_set_int("led_usb3_gpio", 14|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
			nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
#endif
		} else {
			nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
			nvram_set_int("led_wan_gpio", 5);
		}

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
#ifdef RT4GAC68U
		if(nvram_get_int("usb_gobi") == 2){
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_set("ehci_ports", "2-1 2-2.2 2-2.1");
			nvram_set("ohci_ports", "3-1 3-2.2 3-2.1");
		}
#else
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#endif
#else // RTCONFIG_XHCIMODE
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
#ifdef RT4GAC68U
#if 0 // old layout. there is the USB 2.0 port.
			if(nvram_get_int("usb_gobi") == 2){
				nvram_set("ehci_ports", "2-1 2-2");
				nvram_set("ohci_ports", "3-1 3-2");
			}
			else{
				nvram_set("ehci_ports", "2-1 2-2.2 2-2.1");
				nvram_set("ohci_ports", "3-1 3-2.2 3-2.1");
			}
#else
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
#endif
#else
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
#endif
		}
		else{
			nvram_unset("xhci_ports");
#ifdef RT4GAC68U
#if 0 // old layout. there is the USB 2.0 port.
			nvram_set("ehci_ports", "1-1 1-2.2 1-2.1");
			nvram_set("ohci_ports", "2-1 2-2.2 2-2.1");
#else
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
#endif
#else
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
#endif
		}
#endif // RTCONFIG_XHCIMODE

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
#ifdef RT4GAC68U
		add_rc_support("usbX1");
#else
		if (hw_usb_cap())
		add_rc_support("usbX2");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
#ifndef RT4GAC68U
		if (hw_usb_cap())
		add_rc_support("app");
#endif
		if (!hw_vht_cap())
			add_rc_support("no_vht");

		break;
#endif

#if defined(RTAC1200G) || defined(RTAC1200GP)
	case MODEL_RTAC1200G:
	case MODEL_RTAC1200GP:
		nvram_unset("et1macaddr");
		nvram_unset("et1phyaddr");
		nvram_set("wandevs", "et0");
		nvram_set("vlan1hwname", "et0");
		nvram_set("vlan2hwname", "et0");
		nvram_set("landevs", "vlan1 wl0 wl1");
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set_int("btn_rst_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 10|GPIO_ACTIVE_LOW);
//		nvram_set_int("led_5g_gpio", 11);	// active high
		nvram_set_int("led_usb_gpio", 15|GPIO_ACTIVE_LOW);

		nvram_unset("xhci_ports");
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "2-1");
//		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "250000");
		add_rc_support("mssid 2.4G 5G usbX1");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("WIFI_LOGO");
		add_rc_support("update");
		if (model == MODEL_RTAC1200GP){
			add_rc_support("meoVoda");
			add_rc_support("movistarTriple");
		}
		if (model == MODEL_RTAC1200G) {
			add_rc_support("noitunes");
			add_rc_support("nodm");
			add_rc_support("noaidisk");
		}

		break;
#endif

#if defined(GTAC5300)
	case MODEL_GTAC5300:
		model_patch();
		if (is_router_mode()) {
			nvram_set("lan_ifnames", "eth1 eth2 eth3 eth4 eth5 eth6 eth7 eth8");
			nvram_set("wan_ifname", "eth0");
			nvram_set("wan_ifnames", "eth0");
		} else {
			nvram_set("lan_ifnames", "eth0 eth1 eth2 eth3 eth4 eth5 eth6 eth7 eth8");
			nvram_set("wan_ifnames", "");
			nvram_set("wan_ifname", "");
		}
		nvram_set("wl_ifnames", "eth6 eth7 eth8");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set("wl2_vifnames", "wl2.1 wl2.2 wl2.3");

#if defined(RTCONFIG_AMAS)
#ifdef RTCONFIG_DPSTA
		if (dpsta_mode()) {
			nvram_set("sta_phy_ifnames", "dpsta");
		}
#endif
		else if (dpsr_mode()) {
			nvram_set("sta_phy_ifnames", "eth6 eth7 eth8");
		}
		else
		{
			nvram_set("sta_phy_ifnames", "eth6 eth7 eth8");
		}
		if(nvram_get_int("re_mode") == 1) {
			nvram_set("eth_ifnames", "eth0");
			nvram_set("wait_band", "10");
			nvram_set("wait_wifi", "15");
		}
		nvram_set("sta_ifnames", "eth6 eth7 eth8");
		nvram_set("wired_ifnames", "eth1 eth2 eth3 eth4 eth5 bond0");
#endif 

		nvram_set("1:ledbh9", "0x7");
		nvram_set("2:ledbh9", "0x7");
		nvram_set("3:ledbh9", "0x7");

		nvram_set_int("led_pwr_gpio", 17|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 18|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_normal_gpio", 21);
#ifdef RTCONFIG_LAN4WAN_LED
		nvram_set_int("led_lan1_gpio", 25);
#endif
		nvram_set_int("led_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("pwr_usb_gpio", 26);

		nvram_set_int("btn_wltog_gpio", 28|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 29|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 30|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_led_gpio", 31);
		if(nvram_get_int("usb_usb3") == 1)
			nvram_set("xhci_ports", "2-2 2-1");
		else
			nvram_unset("xhci_ports");
		nvram_set("ehci_ports", "3-2 3-1");
		nvram_set("ohci_ports", "4-2 4-1");

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_LAN) {
				if (nvram_match("wans_lanport", "2"))
					set_lan_phy("eth1 eth4 eth3 eth5");
				else if (nvram_match("wans_lanport", "1"))
					set_lan_phy("eth2 eth4 eth3 eth5");
				else if (nvram_match("wans_lanport", "4"))
					set_lan_phy("eth2 eth1 eth3 eth5");
				else if (nvram_match("wans_lanport", "3"))
					set_lan_phy("eth2 eth1 eth4 eth5");
			}
			else
				set_lan_phy("eth2 eth1 eth4 eth3 eth5");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth6");
			if (!(get_wans_dualwan()&WANSCAP_5G)) {
				add_lan_phy("eth7");
				add_lan_phy("eth8");
			}
			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						sprintf(wan_if, "eth%d", nvram_get_int("wans_lanport"));
						add_wan_phy(wan_if);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth6");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth7");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid) {
								add_wan_phy("eth0.v0");
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else
			nvram_unset("wan1_ifname");
#endif	// RTCONFIG_DUALWAN

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
//		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("smart_connect");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		add_rc_support("app");

		break;
#endif

#if defined(RTAC86U) || defined(AC2900)
	case MODEL_RTAC86U:
		if (is_router_mode()) {
			nvram_set("lan_ifnames", "eth1 eth2 eth3 eth4 eth5 eth6");
			nvram_set("wan_ifnames", "eth0");
			nvram_set("wan_ifname", "eth0");
		} else {
			nvram_set("lan_ifnames", "eth0 eth1 eth2 eth3 eth4 eth5 eth6");
			nvram_set("wan_ifnames", "");
			nvram_set("wan_ifname", "");
		}
		nvram_set("wl_ifnames", "eth5 eth6");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

#if defined(RTCONFIG_AMAS)
#ifdef RTCONFIG_DPSTA
		if (dpsta_mode()) {
			nvram_set("sta_phy_ifnames", "dpsta");
		}
#endif
		else if (dpsr_mode()) {
			nvram_set("sta_phy_ifnames", "eth5 eth6");
		}
		else
		{
			nvram_set("sta_phy_ifnames", "eth5 eth6");
		}
		if(nvram_get_int("re_mode") == 1) {
			nvram_set("eth_ifnames", "eth0");
			nvram_set("wait_band", "10");
			nvram_set("wait_wifi", "15");
		}
		nvram_set("sta_ifnames", "eth5 eth6");
		nvram_set("wired_ifnames", "eth1 eth2 eth3 eth4");
#endif

		nvram_set("0:ledbh9", "0x7");
		nvram_set("1:ledbh9", "0x7");

		nvram_set_int("led_pwr_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_normal_gpio", 21);
#ifdef RTCONFIG_LAN4WAN_LED
		nvram_set_int("led_lan1_gpio", 16);
		nvram_set_int("led_lan2_gpio", 17);
		nvram_set_int("led_lan3_gpio", 18);
		nvram_set_int("led_lan4_gpio", 19);
#endif
		nvram_set_int("led_wps_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb3_gpio", 12|GPIO_ACTIVE_LOW);

		nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 22|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 23|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_led_gpio", 14);

		nvram_set_int("pwr_usb_gpio", 3|GPIO_ACTIVE_LOW);

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "2-2");
		nvram_set("ehci_ports", "3-2 3-1");
		nvram_set("ohci_ports", "4-2 4-1");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "2-2");
			nvram_set("ehci_ports", "3-2 3-1");
			nvram_set("ohci_ports", "4-2 4-1");
		} else {
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "3-2 3-1");
			nvram_set("ohci_ports", "4-2 4-1");
		}
#endif

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_LAN) {
				if (nvram_match("wans_lanport", "1"))
					set_lan_phy("eth1 eth2 eth3");
				else if (nvram_match("wans_lanport", "2"))
					set_lan_phy("eth1 eth2 eth4");
				else if (nvram_match("wans_lanport", "3"))
					set_lan_phy("eth1 eth3 eth4");
				else if (nvram_match("wans_lanport", "4"))
					set_lan_phy("eth2 eth3 eth4");
			}
			else
				set_lan_phy("eth1 eth2 eth3 eth4");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth5");
			if (!(get_wans_dualwan()&WANSCAP_5G)) {
				add_lan_phy("eth6");
			}
			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (nvram_match("wans_lanport", "1"))
							sprintf(wan_if, "eth4");
						else if (nvram_match("wans_lanport", "2"))
							sprintf(wan_if, "eth3");
						else if (nvram_match("wans_lanport", "3"))
							sprintf(wan_if, "eth2");
						else if (nvram_match("wans_lanport", "4"))
							sprintf(wan_if, "eth1");
						add_wan_phy(wan_if);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth5");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth6");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid) {
								sprintf(wan_if, "eth0.v0");
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else
			nvram_unset("wan1_ifname");
#endif	// RTCONFIG_DUALWAN

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
//		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("smart_connect");
		add_rc_support("wifi2017");
		add_rc_support("meoVoda");
#if defined(RTAC86U)
		add_rc_support("app");
#endif
#if defined(AC2900)
		add_rc_support("noitunes");
#endif
		break;
#endif

#ifdef RTCONFIG_BCM_7114
	case MODEL_RTAC5300:
		nvram_set("2:ledbh9", "0x7");
		nvram_set("wl2_vifnames", "wl2.1 wl2.2 wl2.3");
#if defined(RTAC5300)
		update_boardlimit_ac5300();
#endif
	case MODEL_RTAC88U:
	case MODEL_RTAC3100:
		ldo_patch();

		set_tcode_misc();
#if defined(RTAC88U) || defined(RTAC3100)
		//if(nvram_match("lazy_et", "1"))
			nvram_set("et_rxlazy_timeout",  "1000");
		//else
		//	nvram_set("et_rxlazy_timeout",  "300");
#endif

		nvram_set("0:ledbh9", "0x7");
		nvram_set("1:ledbh9", "0x7");
#ifdef RTCONFIG_RGMII_BRCM5301X
		hw_name = "et1";

		nvram_set("rgmii_port", "5");
#else
		nvram_unset("rgmii_port");
#endif
#ifdef RTCONFIG_GMAC3
		if (nvram_match("gmac3_enable", "1"))
			hw_name = "et2";
#endif
		if(model == MODEL_RTAC5300)
			nvram_set("landevs", "vlan1 wl0 wl1 wl2");
		else
			nvram_set("landevs", "vlan1 wl0 wl1");

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
#ifdef RTCONFIG_GMAC3
			else
				nvram_set("wandevs", hw_name);
#endif
			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G)) {
				add_lan_phy("eth2");
				if(model == MODEL_RTAC5300)
					add_lan_phy("eth3");
			}

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid) {
								sprintf(wan_if, "vlan%d", wan_vid);
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
#ifdef RTCONFIG_GMAC3
			nvram_set("wandevs", hw_name);
#endif
			if(model == MODEL_RTAC5300)
				nvram_set("lan_ifnames", "vlan1 eth1 eth2 eth3");
			else
				nvram_set("lan_ifnames", "vlan1 eth1 eth2");

			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		if(model == MODEL_RTAC5300)
			nvram_set("lan_ifnames", "vlan1 eth1 eth2 eth3");
		else
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		if(model == MODEL_RTAC5300)
			nvram_set("wl_ifnames", "eth1 eth2 eth3");
		else
			nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
#if defined(RTCONFIG_AMAS)
		if(model == MODEL_RTAC5300) {
#ifdef RTCONFIG_DPSTA
			if (dpsta_mode()) {
				nvram_set("sta_phy_ifnames", "dpsta");
			}
#endif
			else if (dpsr_mode()) {
				nvram_set("sta_phy_ifnames", "eth1 eth2 eth3");
			}
			else
			{
				nvram_set("sta_phy_ifnames", "eth1 eth2 eth3");
			}
			if(nvram_get_int("re_mode") == 1) {
				nvram_set("lan_ifnames", "vlan1 vlan2 eth1 eth2 eth3");
				nvram_set("eth_ifnames", "vlan2");
				nvram_set("wait_band", "10");
				nvram_set("wait_wifi", "15");
			}
			nvram_set("sta_ifnames", "eth1 eth2 eth3");

		}
		else {
#ifdef RTCONFIG_DPSTA
			if (dpsta_mode()) {
				nvram_set("sta_phy_ifnames", "dpsta");
			}
#endif
			else if (dpsr_mode()) {
				nvram_set("sta_phy_ifnames", "eth1 eth2");
			}
			else
			{
				nvram_set("sta_phy_ifnames", "eth1 eth2");
			}
			if(nvram_get_int("re_mode") == 1) {
				nvram_set("lan_ifnames", "vlan1 vlan2 eth1 eth2");
				nvram_set("eth_ifnames", "vlan2");
				nvram_set("wait_band", "10");
				nvram_set("wait_wifi", "15");
			}
			nvram_set("sta_ifnames", "eth1 eth2");
		}
#endif

		/* gpio */
		/* HW reset, 2 | LOW */
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_led_gpio", 4);	// active high(on)
		nvram_set_int("led_wan_gpio", 5);
		/* MDC_4709 RGMII, 6 */
		/* MDIO_4709 RGMII, 7 */
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("reset_switch_gpio", 10);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		/* UART1_RX,  12 */
		/* UART1_TX,  13 */
		/* UART1_CTS, 14*/
		/* UART1_RTS, 15 */
		nvram_set_int("led_usb_gpio", 16|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb3_gpio", 17|GPIO_ACTIVE_LOW);
		//nvram_set_int("led_mmc_gpio",  17|GPIO_ACTIVE_LOW);	/* abort */
		nvram_set_int("led_wps_gpio", 19|GPIO_ACTIVE_LOW);
		if(model == MODEL_RTAC5300) {
			nvram_set_int("btn_wltog_gpio", 20|GPIO_ACTIVE_LOW);
			nvram_set_int("btn_wps_gpio", 18|GPIO_ACTIVE_LOW);
			nvram_set_int("fan_gpio", 15);
			nvram_set_int("rpm_fan_gpio", 14|GPIO_ACTIVE_LOW);
		} else {
			nvram_set_int("btn_wltog_gpio", 18|GPIO_ACTIVE_LOW);
			nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		}
		nvram_set_int("led_lan_gpio", 21|GPIO_ACTIVE_LOW);	/* FAN CTRL: reserved */
		/* PA 5V/3.3V switch, 22 */
		/* SDIO_EN_1P8, 23 | HIGH */

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		add_rc_support("app");
		nvram_set("ehci_irq", "111");
		nvram_set("xhci_irq", "112");
#ifdef RTCONFIG_MMC_LED
		nvram_set("mmc_irq", "177");
#endif
		add_rc_support("smart_connect");
		if (!strncmp(nvram_safe_get("territory_code"), "AU/05", 5)) {
			add_rc_support("nz_isp");
			nvram_set("wifi_psk", cfe_nvram_safe_get_raw("secret_code"));
		}
		break;

#endif

#ifdef RTAC87U
	case MODEL_RTAC87U:
		if(nvram_match("QTNTELNETSRV","")) nvram_set("QTNTELNETSRV", "0");

		nvram_set("vlan1hwname", "et1");
		nvram_set("landevs", "vlan1 wl0");
		nvram_set("rgmii_port", "5");
#ifdef RTCONFIG_QTN
		nvram_set("qtn_ntp_ready", "0");
#endif

#ifdef RTCONFIG_HW_DUALWAN
		if (is_router_mode()) {
			init_dualwan();
		}
		else{
			nvram_set("wandevs", "et1");
			nvram_set("lan_ifnames", "vlan1 eth1 wifi0");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 wifi0");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 wifi0");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		/* WAN2 MAC = 3rd 5G GuestNetwork */
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN){
			nvram_set("wl1_vifnames", "wl1.1 wl1.2");
			nvram_set("wl1.3_bss_enabled", "0");
		}
		nvram_set("wl1_country_code", nvram_safe_get("1:ccode"));
		nvram_set("wl1_country_rev", nvram_safe_get("1:regrev"));
		nvram_set("wl1_ifname", "wifi0");
		nvram_set("wl1_phytype", "v");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("reset_qtn_gpio", 8|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 15|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("btn_led_gpio", 4);	// active high
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		add_rc_support("app");
#ifdef RTCONFIG_WPS_DUALBAND
	nvram_set_int("wps_band_x", 0);
	nvram_set_int("wps_dualband", 1);
#else
	nvram_set_int("wps_dualband", 0);
#endif
#if 0
		if (nvram_match("x_Setting", "0") && nvram_get_int("ac87uopmode") == 5) {
			nvram_set("wl1_txbf", "0");
			nvram_set("wl1_itxbf", "0");
		}
#endif

		break;
#endif

#if defined(RTAC56S) || defined(RTAC56U)
	case MODEL_RTAC56S:
	case MODEL_RTAC56U:
		nvram_set("vlan1hwname", "et0");
		nvram_set("0:ledbh3", "0x87");	  /* since 163.42 */
		nvram_set("1:ledbh10", "0x87");
		nvram_set("landevs", "vlan1 wl0 wl1");

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid) {
								sprintf(wan_if, "vlan%d", wan_vid);
								add_wan_phy(wan_if);
							}
							else
								add_wan_phy(the_wan_phy());
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy(the_wan_phy());
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", the_wan_phy());
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("pwr_usb_gpio2", 10|GPIO_ACTIVE_LOW);	// Use at the first shipment of RT-AC56S,U.
#else
		nvram_set_int("pwr_usb_gpio", 9);
		nvram_set_int("pwr_usb_gpio2", 10);	// Use at the first shipment of RT-AC56S,U.
#endif
		nvram_set_int("led_usb_gpio", 14|GPIO_ACTIVE_LOW);	// change led gpio(usb2/usb3) to sync the outer case
		nvram_set_int("led_wan_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_all_gpio", 4|GPIO_ACTIVE_LOW);	// actually, this is high active, and will power off all led when active; to fake LOW_ACTIVE to sync boardapi
		nvram_set_int("led_5g_gpio", 6|GPIO_ACTIVE_LOW);	// 4352's fake led 5g
		nvram_set_int("led_usb3_gpio", 0|GPIO_ACTIVE_LOW);	// change led gpio(usb2/usb3) to sync the outer case
		nvram_set_int("btn_wps_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 11|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 7|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "1-1");
			nvram_set("ehci_ports", "2-1 2-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("movistarTriple");
		if(model == MODEL_RTAC56U){
			add_rc_support("app");
			add_rc_support("meoVoda");
		}
		break;
#endif

#if defined(RTN66U) || defined(RTAC66U)
	case MODEL_RTN66U:
		nvram_set_int("btn_radio_gpio", 13|GPIO_ACTIVE_LOW);	// ? should not used by Louis
		if (nvram_match("pci/1/1/ccode", "JP") && nvram_match("pci/1/1/regrev", "42") && !nvram_match("watchdog", "20000")) {
			_dprintf("force set watchdog as 20s\n");
			nvram_set("watchdog", "20000");
			nvram_commit();
			reboot(0);
		}

	case MODEL_RTAC66U:
		if (nvram_match("regulation_domain_5G", "EU") && !nvram_match("watchdog", "0")) {
			nvram_set("watchdog", "0");
			nvram_commit();
			reboot(0);
		}

#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");

		nvram_set_int("btn_rst_gpio", 9|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_gpio", 13|GPIO_ACTIVE_LOW);	// tmp use
		nvram_set_int("led_pwr_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 15|GPIO_ACTIVE_LOW);
		nvram_set_int("fan_gpio", 14|GPIO_ACTIVE_LOW);
		nvram_set_int("have_fan_gpio", 6|GPIO_ACTIVE_LOW);
		if (nvram_get("regulation_domain")) {
			nvram_set("ehci_ports", "1-1.2 1-1.1 1-1.4");
			nvram_set("ohci_ports", "2-1.2 2-1.1 2-1.4");
		}
		else {
			nvram_set("ehci_ports", "1-1.2 1-1.1");
			nvram_set("ohci_ports", "2-1.2 2-1.1");
		}
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		add_rc_support("app");
		break;
#endif

#ifdef RTAC53U
	case MODEL_RTAC53U:
#ifdef RTCONFIG_DUALWAN
		if (is_router_mode()) {
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("wandevs", "vlan2 vlan3");
			else
				nvram_set("wandevs", "et0");

			set_lan_phy("vlan1");

			if (!(get_wans_dualwan()&WANSCAP_2G))
				add_lan_phy("eth1");
			if (!(get_wans_dualwan()&WANSCAP_5G))
				add_lan_phy("eth2");

			if (nvram_get("wans_dualwan")) {
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						if (get_wans_dualwan()&WANSCAP_WAN)
							add_wan_phy("vlan3");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("eth1");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("eth2");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
							int wan_vid = nvram_get_int("switch_wan0tagid");
							if (wan_vid)
								sprintf(wan_if, "vlan%d", wan_vid);
							else
								sprintf(wan_if, "eth0");
							add_wan_phy(wan_if);
						}
						else if (get_wans_dualwan()&WANSCAP_LAN)
							add_wan_phy("vlan2");
						else
							add_wan_phy("eth0");
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else
				nvram_set("wan_ifnames", "eth0 usb");
		}
		else{
			nvram_set("wandevs", "et0");
			nvram_set("lan_ifnames", "vlan1 eth1 eth2");
			nvram_set("wan_ifnames", "eth0");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "vlan1 eth1 eth2");
		nvram_set("wan_ifnames", "eth0");
#endif
		nvram_set("wl_ifnames", "eth1 eth2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		nvram_set("sb/1/ledbh3", "0x87");
		nvram_set("0:ledbh9", "0x87");
		nvram_set_int("led_pwr_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 0|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 2|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 7|GPIO_ACTIVE_LOW);
		nvram_set_int("led_usb_gpio", 8|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio",22|GPIO_ACTIVE_LOW);
		nvram_set("ehci_ports", "1-1.4");
		nvram_set("ohci_ports", "2-1.4");
		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("2.4G 5G mssid usbX1 update");
		break;
#endif

#endif // CONFIG_BCMWL5
#ifdef RTCONFIG_REALTEK
	//case MODEL_RTRTL8198C:
	case MODEL_RPAC68U:
		//_dprintf("REALTEK nvram_set........\n");

		//USB Ports Init of RT-RTL8198C
		nvram_set("xhci_ports", "2-1"); // 2-1(at the first place) for USB-3.0 device on USB-3.0 HW Port
		nvram_set("ehci_ports", "1-1 3-1"); // 1-1(at the first place) for USB-2.0 device on USB-3.0 HW Port, 3-1(at the second place) for USB-2.0 device on USB-2.0 HW Port

		nvram_set("lan_ifname", "br0");
		if (is_router_mode())
			nvram_set("lan_ifnames", "eth0 wl0 wl1 eth2 eth3 eth4 wl0-vxd wl1-vxd wl0-wds0 wl1-wds0");
		else
			nvram_set("lan_ifnames", "eth0 wl0 wl1 eth1 eth2 eth3 eth4 wl0-vxd wl1-vxd wl0-wds0 wl1-wds0");
		nvram_set("wl0_ifname", "wl0");
		nvram_set("wl1_ifname", "wl1");
		if (is_router_mode())
			nvram_set("wan_ifnames", "eth1");
		else
			nvram_set("wan_ifnames", "");
		nvram_set("wl_ifnames","wl0 wl1");
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "wl1.1");
		nvram_set("wl0_vxdifnames", "wl0-vxd");
		nvram_set("wl1_vxdifnames", "wl1-vxd");
		if(check_rtk_hw_invaild())
		{
			//cprintf("%s:%d check_rtk_hw_invaild\n",__FUNCTION__,__LINE__);
			set_rtk_hw_default();
		}
		set_nvram_mac_addr_from_flash();
		set_nvram_pincode_from_flash();
		nvram_set_int("btn_rst_gpio", 21|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 20|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 2|GPIO_ACTIVE_LOW);
		//_dprintf("sw_mode=%d\n", sw_mode());
/*#ifdef RTCONFIG_WIRELESSREPEATER
		if (is_router_mode())
			nvram_set_int("sw_mode", SW_MODE_REPEATER);
#endif*/
		nvram_commit();
		add_rc_support("2.4G 5G usbX1 update");
		add_rc_support("rtkwifi");
		_dprintf("sw_mode=%d\n",sw_mode());
		break;

	case MODEL_RPAC53:

		nvram_set("lan_ifname", "br0");
		if (is_router_mode())
			nvram_set("lan_ifnames", "wl0 wl0-vxd wl1 wl1-vxd wl0-wds0 wl1-wds0");
		else if (access_point_mode())
			nvram_set("lan_ifnames", "eth1 wl0 wl0-vxd wl1 wl1-vxd wl0-wds0 wl1-wds0");
		else
			nvram_set("lan_ifnames", "eth1 wl0 wl0-vxd wl1 wl1-vxd");
		nvram_set("wl0_ifname", "wl0");
		nvram_set("wl1_ifname", "wl1");
		if (is_router_mode())
			nvram_set("wan_ifnames", "eth1");
		else
			nvram_set("wan_ifnames", "");
		nvram_set("wl_ifnames","wl0 wl1");
		nvram_set("wl0_vxdifnames", "wl0-vxd");
		nvram_set("wl1_vxdifnames", "wl1-vxd");
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "wl1.1");
		if(check_rtk_hw_invaild())
		{
			set_rtk_hw_default();
		}
		set_nvram_mac_addr_from_flash();
		set_nvram_pincode_from_flash();

		nvram_set_int("btn_rst_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_wps_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 12|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_red_gpio", 13|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_orange_gpio", 11|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_green_gpio", 10|GPIO_ACTIVE_LOW);
		nvram_set_int("led_2g_red_gpio", 5|GPIO_ACTIVE_LOW); // RTL8192ER
		nvram_set_int("led_5g_orange_gpio", 50|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_green_gpio", 51|GPIO_ACTIVE_LOW);
		nvram_set_int("led_5g_red_gpio", 24|GPIO_ACTIVE_LOW);
		nvram_set_int("led_lan_gpio", 14|GPIO_ACTIVE_LOW);

		nvram_commit();
		add_rc_support("2.4G 5G update");
		add_rc_support("rtkwifi");
		_dprintf("sw_mode=%d\n",sw_mode());
		break;

	case MODEL_RPAC55:

		nvram_set("lan_ifname", "br0");
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER)
			nvram_set("lan_ifnames", "wl0 wl0-vxd wl1 wl1-vxd wl0-wds0 wl1-wds0");
		else if (access_point_mode())
			nvram_set("lan_ifnames", "wl0 wl0-vxd wl1 wl1-vxd wl0-wds0 wl1-wds0 eth0");
		else
			nvram_set("lan_ifnames", "wl0 wl0-vxd wl1 wl1-vxd eth0");
		nvram_set("wl0_ifname", "wl0");
		nvram_set("wl1_ifname", "wl1");
		if (nvram_get_int("sw_mode") == SW_MODE_ROUTER)
			nvram_set("wan_ifnames", "eth0");
		else
			nvram_set("wan_ifnames", "");
		nvram_set("wl_ifnames","wl0 wl1");
		nvram_set("wl0_vxdifnames", "wl0-vxd");
		nvram_set("wl1_vxdifnames", "wl1-vxd");
		nvram_set("wl0_vifnames", "wl0.1");
		nvram_set("wl1_vifnames", "wl1.1");
		if(check_rtk_hw_invaild())
		{
			set_rtk_hw_default();
		}
		set_nvram_mac_addr_from_flash();
		set_nvram_pincode_from_flash();

		nvram_set_int("btn_rst_gpio", 54);
		nvram_set_int("btn_wps_gpio", 56|GPIO_ACTIVE_LOW);
		nvram_set_int("led_pwr_gpio", 57);
		nvram_set_int("led_pwr_red_gpio", 24);
		nvram_set_int("led_wifi_gpio", 29);
		nvram_set_int("led_sig1_gpio", 31);
		nvram_set_int("led_sig2_gpio", 32);

		nvram_commit();
		add_rc_support("2.4G 5G update");
		add_rc_support("rtkwifi");
		_dprintf("sw_mode=%d\n",nvram_get_int("sw_mode"));
		break;
#endif /* RTCONFIG_REALTEK */

#ifdef RTCONFIG_ALPINE
	case MODEL_GTAC9600:
		if(nvram_match("QTNTELNETSRV","")) nvram_set("QTNTELNETSRV", "0");

		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "eth3 eth0 eth2 host0");
		nvram_set("qtn_ntp_ready", "0");

#if defined(RTCONFIG_DUALWAN)
		if(is_router_mode()){
			set_lan_phy("eth3");

			// this platform uses host0 to communicate the WiFi interfaces
			add_lan_phy("host0");

			int LAN10G = 3; // 0: no, 1: 10G ETH, 2: 10G SFP+, 3: all.
			if(nvram_get("wans_dualwan")){
				set_wan_phy("");
				for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
					if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_2G)
						add_wan_phy("host0");
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_5G)
						add_wan_phy("host0");
					else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN){
						add_wan_phy("eth1");
					}
					else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN){
						// TODO: can use eth0 or eth2 as the WAN2.
						if(nvram_get_int("wans_lanport") == 2){
							LAN10G = 1;
							add_wan_phy("eth2");
						}
						else{ // wans_lanport == 1
							LAN10G = 2;
							add_wan_phy("eth0");
						}
					}
					else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB)
						add_wan_phy("usb");
#ifdef RTCONFIG_USB_MULTIMODEM
					else if(get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_USB2)
						add_wan_phy("usb2");
#endif
				}
			}
			else{
				nvram_set("wan_ifnames", "eth1");
			}

			if(LAN10G == 3){
				add_lan_phy("eth0");
				add_lan_phy("eth2");
			}
			else if(LAN10G == 2){
				add_lan_phy("eth2");
			}else if(LAN10G == 1)
				add_lan_phy("eth0");
		}
		else{
			nvram_set("lan_ifnames", "eth3 eth0 eth1 eth2 host0");
			nvram_set("wan_ifnames", "");
			nvram_unset("wan1_ifname");
		}
#else
		nvram_set("lan_ifnames", "eth3 eth2 host0");
		nvram_set("wan_ifnames", "eth1");
#endif

		nvram_set("wl_ifnames", "wifi2_0 wifi0_0");
		nvram_set("wl0_vifnames", "wifi2_1 wifi2_2 wifi2_3");
		nvram_set("wl1_vifnames", "wifi0_1 wifi0_2 wifi0_3");
		/* WAN2 MAC = 3rd 5G GuestNetwork */
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN){
			nvram_set("wl1_vifnames", "wl1.1 wl1.2");
			nvram_set("wl1.3_bss_enabled", "0");
		}
		// nvram_set("wl1_country_code", nvram_safe_get("1:ccode"));
		// nvram_set("wl1_country_rev", nvram_safe_get("1:regrev"));
		nvram_set("wl0_ifname", "wifi2_0");
		nvram_set("wl1_ifname", "wifi0_0");
		nvram_set("wl1_phytype", "v");

#ifdef RTCONFIG_USBRESET
		nvram_set_int("pwr_usb_gpio", 9|GPIO_ACTIVE_LOW);
#else
		nvram_set_int("pwr_usb_gpio", 9);
#endif
#if 0
		nvram_set_int("led_pwr_gpio", 3|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 5|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 1|GPIO_ACTIVE_LOW);
#endif
		nvram_set_int("btn_wps_gpio", 38|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 39|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_radio_gpio", 3|GPIO_ACTIVE_LOW);
#ifdef RTCONFIG_WIFI_TOG_BTN
		nvram_set_int("btn_wltog_gpio", 3|GPIO_ACTIVE_LOW);
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("btn_led_gpio", 22);
#endif

#ifdef RTCONFIG_XHCIMODE
		nvram_set("xhci_ports", "1-1 1-2");
		nvram_set("ehci_ports", "2-1 2-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
#if 1 // temp don't support to disable the USB 3.0.
		nvram_set("xhci_ports", "2-1 2-2");
		nvram_set("ehci_ports", "1-1 1-2");
		nvram_set("ohci_ports", "3-1 3-2");
#else
		if (nvram_get_int("usb_usb3") == 1) {
			nvram_set("xhci_ports", "2-1 2-2");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "3-1 3-2");
		}
		else{
			nvram_unset("xhci_ports");
			nvram_set("ehci_ports", "1-1 1-2");
			nvram_set("ohci_ports", "2-1 2-2");
		}
#endif
#endif

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G usbX2");
#ifdef RTCONFIG_MERLINUPDATE
		add_rc_support("update");
#else
		add_rc_support("noupdate");
#endif
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("app");
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
#ifdef RTCONFIG_WPS_DUALBAND
	nvram_set_int("wps_band_x", 0);
	nvram_set_int("wps_dualband", 1);
#else
	nvram_set_int("wps_dualband", 0);
#endif
#if 0
		if (nvram_match("x_Setting", "0") && nvram_get_int("ac87uopmode") == 5) {
			nvram_set("wl1_txbf", "0");
			nvram_set("wl1_itxbf", "0");
		}
#endif

		break;
#endif

#ifdef BLUECAVE
	case MODEL_BLUECAVE:
		_dprintf("BLUECAVE: todo, init_nvram()\n");
		nvram_set("wave_action", "0");
		nvram_set("lan_ifname", "br0");
		nvram_set("landevs", "eth0_1 eth0_2 eth0_3 eth0_4");
		nvram_set("ct_max", "5000");

#if 0
		set_basic_ifname_vars("eth0", "vlan1", "eth1", "eth2", "usb", NULL, "vlan2", "vlan3", 0);
#else
		if (sw_mode()==SW_MODE_AP) {
			nvram_set("lan_ifnames", "eth0_1 eth0_2 eth0_3 eth0_4 wlan0 wlan2 eth1");
			nvram_set("wan_ifnames", "");
		}
		else {
			nvram_set("lan_ifnames", "eth0_1 eth0_2 eth0_3 eth0_4 wlan0 wlan2");
			nvram_set("wan_ifnames", "eth1");
		}
		nvram_set("wl_ifnames", "wlan0 wlan2");
		nvram_set("wl0_vifnames", "wl0.1 wl0.2 wl0.3");
		nvram_set("wl1_vifnames", "wl1.1 wl1.2 wl1.3");
		/* WAN2 MAC = 3rd 5G GuestNetwork */
		if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN){
			nvram_set("wl1_vifnames", "wl1.1 wl1.2");
			nvram_set("wl1.3_bss_enabled", "0");
		}
		// nvram_set("wl1_country_code", nvram_safe_get("1:ccode"));
		// nvram_set("wl1_country_rev", nvram_safe_get("1:regrev"));
		nvram_set("wl1_phytype", "v");

		char prefix[] = "wlXXXXXXXXXXXXX_", tmp[100];
		char prefix2[] = "wlXXXXXXXXXXXXX_", tmp2[100];
		int i, j, wlc_band = nvram_get_int("wlc_band");

		// only for the all SKIP_ABSENT_BAND().
		for(i = 0; i < MAX_NR_WL_IF; ++i){
			snprintf(prefix, sizeof(prefix), "wl%d_", i);

#ifdef RTCONFIG_WIRELESSREPEATER
#ifdef RTCONFIG_PROXYSTA
			if(mediabridge_mode())
				nvram_set(strcat_r(prefix, "ifname", tmp), "");
			else
#endif
			if(repeater_mode() && i == wlc_band)
				nvram_set(strcat_r(prefix, "ifname", tmp), get_staifname(i));
			else
#endif
				nvram_set(strcat_r(prefix, "ifname", tmp), get_wififname(i));

			//for(j = 0; j < MAX_NO_MSSID; ++j){
			for(j = 0; j < 1; ++j){
				snprintf(prefix2, sizeof(prefix2), "wl%d.%d_", i, j+1);

#ifdef RTCONFIG_WIRELESSREPEATER
				if(client_mode() && i == wlc_band && j == 0)
					nvram_set(strcat_r(prefix2, "ifname", tmp2), get_wififname(i));
#endif

				if(!nvram_get(strcat_r(prefix2, "nband", tmp2)) || strlen(nvram_safe_get(tmp2)) <= 0)
					nvram_set(tmp2, nvram_safe_get(strcat_r(prefix, "nband", tmp)));
			}
		}
#endif

#if 0
		nvram_set_int("led_pwr_gpio", 1|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wan_gpio", 4|GPIO_ACTIVE_LOW);
		nvram_set_int("led_wps_gpio", 6|GPIO_ACTIVE_LOW);
#endif

		nvram_set_int("led_ctl_sig1_gpio", 42);	// Level 1
		nvram_set_int("led_ctl_sig2_gpio", 8);	// Level 2
		nvram_set_int("led_ctl_sig3_gpio", 1);	// Level 3

		nvram_set_int("led_idr_sig1_gpio", 4);	// RED
		nvram_set_int("led_idr_sig2_gpio", 6);	// BLUE

		nvram_set_int("btn_wps_gpio", 30|GPIO_ACTIVE_LOW);
		nvram_set_int("btn_rst_gpio", 0|GPIO_ACTIVE_LOW);

		if(nvram_get_int("usb_usb3") == 1)
			nvram_set("xhci_ports", "2-1");
		else
			nvram_unset("xhci_ports");
		nvram_set("ehci_ports", "1-1");
		nvram_set("ohci_ports", "1-1");

		if (!nvram_get("ct_max"))
			nvram_set("ct_max", "300000");
		add_rc_support("mssid 2.4G 5G update usbX1");
		add_rc_support("switchctrl"); // broadcom: for jumbo frame only
		add_rc_support("manual_stb");
		add_rc_support("pwrctrl");
		add_rc_support("WIFI_LOGO");
		add_rc_support("nandflash");
		add_rc_support("meoVoda");
		add_rc_support("movistarTriple");
		add_rc_support("lantiq");
		add_rc_support("app");
#if defined(LANTIQ_BSD)
		add_rc_support("bandstr");
#endif
#ifdef RTCONFIG_LED_BTN
		nvram_set_int("AllLED", 1);
#endif
#ifdef RTCONFIG_WPS_DUALBAND
	nvram_set_int("wps_band_x", 0);
	nvram_set_int("wps_dualband", 1);
#else
	nvram_set_int("wps_dualband", 0);
#endif
#if 0
		if (nvram_match("x_Setting", "0") && nvram_get_int("ac87uopmode") == 5) {
			nvram_set("wl1_txbf", "0");
			nvram_set("wl1_itxbf", "0");
		}
#endif

		break;
#endif

	default:
		_dprintf("############################ unknown model #################################\n");
		break;
	}

#ifdef RTCONFIG_WIFI_SON
	if (nvram_match("x_Setting", "1") && sw_mode() == SW_MODE_ROUTER)
	{
		nvram_set("cfg_master", "1");
	}
#endif

#if !defined(RTCONFIG_DUALWAN) && \
    !(defined(MAPAC1300) || defined(MAPAC2200) || defined(VZWAC1300)) && \
    (defined(CONFIG_BCMWL5) || defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
	/* Broadcom, QCA, MTK (use MT7620/MT7621/MT7628 ESW) platform */
	if (nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none")) {
		char wan_if[10];
		int wan_vid = nvram_get_int("switch_wan0tagid");
		if (wan_vid) {
			sprintf(wan_if, "vlan%d", wan_vid);
			nvram_set("wan_ifnames", wan_if);
		}
#if !defined(RTCONFIG_RALINK)
		else
			nvram_set("wan_ifnames", "eth0");
#endif
	}
#endif

#ifdef RTCONFIG_MULTICAST_IPTV
	if (nvram_get_int("switch_stb_x") > 6) {
#if !defined(HND_ROUTER) && !defined(BLUECAVE)
		if(nvram_match("switch_wantag", "singtel")) {
			nvram_set("iptv_wan_ifnames", "vlan30 vlan40");
		} else if(nvram_match("switch_wantag", "maxis_fiber_sp_iptv")) {
			nvram_set("iptv_wan_ifnames", "vlan15");
			nvram_set("iptv_ifname", "vlan15");
		} else if(nvram_match("switch_wantag", "maxis_fiber_iptv")) {
			nvram_set("iptv_wan_ifnames", "vlan823");
			nvram_set("iptv_ifname", "vlan823");
		} else if(nvram_match("switch_wantag", "movistar")) {
			nvram_set("iptv_wan_ifnames", "vlan2 vlan3");
			nvram_set("iptv_ifname", "vlan2");
		}
#elif defined(HND_ROUTER)
		if(nvram_match("switch_wantag", "movistar")) {
			nvram_set("iptv_wan_ifnames", "eth0.v1 eth0.v2");
			nvram_set("iptv_ifname", "eth0.v1");
		}
#elif defined(BLUECAVE)
		if(nvram_match("switch_wantag", "movistar")) {
			nvram_set("iptv_wan_ifnames", "eth1.2 eth1.3");
			nvram_set("iptv_ifname", "eth1.2");
		}
#endif
	}
#endif /* MULTICAST_IPTV */

#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	char word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";

	unit = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		nvram_set_int(strcat_r(prefix, "unit", tmp), unit);
		nvram_set(strcat_r(prefix, "ifname", tmp), word);

		unit++;
	}
#endif

#ifdef RTCONFIG_REBOOT_SCHEDULE
	add_rc_support("reboot_schedule");
	// tmp to add default nvram
	if(nvram_match("reboot_schedule_enable", ""))
		nvram_set("reboot_schedule_enable", "0");
	if(nvram_match("reboot_schedule", ""))
		nvram_set("reboot_schedule", "00000000000");
#endif

#ifdef RTCONFIG_IPV6
	add_rc_support("ipv6");
#ifdef RTCONFIG_6RELAYD
	add_rc_support("ipv6pt");
#endif
#endif

#ifdef RTCONFIG_FANCTRL
	if ( button_pressed(BTN_HAVE_FAN) )
		add_rc_support("fanctrl");
#endif

#ifdef RTCONFIG_PARENTALCTRL
	add_rc_support("PARENTAL2");
#endif

#ifdef RTCONFIG_TCODE
	config_tcode(0);
	/* Last action for del rc_support */
	config_tcode(2);
#endif

#ifdef RTCONFIG_YANDEXDNS
#ifdef RTCONFIG_TCODE
	if (!nvram_contains_word("rc_support", "yadns") &&
	    !nvram_contains_word("rc_support", "yadns_hideqis"))
#endif
	{
		/* 0: disable, 1: full, -1: partial without qis */
		int yadns_support;

		switch (model) {
		case MODEL_RTN11P:
		case MODEL_RTN300:
			/* Enabled for CE & RU area. (but IN could also be included) */
			yadns_support = nvram_match("reg_spec", "CE") && nvram_match("wl_reg_2g", "2G_CH13");
			break;
		case MODEL_RTN12VP:
		case MODEL_RTN10P:
		case MODEL_RTN10PV2:
			/* Enabled unconditionally */
			yadns_support = 1;
			break;
		default:
#ifdef RTCONFIG_TCODE
		case MODEL_RTAC82U:
		case MODEL_RTAC58U:
		case MODEL_RT4GAC53U:
		case MODEL_MAPAC1300:
		case MODEL_MAPAC2200:
		case MODEL_VZWAC1300:
		case MODEL_MAPAC3000:
			/* Only enabled by territory_code */
			yadns_support = 0;
#else
			/* Enabled by default since YANDEXDNS=y */
			yadns_support = 1;
#endif
			break;
		}
		if (yadns_support == 0)
			nvram_set_int("yadns_enable_x", 0);
		else if (yadns_support < 0)
			add_rc_support("yadns_hideqis");
		else
			add_rc_support("yadns");
	}
#endif

#ifdef RTCONFIG_DNSFILTER
	add_rc_support("dnsfilter");
#endif
#ifdef RTCONFIG_DUALWAN // RTCONFIG_DUALWAN
	add_rc_support("dualwan");

#ifdef RTCONFIG_DSL
	set_wanscap_support("dsl");
#elif defined(RTCONFIG_NO_WANPORT)
	set_wanscap_support("");
#else
	set_wanscap_support("wan");
#endif
#ifdef RTCONFIG_WANPORT2
	add_wanscap_support("wan2");
#endif
#ifdef RTCONFIG_USB_MODEM
#ifdef RTAC68U
	if (hw_usb_cap())
#endif
	add_wanscap_support("usb");
#endif
	add_wanscap_support("lan");
#endif // RTCONFIG_DUALWAN

#ifdef RTCONFIG_MULTIWAN_CFG
	add_rc_support("mtwancfg");
#endif

#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	add_rc_support("pptpd");
#endif

#ifdef RTCONFIG_OPENVPN
#ifdef RTAC68U
	if (!is_n66u_v2())
#endif
	add_rc_support("openvpnd");
	//nvram_set("vpnc_proto", "disable");
#endif

#ifdef RTCONFIG_UTF8_SSID
	add_rc_support("utf8_ssid");
#endif

#ifdef RTCONFIG_USB
#ifdef RTAC68U
	if (!hw_usb_cap())
		goto NO_USB_CAP;
#endif
#ifdef RTCONFIG_USB_PRINTER
	add_rc_support("printer");
#endif

#ifdef RTCONFIG_USB_MODEM
#if !defined(RTAC1200)
	// TODO: hide USB Modem UI in 3.0.0.1
	if (strcmp(nvram_safe_get("firmver"), "3.0.0.1")!=0)
		add_rc_support("modem");
#endif

#ifdef RTCONFIG_USB_BECEEM
	add_rc_support("wimax");
#endif

#ifdef RTCONFIG_USB_SMS_MODEM
	add_rc_support("usbsms");
#endif

#ifdef RTCONFIG_USB_MULTIMODEM
	add_rc_support("multimodem");
#endif

#ifdef RTCONFIG_MODEM_BRIDGE
	add_rc_support("modembridge");
#endif
#endif

#ifdef RTCONFIG_WEBDAV
	add_rc_support("webdav");
#endif

#ifdef RTCONFIG_CLOUDSYNC
	add_rc_support("rrsut");	//syn server
	add_rc_support("cloudsync");

	char ss_support_value[1024]="\0";

#ifdef RTCONFIG_CLOUDSYNC
	strcat(ss_support_value, "asuswebstorage ");
#endif

#ifdef RTCONFIG_SWEBDAVCLIENT
	strcat(ss_support_value, "webdav ");
#endif

#ifdef RTCONFIG_DROPBOXCLIENT
	strcat(ss_support_value, "dropbox ");
#endif

#ifdef RTCONFIG_FTPCLIENT
	strcat(ss_support_value, "ftp ");
#endif

#ifdef RTCONFIG_SAMBACLIENT
	strcat(ss_support_value, "samba ");
#endif

#ifdef RTCONFIG_USBCLIENT
	strcat(ss_support_value, "usb ");
#endif

#ifdef RTCONFIG_FLICKRCLIENT
	strcat(ss_support_value, "flickr ");
#endif

	nvram_set("ss_support", ss_support_value);
#endif

#ifdef RTCONFIG_MEDIA_SERVER
	add_rc_support("media");
#endif

#ifdef RTCONFIG_APP_PREINSTALLED
	add_rc_support("appbase");
#endif // RTCONFIG_APP_PREINSTALLED

#ifdef RTCONFIG_APP_NETINSTALLED
	add_rc_support("appnet");

// DSL-N55U will follow N56U to have DM
//#ifdef RTCONFIG_RALINK
//#ifdef RTCONFIG_DSL
//	if (model==MODEL_DSLN55U)
//		add_rc_support("nodm");
//#endif
//#endif
#endif // RTCONFIG_APP_NETINSTALLED

#ifdef RTCONFIG_TIMEMACHINE
	add_rc_support("timemachine");
#endif

#ifdef RTCONFIG_WIGIG
	add_rc_support("wigig");
#endif

#ifdef RTCONFIG_HD_SPINDOWN
	add_rc_support("hdspindown");
#endif

#ifdef RTCONFIG_DISK_MONITOR
	add_rc_support("diskutility");
#endif

#ifdef RTCONFIG_NFS
	add_rc_support("nfsd");
#endif

#ifdef RTCONFIG_IGD2
	add_rc_support("igd2");
#endif
#ifdef RTCONFIG_DNSSEC
        add_rc_support("dnssec");
#endif

#ifdef RTCONFIG_OPENPLUS_TFAT
#if defined(RTAC58U)
	if(!strncmp(nvram_safe_get("odmpid"), "RT-ACRH", 7))
		nvram_set("usb_fatfs_mod", "open");
	else
		nvram_set("usb_fatfs_mod", "tuxera");
#else
	if(fs_coexist() == 1)
		nvram_set("usb_fatfs_mod", "open");
	else
		nvram_set("usb_fatfs_mod", "tuxera");
#endif
#elif defined(RTCONFIG_TFAT)
	nvram_set("usb_fatfs_mod", "tuxera");
#else
	nvram_set("usb_fatfs_mod", "open");
#endif

#ifdef RTCONFIG_NTFS
	nvram_set("usb_ntfs_mod", "");
#if defined(RTCONFIG_OPENPLUSPARAGON_NTFS) || defined(RTCONFIG_OPENPLUSTUXERA_NTFS)
#if defined(RTAC58U)
	if(!strncmp(nvram_safe_get("odmpid"), "RT-ACRH", 7))
		nvram_set("usb_ntfs_mod", "open");
	else{
		add_rc_support("sparse");
		nvram_set("usb_ntfs_mod", "tuxera");
	}
#else
	if(fs_coexist() == 1)
		nvram_set("usb_ntfs_mod", "open");
	else
		nvram_set("usb_ntfs_mod", "tuxera");
#endif
#else
#ifdef RTCONFIG_OPEN_NTFS3G
	nvram_set("usb_ntfs_mod", "open");
#elif defined(RTCONFIG_PARAGON_NTFS)
	add_rc_support("sparse");
	nvram_set("usb_ntfs_mod", "paragon");
#elif defined(RTCONFIG_TUXERA_NTFS)
	nvram_set("usb_ntfs_mod", "tuxera");
#endif
#endif
#endif // RTCONFIG_NTFS

#ifdef RTCONFIG_HFS
	nvram_set("usb_hfs_mod", "");
#if defined(RTCONFIG_OPENPLUSPARAGON_HFS) || defined(RTCONFIG_OPENPLUSTUXERA_HFS)
#if defined(RTAC58U)
	if(!strncmp(nvram_safe_get("odmpid"), "RT-ACRH", 7))
		nvram_set("usb_hfs_mod", "open");
	else
		nvram_set("usb_hfs_mod", "tuxera");
#else
	if(fs_coexist() == 1)
		nvram_set("usb_hfs_mod", "open");
	else
		nvram_set("usb_hfs_mod", "tuxera");
#endif
#else
#ifdef RTCONFIG_KERNEL_HFSPLUS
	nvram_set("usb_hfs_mod", "open");
#elif defined(RTCONFIG_PARAGON_HFS)
	nvram_set("usb_hfs_mod", "paragon");
#elif defined(RTCONFIG_TUXERA_HFS)
	nvram_set("usb_hfs_mod", "tuxera");
#endif
#endif
#endif // RTCONFIG_HFS

	if(!nvram_get_int("enable_samba_manual")){
#ifdef RTCONFIG_TUXERA_SMBD
		nvram_set("enable_samba_tuxera", "1");
#else
		nvram_set("enable_samba_tuxera", "0");
#endif
	}
#ifdef RTAC68U
NO_USB_CAP:
#endif
#endif // RTCONFIG_USB

#ifdef RTCONFIG_PUSH_EMAIL
//	add_rc_support("feedback");
	add_rc_support("email");
#ifdef RTCONFIG_DBLOG
	add_rc_support("dblog");
#endif /* RTCONFIG_DBLOG */
#endif

#ifdef RTCONFIG_ISP_METER
	add_rc_support("ispmeter");
#endif

#ifdef RTCONFIG_FINDASUS
	add_rc_support("findasus");
#endif

#ifdef RTCONFIG_AIR_TIME_FAIRNESS
	add_rc_support("atf");
#endif

#ifdef RTCONFIG_POWER_SAVE
	add_rc_support("pwrsave");
#endif

#ifdef RTCONFIG_HAS_5G_2
	add_rc_support("5G-2");
#else
	if (nvram_get("wl2_nband") != NULL)
		add_rc_support("5G-2");
#endif

#if defined(RTCONFIG_BWDPI)
#ifdef RTAC68U
	if (!is_n66u_v2())
#endif
	add_rc_support("bwdpi");

	/* modify logic for AiProtection switch */
	// DON'T USE the logic of nvram_match, it's the wrong logic in this case!!
	// 1. when wrs_protect_enable == "", set to 1
	// 2. when wrs_protect_enable == 0, not to change this value
	if (!strcmp(nvram_safe_get("wrs_protect_enable"), ""))
	{
		if (nvram_get_int("wrs_mals_enable") || nvram_get_int("wrs_cc_enable") ||  nvram_get_int("wrs_vp_enable"))
			nvram_set("wrs_protect_enable", "1");
		else
			nvram_set("wrs_protect_enable", "0");
	}
#endif

#ifdef RTCONFIG_TRAFFIC_LIMITER
	add_rc_support("traffic_limiter");
#endif

#ifdef RTCONFIG_ADBLOCK
	add_rc_support("adBlock");
#endif

#ifdef RTCONFIG_SNMPD
	add_rc_support("snmp");
#endif

#ifdef RTCONFIG_TOR
	add_rc_support("tor");
#endif

#ifdef RTCONFIG_HTTPS
	add_rc_support("HTTPS");
#ifdef RTCONFIG_LETSENCRYPT
	add_rc_support("letsencrypt");
#endif
#endif

#ifdef RTCONFIG_SSH
	add_rc_support("ssh");
#endif

#ifdef RTCONFIG_VPNC
	add_rc_support("vpnc");
#endif

#ifdef RTCONFIG_VPN_FUSION
	add_rc_support("vpn_fusion");
#endif

#ifdef RTCONFIG_WIRELESSREPEATER
#ifndef RTCONFIG_DISABLE_REPEATER_UI
	add_rc_support("repeater");
#endif
#endif
#ifdef RTCONFIG_PROXYSTA
#ifndef RTCONFIG_DISABLE_PROXYSTA_UI
	add_rc_support("psta");
#endif
#endif
#if defined(RTCONFIG_VHT80_80)
	add_rc_support("vht80_80");
#endif
#if defined(RTCONFIG_VHT160)
	// add_rc_support("vht160");
#endif
#ifdef RTCONFIG_BCMWL6
	add_rc_support("wl6");
#endif
#if defined(RTCONFIG_BCMWL6) || defined(RTCONFIG_QCA)
#ifdef RTCONFIG_OPTIMIZE_XBOX
	add_rc_support("optimize_xbox");
#endif
#endif
#ifdef RTCONFIG_SFP
	add_rc_support("sfp");
#endif
#ifdef RTCONFIG_4M_SFP
	add_rc_support("sfp4m");
#endif
#ifdef RTCONFIG_8M_SFP
	add_rc_support("sfp8m");
#endif
	//add_rc_support("ruisp");
#ifdef RTCONFIG_WIFI_TOG_BTN
	add_rc_support("wifi_tog_btn");
#endif

#ifdef RTCONFIG_WPSMULTIBAND
	add_rc_support("wps_multiband");
#endif

#if (defined(RTCONFIG_USER_LOW_RSSI) || defined(RTCONFIG_NEW_USER_LOW_RSSI))
	add_rc_support("user_low_rssi");
#endif

#ifdef RTCONFIG_BCMFA
	add_rc_support("bcmfa");
#endif

#ifdef RTCONFIG_ROG
	add_rc_support("rog");
#endif

#ifdef RTCONFIG_TCODE
	add_rc_support("tcode");
#endif

#ifdef RTCONFIG_JFFS2USERICON
	add_rc_support("usericon");
#endif

#ifdef RTCONFIG_TR069
	add_rc_support("tr069");

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)
	add_rc_support("jffs2");
#endif
#endif

#ifdef RTCONFIG_WPS_ALLLED_BTN
	add_rc_support("cfg_wps_btn");
#endif

#ifdef RTCONFIG_STAINFO
	add_rc_support("stainfo");
#endif

#ifdef RTCONFIG_CLOUDCHECK
	add_rc_support("cloudcheck");
#endif

#ifdef RTCONFIG_GETREALIP
	add_rc_support("realip");
#endif
#ifdef RTCONFIG_LACP
	add_rc_support("lacp");
#endif
#ifdef RTCONFIG_KEY_GUARD
	add_rc_support("keyGuard");
#endif
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
#if !defined(RTCONFIG_SSID_AMAPS) /* AMAPS default is open system */
	if(!nvram_match("wifi_psk", ""))
		add_rc_support("defpsk");
#endif /* RTCONFIG_SSID_AMAPS */
#endif

#if defined(RTAC1200)
	add_rc_support("noaidisk nodm noftp");
#endif

#ifdef RTCONFIG_WTFAST
	add_rc_support("wtfast");
#endif
#ifdef RTCONFIG_IFTTT
	add_rc_support("ifttt");
#endif
#ifdef RTCONFIG_ALEXA
	add_rc_support("alexa");
#endif
#ifdef RTCONFIG_WIFILOGO
	add_rc_support("wifilogo");
#endif
#ifdef RTCONFIG_IPSEC
#ifdef RTCONFIG_IPSEC_SERVER
	add_rc_support("ipsec_srv");
#endif
#ifdef RTCONFIG_IPSEC_CLIENT
	add_rc_support("ipsec_cli");
#endif
#endif
#if defined(RTCONFIG_CAPTIVE_PORTAL) && !defined(RTCONFIG_CP_FREEWIFI) && !defined(RTCONFIG_CP_ADVANCED)
	add_rc_support("cp_freewifi cp_advanced captivePortal");
#elif defined(RTCONFIG_CP_FREEWIFI) && defined(RTCONFIG_CAPTIVE_PORTAL)
	add_rc_support("cp_freewifi captivePortal");
#elif defined(RTCONFIG_CP_ADVANCED) && defined(RTCONFIG_CAPTIVE_PORTAL)
	add_rc_support("cp_advanced captivePortal");
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	add_rc_support("gobi");
#endif

#ifdef RTCONFIG_PORT_BASED_VLAN
	add_rc_support("vlan");
#endif
#ifdef RTCONFIG_TAGGED_BASED_VLAN
	add_rc_support("tagged_based_vlan");
#endif
#ifdef RTCONFIG_MUMIMO
	add_rc_support("mumimo");
#endif
#ifdef RTCONFIG_MUMIMO_5G
	add_rc_support("mumimo_5g");
#endif
#ifdef RTCONFIG_MUMIMO_2G
	add_rc_support("mumimo_2g");
#endif
#ifdef RTCONFIG_QAM256_2G
	add_rc_support("qam256_2g");
#endif
#ifdef RTCONFIG_NOIPTV
	add_rc_support("noiptv");
#endif
#ifdef RTCONFIG_NOTIFICATION_CENTER
	//add_rc_support("nt_center");	//UI hide
#endif
#ifdef RTCONFIG_NETOOL
	add_rc_support("netool");
#endif
#ifdef RTCONFIG_DISABLE_NETWORKMAP
	add_rc_support("disable_nwmd");
#endif
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	add_rc_support("permission_management");
#endif

#if defined(RTCONFIG_CAPTIVE_PORTAL) && !defined(RTCONFIG_CP_FREEWIFI) && !defined(RTCONFIG_CP_ADVANCED)
	add_rc_support("cp_freewifi cp_advanced captivePortal");
#elif defined(RTCONFIG_CP_FREEWIFI) && defined(RTCONFIG_CAPTIVE_PORTAL)
	add_rc_support("cp_freewifi captivePortal");
#elif defined(RTCONFIG_CP_ADVANCED) && defined(RTCONFIG_CAPTIVE_PORTAL)
	add_rc_support("cp_advanced captivePortal");
#endif

#ifdef RTCONFIG_FBWIFI
	add_rc_support("fbwifi");
#endif
#ifdef RTCONFIG_CFGSYNC
	add_rc_support("cfg_sync");
#endif

#if (defined(RTAC3200) || defined(HND_ROUTER))
	add_rc_support("no_finiwl");
#endif

#if defined(RTCONFIG_VPN_FUSION)
	add_rc_support("vpn_fusion");
#endif

#ifdef RTCONFIG_FORCE_AUTO_UPGRADE
	add_rc_support("fupgrade");
#endif

#ifdef RTCONFIG_AMAS
	add_rc_support("amas");
	if (nvram_get_int("amas_bdl"))
	add_rc_support("amas_bdl");
#endif

#ifdef RTCONFIG_WIFI_PROXY
	add_rc_support("wifiproxy");
#endif

#ifdef CONFIG_BCMWL5
	add_rc_support("bcmwifi");
#endif

#ifdef RTCONFIG_DFS_US
	add_rc_support("dfs");
#endif

#ifdef RTCONFIG_LYRA_HIDE
	add_rc_support("lyra_hide");
#endif

#ifdef RTCONFIG_PORT2_DEVICE
	add_rc_support("port2_device");
#endif

	return 0;
}

int init_nvram2(void)
{
	char *macp = NULL;
	unsigned char mac_binary[6];
	char friendly_name[32];
	int i;
	char varname[20];

	macp = get_2g_hwaddr();
	ether_atoe(macp, mac_binary);

#ifdef RTAC85U
	int model = get_model();
	switch(model)
	{
	case MODEL_RTAC85U:
#ifdef RTCONFIG_USB_SWAP
		if(nvram_get_int("apps_swap_threshold") == 0) {
			nvram_set("apps_swap_enable", "1");
			nvram_set("apps_swap_threshold", "75000");
			nvram_set("apps_swap_size", "62000");
		}
#endif
		break;
	default:
		_dprintf("############################ unknown model #################################\n");
		break;
	}
#endif

#ifdef RTAC1200GP
	sprintf(friendly_name, "%s-%02X%02X", "RT-AC1200G", mac_binary[4], mac_binary[5]);
#else
	sprintf(friendly_name, "%s-%02X%02X", get_productid(), mac_binary[4], mac_binary[5]);
#endif

#ifdef RTCONFIG_OPENVPN
/* Initialize OpenVPN state flags */

	for (i = 1; i <= OVPN_CLIENT_MAX; i++) {
		sprintf(varname, "vpn_client%d_state", i);
		nvram_set(varname, "0");

		sprintf(varname, "vpn_client%d_errno", i);
		nvram_set(varname, "0");
	}

	nvram_set("vpn_server1_state", "0");
	nvram_set("vpn_server2_state", "0");
	nvram_set("vpn_server1_errno", "0");
	nvram_set("vpn_server2_errno", "0");
	nvram_set("vpn_upload_state", "");
#endif

/* Remove potentially obsolete data, especially if coming from 380.xx */
	nvram_unset("webs_state_info");
	nvram_unset("webs_state_info_beta");

	if (restore_defaults_g)
	{
		nvram_set("computer_name", friendly_name);
		nvram_set("dms_friendly_name", friendly_name);
		nvram_set("daapd_friendly_name", friendly_name);

		nvram_commit();
	}
#if defined(RTCONFIG_CONCURRENTREPEATER)
	if (sw_mode() == SW_MODE_REPEATER) {
		if (nvram_get_int("wlc_express") == 0) {
			nvram_set_int("wlc_band", -1);
#ifdef RTCONFIG_REALTEK
			nvram_set("wlc_ssid", "");
#endif
		}
		else {
			if (nvram_get_int("wlc_express") == 1) {
				nvram_set_int("wlc_band", 0);
				nvram_set_int("wlc1_state", 0);
#ifdef RTCONFIG_REALTEK
				nvram_set("wlc_ssid", nvram_safe_get("wlc1_ssid"));
#endif
			}
#if defined(RTCONFIG_HAS_5G)
			else if (nvram_get_int("wlc_express") == 2) {
				nvram_set_int("wlc_band", 1);
				nvram_set_int("wlc0_state", 0);
#ifdef RTCONFIG_REALTEK
				nvram_set("wlc_ssid", nvram_safe_get("wlc1_ssid"));
#endif
			}
#endif
			//nvram_unset("wlc_band");
		}
	}
#ifdef RTCONFIG_REALTEK
	/* For Realtek Media Bridge Mode condition */
	else if (sw_mode() == SW_MODE_AP && nvram_get_int("wlc_psta") == 1) {
		nvram_set_int("wlc_band", -1);
		nvram_set("wlc_ssid", "");
	}
#endif

	nvram_set_int("led_status", LED_BOOTING);
#endif // RTCONFIG_CONCURRENTREPEATER

#ifdef RTCONFIG_WPS_ALLLED_BTN
	nvram_set_int("AllLED", 1);
#endif
#ifdef RTCONFIG_WIFI_SON
	nvram_set("wsplcd_uptime", "0");
	nvram_set("hyd_uptime", "0");
	nvram_set("cap_syncing","0");
	nvram_set("re_syncing","0");
	nvram_set("re_asuscmd","0");
#if defined(RTCONFIG_AUTHSUPP)
	nvram_set("now_security","0");
	nvram_set("cap_security_old","0");
	nvram_set("re_security_new","0");
	nvram_set("re_security_ext","0");
#endif
	nvram_unset("lyra_re_dist");
	nvram_set("spcmd","0");
#ifdef RTCONFIG_ETHBACKHAUL
	nvram_unset("chaos_eth_daemon");
#endif
#ifndef RTCONFIG_DUAL_BACKHAUL
	nvram_set("wps_band_x","1");
#endif
//debug
	nvram_set("hive_dbg","1");
	nvram_set("log_size","1024");
//
#endif /* RTCONFIG_WIFI_SON */
#ifdef RTCONFIG_CFGSYNC
	nvram_set("cfg_cost", "0");
	nvram_unset("cfg_alive");
	nvram_unset("cfg_masterip");
	if (!is_router_mode())
		nvram_unset("cfg_cost");
#ifdef RTCONFIG_QCA
	if (strlen(nvram_safe_get("cfg_group")) == 0) {
		unsigned char *gid = nvram_safe_get("cfg_group_fac");
		if (strlen(gid))
			nvram_set("cfg_group", gid);
	}
#endif
#endif
#if defined(MAPAC1300) || defined(MAPAC2200) || defined(VZWAC1300) /* for Lyra */
	if (nvram_match("disableWifiDrv_fac", "1"))
		nvram_set("lyra_disable_wifi_drv", "1");
	nvram_unset("disableWifiDrv_fac");
#endif
	return 0;
}

#if defined(RTCONFIG_SOC_IPQ40XX)
int init_nvram3(void)
{

	int model = get_model();
	switch(model)
	{
	case MODEL_RTAC82U:
		/* enable bled */
		config_netdev_bled("led_wan_gpio", "eth0");

#ifdef RTCONFIG_LAN4WAN_LED
		config_swports_bled_sleep("led_lan1_gpio", 1);
		config_swports_bled_sleep("led_lan2_gpio", 2);
		config_swports_bled_sleep("led_lan3_gpio", 4);
		config_swports_bled_sleep("led_lan4_gpio", 8);
#endif

		config_netdev_bled("led_2g_gpio", "ath0");

		config_netdev_bled("led_5g_gpio", "ath1");

		//config_usbbus_bled("led_usb_gpio", "3 4");
		//config_usbbus_bled("led_usb3_gpio", "1 2");
		break;

	case MODEL_RTAC58U:
		/* enable bled */
		config_netdev_bled("led_wan_gpio", "eth0");
#if 0
		config_swports_bled_sleep("led_lan_gpio", 0);
#else  //workaround for lan led
		config_netdev_bled("led_lan_gpio", "eth1");
#endif

		config_netdev_bled("led_2g_gpio", "ath0");

		config_netdev_bled("led_5g_gpio", "ath1");

		config_usbbus_bled("led_usb_gpio", "3 4");
		config_usbbus_bled("led_usb3_gpio", "1 2");
		break;

	case MODEL_RT4GAC53U:
		/* enable bled */
		config_netdev_bled("led_lan_gpio", "eth1");
		config_netdev_bled("led_2g_gpio", "ath0");
		config_netdev_bled("led_5g_gpio", "ath1");
		config_usbbus_bled("led_usb_gpio", "3 4");
		config_usbbus_bled("led_usb3_gpio", "1 2");
		break;

	case MODEL_MAPAC2200:
		/* modify PSGMII parameter that suggested from ODM */
		doSystem("devmem 0x98288 32 0x8280");
	case MODEL_MAPAC1300:
	case MODEL_VZWAC1300:
		/* TBD */
		break;
	case MODEL_MAPAC3000:
		/* TBD */
		break;

	default:
		_dprintf("############################ unknown model #################################\n");
		break;
	}
	return 0;
}
#endif

void force_free_caches()
{
#ifdef RTCONFIG_RALINK
	int model = get_model();

	if (model != MODEL_RTN65U)
		free_caches(FREE_MEM_PAGE, 2, 0);
#elif defined(RTCONFIG_QCA)
	/* do nothing */
#elif defined(RTCONFIG_REALTEK)
	int model = get_model();
	if (/*model != MODEL_RTRTL8198C &&*/ model != MODEL_RPAC68U)
		free_caches(FREE_MEM_PAGE, 2, 0);
#else
	int model = get_model();

	if (model==MODEL_RTN53||model==MODEL_RTN10D1) {
		free_caches(FREE_MEM_PAGE, 2, 0);
	}
#endif

#ifdef RTCONFIG_BCMARM
	f_write_string("/proc/sys/vm/drop_caches", "1", 0, 0);
#endif
	nvram_unset("reload_svc_radio");
}

#ifdef RTN65U
/* Set PCIe ASPM policy */
int set_pcie_aspm(void)
{
	const char *aspm_policy = "powersave";

	if (nvram_get_int("aspm_disable"))
		aspm_policy = "performance";

	return f_write_string("/sys/module/pcie_aspm/parameters/policy", aspm_policy, 0, 0);
}
#endif //RTN65U

int limit_page_cache_ratio(int ratio)
{
	int min = 5;
	char p[] = "100XXX";

	if (ratio > 100 || ratio < min)
		return -1;

	if (ratio < 90) {
#if defined(RTCONFIG_OPEN_NTFS3G) && (LINUX_KERNEL_VERSION < KERNEL_VERSION(3,0,0))
		min = 90;
#endif

		if (get_meminfo_item("MemTotal") <= 64)
			min = 90;
	}

	if (ratio < min)
		ratio = min;

	sprintf(p, "%d", ratio);
	return f_write_string("/proc/sys/vm/pagecache_ratio", p, 0, 0);
}

#ifdef RTCONFIG_BCMFA
#if 0
static int
build_ifnames(char *type, char *names, int *size)
{
	char name[32], *next;
	int len = 0;
	int s;

	/* open a raw scoket for ioctl */
	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		return -1;

	foreach(name, type, next) {
		struct ifreq ifr;
		int i, unit;
		char var[32], *mac;
		unsigned char ea[ETHER_ADDR_LEN];

		/* vlan: add it to interface name list */
		if (!strncmp(name, "vlan", 4)) {
			/* append interface name to list */
			len += snprintf(&names[len], *size - len, "%s ", name);
			continue;
		}

		/* others: proceed only when rules are met */
		for (i = 1; i <= DEV_NUMIFS; i ++) {
			/* ignore i/f that is not ethernet */
			ifr.ifr_ifindex = i;
			if (ioctl(s, SIOCGIFNAME, &ifr))
				continue;
			if (ioctl(s, SIOCGIFHWADDR, &ifr))
				continue;
			if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
				continue;
			if (!strncmp(ifr.ifr_name, "vlan", 4))
				continue;
			/* wl: use unit # to identify wl */
			if (!strncmp(name, "wl", 2)) {
				if (wl_probe(ifr.ifr_name) ||
				    wl_ioctl(ifr.ifr_name, WLC_GET_INSTANCE, &unit, sizeof(unit)) ||
				    unit != atoi(&name[2]))
					continue;
			}
			/* et/il: use mac addr to identify et/il */
			else if (!strncmp(name, "et", 2) || !strncmp(name, "il", 2)) {
				snprintf(var, sizeof(var), "%smacaddr", name);
				if (!(mac = nvram_get(var)) || !ether_atoe(mac, ea) ||
				    bcmp(ea, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN))
					continue;
			}
			/* mac address: compare value */
			else if (ether_atoe(name, ea) &&
				!bcmp(ea, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN))
				;
			/* others: ignore */
			else
				continue;
			/* append interface name to list */
			len += snprintf(&names[len], *size - len, "%s ", ifr.ifr_name);
		}
	}

	close(s);

	*size = len;
	return 0;
}
#endif

/* Override the "0 5u" to "0 5" to backward compatible with old image */
static void
fa_override_vlan2ports()
{
	char port[] = "XXXX", *nvalue;
	char *next, *cur, *ports, *u;
	int len;

	ports = nvram_get("vlan2ports");
	nvalue = malloc(strlen(ports) + 2);
	if (!nvalue) {
		_dprintf("Memory allocate failed!\n");
		return;
	}
	memset(nvalue, 0, strlen(ports) + 2);

	/* search last port include 'u' */
	for (cur = ports; cur; cur = next) {
		/* tokenize the port list */
		while (*cur == ' ')
			cur ++;
		next = strstr(cur, " ");
		len = next ? next - cur : strlen(cur);
		if (!len)
			break;
		if (len > sizeof(port) - 1)
			len = sizeof(port) - 1;
		strncpy(port, cur, len);
		port[len] = 0;

		/* prepare new value */
		if ((u = strchr(port, 'u')))
			*u = '\0';
		strcat(nvalue, port);
		strcat(nvalue, " ");
	}

	/* Remove last " " */
	len = strlen(nvalue);
	if (len) {
		nvalue[len-1] = '\0';
		nvram_set("vlan2ports", nvalue);
	}
}

void
fa_mode_adjust()
{
	char *wan_proto;
	char buf[16];
	int sw_mode = sw_mode();
	int bhdr = 0;

	if (sw_mode == SW_MODE_ROUTER
#ifdef RTCONFIG_RGMII_BCM_FA
		|| sw_mode == SW_MODE_AP
#endif
	) {
		if (!nvram_match("ctf_disable_force", "1")
			&& nvram_get_int("ctf_fa_cap")
#if !defined(HND_ROUTER)
			&& !nvram_get_int("cstats_enable")
#endif
			&& !nvram_match("gmac3_enable", "1")
			&& !nvram_get_int("qos_enable")
			&& nvram_match("x_Setting", "1")
		) {
			bhdr = 1;
			nvram_set_int("ctf_fa_mode", CTF_FA_NORMAL);
		} else {
			nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
		}
	} else { /* repeater mode */
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
	}

	if (sw_mode == SW_MODE_ROUTER) {
		snprintf(buf, 16, "%s", nvram_safe_get("wans_dualwan"));
		if (!strcmp(buf, "wan none") || !strcmp(buf, "wan usb"))
			wan_proto = nvram_safe_get("wan0_proto");
		else // for (usb wan).
			wan_proto = nvram_safe_get("wan1_proto");

		if ((strcmp(wan_proto, "dhcp") && strcmp(wan_proto, "static"))
#if defined(RTCONFIG_BWDPI)
			|| check_bwdpi_nvram_setting()
#endif
		) {
			nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
			_dprintf("wan_proto:%s not support FA mode...\n", wan_proto);
		}

		if (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")) {
			nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
			dbG("IPTV environment, disable FA mode\n");
		}
	}

#ifdef RTCONFIG_DSL	//TODO
	nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
#elif defined(RTCONFIG_PORT_BASED_VLAN) || defined(RTCONFIG_TAGGED_BASED_VLAN)
	if (vlan_enable())
		nvram_set_int("ctf_fa_mode", CTF_FA_DISABLED);
#endif
	nvram_set_int("bhdr_enable", bhdr ? 1 : 0);
}

void
fa_mode_init()
{
	fa_mode_adjust();

	fa_mode = atoi(nvram_safe_get("ctf_fa_mode"));
	switch (fa_mode) {
		case CTF_FA_BYPASS:
		case CTF_FA_NORMAL:
			break;
		default:
			fa_mode = CTF_FA_DISABLED;
			break;
	}
}

void
fa_nvram_adjust()	/* before insmod et */
{
#ifdef RTCONFIG_DUALWAN
	char buf[16];
	snprintf(buf, 16, "%s", nvram_safe_get("wans_dualwan"));
	if (strcmp(buf, "wan none") && strcmp(buf, "wan usb") && strcmp(buf, "usb wan"))
		return;
#endif

	fa_mode = nvram_get_int("ctf_fa_mode");
	if (FA_ON(fa_mode)) {

		_dprintf("\nFA on.\n");

		/* Set et2macaddr, et2phyaddr as same as et0macaddr, et0phyaddr */
		if (!nvram_get("vlan2ports") || !nvram_get("wandevs"))  {
			_dprintf("Insufficient envram, cannot do FA override\n");
			return;
		}

		/* adjusted */
		if (!strcmp(nvram_get("wandevs"), "vlan2") &&
		!strchr(nvram_get("vlan2ports"), 'u'))
			return;

		/* The vlan2ports will be change to "0 8u" dynamically by
		 * robo_fa_imp_port_upd. Keep nvram vlan2ports unchange.
		 */
		fa_override_vlan2ports();

		/* Override wandevs to "vlan2" */
		nvram_set("wandevs", "vlan2");
		nvram_set("wan_ifnames", "vlan2");
		if (nvram_match("wan0_ifname", "eth0")) nvram_set("wan0_ifname", "vlan2");

		if (!RESTORE_DEFAULTS()) {
			_dprintf("Override FA nvram...\n");
			nvram_commit();
		}
	}
	else {
		_dprintf("\nFA off.\n");
	}
}

void
chk_etfa()	/* after insmod et */
{
	if (FA_ON(fa_mode)) {
		if (
			!nvram_get_int("ctf_fa_cap") ||
			nvram_match("ctf_disable", "1") || nvram_match("ctf_disable_force", "1")) { // in case UI chaos
			_dprintf("\nChip Not Support FA Mode or ctf disabled!\n");

			nvram_unset("ctf_fa_mode");
			nvram_commit();
			reboot(RB_AUTOBOOT);
			return;
		}
	} else {
		if (!nvram_get_int("ctf_fa_cap"))
			nvram_unset("ctf_fa_mode");
	}
}
#endif /* RTCONFIG_BCMFA */

#if defined(RPAC51)
void write_caldata_file()
{
TRACE_PT("write_caldata_file\n");
doSystem("dd if=/dev/mtdblock2 of=/tmp/wifi0.caldata bs=32 count=377 skip=128");  //2.4
doSystem("dd if=/dev/mtdblock2 of=/tmp/wifi1.caldata bs=32 count=377 skip=640");  //5G
}
#endif

#ifdef HND_ROUTER
#define WATCHDOG_MIN_LX4x 4000
static void start_hw_wdt(void)
{
	int wdt = nvram_get("watchdog") ? atoi(nvram_safe_get("watchdog")) : atoi(nvram_default_get("watchdog"));

	/* arm the hw watchdog timer */
	if (wdt) {
		char tmp[100];

		wdt = (wdt > WATCHDOG_MIN_LX4x) ? wdt : WATCHDOG_MIN_LX4x;
		/* convert to secs */
		wdt /= 1000;
		printf("wdt:%d\n", wdt);

		snprintf(tmp, sizeof(tmp), "wdtctl -d -t %d start", wdt);
		system(tmp);
	}
}
#endif

void Ate_on_off_led_fail_loop(void)
{
	while(1) {
#ifdef RTCONFIG_LP5523
		lp55xx_leds_proc(LP55XX_RED_LEDS, LP55XX_ACT_NONE);
		pause();
#elif defined(MAPAC1750)
		set_rgbled(RGBLED_RED);
		pause();
#elif defined(RTCONFIG_CONCURRENTREPEATER) && (defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK))
		setAllLedOff();
		sleep(1);
		setAllLedOn();
		sleep(1);
#else
		led_control(LED_POWER, LED_OFF);
		sleep(1);
		led_control(LED_POWER, LED_ON);
		sleep(1);
#endif	/* RTCONFIG_LP5523 */
	}
}

void Ate_on_off_led_success(void)
{
	extern void asus_ate_StartATEMode(void);
	int led_on_cut;
#if defined(RT4GAC68U) || defined(RTCONFIG_LP5523)
	#define LED_ON_CUT_TRY	1
#else
	#define LED_ON_CUT_TRY	6
#endif

	for(led_on_cut=0; led_on_cut<LED_ON_CUT_TRY; led_on_cut++)
	{
		dbG("delay 5 secs & Set_StartATEMode...\n");
#if defined(RTCONFIG_SOC_IPQ40XX)
		doSystem("/sbin/delay_exec 5");
#else
		{
			int i;
			i = 1;
			while(i <= 5){
				sleep(1);

				++i;
			}
		}
#endif
#ifdef RTCONFIG_BCMARM
#ifdef RT4GAC68U
		asus_ate_StartATEMode();
		setAllLedOn();
#else
		setATEModeLedOn();
#endif
#else
		asus_ate_StartATEMode();
#ifdef RTCONFIG_LP5523
		lp55xx_leds_proc(LP55XX_GREENERY_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
		set_rgbled(RGBLED_GREEN);
#else
		if (get_model() == MODEL_RTAC53U) {
			led_control(LED_POWER, LED_ON);
			led_control(LED_USB, LED_ON);
		}
		else
			setAllLedOn();
#endif	/* RTCONFIG_LP5523 */
#endif
	}
}

void Ate_on_off_led_start(void)
{
#ifdef RTCONFIG_LP5523
	lp55xx_leds_proc(LP55XX_PURPLE_LEDS, LP55XX_ACT_3ON1OFF);
#elif defined(MAPAC1750)
	set_rgbled(RGBLED_PURPLE_3ON1OFF);
#endif	/* RTCONFIG_LP5523 */
}

static void sysinit(void)
{
	static int noconsole = 0;
	static const time_t tm = 0;
	int i, r, retry = 0;
	DIR *d;
	struct dirent *de;
	char s[256];
	char t[256];
	int ratio = 30;
	int min_free_kbytes_check= 0;
#if !defined(RTCONFIG_QCA)
	int model;
#endif

#ifdef HND_ROUTER
	_dprintf("\nLaunch boot...\n");

	system("bcm_boot_launcher start");
	start_hw_wdt();
#endif

#define MKNOD(name,mode,dev)		if (mknod(name,mode,dev))		perror("## mknod " name)
#define MOUNT(src,dest,type,flag,data)	if (mount(src,dest,type,flag,data))	perror("## mount " src)
#define MKDIR(pathname,mode)		if (mkdir(pathname,mode))		perror("## mkdir " pathname)
#ifdef RTCONFIG_RTK_NAND
/* [MUST]: Need to Clarify ... */
	system("mount -t yaffs2 -o tags-ecc-off /dev/mtdblock1 /hw_setting");
	system("mount -t yaffs2 -o tags-ecc-off /dev/mtdblock0 /hw_setting2");
	sleep(1);

	if(check_rtk_hw_invaild()) {
		printf("\n==> restore settings from /hw_setting2 !!\n");
		system("cp -f /hw_setting2/*.bin /hw_setting/");
		system("cp -f /hw_setting2/hw.bin /hw_setting/hw_backup.bin");
	}
#endif
#ifndef HND_ROUTER
	MOUNT("proc", "/proc", "proc", MS_MGC_VAL, NULL);
#endif

#ifdef RTCONFIG_REALTEK
#ifdef RPAC68U
	char c;
	f_write_string("/proc/asus_ate", "restore 1", 0, 0);
	f_read("/proc/asus_ate", &c, 1);
	int restore_flag = c - '0';
	if (restore_flag == 1) {
		dbg("Erase NVRAM...\n");
		system("rm /hw_setting/nvram.bin");
		system("rm /hw_setting2/nvram.bin");
		sleep(1);
		reboot(RB_AUTOBOOT);
	}
#endif
#endif

	MOUNT("tmpfs", "/tmp", "tmpfs", 0, NULL);

#ifdef LINUX26
#ifndef DEVTMPFS
	MOUNT("devfs", "/dev", "tmpfs", MS_MGC_VAL | MS_NOATIME, NULL);
#endif
	MKNOD("/dev/null", S_IFCHR | 0666, makedev(1, 3));
	MKNOD("/dev/console", S_IFCHR | 0600, makedev(5, 1));
	MOUNT("sysfs", "/sys", "sysfs", MS_MGC_VAL, NULL);
	MKDIR("/dev/shm", 0777);
	MKDIR("/dev/pts", 0777);
#ifdef BLUECAVE
	MKDIR("/dev/switch_api", 0775);
	MKDIR("/tmp/wireless", 0777);
	MKDIR("/tmp/ramdisk", 0777);
#endif
	MOUNT("devpts", "/dev/pts", "devpts", MS_MGC_VAL, NULL);
#if defined(RTCONFIG_WIFI_QCA9994_QCA9994) || defined(RTCONFIG_SOC_IPQ40XX)
	MOUNT("debugfs", "/sys/kernel/debug", "debugfs", MS_MGC_VAL, NULL);
#endif
#endif

#ifdef __CONFIG_HSPOT__
	if (mkdir(RAMFS_CONFMTD_DIR, 0777) < 0 && errno != EEXIST) {
		perror("/tmp/confmtd not created.");
	}
	if (mkdir(RAMFS_CONFMTD_DIR"/hspot", 0777) < 0 && errno != EEXIST) {
		perror("/tmp/confmtd/hspot not created.");
	}
#endif /* __CONFIG_HSPOT__ */

#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
	if (mkdir(RAMFS_CONFMTD_DIR, 0777) < 0 && errno != EEXIST) {
		perror("/tmp/confmtd not created.");
	}
	if (mkdir(RAMFS_CONFMTD_DIR"/crash_logs", 0777) < 0 && errno != EEXIST) {
		perror("/tmp/confmtd/crash_logs not created.");
	}
#endif

#ifdef HND_ROUTER
	system("ethswctl -c wan -o enable -i eth0");

#if defined(GTAC5300)
	// clean the egress port first to avoid the 2nd WAN's DHCP.
	system("ethswctl -c regaccess -l 4 -v 0x3102 -d 0x60");
	system("ethswctl -c regaccess -l 4 -v 0x3100 -d 0x50");
	system("ethswctl -c regaccess -l 4 -v 0x3106 -d 0x140");
	system("ethswctl -c regaccess -l 4 -v 0x3104 -d 0x60");
	system("ethswctl -c regaccess -l 4 -v 0x310e -d 0x140");
#elif defined(RTAC86U)
	// clean the egress port first to avoid the 2nd WAN's DHCP.
	system("ethswctl -c regaccess -l 4 -v 0x3106 -d 0x1c0");
	system("ethswctl -c regaccess -l 4 -v 0x3104 -d 0xe0");
	system("ethswctl -c regaccess -l 4 -v 0x3102 -d 0xe0");
	system("ethswctl -c regaccess -l 4 -v 0x3100 -d 0xd0");
#endif

	system("tmctl porttminit --devtype 0 --if eth0 --flag 1");
	system("tmctl porttminit --devtype 0 --if eth1 --flag 1");
	system("tmctl porttminit --devtype 0 --if eth2 --flag 1");
	system("tmctl porttminit --devtype 0 --if eth3 --flag 1");
	system("tmctl porttminit --devtype 0 --if eth4 --flag 1");
#ifdef RTCONFIG_EXT_BCM53134
	system("tmctl porttminit --devtype 0 --if eth5 --flag 1");
#endif
#endif

	init_devs(); // for system dependent part

	if (console_init()) noconsole = 1;

	stime(&tm);


	static const char *mkd[] = {
		"/tmp/etc", "/tmp/var", "/tmp/home",
#ifdef RTCONFIG_USB
		POOL_MOUNT_ROOT,
		NOTIFY_DIR,
		NOTIFY_DIR"/"NOTIFY_TYPE_USB,
#endif
		"/tmp/share", "/var/webmon", // !!TB
		"/var/log", "/var/run", "/var/tmp", "/var/lib", "/var/lib/misc",
		"/var/spool", "/var/spool/cron", "/var/spool/cron/crontabs",
		"/tmp/var/wwwext", "/tmp/var/wwwext/cgi-bin",	// !!TB - CGI support
#ifdef BLUECAVE
		"/tmp/etc/rc.d",
#endif
		"/tmp/var/tmp",
		NULL
	};
	umask(0);
	for (i = 0; mkd[i]; ++i) {
		mkdir(mkd[i], 0755);
	}
	// LPRng support
	mkdir("/var/state", 0777);
	mkdir("/var/state/parport", 0777);
	mkdir("/var/state/parport/svr_statue", 0777);

	mkdir("/var/lock", 0777);
	mkdir("/var/tmp/dhcp", 0777);
	mkdir("/home/root", 0700);
	chmod("/tmp", 0777);
#ifdef RTCONFIG_USB
	chmod(POOL_MOUNT_ROOT, 0777);
#endif
	f_write("/etc/hosts", NULL, 0, 0, 0644);			// blank
	f_write("/etc/fstab", NULL, 0, 0, 0644);			// !!TB - blank
	f_write("/tmp/settings", NULL, 0, 0, 0644);

	//umask(022);

	if ((d = opendir("/rom/etc")) != NULL) {
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.') continue;
			snprintf(s, sizeof(s), "%s/%s", "/rom/etc", de->d_name);
			snprintf(t, sizeof(t), "%s/%s", "/etc", de->d_name);
			symlink(s, t);
		}
		closedir(d);
	}
	symlink("/proc/mounts", "/etc/mtab");

	/* /etc/resolv.conf compatibility */
	unlink("/etc/resolv.conf");
	symlink("/tmp/resolv.conf", "/etc/resolv.conf");

#ifdef HND_ROUTER
	system("/sbin/ldconfig &>/dev/null");
#endif

#if defined(RTAC55U) || defined(RTAC55UHP) || defined(RTAC58U) || defined(RT4GAC53U) || defined(RTAC82U) || defined(MAPAC3000)
	ratio = 20;
#elif defined(RTCONFIG_SOC_IPQ8064)
	ratio = 90;
#endif
	limit_page_cache_ratio(ratio);

#ifdef RTN65U
	set_pcie_aspm();
#endif

#ifdef RTCONFIG_SAMBASRV
	if ((d = opendir("/usr/codepages")) != NULL) {
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.') continue;
			snprintf(s, sizeof(s), "/usr/codepages/%s", de->d_name);
			snprintf(t, sizeof(t), "/usr/share/%s", de->d_name);
			symlink(s, t);
		}
		closedir(d);
	}
#endif


#if !defined(RTCONFIG_SOC_IPQ40XX)
#if defined(RTCONFIG_BLINK_LED)
	modprobe("bled");
#endif
#endif
#ifdef LINUX26
	do {
		r = eval("mdev", "-s");
		if (r || retry)
			_dprintf("mdev coldplug terminated. (ret %d retry %d)\n", r, retry);
	} while (r && retry++ < 10);
#ifdef RTCONFIG_LANTIQ
	init_devs_defer(); // for system dependent part
#endif

#ifdef RTCONFIG_NVRAM_FILE
	start_nvram_txt();
#endif
	start_hotplug2();

	static const char *dn[] = {
		"null", "zero", "random", "urandom", "full", "ptmx",
#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
		MTD_OF_NVRAM,
#else
		"nvram",
#endif
		NULL
	};
	for (i = 0; dn[i]; ++i) {
		snprintf(s, sizeof(s), "/dev/%s", dn[i]);
		chmod(s, 0666);
	}
#ifndef HND_ROUTER
	chmod("/dev/gpio", 0660);
#endif
#endif

	set_action(ACT_IDLE);

	for (i = 0; defenv[i]; ++i) {
		putenv(defenv[i]);
	}

	if (!noconsole) {
		printf("\n\nHit ENTER for console...\n\n");
		run_shell(1, 0);
	}

	dbg("firmware version: %s_%s\n", rt_serialno, rt_extendno);

	nvram_set("buildno_org", nvram_safe_get("buildno"));
	if(rt_rcno)
		nvram_set("rcno_org", nvram_safe_get("rcno"));
	else
		nvram_unset("rcno_org");
	nvram_set("extendno_org", nvram_safe_get("extendno"));

	init_syspara();// for system dependent part (befor first get_model())

	set_hostname();

#ifdef RTCONFIG_RALINK
	model = get_model();
	// avoid the process like fsck to devour the memory.
	// ex: when DUT ran fscking, restarting wireless would let DUT crash.

	if ((model == MODEL_RTAC1200) ||(model == MODEL_RTAC1200GA1) ||(model == MODEL_RTAC1200GU))
	{
		f_write_string("/proc/sys/vm/min_free_kbytes", "8192", 0, 0);
		min_free_kbytes_check = 1;
	}
	else if ((model == MODEL_RTN56U) || (model == MODEL_DSLN55U) || (model == MODEL_RTAC52U) || (model == MODEL_RTAC51U) \
			 || (model == MODEL_RTAC51UP) ||(model == MODEL_RTAC53) ||(model == MODEL_RTN14U) ||(model == MODEL_RTN54U) \
			 || (model == MODEL_RTAC54U) ||(model == MODEL_RTAC1200HP) || (model == MODEL_RTN56UB1) || (model == MODEL_RTN56UB2) \
			 || (model == MODEL_RTN800HP))
	{
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
		min_free_kbytes_check = 1;
	}
	else
	{
		f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
		min_free_kbytes_check = 0;
	}
#elif defined(RTCONFIG_QCA)
#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	f_write_string("/proc/sys/vm/min_free_kbytes", "23916", 0, 0);
#elif defined(RPAC51)
	f_write_string("/proc/sys/vm/min_free_kbytes", "8192", 0, 0);
#else
	f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
#endif
	min_free_kbytes_check = 1;
#elif defined(RTCONFIG_REALTEK)
	model = get_model();
	if (/*model == MODEL_RTRTL8198C ||*/ model == MODEL_RPAC68U)
	{
		f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
		min_free_kbytes_check = 1;
		rtl_configRps();
	}

#else
	model = get_model();
	// At the Broadcom platform, restarting wireless won't use too much memory.
	if (model==MODEL_RTN53) {
		f_write_string("/proc/sys/vm/min_free_kbytes", "2048", 0, 0);
		min_free_kbytes_check = 0;
	}
	else if (model==MODEL_RTAC53U) {
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
		min_free_kbytes_check = 1;
	}
#endif
#ifdef RTCONFIG_16M_RAM_SFP
	f_write_string("/proc/sys/vm/min_free_kbytes", "512", 0, 0);
	min_free_kbytes_check = 0;
#endif
#ifdef RTCONFIG_BCMARM
	if (model==MODEL_RTAC1200G || model==MODEL_RTAC1200GP)
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
	else
//	f_write_string("/proc/sys/vm/min_free_kbytes", "14336", 0, 0);
	// fix _dma_rxfill error under stress test
		f_write_string("/proc/sys/vm/min_free_kbytes", "20480", 0, 0);
	min_free_kbytes_check = 1;
#endif
#ifdef RTCONFIG_USB
	if(min_free_kbytes_check<1){
		f_write_string("/proc/sys/vm/min_free_kbytes", "4096", 0, 0);
	}
#endif

#ifdef RTCONFIG_LANTIQ
	if(nvram_match("blver", "0.0.3.12"))
		f_write_string("/proc/sys/vm/min_free_kbytes", "30720", 0, 0);
	else
		f_write_string("/proc/sys/vm/min_free_kbytes", "8192", 0, 0);
	f_write_string("/proc/sys/vm/overcommit_memory", "0", 0, 0);
	f_write_string("/proc/sys/vm/panic_on_oom", "1", 0, 0);
	f_write_string("/proc/sys/vm/overcommit_ratio", "10", 0, 0);

	f_write_string("/proc/sys/vm/lowmem_reserve_ratio", "250", 0, 0);
	f_write_string("/proc/sys/vm/dirty_background_ratio", "2", 0, 0);
	f_write_string("/proc/sys/vm/dirty_writeback_centisecs", "250", 0, 0);
	f_write_string("/proc/sys/vm/dirty_ratio", "10", 0, 0);
	f_write_string("/proc/sys/vm/max_map_count", "16384", 0, 0);
	f_write_string("/proc/sys/vm/scan_unevictable_pages", "1", 0, 0);
	f_write_string("/proc/sys/net/ipv4/netfilter/ip_conntrack_tcp_timeout_established", "2400", 0, 0);
#endif

#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
	// for the nvram file's commit, if don't set this & the commit will cause the abnormal nvram file.
	_dprintf("init: set drop_caches be 1 at ALPINE & LANTIQ...\n");
	f_write_string("/proc/sys/vm/drop_caches", "1", 0, 0);
#else
	force_free_caches();
#endif

	// autoreboot after kernel panic
	f_write_string("/proc/sys/kernel/panic", "3", 0, 0);
	f_write_string("/proc/sys/kernel/panic_on_oops", "3", 0, 0);

	// be precise about vm commit
#ifdef CONFIG_BCMWL5
	f_write_string("/proc/sys/vm/overcommit_memory", "2", 0, 0);

	if (model==MODEL_RTN53 ||
		model==MODEL_RTN10U ||
		model==MODEL_RTN12B1 || model==MODEL_RTN12C1 ||
		model==MODEL_RTN12D1 || model==MODEL_RTN12VP || model==MODEL_RTN12HP || model==MODEL_RTN12HP_B1 ||
		model==MODEL_RTN15U || model==MODEL_RTN14UHP || model==MODEL_RTAC1200G || model==MODEL_RTAC1200GP) {

		f_write_string("/proc/sys/vm/panic_on_oom", "1", 0, 0);
		f_write_string("/proc/sys/vm/overcommit_ratio", "100", 0, 0);
	}
#endif

#ifdef RTCONFIG_IPV6
	// disable IPv6 by default on all interfaces
	f_write_string("/proc/sys/net/ipv6/conf/default/disable_ipv6", "1", 0, 0);
	// disable IPv6 DAD by default on all interfaces
	f_write_string("/proc/sys/net/ipv6/conf/default/accept_dad", "2", 0, 0);
	// increase ARP cache
	f_write_string("/proc/sys/net/ipv6/neigh/default/gc_thresh1", "512", 0, 0);
	f_write_string("/proc/sys/net/ipv6/neigh/default/gc_thresh2", "1024", 0, 0);
	f_write_string("/proc/sys/net/ipv6/neigh/default/gc_thresh3", "2048", 0, 0);
#endif

#if defined(RTCONFIG_PSISTLOG) || defined(RTCONFIG_JFFS2LOG)
	f_write_string("/proc/sys/net/unix/max_dgram_qlen", "150", 0, 0);
#endif

	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++) {
		signal(fatalsigs[i], handle_fatalsigs);
	}
	signal(SIGCHLD, handle_reap);

#ifdef RTCONFIG_DSL
#ifdef RTCONFIG_RALINK
	check_upgrade_from_old_ui();
#endif
#endif

#ifdef RTCONFIG_ATEUSB3_FORCE
	post_syspara();	// make sure usb_usb3 is fix-up in accordance with the force USB3 parameter.
#endif

#ifdef RTCONFIG_GMAC3
	int gmac3 = 0;
#ifdef RTCONFIG_BCM_7114
	if (!nvram_match("stop_gmac3_new", "1")
#if 0
		&& !(nvram_get("switch_wantag") && !nvram_match("switch_wantag", "") && !nvram_match("switch_wantag", "none"))
#endif
	)
	gmac3 = 1;
#else
	nvram_set("gmac3_enable", "0");
#endif
	nvram_set_int("gmac3_enable", gmac3 ? 1 : 0);
	nvram_set_int("bhdr_enable", gmac3 ? 1 : 0);
#endif

#ifdef RTCONFIG_BCMFA
	fa_mode_init();
#endif

#ifdef RTCONFIG_DETWAN
	if ((sw_mode() == SW_MODE_ROUTER)) {
		if (nvram_safe_get("detwan_ifname")[0] != '\0') {
			if (nvram_safe_get("wan0_ifname")[0] == '\0') {
				/* restore wan0_ifname */
				nvram_set("wan0_ifname", nvram_safe_get("detwan_ifname"));
				nvram_set("wanports_mask", nvram_safe_get("detwan_wan_mask"));
				nvram_set("lanports_mask", nvram_safe_get("detwan_lan_mask"));
				nvram_set("detwan_proto", "1");
				nvram_unset("lan_ipaddr");
				nvram_unset("lan_gateway");
				nvram_unset("lan_hwaddr");
			}
		} else if (nvram_match("x_Setting", "1")) {
			/* old firmware, update nvram */
			nvram_set("detwan_ifname", nvram_safe_get("wan0_ifname"));
			nvram_set("detwan_wan_mask", nvram_safe_get("wanports_mask"));
			nvram_set("detwan_lan_mask", nvram_safe_get("lanports_mask"));
		}
	}
#endif
	init_nvram();  // for system indepent part after getting model
	restore_defaults(); // restore default if necessary
	init_nvram2();

#if defined (RTCONFIG_WLMODULE_MT7615E_AP)
#if !defined(RTCONFIG_CONCURRENTREPEATER)
	nvram_set("lan_ifnames_guess", nvram_safe_get("lan_ifnames"));
#endif
#endif

#ifdef RTCONFIG_ATEUSB3_FORCE
	post_syspara(); // adjust nvram variable after restore_defaults
#endif

#ifdef RTCONFIG_BCM_7114
#ifdef RTCONFIG_GMAC3
	chk_gmac3_excludes();
#else
	gmac3_restore_nvram();
#endif
#endif

#if defined(RPAC51)
	write_caldata_file();
#endif

#if !defined(CONFIG_BCMWL5)	//Broadcom set this in check_wl_territory_code()
	void handle_location_code_for_wl(void);
	handle_location_code_for_wl();
#endif	/* CONFIG_BCMWL5 */

	init_gpio();   // for system dependent part
#if defined(RTCONFIG_CONCURRENTREPEATER)
#ifdef RPAC68U
	set_led(LED_BLINK_SLOW, LED_BLINK_SLOW);
#else
	start_led_monitor();
	nvram_set_int("led_status", LED_BOOTING);
#endif
#endif
#ifdef RTCONFIG_SWMODE_SWITCH
	init_swmode(); // need to check after gpio initized
#endif	/* RTCONFIG_SWMODE_SWITCH */
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
	init_others();
#endif

	init_switch(); // for system dependent part

#if defined(RTCONFIG_SOC_IPQ40XX)
#if defined(RTCONFIG_BLINK_LED)
	modprobe("bled");
#endif
	init_nvram3();
#endif

#if defined(RTCONFIG_QCA)
	set_uuid();
#if defined(RTCONFIG_TEST_BOARDDATA_FILE)
	start_jffs2();
#endif
#endif
#if defined(RTCONFIG_BCM_7114)
	mkdir("/tmp/media", 0777);
	symlink("/jffs", "/tmp/media/nand");
#endif

	init_wl(); // for system dependent part
	klogctl(8, NULL, nvram_get_int("console_loglevel"));

	// Enable rate report for tc classes (QoS stats)
#ifdef HND_ROUTER
	f_write_string("/sys/module/sch_htb/parameters/htb_rate_est", "1", 0, 0);
#endif
	setup_conntrack();
	setup_timezone();
	if (!noconsole) xstart("console");

#ifdef RTCONFIG_REALTEK
{
	extern void usb3_enable(int en);
	if (nvram_get("usb_usb3")) {
		usb3_enable(atoi(nvram_get("usb_usb3")));
	}
}
#endif

#ifdef RTCONFIG_BCMFA
	chk_etfa();
#endif

#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	start_thermald();
#endif
	set_power_save_mode();
#if defined(RTCONFIG_BT_CONN)
#if !defined(MAPAC3000) && !defined(RTCONFIG_REALTEK) && !defined(RTCONFIG_ALPINE)
	// initialize BT
	{ /* tmp */
	uint32_t bt_reset, get_value;
#if defined(MAPAC1300) || defined(VZWAC1300)
	bt_reset = 2;
#elif defined(MAPAC1750)
	bt_reset = 8;
#elif defined(MAPAC2200)
	bt_reset = 48;
#elif defined(BLUECAVE)
	bt_reset = 43;
#else
#error NEED bt_reset defined
#endif
	gpio_dir(bt_reset, GPIO_DIR_OUT);
	set_gpio(bt_reset, 1);
	get_value=get_gpio(bt_reset);
	_dprintf("bt reset value1: %d.\n", get_value);
	set_gpio(bt_reset, 0);
	sleep(1);
	get_value=get_gpio(bt_reset);
	_dprintf("bt reset value2: %d.\n", get_value);
	set_gpio(bt_reset, 1);
	get_value=get_gpio(bt_reset);
	_dprintf("bt reset value3: %d.\n", get_value);
//	system("bccmd -t bcsp -d /dev/ttyQHS0 psload -r /etc/3000000.psr");
//	system("hciattach -n -s 115200 /dev/ttyQHS0 bcsp 3000000 &");
	}
#endif
#endif	/* RTCONFIG_BT_CONN */
#if defined(MAPAC3000) /*temp*/
	system("bccmd -t bcsp -d /dev/ttyQHS0 psload -r /etc/3000000.psr");
	system("hciattach -n -s 115200 /dev/ttyQHS0 bcsp 3000000 &");
#endif
#if defined(MAPAC2200)
	nvram_unset("dpdt_ant");
#endif
}

#if defined(RTCONFIG_TEMPROOTFS)
static void sysinit2(void)
{
	static int noconsole = 0;
	static const time_t tm = 0;
	int i;

	MOUNT("proc", "/proc", "proc", 0, NULL);
	MOUNT("tmpfs", "/tmp", "tmpfs", 0, NULL);

#ifdef LINUX26
	MOUNT("sysfs", "/sys", "sysfs", MS_MGC_VAL, NULL);
	MOUNT("devpts", "/dev/pts", "devpts", MS_MGC_VAL, NULL);
#endif

	if (console_init()) noconsole = 1;

	stime(&tm);

	f_write("/tmp/settings", NULL, 0, 0, 0644);
	symlink("/proc/mounts", "/etc/mtab");

	set_action(ACT_IDLE);

	if (!noconsole) {
		printf("\n\nHit ENTER for new console...\n\n");
		run_shell(1, 0);
	}

	for (i = 0; i < sizeof(fatalsigs) / sizeof(fatalsigs[0]); i++) {
		signal(fatalsigs[i], handle_fatalsigs);
	}
	signal(SIGCHLD, handle_reap);

	if (!noconsole) xstart("console");
}
#else
static inline void sysinit2(void) { }
#endif

void
Alarm_Led(void) {
	while(1) {
#if defined(RTCONFIG_CONCURRENTREPEATER)
#if defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK)
		setAllLedOff();
		sleep(1);
		setAllLedOn();
		sleep(1);
#endif
#else
		led_control(LED_POWER, LED_OFF);
		sleep(1);
		led_control(LED_POWER, LED_ON);
		sleep(1);
#endif
	}
}
void config_format_compatibility_handler(void)
{
	adjust_merlin_config();
	adjust_url_urlelist();
	adjust_ddns_config();
	adjust_access_restrict_config();
#if defined(RTCONFIG_VPN_FUSION)
	adjust_vpnc_config();
#endif
#if defined(RTCONFIG_NOTIFICATION_CENTER)
	force_off_push_msg();
#endif
}

static void
run_rc_local(void)
{
	char *cmd;
	struct stat tmp_stat;

	if ((cmd = nvram_get("rclocal")) != NULL &&
		stat(cmd, &tmp_stat) == 0) {
		system(cmd);
	}
}

int init_main(int argc, char *argv[])
{
	int state, i;
	sigset_t sigset;
	siginfo_t info;
	int rc_check, dev_check, boot_check; //Power on/off test
	int boot_fail, dev_fail, dev_fail_count, total_fail_check = 0;
	int check_delay, delay;
	char reboot_log[128], dev_log[128];
	int ret;
#if defined(RTCONFIG_SOC_IPQ40XX)
	char argva[128];
#endif
#if 0
#ifdef RTCONFIG_USB_MODEM
	int modem_unit;
	char tmp2[100], prefix2[32];
	char env_unit[32];
#endif
#endif

#ifdef RT4GAC68U
	set_pwr_modem(1);
#endif

#if defined(RTCONFIG_TEMPROOTFS)
	if (argc >= 2 && !strcmp(argv[1], "reboot")) {
		state = SIGTERM;
		sysinit2();
	} else
#endif
	{
		sysinit();

#ifdef RTCONFIG_USB
		// let usb host & modem drivers be inserted early.
		add_usb_host_module();
#if defined(RTCONFIG_USB_MODEM) && !defined(RTCONFIG_SOC_IPQ40XX)
		add_usb_modem_modules();
#endif
#endif
#ifdef RTCONFIG_LANTIQ
		init_others_defer();
#endif

		config_format_compatibility_handler();

		sigemptyset(&sigset);
		for (i = 0; i < sizeof(initsigs) / sizeof(initsigs[0]); i++) {
			sigaddset(&sigset, initsigs[i]);
		}
		sigprocmask(SIG_BLOCK, &sigset, NULL);

#ifndef RTCONFIG_NVRAM_FILE
#if !defined(RTCONFIG_TEST_BOARDDATA_FILE)
		start_jffs2();
#endif
#endif
#ifdef RTCONFIG_NVRAM_ENCRYPT
		init_enc_nvram();
#endif
		// 1. passwd don't need to write too early.
		// 2. the PMS table is existed at /jffs.
		setup_passwd();

#if defined(RTCONFIG_PSISTLOG)
		//set soft link for some app such as 3ginfo.sh
		char *syslog[]  = { "/tmp/syslog.log", "/tmp/syslog.log-1" };
		char *filename;
		for(i = 0; i < 2; i++)
		{
			filename = get_syslog_fname(i);
			if(strcmp(filename, syslog[i]) != 0)
				eval("ln", "-s", filename, syslog[i]);
		}
#endif

#ifdef RTN65U
		asm1042_upgrade(1);	// check whether upgrade firmware of ASM1042
#endif

		run_custom_script("init-start", NULL);
		setup_passwd();		// Re-apply now that jffs is up, in case of custom configs
		use_custom_config("fstab", "/etc/fstab");
		run_postconf("fstab", "/etc/fstab");
		state = SIGUSR2;	/* START */
	}

	for (;;) {
//		TRACE_PT("main loop signal/state=%d\n", state);

		switch (state) {
		case SIGUSR1:		/* USER1: service handler */
			handle_notifications();
#ifdef RTCONFIG_16M_RAM_SFP
			force_free_caches();
#endif
			if (g_reboot) {
				/* If handle_notifications() run reboot rc_services,
				 * fall through to below statments to reboot.
				 */
				state = SIGTERM;
			} else
				break;

		case SIGHUP:		/* RESTART */
		case SIGINT:		/* STOP */
		case SIGQUIT:		/* HALT */
		case SIGTERM:		/* REBOOT */
			if (state == SIGTERM || state == SIGQUIT)
				if (shutdown_start("init", "init_main", (state == SIGTERM) ? "reboot" : "halt") != 0)
					break;

#if defined(RTCONFIG_USB_MODEM) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS))
		_dprintf("modem data: save the data during the signal %d.\n", state);
		eval("/usr/sbin/modem_status.sh", "bytes+");
#endif

#ifdef RTCONFIG_DSL_TCLINUX
			eval("req_dsl_drv", "reboot");
#endif
			stop_services();

			if (!g_reboot)
				stop_wan();

			stop_lan();
			stop_vlan();
			stop_logger();

#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
			_dprintf("sync during rebooting...\n");
			sync();
#endif

			if ((state == SIGTERM /* REBOOT */) ||
				(state == SIGQUIT /* HALT */)) {
#ifdef RTCONFIG_USB
				remove_storage_main(1);
				if (!g_reboot) {
#if !(defined(RTN56UB1) || defined(RTN56UB2) || defined(RTAC1200GA1) || defined(RTAC1200GU))
#ifndef RTCONFIG_ERPTEST
					stop_usb();
#else
					stop_usb(0);
#endif
#ifndef RTCONFIG_NO_USBPORT
					stop_usbled();
#endif
#endif
				}
#endif
#if defined(RTCONFIG_QCA) && defined(RTCONFIG_QCA8033)
				stop_lan_port();
				start_lan_port(5);
#endif
				shutdn(state == SIGTERM /* REBOOT */);
				sync(); sync(); sync();
				exit(0);
			}
			if (state == SIGINT /* STOP */) {
				break;
			}

			// SIGHUP (RESTART) falls through

		case SIGUSR2:		/* START */
			nvram_unset("shutdown_started");
			start_logger();
#if defined(RTCONFIG_QCA)
			logmessage("INIT", "firmware version: %s_%s_%s\n", nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno"));
#endif
#if defined(RTCONFIG_BT_CONN)
			system("hciconfig hci0 up");
#endif
#ifdef RTL_WTDOG
			// stop watchdog first
			dbG("Stop RTK watchdog\n");
			stop_rtl_watchdog();

			// resume watchdog
			dbG("Resume RTK watchdog\n");
			start_rtl_watchdog();
#endif
#ifdef RTCONFIG_BT_CONN
#ifdef RTCONFIG_REALTEK
			gen_rtlbt_fw_config();
			eval("rtk_hciattach","-n","-s","115200","/dev/ttyS1","rtk_h5","115200");
#endif
#endif
			if (nvram_match("Ate_power_on_off_enable", "1")) {
				Ate_on_off_led_start();
				rc_check = nvram_get_int("Ate_rc_check");
				boot_check = nvram_get_int("Ate_boot_check");
				boot_fail = nvram_get_int("Ate_boot_fail");
				total_fail_check = nvram_get_int("Ate_total_fail_check");
dbG("rc/boot/total chk= %d/%d/%d, boot/total fail= %d/%d\n", rc_check,boot_check,total_fail_check, boot_fail,total_fail_check);
				logmessage("ATE", "rc/boot/total chk= %d/%d/%d, boot/total fail= %d/%d\n", rc_check,boot_check,total_fail_check, boot_fail,total_fail_check);

				if ( nvram_get_int("Ate_FW_err") >= nvram_get_int("Ate_fw_fail")) {
					ate_commit_bootlog("6");
					dbG("*** Bootloader read FW fail: %d ***\n", nvram_get_int("Ate_FW_err"));
					logmessage("ATE", "*** Bootloader read FW fail: %d ***\n", nvram_get_int("Ate_FW_err"));
					Alarm_Led();
				}

				if (rc_check != boot_check) {
					if (nvram_get("Ate_reboot_log")==NULL)
						sprintf(reboot_log, "%d,", rc_check);
					else
						sprintf(reboot_log, "%s%d,", nvram_get("Ate_reboot_log"), rc_check);
					nvram_set("Ate_reboot_log", reboot_log);
					nvram_set_int("Ate_boot_fail", ++boot_fail);
					nvram_set_int("Ate_total_fail_check", ++total_fail_check);
dbg("boot/continue fail= %d/%d\n", nvram_get_int("Ate_boot_fail"),nvram_get_int("Ate_continue_fail"));
					if (boot_fail >= nvram_get_int("Ate_continue_fail")) { //Show boot fail!!
						ate_commit_bootlog("4");
						dbG("*** boot fail continuelly: %s ***\n", reboot_log);
						logmessage("ATE", "*** boot fail continuelly: %s ***\n", reboot_log);
#ifdef RTL_WTDOG
						stop_rtl_watchdog(); // Stop RTK watchdog, because ASUS watchdog process is not be start up.
#endif
						Ate_on_off_led_fail_loop();	// keep loop in this function
					}
					else
					dbg("rc OK\n");
				}

				if (total_fail_check == nvram_get_int("Ate_total_fail")) {
					ate_commit_bootlog("5");
					dbG("*** Total reboot fail: %d times!!!! ***\n", total_fail_check);
					logmessage("ATE", "*** Total reboot fail: %d times!!!! ***\n", total_fail_check);
#ifdef RTL_WTDOG
					stop_rtl_watchdog(); // Stop RTK watchdog, because ASUS watchdog process is not be start up.
#endif
					Ate_on_off_led_fail_loop();	// keep loop in this function
				}

				rc_check++;
				nvram_set_int("Ate_rc_check", rc_check);
				nvram_commit();
				dbG("*** Start rc: %d\n",rc_check);
				logmessage("ATE", "*** Start rc: %d\n",rc_check);
			}
#ifdef RTCONFIG_IPV6
			if ( !(ipv6_enabled() && is_routing_enabled()) )
				f_write_string("/proc/sys/net/ipv6/conf/all/disable_ipv6", "1", 0, 0);
#endif

#if defined(RTCONFIG_RALINK_MT7628)
			start_vlan();
			config_switch();
#else
#if !defined(HND_ROUTER) && !defined(BLUECAVE)
			start_vlan();
#endif
#endif
#ifdef RTCONFIG_DSL
			start_dsl();
#endif
			start_lan();


#ifdef RTCONFIG_QTN
			start_qtn();
			sleep(5);
#endif
#ifdef RTCONFIG_BCMARM
			misc_ioctrl();
#endif
			start_services();
#ifdef CONFIG_BCMWL5
			if (restore_defaults_g)
			{
				restore_defaults_g = 0;
				restart_wireless();
#ifdef RTCONFIG_QTN
				/* add here since tweak wireless-related flow */
				/* RT-AC87U#818, RT-AC87U#819 */
				start_qtn_monitor();
#endif
			}
			else
#endif
			{
				start_wl();
				lanaccess_wl();
			}
#if defined(HND_ROUTER) || defined(BLUECAVE)
			start_vlan();
#endif
			start_wan();
#ifdef HND_ROUTER
			if (is_router_mode()) start_mcpd_proxy();
#endif
#ifdef RTCONFIG_RGBLED
			if(!nvram_match("sb_flash_update", "1"))
			{
				doSystem("sb_flash_update");
				nvram_set("sb_flash_update", "1");
				nvram_commit();
			}
#endif

#if defined(MAPAC2200)
			{
				char *dpdt_ant[] = {"dpdt_ant", NULL};
				pid_t pid;
				_eval(dpdt_ant, ">>/dev/null", 0, &pid);
			}
#endif

#if defined(MAPAC1300) || defined(VZWAC1300)
			{
				char *thermal_txpwr[] = {"thermal_txpwr", NULL};
				pid_t pid;
				_eval(thermal_txpwr, ">>/dev/null", 0, &pid);
			}
#endif

#if defined(RTCONFIG_DETWAN)
			if(nvram_safe_get("wan0_ifname")[0] == '\0' && sw_mode() == SW_MODE_ROUTER)
			{ // block some broadcast in default
				extern void detwan_set_net_block(int add);
				detwan_set_net_block(1);
			}
#endif


#ifdef RTCONFIG_QTN	/* AP and Repeater mode, workaround to infosvr, RT-AC87U bug#38, bug#44, bug#46 */
			if (sw_mode() == SW_MODE_REPEATER ||
				sw_mode() == SW_MODE_AP)
			{
				eval("ifconfig", "br0:0", "169.254.39.3", "netmask", "255.255.255.0");
				if (*nvram_safe_get("QTN_RPC_CLIENT"))
					eval("ifconfig", "br0:0", nvram_safe_get("QTN_RPC_CLIENT"), "netmask", "255.255.255.0");
				else
					eval("ifconfig", "br0:0", "169.254.39.1", "netmask", "255.255.255.0");
			}
#endif

#ifdef REMOVE
// TODO: is it a special case need to handle?
			if (wds_enable()) {
				/* Restart NAS one more time - for some reason without
				 * this the new driver doesn't always bring WDS up.
				 */
				stop_nas();
				start_nas();
			}
#endif
#ifdef RTCONFIG_RALINK
			if(nvram_match("Ate_wan_to_lan", "1"))
			{
				printf("\n\n## ATE mode:set WAN to LAN... ##\n\n");
				set_wantolan();
#if defined (RTCONFIG_WLMODULE_MT7615E_AP)
				doSystem("iwpriv %s set hw_nat_register=%d", get_wifname(0), 0);
#ifdef RTCONFIG_HAS_5G
				doSystem("iwpriv %s set hw_nat_register=%d", get_wifname(1), 0);
#endif
#endif
				modprobe_r("hw_nat");
				modprobe("hw_nat");
#if defined (RTCONFIG_WLMODULE_MT7615E_AP)
				doSystem("iwpriv %s set hw_nat_register=%d", get_wifname(0), 1);
#ifdef RTCONFIG_HAS_5G
				doSystem("iwpriv %s set hw_nat_register=%d", get_wifname(1), 1);
#endif
#endif
#if defined (RTCONFIG_WLMODULE_MT7615E_AP)
				doSystem("iwpriv %s set LanNatSpeedUpEn=%d", get_wifname(0), 1);
#ifdef RTCONFIG_HAS_5G
				doSystem("iwpriv %s set LanNatSpeedUpEn=%d", get_wifname(1), 1);
#endif
#endif
				stop_wanduck();
				killall_tk("udhcpc");
			}
#endif

			if (nvram_match("Ate_power_on_off_enable", "3")|| //Show alert light
				nvram_match("Ate_power_on_off_enable", "4")||
				nvram_match("Ate_power_on_off_enable", "5")  ) {
				start_telnetd();
				Ate_on_off_led_fail_loop();	// keep loop in this function
			}

#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
			if(!factory_debug()) {
				check_4366_dummy();
				sleep(1);
				check_4366_fabid();
			}
#endif

			//For 66U normal boot & check device
			if (((get_model()==MODEL_RTN66U) || (get_model()==MODEL_RTAC66U))
			&& nvram_match("Ate_power_on_off_enable", "0")) {
			    ate_dev_status();
			    if (nvram_get_int("dev_fail_reboot")!=0) {
				if (strchr(nvram_get("Ate_dev_status"), 'X')) {
					dev_fail_count = nvram_get_int("dev_fail_count");
					dev_fail_count++;
					if (dev_fail_count < nvram_get_int("dev_fail_reboot")) {
						nvram_set_int("dev_fail_count", dev_fail_count);
						nvram_commit();
						dbG("device failed %d times, reboot...\n",dev_fail_count);
						kill(1, SIGTERM);
					}
					else {
						nvram_set("dev_fail_count", "0");
						nvram_commit();
					}
				}
				else {
					if (!nvram_match("dev_fail_count", "0")) {
						nvram_set("dev_fail_count", "0");
						nvram_commit();
					}
				}
			    }
			}

#ifdef RTCONFIG_USB
#if defined(RTCONFIG_BT_CONN)	// usb blietooth device "hci0" need to be checked before Ate_power_on_off for checking its init state
#ifdef RTCONFIG_USBRESET
			set_pwr_usb(1);
#else
#if defined(RTCONFIG_USB_XHCI) && !defined(RTCONFIG_XHCIMODE)
			if (nvram_get_int("usb_usb3") == 0 &&
				nvram_get("pwr_usb_gpio") &&
				(nvram_get_int("pwr_usb_gpio") & 0xFF) != 0xFF)
			{
				_dprintf("USB2: reset USB power to call up the device on the USB 3.0 port.\n");
				set_pwr_usb(0);
				sleep(2);
				set_pwr_usb(1);
			}
#endif
#endif // RTCONFIG_USBRESET

			int fd = -1;
			fd = file_lock("usb");  // hold off automount processing
#if defined(RTCONFIG_SOC_IPQ40XX)
			start_usb(1);
#else
			start_usb(0);
#endif
#ifndef RTCONFIG_NO_USBPORT
			start_usbled();
#endif
			file_unlock(fd);	// allow to process usb hotplug events
#endif	/* RTCONFIG_BT_CONN */
#endif	/* RTCONFIG_USB */


			if (nvram_match("Ate_power_on_off_enable", "1")) {
				dev_check = nvram_get_int("Ate_dev_check");
				dev_fail = nvram_get_int("Ate_dev_fail");

				check_delay = 0;
				while(check_delay < nvram_get_int("Ate_check_delay")){
					sleep(1);
					++check_delay;
				}
#ifdef RTCONFIG_LANTIQ
				while(nvram_get_int("wave_ready") != 1){
					sleep(5);
					_dprintf("waiting 2G, 5G interface UP\n");
				}
#endif
				ate_dev_status();

				if (strchr(nvram_get("Ate_dev_status"), 'X')) {
					nvram_set_int("Ate_dev_fail", ++dev_fail);
					nvram_set_int("Ate_total_fail_check", ++total_fail_check);
					if (nvram_get("Ate_dev_log")==NULL)
						sprintf(dev_log, "%d,", dev_check);
					else
						sprintf(dev_log, "%s%d,", nvram_get("Ate_dev_log"), dev_check);
					nvram_set("Ate_dev_log", dev_log);
					dbG("dev Fail at %d, status= %s\n", dev_check, nvram_get("Ate_dev_status"));
					dbG("dev/total fail= %d/%d\n", dev_fail, total_fail_check);
					if (dev_fail== nvram_get_int("Ate_continue_fail")) {
						ate_commit_bootlog("3");
						dbG("*** dev fail continuelly: %s ***\n", dev_log);
						logmessage("ATE", "*** dev fail continuelly: %s ***\n", dev_log);
						Ate_on_off_led_fail_loop();	// keep loop in this function
					}
				}
				else {
					dev_check++;
					nvram_set_int("Ate_dev_check", dev_check);
					nvram_set("Ate_dev_fail", "0");
					dbG("*** dev Up: %d\n", dev_check);
				}

				boot_check = nvram_get_int("Ate_boot_check");
				boot_check++;
				nvram_set_int("Ate_boot_check", boot_check);
				nvram_set_int("Ate_rc_check", boot_check);
				nvram_set("Ate_boot_fail", "0");

				if (boot_check < nvram_get_int("Ate_reboot_count")) {
					delay = nvram_get_int("Ate_reboot_delay");
#ifdef HND_ROUTER
					if (delay > 2) {
						dbG("fix delay %d as %d\n", delay, delay % 3);
						delay = delay % 3;
					}
#endif
					nvram_commit();
					dbG("System boot up %d times,  delay %d secs & reboot...\n", boot_check, delay);
					logmessage("ATE", "System boot up %d times,  delay %d secs & reboot...\n", boot_check, delay);

#if defined(RTCONFIG_SOC_IPQ40XX)
					snprintf(argva,128,"/sbin/delay_exec %s",nvram_get("Ate_reboot_delay"));
					doSystem(argva);
#else
#if 0
					sleep(delay);
#else
					i = 1;
					while(i <= delay){
						sleep(1);

						++i;
					}
#endif
#endif

#ifdef RTCONFIG_QSR10G
					if(rc_check == 1){
						ate_run_in_preconfig();
					}
#endif
					kill(1, SIGTERM);
				}
				else {
					dbG("System boot up success %d times\n", boot_check);
					logmessage("ATE", "System boot up success %d times\n", boot_check);
					ate_commit_bootlog("2");
					Ate_on_off_led_success();
#if defined(RTCONFIG_RALINK) && defined(RTN65U)
					ate_run_in();
#endif
#ifdef RTCONFIG_QSR10G
					ate_run_in();
#endif
					eval("arpstorm");
				}
			}
			else {
				ate_run_arpstrom();
			}

#ifdef RTCONFIG_USB
#if ! defined(RTCONFIG_BT_CONN)	// usb bluetooth "hci0" is init in the front
#ifdef RTCONFIG_USBRESET
			set_pwr_usb(1);
#else
#if defined(RTCONFIG_USB_XHCI) && !defined(RTCONFIG_XHCIMODE)
			if (nvram_get_int("usb_usb3") == 0 &&
				nvram_get("pwr_usb_gpio") &&
				(nvram_get_int("pwr_usb_gpio") & 0xFF) != 0xFF)
			{
				_dprintf("USB2: reset USB power to call up the device on the USB 3.0 port.\n");
				set_pwr_usb(0);
				sleep(2);
				set_pwr_usb(1);
			}
#endif
#endif // RTCONFIG_USBRESET

			int fd = -1;
			fd = file_lock("usb");  // hold off automount processing
#if defined(RTCONFIG_SOC_IPQ40XX)
			start_usb(1);
#else
			start_usb(0);
#endif
#ifndef RTCONFIG_NO_USBPORT
			start_usbled();
#endif
			file_unlock(fd);	// allow to process usb hotplug events
#endif	/* RTCONFIG_BT_CONN */

			/*
			 * On RESTART some partitions can stay mounted if they are busy at the moment.
			 * In that case USB drivers won't unload, and hotplug won't kick off again to
			 * remount those drives that actually got unmounted. Make sure to remount ALL
			 * partitions here by simulating hotplug event.
			 */
			if (state == SIGHUP /* RESTART */)
				add_remove_usbhost("-1", 1);

#ifdef RTCONFIG_USB_PRINTER
			start_usblpsrv();
#endif
#ifdef RTCONFIG_BCMARM
			set_pwr_usb(1);
#endif

#endif // RTCONFIG_USB

			run_rc_local();

#ifndef RTCONFIG_LANTIQ
			nvram_set("success_start_service", "1");
			force_free_caches();
#endif

#ifdef RTCONFIG_AMAS
			nvram_set("start_service_ready", "1");
#endif

#ifdef RTCONFIG_REALTEK
			if (nvram_match("Ate_power_on_off_enable", "0")) {	/* avoid run in test to let all led off */
				if (sw_mode() == SW_MODE_REPEATER ||
				    (sw_mode() == SW_MODE_AP && nvram_get_int("wlc_psta") == 1)) // Repeater and Media bridge mode
					set_led(LED_OFF_ALL, LED_OFF_ALL);
				else if (sw_mode() == SW_MODE_AP) { // AP mode
					char word[8], *next, tmp[16];
					int wl0_stage = LED_OFF_ALL, wl1_stage = LED_OFF_ALL;
					foreach (word, nvram_safe_get("wl_ifnames"), next) {
						if (nvram_match(strcat_r(word, "_radio", tmp), "1")) {
							if (strstr(word, "0"))
								wl0_stage = LED_ON_ALL;
							else if (strstr(word, "1"))
								wl1_stage = LED_ON_ALL;
						}
					}
					set_led(wl0_stage, wl1_stage);
				}
			}
			else if (nvram_match("Ate_power_on_off_enable", "2"))
				setAllLedOn();
#endif

#if defined(RTCONFIG_CONCURRENTREPEATER)
			if (sw_mode() == SW_MODE_REPEATER
#ifdef RTCONFIG_REALTEK
				|| mediabridge_mode()
#endif
			)
				nvram_set_int("led_status", LED_BOOTED);
			else if (sw_mode() == SW_MODE_AP)
				nvram_set_int("led_status", LED_BOOTED_APMODE);
#endif
			if (nvram_match("commit_test", "1")) {
				int x=0;
				while(1) {
					x++;
					dbG("Nvram commit: %d\n", x);
					nvram_set_int("commit_cut", x);
					nvram_commit();
					if (nvram_get_int("commit_cut")!=x) {
						TRACE_PT("\n\n======== NVRAM Commit Failed at: %d =========\n\n", x);
						break;
					}
				}
			}

			break;
		}

		if (!(g_reboot || g_upgrade) && !nvram_get_int("asus_mfg")) {
			chld_reap(0);	/* Periodically reap zombies. */
			check_services();
		}

		do {
		ret = sigwaitinfo(&sigset, &info);
		} while (ret == -1);
		state = info.si_signo;
#if !(defined(HND_ROUTER) && defined(RTCONFIG_HNDMFG))
		TRACE_PT("recv signal %d from pid [%u%s%s] (from %s)\n", info.si_signo, info.si_pid, ((info.si_code <= 0) ? ":" : ""), ((info.si_code <= 0) ? get_process_name_by_pid(info.si_pid) : ""), (info.si_code <= 0) ? "user" : "kernel");
#endif
#if defined(RTCONFIG_BCMARM) && !defined(HND_ROUTER)
		/* free pagecache */
		if (nvram_get_int("drop_caches"))
			f_write_string("/proc/sys/vm/drop_caches", "1", 0, 0);
#endif
#if defined(RTCONFIG_SOC_IPQ40XX)
		/* free pagecache */
		f_write_string("/proc/sys/vm/drop_caches", "3", 0, 0);
#endif
	}

	return 0;
}

int reboothalt_main(int argc, char *argv[])
{
	int reboot = (strstr(argv[0], "reboot") != NULL);
	int def_reset_wait = 20;
	int result = shutdown_start("rc", "reboothalt_main", argv[0]);

	if (result != 0)
		return result;

	_dprintf(reboot ? "Rebooting..." : "Shutting down...");
	kill(1, reboot ? SIGTERM : SIGQUIT);

#if defined(RTN14U) || defined(RTN65U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTCONFIG_QCA) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U) || defined(RTN56UB2) || defined(RTAC85U) || defined(RTN800HP)
	def_reset_wait = 50;
#endif

	int wait = nvram_get_int("reset_wait") ? : def_reset_wait;
	/* In the case we're hung, we'll get stuck and never actually reboot.
	 * The only way out is to pull power.
	 * So after 'reset_wait' seconds (default: 20), forcibly crash & restart.
	 */
	if (fork() == 0) {
		if ((wait < 10) || (wait > 120)) wait = 10;

		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(wait);
		_dprintf("Still running... Doing machine reset.\n");
#ifdef RTCONFIG_USB
		remove_usb_module();
#endif
		f_write("/proc/sysrq-trigger", "s", 1, 0 , 0); /* sync disks */
		sleep(1);
		f_write("/proc/sysrq-trigger", "b", 1, 0 , 0); /* machine reset */
	}

	return 0;
}

