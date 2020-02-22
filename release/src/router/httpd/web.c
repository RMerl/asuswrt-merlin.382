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
 */
/*
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2001, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 * $Id: web_ex.c,v 1.4 2007/04/09 12:01:50 shinjung Exp $
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <dirent.h>
#include <proto/ethernet.h>   //add by Viz 2010.08
#include <net/route.h>
#include <sys/ioctl.h>
#include <math.h>	//for ceil()
#include <sys/sysinfo.h>

#include <typedefs.h>
#include <bcmutils.h>
#include <shutils.h>
#include <bcmnvram.h>
#include <bcmnvram_f.h>
#include <common.h>
#include <shared.h>
#include <rtstate.h>
#include <timezone.h>
#include <nvram_config.h>

#ifdef RTCONFIG_FANCTRL
#include <wlutils.h>
#endif

#ifdef RTCONFIG_DSL
#include <web-dsl.h>
#include <web-dsl-upg.h>
#endif

#ifdef RTCONFIG_NOTIFICATION_CENTER
#include <libnt.h>
#include <nt_eInfo.h>
#endif

#ifdef RTCONFIG_USB
#include <usb_info.h>
#include <disk_io_tools.h>
#include <disk_initial.h>
#include <disk_share.h>

#ifdef RTCONFIG_NOTIFICATION_CENTER
#include <libnt.h>
#include <nt_eInfo.h>
#endif

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
#include <PMS_DBAPIs.h>
#endif

#ifdef RTCONFIG_NETOOL
#include <netool.h>
#endif

#if defined(RTCONFIG_VPN_FUSION)
#include <vpnc_fusion.h>
#endif

#include <apps.h>

#if defined(RTCONFIG_USB_SMS_MODEM) && !defined(RTCONFIG_USB_MULTIMODEM)
#include "libsmspdu.h"
#endif
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#endif
#include "initial_web_hook.h"
//#endif

#ifdef RTCONFIG_OPENVPN
#include "openvpn_options.h"
#include "openvpn_config.h"
#endif

#include <net/if.h>
#include <linux/sockios.h>
#include <networkmap.h> //2011.03 Yau add for new networkmap 2017.03 Rawny add
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sysinfo.h>
#include <httpd.h>
#include <json.h>

#include "sysinfo.h"
#include "data_arrays.h"

#ifdef RTCONFIG_QTN
#include "web-qtn.h"
#endif

#ifdef RTCONFIG_QSR10G
#include "web-qsr10g.h"
#endif

#if defined(RTCONFIG_BWDPI)
#include "bwdpi.h"
#include "sqlite3.h"
#include "bwdpi_sqlite.h"
#endif

#ifdef RTCONFIG_TRAFFIC_LIMITER
#include "traffic_limiter.h"
#endif

#ifdef RTCONFIG_REALTEK
#include <realtek.h>
#endif

#ifdef RTCONFIG_QCA_PLC_UTILS
#include <plc_utils.h>
#endif

#ifdef RTCONFIG_CFGSYNC
#include <cfg_param.h>
#include <cfg_slavelist.h>
#include <cfg_wevent.h>
#include <cfg_event.h>
#include <cfg_lib.h>
#include <cfg_clientlist.h>
#include <cfg_onboarding.h>
#endif

#if defined(HND_ROUTER) && defined(RTCONFIG_VISUALIZATION)
#include "vis_gui.h"
#endif

#include <passwd.h>

static void do_jffs_file(char *url, FILE *stream);
static void do_jffsupload_cgi(char *url, FILE *stream);
static void do_jffsupload_post(char *url, FILE *stream, int len, char *boundary);

#if 0 // obsoleted, RTCONFIG_RGBLED
#include <aura_rgb.h>
#endif

#ifdef RTCONFIG_HTTPS
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/opensslconf.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define X509_getm_notBefore X509_get_notBefore
#define X509_getm_notAfter X509_get_notAfter
#endif
#ifdef RTCONFIG_LETSENCRYPT
#include <letsencrypt_config.h>
#endif
extern int do_ssl;
extern int ssl_stream_fd;
#endif

#if defined(RTCONFIG_AIHOME_TUNNEL)
#include <mastiff.h>
#endif

#if defined(RTCONFIG_AMAS)
#include <stdbool.h>
#endif

#include <iboxcom.h>

#ifdef RTCONFIG_CONNDIAG
#include <conn_diag-sql.h>
#endif


extern int ej_wl_sta_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#ifndef RTCONFIG_QTN
extern int ej_wl_sta_list_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#ifdef RTCONFIG_STAINFO
extern int ej_wl_stainfo_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_stainfo_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#if !defined(RTCONFIG_QTN) && !defined(RTCONFIG_ALPINE) && !defined(RTCONFIG_LANTIQ)
extern int ej_wl_stainfo_list_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#endif
extern int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv);
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ)
extern int ej_wl_control_channel(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_extent_channel(int eid, webs_t wp, int argc, char_t **argv);
#endif
extern int ej_get_wlstainfo_list(int eid, webs_t wp, int argc, char_t **argv);

#if defined(RTCONFIG_REALTEK) || defined(RTCONFIG_WIRELESSWAN)
extern int ej_SiteSurvey(int eid, webs_t wp, int argc, char_t **argv);
#endif

extern int ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_scan_5g_2(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv);
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
extern int ej_wl_channel_list_5g_20m(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g_40m(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_5g_80m(int eid, webs_t wp, int argc, char_t **argv);
#endif
extern int ej_wl_channel_list_5g_2(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_channel_list_60g(int eid, webs_t wp, int argc, char_t **argv);
#ifdef CONFIG_BCMWL5
extern int ej_wl_chanspecs_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_chanspecs_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_chanspecs_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_REALTEK)
extern int ej_wl_rssi_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_rssi_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_rssi_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G)
extern int ej_wl_rate_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_rate_5g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_rate_5g_2(int eid, webs_t wp, int argc, char_t **argv);
#endif
extern int ej_nat_accel_status(int eid, webs_t wp, int argc, char_t **argv);
#ifdef RTCONFIG_PROXYSTA
int ej_wl_auth_psta(int eid, webs_t wp, int argc, char_t **argv);
#endif

#if defined(RTCONFIG_USB) || defined(RTCONFIG_PERMISSION_MANAGEMENT)
void not_ej_initial_folder_var_file();
#endif

#ifdef RTCONFIG_IPV6
extern int ej_lan_ipv6_network(int eid, webs_t wp, int argc, char_t **argv);
#endif

extern int ej_get_default_reboot_time(int eid, webs_t wp, int argc, char_t **argv);

static int b64_decode( const char* str, unsigned char* space, int size );

extern void send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time, int logintry);

extern char *get_cgi_json(char *name, json_object *root);
extern char *safe_get_cgi_json(char *name, json_object *root);

extern int ej_generate_region(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_get_support_region_list(int eid, webs_t wp, int argc, char_t **argv);

void substr(char *dest, const char* src, unsigned int start, unsigned int cnt);
#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
extern  int get_wifi_probe_result(void);
extern char* get_encrypt_wifi_status(char *buffer, size_t size);
#endif

extern int upgrade_rc(char *action, char *autoreboot, char *reset, int wait);

extern void unescape(char *s);

void response_nvram_config(webs_t wp, char *config_name, json_object *res, json_object *root);

extern int get_lang_num();
extern int ej_get_iptvSettings(int eid, webs_t wp, int argc, char_t **argv);
extern int config_iptv_vlan(char *isp);

#if 0
static int nvram_check_and_set(char *name, char *value);
#endif
static int nvram_check_and_set_for_prefix(char *name, char *tmp, char *value);
#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)

#define nvram_default_safe_get(name) (nvram_default_get(name) ? : "")

/*
#define csprintf(fmt, args...) do{\
	FILE *cp = fopen("/dev/console", "w");\
	if (cp) {\
		fprintf(cp, fmt, ## args);\
		fclose(cp);\
	}\
}while (0)
*/
// 2008.08 magic }

#include <sys/mman.h>
typedef uint32_t __u32; //2008.08 magic
#ifndef	O_BINARY		/* should be define'd on __WIN32__ */
#define O_BINARY	0
#endif
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

/* #define sys_upgrade(image) eval("mtd-write", "-i", image, "-d", "linux"); */
#define sys_upload(image) eval("nvram", "restore", image)
#define sys_download(file) eval("nvram", "save", file)
#define sys_download_ap(file) eval("nvram", "save_ap", file);
#define sys_download_rp_2g(file) eval("nvram", "save_rp_2g", file);
#define sys_download_rp_5g(file) eval("nvram", "save_rp_5g", file);
#define sys_download_fb(file) eval("nvram", "fb_save", file);
#if defined(RTAC3200) || defined(RTAC5300) || defined(GTAC5300)
#define sys_download_rp_5g2(file) eval("nvram", "save_rp_5g2", file);
#endif
#ifdef RTCONFIG_LANTIQ
#define sys_default() { eval("ejusb", "-1"); notify_rc("resetdefault");} //   eval("mtd-erase", "-d", "nvram")
#else
#define sys_default() notify_rc("resetdefault"); //   eval("mtd-erase", "-d", "nvram")
#endif
#define sys_reboot() notify_rc("reboot");
#define sys_default_erase() notify_rc("resetdefault_erase");

#define PROFILE_HEADER 	"HDR1"
#ifdef RTCONFIG_DSL
#define PROFILE_HEADER_NEW	"N55U"
#else
#if RTCONFIG_QCA
#define PROFILE_HEADER_NEW	"AC55U"
#elif defined(RTCONFIG_LANTIQ)
#define PROFILE_HEADER_NEW	"BLUE"
#else
#define PROFILE_HEADER_NEW	"HDR2"
#endif
#endif
#define IH_MAGIC	0x27051956	/* Image Magic Number		*/

int count_sddev_mountpoint();

#define Ralink_WPS	1 //2009.01 magic

//char ibuf2[8192];

//static int ezc_error = 0;

#define ACTION_UPGRADE_OK   0
#define ACTION_UPGRADE_FAIL 1

int action;

char *serviceId;
#define MAX_GROUP_ITEM 10
#define MAX_GROUP_COUNT 300
#define MAX_LINE_SIZE 512
char *groupItem[MAX_GROUP_ITEM];
char urlcache[128];
char *next_host;
int delMap[MAX_GROUP_COUNT];
char SystemCmd[128];
char UserID[32]="";
char UserPass[32]="";
char ProductID[32]="";
#ifdef RTCONFIG_LANTIQ
int wave_app_flag=0;
#endif
extern int redirect;
extern int change_passwd;	// 2008.08 magic
extern int reget_passwd;	// 2008.08 magic
extern int skip_auth;

extern time_t login_timestamp; // the timestamp of the logined ip
extern time_t login_dt;
extern char login_url[128];
extern int login_error_status;
extern char cloud_file[256];

#ifdef RTCONFIG_LACP
#if defined(GTAC5300)
	#define LACP_PORT1		"LACP5"
	#define LACP_PORT2		"LACP6"
#else
	#define LACP_PORT1		"LACP1"
	#define LACP_PORT2		"LACP2"
#endif
#endif

#ifdef RTCONFIG_JFFS2USERICON
#define JFFS_USERICON		"/jffs/usericon/"
#endif

#ifdef RTCONFIG_IPSEC
#ifdef RTCONFIG_STRONGSWAN		
#define FILE_PATH_IPSEC_LOG	"/var/log/strongswan.charon.log"
#elif defined(RTCONFIG_QUICKSEC) 
#define FILE_PATH_IPSEC_LOG	"/tmp/quicksecpm.log"
#endif
#endif

#ifdef RTCONFIG_HTTPS
#define END_KEY "END RSA PRIVATE KEY"
#define END_CERT "END CERTIFICATE"
#endif

#ifdef RTCONFIG_CFGSYNC
#define CFG_JSON_FILE           "/jffs/cfg.json"
#define CFG_SERVER_PID		"/var/run/cfg_server.pid"
int cfg_changed = 0;
void save_changed_param(json_object *cfg_root, char *param);
int check_cfg_changed(json_object *root);
static int is_wireless_client(char *mac);
static int is_cfg_client_by_unique_mac(P_CM_CLIENT_TABLE p_client_tbl, char *mac);
static int is_cfg_client_by_sta_mac(P_CM_CLIENT_TABLE p_client_tbl, char *staMac);
static void gen_wired_client_info(P_CM_CLIENT_TABLE p_client_tbl, json_object *wiredClietListObj, json_object *wiredInfoObj);
static int check_wired_client_connected_node(char *nodeMac, char *wiredMac, json_object *wiredClietListObj, json_object *wiredInfoObj);
#endif

#define FB_FILE_WEB "/tmp/xdslissuestracking_web"

/* networkmap CACHE FILE */
#define NMP_CACHE_FILE	"/tmp/nmp_cache.js"
/* DEBUG DEFINE */
#define CLIENT_DEBUG	"/tmp/CLIENT_DEBUG"
/* DEBUG FUNCTION */
#define CLIENT_DPRINTF(fmt,args...) \
	if(f_exists(CLIENT_DEBUG) > 0) { \
		_dprintf("[CLIENT][%s]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

#define AVLBL_BW20 1
#define AVLBL_BW40 2
#define AVLBL_BW80 4
#define AVLBL_BW160 8

static int get_client_detail_info(struct json_object *clients, struct json_object *macArray, key_t shmkey);
static int get_custom_clientlist_info(struct json_object *json_object_ptr);
#define CLIENTAPILEVEL "1"

void not_ej_initial_folder_var_file();

#ifdef RTCONFIG_AMAS
static int is_re_node(char *client_mac, int check_path);
int get_amas_info(struct json_object *json_object_ptr);
#endif

#ifdef RTCONFIG_USB
static void insert_hook_func(webs_t wp, char *fname, char *param)
{
	if (!wp || !fname) {
		_dprintf("%s: invalid parameter (%p, %p, %p)\n", __func__, wp, fname, param);
		return;
	}

	if (!param)
		param = "";

	websWrite(wp, "<script>\n");
	websWrite(wp, "%s(%s);\n", fname, param);
	websWrite(wp, "</script>\n");
}
#endif

char *
rfctime(const time_t *timep)
{
	static char s[200];
	struct tm tm;

	if(setenv("TZ", nvram_safe_get_x("", "time_zone_x"), 1)==0)
		tzset();

	localtime_r(timep, &tm);
	strftime(s, sizeof(s), "%a, %d %b %Y %H:%M:%S %z", &tm);
	return s;
}

void
reltime(unsigned int seconds, char *cs)
{
#ifdef SHOWALL
	int days=0, hours=0, minutes=0;

	if (seconds > 60*60*24) {
		days = seconds / (60*60*24);
		seconds %= 60*60*24;
	}
	if (seconds > 60*60) {
		hours = seconds / (60*60);
		seconds %= 60*60;
	}
	if (seconds > 60) {
		minutes = seconds / 60;
		seconds %= 60;
	}
	sprintf(cs, "%d days, %d hours, %d minutes, %d seconds", days, hours, minutes, seconds);
#else
	sprintf(cs, "%d secs", seconds);
#endif
}

#if defined(RTCONFIG_BT_CONN)
void lyraQIS_extension()
{
	if (pids("bluetoothd") && nvram_match("x_Setting", "1")) {
		struct in_addr lan_addr, dhcp_start_addr, dhcp_end_addr;
		int wan_state, wan_sbstate, wan_auxstate;
		char *lan_ipaddr, *lan_netmask;
		char *wan_ipaddr, *wan_netmask;
		in_addr_t lan_mask, tmp_ip;


		wan_state = nvram_get_int("wan0_state_t");
		wan_sbstate = nvram_get_int("wan0_sbstate_t");
		wan_auxstate = nvram_get_int("wan0_auxstate_t");
		if (wan_state == 4 && wan_sbstate == 4 && wan_auxstate == 0)
		{
			lan_ipaddr = nvram_safe_get("lan_ipaddr");
			lan_netmask = nvram_safe_get("lan_netmask");

			wan_ipaddr = nvram_safe_get("wan0_ipaddr");
			wan_netmask = nvram_safe_get("wan0_netmask");

			if (inet_deconflict(lan_ipaddr, lan_netmask, wan_ipaddr, wan_netmask, &lan_addr)) {
				_dprintf("[IP conflict]: change lan IP to %s\n", inet_ntoa(lan_addr));
				logmessage("HTTPD", "[IP conflict] change lan IP to %s\n", inet_ntoa(lan_addr));
				lan_mask = inet_network(lan_netmask);
				tmp_ip = ntohl(lan_addr.s_addr);
				dhcp_start_addr.s_addr = htonl(tmp_ip + 1);
				dhcp_end_addr.s_addr = htonl((tmp_ip | ~lan_mask) & 0xfffffffe);

				nvram_set("lan_ipaddr", inet_ntoa(lan_addr));
				nvram_set("dhcp_start", inet_ntoa(dhcp_start_addr));
				nvram_set("dhcp_end", inet_ntoa(dhcp_end_addr));
			}
		}

	}
}
#endif

/******************************************************************************/
/*
 *	Redirect the user to another webs page
 */

//2008.08 magic{
void websRedirect(webs_t wp, char_t *url)
{
	char url_str[128];

	if(check_xss_blacklist(url, 1)){
		memset(url_str, 0, sizeof(url_str));
		strlcpy(url_str, indexpage, sizeof(url_str));
	}else
		strlcpy(url_str, url, sizeof(url_str));

	websWrite(wp, T("<html><head>\r\n"));

	if(strchr(url_str, '>') || strchr(url_str, '<'))
	{
		websWrite(wp,"<script>parent.location.href='/%s';</script>\n", indexpage);
	}
	else
	{
#ifdef RTCONFIG_HTTPS
		if(do_ssl){
			//websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=https://%s/%s\">\r\n"), gethost(), url);
			websWrite(wp,"<script>parent.location.href='/%s';</script>\n",url_str);
		}else
#endif
		{
			//websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=http://%s/%s\">\r\n"), gethost(), url);
			websWrite(wp,"<script>parent.location.href='/%s';</script>\n",url_str);
		}
	}

	websWrite(wp, T("<meta http-equiv=\"Content-Type\" content=\"text/html\">\r\n"));
	websWrite(wp, T("</head></html>\r\n"));

	websDone(wp, 200);
}

void websRedirect_iframe(webs_t wp, char_t *url)
{
	char url_str[32];

	if(check_xss_blacklist(url, 1)){
		memset(url_str, 0, sizeof(url_str));
		strlcpy(url_str, "index.asp", sizeof(url_str));
	}else
		strlcpy(url_str, url, sizeof(url_str));

	websWrite(wp, T("<html><head>\r\n"));

	if(strchr(url_str, '>') || strchr(url_str, '<'))
	{
		websWrite(wp,"<script>parent.location.href='/%s';</script>\n", indexpage);
	}
	else
	{
#ifdef RTCONFIG_HTTPS
		if(do_ssl){
			websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=https://%s/%s\">\r\n"), gethost(), url_str);
		}else
#endif
		{
			websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=http://%s/%s\">\r\n"), gethost(), url_str);
		}
	}

	websWrite(wp, T("<meta http-equiv=\"Content-Type\" content=\"text/html\">\r\n"));
	websWrite(wp, T("</head></html>\r\n"));

	websDone(wp, 200);
}
//2008.08 magic}

void sys_script(char *name)
{

     char scmd[64];

     snprintf(scmd, sizeof(scmd), "/tmp/%s", name);

     //handle special scirpt first
     if (strcmp(name,"syscmd.sh")==0)
     {
	   if (strcmp(SystemCmd, "")!=0)
	   {
		char CMD[256];
		snprintf(CMD, sizeof(CMD), "%s > /tmp/syscmd.log 2>&1 && echo 'XU6J03M6' >> /tmp/syscmd.log &\n", SystemCmd);
		system(CMD);
		strcpy(SystemCmd, ""); // decrease loading time.
	   }
	   else
	   {
	   	f_write_string("/tmp/syscmd.log", "", 0, 0);
	   }
     }
//#ifdef U2EC
     else if (strcmp(name,"eject-usb.sh")==0)
     {
		eval("rmstorage");
     }
#ifdef ASUS_DDNS //2007.03.22 Yau add
     else if (strcmp(name,"ddnsclient")==0)
     {
		notify_rc("restart_ddns");
     }
     else if (strstr(name,"asusddns_register") != NULL)
     {
		notify_rc("asusddns_reg_domain");
     }
#endif
     else if (strcmp(name, "leases.sh")==0 || strcmp(name, "dleases.sh")==0)
     {
		// do nothing
     }
     else if (strstr(scmd, " ") == 0) // no parameter, run script with eval
     {
		eval(scmd);
     }
     else system(scmd);
}

void websScan(char_t *str)
{
	unsigned int i, flag;
	char_t *v1, *v2, *v3, *sp=NULL;
	char_t groupid[64];
	char_t value[MAX_LINE_SIZE];
	char_t name[MAX_LINE_SIZE];

	v1 = strchr(str, '?');

	i = 0;
	flag = 0;

	while (v1!=NULL)
	{
	    v2 = strchr(v1+1, '=');
	    v3 = strchr(v1+1, '&');

// 2008.08 magic {
		if (v2 == NULL)
			break;
// 2008.08 magic }

	    if (v3!=NULL)
	    {
	       strncpy(value, v2+1, v3-v2-1);
	       value[v3-v2-1] = 0;
	    }
	    else
	    {
	       strcpy(value, v2+1);
	    }

	    strncpy(name, v1+1, v2-v1-1);
	    name[v2-v1-1] = 0;
	    /*printf("Value: %s %s\n", name, value);*/

	    if (v2 != NULL && ((sp = strchr(v1+1, ' ')) == NULL || (sp > v2)))
	    {
	       if (flag && strncmp(v1+1, groupid, strlen(groupid))==0)
	       {
		   delMap[i] = atoi(value);
		   /*printf("Del Scan : %x\n", delMap[i]);*/
		   if (delMap[i]==-1)  break;
		   i++;
	       }
	       else if (strncmp(v1+1,"group_id", 8)==0)
	       {
		   sprintf(groupid, "%s_s", value);
		   flag = 1;
	       }
	    }
	    v1 = strchr(v1+1, '&');
	}
	delMap[i] = -1;
	return;
}


void websApply(webs_t wp, char_t *url)
{
#ifdef TRANSLATE_ON_FLY
	do_ej (url, wp);
	websDone (wp, 200);
#else   // define TRANSLATE_ON_FLY

     FILE *fp;
     char buf[MAX_LINE_SIZE];

     fp = fopen(url, "r");

     if (fp==NULL) return;

     while (fgets(buf, sizeof(buf), fp))
     {
	websWrite(wp, buf);
     }

     websDone(wp, 200);
     fclose(fp);
#endif
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1
 * <% nvram_get("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get("undefined"); %> produces ""
 */
static int
ej_nvram_get(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;
//	char sid_dummy = "",
	int from_app = 0;
	char dec_passwd[4096];

	memset(dec_passwd, 0, sizeof(dec_passwd));

	from_app = check_user_agent(user_agent);

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (strcmp(name, "modem_spn") == 0 && !nvram_invmatch(name, ""))
		name = "modem_isp";

#ifdef HND_ROUTER
	if (!strcmp(name, "dhcp_hostnames")) {
		c = jffs_nvram_get(name);
		if (!c)
			c = "";
	} else
#endif
		c = nvram_safe_get(name);

	//if((ret = dec_nvram(name, c, dec_passwd)) == 1){
		//_dprintf("ej_nvram_get: name = %s, enc_value = %s\n", name, enc_passwd);
	//	c = dec_passwd;
	//}

	for (; *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\')
			ret += websWrite(wp, "%c", *c);
		else if(*c == '\\'){
			if(from_app != 0)
				ret += websWrite(wp, "\\\\");
			else
				ret += websWrite(wp, "%c", *c);
		}
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}


/* This will return properly encoded HTML entities - required
   for retrieving stored certs/keys.
*/

static int
ej_nvram_clean_get(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
			*c != '"' && *c != '&' && *c != '<' && *c != '>')
				ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d;", *c);
	}

	return ret;
}


static int
ej_nvram_default_get(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;
//	char sid_dummy = "",

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_default_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1
 * <% nvram_get_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_x("undefined"); %> produces ""
 */
static int
ej_nvram_get_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get_x(sid, name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

#ifdef ASUS_DDNS

static int
ej_nvram_get_ddns(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get_x(sid, name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}
	if (strcmp(name,"ddns_return_code")==0) {
		if(!nvram_match("ddns_return_code", "ddns_query")) {
			nvram_set("ddns_return_code","");
		}
	}

	return ret;
}
#endif
/*
 * Example:
 * lan_ipaddr=192.168.1.1
 * <% nvram_get_x("lan_ipaddr"); %> produces "192.168.1.1"
 * <% nvram_get_x("undefined"); %> produces ""
 */
static int
ej_nvram_get_f(int eid, webs_t wp, int argc, char_t **argv)
{
	char *file, *field, *c, buf[64];
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s", &file, &field) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	strcpy(buf, nvram_safe_get_f(file, field));
	for (c = buf; *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

static int
ej_nvram_show_chinese_char(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name, *c;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	for (c = nvram_safe_get(name); *c; c++) {
		ret += websWrite(wp, "%c", *c);
	}

	return ret;
}

/*
 * Example:
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match(int eid, webs_t wp, int argc, char_t **argv)
{
	int i=0;
	char *name, *match, *output;

	if (ejArgs(argc, argv, "%s %s %s", &name, &match, &output) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match(name, match))
	{
		for (i= 0 ; output[i]!= '\0'; i++){
			if (!isalnum(output[i]))
				return 0;
		}
		return websWrite(wp, output);
	}

	return 0;
}

/*
 * Example:
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match_x(int eid, webs_t wp, int argc, char_t **argv)
{
	int i=0;
	char *sid, *name, *match, *output;

	if (ejArgs(argc, argv, "%s %s %s %s", &sid, &name, &match, &output) < 4) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_x(sid, name, match))
	{
		for (i= 0 ; output[i]!= '\0'; i++){
			if (!isalnum(output[i]))
				return 0;
		}
		return websWrite(wp, output);
	}

	return 0;
}

static int
ej_nvram_double_match(int eid, webs_t wp, int argc, char_t **argv)
{
	int i=0;
	char *name, *match, *output;
	char *name2, *match2;

	if (ejArgs(argc, argv, "%s %s %s %s %s", &name, &match, &name2, &match2, &output) < 5) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match(name, match) && nvram_match(name2, match2))
	{
		for (i= 0 ; output[i]!= '\0'; i++){
			if (!isalnum(output[i]))
				return 0;
		}
		return websWrite(wp, output);
	}

	return 0;
}

static int
ej_nvram_double_match_x(int eid, webs_t wp, int argc, char_t **argv)
{
	int i=0;
	char *sid, *name, *match, *output;
	char *sid2, *name2, *match2;

	if (ejArgs(argc, argv, "%s %s %s %s %s %s %s", &sid, &name, &match, &sid2, &name2, &match2, &output) < 7) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_x(sid, name, match) && nvram_match_x(sid2, name2, match2))
	{
		for (i= 0 ; output[i]!= '\0'; i++){
			if (!isalnum(output[i]))
				return 0;
		}
		return websWrite(wp, output);
	}

	return 0;
}

/*
 * Example:
 * wan_proto=dhcp
 * <% nvram_match("wan_proto", "dhcp", "selected"); %> produces "selected"
 * <% nvram_match("wan_proto", "static", "selected"); %> does not produce
 */
static int
ej_nvram_match_both_x(int eid, webs_t wp, int argc, char_t **argv)
{
	int i=0;
	char *sid, *name, *match, *output, *output_not;

	if (ejArgs(argc, argv, "%s %s %s %s %s", &sid, &name, &match, &output, &output_not) < 5)
	{
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_x(sid, name, match))
	{
		for (i= 0 ; output[i]!= '\0'; i++){
			if (!isalnum(output[i]))
				return 0;
		}
		return websWrite(wp, output);
	}
	else
	{
		for (i= 0 ; output_not[i]!= '\0'; i++){
			if (!isalnum(output_not[i]))
				return 0;
		}
		return websWrite(wp, output_not);
	}
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_list("lan_ipaddr", 0); %> produces "192.168.1.1"
 * <% nvram_get_list("lan_ipaddr", 1); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_list_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int which;
	int ret = 0;

	if (ejArgs(argc, argv, "%s %s %d", &sid, &name, &which) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	ret += websWrite(wp, nvram_get_list_x(sid, name, which));
	return ret;
}

/*
 * Example:
 * lan_ipaddr=192.168.1.1 192.168.39.248
 * <% nvram_get_list("lan_ipaddr", 0); %> produces "192.168.1.1"
 * <% nvram_get_list("lan_ipaddr", 1); %> produces "192.168.39.248"
 */
static int
ej_nvram_get_buf_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	int which;

	if (ejArgs(argc, argv, "%s %s %d", &sid, &name, &which) < 3) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	return 0;
}


/*
 * Example:
 * wan_proto=dhcp;dns
 * <% nvram_match_list("wan_proto", "dhcp", "selected", 0); %> produces "selected"
 * <% nvram_match_list("wan_proto", "static", "selected", 1); %> does not produce
 */
static int
ej_nvram_match_list_x(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name, *match, *output;
	int which, i=0;

	if (ejArgs(argc, argv, "%s %s %s %s %d", &sid, &name, &match, &output, &which) < 5) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (nvram_match_list_x(sid, name, match, which)){
		for (i= 0 ; output[i]!= '\0'; i++){
			if (!isalnum(output[i]))
				return 0;
		}
		return websWrite(wp, output);
	}
	else
		return 0;
}

static int
ej_select_channel(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, chstr[32];
	int ret = 0;
	int idx = 0, channel;
	char *value = nvram_safe_get("wl0_country_code");
	char *channel_s = nvram_safe_get("wl_channel");

	if (ejArgs(argc, argv, "%s", &sid) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	channel = (channel_s == NULL)? 0 : atoi(channel_s);

	for (idx = 0; idx < 12; idx++)
	{
		if (idx == 0)
			strcpy(chstr, "Auto");
		else
			sprintf(chstr, "%d", idx);
		ret += websWrite(wp, "<option value=\"%d\" %s>%s</option>", idx, (idx == channel)? "selected" : "", chstr);
	}

	if (    strcasecmp(value, "CA") && strcasecmp(value, "CO") && strcasecmp(value, "DO") &&
		strcasecmp(value, "GT") && strcasecmp(value, "MX") && strcasecmp(value, "NO") &&
		strcasecmp(value, "PA") && strcasecmp(value, "PR") && strcasecmp(value, "TW") &&
		strcasecmp(value, "US") && strcasecmp(value, "UZ") )
	{
		for (idx = 12; idx < 14; idx++)
		{
			sprintf(chstr, "%d", idx);
			ret += websWrite(wp, "<option value=\"%d\" %s>%s</option>", idx, (idx == channel)? "selected" : "", chstr);
		}
	}

	if ((strcmp(value, "") == 0) || (strcasecmp(value, "DB") == 0)/* || (strcasecmp(value, "JP") == 0)*/)
		ret += websWrite(wp, "<option value=\"14\" %s>14</option>", (14 == channel)? "selected" : "");

	return ret;
}

static int
ej_nvram_char_to_ascii(int eid, webs_t wp, int argc, char_t **argv)
{
	char *sid, *name;
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp, *str;
	int ret;

	if (ejArgs(argc, argv, "%s %s", &sid, &name) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	str = nvram_safe_get_x(sid, name);

	/* each char expands to %XX at max */
	ret = strlen(str) * sizeof(char)*3 + sizeof(char);
	if (ret > sizeof(tmp)) {
		buf = (char *)malloc(ret);
		if (buf == NULL) {
			csprintf("No memory.\n");
			return 0;
		}
	}

#ifdef RTCONFIG_UTF8_SSID
	char_to_ascii_safe_with_utf8(buf, str, ret);
#else
	char_to_ascii_safe(buf, str, ret);
#endif
	ret = websWrite(wp, "%s", buf);

	if (buf != tmp)
		free(buf);

	return ret;
}

static int
ej_get_clientlist_from_json_database(int eid, webs_t wp, int argc, char_t **argv)
{
	struct json_object *clients;
	struct json_object *macArray = json_object_new_array();
	int ret;

	struct json_object *customList = NULL, *custom_attr_get = NULL, *custom_client_type = NULL, *custom_client_name = NULL;
	struct json_object *db_specific_client = NULL, *db_specific_client_defaultType = NULL;
	struct json_object *never_online_client = NULL, *new_never_online_client = NULL, *new_never_online_client_type = NULL, *new_never_online_client_name = NULL;
	int customList_status = 0;
#ifdef RTCONFIG_AMAS
	struct json_object *amasList = NULL, *amas_attr_get = NULL;;
	int amasList_status = 0;
	//get amas cap re info
	amasList = json_object_new_object();
	amasList_status = get_amas_info(amasList);
#endif

	if(!pids("networkmap"))
		return 0;
	
	clients = json_object_from_file(NMP_CL_JSON_FILE);

	/* return fake json data to avoid js error */
	if(!clients) {
		clients = json_object_new_object();
		json_object_object_add(clients, "maclist", macArray);
		//json_object_object_add(clients, "ClientAPILevel", json_object_new_string(CLIENTAPILEVEL));
		ret = websWrite(wp, "%s", json_object_to_json_string(clients));

		json_object_put(clients);

		return ret;
	}

	//get custom_clientlist
	customList = json_object_new_object();
	customList_status = get_custom_clientlist_info(customList);

	json_object_object_foreach(clients, key, val) {
#ifdef RTCONFIG_AMAS
		if (is_re_node(key, 1))
			continue;
#endif
		json_object_get_string(val);
		json_object_array_add(macArray, json_object_new_string(key));

		json_object_object_get_ex(clients, key, &db_specific_client);
		//create nickName and defaultType
		json_object_object_add(db_specific_client, "nickName", json_object_new_string(""));
		json_object_object_add(db_specific_client, "defaultType", json_object_new_string("0"));
		//update database type to defaultType
		if(json_object_object_get_ex(db_specific_client, "type", &db_specific_client_defaultType)) {
			json_object_object_add(db_specific_client, "defaultType", json_object_new_string(json_object_get_string(db_specific_client_defaultType)));
			json_object_object_add(db_specific_client, "type", json_object_new_string(json_object_get_string(db_specific_client_defaultType)));//transform int to string
		}
		json_object_object_add(db_specific_client, "from", json_object_new_string("nmpClient"));

		if(customList_status) {
			json_object_object_get_ex(customList, key, &custom_attr_get);
			if(custom_attr_get != NULL) {
				//update custom_clientlist type and nickName
				if(json_object_object_get_ex(custom_attr_get, "type", &custom_client_type))
					json_object_object_add(db_specific_client, "type", json_object_new_string(json_object_get_string(custom_client_type)));
				if(json_object_object_get_ex(custom_attr_get, "name", &custom_client_name))
					json_object_object_add(db_specific_client, "nickName", json_object_new_string(json_object_get_string(custom_client_name)));
			}
		}
#ifdef RTCONFIG_AMAS
		json_object_object_add(db_specific_client, "amesh_isRe", json_object_new_string("0"));
		if(amasList_status) {
			json_object_object_get_ex(amasList, key, &amas_attr_get);
			if(amas_attr_get != NULL) {
				struct json_object *amas_get_type = NULL;
				if(json_object_object_get_ex(amas_attr_get, "type", &amas_get_type))
					json_object_object_add(db_specific_client, "amesh_isRe", json_object_new_string(!(strcmp(json_object_get_string(amas_get_type), "RE")) ? "1" : "0"));
			}
		}
#endif
	}

	//add nerver online client
	if(customList_status) {
		json_object_object_foreach(customList, key, val) {
			json_object_object_get_ex(clients, key, &never_online_client);
			if(never_online_client == NULL) {
				json_object_array_add(macArray, json_object_new_string(key));

				new_never_online_client = json_object_new_object();
				json_object_object_add(new_never_online_client, "type", json_object_new_string("0"));
				json_object_object_add(new_never_online_client, "mac", json_object_new_string(key));
				json_object_object_add(new_never_online_client, "name", json_object_new_string(key));
				json_object_object_add(new_never_online_client, "vendor", json_object_new_string(""));
				json_object_object_add(new_never_online_client, "nickName", json_object_new_string(""));
				json_object_object_add(new_never_online_client, "defaultType", json_object_new_string("0"));
				if(json_object_object_get_ex(val, "type", &new_never_online_client_type))
					json_object_object_add(new_never_online_client, "type", json_object_new_string(json_object_get_string(new_never_online_client_type)));
				if(json_object_object_get_ex(val, "name", &new_never_online_client_name))
					json_object_object_add(new_never_online_client, "nickName", json_object_new_string(json_object_get_string(new_never_online_client_name)));
				json_object_object_add(new_never_online_client, "from", json_object_new_string("customList"));
				json_object_object_add(clients, key, new_never_online_client);
			}
		}
	}

	json_object_object_add(clients, "maclist", macArray);
	//json_object_object_add(clients, "ClientAPILevel", json_object_new_string(CLIENTAPILEVEL));
	ret = websWrite(wp, "%s", json_object_to_json_string(clients));

	if(customList)
		json_object_put(customList);
	if(new_never_online_client)
		json_object_put(new_never_online_client);
#ifdef RTCONFIG_AMAS
	if(amasList)
		json_object_put(amasList);
#endif
	if(clients)
		json_object_put(clients);

	return ret;
}

static int
ej_get_all_basic_clientlist(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret;
	struct json_object *clients=NULL;
	struct json_object *clients_array_obj = NULL, *client_name = NULL, *macArray_tmp = NULL;
	struct json_object *customList = NULL, *custom_attr_get = NULL, *custom_client_name = NULL;
	int customList_status = 0;

	if(!pids("networkmap") || (clients = json_object_from_file(NMP_CL_JSON_FILE)) == NULL){
		ret = websWrite(wp, "[]");
		return 0;
	}

	clients_array_obj = json_object_new_array();
	customList = json_object_new_object();
	customList_status = get_custom_clientlist_info(customList);

	json_object_object_foreach(clients, key, val) {
		macArray_tmp = json_object_new_array();
		json_object_array_add(macArray_tmp, json_object_new_string(key));
		if(customList_status && json_object_object_get_ex(customList, key, &custom_attr_get) && json_object_object_get_ex(custom_attr_get, "name", &custom_client_name)){
			json_object_array_add(macArray_tmp, custom_client_name);
		}else{
			json_object_object_get_ex(val, "name", &client_name);
			json_object_array_add(macArray_tmp, client_name);
		}
		json_object_array_add(clients_array_obj, macArray_tmp);
	}

	ret = websWrite(wp, "%s", json_object_to_json_string(clients_array_obj));

	if(clients_array_obj)
		json_object_put(clients_array_obj);
	if(customList)
		json_object_put(customList);
	if(clients)
		json_object_put(clients);

	return ret;
}

#ifdef RTCONFIG_INTERNETCTRL
static time_t get_ic_local_timestamp(char *timestamp)
{
	char *ptr=NULL;
	time_t ts=0;
	ts = (time_t)strtol(timestamp+2, &ptr, 10);
	struct tm *plocal_tm = localtime(&ts);
	return mktime(plocal_tm);
}

/* info: mac, ip, name, isonline, networkAccess(ICFILTER), starttime(ICFILTER), endtime(ICFILTER) */
static int ej_get_clientlist_reportstate(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0, networkAccess=0;
	int mac_idx=0, daytime_idx=0;
	time_t starttime=0, endtime=0;
	struct json_object *clients = NULL, *macArray=NULL, *client_obj=NULL, *allclient_obj=NULL;
	struct json_object *ip = NULL, *name = NULL, *type = NULL, *isOnline = NULL;
	char *stime_t=NULL;
	char icfilter_mac[512]={0}, icfilter_macfilter_daytime[512]={0};
	char icfilter_mac_tmp[512]={0}, *icfilter_mac_next=NULL;
	char icfilter_daytime_tmp[512]={0}, *icfilter_daytime_next=NULL;
	char starttime_buf[16]={0}, endtime_buf[16]={0};

	time_t now = time(NULL);

	macArray = json_object_new_array();
	allclient_obj = json_object_new_object();

	strlcpy(icfilter_mac, nvram_safe_get("ICFILTER_MAC"), sizeof(icfilter_mac));
	strlcpy(icfilter_macfilter_daytime, nvram_safe_get("ICFILTER_MACFILTER_DAYTIME"), sizeof(icfilter_macfilter_daytime));

	if(pids("networkmap")){
			clients = json_object_new_object();
			get_client_detail_info(clients, macArray, SHMKEY_LAN);
	}

	if(clients) {
		json_object_object_foreach(clients, key, val){
			client_obj = json_object_new_object();
			mac_idx = 0;
			if(json_object_object_get_ex(val, "ip", &ip))
				json_object_object_add(client_obj, "ip", ip);

			if(json_object_object_get_ex(val, "name", &name))
				json_object_object_add(client_obj, "deviceName", name);

			if(json_object_object_get_ex(val, "type", &type))
				json_object_object_add(client_obj, "deviceType", type);

			if(json_object_object_get_ex(val, "isOnline", &isOnline))
				json_object_object_add(client_obj, "Connectivity", isOnline);

			if (strstr(icfilter_mac, key)!=NULL){
				foreach_62(icfilter_mac_tmp, icfilter_mac, icfilter_mac_next){
					mac_idx++;
					if(!strcmp(icfilter_mac_tmp, key)){
						daytime_idx = 0;
						foreach_62(icfilter_daytime_tmp, icfilter_macfilter_daytime, icfilter_daytime_next){
							daytime_idx++;
							if(mac_idx == daytime_idx){
								stime_t = strtok(icfilter_daytime_tmp, "<");
								starttime = get_ic_local_timestamp(stime_t);
								snprintf(starttime_buf, sizeof(starttime_buf), "%ld", starttime);
								json_object_object_add(client_obj, "starttime", json_object_new_string(starttime_buf));
								if(stime_t != NULL){
									stime_t = strtok(NULL, "<");
									if(stime_t != NULL){
										endtime = get_ic_local_timestamp(stime_t);
										snprintf(endtime_buf, sizeof(endtime_buf), "%ld", endtime);
										json_object_object_add(client_obj, "endtime", json_object_new_string(endtime_buf));
									}else
										endtime = 0;
								}
								if(icfilter_daytime_tmp[1] == '0'){
									json_object_object_add(client_obj, "networkAccess", json_object_new_string("0"));
									networkAccess = (now > starttime && (endtime == 0 || now < endtime))?0:1;
								}
								else{
									json_object_object_add(client_obj, "networkAccess", json_object_new_string("1"));
									networkAccess = (now > starttime && (endtime == 0 || now < endtime))?1:0;
								}
								json_object_object_add(client_obj, "networkAccess_now", json_object_new_string(networkAccess?"1":"0"));
							}
						}
						break;
					}
				}
			}
			json_object_object_add(allclient_obj, key, client_obj);
		}
	}
	ret = websWrite(wp, "%s", json_object_to_json_string(allclient_obj));

	if(clients)
		json_object_put(clients);
	if(allclient_obj)
		json_object_put(allclient_obj);
	return ret;
}
#endif

static int get_basic_clientlist_info(webs_t wp, int opt)
{
	int ret = 0, client_name_status = 0, wl_count = 0, wire_count = 0;
	struct json_object *clients = NULL, *cache_clients = NULL, *wire_clients_array_obj = NULL, *wl_clients_array_obj = NULL, *clients_array_obj = NULL;
	struct json_object *client_tmp = NULL, *client_name = NULL, *isWL = NULL, *macArray = NULL, *macArray_tmp = NULL;
	macArray = json_object_new_array();
	clients_array_obj = json_object_new_array();
	wire_clients_array_obj = json_object_new_array();
	wl_clients_array_obj = json_object_new_array();

	if(opt == 3 && check_if_file_exist(NMP_CL_JSON_FILE))
	{
		cache_clients = json_object_from_file(NMP_CL_JSON_FILE);
	}

	if(pids("networkmap")){
			clients = json_object_new_object();
			get_client_detail_info(clients, macArray, SHMKEY_LAN);
	}

	if(opt == 0 || opt == 1|| opt == 2){
		if(clients) {
			json_object_object_foreach(clients, key, val){
				macArray_tmp = json_object_new_array();
				client_name_status = json_object_object_get_ex(val, "name", &client_name);

				if(json_object_object_get_ex(val, "isWL", &isWL)){
					if(atoi(json_object_get_string(isWL)) > 0){
						json_object_array_add(macArray_tmp, json_object_new_string(key));
						if(client_name_status)
							json_object_array_add(macArray_tmp, client_name);
						else
							json_object_array_add(macArray_tmp, json_object_new_string(key));

						json_object_array_add(wl_clients_array_obj, macArray_tmp);
						wl_count++;
					}else{
						json_object_array_add(macArray_tmp, json_object_new_string(key));
						if(client_name_status)
							json_object_array_add(macArray_tmp, client_name);
						else
							json_object_array_add(macArray_tmp, json_object_new_string(key));

						json_object_array_add(wire_clients_array_obj, macArray_tmp);
						wire_count++;
					}
				}
			}
		}
	}else if(opt == 3){
		if(cache_clients && clients){
			json_object_object_foreach(cache_clients, key, val) {
				macArray_tmp = json_object_new_array();
				client_name_status = json_object_object_get_ex(val, "name", &client_name);
				if(json_object_object_get_ex(clients, key, &client_tmp))
					if(json_object_object_get_ex(client_tmp, "isWL", &isWL))
						if(atoi(json_object_get_string(isWL)) > 0)
							continue;

				json_object_array_add(macArray_tmp, json_object_new_string(key));
				if(client_name_status)
					json_object_array_add(macArray_tmp, client_name);
				else
					json_object_array_add(macArray_tmp, json_object_new_string(key));

				json_object_array_add(clients_array_obj, macArray_tmp);
			}
		}

	}

	if(opt == 0)	//wire client
		ret = websWrite(wp, "%s", json_object_to_json_string(wire_clients_array_obj));
	else if(opt == 1)	//wireless client
		ret = websWrite(wp, "%s", json_object_to_json_string(wl_clients_array_obj));
	else if(opt == 2)
		ret = websWrite(wp, "{\"wireless\":\"%d\", \"wire\":\"%d\"}", wl_count, wire_count);
	else if(opt == 3)	//wireless client
		ret = websWrite(wp, "%s", json_object_to_json_string(clients_array_obj));

	if(clients)
		json_object_put(clients);
	if(cache_clients)
		json_object_put(cache_clients);
	if(macArray)
		json_object_put(macArray);
	if(wl_clients_array_obj)
		json_object_put(clients_array_obj);
	if(wire_clients_array_obj)
		json_object_put(wire_clients_array_obj);
	if(clients_array_obj)
		json_object_put(clients_array_obj);
	if(macArray_tmp)
		json_object_put(macArray_tmp);
	return ret;
}

static int
ej_get_wol_clientlist(int eid, webs_t wp, int argc, char_t **argv)
{
	get_basic_clientlist_info(wp, 3);
	return 0;
}

static int ej_get_basic_clientlist(int eid, webs_t wp, int argc, char_t **argv)
{
	/* 1:wireless client 2: wireless/wired count */
	get_basic_clientlist_info(wp, 1);
	return 0;
}


static int ej_get_basic_clientlist_count(int eid, webs_t wp, int argc, char_t **argv)
{
	/* 1:wireless client 2: wireless/wired count */
	get_basic_clientlist_info(wp, 2);
	return 0;
}


/* Report sys up time */
static int
ej_uptime(int eid, webs_t wp, int argc, char_t **argv)
{

//	FILE *fp;
	char buf[MAX_LINE_SIZE];
	char bufx[MAX_LINE_SIZE];
//	unsigned long uptime;
	int ret;
	char *str = file2str("/proc/uptime");
	time_t tm;

	time(&tm);
	sprintf(buf, rfctime(&tm));

	if (str) {
		unsigned int up = atoi(str);
		free(str);
		char lease_buf[128];
		memset(lease_buf, 0, sizeof(lease_buf));
		reltime(up, lease_buf);
		sprintf(bufx, "%s(%s since boot)", buf, lease_buf);
		strlcpy(buf, bufx, sizeof(buf));
	}

	ret = websWrite(wp, buf);
	return ret;
}

static int
ej_sysuptime(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret=0;
	char *str = file2str("/proc/uptime");

	if (str) {
		unsigned int up = atoi(str);
		free(str);

		char lease_buf[128];
		memset(lease_buf, 0, sizeof(lease_buf));
		reltime(up, lease_buf);
		ret = websWrite(wp, "%s since boot", lease_buf);
	}

	return ret;
}

struct lease_t {
	unsigned char chaddr[16];
	u_int32_t yiaddr;
	u_int32_t expires;
	char hostname[64];
};


static int
ej_ddnsinfo(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret;

	ret = websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\"]",
		nvram_safe_get("ddns_enable_x"),
		nvram_safe_get("ddns_server_x"),
		nvram_safe_get("ddns_hostname_x"),
		nvram_safe_get("ddns_return_code")
		);

	return ret;
}

int
websWriteCh(webs_t wp, char *ch, int count)
{
   int i, ret;

   ret = 0;
   for (i=0; i<count; i++)
      ret+=websWrite(wp, "%s", ch);
   return (ret);
}

int skip_log(char *message)
{
	if(strstr(message, "net_ratelimit") != NULL) return 1;
#ifdef RTCONFIG_LANTIQ
	if(strstr(message, "drvhlpr") != NULL) return 1;
	if(strstr(message, "mtlk") != NULL) return 1;
	if(strstr(message, "libhelper") != NULL) return 1;
	if(strstr(message, "mtdump") != NULL) return 1;
	if(strstr(message, "hostapd") != NULL) return 1;
	if(strstr(message, "mcastd") != NULL) return 1;
#endif
#ifdef RTCONFIG_HND_ROUTER_AX
	if(strstr(message, "own address as source") != NULL) return 1;
#endif
	return 0;
}


static int dump_file(webs_t wp, char *filename)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret=0;

	fp = fopen(filename, "r");

	if (fp==NULL)
	{
		ret+=websWrite(wp, "%s", "");
		return (ret);
	}

	ret = 0;

	while (fgets(buf, MAX_LINE_SIZE, fp)!=NULL)
	{
	    int len;
	    len = strlen(buf); // fgets() would fill the '\0' at the last character in buffer.
		if(skip_log(buf) == 0)
			ret += websWriteData(wp, buf, len);
	}

	fclose(fp);

	return (ret);
}

extern int wl_wps_info(int eid, webs_t wp, int argc, char_t **argv, int unit);

static int
ej_dump(int eid, webs_t wp, int argc, char_t **argv)
{
//	FILE *fp;
//	char buf[MAX_LINE_SIZE];
	char filename[PATH_MAX], path[PATH_MAX];
	char *file,*script;
	int ret;

	if (ejArgs(argc, argv, "%s %s", &file, &script) < 2) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	//csprintf("Script : %s, File: %s\n", script, file);

	// run scrip first to update some status
	if (strcmp(script,"syscmd.sh")==0) sys_script(script);

	if (strcmp(file, "wlan11b.log")==0)
		return (ej_wl_status(eid, wp, 0, NULL, 0));	/* FIXME */
	else if (strcmp(file, "wlan11b_2g.log")==0)
		return (ej_wl_status_2g(eid, wp, 0, NULL));
	else if (strcmp(file, "leases.log")==0)
		return (ej_lan_leases(eid, wp, 0, NULL));
#ifdef RTCONFIG_IPV6
	else if (strcmp(file, "ipv6_network.log")==0)
		return (ej_lan_ipv6_network(eid, wp, 0, NULL));
#endif
#if 0 // We use data_arrays functions
	else if (strcmp(file, "iptable.log")==0)
		return (get_nat_vserver_table(eid, wp, 0, NULL));
	else if (strcmp(file, "route.log")==0)
		return (ej_route_table(eid, wp, 0, NULL));
#endif
	else if (strcmp(file, "wps_info.log")==0)
	{
#ifndef RTAC3200
		if (nvram_match("wps_band_x", "0"))
			return (ej_wps_info_2g(eid, wp, 0, NULL));
		else
			return (ej_wps_info(eid, wp, 0, NULL));
#else
		return wl_wps_info(eid, wp, argc, argv, nvram_get_int("wps_band_x"));
#endif
	}
	ret = 0;
	strcpy(path, get_logfile_path());
	if (strcmp(file, "syslog.log")==0)
	{
		sprintf(filename, "%s/%s-1", path, file);
		ret += dump_file(wp, filename);
		sprintf(filename, "%s/%s", path, file);
		ret += dump_file(wp, filename);
	}
//#ifdef RTCONFIG_CLOUDSYNC
	else if(!strcmp(file, "cloudsync.log")){
		sprintf(filename, "/tmp/smartsync/.logs/system.log");
		ret += dump_file(wp, filename);
		sprintf(filename, "/tmp/%s", file);
		ret += dump_file(wp, filename);
	}
	else if(!strcmp(file, "clouddisk.log")){
		sprintf(filename, "/tmp/lighttpd/syslog.log");
		ret += dump_file(wp, filename);
		sprintf(filename, "/tmp/%s", file);
		ret += dump_file(wp, filename);
	}
//#endif
#ifdef RTCONFIG_OPENVPN
	else if(!strcmp(file, "openvpn_connected")){
		int unit = nvram_get_int("vpn_server_unit");
		parse_openvpn_status(unit);
		sprintf(filename, "/etc/openvpn/server%d/client_status", unit);
		ret += dump_file(wp, filename);
	}
#endif
#ifdef RTCONFIG_PUSH_EMAIL
#ifdef RTCONFIG_DSL
	else if(!strcmp(file, "fb_fail_content")){
		sprintf(filename, FB_FILE_WEB);
		if(check_if_file_exist(filename)) {
#if 0
			eval("sed", "-i", "/PIN Code:/d", filename);
#endif
			eval("sed", "-i", "/MAC Address:/d", filename);
			eval("sed", "-i", "/E-mail:/d", filename);
			eval("sed", "-i", "/Download Master:/d", filename);
			eval("sed", "-i", "/Cloud Disk:/d", filename);
			eval("sed", "-i", "/Smart Access:/d", filename);
			eval("sed", "-i", "/Smart Sync:/d", filename);
			eval("sed", "-i", "/Guest Network 1\\/2\\/3 \\(.*\\):/d", filename);
			eval("sed", "-i", "/Current connected Clients:/d", filename);
			eval("sed", "-i", "/CC\\(.*\\)\\/CC\\(.*\\)\\/TC:/d", filename);
			eval("sed", "-i", "/regrev\\(.*\\)\\/regrev\\(.*\\):/d", filename);
			ret += dump_file(wp, filename);
			unlink(filename);
		}
		else {
			char buf[4096] = {0};
			snprintf(buf, 4095,
				"Your ISP / Internet Service Provider: %s\n"
				"Name of the Subscribed Plan/Service/Package:  %s\n"
				"DSL service performance option: %s\n"
				"Firmware version: %s.%s_%s\n"
				"DSL Firmware version: %s\n"
				"&nbsp;\n"
				"Comments / Suggestions:\n"
				"%s\n"
				, nvram_safe_get("fb_ISP")
				, nvram_safe_get("fb_Subscribed_Info")
				, nvram_safe_get("fb_availability")
				, nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno")
				, nvram_safe_get("dsllog_fwver")
				, nvram_safe_get("fb_comment")
				);
			ret += websWrite(wp, buf);
		}
	}
#else /* RTCONFIG_DSL */
	else if(!strcmp(file, "fb_fail_content")){
		sprintf(filename, FB_FILE_WEB);
		if(check_if_file_exist(filename)) {
#if 0
			eval("sed", "-i", "/PIN Code:/d", filename);
#endif
#if !defined(RTCONFIG_BCM_7114) && !defined(HND_ROUTER)
			eval("sed", "-i", "/MAC Address:/d", filename);
#endif
			eval("sed", "-i", "/E-mail:/d", filename);
			eval("sed", "-i", "/Download Master:/d", filename);
			eval("sed", "-i", "/Cloud Disk:/d", filename);
			eval("sed", "-i", "/Smart Access:/d", filename);
			eval("sed", "-i", "/Smart Sync:/d", filename);
			eval("sed", "-i", "/Guest Network 1\\/2\\/3 \\(.*\\):/d", filename);
			eval("sed", "-i", "/Current connected Clients:/d", filename);
			eval("sed", "-i", "/CC\\(.*\\)\\/CC\\(.*\\)\\/TC:/d", filename);
			eval("sed", "-i", "/regrev\\(.*\\)\\/regrev\\(.*\\):/d", filename);
			ret += dump_file(wp, filename);
			unlink(filename);
		}
		else {
			char buf[4096] = {0};
			snprintf(buf, 4095,
				"Feedback problem type: %s\n"
				"Feedback problem description:  %s\n"
				"Firmware version: %s.%s_%s\n"
				"&nbsp;\n"
				"Comments / Suggestions:\n"
				"%s\n"
				, nvram_safe_get("fb_ptype")
				, nvram_safe_get("fb_pdesc")
				, nvram_safe_get("firmver"), nvram_safe_get("buildno"), nvram_safe_get("extendno")
				, nvram_safe_get("fb_comment")
				);
			ret += websWrite(wp, buf);
		}
	}
#endif /* RTCONFIG_DSL */
#endif /* RTCONFIG_PUSH_EMAIL */
#ifdef RTCONFIG_IPSEC
	else if (strcmp(file, "ipsec.log")==0) {
		sprintf(filename, FILE_PATH_IPSEC_LOG);
		ret += dump_file(wp, filename);
	}
#endif
	else if (strcmp(file, "connect.log")==0) {
		sprintf(filename, "/tmp/%s", file);
		system("/usr/sbin/netstat-nat -r state -xn > /tmp/connect.log 2>&1");
		ret += dump_file(wp, filename);
	}
	else if ( strcmp(file, "syscmd.log")==0
		|| strcmp(file, "pptp_connected")==0
		|| strcmp(file, "release_note0.txt")==0
		|| strcmp(file, "release_note1.txt")==0
		|| strcmp(file, "release_note.txt")==0
#ifdef RTCONFIG_DSL
		|| strcmp(file, "adsl/tc_fw_ver_short.txt")==0
		|| strcmp(file, "adsl/tc_ras_ver.txt")==0
		|| strcmp(file, "adsl/tc_fw_ver.txt")==0
		|| strcmp(file, "adsl/adsllog.log")==0
#endif
	){
		sprintf(filename, "/tmp/%s", file);
		ret += dump_file(wp, filename);
	}

	return ret;
}

#if 0
static int
ej_load(int eid, webs_t wp, int argc, char_t **argv)
{
	char *script;

	if (ejArgs(argc, argv, "%s", &script) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	sys_script(script);
	return (websWrite(wp,"%s",""));
}
#endif

/*
 * retreive and convert wl values for specified wl_unit
 * Example:
 * <% wl_get_parameter(); %> for coping wl[n]_ to wl_
 */

static int
ej_wl_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit, subunit;

	unit = nvram_get_int("wl_unit");
	subunit = nvram_get_int("wl_subunit");

	// handle generate cases first
	(void)copy_index_to_unindex("wl_", unit, subunit);

	return (websWrite(wp,"%s",""));
}

int webWriteNvram(webs_t wp, char *name)
{
	char *c;
	int ret = 0;

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' && *c != '%')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "%%%02X", *c);
	}

	return ret;
}

int webWriteUTF8Nvram(webs_t wp, char *name)
{
	int ret = 0;

#ifdef RTCONFIG_UTF8_SSID
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp;
	char *c = nvram_safe_get(name);
	ret = strlen(c) * sizeof(char)*3 + sizeof(char);
	if (ret > sizeof(tmp)) {
		buf = (char *)malloc(ret);
		if (buf == NULL) {
			csprintf("No memory.\n");
			return 0;
		}
	}
	char_to_ascii_safe_with_utf8(buf, c, ret);
	ret = websWrite(wp, "%s", buf);

	if (buf != tmp)
		free(buf);
#else
	webWriteNvram(wp, name);
#endif
	return ret;
}

int webWriteNvram2(webs_t wp, char *name)
{
	char *c;
	int ret = 0;

	for (c = nvram_safe_get(name); *c; c++) {
		if (isprint(*c) &&
		    *c != '"' && *c != '&' && *c != '<' && *c != '>' && *c != '\\' && *c != '%')
			ret += websWrite(wp, "%c", *c);
		else
			ret += websWrite(wp, "&#%d", *c);
	}

	return ret;
}

/*
 * retreive guest network releated wl values
 */

static int
ej_wl_get_guestnetwork(int eid, webs_t wp, int argc, char_t **argv)
{
	char word2[128], tmp[128], *next2;
	char *unitname;
	char prefix[32];
	int  unit, subunit;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &unitname) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	unit = atoi(unitname);
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	ret += websWrite(wp, "[");

	subunit = 0;
	foreach(word2, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {
		if(subunit>0) websWrite(wp, ", ");

		subunit++;

		ret += websWrite(wp, "[\"");
		ret += webWriteNvram(wp, strcat_r(word2, "_bss_enabled", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteUTF8Nvram(wp, strcat_r(word2, "_ssid", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_auth_mode_x", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_crypto", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_wpa_psk", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_wep_x", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key1", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key2", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key3", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_key4", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_expire", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_lanaccess", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_expire_tmp", tmp));
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_macmode", tmp));	// gn_array[][14]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram(wp, strcat_r(word2, "_mbss", tmp));	// gn_array[][15]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_maclist_x", tmp));	// gn_array[][16]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_phrase_x", tmp));	// gn_array[][17]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_bw_enabled", tmp));	// gn_array[][18]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_bw_dl", tmp));	// gn_array[][19]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_bw_ul", tmp));	// gn_array[][20]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_guest_num", tmp));	// gn_array[][21], original 18 in ac88q branch
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_closed", tmp));	// gn_array[][22]
		ret += websWrite(wp, "\", \"");
		ret += webWriteNvram2(wp, strcat_r(word2, "_sync_node", tmp));  // gn_array[][23]
		ret += websWrite(wp, "\"]");
	}
	ret += websWrite(wp, "]");
	return ret;
}

/*
 * retreive and convert wan values for specified wan_unit
 * Example:
 * <% wan_get_parameter(); %> for coping wan[n]_ to wan_
 */

static int
ej_wan_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("wan_unit");

	// handle generate cases first
	(void)copy_index_to_unindex("wan_", unit, -1);

	return (websWrite(wp,"%s",""));
}


/*

 * retreive and convert lan values for specified lan_unit
 * Example:
 * <% lan_get_parameter(); %> for coping lan[n]_ to lan_
 */

static int
ej_lan_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("lan_unit");

	// handle generate cases first
	(void)copy_index_to_unindex("lan_", unit, -1);

	return (websWrite(wp,"%s",""));
}

#ifdef RTCONFIG_OPENVPN
static int
ej_vpn_server_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("vpn_server_unit");
	// handle generate cases first
	(void)copy_index_to_unindex("vpn_server_", unit, -1);
	(void)copy_index_to_unindex("vpn_crt_server_", unit, -1);

	return (websWrite(wp,"%s",""));
}

static int
ej_vpn_client_get_parameter(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;

	unit = nvram_get_int("vpn_client_unit");
	// handle generate cases first
	(void)copy_index_to_unindex("vpn_client_", unit, -1);
	(void)copy_index_to_unindex("vpn_crt_client_", unit, -1);

	return (websWrite(wp,"%s",""));
}

void
_get_vpn_crt_value(int eid, webs_t wp, int type, int cert, int idx) {
	char buf[8000];
	char filename[32];
	char *c;

	get_ovpn_filename(type, idx, cert, filename, sizeof(filename));
	get_ovpn_key(type, idx, cert, buf, sizeof(buf));

	websWrite(wp, "%s=['", filename);

	for (c = buf; *c; c++) {
		if (isprint(*c) &&
			*c != '"' && *c != '&' && *c != '<' && *c != '>')
			websWrite(wp, "%c", *c);
		else
			websWrite(wp, "&#%d", *c);
	}
	websWrite(wp, "'];\n");
}

static int
ej_vpn_crt_server(int eid, webs_t wp, int argc, char **argv) {
	int idx = 0;

	for (idx = 1; idx <= OVPN_SERVER_MAX; idx++) {

		//vpn_crt_server_ca
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_CA, idx);

		//vpn_crt_server_crt
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_CERT, idx);

		//vpn_crt_server_key
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_KEY, idx);

		//vpn_crt_server_dh
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_DH, idx);

		//vpn_crt_server_crl
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_CRL, idx);

		//vpn_crt_server_static
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_STATIC, idx);

		//vpn_crt_server_extra
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_SERVER, OVPN_SERVER_EXTRA, idx);

	}
	return 0;
}
static int
ej_vpn_crt_client(int eid, webs_t wp, int argc, char **argv) {
	int idx = 0;

	for (idx = 1; idx <= OVPN_CLIENT_MAX; idx++) {

		//vpn_crt_client_ca
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_CLIENT, OVPN_CLIENT_CA, idx);

		//vpn_crt_client_crt
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_CLIENT, OVPN_CLIENT_CERT, idx);

		//vpn_crt_client_key
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_CLIENT, OVPN_CLIENT_KEY, idx);

		//vpn_crt_client_static
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_CLIENT, OVPN_CLIENT_STATIC, idx);

		//vpn_crt_client_crl
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_CLIENT, OVPN_CLIENT_CRL, idx);

		//vpn_crt_client_extra
		_get_vpn_crt_value(eid, wp, OVPN_TYPE_CLIENT, OVPN_CLIENT_EXTRA, idx);

	}
	return 0;
}
#endif

//2008.08 magic {
// Largest POST will be the OpenVPN key page:
// 7 fields at up to 8000 bytes each, for 56KB in worst-case scenario
// Add the other settings, would fit in a 64 KB buffer

static char post_buf[65535] = { 0 };
static char post_buf_backup[65535] = { 0 };
static char post_json_buf[65535] = { 0 };

static void do_html_post_and_get(char *url, FILE *stream, int len, char *boundary){
	char *query = NULL;

	init_cgi(NULL);

	memset(post_buf, 0, sizeof(post_buf));
	memset(post_buf_backup, 0, sizeof(post_buf));
	memset(post_json_buf, 0, sizeof(post_json_buf));

	if (fgets(post_buf, MIN(len+1, sizeof(post_buf)), stream)){
		len -= strlen(post_buf);

		while (len--)
			(void)fgetc(stream);
	}
	sprintf(post_json_buf, "%s", post_buf);

	query = url;
	strsep(&query, "?");

	HTTPD_DBG("post_buf = %s, query = %s\n", post_buf, query);

	if (query && strlen(query) > 0){
		if (strlen(post_buf) > 0)
			snprintf(post_buf_backup, sizeof(post_buf_backup), "?%s&%s", post_buf, query);
		else
			snprintf(post_buf_backup, sizeof(post_buf_backup), "?%s", query);
		sprintf(post_buf, "%s", post_buf_backup+1);
	}
	else if (strlen(post_buf) > 0)
		sprintf(post_buf_backup, "?%s", post_buf);
	//websScan(post_buf_backup);
	init_cgi(post_buf);
}

static void do_html_get(char *url, int len, char *boundary){
	char *query = NULL;

	init_cgi(NULL);

	memset(post_buf, 0, sizeof(post_buf));
	memset(post_buf_backup, 0, sizeof(post_buf));
	memset(post_json_buf, 0, sizeof(post_json_buf));

	query = url;
	strsep(&query, "?");

	if (query && strlen(query) > 0){
		unescape(query);
		sprintf(post_buf_backup, "?%s", query);
		sprintf(post_buf, "%s", post_buf_backup+1);
	}
	//websScan(post_buf_backup);
	init_cgi(post_buf);
}

extern struct nvram_tuple router_defaults[];
extern struct nvram_tuple router_state_defaults[];

// used for handling multiple instance
// copy nvram from, ex wl0_ to wl_
// prefix is wl_, wan_, or other function with multiple instance
// [prefix]_unit and [prefix]_subunit must exist
void copy_index_to_unindex(char *prefix, int unit, int subunit)
{
	struct nvram_tuple *t;
	char *value;
	char name[64], unitname[64], unitptr[32];
	char tmp[64], unitprefix[32];

	// check if unit exist
	if(unit == -1) return;
	snprintf(unitptr, sizeof(unitptr), "%sunit", prefix);
	if((value=nvram_get(unitptr))==NULL) return;

	strncpy(tmp, prefix, sizeof(tmp));
	tmp[strlen(prefix)-1]=0;
	if(subunit==-1||subunit==0)
		snprintf(unitprefix, sizeof(unitprefix), "%s%d_", tmp, unit);
	else snprintf(unitprefix, sizeof(unitprefix), "%s%d.%d_", tmp, unit, subunit);

	/* go through each nvram value */
	for (t = router_defaults; t->name; t++)
	{
		memset(name, 0, 64);
		sprintf(name, "%s", t->name);

		// exception here
		if(strcmp(name, unitptr)==0) continue;
		if(!strcmp(name, "wan_primary")) continue;

		if(!strncmp(name, prefix, strlen(prefix)))
		{
			(void)strcat_r(unitprefix, &name[strlen(prefix)], unitname);

			if((value=nvram_get(unitname))!=NULL)
			{
				nvram_set(name, value);
			}
		}
	}
}

#ifdef RTCONFIG_DUALWAN
void save_index_to_interface(){//Cherry Cho added for exchanging settings of dualwan in 2014/10/20.
	int i;
	char word[80], *next;
	char tmp[64], prefix[32], unitprefix[32], name[64], unitname[64];
	char *wans_dualwan = nvram_get("wans_dualwan");
	char *wan_prefix = "wan_";
	struct nvram_tuple *t;
	char *value;

	i = 0;
	foreach(word, wans_dualwan, next) {
		memset(prefix, 0, sizeof(prefix));
		memset(unitprefix, 0, sizeof(unitprefix));
		snprintf(prefix, sizeof(prefix), "%s_", word);
		snprintf(unitprefix, sizeof(unitprefix), "wan%d_", i);

		/* go through each nvram value */
		for (t = router_defaults; t->name; t++)
		{
			memset(name, 0, 64);
			sprintf(name, "%s", t->name);

			if(!strcmp(name, "wan_unit")) continue;
			if(!strcmp(name, "wan_primary")) continue;

			if(!strncmp(name, wan_prefix, strlen(wan_prefix))){
				memset(tmp, 0, sizeof(tmp));
				memset(unitname, 0, sizeof(unitname));
				(void)strcat_r(prefix, name, tmp);
				(void)strcat_r(unitprefix, &name[strlen(wan_prefix)], unitname);
				if((value = nvram_get(unitname)) != NULL){
					nvram_set(tmp, value);
				}
			}
		}
		i++;
	}
}


void save_interface_to_index(char *wans_dualwan){
	int i;
	char word[80], *next;
	char tmp[64], prefix[32], unitprefix[32], name[64], unitname[64];
	char *wan_prefix = "wan_";
	struct nvram_tuple *t;
	char *value;

	i = 0;
	foreach(word, wans_dualwan, next) {
		memset(prefix, 0, sizeof(prefix));
		memset(unitprefix, 0, sizeof(unitprefix));
		snprintf(prefix, sizeof(prefix), "%s_", word);
		snprintf(unitprefix, sizeof(unitprefix), "wan%d_", i);

		/* go through each nvram value */
		for (t = router_defaults; t->name; t++)
		{
			memset(name, 0, 64);
			sprintf(name, "%s", t->name);

			if(!strcmp(name, "wan_unit")) continue;
			if(!strcmp(name, "wan_primary")) continue;

			if(!strncmp(name, wan_prefix, strlen(wan_prefix))){
				memset(tmp, 0, sizeof(tmp));
				memset(unitname, 0, sizeof(unitname));
				(void)strcat_r(prefix, name, tmp);
				(void)strcat_r(unitprefix, &name[strlen(wan_prefix)], unitname);
				if((value = nvram_get(tmp)) != NULL){
					nvram_set(unitname, value);
				}
			}
		}
		i++;
	}
}
#endif

#ifdef RTCONFIG_JFFS2USERICON
void handle_upload_icon(char *value) {
	char *mac, *uploadicon;
	char filename[32];
	memset(filename, 0, 32);

	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_USERICON))
		mkdir(JFFS_USERICON, 0755);

	if((vstrsep(value, ">", &mac, &uploadicon) == 2)) {
		sprintf(filename, "/jffs/usericon/%s.log", mac);

		//Delete exist file
		if(check_if_file_exist(filename)) {
			unlink(filename);
		}
		//If upload icon string is not noupload, then write to file.
		if(strcmp(uploadicon, "noupload")) {
			FILE *fp;
			if((fp = fopen(filename, "w")) != NULL) {
				fprintf(fp, "%s", uploadicon);
				fclose(fp);
			}
		}
	}
}
void del_upload_icon(char *value) {
	char *buf, *g, *p;
	char filename[32];
	memset(filename, 0, 32);

	g = buf = strdup(value);
	while (buf) {
		if ((p = strsep(&g, ">")) == NULL) break;

		if(strcmp(p, "")) {
			sprintf(filename, "/jffs/usericon/%s.log", p);
			//Delete exist file
			if(check_if_file_exist(filename)) {
				unlink(filename);
			}
		}
	}
	free(buf);
}
#endif

#define NVRAM_MODIFIED_BIT		1
#define NVRAM_MODIFIED_WL_BIT		2
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
#define NVRAM_MODIFIED_WL_QTN_BIT	4
#endif
#define NVRAM_MODIFIED_DUALWAN_ADDUSB		8	/* ex: {wan, none}  =>  {wan, usb} */
#define NVRAM_MODIFIED_DUALWAN_EXCHANGE		16	/* ex: {wan, usb}   =>  {usb, wan}   {wan, lan} => {lan, wan}*/
#define NVRAM_MODIFIED_DUALWAN_REBOOT		32	/* Other cases */
#define NVRAM_MODIFIED_DUALWAN_MODE			64  /* ex: FO => LB  or LB => FO */
#define NVRAM_MODIFIED_DUALWAN_REMOVEUSB	128	/* ex: {wan, usb}  =>  {wan, none} */

#ifdef RTCONFIG_CFGSYNC
int validate_instance(webs_t wp, char *name, json_object *root, json_object *cfg_root)
#else
int validate_instance(webs_t wp, char *name, json_object *root)
#endif
{
	char prefix[32], word[100], tmp[100], *next, *value;
	char prefix1[32], word1[100], *next1;
	int i=0; /*, j=0;*/
	int found = 0;
#ifdef RTCONFIG_MULTICAST_IPTV
	int unit = -1;
#endif

	// handle instance for wlx, wanx, lanx
	if(strncmp(name, "wl", 2)==0) {
		foreach(word, nvram_safe_get("wl_ifnames"), next) {
			SKIP_ABSENT_BAND_AND_INC_UNIT(i);
			sprintf(prefix, "wl%d_", i++);
			value = get_cgi_json(strcat_r(prefix, name+3, tmp),root);
			if(!value) {
				// find variable with subunit
				foreach(word1, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next1) {
					sprintf(prefix1, "%s_", word1);
					value = get_cgi_json(strcat_r(prefix1, name+3, tmp),root);
					//printf("find %s\n", tmp);
					if(value)
#ifndef RTCONFIG_REALTEK // non Realtek
						break;
#else
					{
						if (repeater_mode()) {
							if (strcmp(nvram_safe_get(tmp), value)) {
								nvram_check_and_set_for_prefix(name, tmp, value);
								//nvram_set(tmp, value);
								found = NVRAM_MODIFIED_BIT|NVRAM_MODIFIED_WL_BIT;
								cprintf("%s:%d set: %s=%s\n", __FUNCTION__, __LINE__, tmp, value);
							}
							strcpy(tmp+3, tmp+5);
						}
						break;
					}
#endif
				}
#ifdef RTCONFIG_REALTEK
				//cprintf("%s:%d \n",__FUNCTION__,__LINE__);
				if(sw_mode() == SW_MODE_REPEATER && found == 0)
				{//set root ap info
					sprintf(prefix1, "wl%d.1_", *(prefix+2)-'0');

					value = websGetVar(wp, strcat_r(prefix1, name+3, tmp), NULL);
					if(value)cprintf("%s:%d find %s value=%s\n",__FUNCTION__,__LINE__, tmp,value);
					if(value&& strcmp(nvram_safe_get(tmp), value))
					{
						nvram_check_and_set_for_prefix(name, tmp, value);
						//nvram_set(tmp, value);
						found = NVRAM_MODIFIED_BIT|NVRAM_MODIFIED_WL_BIT;
					}
					strcpy(tmp+3,tmp+5);
					if(value)cprintf("%s:%d find %s value=%s\n",__FUNCTION__,__LINE__, tmp,value);
				}
#endif /* RTCONFIG_REALTEK */
			}

			if(value && strcmp(nvram_safe_get(tmp), value)) {
				//printf("instance value %s=%s\n", tmp, value);
				dbG("nvram set %s = %s\n", tmp, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
				if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
					IFTTT_DEBUG("[HTTPD] nvram set %s = %s\n", tmp, value);
#endif
				nvram_check_and_set_for_prefix(name, tmp, value);
#ifdef RTCONFIG_LANTIQ
				wave_app_flag = wave_handle_app_flag(tmp, wave_app_flag);
#endif
				//nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT|NVRAM_MODIFIED_WL_BIT;
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
				if (!strncmp(tmp, "wl1", 3))
				{
					if (rpc_qtn_ready())
					{
						rpc_parse_nvram(tmp, value);
						found |= NVRAM_MODIFIED_WL_QTN_BIT;
					}
				}
#endif

#ifdef RTCONFIG_CFGSYNC
				save_changed_param(cfg_root, tmp);
#endif
			}
		}
	}
	else if(strncmp(name, "wan", 3)==0) {
		foreach(word, nvram_safe_get("wan_ifnames"), next) {
			sprintf(prefix, "wan%d_", i++);
			value = get_cgi_json(strcat_r(prefix, name+4, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_check_and_set_for_prefix(name, tmp, value);
				//nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}

#ifdef RTCONFIG_MULTICAST_IPTV
        if(nvram_match("switch_wantag", "movistar")){
                sprintf(prefix, "wan10_");//IPTV
                value = get_cgi_json(strcat_r(prefix, name+4, tmp), root);
                if(value && strcmp(nvram_safe_get(tmp), value)) {
			//dbG("nvram set %s = %s\n", tmp, value);
			nvram_check_and_set_for_prefix(name, tmp, value);
			found = NVRAM_MODIFIED_BIT;
		}

		sprintf(prefix, "wan11_");//VoIP
		value = get_cgi_json(strcat_r(prefix, name+4, tmp), root);
		if(value && strcmp(nvram_safe_get(tmp), value)) {
			//dbG("nvram set %s = %s\n", tmp, value);
			nvram_check_and_set_for_prefix(name, tmp, value);
			found = NVRAM_MODIFIED_BIT;
		}

		value = get_cgi_json("wan_unit", root);
		if(value){
			unit = atoi(value);
			if(unit != -1){
				value = get_cgi_json(name, root);
  				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);
				if(value && strcmp(nvram_safe_get(tmp), value)){
					//dbG("nvram set %s = %s\n", tmp, value);
					nvram_check_and_set_for_prefix(name, tmp, value);
					found = NVRAM_MODIFIED_BIT;
				}
			}
		}
	}
#endif
	}
#ifdef RTCONFIG_DSL
	else if(strncmp(name, "dsl", 3)==0) {
		for(i=0;i<8;i++) {
			sprintf(prefix, "dsl%d_", i++);
			value = get_cgi_json(strcat_r(prefix, name+4, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_check_and_set_for_prefix(name, tmp, value);
				//nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
#ifdef RTCONFIG_VDSL
		for(i=0;i<8;i++) {
			if(i)
				snprintf(prefix, sizeof(prefix), "dsl8.%d_", i);
			else
				snprintf(prefix, sizeof(prefix), "dsl8_");
			value = get_cgi_json(strcat_r(prefix, name+4, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_check_and_set_for_prefix(name, tmp, value);
				//nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
#endif
	}
#endif
	else if(strncmp(name, "lan", 3)==0) {
	}
// This seems to create default values for each unit.
// Is it really necessary?  lan_ does not seem to use it.
#ifdef RTCONFIG_OPENVPN
	else if(strncmp(name, "vpn_server_", 11)==0) {
		for(i=1;i<=OVPN_SERVER_MAX;i++) {
			sprintf(prefix, "vpn_server%d_", i);
			value = get_cgi_json(strcat_r(prefix, name+11, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_check_and_set_for_prefix(name, tmp, value);
				//nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
	}
	else if(strncmp(name, "vpn_crt_server_", 15)==0) {
		for(i=1;i<=OVPN_SERVER_MAX;i++) {
			sprintf(prefix, "vpn_crt_server%d_", i);
			value = get_cgi_json(strcat_r(prefix, name+15, tmp),root);
			if(value) {
				ovpn_key_t key_type;
				char buf[8000];

				if(!strcmp(name+15, "static")) {
					key_type = OVPN_SERVER_STATIC;
				}
				else if(!strcmp(name+15, "ca")) {
					key_type = OVPN_SERVER_CA;
				}
				else if(!strcmp(name+15, "crt")) {
					key_type = OVPN_SERVER_CERT;
				}
				else if(!strcmp(name+15, "key")) {
					key_type = OVPN_SERVER_KEY;
				}
				else if(!strcmp(name+15, "crl")) {
					key_type = OVPN_SERVER_CRL;
				}
				else if(!strcmp(name+15, "dh")) {
					key_type = OVPN_SERVER_DH;
				}
				else if(!strcmp(name+15, "extra")) {
					key_type = OVPN_SERVER_EXTRA;
				}
				else {
					continue;
				}

				get_ovpn_key(OVPN_TYPE_SERVER, i, key_type, buf, sizeof(buf));

				if(strcmp(buf, value)) {
					dbG("set %s = %s\n", tmp, value);
					set_ovpn_key(OVPN_TYPE_SERVER, i, key_type, value, NULL);
					found = NVRAM_MODIFIED_BIT;
				}
			}
		}
	}
	else if(strncmp(name, "vpn_client_", 11)==0) {
		for(i=1;i<=OVPN_CLIENT_MAX;i++) {
			sprintf(prefix, "vpn_client%d_", i);
			value = get_cgi_json(strcat_r(prefix, name+11, tmp),root);
			if(value && strcmp(nvram_safe_get(tmp), value)) {
				dbG("nvram set %s = %s\n", tmp, value);
				nvram_check_and_set_for_prefix(name, tmp, value);
				//nvram_set(tmp, value);
				found = NVRAM_MODIFIED_BIT;
			}
		}
	}
	else if(strncmp(name, "vpn_crt_client_", 15)==0) {
		for(i=1;i<=OVPN_CLIENT_MAX;i++) {
			sprintf(prefix, "vpn_crt_client%d_", i);
			value = get_cgi_json(strcat_r(prefix, name+15, tmp),root);
			if(value) {
				ovpn_key_t key_type;
				char buf[8000];

				if(!strcmp(name+15, "static")) {
					key_type = OVPN_CLIENT_STATIC;
				}
				else if(!strcmp(name+15, "ca")) {
					key_type = OVPN_CLIENT_CA;
				}
				else if(!strcmp(name+15, "crt")) {
					key_type = OVPN_CLIENT_CERT;
				}
				else if(!strcmp(name+15, "key")) {
					key_type = OVPN_CLIENT_KEY;
				}
				else if(!strcmp(name+15, "crl")) {
					key_type = OVPN_CLIENT_CRL;
				}
				else if(!strcmp(name+15, "extra")) {
					key_type = OVPN_CLIENT_EXTRA;
				}
				else {
					continue;
				}

				get_ovpn_key(OVPN_TYPE_CLIENT, i, key_type, buf, sizeof(buf));

				if(strcmp(buf, value)) {
					dbG("set %s = %s\n", tmp, value);
					set_ovpn_key(OVPN_TYPE_CLIENT, i, key_type, value, NULL);
					found = NVRAM_MODIFIED_BIT;
				}
			}
		}
	}
#endif
	return found;
}

static
int nvram_check(char *name, char *value, struct nvram_tuple *t, char *output)
{
	int ret = 0;
	//_dprintf("nvram_check: t->name = %s, t->len = %d, t->type = %d, value = %s, strlen(value) = %d\n", t->name, t->len, t->type, value, strlen(value));
	if(strlen(value) > t->len)
	{
		ret=1;
		_dprintf("nvram_check fail: nvram %s over length\n", t->name);
		logmessage("httpd", "nvram_check fail: nvram %s over length (%d > %d)", t->name, strlen(value), t->len);
	}
#if defined(RTCONFIG_NVRAM_ENCRYPT)
	else if(t->enc == 1){
		set_enc_nvram(name, value, output);
		ret=2;
	}
#elif defined(RTCONFIG_HTTPS)
	else if(!strcmp(name, "PM_SMTP_AUTH_PASS")){
		pwenc(value, output, t->len);
	}
#endif
	return ret;
}

static
int nvram_check_and_set_for_prefix(char *name, char *tmp, char *value)
{
	int ret = 0;
	char output[2048];
	memset(output, 0, sizeof(output));
	struct nvram_tuple *t;
	for (t = router_defaults; t->name; t++)
	{
		if(!strcmp(t->name, name)){
			ret = nvram_check(name, value, t, output);
#ifdef RTCONFIG_NVRAM_ENCRYPT
			if(ret == 2)
				value = output;
#endif
		}
	}
	//_dprintf("nvram_check_and_set: name = %s, value = %s, tmp = %s\n", name, value, tmp);

	if(ret == 0
#ifdef RTCONFIG_NVRAM_ENCRYPT
	|| ret == 2
#endif
	)
		nvram_set(tmp, value);

	return ret;
}

#if 0
static
int nvram_check_and_set(char *name, char *value)
{
	int ret = 0;
	char output[2048];
	memset(output, 0, sizeof(output));
	struct nvram_tuple *t;
	for (t = router_defaults; t->name; t++)
	{
		if(!strncmp(t->name, name, strlen(t->name))){
			ret = nvram_check(name, value, t, output);
#ifdef RTCONFIG_NVRAM_ENCRYPT
			if(ret == 2)
				value = output;
#endif
		}
	}
	//_dprintf("nvram_check_and_set: name = %s, value = %s\n", name, value);

	if(ret == 0
#ifdef RTCONFIG_NVRAM_ENCRYPT
	|| ret == 2
#endif
	)
		nvram_set(name, value);

	return ret;
}
#endif

static int is_passwd_default(){
	char *http_passwd = nvram_safe_get("http_passwd");
#ifdef RTCONFIG_NVRAM_ENCRYPT
	char dec_passwd[NVRAM_ENC_LEN];
	pw_dec(http_passwd, dec_passwd);
	http_passwd = dec_passwd;
#endif
	if(strcmp(nvram_default_get("http_passwd"), http_passwd) == 0)
		return 1;
	else
		return 0;
}


int validate_apply(webs_t wp, json_object *root) {
	struct nvram_tuple *t;
	char *value;
	char name[64];
	char tmp[3500], prefix[32];
	char dec_passwd[4096];
#ifdef RTCONFIG_NVRAM_ENCRYPT
	char dec_passwd2[128];
	char dec_passwd3[128];
#endif
	int unit=-1, subunit=-1;
	int nvram_modified = 0;
	int nvram_modified_wl = 0;
	int acc_modified = 0;
	int ret;
	int ckn_ret = 0;
#ifdef RTCONFIG_DUALWAN
	int wans_dualwan_usb = 0;
#endif
#ifdef RTCONFIG_USB
	char orig_acc[128], modified_acc[128], modified_pass[128];

	memset(orig_acc, 0, 128);
	memset(modified_acc, 0, 128);
	memset(modified_pass, 0, 128);
#endif
#ifdef RTCONFIG_CFGSYNC
	char *action_script = websGetVar(wp, "action_script", "");
	char cfg_action_script[256], acc_action_script[64], cfg_ver[9];
	json_object *cfg_root = json_object_new_object();

	memset(cfg_action_script, 0, sizeof(cfg_action_script));
	memset(acc_action_script, 0, sizeof(acc_action_script));
#endif

	/* go through each nvram value */
	for (t = router_defaults; t->name; t++)
	{
		snprintf(name, sizeof(name), t->name);

		value = get_cgi_json(name, root);

		if(!value || (!strncmp(name, "wan_", 4) && nvram_match("switch_wantag", "movistar"))) {
#ifdef RTCONFIG_CFGSYNC
			if((ret=validate_instance(wp, name,root, cfg_root)))
#else 
			if((ret=validate_instance(wp, name,root)))
#endif
			{
				if(ret&NVRAM_MODIFIED_BIT) nvram_modified = 1;
				if(ret&NVRAM_MODIFIED_WL_BIT) nvram_modified_wl = 1;
			}
		}
		else {

			memset(dec_passwd, 0, sizeof(dec_passwd));
			if((ckn_ret = nvram_check(name, value, t, dec_passwd)) == 1) {
				continue;
			}
#if defined(RTCONFIG_NVRAM_ENCRYPT)
			else if(ckn_ret == 2){
				value = dec_passwd;
			}
#elif defined(RTCONFIG_HTTPS)
			else if(!strcmp(name, "PM_SMTP_AUTH_PASS")){
				value = dec_passwd;
			}
#endif

#ifdef RTCONFIG_JFFS2USERICON
			if(strcmp(name, "custom_usericon"))
#endif
			_dprintf("value %s=%s\n", name, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
			if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
				IFTTT_DEBUG("[HTTPD] value %s=%s\n", name, value);
#endif
			// unit nvram should be in fron of each apply,
			// seems not a good design

			if(!strcmp(name, "wl_unit")
					|| !strcmp(name, "wan_unit")
					|| !strcmp(name, "lan_unit")
#ifdef RTCONFIG_DSL
					|| !strcmp(name, "dsl_unit")
#endif
#ifdef RTCONFIG_OPENVPN
					|| !strcmp(name, "vpn_server_unit")
					|| !strcmp(name, "vpn_client_unit")
#endif
					) {
				unit = atoi(value);
				if(unit != nvram_get_int(name)) {
					nvram_set_int(name, unit);
					nvram_modified=1;
				}
			}
			else if(!strcmp(name, "wl_subunit")) {
				subunit = atoi(value);
				if(subunit!=nvram_get_int(name)) {
					nvram_set_int(name, subunit);
					nvram_modified=1;
				}
			}
			else if(!strncmp(name, "wl_", 3) && unit != -1) {
				// convert wl_ to wl[unit], only when wl_unit is parsed
				if(subunit==-1||subunit==0)
					snprintf(prefix, sizeof(prefix), "wl%d_", unit);
				else snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
				(void)strcat_r(prefix, name+3, tmp);
				if(strcmp(nvram_safe_get(tmp), value))
				{
#ifdef RTCONFIG_LANTIQ
					wave_app_flag = wave_handle_app_flag(tmp, wave_app_flag);
#endif
					nvram_set(tmp, value);
					nvram_modified = 1;
					nvram_modified_wl = 1;
					_dprintf("set %s=%s\n", tmp, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
					if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
						IFTTT_DEBUG("[HTTPD] set %s=%s\n", tmp, value);
#endif

#ifdef RTCONFIG_LYRA_HIDE
					if( sw_mode() == SW_MODE_ROUTER &&
						( !strcmp(name, "wl_ssid") ||
						  !strcmp(name, "wl_auth_mode_x") ||
						  !strcmp(name, "wl_crypto") ||
						  !strcmp(name, "wl_wpa_psk") ||
						  !strcmp(name, "wl_radius_ipaddr") ||
						  !strcmp(name, "wl_radius_port") ||
						  !strcmp(name, "wl_radius_key") ||
						  !strcmp(name, "wl_wep_x") ||
						  !strcmp(name, "wl_key") ||
						  !strcmp(name, "wl_key1") ||
						  !strcmp(name, "wl_key2") ||
						  !strcmp(name, "wl_key3") ||
						  !strcmp(name, "wl_key4") ||
						  !strcmp(name, "wl_phrase_x")
						) ){
						char word[256], *next;
						int other_unit = 0;
						foreach (word, nvram_safe_get("wl_ifnames"), next) {
							if((other_unit != unit) && (subunit == -1 || subunit == 0)){
								snprintf(prefix, sizeof(prefix), "wl%d_", other_unit);
								(void)strcat_r(prefix, name+3, tmp);
								if(strcmp(nvram_safe_get(tmp), value))
								{
									nvram_set(tmp, value);
									_dprintf("set %s=%s\n", tmp, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
									if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
										IFTTT_DEBUG("[HTTPD] set %s=%s\n", tmp, value);
#endif
								}
							}
							other_unit++;
						}
					}
#endif

#ifdef RTCONFIG_REALTEK
					if(sw_mode() == SW_MODE_REPEATER && strstr(prefix,".1_"))
					{//set root ap info
						strcpy(tmp+3,tmp+5);
						if(strcmp(nvram_safe_get(tmp), value))
						{
							nvram_set(tmp, value);
							_dprintf("set %s=%s\n", tmp, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
							if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
								IFTTT_DEBUG("[HTTPD] set %s=%s\n", tmp, value);
#endif
						}
					}
#endif

#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
					if (unit == 1)
					{
						if (rpc_qtn_ready())
						{
							rpc_parse_nvram(tmp, value);
						}
					}
#endif

#ifdef RTCONFIG_CFGSYNC
					save_changed_param(cfg_root, tmp);
#endif
				}
			}
			else if(!strncmp(name, "wan_", 4) && unit != -1) {
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
					if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
						IFTTT_DEBUG("[HTTPD] set %s=%s\n", tmp, value);
#endif
				}
			}
			else if(!strncmp(name, "lan_", 4) && unit != -1) {
				snprintf(prefix, sizeof(prefix), "lan%d_", unit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
					if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
						IFTTT_DEBUG("[HTTPD] set %s=%s\n", tmp, value);
#endif
				}
			}
#ifdef RTCONFIG_DSL
			else if(!strcmp(name, "dsl_subunit")) {
				subunit = atoi(value);
				if(subunit!=nvram_get_int(name)) {
					nvram_set_int(name, subunit);
					nvram_modified=1;
				}
			}
			else if(!strncmp(name, "dsl_", 4) && unit != -1) {
				if(subunit==-1||subunit==0)
					snprintf(prefix, sizeof(prefix), "dsl%d_", unit);
				else
					snprintf(prefix, sizeof(prefix), "dsl%d.%d_", unit, subunit);
				(void)strcat_r(prefix, name+4, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#endif
#ifdef RTCONFIG_OPENVPN
			else if(!strncmp(name, "vpn_crt_server_", 15) && unit!=-1) {
				ovpn_key_t key_type;
				char buf[8000];

				snprintf(prefix, sizeof(prefix), "vpn_crt_server%d_", unit);
				(void)strcat_r(prefix, name+15, tmp);

				if(!strcmp(name+15, "static")) {
					key_type = OVPN_SERVER_STATIC;
				}
				else if(!strcmp(name+15, "ca")) {
					key_type = OVPN_SERVER_CA;
				}
				else if(!strcmp(name+15, "crt")) {
					key_type = OVPN_SERVER_CERT;
				}
				else if(!strcmp(name+15, "key")) {
					key_type = OVPN_SERVER_KEY;
				}
				else if(!strcmp(name+15, "crl")) {
					key_type = OVPN_SERVER_CRL;
				}
				else if(!strcmp(name+15, "dh")) {
					key_type = OVPN_SERVER_DH;
				}
				else if(!strcmp(name+15, "extra")) {
					key_type = OVPN_SERVER_EXTRA;
				}
				else {
					_dprintf("unknown key type %s\n", name);
					continue;
				}

				get_ovpn_key(OVPN_TYPE_SERVER, unit, key_type, buf, sizeof(buf));

				if(strcmp(buf, value)) {
					set_ovpn_key(OVPN_TYPE_SERVER, unit, key_type, value, NULL);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
			else if(!strncmp(name, "vpn_crt_client_", 15) && unit!=-1) {
				ovpn_key_t key_type;
				char buf[8000];

				snprintf(prefix, sizeof(prefix), "vpn_crt_client%d_", unit);
				(void)strcat_r(prefix, name+15, tmp);

				if(!strcmp(name+15, "static")) {
					key_type = OVPN_CLIENT_STATIC;
				}
				else if(!strcmp(name+15, "ca")) {
					key_type = OVPN_CLIENT_CA;
				}
				else if(!strcmp(name+15, "crt")) {
					key_type = OVPN_CLIENT_CERT;
				}
				else if(!strcmp(name+15, "key")) {
					key_type = OVPN_CLIENT_KEY;
				}
				else if(!strcmp(name+15, "crl")) {
					key_type = OVPN_CLIENT_CRL;
				}
				else if(!strcmp(name+15, "extra")) {
					key_type = OVPN_CLIENT_EXTRA;
				}
				else {
					_dprintf("unknown key type %s\n", name);
					continue;
				}

				get_ovpn_key(OVPN_TYPE_CLIENT, unit, key_type, buf, sizeof(buf));

				if(strcmp(buf, value)) {
					set_ovpn_key(OVPN_TYPE_CLIENT, unit, key_type, value, NULL);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
			else if(!strncmp(name, "vpn_server_", 11) && unit!=-1) {
				snprintf(prefix, sizeof(prefix), "vpn_server%d_", unit);
				(void)strcat_r(prefix, name+11, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
			else if(!strncmp(name, "vpn_client_", 11) && unit!=-1) {
				snprintf(prefix, sizeof(prefix), "vpn_client%d_", unit);
				(void)strcat_r(prefix, name+11, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#endif
			else if(!strncmp(name, "sshd_", 5)) {
				write_encoded_crt(name, value);
				nvram_modified = 1;
				_dprintf("set %s=%s\n", name, value);
			}
#ifdef HND_ROUTER
			else if(!strcmp(name, "dhcp_hostnames")) {
				jffs_nvram_set(name, value);
				_dprintf("set jffs %s = %s\n", name, value);
				 nvram_modified = 1;
			}
#endif
			else if(!strcmp(name, "amng_custom")) {
				write_custom_settings(value);
				_dprintf("set amng_custom to %s\n", value);
				nvram_modified = 1;
			}

#ifdef RTCONFIG_DISK_MONITOR
			else if(!strncmp(name, "diskmon_", 8)) {
				snprintf(prefix, sizeof(prefix), "usb_path%s_diskmon_", nvram_safe_get("diskmon_usbport"));
				(void)strcat_r(prefix, name+8, tmp);

				if(strcmp(nvram_safe_get(tmp), value)) {
					nvram_set(tmp, value);
					nvram_modified = 1;
					_dprintf("set %s=%s\n", tmp, value);
				}
			}
#endif
#ifdef RTCONFIG_JFFS2USERICON
			else if(!strcmp(name, "custom_usericon")) {
				(void)handle_upload_icon(value);
				nvram_set(name, "");
				nvram_modified = 1;
			}
			else if(!strcmp(name, "custom_usericon_del")) {
				(void)del_upload_icon(value);
				nvram_set(name, "");
				nvram_modified = 1;
			}
#endif
			// TODO: add other multiple instance handle here
			else if(strcmp(nvram_safe_get(name), value)) {

				// the flag is set only when username or password is changed
				if(!strcmp(t->name, "http_username")
						|| !strcmp(t->name, "http_passwd")){
#ifdef RTCONFIG_USB
					if(!strcmp(t->name, "http_username")){
						strncpy(orig_acc, nvram_safe_get(name), 128);
						strncpy(modified_acc, value, 128);
					}
					else if(!strcmp(t->name, "http_passwd")){
#ifdef RTCONFIG_NVRAM_ENCRYPT
						memset(dec_passwd2, 0, sizeof(dec_passwd2));
						pw_dec(value, dec_passwd2);
						strncpy(modified_pass, dec_passwd2, 128);
#else
						strncpy(modified_pass, value, 128);
#endif
					}

#endif
					acc_modified = 1;
					change_passwd = 1;
				}

#ifdef RTCONFIG_DUALWAN//Cherry Cho added for exchanging settings of dualwan in 2014/10/20.
				if(!strcmp(name, "wans_dualwan")){
					char *orig_wans_dualwan, *wans_dualwan;
					char *orig_primary, *orig_second, *new_primary, *new_second;
					char *orig, *new, *tmp, *tmp2;

					save_index_to_interface();
					save_interface_to_index(value);

					orig_wans_dualwan = nvram_safe_get("wans_dualwan");
					tmp = orig = strdup(orig_wans_dualwan);
					vstrsep(orig, " ", &orig_primary, &orig_second);

					wans_dualwan = value;
					tmp2 = new = strdup(value);
					vstrsep(new, " ", &new_primary, &new_second);
					//_dprintf("validate_apply: orig_primary = %s orig_second = %s new_primary = %s new_second = %s\n", orig_primary, orig_second, new_primary, new_second);

					if( strstr(orig_wans_dualwan, "none") && (strstr(orig_wans_dualwan, "usb") == NULL) &&
					    strstr(wans_dualwan, "usb") && (strstr(wans_dualwan, "none") == NULL) &&
					   (strstr(wans_dualwan, orig_primary) || strstr(wans_dualwan, orig_second)) ){ //Add USB
						wans_dualwan_usb |= NVRAM_MODIFIED_DUALWAN_ADDUSB;
					}
					else if( strstr(orig_wans_dualwan, "usb") && (strstr(orig_wans_dualwan, "none") == NULL) &&
					    strstr(wans_dualwan, "none") && (strstr(wans_dualwan, "usb") == NULL) &&
					   (strstr(wans_dualwan, orig_primary) || strstr(wans_dualwan, orig_second)) ){ //Remove USB
						wans_dualwan_usb |= NVRAM_MODIFIED_DUALWAN_REMOVEUSB;
					}
					/*else if(!strcmp(orig_primary, new_second) && !strcmp(orig_second, new_primary)){ // Exchange Value
						wans_dualwan_usb |= NVRAM_MODIFIED_DUALWAN_EXCHANGE;
					}*/
					else{
						wans_dualwan_usb |= NVRAM_MODIFIED_DUALWAN_REBOOT;
					}
					free(tmp);
					free(tmp2);
				}

				if( !strcmp(name, "wans_mode")  || !strcmp(name, "wandog_interval") || !strcmp(name, "wandog_maxfail") ||
					!strcmp(name, "wandog_enable") || !strcmp(name, "wans_lb_ratio") || !strcmp(name, "wans_routing_enable")){
						wans_dualwan_usb |= NVRAM_MODIFIED_DUALWAN_REBOOT;
				}
#endif
				if( !strcmp(name, "PM_MY_EMAIL") || !strcmp(name, "PM_SMTP_AUTH_USER") || !strcmp(name, "fb_email")){
					if (strchr(value, '`') != NULL)
						continue;
				}
#ifdef RTCONFIG_CFGSYNC
				save_changed_param(cfg_root, name);
#endif                           
				nvram_set(name, value);
#ifdef RTAC68U
				if (is_dpsta_repeater() && access_point_mode() && !strcmp(name, "wlc_psta") && !nvram_get_int(name))
				{
					int idx, subidx;
					char ssid[64], ssid2[64];

					nvram_set_int("wl0.1_bss_enabled", 0);
					nvram_set_int("wl1.1_bss_enabled", 0);

					for (idx = 0; idx < 2; idx++)
						for (subidx = 1; subidx < 4; subidx++) {
							memset(ssid, 0, sizeof(ssid));
							memset(ssid2, 0, sizeof(ssid2));
							strlcpy(ssid, get_default_ssid(idx, 0), sizeof(ssid));
							snprintf(ssid2, sizeof(ssid2), "%s_Guest%d", ssid, subidx);
							strlcpy(ssid, ssid2, sizeof(ssid));
							snprintf(prefix, sizeof(prefix), "wl%d.%d_", idx, subidx);
							nvram_set(strcat_r(prefix, "ssid", tmp), ssid);
						}
				}
#endif
				if(!strcmp(name, "wps_enable"))
					nvram_set("wps_enable_x", value);

				if(strcmp(name, "wans_dualwan") && strcmp(name, "wans_mode")) //not wans_dualwan
					nvram_modified = 1;
				_dprintf("set %s=%s\n", name, value);
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
				if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
					IFTTT_DEBUG("[HTTPD] set %s=%s\n", tmp, value);
#endif

#ifdef RTCONFIG_USB_MODEM
				if(!strcmp(name, "modem_lte_band"))
					notify_rc("setband");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
				else if(!strcmp(name, "modem_bytes_data_cycle") || !strcmp(name, "modem_bytes_data_limit") || !strcmp(name, "modem_bytes_data_warning")){
					notify_rc("restart_set_dataset");
				}
#endif
#endif
#if defined(RTCONFIG_LETSENCRYPT)
				if(!strcmp(name, "le_enable") && !strcmp(value, "0")){
					unlink(HTTPD_CERT);
					unlink(HTTPD_KEY);
				}
#endif
#if defined(RTCONFIG_AIHOME_TUNNEL)
				if(!strcmp(name, "ddns_enable_x") || !strcmp(name, "ddns_hostname_x") || !strcmp(name, "misc_http_x") || !strcmp(name, "misc_httpsport_x")){
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
					IFTTT_DEBUG("[HTTPD] nvram=%s is change and notice mastiff update\n", name);
#endif
					kill_pidfile_s(MASTIFF_PID_PATH, SIGUSR1);
				}
#endif

				if(!strcmp(name, "switch_wantag") && strcmp(value, "manual")){
					config_iptv_vlan(value);
				}

				if(!strncmp(name, "TM_EULA", 1) && !strncmp(value, "1", 1)){
					time_t now;
					char timebuf[100];

					now = time( (time_t*) 0 );
					sprintf(timebuf, rfctime(&now));
					nvram_set("TM_EULA_time", timebuf);
				}
			}
		}
	}

	if(acc_modified){
		// ugly solution?
#ifdef RTCONFIG_CFGSYNC
		if (nvram_match("x_Setting", "1") && pids("cfg_server"))
			strlcat(acc_action_script, "chpass", sizeof(acc_action_script));
		else
#endif
		notify_rc("chpass");

#ifdef RTCONFIG_USB
		if(strlen(orig_acc) <= 0)
			strncpy(orig_acc, nvram_safe_get("http_username"), 128);
		if(strlen(modified_pass) <= 0){
#ifdef RTCONFIG_NVRAM_ENCRYPT
			char *http_passwd_t = nvram_safe_get("http_passwd");
			memset(dec_passwd3, 0, sizeof(dec_passwd3));
			pw_dec(http_passwd_t, dec_passwd3);
			strncpy(modified_pass, dec_passwd3, 128);
#else
			strncpy(modified_pass, nvram_safe_get("http_passwd"), 128);
#endif
		}

		if(strlen(modified_acc) <= 0)
			mod_account(orig_acc, NULL, modified_pass);
		else
			mod_account(orig_acc, modified_acc, modified_pass);

#ifdef RTCONFIG_CFGSYNC
		if (nvram_match("x_Setting", "1") && pids("cfg_server")) {
			if (strlen(acc_action_script))
				strlcat(acc_action_script, ";", sizeof(acc_action_script));
			strlcat(acc_action_script, "restart_ftpsamba", sizeof(acc_action_script));
		}
		else
#endif
		notify_rc_and_wait("restart_ftpsamba");
#endif
	}

	/* go through each temp nvram value */
	/* but not support instance now */
	for (t = router_state_defaults; t->name; t++)
	{
		// skip some unhandled variables
		if(strcmp(t->name, "rc_service")==0)
			continue;

		memset(name, 0, 64);
		sprintf(name, "%s", t->name);

		value = websGetVar(wp, name, NULL);

		if(value) {

			memset(dec_passwd, 0, sizeof(dec_passwd));

			if((ckn_ret = nvram_check(name, value, t, dec_passwd)) == 1) {
				continue;
			}else if(ckn_ret == 2){
				value = dec_passwd;
			}

			if(strcmp(nvram_safe_get(name), value)) {
				nvram_set(name, value);
			}
		}
	}

#ifdef RTCONFIG_DSL
	if(nvram_match("dsltmp_qis_dsl_pvc_set", "1")) {
		update_dsl_iptv_variables();
		nvram_modified = 1;
	}
#endif

#ifdef RTCONFIG_DUALWAN
	nvram_modified |= wans_dualwan_usb;
	//_dprintf("validate_apply: nvram_modified = %X\n", nvram_modified);
#endif

	if(nvram_modified)
	{
#ifdef RTCONFIG_TR069
		if(pids("tr069")) {
			_dprintf("value change from web!\n");
			eval("sendtocli", "http://127.0.0.1:1234/web/value/change", "\"name=change\"");
		}
#endif
		// TODO: is it necessary to separate the different?
		if(nvram_match("x_Setting", "0")){

			int fromapp_flag = 0;
			char *current_page;

			fromapp_flag = check_user_agent(user_agent);

			current_page = websGetVar(wp, "current_page", NULL);
			if(current_page != NULL){
				if(!strstr(current_page, "QIS_"))
					nvram_set("x_Setting", "1");
			}else if(fromapp_flag != 0)
				nvram_set("x_Setting", "1");
		}

		if (nvram_modified_wl)
			nvram_set("w_Setting", "1");

#ifdef RTCONFIG_CFGSYNC
		if (nvram_match("x_Setting", "1") && pids("cfg_server") &&
			check_if_file_exist(CFG_SERVER_PID))
		{
			/* add action_script */
			if (!acc_modified && strlen(action_script) > 0)
				json_object_object_add(cfg_root, "action_script", json_object_new_string(action_script));
			else if (acc_modified && strlen(acc_action_script)) {
				strlcat(cfg_action_script, acc_action_script, sizeof(cfg_action_script));
				if (strlen(action_script)) {
					strlcat(cfg_action_script, ";", sizeof(cfg_action_script));
					strlcat(cfg_action_script, action_script, sizeof(cfg_action_script));
				}
				json_object_object_add(cfg_root, "action_script", json_object_new_string(cfg_action_script));
			}

			if (check_cfg_changed(cfg_root)) {
				/* save the changed nvram parameters */
				json_object_to_file(CFG_JSON_FILE, cfg_root);

				/* change cfg_ver when setting changed */
				memset(cfg_ver, 0, sizeof(cfg_ver));
				srand(time(NULL));
				snprintf(cfg_ver, sizeof(cfg_ver), "%d%d", rand(), rand());
				nvram_set("cfg_ver", cfg_ver);
				cfg_changed = 1;

				/* for not from web browser */
				if (acc_modified && strlen(acc_action_script) && root) {
					kill_pidfile_s(CFG_SERVER_PID, SIGUSR2);
					cfg_changed = 0;
				}
			}
			else
				cfg_changed = 0;
		}
#endif
#if defined(RTCONFIG_BT_CONN)
		lyraQIS_extension();
#endif

		nvram_commit();
	}

#ifdef RTCONFIG_CFGSYNC
        json_object_put(cfg_root);
#endif

	return nvram_modified;
}

bool isIP(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

bool isMAC(const char* mac) {
    int i = 0;
    int s = 0;

    while (*mac) {
        if (isxdigit(*mac)) {
            i++;
        }
        else if (*mac == ':') {
            if (i == 0 || i / 2 - 1 != s)
            break;
            ++s;
        }
        else {
            s = -1;
        }
        ++mac;
    }
    return (i == 12 && s == 5);
}

bool isNumber(const char*s) {
   char* e = NULL;
   (void) strtol(s, &e, 0);
   return e != NULL && *e == (char)0;
}

#ifdef RTCONFIG_ROG
#define ApiMaxNumRegister 32
#define ApiMaxNumPortforward 32
#define ApiMaxNumQos 32
static int ej_set_variables(int eid, webs_t wp, int argc, char_t **argv) {
	char *apiName;
	char *apiAction;
	char *webVar_1;
	char *webVar_2;
	char *webVar_3;
	char *webVar_4;
	char *webVar_5;
	char *webVar_6;
	int retStatus = 0;
	char retList[65536]={0};
	char *buf, *g, *p;
	char nvramTmp[4096]={0}, strTmp[4096]={0};
	int iCurrentListNum = 0;

	apiName = websGetVar(wp, "apiname", "");
	apiAction = websGetVar(wp, "action", "");
	_dprintf("[httpd] api.asp: apiname->%s, apiAction->%s\n", apiName, apiAction);

	if(!strcmp(apiName, "register")){
		char *name, *mac, *group, *type, *callback, *keeparp;

		if(!strcmp(apiAction, "add")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "mac", "");
			webVar_3 = websGetVar(wp, "group", "");
			webVar_4 = websGetVar(wp, "type", "");
			webVar_5 = websGetVar(wp, "callback", "");
			webVar_6 = websGetVar(wp, "keeparp", "");

			if(!strcmp(webVar_1, "") || !strcmp(webVar_2, "") ||
			   !strcmp(webVar_3, "") || !strcmp(webVar_4, "") ){
				retStatus = 3;
			}
			else if(strlen(webVar_1) > 32){
				retStatus = 3;
			}
			else if(!isMAC(webVar_2)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_3)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_4)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_5)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_6)){
				retStatus = 3;
			}
			else{
				g = buf = strdup(nvram_safe_get("custom_clientlist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;

					iCurrentListNum++;

					if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

					if(!strcmp(webVar_2, mac)){
						retStatus = 1;
						break;
					}
				}
				free(buf);

				// if this current list already is the maximum, can't be added.
				if((iCurrentListNum-1) == ApiMaxNumRegister)
					retStatus = 4;

				if(retStatus == 0){
					strcat(nvramTmp, "<");
					strcat(nvramTmp, webVar_1);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_2);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_3);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_4);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_5);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_6);
					strcat(nvramTmp, nvram_safe_get("custom_clientlist"));

					nvram_set("custom_clientlist", nvramTmp);
					nvram_commit();
				}
			}
		}
		else if(!strcmp(apiAction, "del")){
			webVar_1 = websGetVar(wp, "mac", "");

			if(!strcmp(webVar_1, "")){
				retStatus = 3;
			}
			else{
				retStatus = 2;
				g = buf = strdup(nvram_safe_get("custom_clientlist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;
					if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

					if(!strcmp(webVar_1, mac)){
						retStatus = 0;
						continue;
					}
					else{
						strcat(nvramTmp, "<");
						strcat(nvramTmp, name);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, mac);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, group);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, type);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, callback);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, keeparp);
					}
				}
				free(buf);

				if(retStatus == 0){
					nvram_set("custom_clientlist", nvramTmp);
					nvram_commit();
				}
			}
		}
		else if(!strcmp(apiAction, "list")){
			g = buf = strdup(nvram_safe_get("custom_clientlist"));
			strcat(retList, "<list>\n");
			while (buf) {
				if ((p = strsep(&g, "<")) == NULL) break;
				if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

				strcat(retList, "<device>\n");
				sprintf(strTmp, "<name>%s</name>\n", name);
				strcat(retList, strTmp);
				sprintf(strTmp, "<mac>%s</mac>\n", mac);
				strcat(retList, strTmp);
				sprintf(strTmp, "<group>%s</group>\n", group);
				strcat(retList, strTmp);
				sprintf(strTmp, "<type>%s</type>\n", type);
				strcat(retList, strTmp);
				sprintf(strTmp, "<callback>%s</callback>\n", callback);
				strcat(retList, strTmp);
				sprintf(strTmp, "<keeparp>%s</keeparp>\n", keeparp);
				strcat(retList, strTmp);
				strcat(retList, "</device>\n");
			}
			free(buf);
			strcat(retList, "</list>\n");
		}
		else{
			retStatus = 3;
		}
	}
	else if(!strcmp(apiName, "portforward")){
		char *name, *rport, *srcip, *lip, *lport, *proto;
		int cnt;

		if(!strcmp(apiAction, "enable")){
			if(nvram_match("vts_enable_x", "0")){
				nvram_set("vts_enable_x", "1");
				nvram_commit();
				notify_rc("restart_firewall");
			}
		}
		else if(!strcmp(apiAction, "disable")){
			if(nvram_match("vts_enable_x", "1")){
				nvram_set("vts_enable_x", "0");
				nvram_commit();
				notify_rc("restart_firewall");
			}
		}
		else if(!strcmp(apiAction, "add")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "rport", "");
			webVar_3 = websGetVar(wp, "lip", "");
			webVar_4 = websGetVar(wp, "lport", "");
			webVar_5 = websGetVar(wp, "proto", "");
			webVar_6 = websGetVar(wp, "src", "");

			if(!strcmp(webVar_1, "") || !strcmp(webVar_2, "") || !strcmp(webVar_3, "") ||
			   !strcmp(webVar_4, "") || !strcmp(webVar_5, "")){
				retStatus = 3;
			}
			else if(strlen(webVar_1) > 32){
				retStatus = 3;
			}
			else if(!isNumber(webVar_2)){
				retStatus = 3;
			}
			else if(!isIP(webVar_3)){
				retStatus = 3;
			}
			else if(!isNumber(webVar_4)){
				retStatus = 3;
			}
			else if(strcmp(webVar_5, "TCP") && strcmp(webVar_5, "UDP") &&
					strcmp(webVar_5, "BOTH") && strcmp(webVar_5, "OTHER")){
				retStatus = 3;
			}
			else{
				g = buf = strdup(nvram_safe_get("vts_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;

					iCurrentListNum++;

					if ((cnt = vstrsep(p, ">", &name, &rport, &lip, &lport, &proto, &srcip)) < 5)
						continue;
					else if (cnt < 6)
						srcip = "";

					if(!strcmp(webVar_1, name) && !strcmp(webVar_2, rport) &&
					   !strcmp(webVar_3, lip) && !strcmp(webVar_4, lport) &&
					   !strcmp(webVar_5, proto) && !strcmp(webVar_6, srcip)
					){
						retStatus = 1;
						break;
					}
				}
				free(buf);

				// if this current list already is the maximum, can't be added.
				if((iCurrentListNum-1) == ApiMaxNumPortforward)
					retStatus = 4;

				if(retStatus == 0){
					strcat(nvramTmp, "<");
					strcat(nvramTmp, webVar_1);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_2);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_3);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_4);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_5);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_6);
					strcat(nvramTmp, nvram_safe_get("vts_rulelist"));

					nvram_set("vts_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_firewall"); // need to reboot if ctf is enabled.
				}
			}
		}
		else if(!strcmp(apiAction, "del")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "rport", "");
			webVar_3 = websGetVar(wp, "lip", "");
			webVar_4 = websGetVar(wp, "lport", "");
			webVar_5 = websGetVar(wp, "proto", "");
			webVar_6 = websGetVar(wp, "src", "");

			if(!strcmp(webVar_1, "") && !strcmp(webVar_2, "") && !strcmp(webVar_3, "") &&
			   !strcmp(webVar_4, "") && !strcmp(webVar_5, "")){
				retStatus = 3;
			}
			else{
				retStatus = 2;
				g = buf = strdup(nvram_safe_get("vts_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;
					if ((cnt = vstrsep(p, ">", &name, &rport, &lip, &lport, &proto, &srcip)) < 5)
						continue;
					else if (cnt < 6)
						srcip = "";

					if(!strcmp(webVar_1, name) && !strcmp(webVar_2, rport) &&
					   !strcmp(webVar_3, lip) && !strcmp(webVar_4, lport) &&
					   !strcmp(webVar_5, proto) && !strcmp(webVar_6, srcip)
					){
						retStatus = 0;
						continue;
					}
					else{
						strcat(nvramTmp, "<");
						strcat(nvramTmp, name);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, rport);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, lip);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, lport);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, proto);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, srcip);
					}
				}
				free(buf);

				if(retStatus == 0){
					nvram_set("vts_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_firewall");
				}
			}
		}
		else if(!strcmp(apiAction, "list")){
			g = buf = strdup(nvram_safe_get("vts_rulelist"));
			strcat(retList, "<list>\n");
			while (buf) {
				if ((p = strsep(&g, "<")) == NULL) break;
				if ((cnt = vstrsep(p, ">", &name, &rport, &lip, &lport, &proto, &srcip)) < 5)
					continue;
				else if (cnt < 6)
					srcip = "";

				strcat(retList, "<item>\n");
				sprintf(strTmp, "<name>%s</name>\n", name);
				strcat(retList, strTmp);
				sprintf(strTmp, "<rport>%s</rport>\n", rport);
				strcat(retList, strTmp);
				sprintf(strTmp, "<lip>%s</lip>\n", lip);
				strcat(retList, strTmp);
				sprintf(strTmp, "<lport>%s</lport>\n", lport);
				strcat(retList, strTmp);
				sprintf(strTmp, "<proto>%s</proto>\n", proto);
				strcat(retList, strTmp);
				sprintf(strTmp, "<src>%s</src>\n", srcip);
				strcat(retList, strTmp);
				strcat(retList, "</item>\n");
			}
			free(buf);

			strcat(retList, "</list>\n");
		}
		else{
			retStatus = 3;
		}
	}
	else if(!strcmp(apiName, "qos")){
		char *desc, *addr, *port, *prio, *transferred, *proto;

		if(!strcmp(apiAction, "enable")){
			webVar_1 = websGetVar(wp, "upbw", "");
			webVar_2 = websGetVar(wp, "downbw", "");

			if(!strcmp(webVar_1, "") || !isNumber(webVar_1)){
				retStatus = 3;
			}
			else if(!strcmp(webVar_2, "") || !isNumber(webVar_2)){
				retStatus = 3;
			}
			else{
				nvram_set("qos_obw", webVar_1);
				nvram_set("qos_ibw", webVar_2);
				if(nvram_match("qos_enable", "1")){
					nvram_commit();
					notify_rc("restart_qos");
				}
				else{
					nvram_set("qos_enable", "1");
					nvram_commit();
					sys_reboot();
				}
			}
		}
		else if(!strcmp(apiAction, "disable")){
			if(nvram_match("qos_enable", "1")){
				nvram_set("qos_enable", "0");
				nvram_commit();
				sys_reboot();
			}
		}
		else if(!strcmp(apiAction, "add")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "src", "");
			webVar_3 = websGetVar(wp, "dstport", "");
			webVar_4 = websGetVar(wp, "proto", "");
			webVar_5 = websGetVar(wp, "transferred", "");
			webVar_6 = websGetVar(wp, "prio", "");

			if(!strcmp(webVar_1, "") || !strcmp(webVar_6, "")){
				retStatus = 3;
			}
			else if(!strcmp(webVar_2, "") && !strcmp(webVar_3, "") &&
					!strcmp(webVar_4, "") && !strcmp(webVar_5, "")){
				retStatus = 3;
			}
			else if(strlen(webVar_1) > 32){
				retStatus = 3;
			}
			else if(strcmp(webVar_2, "") && (!isMAC(webVar_2) && !isIP(webVar_2))){
				retStatus = 3;
			}
			else if(strcmp(webVar_4, "") &&
					(strcmp(webVar_4, "tcp") && strcmp(webVar_4, "udp") &&
					 strcmp(webVar_4, "tcp/udp") && strcmp(webVar_4, "any"))) {
				retStatus = 3;
			}
			else if(strcmp(webVar_6, "0") && strcmp(webVar_6, "1") &&
					strcmp(webVar_6, "2") && strcmp(webVar_6, "3") && strcmp(webVar_6, "4")){
				retStatus = 3;
			}
			else{
				g = buf = strdup(nvram_safe_get("qos_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;

					iCurrentListNum++;

					if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

					if(!strcmp(webVar_1, desc) && !strcmp(webVar_2, addr) &&
					   !strcmp(webVar_3, port) && !strcmp(webVar_4, proto) &&
					   !strcmp(webVar_5, transferred) && !strcmp(webVar_6, prio)
					){
						retStatus = 1;
						break;
					}
				}
				free(buf);

				// if this current list already is the maximum, can't be added.
				if((iCurrentListNum-1) == ApiMaxNumQos)
					retStatus = 4;

				if(retStatus == 0){
					strcat(nvramTmp, "<");
					strcat(nvramTmp, webVar_1);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_2);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_3);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_4);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_5);
					strcat(nvramTmp, ">");
					strcat(nvramTmp, webVar_6);
					strcat(nvramTmp, nvram_safe_get("qos_rulelist"));

					nvram_set("qos_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_qos"); // need to reboot if ctf is enabled.
				}
			}
		}
		else if(!strcmp(apiAction, "del")){
			webVar_1 = websGetVar(wp, "name", "");
			webVar_2 = websGetVar(wp, "src", "");
			webVar_3 = websGetVar(wp, "dstport", "");
			webVar_4 = websGetVar(wp, "proto", "");
			webVar_5 = websGetVar(wp, "transferred", "");
			webVar_6 = websGetVar(wp, "prio", "");

			if(!strcmp(webVar_1, "") && !strcmp(webVar_2, "") && !strcmp(webVar_3, "") &&
			   !strcmp(webVar_4, "") && !strcmp(webVar_5, "") && !strcmp(webVar_6, "")){
				retStatus = 3;
			}
			else{
				retStatus = 2;
				g = buf = strdup(nvram_safe_get("qos_rulelist"));
				while (buf) {
					if ((p = strsep(&g, "<")) == NULL) break;
					if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

					if(!strcmp(webVar_1, desc) && !strcmp(webVar_2, addr) &&
					   !strcmp(webVar_3, port) && !strcmp(webVar_4, proto) &&
					   !strcmp(webVar_5, transferred) && !strcmp(webVar_6, prio)
					){
						retStatus = 0;
						continue;
					}
					else{
						strcat(nvramTmp, "<");
						strcat(nvramTmp, desc);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, addr);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, port);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, proto);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, transferred);
						strcat(nvramTmp, ">");
						strcat(nvramTmp, prio);
					}
				}
				free(buf);

				if(retStatus == 0){
					nvram_set("qos_rulelist", nvramTmp);
					nvram_commit();
					notify_rc("restart_qos"); // need to reboot if ctf is enabled.
				}
			}
		}
		else if(!strcmp(apiAction, "list")){
			g = buf = strdup(nvram_safe_get("qos_rulelist"));
			strcat(retList, "<list>\n");

			sprintf(strTmp, "<enable>%s</enable>\n", nvram_get("qos_enable"));
			strcat(retList, strTmp);
			sprintf(strTmp, "<upbw>%s</upbw>\n", nvram_get("qos_obw"));
			strcat(retList, strTmp);
			sprintf(strTmp, "<downbw>%s</downbw>\n", nvram_get("qos_ibw"));
			strcat(retList, strTmp);

			while (buf) {
				if ((p = strsep(&g, "<")) == NULL) break;
				if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

				strcat(retList, "<item>\n");
				sprintf(strTmp, "<name>%s</name>\n", desc);
				strcat(retList, strTmp);
				sprintf(strTmp, "<src>%s</src>\n", addr);
				strcat(retList, strTmp);
				sprintf(strTmp, "<dstport>%s</dstport>\n", port);
				strcat(retList, strTmp);
				sprintf(strTmp, "<proto>%s</proto>\n", proto);
				strcat(retList, strTmp);
				sprintf(strTmp, "<transferred>%s</transferred>\n", transferred);
				strcat(retList, strTmp);
				sprintf(strTmp, "<prio>%s</prio>\n", prio);
				strcat(retList, strTmp);
				strcat(retList, "</item>\n");
			}
			free(buf);

			strcat(retList, "</list>\n");
		}
		else{
				retStatus = 3;
		}
	}
	else{
		retStatus = 3;
	}

	websWrite(wp, "<status>%d</status>\n", retStatus);
	if(!strcmp(apiAction, "list"))
		websWrite(wp, "%s", retList);
	return 0;
}
#endif

#if defined(RTCONFIG_DUALWAN) && defined(RTCONFIG_USB)
int usb_modem_plugged(){
	DIR *bus_usb;
	struct dirent *interface;
	char usb_port[8], port_path[8];
	char prefix[32];
	int modem_plugged = 0;

	if((bus_usb = opendir(USB_DEVICE_PATH)) == NULL)
		return 0;

	while((interface = readdir(bus_usb)) != NULL){
		if(interface->d_name[0] == '.')
			continue;

		if(!isdigit(interface->d_name[0]))
			continue;

		if(strchr(interface->d_name, ':'))
			continue;

		if(get_usb_port_by_string(interface->d_name, usb_port, 8) == NULL)
			continue;

		if(get_path_by_node(interface->d_name, port_path, 8) == NULL)
			continue;

		snprintf(prefix, 32, "usb_path%s", port_path);
		if(!strcmp(nvram_safe_get(prefix), "modem")){
			modem_plugged = 1;
			break;
		}
	}
	closedir(bus_usb);

	return modem_plugged;
}
#endif

static int ej_update_variables(int eid, webs_t wp, int argc, char_t **argv) {
	char *action_mode;
	char *action_script;
	char *action_wait;
	char *wan_unit = websGetVar(wp, "wan_unit", "0");
	char notify_cmd[128];
	int do_apply;
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ) || defined(RTCONFIG_CAPTIVE_PORTAL)
	int bss_sleep = 0;
	const int nr_guest = get_nr_guest_network(-1);
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ)
	char *rc_support = nvram_safe_get("rc_support");
	int rc_2g = find_word(rc_support, "2.4G")? 1 : 0;
	int rc_5g = find_word(rc_support, "5G")? 1 : 0;
	const int nr_band = rc_2g + rc_5g;
	int sleep1 = 0, sleep2 = 0;
	int delta1 = 0;
#endif
#endif
#if defined(RTCONFIG_QCA)
	int delta2 = 0;
#endif
#if defined(RTCONFIG_CAPTIVE_PORTAL)
	int delta3 = 0;
#endif
#ifdef RTCONFIG_DUALWAN
	char new_action_script[128], new_action_wait[16];
#endif
#if defined(RTCONFIG_DUALWAN) && defined(RTCONFIG_USB)
	int usb_modem_plug = usb_modem_plugged();
#endif

#if defined(RTCONFIG_RALINK)
	sleep1 = 10;
	sleep2 = 5;
	bss_sleep = 0;
#if defined(RTCONFIG_WLMODULE_MT7615E_AP)
	delta1 = 35;
#endif
#endif
#if defined(RTCONFIG_QCA)
#if defined(RTCONFIG_WIFI_QCA9557_QCA9882) || \
    defined(RTCONFIG_WIFI_QCA9990_QCA9990) || \
    defined(RTCONFIG_WIFI_QCA9994_QCA9994)
	sleep1 = 11;
	sleep2 = 5;
	bss_sleep = 3;
	delta1 = 5;
	delta2 = 11;
#elif defined(RTCONFIG_SOC_IPQ40XX)
	sleep1 = 13;
#endif
#endif
#if defined(RTCONFIG_LANTIQ)
	sleep1 = 15;
	sleep2 = 30;
	delta1 = 5;
#endif

#if defined(RTCONFIG_CAPTIVE_PORTAL)
	delta3 = -10;
#endif

	// assign control variables
	action_mode = websGetVar(wp, "action_mode", "");
	action_script = websGetVar(wp, "action_script", "restart_net");
	action_wait = websGetVar(wp, "action_wait", "5");

	_dprintf("update_variables: [%s] [%s] [%s]\n", action_mode, action_script, action_wait);

#ifdef RTCONFIG_REALTEK
	if(strcmp(action_script,"restart_all") == 0)
	{
		if (pids("udhcpc"))
		{
			killall("udhcpc", SIGUSR2);
			killall("udhcpc", SIGTERM);
		}
	}
#endif
	if ((do_apply = !strcmp(action_mode, "apply")) ||
	    !strcmp(action_mode, "apply_new"))
	{
		int has_modify;
		if (!(has_modify = validate_apply(wp, NULL))) {
			websWrite(wp, "<script>no_changes_and_no_committing();</script>\n");
		}
		else {
			websWrite(wp, "<script>done_committing();</script>\n");
		}
		if(do_apply || has_modify) {

#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
			/* early stop wps for QTN */
			if (strcmp(action_script, "restart_wireless") == 0
			  ||strcmp(action_script, "restart_net") == 0)
			{
#if 0
				if (rpc_qtn_ready())
				{
					rpc_qcsapi_wifi_disable_wps(WIFINAME, 1);

					if (nvram_get_int("wps_enable")){
						rpc_qcsapi_wifi_disable_wps(WIFINAME, !nvram_get_int("wps_enable"));
						qcsapi_wps_set_ap_pin(WIFINAME, nvram_safe_get("wps_device_pin"));
					}

				}
#endif
			}
#endif

#ifdef RTCONFIG_DUALWAN
			memset(new_action_script, 0, sizeof(new_action_script));
			memset(new_action_wait, 0, sizeof(new_action_wait));
			if( (has_modify & 1) == 0){
				if( ((has_modify & NVRAM_MODIFIED_DUALWAN_REBOOT) == NVRAM_MODIFIED_DUALWAN_REBOOT) 
#ifdef RTCONFIG_USB
				|| (((has_modify & NVRAM_MODIFIED_DUALWAN_ADDUSB) == NVRAM_MODIFIED_DUALWAN_ADDUSB) && usb_modem_plug) 
#endif
				)
				{
					if(!strstr(action_script, "reboot")){
						sprintf(new_action_script, "%s reboot", action_script);
						action_script = (char *)new_action_script;
						strcpy(new_action_wait, nvram_safe_get("reboot_time"));
						action_wait = (char *)new_action_wait;
					}
				}
#ifdef RTCONFIG_USB
				else if( (((has_modify & NVRAM_MODIFIED_DUALWAN_ADDUSB) == NVRAM_MODIFIED_DUALWAN_ADDUSB) && !usb_modem_plug) ||
						 ((has_modify & NVRAM_MODIFIED_DUALWAN_REMOVEUSB) == NVRAM_MODIFIED_DUALWAN_REMOVEUSB)/* ||
						 ((has_modify & NVRAM_MODIFIED_DUALWAN_EXCHANGE) == NVRAM_MODIFIED_DUALWAN_EXCHANGE) */){
					strcpy(new_action_script, "start_multipath");
					action_script = (char *)new_action_script;
					strcpy(new_action_wait, "10");
					action_wait = (char *)new_action_wait;
				}
#endif
			}
#endif

			if (strlen(action_script) > 0) {
				char *p1, p2[sizeof(notify_cmd)];

				if((p1 = strstr(action_script, "_wan_if")))
				{
					p1 += sizeof("_wan_if") - 1;
					strlcpy(p2, action_script, MIN(p1 - action_script + 1, sizeof(p2)));
					snprintf(notify_cmd, sizeof(notify_cmd), "%s %s%s", p2, wan_unit, p1);
				}
#if defined(RTCONFIG_POWER_SAVE)
				else if (strstr(action_script, "pwrsave")) {
					char tmp[64], tmp_script[sizeof(notify_cmd)], *next;
					*notify_cmd = '\0';
					strlcpy(tmp_script, action_script, sizeof(tmp_script));
					foreach_59(tmp, tmp_script, next) {
						if (!strcmp(tmp, "pwrsave"))
							continue;
						strlcat(notify_cmd, tmp, sizeof(notify_cmd));
						strlcat(notify_cmd, ";", sizeof(notify_cmd));
					}
					set_power_save_mode();
				}
#endif
				else
					strlcpy(notify_cmd, action_script, sizeof(notify_cmd));

				if(strcmp(action_script, "saveNvram"))
				{
#ifdef RTCONFIG_CFGSYNC
					if (nvram_match("x_Setting", "1") && cfg_changed && pids("cfg_server") &&
						check_if_file_exist(CFG_SERVER_PID))
					{
						/* trigger cfg_server to send notification */
						kill_pidfile_s(CFG_SERVER_PID, SIGUSR2);
						cfg_changed = 0;
					}
					else
#endif
					{
						nvram_set("freeze_duck", "15");
						notify_rc(notify_cmd);
					}
				}
			}
#if defined(RTCONFIG_RALINK) ||  defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ)
			if (strstr(action_script, "restart_wireless") || strstr(action_script, "restart_net")) {
				websWrite(wp, "<script>restart_needed_time(%d);</script>\n",
					atoi(action_wait) + sleep1 * nr_band + bss_sleep * nr_guest);
			}
			else
			if (strstr(action_script, "restart_net_and_phy") || strstr(action_script, "restart_all")) {
				websWrite(wp, "<script>restart_needed_time(%d);</script>\n",
					atoi(action_wait) + delta1 + sleep2 * nr_band + bss_sleep * nr_guest);
			}
			else
#endif			
#if defined(RTCONFIG_QCA)
			if (strstr(action_script, "restart_allnet")) {
				websWrite(wp, "<script>restart_needed_time(%d);</script>\n",
					atoi(action_wait) + delta2 + sleep2 * nr_band + bss_sleep * nr_guest);
			}
			else
#endif
#if defined(RTCONFIG_CAPTIVE_PORTAL)
			if (!strncmp(action_script, "set_captive_portal_wl", sizeof("set_captive_portal_wl") - 1)) {
				websWrite(wp, "<script>restart_needed_time(%d);</script>\n",
					atoi(action_wait) + delta3 + bss_sleep * nr_guest);
			}
			else
#endif
			websWrite(wp, "<script>restart_needed_time(%d);</script>\n", atoi(action_wait));
		}
	}
#if defined(RTCONFIG_USB_SMS_MODEM) && !defined(RTCONFIG_USB_MULTIMODEM)
	else if(!strcmp(action_script, "start_savesms")){
		int fd;
		char tmpfile[] = "/tmp/sms/SMS_XXXXXX";
		char *dest_number = websGetVar(wp, "dest_number", "");
		char *content = websGetVar(wp, "sms_content", "");
		int len;

		printf("Got dest_number: %s.\n", dest_number);
		printf("Got content:\n**********\n%s**********\n", content);
		len = strlen(content);
		printf("len = %d.\n", len);
		fd = mkstemp(tmpfile);
		write(fd, content, len);
		close(fd);

		nvram_set("freeze_duck", "3");
		snprintf(notify_cmd, 128, "%s %s %s", action_script, dest_number, tmpfile);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_sendsmsbyindex")){
		char *index = websGetVar(wp, "sms_index", "");

		printf("Got index: %s.\n", index);

		nvram_set("freeze_duck", "10");
		snprintf(notify_cmd, 128, "%s %s", action_script, index);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_sendsmsnow")){
		int fd;
		char tmpfile[] = "/tmp/sms/SMS_XXXXXX";
		char *dest_number = websGetVar(wp, "dest_number", "");
		char *content = websGetVar(wp, "sms_content", "");
		int len;

		printf("Got dest_number: %s.\n", dest_number);
		printf("Got content:\n**********\n%s**********\n", content);
		len = strlen(content);
		printf("len = %d.\n", len);
		fd = mkstemp(tmpfile);
		write(fd, content, len);
		close(fd);

		nvram_set("freeze_duck", "10");
		snprintf(notify_cmd, 128, "%s %s %s", action_script, dest_number, tmpfile);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_delsms")){
		char *index = websGetVar(wp, "sms_index", "");

		printf("Got index: %s.\n", index);

		nvram_set("freeze_duck", "3");
		snprintf(notify_cmd, 128, "%s %s", action_script, index);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_modsmsdraft")){
		int fd;
		char tmpfile[] = "/tmp/sms/SMS_XXXXXX";
		char *index = websGetVar(wp, "sms_index", "");
		char *dest_number = websGetVar(wp, "dest_number", "");
		char *content = websGetVar(wp, "sms_content", "");
		int len;

		printf("Got index: %s.\n", index);
		printf("Got dest_number: %s.\n", dest_number);
		printf("Got content:\n**********\n%s**********\n", content);

		len = strlen(content);
		printf("len = %d.\n", len);
		fd = mkstemp(tmpfile);
		write(fd, content, len);
		close(fd);

		nvram_set("freeze_duck", "3");
		snprintf(notify_cmd, 128, "%s %s %s %s", action_script, index, dest_number, tmpfile);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_savephonenum")){
		char *number = websGetVar(wp, "phonenum", "");
		char *name = websGetVar(wp, "phonename", "");

		printf("Got number=%s, name=%s.\n", number, name);
		nvram_set("freeze_duck", "3");
		snprintf(notify_cmd, 128, "%s %s %s", action_script, number, name);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_delphonenum")){
		char *index = websGetVar(wp, "phoneindex", "");

		printf("Got phoneindex=%s.\n", index);
		nvram_set("freeze_duck", "3");
		snprintf(notify_cmd, 128, "%s %s", action_script, index);
		notify_rc(notify_cmd);
	}
	else if(!strcmp(action_script, "start_modphonenum")){
		char *index = websGetVar(wp, "phoneindex", "");
		char *number = websGetVar(wp, "phonenum", "");
		char *name = websGetVar(wp, "phonename", "");

		printf("Got index=%s, number=%s, name=%s.\n", index, number, name);
		nvram_set("freeze_duck", "3");
		snprintf(notify_cmd, 128, "%s %s %s %s", action_script, index, number, name);
		notify_rc(notify_cmd);
	}
#endif

	return 0;
}

static int convert_asus_variables(int eid, webs_t wp, int argc, char_t **argv) {
	return 0;
}

static int asus_nvram_commit(int eid, webs_t wp, int argc, char_t **argv) {
	return 0;
}

static int ej_notify_services(int eid, webs_t wp, int argc, char_t **argv) {
	return 0;
}

char *Ch_conv(char *proto_name, int idx)
{
	char *proto;
	char qos_name_x[32];
	sprintf(qos_name_x, "%s%d", proto_name, idx);
	if (nvram_match(qos_name_x,""))
	{
		return NULL;
	}
	else
	{
		proto=nvram_get(qos_name_x);
		return proto;
	}
}

static int wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
		unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "wanstate = %d;\n", wan_state);
	websWrite(wp, "wansbstate = %d;\n", wan_sbstate);
	websWrite(wp, "wanauxstate = %d;\n", wan_auxstate);

	return 0;
}

static int dual_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	unit = WAN_UNIT_FIRST;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "first_wanstate = %d;\n", wan_state);
	websWrite(wp, "first_wansbstate = %d;\n", wan_sbstate);
	websWrite(wp, "first_wanauxstate = %d;\n", wan_auxstate);

	memset(prefix, 0, sizeof(prefix));
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	memset(tmp, 0, 100);
	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "second_wanstate = %d;\n", wan_state);
	websWrite(wp, "second_wansbstate = %d;\n", wan_sbstate);
	websWrite(wp, "second_wanauxstate = %d;\n", wan_auxstate);
#else
	websWrite(wp, "first_wanstate = -1;\n");
	websWrite(wp, "first_wansbstate = -1;\n");
	websWrite(wp, "first_wanauxstate = -1;\n");
	websWrite(wp, "second_wanstate = -1;\n");
	websWrite(wp, "second_wansbstate = -1;\n");
	websWrite(wp, "second_wanauxstate = -1;\n");
#endif

	return 0;
}

static int ajax_dualwanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	unit = WAN_UNIT_FIRST;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<first_wan>%d</first_wan>\n", wan_state);
	websWrite(wp, "<first_wan>%d</first_wan>\n", wan_sbstate);
	websWrite(wp, "<first_wan>%d</first_wan>\n", wan_auxstate);

	memset(prefix, 0, sizeof(prefix));
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	memset(tmp, 0, 100);
	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<second_wan>%d</second_wan>\n", wan_state);
	websWrite(wp, "<second_wan>%d</second_wan>\n", wan_sbstate);
	websWrite(wp, "<second_wan>%d</second_wan>\n", wan_auxstate);
#else
	websWrite(wp, "<first_wan>-1</first_wan>\n");
	websWrite(wp, "<first_wan>-1</first_wan>\n");
	websWrite(wp, "<first_wan>-1</first_wan>\n");
	websWrite(wp, "<second_wan>-1</second_wan>\n");
	websWrite(wp, "<second_wan>-1</second_wan>\n");
	websWrite(wp, "<second_wan>-1</second_wan>\n");
#endif

	return 0;
}


static int ajax_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
		unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<wan>%d</wan>\n", wan_state);
	websWrite(wp, "<wan>%d</wan>\n", wan_sbstate);
	websWrite(wp, "<wan>%d</wan>\n", wan_auxstate);

	return 0;
}

static int secondary_ajax_wanstate_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;

	/* current unit */
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "<secondary_wan>%d</secondary_wan>\n", wan_state);
	websWrite(wp, "<secondary_wan>%d</secondary_wan>\n", wan_sbstate);
	websWrite(wp, "<secondary_wan>%d</secondary_wan>\n", wan_auxstate);
#else
	websWrite(wp, "<secondary_wan>-1</secondary_wan>\n");
	websWrite(wp, "<secondary_wan>-1</secondary_wan>\n");
	websWrite(wp, "<secondary_wan>-1</secondary_wan>\n");
#endif

	return 0;
}

static int wanlink_hook(int eid, webs_t wp, int argc, char_t **argv){
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	char *dns = "";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	char *xdns = "";
	unsigned int xlease = 0, xexpires = 0;
	char *name = NULL;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
	unit = wan_primary_ifunit(); //Paul add 2013/7/24, get current working wan unit

	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
/*
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
*/
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		//if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

#ifdef RTCONFIG_USB
	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
#endif
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	if(name == NULL){
		websWrite(wp, "function wanlink_status() { return %d;}\n", status);
		websWrite(wp, "function wanlink_statusstr() { return '%s';}\n", statusstr[status]);
		websWrite(wp, "function wanlink_type() { return '%s';}\n", type);
		websWrite(wp, "function wanlink_ipaddr() { return '%s';}\n", ip);
		websWrite(wp, "function wanlink_netmask() { return '%s';}\n", netmask);
		websWrite(wp, "function wanlink_gateway() { return '%s';}\n", gateway);
		websWrite(wp, "function wanlink_dns() { return '%s';}\n", dns);
		websWrite(wp, "function wanlink_lease() { return %d;}\n", lease);
		websWrite(wp, "function wanlink_expires() { return %d;}\n", expires);
		websWrite(wp, "function is_private_subnet() { return '%d';}\n", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));
	}else if(!strcmp(name,"status"))
		websWrite(wp, "\"%d\"", status);
	else if(!strcmp(name,"statusstr"))
		websWrite(wp, "\"%s\"", statusstr[status]);
	else if(!strcmp(name,"type"))
		websWrite(wp, "\"%s\"", type);
	else if(!strcmp(name,"ipaddr"))
		websWrite(wp, "\"%s\"", ip);
	else if(!strcmp(name,"netmask"))
		websWrite(wp, "\"%s\"", netmask);
	else if(!strcmp(name,"gateway"))
		websWrite(wp, "\"%s\"", gateway);
	else if(!strcmp(name,"dns"))
		websWrite(wp, "\"%s\"", dns);
	else if(!strcmp(name,"lease"))
		websWrite(wp, "\"%d\"", lease);
	else if(!strcmp(name,"expires"))
		websWrite(wp, "\"%d\"", expires);
	else if(!strcmp(name,"private_subnet"))
		websWrite(wp, "\"%d\"", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xdns = nvram_safe_get(strcat_r(prefix, "xdns", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	if(name == NULL){
		websWrite(wp, "function wanlink_xtype() { return '%s';}\n", xtype);
		websWrite(wp, "function wanlink_xipaddr() { return '%s';}\n", xip);
		websWrite(wp, "function wanlink_xnetmask() { return '%s';}\n", xnetmask);
		websWrite(wp, "function wanlink_xgateway() { return '%s';}\n", xgateway);
		websWrite(wp, "function wanlink_xdns() { return '%s';}\n", xdns);
		websWrite(wp, "function wanlink_xlease() { return %d;}\n", xlease);
		websWrite(wp, "function wanlink_xexpires() { return %d;}\n", xexpires);
	}else if(!strcmp(name,"xtype"))
		websWrite(wp, "\"%s\"", xtype);
	else if(!strcmp(name,"xipaddr"))
		websWrite(wp, "\"%s\"", xip);
	else if(!strcmp(name,"xnetmask"))
		websWrite(wp, "\"%s\"", xnetmask);
	else if(!strcmp(name,"xgateway"))
		websWrite(wp, "\"%s\"", xgateway);
	else if(!strcmp(name,"xdns"))
		websWrite(wp, "\"%s\"", xdns);
	else if(!strcmp(name,"xlease"))
		websWrite(wp, "\"%d\"", xlease);
	else if(!strcmp(name,"xexpires"))
		websWrite(wp, "\"%d\"", xexpires);

	return 0;
}

static int first_wanlink_hook(int eid, webs_t wp, int argc, char_t **argv){
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	char *dns = "";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	char *xdns = "";
	unsigned int xlease = 0, xexpires = 0;

	unit = WAN_UNIT_FIRST;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
/*
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
*/
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		//if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

#ifdef RTCONFIG_USB
	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
#endif
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	websWrite(wp, "function first_wanlink_status() { return %d;}\n", status);
	websWrite(wp, "function first_wanlink_statusstr() { return '%s';}\n", statusstr[status]);
	websWrite(wp, "function first_wanlink_type() { return '%s';}\n", type);
	websWrite(wp, "function first_wanlink_ipaddr() { return '%s';}\n", ip);
	websWrite(wp, "function first_wanlink_netmask() { return '%s';}\n", netmask);
	websWrite(wp, "function first_wanlink_gateway() { return '%s';}\n", gateway);
	websWrite(wp, "function first_wanlink_dns() { return '%s';}\n", dns);
	websWrite(wp, "function first_wanlink_lease() { return %d;}\n", lease);
	websWrite(wp, "function first_wanlink_expires() { return %d;}\n", expires);

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xdns = nvram_safe_get(strcat_r(prefix, "xdns", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	websWrite(wp, "function first_wanlink_xtype() { return '%s';}\n", xtype);
	websWrite(wp, "function first_wanlink_xipaddr() { return '%s';}\n", xip);
	websWrite(wp, "function first_wanlink_xnetmask() { return '%s';}\n", xnetmask);
	websWrite(wp, "function first_wanlink_xgateway() { return '%s';}\n", xgateway);
	websWrite(wp, "function first_wanlink_xdns() { return '%s';}\n", xdns);
	websWrite(wp, "function first_wanlink_xlease() { return %d;}\n", xlease);
	websWrite(wp, "function first_wanlink_xexpires() { return %d;}\n", xexpires);

	return 0;
}

static int secondary_wanlink_hook(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_DUALWAN
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	char *dns = "";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	char *xdns = "";
	unsigned int xlease = 0, xexpires = 0;

	/* current unit */
	unit = WAN_UNIT_SECOND;
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
/*
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
*/
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	websWrite(wp, "function secondary_wanlink_status() { return %d;}\n", status);
	websWrite(wp, "function secondary_wanlink_statusstr() { return '%s';}\n", statusstr[status]);
	websWrite(wp, "function secondary_wanlink_type() { return '%s';}\n", type);
	websWrite(wp, "function secondary_wanlink_ipaddr() { return '%s';}\n", ip);
	websWrite(wp, "function secondary_wanlink_netmask() { return '%s';}\n", netmask);
	websWrite(wp, "function secondary_wanlink_gateway() { return '%s';}\n", gateway);
	websWrite(wp, "function secondary_wanlink_dns() { return '%s';}\n", dns);
	websWrite(wp, "function secondary_wanlink_lease() { return %d;}\n", lease);
	websWrite(wp, "function secondary_wanlink_expires() { return %d;}\n", expires);

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xdns = nvram_safe_get(strcat_r(prefix, "xdns", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	websWrite(wp, "function secondary_wanlink_xtype() { return '%s';}\n", xtype);
	websWrite(wp, "function secondary_wanlink_xipaddr() { return '%s';}\n", xip);
	websWrite(wp, "function secondary_wanlink_xnetmask() { return '%s';}\n", xnetmask);
	websWrite(wp, "function secondary_wanlink_xgateway() { return '%s';}\n", xgateway);
	websWrite(wp, "function secondary_wanlink_xdns() { return '%s';}\n", xdns);
	websWrite(wp, "function secondary_wanlink_xlease() { return %d;}\n", xlease);
	websWrite(wp, "function secondary_wanlink_xexpires() { return %d;}\n", xexpires);
#else
	websWrite(wp, "function secondary_wanlink_status() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_statusstr() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_type() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_ipaddr() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_netmask() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_gateway() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_dns() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_lease() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_expires() { return -1;}\n");

	websWrite(wp, "function secondary_wanlink_xtype() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xipaddr() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xnetmask() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xgateway() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xdns() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xlease() { return -1;}\n");
	websWrite(wp, "function secondary_wanlink_xexpires() { return -1;}\n");
#endif
	return 0;
}

static int wan_action_hook(int eid, webs_t wp, int argc, char_t **argv){
	char *action;
	int needed_seconds = 0;
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_enable[16];

	unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);
	memset(wan_enable, 0, 16);
	strcpy(wan_enable, strcat_r(prefix, "enable", tmp));

	// assign control variables
	action = websGetVar(wp, "wanaction", "");
	if (strlen(action) <= 0){
		fprintf(stderr, "No connect action in wan_action_hook!\n");
		return -1;
	}

	fprintf(stderr, "wan action: %s\n", action);

	// TODO: multiple interface
	if(!strcmp(action, "Connect")){
		nvram_set_int(wan_enable, 0);
		nvram_set("freeze_duck", "15");
		notify_rc("start_wan");
	}
	else if (!strcmp(action, "Disconnect")){
		nvram_set_int(wan_enable, 1);
		nvram_set("freeze_duck", "10");
		notify_rc("stop_wan");
	}

	websWrite(wp, "<script>restart_needed_time(%d);</script>\n", needed_seconds);

	return 0;
}

static int get_wan_unit_hook(int eid, webs_t wp, int argc, char_t **argv){
	int unit;

#ifdef RTCONFIG_DUALWAN
	int connected = 0;
	if(nvram_match("wans_mode", "lb")){
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++){
			if(is_wan_connect(unit)){
				connected = 1;
				break;
			}
		}

		if(!connected)
			unit = WAN_UNIT_FIRST;
	}
	else
#endif
		unit = wan_primary_ifunit();

	websWrite(wp, "%d", unit);

	return 0;
}

static int wanlink_state_hook(int eid, webs_t wp, int argc, char_t **argv){

	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	int wan_state = -1, wan_sbstate = -1, wan_auxstate = -1;
	int unit, status = 0;
	char *statusstr[2] = { "Disconnected", "Connected" };
	char *wan_proto, *type;
	char *ip = "0.0.0.0";
	char *netmask = "0.0.0.0";
	char *gateway = "0.0.0.0";
	char *dns = "";
	unsigned int lease = 0, expires = 0;
	char *xtype = "";
	char *xip = "0.0.0.0";
	char *xnetmask = "0.0.0.0";
	char *xgateway = "0.0.0.0";
	char *xdns = "";
	unsigned int xlease = 0, xexpires = 0;
	char *name = NULL;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
	}

	/* current unit */
#ifdef RTCONFIG_DUALWAN
	if(nvram_match("wans_mode", "lb"))
		unit = WAN_UNIT_FIRST;
	else
#endif
		unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);

	wan_state = nvram_get_int(strcat_r(prefix, "state_t", tmp));
	wan_sbstate = nvram_get_int(strcat_r(prefix, "sbstate_t", tmp));
	wan_auxstate = nvram_get_int(strcat_r(prefix, "auxstate_t", tmp));

	websWrite(wp, "\"wanstate\":\"%d\",\n", wan_state);
	websWrite(wp, "\"wansbstate\":\"%d\",\n", wan_sbstate);
	websWrite(wp, "\"wanauxstate\":\"%d\",\n", wan_auxstate);
	websWrite(wp, "\"autodet_state\":\"%d\",\n", nvram_get_int("autodet_state"));
	websWrite(wp, "\"autodet_auxstate\":\"%d\",\n", nvram_get_int("autodet_auxstate"));

	wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));

	if (dualwan_unit__usbif(unit)) {
		if(wan_state == WAN_STATE_CONNECTED){
			status = 1;
		}
		else{
			status = 0;
		}
	}
	else if(wan_state == WAN_STATE_DISABLED){
		status = 0;
	}
// DSLTODO, need a better integration
#ifdef RTCONFIG_DSL
	// if dualwan & enable lan port as wan
	// it always report disconnected
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#else
	//Some AUXSTATE is displayed for reference only
	else if(wan_auxstate == WAN_AUXSTATE_NOPHY && (nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOLINK)) {
		status = 0;
	}
#endif
/*
	else if(wan_auxstate == WAN_AUXSTATE_NO_INTERNET_ACTIVITY&&(nvram_get_int("web_redirect")&WEBREDIRECT_FLAG_NOINTERNET)) {
		status = 0;
	}
*/
	else if(!strcmp(wan_proto, "pppoe")
			|| !strcmp(wan_proto, "pptp")
			|| !strcmp(wan_proto, "l2tp")
			)
	{
		if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_STOPPED && wan_sbstate != WAN_STOPPED_REASON_PPP_LACK_ACTIVITY){
			status = 0;
		}
		else{
			status = 1;
		}
	}
	else{
		//if(wan_state == WAN_STATE_STOPPED && wan_sbstate == WAN_STOPPED_REASON_INVALID_IPADDR){
		if(wan_state == WAN_STATE_STOPPED){
			status = 0;
		}
		else if(wan_state == WAN_STATE_INITIALIZING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_CONNECTING){
			status = 0;
		}
		else if(wan_state == WAN_STATE_DISCONNECTED){
			status = 0;
		}
		else {
			// treat short lease time as disconnected
			if(!strcmp(wan_proto, "dhcp") &&
			  nvram_get_int(strcat_r(prefix, "lease", tmp)) <= 60 &&
			  is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp)))
			) {
				status = 0;
			}
			else {
				status = 1;
			}
		}
	}

#ifdef RTCONFIG_USB
	if (dualwan_unit__usbif(unit))
		type = "USB Modem";
	else
#endif
		type = wan_proto;

	if(status != 0){
		ip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
		netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		gateway = nvram_safe_get(strcat_r(prefix, "gateway", tmp));
		dns = nvram_safe_get(strcat_r(prefix, "dns", tmp));
		lease = nvram_get_int(strcat_r(prefix, "lease", tmp));
		if (lease > 0)
			expires = nvram_get_int(strcat_r(prefix, "expires", tmp)) - uptime();
	}

	if(!strcmp(name,"appobj")){
		websWrite(wp, "\"wanlink_status\":\"%d\",\n", status);
		websWrite(wp, "\"wanlink_statusstr\":\"%s\",\n", statusstr[status]);
		websWrite(wp, "\"wanlink_type\":\"%s\",\n", type);
		websWrite(wp, "\"wanlink_ipaddr\":\"%s\",\n", ip);
		websWrite(wp, "\"wanlink_netmask\":\"%s\",\n", netmask);
		websWrite(wp, "\"wanlink_gateway\":\"%s\",\n", gateway);
		websWrite(wp, "\"wanlink_dns\":\"%s\",\n", dns);
		websWrite(wp, "\"wanlink_lease\":\"%d\",\n", lease);
		websWrite(wp, "\"wanlink_expires\":\"%d\",\n", expires);
		websWrite(wp, "\"is_private_subnet\":\"%d\",\n", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));
	}else if(!strcmp(name,"status"))
		websWrite(wp, "%d", status);
	else if(!strcmp(name,"statusstr"))
		websWrite(wp, "%s", statusstr[status]);
	else if(!strcmp(name,"type"))
		websWrite(wp, "%s", type);
	else if(!strcmp(name,"ipaddr"))
		websWrite(wp, "%s", ip);
	else if(!strcmp(name,"netmask"))
		websWrite(wp, "%s", netmask);
	else if(!strcmp(name,"gateway"))
		websWrite(wp, "%s", gateway);
	else if(!strcmp(name,"dns"))
		websWrite(wp, "%s", dns);
	else if(!strcmp(name,"lease"))
		websWrite(wp, "%d", lease);
	else if(!strcmp(name,"expires"))
		websWrite(wp, "%d", expires);
	else if(!strcmp(name,"private_subnet"))
		websWrite(wp, "%d", is_private_subnet(nvram_safe_get(strcat_r(prefix, "ipaddr", tmp))));

	if (strcmp(wan_proto, "pppoe") == 0 ||
	    strcmp(wan_proto, "pptp") == 0 ||
	    strcmp(wan_proto, "l2tp") == 0) {
		int dhcpenable = nvram_get_int(strcat_r(prefix, "dhcpenable_x", tmp));
		xtype = (dhcpenable == 0) ? "static" :
			(strcmp(wan_proto, "pppoe") == 0 && nvram_match(strcat_r(prefix, "vpndhcp", tmp), "0")) ? "" : /* zeroconf */
			"dhcp";
		xip = nvram_safe_get(strcat_r(prefix, "xipaddr", tmp));
		xnetmask = nvram_safe_get(strcat_r(prefix, "xnetmask", tmp));
		xgateway = nvram_safe_get(strcat_r(prefix, "xgateway", tmp));
		xdns = nvram_safe_get(strcat_r(prefix, "xdns", tmp));
		xlease = nvram_get_int(strcat_r(prefix, "xlease", tmp));
		if (xlease > 0)
			xexpires = nvram_get_int(strcat_r(prefix, "xexpires", tmp)) - uptime();
	}

	if(!strcmp(name,"appobj")){
		websWrite(wp, "\"wanlink_xtype\":\"%s\",\n", xtype);
		websWrite(wp, "\"wanlink_xipaddr\":\"%s\",\n", xip);
		websWrite(wp, "\"wanlink_xnetmask\":\"%s\",\n", xnetmask);
		websWrite(wp, "\"wanlink_xgateway\":\"%s\",\n", xgateway);
		websWrite(wp, "\"wanlink_xdns\":\"%s\",\n", xdns);
		websWrite(wp, "\"wanlink_xlease\":\"%d\",\n", xlease);
		websWrite(wp, "\"wanlink_xexpires\":\"%d\"\n", xexpires);
	}else if(!strcmp(name,"xtype"))
		websWrite(wp, "%s", xtype);
	else if(!strcmp(name,"xipaddr"))
		websWrite(wp, "%s", xip);
	else if(!strcmp(name,"xnetmask"))
		websWrite(wp, "%s", xnetmask);
	else if(!strcmp(name,"xgateway"))
		websWrite(wp, "%s", xgateway);
	else if(!strcmp(name,"xdns"))
		websWrite(wp, "%s", xdns);
	else if(!strcmp(name,"xlease"))
		websWrite(wp, "%d", xlease);
	else if(!strcmp(name,"xexpires"))
		websWrite(wp, "%d", xexpires);

	return 0;
}

static int ej_get_ascii_parameter(int eid, webs_t wp, int argc, char_t **argv){
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp, *str;
	int ret = 0;

	if (argc < 1){
		websError(wp, 400,
			"get_parameter() used with no arguments, but at least one "
			"argument is required to specify the parameter name\n");
		return -1;
	}

	str = websGetVar(wp, argv[0], "");

	/* each char expands to %XX at max */
	ret = strlen(str) * sizeof(char)*3 + sizeof(char);
	if (ret > sizeof(tmp)) {
		buf = (char *)malloc(ret);
		if (buf == NULL) {
			csprintf("No memory.\n");
			return 0;
		}
	}

#ifdef RTCONFIG_UTF8_SSID
	char_to_ascii_safe_with_utf8(buf, str, ret);
#else
	char_to_ascii_safe(buf, str, ret);
#endif
	ret = websWrite(wp, "%s", buf);

	if (buf != tmp)
		free(buf);
	return ret;
}

static int ej_get_parameter(int eid, webs_t wp, int argc, char_t **argv){

	char *c;
	int ret = 0;

	if (argc < 1){
		websError(wp, 400,
				"get_parameter() used with no arguments, but at least one "
				"argument is required to specify the parameter name\n");
		return -1;
	}

	char *value = websGetVar(wp, argv[0], "");
	if(value != NULL){
		if(check_xss_blacklist(value, 0)){
			return ret;
		}
	}
	//websWrite(wp, "%s", value);
	for (c = websGetVar(wp, argv[0], ""); *c; c++){
		if (isalnum(*c) != 0 || *c == '-' || *c == '_' || *c == '.' || *c == '/' || *c == ':')
		{
			ret += websWrite(wp, "%c", *c);
		}
		else
		{
			ret += websWrite(wp, " ");
			//ret += websWrite(wp, "&#%d", *c);
		}
	}

	return ret;
}

unsigned int getpeerip(webs_t wp){
	int fd, ret;
	struct sockaddr peer;
	socklen_t peerlen = sizeof(struct sockaddr);
	struct sockaddr_in *sa;

#ifdef RTCONFIG_HTTPS
	if(do_ssl)
	{
		fd = ssl_stream_fd;
	}
	else
#endif
	{
		fd = fileno((FILE *)wp);
	}
	ret = getpeername(fd, (struct sockaddr *)&peer, &peerlen);
	sa = (struct sockaddr_in *)&peer;

	if (!ret){
//		csprintf("peer: %x\n", sa->sin_addr.s_addr);
		return (unsigned int)sa->sin_addr.s_addr;
	}
	else{
		csprintf("error: %d %d \n", ret, errno);
		return 0;
	}
}

extern long uptime(void);

static int login_state_hook(int eid, webs_t wp, int argc, char_t **argv){
	unsigned int ip, login_ip, login_port;
	char ip_str[16], login_ip_str[16];
	time_t login_timestamp_t;
	struct in_addr now_ip_addr, login_ip_addr;
	time_t now;
	const int MAX = 80;
	const int VALUELEN = 18;
	char buffer[MAX], values[6][VALUELEN];

	ip = getpeerip(wp);
	//csprintf("ip = %u\n",ip);

	now_ip_addr.s_addr = ip;
	memset(ip_str, 0, 16);
	strcpy(ip_str, inet_ntoa(now_ip_addr));
//	time(&now);
	now = uptime();

	login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));
	login_ip_addr.s_addr = login_ip;
	memset(login_ip_str, 0, 16);
	strcpy(login_ip_str, inet_ntoa(login_ip_addr));
//	login_timestamp = (unsigned long)atol(nvram_safe_get("login_timestamp"));
	login_timestamp_t = strtoul(nvram_safe_get("login_timestamp"), NULL, 10);
	login_port = (unsigned int)atol(nvram_safe_get("login_port"));

	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp){
		memset(buffer, 0, MAX);
		memset(values, 0, 6*VALUELEN);

		while (fgets(buffer, MAX, fp)){
			if (strstr(buffer, "br0") && !strstr(buffer, "00:00:00:00:00:00")){
				if (sscanf(buffer, "%s%s%s%s%s%s", values[0], values[1], values[2], values[3], values[4], values[5]) == 6){
					if (!strcmp(values[0], ip_str)){
						break;
					}
				}

				memset(values, 0, 6*VALUELEN);
			}

			memset(buffer, 0, MAX);
		}

		fclose(fp);
	}

	if (ip != 0 && login_ip == ip && login_port != 0 && login_port == http_port) {
		websWrite(wp, "function is_logined() { return 1; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);
		websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);
		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);

		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
//		time(&login_timestamp);
		login_timestamp_t = uptime();
	}
	else{
		websWrite(wp, "function is_logined() { return 0; }\n");
		websWrite(wp, "function login_ip_dec() { return '%u'; }\n", login_ip);

		if ((unsigned long)(now-login_timestamp_t) > 60)	//one minitues
			websWrite(wp, "function login_ip_str() { return '0.0.0.0'; }\n");
		else
			websWrite(wp, "function login_ip_str() { return '%s'; }\n", login_ip_str);

		websWrite(wp, "function login_ip_str_now() { return '%s'; }\n", ip_str);

		if (values[3] != NULL)
			websWrite(wp, "function login_mac_str() { return '%s'; }\n", values[3]);
		else
			websWrite(wp, "function login_mac_str() { return ''; }\n");
	}

	return 0;
}

static int ej_is_logined_hook(int eid, webs_t wp, int argc, char_t **argv){
	unsigned int ip, login_ip;
	char ip_str[16], login_ip_str[16];
	struct in_addr now_ip_addr, login_ip_addr;

	ip = getpeerip(wp);
	now_ip_addr.s_addr = ip;
	strlcpy(ip_str, inet_ntoa(now_ip_addr), sizeof(ip_str));

	login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));
	login_ip_addr.s_addr = login_ip;
	strlcpy(login_ip_str, inet_ntoa(login_ip_addr), sizeof(login_ip_str));

	if(strcmp(login_ip_str, "0.0.0.0") && strcmp(login_ip_str, ip_str))
		return websWrite(wp, "\"0\"");
	else
		return websWrite(wp, "\"1\"");
}

#ifdef RTCONFIG_FANCTRL
static int get_fanctrl_info(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	unsigned int *temp_24 = NULL;
	unsigned int *temp_50 = NULL;
	char buf[WLC_IOCTL_SMLEN];
	char buf2[WLC_IOCTL_SMLEN];

	strcpy(buf, "phy_tempsense");
	strcpy(buf2, "phy_tempsense");

	if ((ret = wl_ioctl("eth1", WLC_GET_VAR, buf, sizeof(buf))))
		return ret;

	if ((ret = wl_ioctl("eth2", WLC_GET_VAR, buf2, sizeof(buf2))))
		return ret;

	temp_24 = (unsigned int *)buf;
	temp_50 = (unsigned int *)buf2;
//	dbG("phy_tempsense 2.4G: %d, 5G: %d\n", *temp_24, *temp_50);

	ret += websWrite(wp, "[\"%d\", \"%d\", \"%d\", \"%s\"]", button_pressed(BTN_FAN), *temp_24, *temp_50, nvram_safe_get("fanctrl_dutycycle_ex"));

	return ret;
}
#endif

#ifdef RTCONFIG_BCMARM
static int get_cpu_temperature(int eid, webs_t wp, int argc, char_t **argv)
{
#ifdef HND_ROUTER
	FILE *fp;
	int temperature;

	if ((fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r")) != NULL) {
		fscanf(fp, "%d", &temperature);
		fclose(fp);
		return websWrite(wp, "%3.3f", (double) temperature / 1000);
	} else {
		return websWrite(wp, "%s", "");
	}
#else
	FILE *fp;
	int temperature = -1;

	if ((fp = fopen("/proc/dmu/temperature", "r")) != NULL) {
		if (fscanf(fp, "%*s %*s %*s %d%*s", &temperature) != 1)
			temperature = -1;
		fclose(fp);
	}

	return websWrite(wp, "%d", temperature);
#endif
}
#endif

static int get_machine_name(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret = 0;
	struct utsname utsn;

	uname(&utsn);

	ret += websWrite(wp, "%s", utsn.machine);

	return ret;
}

int ej_dhcp_leases(int eid, webs_t wp, int argc, char_t **argv)
{
	return 0;
}

int
ej_dhcpLeaseInfo(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	if (!nvram_get_int("dhcp_enable_x") || !is_router_mode())
		return ret;

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return ret;

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		ret += websWrite(wp,
			"<client>\n"
			    "<mac>value=%s</mac>\n"
			    "<hostname>value=%s</hostname>\n"
			"</client>\n", hwaddr, name);
	}
	fclose(fp);

	return ret;
}

int
ej_dhcpLeaseMacList(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;
	int name_len;
	char tmp[MAX_LINE_SIZE];
	char *buf = tmp;

	if (!nvram_get_int("dhcp_enable_x") || !is_router_mode()){
		ret += websWrite(wp, "[[\"\", \"\"]]");
		return ret;
	}

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r"))){
		ret += websWrite(wp, "[[\"\", \"\"]]");
		return ret;
	}

	ret += websWrite(wp, "[");
	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		/* each char expands to %XX at max */
		name_len = strlen(name) * sizeof(char)*3 + sizeof(char);

		if (name_len > sizeof(tmp)) {
			buf = (char *)malloc(name_len);
			if (buf == NULL) {
				csprintf("No memory.\n");
				break;
			}
		}

		char_to_ascii_safe(buf, name, name_len);

		ret += websWrite(wp,"[\"%s\", \"%s\"],", hwaddr, buf);
	}
	ret += websWrite(wp, "[\"\",\"\"]]");

	fclose(fp);

	if (buf != tmp)
		free(buf);

	return ret;
}

int
ej_lan_leases(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256], timestr[sizeof("999:59:59")];
	char *hwaddr, *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	ret += websWrite(wp, "%-32s %-15s %-17s %-9s\n",
		"Hostname", "IP Address", "MAC Address", "Expires");

	if (!nvram_get_int("dhcp_enable_x"))
		return ret;

	/* Refresh lease file to get actual expire time */
/*	nvram_set("flush_dhcp_lease", "1"); */
	killall("dnsmasq", SIGUSR2);
	sleep (1);
/*	while(nvram_match("flush_dhcp_lease", "1"))
		sleep(1); */

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return ret;

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		hwaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (strlen(name) > 32)
		{
			strcpy(name + 29, "...");
			name[32] = '\0';
		}

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			// hwaddr = next ? : "";
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		if (expires) {
			snprintf(timestr, sizeof(timestr), "%u:%02u:%02u",
			    expires / 3600,
			    expires % 3600 / 60,
			    expires % 60);
		}

		ret += websWrite(wp, "%-32s %-15s %-17s %-9s\n",
			name, ipaddr, hwaddr,
			expires ? timestr : "Static");
	}
	fclose(fp);

	return ret;
}

int
ej_IP_dhcpLeaseInfo(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	struct in_addr addr4;
	struct in6_addr addr6;
	char line[256];
	char *ipaddr, *name, *next;
	unsigned int expires;
	int ret = 0;

	if (!nvram_get_int("dhcp_enable_x") || !is_router_mode())
		return (ret + websWrite(wp, "[]"));

	/* Read leases file */
	if (!(fp = fopen("/var/lib/misc/dnsmasq.leases", "r")))
		return (ret + websWrite(wp, "[]"));

	ret += websWrite(wp, "[");
	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		strsep(&next, " ") ? : ""; // hwaddr
		ipaddr = strsep(&next, " ") ? : "";
		name = strsep(&next, " ") ? : "";

		if (inet_pton(AF_INET6, ipaddr, &addr6) != 0) {
			/* skip ipv6 leases, thay have no hwaddr, but client id */
			continue;
		} else if (inet_pton(AF_INET, ipaddr, &addr4) == 0)
			continue;

		ret += websWrite(wp,"['%s', '%s'],", ipaddr, name);
	}
	ret += websWrite(wp, "['', '']];");
	fclose(fp);

	return ret;
}

#ifdef RTCONFIG_IPV6
#define DHCP_LEASE_FILE		"/var/lib/misc/dnsmasq.leases"
#define IPV6_CLIENT_NEIGH	"/tmp/ipv6_neigh"
#define IPV6_CLIENT_INFO	"/tmp/ipv6_client_info"
//#define	IPV6_CLIENT_LIST	"/tmp/ipv6_client_list"	// Moved to httpd.h
#define	MAC			1
#define	HOSTNAME		2
#define	IPV6_ADDRESS		3
#define BUFSIZE			8192

static int compare_back(FILE *fp, int current_line, char *buffer);
static int check_mac_previous(char *mac);
static char *value(FILE *fp, int line, int token);
static void find_hostname_by_mac(char *mac, char *hostname);
static int total_lines = 0;

/* Init File and clear the content */
void init_file(char *file)
{
	FILE *fp;

	if ((fp = fopen(file ,"w")) == NULL) {
		_dprintf("can't open %s: %s", file,
			strerror(errno));
	}

	fclose(fp);
}

void save_file(const char *file, const char *fmt, ...)
{
	char buf[BUFSIZE];
	va_list args;
	FILE *fp;

	if ((fp = fopen(file ,"a")) == NULL) {
		_dprintf("can't open %s: %s", file,
			strerror(errno));
	}

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	va_start(args, fmt);
	fprintf(fp, "%s", buf);
	va_end(args);

	fclose(fp);
}

static char *get_stok(char *str, char *dest, char delimiter)
{
	char *p;

	p = strchr(str, delimiter);
	if (p) {
		if (p == str)
			*dest = '\0';
		else
			strlcpy(dest, str, p-str);

		p++;
	} else
		strcpy(dest, str);

	return p;
}

static char *value(FILE *fp, int line, int token)
{
	int i;
	static char temp[BUFSIZE], buffer[BUFSIZE];
	char *ptr;
	int temp_len;

	fseek(fp, 0, SEEK_SET);
	for(i = 0; i < line; i++) {
		memset(temp, 0, sizeof(temp));
		fgets(temp, sizeof(temp), fp);
		temp_len = strlen(temp);
		if (temp_len && temp[temp_len-1] == '\n')
			temp[temp_len-1] = '\0';
	}
	memset(buffer, 0, sizeof(buffer));
	switch (token) {
		case HOSTNAME:
			get_stok(temp, buffer, ' ');
			break;
		case MAC:
			ptr = get_stok(temp, buffer, ' ');
			if (ptr)
				get_stok(ptr, buffer, ' ');
			break;
		case IPV6_ADDRESS:
			ptr = get_stok(temp, buffer, ' ');
			if (ptr) {
				ptr = get_stok(ptr, buffer, ' ');
				if (ptr)
					ptr = get_stok(ptr, buffer, ' ');
			}
			break;
		default:
			_dprintf("error option\n");
			strcpy(buffer, "ERROR");
			break;
	}

	return buffer;
}

static int check_mac_previous(char *mac)
{
	FILE *fp;
	char temp[BUFSIZE];
	memset(temp, 0, sizeof(temp));

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_LIST,
			strerror(errno));

		return 0;
	}

	while (fgets(temp, BUFSIZE, fp)) {
		if (strstr(temp, mac)) {
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);

	return 0;
}

static int compare_back(FILE *fp, int current_line, char *buffer)
{
	int i = 0;
	char mac[32], compare_mac[32];

	buffer[strlen(buffer) -1] = '\0';
	strcpy(mac, value(fp, current_line, MAC));

	if (check_mac_previous(mac))
		return 0;

	for(i = 0; i<(total_lines - current_line); i++) {
		strcpy(compare_mac, value(fp, current_line + 1 + i, MAC));
		if (strcmp(mac, compare_mac) == 0) {
			strcat(buffer, ",");
			strcat(buffer, value(fp, current_line + 1 + i, IPV6_ADDRESS));
		}
	}
	save_file(IPV6_CLIENT_LIST, "%s\n", buffer);

	return 0;
}

static void find_hostname_by_mac(char *mac, char *hostname)
{
	FILE *fp;
	unsigned int expires;
	char *macaddr, *ipaddr, *host_name, *next;
	char line[256];

	if ((fp = fopen(DHCP_LEASE_FILE, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", DHCP_LEASE_FILE,
			strerror(errno));

		goto END;
	}

	while ((next = fgets(line, sizeof(line), fp)) != NULL)
	{
		if (sscanf(next, "%u ", &expires) != 1)
			continue;

		strsep(&next, " ");
		macaddr = strsep(&next, " ") ? : "";
		ipaddr = strsep(&next, " ") ? : "";
		host_name = strsep(&next, " ") ? : "";

		if (strncasecmp(macaddr, mac, 17) == 0) {
			fclose(fp);
			strlcpy(hostname, host_name, 64);
			return;
		}

		memset(macaddr, 0, 32);
		memset(ipaddr, 0, 128);
		memset(host_name, 0, 64);
	}
	fclose(fp);
END:
	strcpy(hostname, "<unknown>");
}

void get_ipv6_client_info()
{
	FILE *fp;
	char buffer[128], ipv6_addr[128], mac[32];
	char *ptr_end, hostname[64];
	doSystem("ip -f inet6 neigh show dev %s > %s", nvram_safe_get("lan_ifname"), IPV6_CLIENT_NEIGH);
	usleep(1000);

	if ((fp = fopen(IPV6_CLIENT_NEIGH, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_NEIGH,
			strerror(errno));

		return;
	}

	init_file(IPV6_CLIENT_INFO);
	while (fgets(buffer, 128, fp)) {
		int temp_len = strlen(buffer);
		if (temp_len && buffer[temp_len-1] == '\n')
			buffer[temp_len-1] = '\0';
		if ((ptr_end = strstr(buffer, "lladdr")))
		{
			ptr_end = ptr_end - 1;
			memset(ipv6_addr, 0, sizeof(ipv6_addr));
			strncpy(ipv6_addr, buffer, ptr_end - buffer);
			ptr_end = ptr_end + 8;
			memset(mac, 0, sizeof(mac));
			strncpy(mac, ptr_end, 17);
			find_hostname_by_mac(mac, hostname);
			if ( (ipv6_addr[0] == '2' || ipv6_addr[0] == '3')
				&& ipv6_addr[0] != ':' && ipv6_addr[1] != ':'
				&& ipv6_addr[2] != ':' && ipv6_addr[3] != ':')
				save_file(IPV6_CLIENT_INFO, "%s %s %s\n", hostname, mac, ipv6_addr);
		}

		memset(buffer, 0, sizeof(buffer));
	}
	fclose(fp);
}

void get_ipv6_client_list(void)
{
	FILE *fp;
	int line_index = 1;
	char temp[BUFSIZE];
	memset(temp, 0, sizeof(temp));
	init_file(IPV6_CLIENT_LIST);

	if ((fp = fopen(IPV6_CLIENT_INFO, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_INFO,
			strerror(errno));

		return;
	}

	total_lines = 0;
	while (fgets(temp, BUFSIZE, fp))
		total_lines++;
	fseek(fp, 0, SEEK_SET);
	memset(temp, 0, sizeof(temp));

	while (fgets(temp, BUFSIZE, fp)) {
		compare_back(fp, line_index, temp);
		value(fp, line_index, MAC);
		line_index++;
	}
	fclose(fp);

	line_index = 1;
}
#if 0
static int ipv6_client_numbers(void)
{
	FILE *fp;
	int numbers = 0;
	char temp[BUFSIZE];

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL)
	{
		_dprintf("can't open %s: %s", IPV6_CLIENT_LIST,
			strerror(errno));

		return 0;
	}

	while (fgets(temp, BUFSIZE, fp))
		numbers++;
	fclose(fp);

	return numbers;
}
#endif

int
ej_lan_ipv6_network(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[64+32+8192+1];
	char *hostname, *macaddr, ipaddrs[8192+1];
	char dnsbuf[INET6_ADDRSTRLEN*3 + 3], *next;
	char *wan_type, *wan_dns, *p;
	int service, i, ret = 0;

	if (!(ipv6_enabled() && is_routing_enabled())) {
		ret += websWrite(wp, "IPv6 disabled\n");
		return ret;
	}

	service = get_ipv6_service();
	switch (service) {
	case IPV6_NATIVE_DHCP:
		wan_type = nvram_get_int(ipv6_nvname("ipv6_dhcp_pd")) ? "Native with DHCP-PD" : "Native"; break;
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
		wan_type = "Passthrough"; break;
#endif
	case IPV6_6TO4:
		wan_type = "Tunnel 6to4"; break;
	case IPV6_6IN4:
		wan_type = "Tunnel 6in4"; break;
	case IPV6_6RD:
		wan_type = "Tunnel 6rd"; break;
	case IPV6_MANUAL:
		wan_type = "Static"; break;
	default:
		wan_type = "Disabled"; break;
	}

	ret += websWrite(wp, "%30s: %s\n", "IPv6 Connection Type", wan_type);
	ret += websWrite(wp, "%30s: %s\n", "WAN IPv6 Address",
			 getifaddr(get_wan6face(), AF_INET6, GIF_PREFIXLEN) ? : ((service == IPV6_MANUAL) ? nvram_safe_get(ipv6_nvname("ipv6_ipaddr")) : ""));
	ret += websWrite(wp, "%30s: %s\n", "WAN IPv6 Link-Local Address",
			 getifaddr(get_wan6face(), AF_INET6, GIF_LINKLOCAL | GIF_PREFIXLEN) ? : "");
	ret += websWrite(wp, "%30s: %s\n", "WAN IPv6 Gateway",
			 ipv6_gateway_address() ? : "");
#ifdef RTCONFIG_6RELAYD
	if (service == IPV6_PASSTHROUGH)
	ret += websWrite(wp, "%30s: %s\n", "LAN IPv6 Address",
			 getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, GIF_PREFIXLEN) ? : "");
	else
#endif
	ret += websWrite(wp, "%30s: %s/%d\n", "LAN IPv6 Address",
			 nvram_safe_get(ipv6_nvname("ipv6_rtr_addr")), nvram_get_int(ipv6_nvname("ipv6_prefix_length")));
	ret += websWrite(wp, "%30s: %s\n", "LAN IPv6 Link-Local Address",
			 getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, GIF_LINKLOCAL | GIF_PREFIXLEN) ? : "");
	if (service == IPV6_NATIVE_DHCP) {
		ret += websWrite(wp, "%30s: %s\n", "DHCP-PD",
				 nvram_get_int(ipv6_nvname("ipv6_dhcp_pd")) ? "Enabled" : "Disabled");
	}
#ifdef RTCONFIG_6RELAYD
	if (service == IPV6_PASSTHROUGH)
	ret += websWrite(wp, "%30s: %s\n", "LAN IPv6 Prefix",
			 getifaddr(nvram_safe_get("lan_ifname"), AF_INET6, GIF_PREFIX | GIF_PREFIXLEN) ? : "");
	else
#endif
	ret += websWrite(wp, "%30s: %s/%d\n", "LAN IPv6 Prefix",
			 nvram_safe_get(ipv6_nvname("ipv6_prefix")), nvram_get_int(ipv6_nvname("ipv6_prefix_length")));

	switch (service) {
	case IPV6_NATIVE_DHCP:
#ifdef RTCONFIG_6RELAYD
	case IPV6_PASSTHROUGH:
#endif
		if (nvram_get_int(ipv6_nvname("ipv6_dnsenable"))) {
			wan_dns = nvram_safe_get(ipv6_nvname("ipv6_get_dns"));
			//wan_domain = nvram_safe_get(ipv6_nvname("ipv6_get_domain"));
			break;
		}
		/* fall through */
	default:
		next = strcpy(dnsbuf, "");
		for (i = 1; i <= 3; i++) {
			char tmp[sizeof("ipv6_dnsXXX")];
			snprintf(tmp, sizeof(tmp), "ipv6_dns%d", i);
			next += snprintf(next, sizeof(dnsbuf) + dnsbuf - next,
				    *dnsbuf ? " %s" : "%s", nvram_safe_get(ipv6_nvname(tmp)));
		}
		wan_dns = dnsbuf;
		//wan_domain = "";
		break;
	}
	ret += websWrite(wp, "%30s: %s\n", "DNS Servers", wan_dns);
	//ret += websWrite(wp, "%30s: %s\n", "Domain Search List", wan_domain);

	ret += websWrite(wp, "\n\nIPv6 LAN Devices List\n");
	ret += websWrite(wp, "-------------------------------------------------------------------\n");
	ret += websWrite(wp, "%-32s %-17s %-39s\n",
			 "Hostname", "MAC Address", "IPv6 Address");

	/* Refresh lease file to get actual expire time */
	killall("dnsmasq", SIGUSR2);
	usleep(100 * 1000);

	get_ipv6_client_info();
	get_ipv6_client_list();

	if ((fp = fopen(IPV6_CLIENT_LIST, "r")) == NULL) {
		_dprintf("can't open %s: %s", IPV6_CLIENT_LIST, strerror(errno));
		return ret;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *ptr = buf;

		ptr = strsep(&ptr, "\n");
		hostname = strsep(&ptr, " ");
		macaddr = strsep(&ptr, " ");
		if (!macaddr || *macaddr == '\0' ||
		    !ptr || *ptr == '\0')
			continue;

		if (strlen(hostname) > 32)
			sprintf(hostname + 29, "...");

		ipaddrs[0] = '\0';
		p = ipaddrs;
		while (ptr && *ptr) {
			char *next = strsep(&ptr, ",\n");
			if (next && *next)
				p += snprintf(p, sizeof(ipaddrs) + ipaddrs - p, "%s%s", *ipaddrs ? ", " : "", next);
		}

		ret += websWrite(wp, "%-32s %-17s %-39s\n",
				 hostname, macaddr, ipaddrs);
	}
	fclose(fp);

	return ret;
}
#endif

#ifndef RTF_PREFIX_RT
#define RTF_PREFIX_RT	0x00080000
#endif
#ifndef RTF_EXPIRES
#define RTF_EXPIRES	0x00400000
#endif
#ifndef RTF_ROUTEINFO
#define RTF_ROUTEINFO	0x00800000
#endif

const static struct {
	unsigned int flag;
	char name;
} route_flags[] = {
	{ RTF_REJECT,    '!' },		/* be first */
	{ RTF_UP,        'U' },
	{ RTF_GATEWAY,   'G' },
	{ RTF_HOST,      'H' },
	{ RTF_REINSTATE, 'R' },		/* ipv4 only */
	{ RTF_DYNAMIC,   'D' },		/* ipv4 only */
	{ RTF_MODIFIED,  'M' },		/* ipv4 only */
#ifdef RTCONFIG_IPV6
	{ RTF_DEFAULT,   'D' },		/* ipv6 only */
	{ RTF_ADDRCONF,  'A' },		/* ipv6 only */
	{ RTF_PREFIX_RT, 'P' },		/* ipv6 only */
	{ RTF_NONEXTHOP, 'n' },		/* ipv6 only */
	{ RTF_EXPIRES,   'E' },		/* ipv6 only */
	{ RTF_ROUTEINFO, 'R' },		/* ipv6 only */
//	{ RTF_CACHE,     'C' },		/* ipv6 only */
#endif
};

#ifdef RTCONFIG_IPV6
int inet_raddr6_pton(const char *src, void *dst, void *buf)
{
	char *sptr = (char *) src;
	char *dptr = buf;
	int i;

	for (i = 0; *sptr && i < 32; i++) {
		if (i && (i % 4) == 0)
			*dptr++ = ':';
		*dptr++ = *sptr++;
	} *dptr = '\0';

	return inet_pton(AF_INET6, buf, dst);
}

#if 0
static int ipv6_route_table(webs_t wp)
{
	const struct in6_addr lroute = { { { 0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } };
	const struct in6_addr mroute = { { { 0xff,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } };
	FILE *fp;
	char buf[256], *str, *dev, *sflags, *route, fmt[sizeof("%999s")];
	char sdest[INET6_ADDRSTRLEN], snexthop[INET6_ADDRSTRLEN], ifname[16];
	char iflist[1024], ifitem[18];
	struct in6_addr dest, nexthop;
	int flags, ref, use, metric, prefix;
	int i, pass, maxlen, routing, skip, ret = 0;

	fp = fopen("/proc/net/ipv6_route", "r");
	if (fp == NULL)
		return 0;

	pass = maxlen = 0;
	strcpy(iflist, "");
	routing = is_routing_enabled();
again:
	if (pass) {
		ret += websWrite(wp, fmt, "Destination & Next Hop");
		ret += websWrite(wp, "%-9s%s\n",
				 "Flags", "Metric Ref    Use Type Iface");
	}
	while ((str = fgets(buf, sizeof(buf), fp)) != NULL) {
		if (sscanf(str, "%32s%x%*s%*x%32s%x%x%x%x%15s",
			   sdest, &prefix, snexthop,
			   &metric, &ref, &use, &flags, ifname) != 8)
			continue;

		/* Skip down and cache routes */
		if ((flags & (RTF_UP | RTF_CACHE)) != RTF_UP)
			continue;
		/* Skip interfaces here */
		if (strcmp(ifname, "lo") == 0)
			continue;

		/* Parse dst, reuse buf */
		if (inet_raddr6_pton(sdest, &dest, str) < 1)
			break;

		if (prefix || !IN6_IS_ADDR_UNSPECIFIED(&dest)) {
			inet_ntop(AF_INET6, &dest, sdest, sizeof(sdest));
			if (prefix != 128) {
				i = strlen(sdest);
				snprintf(sdest + i, sizeof(sdest) - i, "/%d", prefix);
			}
		} else
			snprintf(sdest, sizeof(sdest), "default");

		/* Parse nexthop, reuse buf */
		if (inet_raddr6_pton(snexthop, &nexthop, str) < 1)
			break;
		inet_ntop(AF_INET6, &nexthop, snexthop, sizeof(snexthop));

		/* Format addresses, reuse buf */
		route = str;
		i = snprintf(str, buf + sizeof(buf) - str, ((flags & RTF_NONEXTHOP) ||
			     IN6_IS_ADDR_UNSPECIFIED(&nexthop)) ? "%s" : "%s via %s",
			     sdest, snexthop);
		skip = (flags == RTF_UP) &&
		       ((IN6_ARE_ADDR_EQUAL(&dest, &lroute) && prefix == 64) ||
			(IN6_ARE_ADDR_EQUAL(&dest, &mroute) && prefix == 8));
		if (pass == 0) {
			if (maxlen < i)
				maxlen = i;
			if (!skip)
				snprintf(iflist, sizeof(iflist),"%s;%s;", iflist, ifname);
			continue;
		} else
			str += i + 1;

		/* Parse flags, reuse buf */
		sflags = str;
		for (i = 0; i < ARRAY_SIZE(route_flags); i++) {
			if (flags & route_flags[i].flag)
				*str++ = route_flags[i].name;
		}
		*str++ = '\0';

		/* Replace known interfaces with LAN/WAN/MAN */
		dev = NULL;
		if (nvram_match("lan_ifname", ifname)) /* br0, wl0, etc */
			dev = "LAN";
		else if (routing && strcmp(get_wan6face(), ifname) == 0)
			dev = "WAN";
		else if (skip) {
			snprintf(ifitem, sizeof(ifitem), ";%s;", ifname);
			if (strstr(iflist, ifitem) == NULL)
				continue;
		}

		ret = websWrite(wp, fmt, route);
		ret += websWrite(wp, "%-9s%-6d %-3d%7d %-4s %s\n",
				 sflags, metric, ref, use, dev ? : "", ifname);
	}
	if (pass++ == 0) {
		/* NB: 48 is to fit IPv4 routing table header */
		snprintf(fmt, sizeof(fmt), "%%-%ds", max(maxlen + 1, 48));
		if (maxlen > 0)
			rewind(fp);
		goto again;
	}
	fclose(fp);

	return ret;
}
#endif	// if 0
#endif

#if 0
static int ipv4_route_table(webs_t wp)
{
	FILE *fp;
	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char buf[256], *str, *dev, *sflags, *ifname;
	struct in_addr dest, gateway, mask;
	int flags, ref, use, metric;
	int i, unit, routing, ret = 0;

	fp = fopen("/proc/net/route", "r");
	if (fp == NULL)
		return 0;

	routing = is_routing_enabled();

	ret += websWrite(wp, "%-16s%-16s%-16s%-9s%s\n",
			 "Destination", "Gateway", "Genmask",
			 "Flags", "Metric Ref    Use Type Iface");

	while ((str = fgets(buf, sizeof(buf), fp)) != NULL) {
		ifname = strsep(&str, " \t");
		if (!str || ifname == str)
			continue;
		if (sscanf(str, "%x%x%x%d%u%d%x", &dest.s_addr, &gateway.s_addr,
			   &flags, &ref, &use, &metric, &mask.s_addr) != 7)
			continue;

		/* Skip interfaces here */
		if (strcmp(ifname, "lo") == 0)
			continue;

		/* Parse flags, reuse buf */
		sflags = str;
		for (i = 0; i < ARRAY_SIZE(route_flags); i++) {
			if (flags & route_flags[i].flag)
				*str++ = route_flags[i].name;
		}
		*str++ = '\0';

		/* Replace known interfaces with LAN/WAN/MAN */
		dev = NULL;
		if (nvram_match("lan_ifname", ifname)) /* br0, wl0, etc */
			dev = "LAN";
		else if (routing) {
			/* Tricky, it's better to move wan_ifunit/wanx_ifunit to shared instead */
			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
				wan_prefix(unit, prefix);
				if (nvram_match(strcat_r(prefix, "pppoe_ifname", tmp), ifname)) {
					dev = "WAN";
					break;
				}
				if (nvram_match(strcat_r(prefix, "ifname", tmp), ifname)) {
					char *wan_proto = nvram_safe_get(strcat_r(prefix, "proto", tmp));
					dev = (strcmp(wan_proto, "dhcp") == 0 ||
					       strcmp(wan_proto, "static") == 0 ) ? "WAN" : "MAN";
					break;
				}
			}
#ifdef RTCONFIG_DUALWAN
			if (dev) {
				snprintf(str, sizeof(buf) - (str - buf), "%s%d", dev, unit);
				dev = str;
			}
#endif
		}

		ret += websWrite(wp, "%-16s", (dest.s_addr || mask.s_addr) ? inet_ntoa(dest) : "default");
		ret += websWrite(wp, "%-16s", gateway.s_addr == INADDR_ANY ? "*" : inet_ntoa(gateway));
		ret += websWrite(wp, "%-16s%-9s%-6d %-3d%7d %-4s %s\n",
				 inet_ntoa(mask), sflags, metric, ref, use, dev ? : "", ifname);
	}
	fclose(fp);
	if (buf != tmp) free(buf);

	return ret;
}


int
ej_route_table(int eid, webs_t wp, int argc, char_t **argv)
{
	int ret;

	ret = ipv4_route_table(wp);

#ifdef RTCONFIG_IPV6
	if (get_ipv6_service() != IPV6_DISABLED) {
		ret += websWrite(wp, "\n"
				     "IPv6 routing table\n");
		ret += ipv6_route_table(wp);
	}
#endif

	return ret;
}
#endif // if 0

#ifdef RTCONFIG_NETOOL
int
send_netool_req(void *data)
{
	struct    sockaddr_un addr;
	int       sockfd, n;
	
	if ( (sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		_dprintf("[%s:(%d)] ERROR socket.\n", __FUNCTION__, __LINE__);
		perror("socket error");
		return 0;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, NETOOL_SOCKET_PATH, sizeof(addr.sun_path)-1);
	
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		_dprintf("[%s:(%d)] ERROR connecting:%s.\n", __FUNCTION__, __LINE__, strerror(errno));
		perror("connect error");
		close(sockfd);
		return 0;
	}
	
	n = write(sockfd, (REQUEST_INFO_T *)data, sizeof(REQUEST_INFO_T));
	
	close(sockfd);
	
	if(n < 0) {
		_dprintf("[%s:(%d)] ERROR writing:%s.\n", __FUNCTION__, __LINE__, strerror(errno));
		perror("writing error");
		return 0;
	}
	
	return 1;
}

static void
netool(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
	char *type      = NULL;
	char *ver       = NULL;
	char *target    = NULL;
	char *pcnt      = NULL;
	char *hops      = NULL;
	char *response  = NULL;
	char *exec      = NULL;
	char *netst     = NULL;
	char *sort      = NULL;
	char *proto     = NULL;
	char *srchost   = NULL;
	char *dsthost   = NULL;
	char *interface = NULL;
	
	type          = websGetVar(wp, "type"     , "");
	type          = check_cmd_whitelist(type) ? "" : type;
	ver            = websGetVar(wp, "ver"      , "");
	ver            = check_cmd_whitelist(ver) ? "" : ver;
	target       = websGetVar(wp, "target"   , "");
	target       = check_cmd_whitelist(target) ? "" : target;
	pcnt          = websGetVar(wp, "pcnt"     , "");
	pcnt          = check_cmd_whitelist(pcnt) ? "" : pcnt;
	hops         = websGetVar(wp, "hops"     , "");
	hops         = check_cmd_whitelist(hops) ? "" : hops;
	response  = websGetVar(wp, "response" , "");
	response  = check_cmd_whitelist(response) ? "" : response;
	exec          = websGetVar(wp, "exec"     , "");
	exec          = check_cmd_whitelist(exec) ? "" : exec;
	netst         = websGetVar(wp, "netst"    , "");
	netst         = check_cmd_whitelist(netst) ? "" : netst;
	sort           = websGetVar(wp, "sort"     , "");
	sort           = check_cmd_whitelist(sort) ? "" : sort;
	proto        = websGetVar(wp, "proto"    , "");
	proto        = check_cmd_whitelist(proto) ? "" : proto;
	srchost     = websGetVar(wp, "srchost"  , "");
	srchost     = check_cmd_whitelist(srchost) ? "" : srchost;
	dsthost     = websGetVar(wp, "dsthost"  , "");
	dsthost     = check_cmd_whitelist(dsthost) ? "" : dsthost;
	interface   = websGetVar(wp, "interface", "");
	interface   = check_cmd_whitelist(interface) ? "" : interface;

	REQUEST_INFO_T req_t;
	memset(&req_t, 0, sizeof(REQUEST_INFO_T));
	
	if (!(strcmp(type, "")) || strtoul(type, NULL, 10) < 0 || strtoul(type, NULL, 10) >= REQ_MODE_TOTAL) {
		websWrite(wp, "{\"successful\":\"0\"}");
		return;
	} 
	
	if (!(!strcmp(ver, "v4") || !strcmp(ver, "v6") || !strcmp(ver, ""))) {
		websWrite(wp, "{\"successful\":\"0\"}");
		return;
	}
	
	snprintf(req_t.ver, sizeof(req_t.ver), ver);
	snprintf(req_t.target, sizeof(req_t.target), target);
	snprintf(req_t.interface, sizeof(req_t.interface), "%s", (!strcmp(interface,"")) ? "" : interface);
	
	if (atoi(type) == REQ_PING_MODE) {
		
		req_t.type     = REQ_PING_MODE;
		req_t.ping_cnt = (!strcmp(pcnt    ,"")) ? 1 : (strtoul(pcnt    , NULL, 10) <= 0) ? 1 : atoi(pcnt);     /* Defalut as 1 */
		req_t.response = (!strcmp(response,"")) ? 5 : (strtoul(response, NULL, 10) <= 0) ? 5 : atoi(response); /* Defalut as 5 */
		req_t.exec_cnt = (!strcmp(exec    ,"")) ? 1 : (strtoul(exec    , NULL, 10) <= 0) ? 1 : atoi(exec);     /* Default as 1 */
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", req_t.target);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_TRACEROUTE_MODE) {
		
		req_t.type     = REQ_TRACEROUTE_MODE;
		req_t.hops     = (!strcmp(hops    ,"")) ? 20 : (strtoul(hops    , NULL, 10) <= 0) ? 20 : atoi(hops);    /* Default as 20 */
		req_t.response = (!strcmp(response,"")) ?  1 : (strtoul(response, NULL, 10) <= 0) ?  1 : atoi(response);/* Default as  1 */
		req_t.exec_cnt = (!strcmp(exec    ,"")) ?  1 : (strtoul(exec    , NULL, 10) <= 0) ?  1 : atoi(exec);    /* Default as  1 */
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", req_t.target);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_PING_NORMAL_MODE) {
		
		req_t.type     = REQ_PING_NORMAL_MODE;
		req_t.ping_cnt = (!strcmp(pcnt    ,"")) ?  5 : (strtoul(pcnt    , NULL, 10) <= 0) ?  5 : atoi(pcnt);     /* Defalut as  5 */
		req_t.response = (!strcmp(response,"")) ? 10 : (strtoul(response, NULL, 10) <= 0) ? 10 : atoi(response); /* Defalut as 10 */
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", NETOOL_RESULT_PING_NORMAL_LOG);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_TRACEROUTE_NORMAL_MODE) {
		
		req_t.type     = REQ_TRACEROUTE_NORMAL_MODE;
		req_t.hops     = (!strcmp(hops    ,"")) ? 30 : (strtoul(hops    , NULL, 10) <= 0) ? 30 : atoi(hops);    /* Default as 30 */
		req_t.response = (!strcmp(response,"")) ?  3 : (strtoul(response, NULL, 10) <= 0) ?  3 : atoi(response);/* Default as  3 */
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", NETOOL_RESULT_TRACERT_NORMAL_LOG);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_NETSTAT_MODE) {
		
		req_t.type     = REQ_NETSTAT_MODE;
		req_t.netst    = strtoul(netst, NULL, 16);
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", NETOOL_RESULT_NETSTAT_LOG);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_NETSTAT_NAT_MODE) {
		
		req_t.type     = REQ_NETSTAT_NAT_MODE;
		req_t.netst    = strtoul(netst, NULL, 16);
		
		snprintf(req_t.sort   , sizeof(req_t.sort)   , "%s", (!strcmp(sort,"")) ? "" : 
			(!strcmp(sort, "state") || !strcmp(sort, "src") || !strcmp(sort, "dst") ||
			 !strcmp(sort, "src-port") || !strcmp(sort, "dst-port")) ? sort : "");
		
		snprintf(req_t.proto  , sizeof(req_t.proto)  , "%s", (!strcmp(proto  ,"")) ? "" :
			(!strcmp(proto  ,"tcp") || !strcmp(proto  ,"udp") || !strcmp(proto  ,"igmp")) ? proto : "");
		
		snprintf(req_t.srchost, sizeof(req_t.srchost), "%s", (!strcmp(srchost,"")) ? "" : srchost);
		snprintf(req_t.dsthost, sizeof(req_t.dsthost), "%s", (!strcmp(dsthost,"")) ? "" : dsthost);
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", NETOOL_RESULT_NETSTAT_NAT_LOG);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_NSLOOKUP_MODE) {
		
		req_t.type     = REQ_NSLOOKUP_MODE;
		
		if (send_netool_req((void *)&req_t) > 0) {
			websWrite(wp, "{\"successful\":\"%s\"}", NETOOL_RESULT_NSLOOKUP_LOG);
		} else {
			websWrite(wp, "{\"successful\":\"0\"}");
		}
		
	} else if (atoi(type) == REQ_GET_RESULT) {
		
		FILE *fp;
		char buf[4096];
		char path[256];
		
		snprintf(path, sizeof(path), NETOOL_RESULT_DIR"/%s", req_t.target);
		
		if ((fp = fopen(path, "r")) != NULL) {
			
			websWrite(wp, "{\"result\":");
			while(fgets(buf, sizeof(buf), fp) != NULL) {
				websWrite(wp, "%s", buf);
			}
			fclose(fp);
			websWrite(wp, "}");
		}
		
	}
	return;
}
#endif

static int ej_get_arp_table(int eid, webs_t wp, int argc, char_t **argv){
	const int MAX = 80;
	const int FIELD_NUM = 6;
	const int VALUELEN = 18;
	char buffer[MAX], values[FIELD_NUM][VALUELEN];
	int num, firstRow;

	FILE *fp = fopen("/proc/net/arp", "r");
	if (fp){
		memset(buffer, 0, MAX);
		memset(values, 0, FIELD_NUM*VALUELEN);

		firstRow = 1;
		while (fgets(buffer, MAX, fp)){
			if (strstr(buffer, "br0") && !strstr(buffer, "00:00:00:00:00:00")){
				if (firstRow == 1)
					firstRow = 0;
				else
					websWrite(wp, ", ");

				if ((num = sscanf(buffer, "%s%s%s%s%s%s", values[0], values[1], values[2], values[3], values[4], values[5])) == FIELD_NUM){
					websWrite(wp, "[\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]", values[0], values[1], values[2], values[3], values[4], values[5]);
				}

				memset(values, 0, FIELD_NUM*VALUELEN);
			}

			memset(buffer, 0, MAX);
		}

		fclose(fp);
	}

	return 0;
}

#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
static int ej_get_ap_info(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[MAX_LINE_SIZE];
	int ret_l = 0;
	unsigned int i_l, len_l;
	int i;
	int lock;

	// get info from file generated by wlc_scan
	if(nvram_get_int("wlc_scan_state")!=WLCSCAN_STATE_FINISHED){
		ret_l += websWrite(wp, "[]");
		return ret_l;
	}
	lock = file_lock("sitesurvey");
	fp = fopen("/tmp/apscan_info.txt", "r");
	if( fp == NULL ){
		file_unlock(lock);
		csprintf("[httpd] open apscan_info.txt error\n");
		ret_l += websWrite(wp, "[]");
		return ret_l;
	}else{
		ret_l += websWrite(wp, "[");

		i = 0;
		while (fgets(buf, MAX_LINE_SIZE, fp)){
			if(i>0) ret_l += websWrite(wp, ",");
			len_l = strlen(buf);
			i_l = 0;
			while (i_l < len_l + 1){
				if(buf[i_l] == '\n') buf[i_l] = '\0';
				i_l++;
			}
			ret_l += websWrite(wp, "[");
			ret_l += websWrite(wp, "%s", buf);
			ret_l += websWrite(wp, "]");
			memset(buf, 0, MAX_LINE_SIZE);
			i++;
		}

		ret_l += websWrite(wp, "]");
	}
	fclose(fp);
	file_unlock(lock);
	return ret_l;

}
#endif

/******************************************************************************/
/*
 *	networkmap API
 */


#if 0
static int
check_macrepeat(char *maclist,char *mac){
	int total=0;
	while ( strstr(maclist,mac) != NULL ) 
   	{
		maclist += strlen(maclist);
		total++;
	}
	return total;
}
#endif
static int
check_macrepeat(struct json_object *macArray, char *mac){
	int total=0, arraylen, i;
	struct json_object *macStr;

	arraylen = json_object_array_length(macArray);
	for(i = 0; i < arraylen; i++) {
		macStr = json_object_array_get_idx(macArray, i);
		if(!strcmp(mac, json_object_get_string(macStr))) total++;
	}
	return total;
}

static int get_custom_clientlist_info(struct json_object *json_object_ptr) {
	int have_data = 0;
	char *buf, *g, *p;
	char *name, *mac, *group, *type, *callback, *keeparp;
	g = buf = strdup(nvram_safe_get("custom_clientlist"));
	struct json_object *client_attr = NULL;
	
	if(strcmp(buf, "") != 0) {
		while (buf) {
			if ((p = strsep(&g, "<")) == NULL) break;

			if((vstrsep(p, ">", &name, &mac, &group, &type, &callback, &keeparp)) != 6) continue;

			client_attr = json_object_new_object();
			json_object_object_add(client_attr, "name", json_object_new_string(name));
			json_object_object_add(client_attr, "group", json_object_new_string(group));
			json_object_object_add(client_attr, "type", json_object_new_int(atoi(type)));
			json_object_object_add(client_attr, "callback", json_object_new_string(callback));
			json_object_object_add(client_attr, "keeparp", json_object_new_string(keeparp));
			json_object_object_add(json_object_ptr, mac, client_attr);

			if(!have_data)
				have_data = 1;
		}
	}

	free(buf);

	return have_data;
}

static int get_qos_rulelist_info(struct json_object *json_object_ptr) {
	int have_data = 0;
	char *buf, *g, *p;
	char *desc, *mac, *port, *proto, *transferred, *prio;
	g = buf = strdup(nvram_safe_get("qos_rulelist"));

	if(strcmp(buf, "") != 0) {
		while (buf) {
			if ((p = strsep(&g, "<")) == NULL) break;

			if((vstrsep(p, ">", &desc, &mac, &port, &proto, &transferred, &prio)) != 6) continue;

			if(strcmp(mac, "")) {
				json_object_object_add(json_object_ptr, mac, json_object_new_string(prio));
				if(!have_data)
					have_data = 1;
			}
		}
	}

	free(buf);
	return have_data;
}

static int get_wtf_rulelist_info(struct json_object *json_object_ptr) {
	int have_data = 0;
	char *buf, *g, *p;
	char *status, *mac, *server1, *server2, *game;
	g = buf = strdup(nvram_safe_get("wtf_rulelist"));

	if(strcmp(buf, "") != 0) {
		while (buf) {
			if ((p = strsep(&g, "<")) == NULL) break;

			if((vstrsep(p, ">", &status, &mac, &server1, &server2, &game)) != 5) continue;

			if(strcmp(mac, "")) {
				json_object_object_add(json_object_ptr, mac, json_object_new_int(atoi(status)));
				if(!have_data)
					have_data = 1;
			}
		}
	}

	free(buf);
	return have_data;
}
static int check_internetState(char *timeList) {
	int state = 0;
	char *buf, *g, *p;
	int system_week = 0, system_hour = 0,  week_start = 0, week_end = 0, hour_start = 0, hour_end = 0;
	char time_item[2];

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	system_week = tm.tm_wday;
	system_hour = tm.tm_hour;
	if(system_week == 0)
		system_week = 7;

	g = buf = strdup(timeList);
	if(strcmp(buf, "") != 0) {
		while (buf) {
			if ((p = strsep(&g, "<")) == NULL) break;

			if(strcmp(p, "T")) {
				memset(time_item, 0 , 2);
				strncpy(time_item, p, 1);
				week_start = atoi(time_item);

				memset(time_item, 0 , 2);
				strncpy(time_item, p + 1, 1);
				week_end = atoi(time_item);

				memset(time_item, 0 , 2);
				strncpy(time_item, p + 2, 2);
				hour_start = atoi(time_item);

				memset(time_item, 0 , 2);
				strncpy(time_item, p + 4, 2);
				hour_end = atoi(time_item);
				
				if((week_start == 0 && week_end == 0 && hour_start == 0 && hour_end == 0) || week_start > week_end)
					week_end = 7;

				if(week_start == 0 && week_end == 7 && hour_start == 0 && hour_end == 0) { //all time setting
					state = 1;
					break;
				}
				else if(week_start == system_week && week_end == system_week) {
					if(hour_start <= system_hour && hour_end > system_hour) {
						state = 1;
						break;
					}
				}
				else if(week_start == system_week && week_end > system_week) {
					if(hour_start <= system_hour) {
						state = 1;
						break;
					}
				}
				else if(week_start < system_week && week_end > system_week) {
					state = 1;
					break;
				}
				else if(week_start < system_week && week_end >= system_week) {
					if(hour_end > system_hour) {
						state = 1;
						break;
					}
				}
				else if(week_start == 0 && week_end == 0 && system_week == 7) {//for sunday
					if(hour_start <= system_hour && hour_end > system_hour) {
						state = 1;
						break;
					}
				}
			}
		}
	}

	free(buf);
	return state;
}
static int get_multifilter_info(struct json_object *json_object_ptr) {
	int have_data = 0;
	char *buf, *g, *p, *timeList = NULL;
	struct json_object *statusArray = NULL, *timeArray = NULL, *multifilter_attr = NULL, *ruleIdxArray = NULL;
	int arraylen = 0, idx = 0, internetState = 0;
	char internetMode[8];
	int ruleStatus = 0;

	if (nvram_get_int("MULTIFILTER_ALL")) {
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
		g = buf = strdup(nvram_safe_get("MULTIFILTER_DEVICENAME"));
#else
		g = buf = strdup(nvram_safe_get("MULTIFILTER_MAC"));
#endif
		if(strcmp(buf, "") != 0) {
			ruleIdxArray = json_object_new_array();
			while (buf) {
				if ((p = strsep(&g, ">")) == NULL) break;

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
				json_object_array_add(ruleIdxArray, json_object_new_string(strtok(p, "@")));
#else
				json_object_array_add(ruleIdxArray, json_object_new_string(p));
#endif	
			}
			if(!have_data)
				have_data = 1;
		}
		else {
			free(buf);
			goto finish;
		}

		free(buf);
		
		g = buf = strdup(nvram_safe_get("MULTIFILTER_ENABLE"));
		if(strcmp(buf, "") != 0) {
			statusArray = json_object_new_array();
			while (buf) {
				if ((p = strsep(&g, ">")) == NULL) break;

				json_object_array_add(statusArray, json_object_new_string(p));
			}
		}

		free(buf);

		g = buf = strdup(nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"));
		if(strcmp(buf, "") != 0) {
			timeArray = json_object_new_array();
			while (buf) {
				if ((p = strsep(&g, ">")) == NULL) break;

				json_object_array_add(timeArray, json_object_new_string(p));	
			}
		}

		free(buf);
	}

finish:
	if (nvram_get_int("MULTIFILTER_ALL")) {
		if(ruleIdxArray != NULL) {
			arraylen = json_object_array_length(ruleIdxArray);
			for (idx = 0; idx < arraylen; idx++) {
				multifilter_attr = json_object_new_object();
				memset(internetMode, 0, 8);
				sprintf(internetMode, "allow");
				ruleStatus = 0;
				internetState = 1;

				if(statusArray != NULL && json_object_array_get_idx(statusArray, idx) != NULL) {
					ruleStatus = json_object_get_int(json_object_array_get_idx(statusArray, idx));
					if(ruleStatus == 1)
						sprintf(internetMode, "time");
					else if(ruleStatus == 2)
						sprintf(internetMode, "block");
				}

				json_object_object_add(multifilter_attr, "internetMode", json_object_new_string(internetMode));

				if(ruleStatus == 1) {
					if(timeArray != NULL && json_object_array_get_idx(timeArray, idx) != NULL) {
						if(!strcmp(json_object_get_string(json_object_array_get_idx(timeArray, idx)), "<"))
							internetState = 0;
						else {
							timeList = (char *) json_object_get_string(json_object_array_get_idx(timeArray, idx));
							internetState = check_internetState(timeList);
						}
					}
				}
				else if(ruleStatus == 2)
					internetState = 0;

				json_object_object_add(multifilter_attr, "internetState", json_object_new_int(internetState));

				json_object_object_add(json_object_ptr, json_object_get_string(json_object_array_get_idx(ruleIdxArray, idx)), multifilter_attr);
			}
		}
	}
	return have_data;
}
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
static int get_pms_device_info(struct json_object *json_object_ptr) {
	int have_data = 0;
	struct json_object *item = NULL;
	json_object *part_jarray = NULL;

	int ret=0;
	int dev_num, group_num, owned_group_num = 0;
	PMS_DEVICE_INFO_T *device_list, *follow_account;
	PMS_DEVICE_GROUP_INFO_T *group_list;

	// get the device list
	if(( ret = PMS_GetDeviceInfo(PMS_ACTION_GET_FULL, &device_list, &group_list, &dev_num, &group_num)) < 0){
		_dprintf("Can't read the account list.\n");
		return have_data;
	}

	for(follow_account = device_list; follow_account != NULL; follow_account = follow_account->next){
		owned_group_num = 0;
		item = json_object_new_object();
		part_jarray = json_object_new_array();

		json_object_object_add(item,"active", json_object_new_int(follow_account->active));
		json_object_object_add(item,"devname", json_object_new_string(follow_account->devname));
		json_object_object_add(item,"devtype", json_object_new_int(follow_account->devtype));
		json_object_object_add(item,"desc", json_object_new_string(follow_account->desc));

		PMS_OWNED_INFO_T *owned_group=follow_account->owned_group;
		while(owned_group!=NULL){
			PMS_DEVICE_GROUP_INFO_T *Group_owned=(PMS_DEVICE_GROUP_INFO_T *)owned_group->member;
			owned_group=owned_group->next;
			json_object_array_add(part_jarray,json_object_new_string(Group_owned->name));
			owned_group_num++;
		}
		json_object_object_add(item,"device_group", part_jarray);	//add Partition to item
		json_object_object_add(item,"owned_group_num", json_object_new_int(owned_group_num));
		json_object_object_add(json_object_ptr, follow_account->mac, item);

		if(!have_data)
			have_data = 1;
	}

	PMS_FreeDevInfo(&device_list, &group_list);
	return have_data;
}
#endif

#if defined(RTCONFIG_AMAS) || defined(RTCONFIG_WIFI_SON)
int get_amas_info(struct json_object *json_object_ptr) { //get cap and re info
	int have_data = 0;
	
	int shm_client_tbl_id;
	int lock;
	P_CM_CLIENT_TABLE p_client_tbl;
	void *shared_client_info=(void *) 0;
	int i = 0;
	int j = 0;
	char ip_buf[16] = {0};
	char alias_buf[33] = {0};
	char rmac_buf[32] = {0};
	char ap2g_buf[32] = {0};
	char ap5g_buf[32] = {0};
	char ap5g1_buf[32] = {0};
	char pap2g_buf[32] = {0};
	char pap5g_buf[32] = {0};
	char rssi2g_buf[8] = {0};
	char rssi5g_buf[8] = {0};
	char model_name_buf[33] = {0};
	char fwver_buf[33] = {0};
	char newfwver_buf[33] = {0};
	struct json_object *allBrMacListObj = NULL;
	struct json_object *macEntryObj = NULL;
	int online = 0;
	struct json_object *amas_client_attr = NULL;

	lock = file_lock(CFG_FILE_LOCK);
	shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
	if (shm_client_tbl_id == -1){
		fprintf(stderr, "shmget failed\n");
		file_unlock(lock);
		return 0;
	}

	shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr, "shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	allBrMacListObj = json_object_from_file(MAC_LIST_JSON_FILE);

	p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
	for(i = 0; i < p_client_tbl->count; i++) {
		char macList[1024] = {0};
		char *p = NULL;

		memset(alias_buf, 0, sizeof(alias_buf));
		memset(ip_buf, 0, sizeof(ip_buf));
		memset(rmac_buf, 0, sizeof(rmac_buf));
		memset(ap2g_buf, 0, sizeof(ap2g_buf));
		memset(ap5g_buf, 0, sizeof(ap5g_buf));
		memset(ap5g1_buf, 0, sizeof(ap5g1_buf));
		memset(pap2g_buf, 0, sizeof(pap2g_buf));
		memset(pap5g_buf, 0, sizeof(pap5g_buf));
		memset(rssi2g_buf, 0, sizeof(rssi2g_buf));
		memset(rssi5g_buf, 0, sizeof(rssi5g_buf));

		if (i == 0) /* master */
			snprintf(alias_buf, sizeof(alias_buf), "%s", nvram_safe_get("cfg_alias"));
		else
			snprintf(alias_buf, sizeof(alias_buf), "%s", p_client_tbl->alias[i]); 

		snprintf(ip_buf, sizeof(ip_buf), "%d.%d.%d.%d", p_client_tbl->ipAddr[i][0], p_client_tbl->ipAddr[i][1],
			p_client_tbl->ipAddr[i][2], p_client_tbl->ipAddr[i][3]);

		snprintf(rmac_buf, sizeof(rmac_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->realMacAddr[i][0], p_client_tbl->realMacAddr[i][1],
			p_client_tbl->realMacAddr[i][2], p_client_tbl->realMacAddr[i][3],
			p_client_tbl->realMacAddr[i][4], p_client_tbl->realMacAddr[i][5]);

		if (p_client_tbl->rssi2g[i] != 0) {
			snprintf(pap2g_buf, sizeof(pap2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->pap2g[i][0], p_client_tbl->pap2g[i][1],
				p_client_tbl->pap2g[i][2], p_client_tbl->pap2g[i][3],
				p_client_tbl->pap2g[i][4], p_client_tbl->pap2g[i][5]);
			snprintf(rssi2g_buf, sizeof(rssi2g_buf), "%d", p_client_tbl->rssi2g[i]);
		}

		if (p_client_tbl->rssi5g[i] != 0) {
			snprintf(pap5g_buf, sizeof(pap5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->pap5g[i][0], p_client_tbl->pap5g[i][1],
				p_client_tbl->pap5g[i][2], p_client_tbl->pap5g[i][3],
				p_client_tbl->pap5g[i][4], p_client_tbl->pap5g[i][5]);
			snprintf(rssi5g_buf, sizeof(rssi5g_buf), "%d", p_client_tbl->rssi5g[i]);
		}

		snprintf(ap2g_buf, sizeof(ap2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->ap2g[i][0], p_client_tbl->ap2g[i][1],
			p_client_tbl->ap2g[i][2], p_client_tbl->ap2g[i][3],
			p_client_tbl->ap2g[i][4], p_client_tbl->ap2g[i][5]);

		snprintf(ap5g_buf, sizeof(ap5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->ap5g[i][0], p_client_tbl->ap5g[i][1],
			p_client_tbl->ap5g[i][2], p_client_tbl->ap5g[i][3],
			p_client_tbl->ap5g[i][4], p_client_tbl->ap5g[i][5]);

		snprintf(ap5g1_buf, sizeof(ap5g1_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->ap5g1[i][0], p_client_tbl->ap5g1[i][1],
			p_client_tbl->ap5g1[i][2], p_client_tbl->ap5g1[i][3],
			p_client_tbl->ap5g1[i][4], p_client_tbl->ap5g1[i][5]);

		/* modle name */
		snprintf(model_name_buf, sizeof(model_name_buf), "%s", p_client_tbl->modelName[i]);

		/* firmware version */
		snprintf(fwver_buf, sizeof(fwver_buf), "%s", p_client_tbl->fwVer[i]);

		/* new firmware version */
		snprintf(newfwver_buf, sizeof(newfwver_buf), "%s", p_client_tbl->newFwVer[i]);

		if (allBrMacListObj) {
			json_object_object_get_ex(allBrMacListObj, rmac_buf, &macEntryObj);
			if (macEntryObj) {
				int macEntryLen = json_object_array_length(macEntryObj);

				if (macEntryLen) {
					memset(macList, 0, sizeof(macList));
					p = macList;
					p += sprintf(p, "[");
					for (j = 0; j < macEntryLen; j++) {
						struct json_object *entry = json_object_array_get_idx(macEntryObj, j);
						if (j) p += sprintf(p, ",");
						p += sprintf(p, "\"%s\"", json_object_get_string(entry));	
					}
					p += sprintf(p, "]");
				}
			}
		}


		if (i == 0)	/* DUT info */
			online = 1;
		else
			online = ((int) difftime(time(NULL), p_client_tbl->reportStartTime[i]) < OFFLINE_THRESHOLD) ? 1 : 0;

		amas_client_attr = json_object_new_object();
		json_object_object_add(amas_client_attr, "alias", json_object_new_string((strlen(alias_buf)) ? alias_buf : rmac_buf));
		json_object_object_add(amas_client_attr, "model_name", json_object_new_string(model_name_buf));
		json_object_object_add(amas_client_attr, "fwver", json_object_new_string(fwver_buf));
		json_object_object_add(amas_client_attr, "newfwver", json_object_new_string(newfwver_buf));
		json_object_object_add(amas_client_attr, "ip_buf", json_object_new_string(ip_buf));
		json_object_object_add(amas_client_attr, "online", json_object_new_int(online));
		json_object_object_add(amas_client_attr, "ap2g", json_object_new_string((strcmp(ap2g_buf, "00:00:00:00:00:00")) ? ap2g_buf : ""));
		json_object_object_add(amas_client_attr, "ap5g", json_object_new_string((strcmp(ap5g_buf, "00:00:00:00:00:00")) ? ap5g_buf : ""));
		json_object_object_add(amas_client_attr, "ap5g1", json_object_new_string((strcmp(ap5g1_buf, "00:00:00:00:00:00")) ? ap5g1_buf : ""));
		json_object_object_add(amas_client_attr, "wired_mac", json_object_new_string((strlen(macList)) ? macList : "[]"));
		json_object_object_add(amas_client_attr, "pap2g", json_object_new_string((strlen(pap2g_buf)) ? pap2g_buf : ""));
		json_object_object_add(amas_client_attr, "rssi2g", json_object_new_string((strlen(rssi2g_buf)) ? rssi2g_buf : ""));
		json_object_object_add(amas_client_attr, "pap5g", json_object_new_string((strlen(pap5g_buf)) ? pap5g_buf : ""));
		json_object_object_add(amas_client_attr, "rssi5g", json_object_new_string((strlen(rssi5g_buf)) ? rssi5g_buf : ""));
		json_object_object_add(amas_client_attr, "type", json_object_new_string((i == 0) ? "CAP" : "RE"));
		json_object_object_add(json_object_ptr, rmac_buf, amas_client_attr);

		if(!have_data)
			have_data = 1;
	}

	shmdt(shared_client_info);

	if (allBrMacListObj)
		json_object_put(allBrMacListObj);

	file_unlock(lock);

	return have_data;
}
static int get_amas_re_client_info(struct json_object *json_object_ptr) { //get re client info
	int have_data = 0;

	int shm_client_tbl_id;
	int lock;
	void *shared_client_info = (void *) 0;
	json_object *wClietListObj = NULL;
	json_object *brMacObj = NULL;
	json_object *bandObj = NULL;
	json_object *staObj = NULL;
	struct json_object *amas_re_client_attr = NULL;

	lock = file_lock(ALLWEVENT_FILE_LOCK);
	wClietListObj = json_object_from_file(ALLWCLIENT_LIST_JSON_PATH);
	file_unlock(lock);

	if (wClietListObj) {
		P_CM_CLIENT_TABLE p_client_tbl;
		int i = 0;
		char sta2g_buf[32] = {0};
		char sta5g_buf[32] = {0};
		char papMac_buf[32] = {0};
		char band_buf[16] = {0};

		lock = file_lock(CFG_FILE_LOCK);
		shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
		if (shm_client_tbl_id == -1){
			fprintf(stderr, "shmget failed\n");
			file_unlock(lock);
			json_object_put(wClietListObj);
			return 0;
		}

		shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
		if (shared_client_info == (void *)-1){
			fprintf(stderr, "shmat failed\n");
			file_unlock(lock);
			json_object_put(wClietListObj);
			return 0;
		}

		p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
		for(i = 0; i < p_client_tbl->count; i++) {
			memset(sta2g_buf, 0, sizeof(sta2g_buf));
			memset(sta5g_buf, 0, sizeof(sta5g_buf));

			snprintf(sta2g_buf, sizeof(sta2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->sta2g[i][0], p_client_tbl->sta2g[i][1],
				p_client_tbl->sta2g[i][2], p_client_tbl->sta2g[i][3],
				p_client_tbl->sta2g[i][4], p_client_tbl->sta2g[i][5]);

			snprintf(sta5g_buf, sizeof(sta5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->sta5g[i][0], p_client_tbl->sta5g[i][1],
				p_client_tbl->sta5g[i][2], p_client_tbl->sta5g[i][3],
				p_client_tbl->sta5g[i][4], p_client_tbl->sta5g[i][5]);

			/* filter ASUS device first */
			json_object_object_foreach(wClietListObj, key, val) {
				brMacObj = val;
				json_object_object_foreach(brMacObj, key, val) {
					bandObj = val;
					/* filter sta for 2G */
					if (strlen(sta2g_buf)) {
						json_object_object_get_ex(bandObj, sta2g_buf, &staObj);
						if (staObj) json_object_object_del(bandObj, sta2g_buf);
					}

					/* filter sta for 5G */
					if (strlen(sta5g_buf)) {
						json_object_object_get_ex(bandObj, sta5g_buf, &staObj);
						if (staObj) json_object_object_del(bandObj, sta5g_buf);
					}
				}
			}
		}

		/* assemble output */
		json_object_object_foreach(wClietListObj, key, val) {
			brMacObj = val;
			memset(papMac_buf, 0, sizeof(papMac_buf));
			snprintf(papMac_buf, sizeof(papMac_buf), "%s", key);
			json_object_object_foreach(brMacObj, key, val) {
				bandObj = val;
				memset(band_buf, 0, sizeof(band_buf));
				snprintf(band_buf, sizeof(band_buf), "%s", key);
				json_object_object_foreach(bandObj, key, val) {
					amas_re_client_attr = json_object_new_object();
					if(!strcmp(band_buf, "2G") || !strcmp(band_buf, "2G_1") || !strcmp(band_buf, "2G_2") || !strcmp(band_buf, "2G_3"))
						json_object_object_add(amas_re_client_attr, "isWL", json_object_new_string("1"));
					else if(!strcmp(band_buf, "5G") || !strcmp(band_buf, "5G_1") || !strcmp(band_buf, "5G_2") || !strcmp(band_buf, "5G_3"))
						json_object_object_add(amas_re_client_attr, "isWL", json_object_new_string("2"));
					else if(!strcmp(band_buf, "5G1")|| !strcmp(band_buf, "5G1_1") || !strcmp(band_buf, "5G1_2") || !strcmp(band_buf, "5G1_3"))
						json_object_object_add(amas_re_client_attr, "isWL", json_object_new_string("3"));
					else
						json_object_object_add(amas_re_client_attr, "isWL", json_object_new_string("0"));
					json_object_object_add(amas_re_client_attr, "papMac", json_object_new_string(papMac_buf));

					json_object_object_add(json_object_ptr, key, amas_re_client_attr);
				}
			}
		}
		shmdt(shared_client_info);
		file_unlock(lock);

		if(!have_data)
			have_data = 1;
	}

	json_object_put(wClietListObj);

	return have_data;
}
static int get_amas_wired_client_info(struct json_object *json_object_ptr) { //get wired client info
	int have_data = 0;

	int shm_client_tbl_id;
	int lock;
	void *shared_client_info = (void *) 0;
	json_object *wiredClietListObj = NULL;
	struct json_object *amas_wired_client_attr = NULL;

	lock = file_lock(WIREDCLIENTLIST_FILE_LOCK);
	wiredClietListObj = json_object_from_file(WIRED_CLIENT_LIST_JSON_PATH);
	file_unlock(lock);

	if (wiredClietListObj) {
		P_CM_CLIENT_TABLE p_client_tbl;
		int i = 0;
		int brmac_first = 0;
		int wired_client_first = 0;
		int wiredClientListLen = 0;
		json_object *wiredClientEntry = NULL;
		json_object *wiredInfoObj = NULL;
		char papMac_buf[32] = {0};

		lock = file_lock(CFG_FILE_LOCK);
		shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
		if (shm_client_tbl_id == -1){
			fprintf(stderr, "shmget failed\n");
			file_unlock(lock);
			json_object_put(wiredClietListObj);
			return 0;
		}

		shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
		if (shared_client_info == (void *)-1){
			fprintf(stderr, "shmat failed\n");
			file_unlock(lock);
			json_object_put(wiredClietListObj);
			return 0;
		}

		p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;

		wiredInfoObj = json_object_new_object();

		if (!wiredInfoObj) {
			fprintf(stderr, "wiredInfoObj is NULL\n");
			file_unlock(lock);
			json_object_put(wiredClietListObj);
			return 0;
		}

		/* gen the info of wired client for temp */
		gen_wired_client_info(p_client_tbl, wiredClietListObj, wiredInfoObj);

		/* assemble output */
		json_object_object_foreach(wiredClietListObj, key, val) {
			if (!brmac_first)
				brmac_first = 1;

			wired_client_first = 0;
			memset(papMac_buf, 0, sizeof(papMac_buf));
			snprintf(papMac_buf, sizeof(papMac_buf), "%s", key);

			/* process for filtering device */
			wiredClientListLen = json_object_array_length(val);
			for (i = 0; i < wiredClientListLen; i++) {
				wiredClientEntry = json_object_array_get_idx(val, i);

				/* filter wireless client */
				if (is_wireless_client((char *)json_object_get_string(wiredClientEntry)))
					continue;

				/* filter cfg client by unique mac */
				if (is_cfg_client_by_unique_mac(p_client_tbl, (char *)json_object_get_string(wiredClientEntry)))
					continue;

				/* filter cfg client by sta mac */
				if (is_cfg_client_by_sta_mac(p_client_tbl, (char *)json_object_get_string(wiredClientEntry)))
					continue;

				/* filter not connected to correct node */
				if (!check_wired_client_connected_node(key, (char *)json_object_get_string(wiredClientEntry), wiredClietListObj, wiredInfoObj))
					continue;

				if (!wired_client_first)
					wired_client_first = 1;

				amas_wired_client_attr = json_object_new_object();
				json_object_object_add(amas_wired_client_attr, "papMac", json_object_new_string(papMac_buf));
				json_object_object_add(json_object_ptr, json_object_get_string(wiredClientEntry), amas_wired_client_attr);
			}
		}

		shmdt(shared_client_info);
		file_unlock(lock);

		if(!have_data)
			have_data = 1;
		json_object_put(wiredInfoObj);
	}

	json_object_put(wiredClietListObj);

	return have_data;
}

static int get_amas_re_client_detail_info(struct json_object *json_object_ptr) { //get re client detail info
	int have_data = 0;

	int lock;
	json_object *clietListObj = NULL;
	json_object *brMacObj = NULL;
	json_object *bandObj = NULL;
	struct json_object *amas_re_client_detail_attr = NULL;

	lock = file_lock(CLIENTLIST_FILE_LOCK);

	clietListObj = json_object_from_file(CLIENT_LIST_JSON_PATH);
	if (clietListObj) {
		char papMac_buf[32] = {0};
		char band_buf[16] = {0};
		json_object_object_foreach(clietListObj, key, val) {
			brMacObj = val;
			memset(papMac_buf, 0, sizeof(papMac_buf));
			snprintf(papMac_buf, sizeof(papMac_buf), "%s", key);
			json_object_object_foreach(brMacObj, key, val) {
				bandObj = val;
				memset(band_buf, 0, sizeof(band_buf));
				snprintf(band_buf, sizeof(band_buf), "%s", key);
				json_object_object_foreach(bandObj, key, val) {
					struct json_object *amas_re_get_rssi = NULL;
					json_object_object_get_ex(val, "rssi", &amas_re_get_rssi);
					if(amas_re_get_rssi != NULL) {
						int isWL = 0;
						char client_key[40] = {0};
						memset(client_key, 0, sizeof(client_key));
						amas_re_client_detail_attr = json_object_new_object();
						if(!strcmp(band_buf, "2G") || !strcmp(band_buf, "2G_1") || !strcmp(band_buf, "2G_2") || !strcmp(band_buf, "2G_3")) {
							json_object_object_add(amas_re_client_detail_attr, "isWL", json_object_new_string("1"));
							isWL = 1;
						}
						else if(!strcmp(band_buf, "5G") || !strcmp(band_buf, "5G_1") || !strcmp(band_buf, "5G_2") || !strcmp(band_buf, "5G_3")) {
							json_object_object_add(amas_re_client_detail_attr, "isWL", json_object_new_string("2"));
							isWL = 2;
						}
						else if(!strcmp(band_buf, "5G1")|| !strcmp(band_buf, "5G1_1") || !strcmp(band_buf, "5G1_2") || !strcmp(band_buf, "5G1_3")) {
							json_object_object_add(amas_re_client_detail_attr, "isWL", json_object_new_string("3"));
							isWL = 3;
						}
						else {
							json_object_object_add(amas_re_client_detail_attr, "isWL", json_object_new_string("0"));
							isWL = 0;
						}

						json_object_object_add(amas_re_client_detail_attr, "rssi", json_object_new_string(json_object_get_string(amas_re_get_rssi)));
						json_object_object_add(amas_re_client_detail_attr, "papMac", json_object_new_string(papMac_buf));
						snprintf(client_key, sizeof(client_key), "%s_%d_%s", key, isWL, papMac_buf);//mac_band_papMac
						json_object_object_add(json_object_ptr, client_key, amas_re_client_detail_attr);
					}
				}
			}
			if(!have_data)
				have_data = 1;
		}


		json_object_put(clietListObj);
		
	}

	file_unlock(lock);


	return have_data;
}
static int is_re_node(char *client_mac, int check_path) {
	int i;
	int ret = 0;
	int shm_client_tbl_id;
	P_CM_CLIENT_TABLE p_client_tbl;
	void *shared_client_info=(void *) 0;
	unsigned char mac_buf[6] = {0};

	shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
	if (shm_client_tbl_id == -1){
		fprintf(stderr, "shmget failed\n");
		return 0;
	}

	shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr, "shmat failed\n");
		return 0;
	}

	ether_atoe(client_mac, mac_buf);

	p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
	for(i = 1; i < p_client_tbl->count; i++) {
		if ((memcmp(p_client_tbl->sta2g[i], mac_buf, sizeof(mac_buf)) == 0 ||
			memcmp(p_client_tbl->sta5g[i], mac_buf, sizeof(mac_buf)) == 0) &&
			((check_path && p_client_tbl->activePath[i] == 1) || check_path == 0))
		{
			ret = 1;
			break;
		}
	}

	shmdt(shared_client_info);

	return ret;
}

static int get_amas_client_mac(json_object *allClientList, char *ip, char *mac, int macBufSize) {
	int i;
	int shm_client_tbl_id;
	P_CM_CLIENT_TABLE p_client_tbl;
	void *shared_client_info=(void *) 0;
	unsigned char mac_buf[6] = {0};
	json_object *nodeEntry = NULL;
	json_object *bandEntry = NULL;
	json_object *macEntry = NULL;
	json_object *ipEntry = NULL;

	if (allClientList == NULL)
		return 0;

	/* get real mac for wireless client */
	json_object_object_foreach(allClientList, key, val) {
		nodeEntry = val;
		json_object_object_foreach(nodeEntry, key, val) {
			bandEntry = val;
			json_object_object_foreach(bandEntry, key, val) {
				macEntry = val;
				if (!is_re_node(key, 0)) {
					json_object_object_get_ex(macEntry, "ip", &ipEntry);
					if (ipEntry) {
						if (strcmp(ip, json_object_get_string(ipEntry)) == 0) {
							memset(mac, 0, macBufSize);
							snprintf(mac, macBufSize, "%s", key);
							break;
						}
					}
				}
			}
		}
	}

	/* check sta2g and sta5g, replace real mac for RE if client is RE */
	if (strlen(mac)) {
		shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
		if (shm_client_tbl_id == -1){
			fprintf(stderr, "shmget failed\n");
			return 0;
		}

		shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
		if (shared_client_info == (void *)-1){
			fprintf(stderr, "shmat failed\n");
			return 0;
		}

		ether_atoe(mac, mac_buf);

		p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
		for(i = 1; i < p_client_tbl->count; i++) {
			if (memcmp(p_client_tbl->sta2g[i], mac_buf, sizeof(mac_buf)) == 0 ||
				memcmp(p_client_tbl->sta5g[i], mac_buf, sizeof(mac_buf)) == 0)
			{
				//_dprintf("replace client mac (%s) by ", mac);
				memset(mac, 0, macBufSize);
				snprintf(mac, macBufSize, "%02X:%02X:%02X:%02X:%02X:%02X",
					p_client_tbl->realMacAddr[i][0], p_client_tbl->realMacAddr[i][1],
					p_client_tbl->realMacAddr[i][2], p_client_tbl->realMacAddr[i][3],
					p_client_tbl->realMacAddr[i][4], p_client_tbl->realMacAddr[i][5]);
				_dprintf("(%s)\n", mac);
				break;
			}
		}

		shmdt(shared_client_info);
	}

	return strlen(mac);
}
#endif

//2016.09 Rawny add for new networkmap
//static int get_client_detail_info(int eid, webs_t wp, int argc, char_t **argv, key_t shmkey, char *maclist_buf){
static int get_client_detail_info(struct json_object *clients, struct json_object *macArray, key_t shmkey){
	CLIENT_DPRINTF("get_client_detail_info start\n");
	int i, shm_client_info_id;
	void *shared_client_info = (void *) 0;
	//char output_buf[2048];
	char mac_buf[32], dev_name[32];
	char type[8], defaultType[8], macRepeat[8], opMode[8], rssi[8], wtfast[8], internetState[8], wireless[8];
#if 0
	char *dot = ",";
#endif
	char ipaddr[16];
	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
	int lock;
	char devname[LINE_SIZE], character;
	int j, len;
#if 0
	int first_mac=1, first_info=1;
#endif
	struct json_object *client = NULL;

	struct json_object *customList = NULL, *qosRuleList = NULL, *wtfRulelist = NULL, *multifilterList = NULL, *custom_attr_get = NULL;
	int customList_status = 0, qosRuleList_status = 0, wtfRulelist_status = 0, multifilterList_status = 0;
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	struct json_object *pmInfo = NULL, *pmGroupArray = NULL, *pmGroupArray_temp = NULL;
	int pmInfo_status = 0;
	int idx = 0, arraylen = 0;
#endif
#ifdef RTCONFIG_AMAS
	struct json_object *amasList = NULL, *amasReClientList = NULL, *amasReClientDetailList = NULL, *amasWiredClientList = NULL;
	struct json_object *allClientList = NULL;
	int amasList_status = 0, amasReClientList_status = 0, amasReClientDetailList_status = 0, amasWiredClientList_status = 0;
	struct json_object *amasPAP_attr_get = NULL;
#endif

	//get custom_clientlist
	customList = json_object_new_object();
	customList_status = get_custom_clientlist_info(customList);

	//get qos_rulelist
	qosRuleList = json_object_new_object();
	qosRuleList_status = get_qos_rulelist_info(qosRuleList);

	//get wtf_rulelist
	wtfRulelist = json_object_new_object();
	wtfRulelist_status = get_wtf_rulelist_info(wtfRulelist);

	//get MULTIFILTER
	multifilterList = json_object_new_object();
	multifilterList_status = get_multifilter_info(multifilterList);

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	//get_pms_device_info
	pmInfo = json_object_new_object();
	pmInfo_status = get_pms_device_info(pmInfo);
#endif

#ifdef RTCONFIG_AMAS
	//get amas cap re info
	amasList = json_object_new_object();
	amasList_status = get_amas_info(amasList);
	//get amas re client info
	amasReClientList = json_object_new_object();
	amasReClientList_status = get_amas_re_client_info(amasReClientList);
	//get amas wired client info
	amasWiredClientList = json_object_new_object();
	amasWiredClientList_status = get_amas_wired_client_info(amasWiredClientList);
	//get amas re client detail info
	amasReClientDetailList = json_object_new_object();
	amasReClientDetailList_status = get_amas_re_client_detail_info(amasReClientDetailList);
	//get all client list
	lock = file_lock(CLIENTLIST_FILE_LOCK);
	allClientList = json_object_from_file(CLIENT_LIST_JSON_PATH);
	file_unlock(lock);
#endif

	// set check wireless offline
	nvram_set("nmp_wl_offline_check", "1");

	lock = file_lock("networkmap");
	shm_client_info_id = shmget((key_t)shmkey, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
		fprintf(stderr,"shmget failed\n");
		file_unlock(lock);
		return 0;
	}

	shared_client_info = shmat(shm_client_info_id, (void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		file_unlock(lock);
		return 0;
	}

#ifdef RTCONFIG_DISABLE_NETWORKMAP
	if (nvram_match("networkmap_enable", "0")){
		nvram_set("networkmap_fullscan", "0");
		file_unlock(lock);
		return 0;
	}
#endif


	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i = 0; i < p_client_info_tab->ip_mac_num; i++) {
		memset(dev_name, 0, sizeof(dev_name));
		//memset(output_buf, 0, sizeof(output_buf));
		memset(ipaddr, 0, sizeof(ipaddr));
		memset(mac_buf, 0, sizeof(mac_buf));
		memset(devname, 0, LINE_SIZE);
		memset(type, 0, sizeof(type));
		memset(defaultType, 0, sizeof(defaultType));
		memset(macRepeat, 0, sizeof(macRepeat));
		memset(opMode, 0, sizeof(opMode));
		memset(rssi, 0, sizeof(rssi));
		memset(wtfast, 0, sizeof(wtfast));
		memset(internetState, 0, sizeof(internetState));
		memset(wireless, 0, sizeof(wireless));

		if (*p_client_info_tab->user_define[i])
			strlcpy(dev_name, (const char *)p_client_info_tab->user_define[i], sizeof(dev_name));
		else
			strlcpy(dev_name, (const char *)p_client_info_tab->device_name[i], sizeof(dev_name));

#ifndef RTCONFIG_AMAS
		if(p_client_info_tab->device_flag[i]&(1<<FLAG_EXIST)) {
#endif
			len = strlen(dev_name);
			for (j = 0; (j < len) && (j < LINE_SIZE-1); j++) {
				character = dev_name[j];
				if ((isalnum(character)) || (character == ' ') || (character == '-') || (character == '_')
					|| (character == '(') || (character == ')'))
					devname[j] = character;
				else
					devname[j] = ' ';
			}

			sprintf(ipaddr, "%d.%d.%d.%d", p_client_info_tab->ip_addr[i][0],p_client_info_tab->ip_addr[i][1],
			p_client_info_tab->ip_addr[i][2],p_client_info_tab->ip_addr[i][3]);

			sprintf(mac_buf, "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
			p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
			p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5]
			);
#ifdef RTCONFIG_AMAS
			/* replace client mac if needed */
			if (allClientList)
				get_amas_client_mac(allClientList, ipaddr, mac_buf, sizeof(mac_buf));

			if (is_re_node(mac_buf, 1))
				continue;
#endif
			json_object_array_add(macArray, json_object_new_string(mac_buf));

			sprintf(type, "%d", p_client_info_tab->type[i]);
			sprintf(defaultType, "%d", p_client_info_tab->type[i]);
			sprintf(macRepeat, "%d", check_macrepeat(macArray, mac_buf));
			sprintf(opMode, "%d", p_client_info_tab->opMode[i]);
			sprintf(rssi, "%d", p_client_info_tab->rssi[i]);
			sprintf(wireless, "%d", p_client_info_tab->wireless[i]);

			client = json_object_new_object();
			json_object_object_add(client, "type", json_object_new_string(type));
			json_object_object_add(client, "defaultType", json_object_new_string(defaultType));
			json_object_object_add(client, "name", json_object_new_string(devname));
			json_object_object_add(client, "nickName", json_object_new_string(""));
			json_object_object_add(client, "ip", json_object_new_string(ipaddr));
			json_object_object_add(client, "mac", json_object_new_string(mac_buf));
			json_object_object_add(client, "from", json_object_new_string("networkmapd"));
			json_object_object_add(client, "macRepeat", json_object_new_string(macRepeat));
			json_object_object_add(client, "isGateway", json_object_new_string(!strcmp(nvram_safe_get("lan_ipaddr"), ipaddr) ? "1" : "0"));
			json_object_object_add(client, "isWebServer", json_object_new_string((p_client_info_tab->device_flag[i] & (1<FLAG_HTTP)) ? "1" : "0"));
			json_object_object_add(client, "isPrinter", json_object_new_string((p_client_info_tab->device_flag[i] & (1<FLAG_PRINTER)) ? "1" : "0"));
			json_object_object_add(client, "isITunes", json_object_new_string((p_client_info_tab->device_flag[i] & (1<FLAG_ITUNE)) ? "1" : "0"));
			json_object_object_add(client, "dpiType", json_object_new_string(""));
			json_object_object_add(client, "dpiDevice", json_object_new_string((const char *) p_client_info_tab->apple_model[i]));
			json_object_object_add(client, "vendor", json_object_new_string((const char *) p_client_info_tab->vendor_name[i]));
			json_object_object_add(client, "isWL", json_object_new_string(wireless));
#ifndef RTCONFIG_AMAS
			json_object_object_add(client, "isOnline", json_object_new_string("1"));
#else
			//check nmp shm on RP/MB mode(no CFG)
			if(!nvram_match("re_mode", "1") && nvram_get_int("sw_mode") != SW_MODE_ROUTER) {
				if(p_client_info_tab->device_flag[i]&(1<<FLAG_EXIST))
					json_object_object_add(client, "isOnline", json_object_new_string("1"));
				else
					json_object_object_add(client, "isOnline", json_object_new_string("0"));
			}
			else
				json_object_object_add(client, "isOnline", json_object_new_string("0"));
#endif
			json_object_object_add(client, "ssid", json_object_new_string(p_client_info_tab->ssid[i]));
			if(!strcmp(ipaddr, nvram_safe_get("login_ip_str"))){
				json_object_object_add(client, "isLogin", json_object_new_string("1"));
			}
			else{
				json_object_object_add(client, "isLogin", json_object_new_string("0"));
			}
			//asus device
			//json_object_object_add(client, "isASUS", json_object_new_int(0));
			json_object_object_add(client, "opMode", json_object_new_string(opMode));

			//wireless hook
			json_object_object_add(client, "rssi", json_object_new_string(rssi));
			json_object_object_add(client, "curTx", json_object_new_string(p_client_info_tab->txrate[i]));
			json_object_object_add(client, "curRx", json_object_new_string(p_client_info_tab->rxrate[i]));
			json_object_object_add(client, "totalTx", json_object_new_string(""));
			json_object_object_add(client, "totalRx", json_object_new_string(""));
			json_object_object_add(client, "wlConnectTime", json_object_new_string(p_client_info_tab->conn_time[i]));
#if defined(BRTAC828)
			json_object_object_add(client, "wlInterface", json_object_new_string(&p_client_info_tab->subunit[i]));
#endif

			//ipMethod
			json_object_object_add(client, "ipMethod", json_object_new_string((const char *) p_client_info_tab->ipMethod[i]));

			//custom_clientlist
			json_object_object_add(client, "group", json_object_new_string(""));
			json_object_object_add(client, "callback", json_object_new_string(""));
			json_object_object_add(client, "keeparp", json_object_new_string(""));

			//Update attribute by custom_clientlist
			if(customList_status) {
				 json_object_object_get_ex(customList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *custom_attr_get_type = NULL, *custom_attr_get_name = NULL, *custom_attr_get_callback = NULL, *custom_attr_get_keeparp = NULL;
					memset(type, 0, sizeof(type));
					if(json_object_object_get_ex(custom_attr_get, "type", &custom_attr_get_type))
						sprintf(type, "%d", json_object_get_int(custom_attr_get_type));
					if(strcmp(type, "0")) //if custom_clientlist type is 0, not update.
						json_object_object_add(client, "type", json_object_new_string(type));
					if(json_object_object_get_ex(custom_attr_get, "name", &custom_attr_get_name))
						json_object_object_add(client, "nickName", json_object_new_string(json_object_get_string(custom_attr_get_name)));
					//json_object_object_add(client, "group", json_object_new_string(json_object_get_string(json_object_object_get(custom_attr_get, "group"))));
					if(json_object_object_get_ex(custom_attr_get, "callback", &custom_attr_get_callback))
						json_object_object_add(client, "callback", json_object_new_string(json_object_get_string(custom_attr_get_callback)));
					if(json_object_object_get_ex(custom_attr_get, "keeparp", &custom_attr_get_keeparp))
						json_object_object_add(client, "keeparp", json_object_new_string(json_object_get_string(custom_attr_get_keeparp)));
				}
				CLIENT_DPRINTF("customList finish\n");
			}

			//qos_rulelist
			json_object_object_add(client, "qosLevel", json_object_new_string(""));
			//Update attribute by qos_rulelist
			if(qosRuleList_status) {
				json_object_object_get_ex(qosRuleList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					json_object_object_add(client, "qosLevel", json_object_new_string(json_object_get_string(custom_attr_get)));
				}
				CLIENT_DPRINTF("qosRuleList finish\n");
			}

			//wtf_rulelist
			json_object_object_add(client, "wtfast", json_object_new_string("0"));
			//Update attribute by qos_rulelist
			if(wtfRulelist_status) {
				json_object_object_get_ex(wtfRulelist, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					sprintf(wtfast, "%d", json_object_get_int(custom_attr_get));
					json_object_object_add(client, "wtfast", json_object_new_string(wtfast));
				}
				CLIENT_DPRINTF("wtfRulelist finish\n");
			}

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
			//Update attribute by pmInfo
			if(pmInfo_status) {
				json_object_object_get_ex(pmInfo, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					pmGroupArray = json_object_new_array();
					json_object_object_get_ex(custom_attr_get, "device_group", &pmGroupArray_temp);
					arraylen = json_object_array_length(pmGroupArray_temp);
					for (idx = 0; idx < arraylen; idx++) {
						json_object_array_add(pmGroupArray, json_object_new_string(json_object_get_string(json_object_array_get_idx(pmGroupArray_temp, idx))));
					}
					json_object_object_add(client, "group", pmGroupArray);
				}
				CLIENT_DPRINTF("pmInfo finish\n");
			}
#endif

			//time_scheduling
			json_object_object_add(client, "internetMode", json_object_new_string("allow"));
			json_object_object_add(client, "internetState", json_object_new_string("1"));
			if(multifilterList_status) {
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
				json_object_object_get_ex(client, "group", &pmGroupArray_temp);
				if(strcmp(json_object_get_string(pmGroupArray_temp), "") && strcmp(json_object_get_string(pmGroupArray_temp), "0")) {
					arraylen = json_object_array_length(pmGroupArray_temp);
					for (idx = 0; idx < arraylen; idx++) {
						json_object_object_get_ex(multifilterList, json_object_get_string(json_object_array_get_idx(pmGroupArray_temp, idx)), &custom_attr_get);
						if(custom_attr_get != NULL) {
							struct json_object *custom_attr_get_internetState = NULL, *custom_attr_get_internetMode = NULL;
							if(json_object_object_get_ex(custom_attr_get, "internetState", &custom_attr_get_internetState))
								sprintf(internetState, "%d", json_object_get_int(custom_attr_get_internetState));
							if(json_object_object_get_ex(custom_attr_get, "internetMode", &custom_attr_get_internetMode))
								json_object_object_add(client, "internetMode", json_object_new_string(json_object_get_string(custom_attr_get_internetMode)));
							json_object_object_add(client, "internetState", json_object_new_string(internetState));
							if(!json_object_get_int(custom_attr_get_internetState)) {
								break;
							}
						}
					}
					CLIENT_DPRINTF("multifilterList by group finish\n");
				}
#else
				json_object_object_get_ex(multifilterList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *custom_attr_get_internetState = NULL, *custom_attr_get_internetMode = NULL;
					if(json_object_object_get_ex(custom_attr_get, "internetMode", &custom_attr_get_internetMode))
						json_object_object_add(client, "internetMode", json_object_new_string(json_object_get_string(custom_attr_get_internetMode)));
					if(json_object_object_get_ex(custom_attr_get, "internetState", &custom_attr_get_internetState))
						json_object_object_add(client, "internetState", json_object_new_int(json_object_get_int(custom_attr_get_internetState)));
				}
				CLIENT_DPRINTF("multifilterList by mac finish\n");
#endif
			}

#ifdef RTCONFIG_AMAS
			if(amasList_status) {
				json_object_object_get_ex(amasList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *amas_get_pap2g = NULL, *amas_get_rssi2g = NULL, *amas_get_pap5g = NULL, *amas_get_rssi5g = NULL, *amas_get_type = NULL, *amas_get_online = NULL;
					json_object_object_get_ex(custom_attr_get, "pap2g", &amas_get_pap2g);
					json_object_object_get_ex(custom_attr_get, "rssi2g", &amas_get_rssi2g);
					json_object_object_get_ex(custom_attr_get, "pap5g", &amas_get_pap5g);
					json_object_object_get_ex(custom_attr_get, "rssi5g", &amas_get_rssi5g);
					json_object_object_get_ex(custom_attr_get, "type", &amas_get_type);
					json_object_object_get_ex(custom_attr_get, "online", &amas_get_online);
					json_object_object_add(client, "amesh_isRe", json_object_new_string(!(strcmp(json_object_get_string(amas_get_type), "RE")) ? "1" : "0"));
					if(strcmp(json_object_get_string(amas_get_pap2g), "") && strcmp(json_object_get_string(amas_get_rssi2g), "") && 
						strcmp(json_object_get_string(amas_get_pap5g), "") && strcmp(json_object_get_string(amas_get_rssi5g), "")) {
						if(atoi(json_object_get_string(amas_get_rssi2g)) > atoi(json_object_get_string(amas_get_rssi5g))) {
							json_object_object_add(client, "isWL", json_object_new_string("1"));
							json_object_object_add(client, "rssi", json_object_new_string(json_object_get_string(amas_get_rssi2g)));
						}
						else {
							json_object_object_add(client, "isWL", json_object_new_string("2"));
							json_object_object_add(client, "rssi", json_object_new_string(json_object_get_string(amas_get_rssi5g)));
						}
					}
					else if(strcmp(json_object_get_string(amas_get_pap2g), "") && strcmp(json_object_get_string(amas_get_rssi2g), "")) {
						json_object_object_add(client, "isWL", json_object_new_string("1"));
						json_object_object_add(client, "rssi", json_object_new_string(json_object_get_string(amas_get_rssi2g)));
					}
					else if(strcmp(json_object_get_string(amas_get_pap5g), "") && strcmp(json_object_get_string(amas_get_rssi5g), "")) {
						json_object_object_add(client, "isWL", json_object_new_string("2"));
						json_object_object_add(client, "rssi", json_object_new_string(json_object_get_string(amas_get_rssi5g)));
					}
					else {
						json_object_object_add(client, "isWL", json_object_new_string("0"));
						json_object_object_add(client, "rssi", json_object_new_string(""));
					}
					json_object_object_add(client, "isOnline", json_object_new_string(json_object_get_string(amas_get_online)));
				}
				CLIENT_DPRINTF("amasList finish\n");
			}
			if(amasReClientList_status) {
				json_object_object_get_ex(amasReClientList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *amas_re_get_isWL = NULL, *amas_re_get_papMac = NULL, *amas_re_get_online = NULL;
					json_object_object_get_ex(custom_attr_get, "isWL", &amas_re_get_isWL);
					json_object_object_get_ex(custom_attr_get, "papMac", &amas_re_get_papMac);
					json_object_object_add(client, "amesh_isReClient", json_object_new_string("1"));
					json_object_object_add(client, "isWL", json_object_new_string(json_object_get_string(amas_re_get_isWL)));
					json_object_object_add(client, "amesh_papMac", json_object_new_string(json_object_get_string(amas_re_get_papMac)));
					if(amasList_status) {
						json_object_object_get_ex(amasList, json_object_get_string(amas_re_get_papMac), &amasPAP_attr_get);
						if(amasPAP_attr_get != NULL) {
							json_object_object_get_ex(amasPAP_attr_get, "online", &amas_re_get_online);
							json_object_object_add(client, "isOnline", json_object_new_string(json_object_get_string(amas_re_get_online)));
						}
					}
				}
				CLIENT_DPRINTF("amasReClientList finish\n");
			}
			if(amasWiredClientList_status) {
				json_object_object_get_ex(amasWiredClientList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *amas_re_get_papMac = NULL, *amas_re_get_online = NULL;
					json_object_object_get_ex(custom_attr_get, "papMac", &amas_re_get_papMac);
					json_object_object_add(client, "amesh_isReClient", json_object_new_string("1"));
					json_object_object_add(client, "isWL", json_object_new_string("0"));
					json_object_object_add(client, "amesh_papMac", json_object_new_string(json_object_get_string(amas_re_get_papMac)));
					if(amasList_status) {
						json_object_object_get_ex(amasList, json_object_get_string(amas_re_get_papMac), &amasPAP_attr_get);
						if(amasPAP_attr_get != NULL) {
							json_object_object_get_ex(amasPAP_attr_get, "online", &amas_re_get_online);
							json_object_object_add(client, "isOnline", json_object_new_string(json_object_get_string(amas_re_get_online)));
						}
					}
				}
#ifdef RTCONFIG_LACP
				//search nmp shm for bonding devices
				else if (nvram_match("lacp_enabled", "1")) {
					if(p_client_info_tab->device_flag[i]&(1<<FLAG_EXIST))
						json_object_object_add(client, "isOnline", json_object_new_string("1"));
				}
#endif
				CLIENT_DPRINTF("amasWiredClientList finish\n");
			}
			if(amasReClientDetailList_status) {
				struct json_object *amas_re_get_isWL = NULL, *amas_re_get_amesh_papMac = NULL;
				json_object_object_get_ex(client, "isWL", &amas_re_get_isWL);
				json_object_object_get_ex(client, "amesh_papMac", &amas_re_get_amesh_papMac);
				if(amas_re_get_amesh_papMac != NULL && strcmp(json_object_get_string(amas_re_get_isWL), "0")) {
					char client_key[40] = {0};
					memset(client_key, 0, sizeof(client_key));
					snprintf(client_key, sizeof(client_key), "%s_%s_%s", mac_buf, json_object_get_string(amas_re_get_isWL), json_object_get_string(amas_re_get_amesh_papMac));//mac_band_papMac
					json_object_object_get_ex(amasReClientDetailList, client_key, &custom_attr_get);
					if(custom_attr_get != NULL) {
						struct json_object *amas_re_get_rssi = NULL;
						json_object_object_get_ex(custom_attr_get, "rssi", &amas_re_get_rssi);
						json_object_object_add(client, "rssi", json_object_new_string(json_object_get_string(amas_re_get_rssi)));
					}
				}
				CLIENT_DPRINTF("amasReClientDetailList finish\n");
			}
#endif

			json_object_object_add(clients, mac_buf, client);
#ifndef RTCONFIG_AMAS
		}
#endif
	}
	shmdt(shared_client_info);
	file_unlock(lock);

	if(customList)
		json_object_put(customList);
	if(qosRuleList)
		json_object_put(qosRuleList);
	if(wtfRulelist)
		json_object_put(wtfRulelist);
	if(multifilterList)
		json_object_put(multifilterList);
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	if(pmInfo)
		json_object_put(pmInfo);
#endif
#ifdef RTCONFIG_AMAS
	if(amasList)
		json_object_put(amasList);
	if(amasReClientList)
		json_object_put(amasReClientList);
	if(amasWiredClientList)
		json_object_put(amasWiredClientList);
	if(amasReClientDetailList)
		json_object_put(amasReClientDetailList);
	json_object_put(allClientList);
#endif
	if(custom_attr_get)
		json_object_put(custom_attr_get);
	CLIENT_DPRINTF("get_client_detail_info finish\n");

	return 0;
}

static int ej_get_clientlist_maclist(int eid, webs_t wp, int argc, char_t **argv)
{
        if(!pids("networkmap")){
                websWrite(wp, "[]");
                return 0;
        }

	struct json_object *macArray = json_object_new_array();
	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
	void *shared_client_info = (void *) 0;
	char mac_buf[32];
	int i, lock, shm_client_info_id;

#ifdef RTCONFIG_AMAS
	char ipaddr[16];
	char clientMac[18];
	struct json_object *amasList = NULL, *amasReClientList = NULL, *amasWiredClientList = NULL;
	int amasList_status = 0, amasReClientList_status = 0, amasWiredClientList_status = 0;
	int amasClient = false;
	struct json_object *custom_attr_get = NULL;
	struct json_object *amasPAP_attr_get = NULL;
	struct json_object *allClientList = NULL;

	lock = file_lock(CLIENTLIST_FILE_LOCK);
	//get amas cap re info
	amasList = json_object_new_object();
	amasList_status = get_amas_info(amasList);
	//get amas re client info
	amasReClientList = json_object_new_object();
	amasReClientList_status = get_amas_re_client_info(amasReClientList);
	//get amas wired client info
	amasWiredClientList = json_object_new_object();
	amasWiredClientList_status = get_amas_wired_client_info(amasWiredClientList);
	allClientList = json_object_from_file(CLIENT_LIST_JSON_PATH);
	file_unlock(lock);
#endif

	lock = file_lock("networkmap");
	shm_client_info_id = shmget((key_t)SHMKEY_LAN, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
		fprintf(stderr,"shmget failed\n");
		file_unlock(lock);
		json_object_put(macArray);
#ifdef RTCONFIG_AMAS
		json_object_put(allClientList);
#endif
		websWrite(wp, "[]");
		return 0;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr, "shmat failed\n");
		file_unlock(lock);
		websWrite(wp, "[]");
		json_object_put(macArray);
#ifdef RTCONFIG_AMAS
		json_object_put(allClientList);
#endif
		return 0;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i = 0; i < p_client_info_tab->ip_mac_num; i++) {
#ifndef RTCONFIG_AMAS
		if(p_client_info_tab->device_flag[i]&(1<<FLAG_EXIST)) {
#endif
			memset(mac_buf, 0, sizeof(mac_buf));
			sprintf(mac_buf, "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
				p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
				p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5]
				);
#ifdef RTCONFIG_AMAS
			memset(ipaddr, 0, sizeof(ipaddr));
			sprintf(ipaddr, "%d.%d.%d.%d", p_client_info_tab->ip_addr[i][0],p_client_info_tab->ip_addr[i][1],
			p_client_info_tab->ip_addr[i][2],p_client_info_tab->ip_addr[i][3]);
			amasClient = false;

			/* replace client mac if needed */
			memset(clientMac, 0, sizeof(clientMac));
			snprintf(clientMac, sizeof(clientMac), "%s", mac_buf);
			if (allClientList && get_amas_client_mac(allClientList, ipaddr, clientMac, sizeof(clientMac))) {
				memset(mac_buf, 0, sizeof(mac_buf));
				snprintf(mac_buf, sizeof(mac_buf), "%s", clientMac);
			}
			if (is_re_node(mac_buf, 1))
				continue;

			if(amasList_status) {//filter RE node
				json_object_object_get_ex(amasList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					amasClient = true;
					continue;
				}
			}
			if(amasReClientList_status) {
				json_object_object_get_ex(amasReClientList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *amas_re_get_papMac = NULL, *amas_re_get_online = NULL;
					json_object_object_get_ex(custom_attr_get, "papMac", &amas_re_get_papMac);
					if(amasList_status) {
						json_object_object_get_ex(amasList, json_object_get_string(amas_re_get_papMac), &amasPAP_attr_get);
						if(amasPAP_attr_get != NULL) {
							json_object_object_get_ex(amasPAP_attr_get, "online", &amas_re_get_online);
							if(!strcmp(json_object_get_string(amas_re_get_online), "1"))
								amasClient = true;
						}
					}
				}
			}
			if(amasWiredClientList_status) {
				json_object_object_get_ex(amasWiredClientList, mac_buf, &custom_attr_get);
				if(custom_attr_get != NULL) {
					struct json_object *amas_re_get_papMac = NULL, *amas_re_get_online = NULL;
					json_object_object_get_ex(custom_attr_get, "papMac", &amas_re_get_papMac);
					if(amasList_status) {
						json_object_object_get_ex(amasList, json_object_get_string(amas_re_get_papMac), &amasPAP_attr_get);
						if(amasPAP_attr_get != NULL) {
							json_object_object_get_ex(amasPAP_attr_get, "online", &amas_re_get_online);
							if(!strcmp(json_object_get_string(amas_re_get_online), "1"))
								amasClient = true;
						}
					}
				}
			}
			if(!amasClient)
				continue;
#endif
			json_object_array_add(macArray, json_object_new_string(mac_buf));
#ifndef RTCONFIG_AMAS
		}
#endif
	}

	shmdt(shared_client_info);
	file_unlock(lock);

	websWrite(wp, "%s", json_object_to_json_string(macArray));

	if(macArray)
		json_object_put(macArray);

#ifdef RTCONFIG_AMAS
	if(amasList)
		json_object_put(amasList);
	if(amasReClientList)
		json_object_put(amasReClientList);
	if(amasWiredClientList)
		json_object_put(amasWiredClientList);
	if(custom_attr_get)
		json_object_put(custom_attr_get);
	if(amasPAP_attr_get)
		json_object_put(amasPAP_attr_get);
	if(allClientList)
		json_object_put(allClientList);
#endif

	return 0;
}

static int ej_get_clientlist(int eid, webs_t wp, int argc, char_t **argv)
{
	struct json_object *clients;
	if((nvram_match("refresh_networkmap", "1") || nvram_match("rescan_networkmap", "1")) && (check_if_file_exist(NMP_CACHE_FILE)))
	{
		clients = json_object_from_file(NMP_CACHE_FILE);
		websWrite(wp, "%s", json_object_to_json_string(clients));
		if(clients)
			json_object_put(clients);	
		return 0;
	}
	
	if(!pids("networkmap")){
		//websWrite(wp, "{\"maclist\": [], \"ClientAPILevel\":\"%s\"}", CLIENTAPILEVEL);
		websWrite(wp, "{\"maclist\": []}");
		return 0;
	}

	clients = json_object_new_object();

	struct json_object *macArray = json_object_new_array();
	//char maclist_buf[32768]=",\"maclist\":"; //tune for multi-subnet clients
	//char *brackets_h = "[";
	//char *brackets_d = "]";

	//strcat(maclist_buf,brackets_h);
#ifdef RTCONFIG_TAGGED_BASED_VLAN
	int i, vlan_flag;
	int shmkeys[8], shmkey = 1003;
	for(i = 0; i < 8; i++){
		shmkeys[i] = shmkey;
		shmkey++;
	}
#endif
	/*
	shmkey	index
	1001	LAN
	1003	VLAN1
	1004	VLAN2
	1005	VLAN3
	1006	VLAN4
	1007	VLAN5
	1008	VLAN6
	1009	VLAN7
	1010	VLAN8
	1011	FREE-WIFI
	1012	CAPTIVE PORTAL
	*/
	//get_client_detail_info(eid, wp, argc, argv, SHMKEY_LAN, &maclist_buf);
	get_client_detail_info(clients, macArray, SHMKEY_LAN);

#ifdef RTCONFIG_TAGGED_BASED_VLAN
	vlan_flag = nvram_get_int("vlan_flag");
	if(vlan_flag){
		for(i = 0; i < 8; i++){
			if(vlan_flag & (1<<i)){
				//get_client_detail_info(eid, wp, argc, argv, shmkeys[i], &maclist_buf);
				_dprintf("VLAN subnet search %d\n", shmkeys[i]);
				get_client_detail_info(clients, macArray, shmkeys[i]);
			}
		}
	}
#endif
#ifdef RTCONFIG_CAPTIVE_PORTAL
	if (nvram_match("captive_portal_enable", "on")){
		//get_client_detail_info(eid, wp, argc, argv, SHMKEY_FREEWIFI, maclist_buf);
		get_client_detail_info(clients, macArray, SHMKEY_FREEWIFI);
	}
	if (nvram_match("captive_portal_adv_enable", "on")){
		//get_client_detail_info(eid, wp, argc, argv, SHMKEY_CP, maclist_buf);
		get_client_detail_info(clients, macArray, SHMKEY_CP);
	}
#endif

	json_object_object_add(clients, "maclist", macArray);
	//json_object_object_add(clients, "ClientAPILevel", json_object_new_string(CLIENTAPILEVEL));
	websWrite(wp, "%s", json_object_to_json_string(clients));

	//strcat(maclist_buf,brackets_d);
	//websWrite(wp, maclist_buf);
	json_object_to_file(NMP_CACHE_FILE, clients);
	if(clients)
		json_object_put(clients);
	return 0;
}

// for detect static IP's client.
static int ej_get_static_client(int eid, webs_t wp, int argc, char_t **argv){
	FILE *fp = fopen("/tmp/static_ip.inf", "r");
	char buf[1024], *head, *tail, field[1024];
	int len, i, first_client, first_field;

	if (fp == NULL){
		csprintf("Don't detect static clients!\n");
		return 0;
	}

	memset(buf, 0, 1024);

	first_client = 1;
	while (fgets(buf, 1024, fp)){
		if (first_client == 1)
			first_client = 0;
		else
			websWrite(wp, ", ");

		len = strlen(buf);
		buf[len-1] = ',';
		head = buf;
		first_field = 1;
		for (i = 0; i < 7; ++i){
			tail = strchr(head, ',');
			if (tail != NULL){
				memset(field, 0, 1024);
				strncpy(field, head, (tail-head));
			}

			if (first_field == 1){
				first_field = 0;
				websWrite(wp, "[");
			}
			else
				websWrite(wp, ", ");

			if (strlen(field) > 0)
				websWrite(wp, "\"%s\"", field);
			else
				websWrite(wp, "null");

			//if (tail+1 != NULL)
				head = tail+1;

			if (i == 6)
				websWrite(wp, "]");
		}

		memset(buf, 0, 1024);
	}

	fclose(fp);
	return 0;
}

/*
 *	networkmap API
 */
/******************************************************************************/

static int yadns_servers_hook(int eid, webs_t wp, int argc, char_t **argv)
{
#ifdef RTCONFIG_YANDEXDNS
	int yadns_mode = nvram_get_int("yadns_enable_x") ? nvram_get_int("yadns_mode") : YADNS_DISABLED;
	char *server[2];
	int i, count;

	if (yadns_mode != YADNS_DISABLED) {
		count = get_yandex_dns(AF_INET, yadns_mode, server, sizeof(server)/sizeof(server[0]));
		for (i = 0; i < count; i++)
			websWrite(wp, i ? ",\"%s\"" : "\"%s\"", server[i]);
	}
#endif
	return 0;
}

static int yadns_clients_hook(int eid, webs_t wp, int argc, char_t **argv)
{
#ifdef RTCONFIG_YANDEXDNS
	char *name, *mac, *mode, *enable;
	char *nv, *nvp, *b;
	int i, dnsmode, clients[YADNS_COUNT];

	memset(&clients, 0, sizeof(clients));

	if (nvram_get_int("yadns_enable_x")) {
		nv = nvp = strdup(nvram_safe_get("yadns_rulelist"));
		while (nv && (b = strsep(&nvp, "<")) != NULL) {
			if (vstrsep(b, ">", &name, &mac, &mode, &enable) < 3)
				continue;
			if (enable && atoi(enable) == 0)
				continue;
			if (!*mac || !*mode)
				continue;
			dnsmode = atoi(mode);
			/* Skip incorrect levels */
			if (dnsmode < YADNS_FIRST || dnsmode >= YADNS_COUNT)
				continue;
			clients[dnsmode]++;
		}
		free(nv);
	}

	for (i = YADNS_FIRST; i < YADNS_COUNT; i++)
		websWrite(wp, (i == YADNS_FIRST) ? "%d" : ",%d", clients[i]);
#endif
	return 0;
}

static int ej_get_changed_status(int eid, webs_t wp, int argc, char_t **argv){
	char *arp_info = read_whole_file("/proc/net/arp");
#ifdef RTCONFIG_USB
	char *disk_info = read_whole_file(PARTITION_FILE);
	char *mount_info = read_whole_file(MOUNT_FILE);
	u32 disk_info_len, mount_info_len;
#endif
	u32 arp_info_len;
//	u32 arp_change, disk_change;

	if (arp_info != NULL){
		arp_info_len = strlen(arp_info);
		free(arp_info);
	}
	else
		arp_info_len = 0;

#ifdef RTCONFIG_USB
	if (disk_info != NULL){
		disk_info_len = strlen(disk_info);
		free(disk_info);
	}
	else
		disk_info_len = 0;

	if (mount_info != NULL){
		mount_info_len = strlen(mount_info);
		free(mount_info);
	}
	else
		mount_info_len = 0;
#endif

	websWrite(wp, "function get_client_status_changed(){\n");
	websWrite(wp, "    return %u;\n", arp_info_len);
	websWrite(wp, "}\n\n");

#ifdef RTCONFIG_USB
	websWrite(wp, "function get_disk_status_changed(){\n");
	websWrite(wp, "    return %u;\n", disk_info_len);
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_mount_status_changed(){\n");
	websWrite(wp, "    return %u;\n", mount_info_len);
	websWrite(wp, "}\n\n");
#endif
	return 0;
}

#ifdef RTCONFIG_USB
static int ej_show_usb_path(int eid, webs_t wp, int argc, char_t **argv){
	DIR *bus_usb = NULL;
	struct dirent *interface = NULL;
	char usb_port[8] = {0} , port_path[8] = {0}, *ptr = NULL;
	int port_num = 0, port_order = 0, hub_order = 0;
	char all_usb_path[MAX_USB_PORT][MAX_USB_HUB_PORT][3][16]; // MAX USB hub port number is 6.
	char tmp[100] = {0}, prefix[32] = {0};
	int port_set = 0, got_port = 0, got_hub = 0;
	 
	if((bus_usb = opendir(USB_DEVICE_PATH)) == NULL)
		return -1;
	
	memset(all_usb_path, 0x00, MAX_USB_PORT*MAX_USB_HUB_PORT*3*16);

	while((interface = readdir(bus_usb)) != NULL){
		if(interface->d_name[0] == '.')
			continue;

		if(!isdigit(interface->d_name[0]))
			continue;

		if(strchr(interface->d_name, ':'))
			continue;

		if(get_usb_port_by_string(interface->d_name, usb_port, 8) == NULL)
			continue;

		port_num = get_usb_port_number(usb_port);
		port_order = port_num-1;
		if((ptr = strchr(interface->d_name+strlen(usb_port), '.')) != NULL)
			hub_order = atoi(ptr+1);
		else
			hub_order = 0;

		if(get_path_by_node(interface->d_name, port_path, 8) == NULL)
			continue;

		strncpy(all_usb_path[port_order][hub_order][0], port_path, 16);
		snprintf(prefix, 32, "usb_path%s", port_path);
		strncpy(all_usb_path[port_order][hub_order][1], nvram_safe_get(prefix), 16);
		strncpy(all_usb_path[port_order][hub_order][2], nvram_safe_get(strcat_r(prefix, "_speed", tmp)), 16);
	}

	closedir(bus_usb);
	bus_usb = NULL;

	port_set = got_port = got_hub = 0;

	websWrite(wp, "[");
	for(port_order = 0; port_order < MAX_USB_PORT; ++port_order){
		got_hub = 0;
		for(hub_order = 0; hub_order < MAX_USB_HUB_PORT; ++hub_order){
			if(strlen(all_usb_path[port_order][hub_order][1]) > 0){
				if(!port_set){ // port range.
					port_set = 1;

					if(!got_port)
						got_port = 1;
					else
						websWrite(wp, ", ");

					websWrite(wp, "[");
				}

				if(!got_hub)
					got_hub = 1;
				else
					websWrite(wp, ", ");

				websWrite(wp, "[\"%s\", \"%s\", \"%s\"]", all_usb_path[port_order][hub_order][0], all_usb_path[port_order][hub_order][1], all_usb_path[port_order][hub_order][2]);
			}
		}

		if(port_set){ // port range.
			port_set = 0;
			websWrite(wp, "]");
		}
	}
	websWrite(wp, "]");
	return 0;
}


static int ej_disk_pool_mapping_info(int eid, webs_t wp, int argc, char_t **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int first;
	char *Ptr;

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "%s", initial_disk_pool_mapping_info());
		return -1;
	}

	websWrite(wp, "function total_disk_sizes(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%llu\"", follow_disk->size_in_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function disk_interface_names(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"usb\"");
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_names(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL){
				websWrite(wp, "\"%s\"", follow_partition->device);
				continue;
			}

			Ptr = rindex(follow_partition->mount_point, '/');
			if (Ptr == NULL){
				websWrite(wp, "\"unknown\"");
				continue;
			}

			++Ptr;
			websWrite(wp, "\"%s\"", Ptr);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_devices(){\n");
	websWrite(wp, "    return [");

	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"%s\"", follow_partition->device);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_types(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL){
				websWrite(wp, "\"unknown\"");
				continue;
			}

			websWrite(wp, "\"%s\"", follow_partition->file_system);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_mirror_counts(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "0");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_status(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			if (follow_partition->mount_point == NULL){
				websWrite(wp, "\"unmounted\"");
				continue;
			}

			//if (strcmp(follow_partition->file_system, "ntfs") == 0)
			//	websWrite(wp, "\"ro\"");
			//else
			websWrite(wp, "\"rw\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_kilobytes(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "%llu", follow_partition->size_in_kilobytes);
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_encryption_password_is_missing(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "\"no\"");
		}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function pool_kilobytes_in_use(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "%llu", follow_partition->used_kilobytes);
		}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	u64 disk_used_kilobytes;

	websWrite(wp, "function disk_usage(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		disk_used_kilobytes = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			disk_used_kilobytes += follow_partition->size_in_kilobytes;

		websWrite(wp, "%llu", disk_used_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	disk_info_t *follow_disk2;
	u32 disk_num, pool_num;
	websWrite(wp, "function per_pane_pool_usage_kilobytes(pool_num, disk_num){\n");
	for (follow_disk = disks_info, pool_num = 0; follow_disk != NULL; follow_disk = follow_disk->next){
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++pool_num){
			websWrite(wp, "    if (pool_num == %d){\n", pool_num);
			for (follow_disk2 = disks_info, disk_num = 0; follow_disk2 != NULL; follow_disk2 = follow_disk2->next, ++disk_num){
				websWrite(wp, "	if (disk_num == %d) {\n", disk_num);

				//if (strcmp(follow_disk2->tag, follow_disk->tag) == 0)
				if (follow_disk2->major == follow_disk->major && follow_disk2->minor == follow_disk->minor)
					websWrite(wp, "	    return [%llu];\n", follow_partition->size_in_kilobytes);
				else
					websWrite(wp, "	    return [0];\n");

				websWrite(wp, "	}\n");
			}
			websWrite(wp, "    }\n");
		}
	}
	websWrite(wp, "}\n\n");
	free_disk_data(&disks_info);

	return 0;
}

#ifdef RTCONFIG_INTERNAL_GOBI
#define MAX_MODEMINFO_NUM 6
#else
#define MAX_MODEMINFO_NUM 4
#endif

static int ej_get_usb_info(int eid, webs_t wp, int argc, char_t **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char ascii_tag[PATH_MAX];
	int disk_num = 0;
	char *Ptr;
	//int fromapp_flag = 0;

	//fromapp_flag = check_user_agent(user_agent);

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "[]");
		return -1;
	}

	char disk_size[16], disk_use[16], partNumber[16], mountNumber[16], part_disk_size[16], part_disk_use[16], error_code_s[16], usb_path_tmp[16];
	struct json_object *item = NULL;
	struct json_object *part_item = NULL;
	struct json_object *part_array_obj = NULL;
	struct json_object *part_array_obj_size = NULL;
	struct json_object *part_array_obj_use = NULL;

	json_object *jarray = json_object_new_array();
	json_object *part_jarray = NULL;

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {

		item = json_object_new_object();
		part_jarray = json_object_new_array();

		/* tmpdisk.deviceIndex */
		json_object_object_add(item,"deviceIndex", json_object_new_int(disk_num));

		/* tmpdisk.deviceName */
		memset(ascii_tag, 0, PATH_MAX);
		char_to_ascii_safe(ascii_tag, follow_disk->tag, PATH_MAX);
		json_object_object_add(item,"deviceName", json_object_new_string(ascii_tag));

		/* tmpdisk.usbPath */
		memset(usb_path_tmp, 0, 16);
		sprintf(usb_path_tmp, "%c", follow_disk->port[0]);
		json_object_object_add(item,"usbPath", json_object_new_string(usb_path_tmp));

		/* tmpdisk.node */
		json_object_object_add(item,"node", json_object_new_string(follow_disk->port));

		/* tmpdisk.deviceType */
		json_object_object_add(item,"deviceType", json_object_new_string("storage"));

		/* tmpdisk.mountNumber */
		memset(mountNumber, 0, 16);
		sprintf(mountNumber, "%u", follow_disk->mounted_number);
		json_object_object_add(item,"mountNumber", json_object_new_string(mountNumber));

		/* tmpdisk.partNumber */
		memset(partNumber, 0, 16);
		sprintf(partNumber, "%u", follow_disk->partition_number);
		json_object_object_add(item,"partNumber", json_object_new_string(partNumber));

		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			part_item = json_object_new_object();

			/* tmpParts.partName tmpParts.format tmpParts.status */
			if (follow_partition->mount_point == NULL){
				json_object_object_add(part_item,"partName", json_object_new_string(follow_partition->device));
				json_object_object_add(part_item,"format", json_object_new_string("unknown"));
				json_object_object_add(part_item,"status", json_object_new_string("unmounted"));
			}else{
				Ptr = rindex(follow_partition->mount_point, '/');
				if (Ptr == NULL)
					json_object_object_add(part_item,"partName", json_object_new_string("unknown"));
				else{
					++Ptr;
					json_object_object_add(part_item,"partName", json_object_new_string(Ptr));
				}

				json_object_object_add(part_item,"format", json_object_new_string(follow_partition->file_system));
				json_object_object_add(part_item,"status", json_object_new_string("rw"));
			}

			/* tmpParts.mountPoint */
			json_object_object_add(part_item,"mountPoint", json_object_new_string(follow_partition->device));

			/* tmpParts.isAppDev tmpdisk.hasAppDev */
			if(nvram_match("apps_dev", follow_partition->device)){
				json_object_object_add(part_item,"isAppDev", json_object_new_int(1));
				json_object_object_add(item,"hasAppDev", json_object_new_int(1));
			}else{
				json_object_object_add(part_item,"isAppDev", json_object_new_int(0));
				json_object_object_add(item,"hasAppDev", json_object_new_int(0));
			}

			/* tmpParts.isTM tmpdisk.hasTM */
			if(nvram_match("tm_device_name", follow_partition->device)){

				json_object_object_add(part_item,"isTM", json_object_new_int(1));
				json_object_object_add(item,"hasTM", json_object_new_int(1));
			}else{
				json_object_object_add(part_item,"isTM", json_object_new_int(0));
				json_object_object_add(item,"hasTM", json_object_new_int(0));
			}

			/* tmpParts.size */
			memset(part_disk_size, 0, 16);
			sprintf(part_disk_size, "%llu", follow_partition->size_in_kilobytes);
			json_object_object_add(part_item,"size", json_object_new_string(part_disk_size));
			json_object_object_add(part_item,"size_int", json_object_new_int(follow_partition->size_in_kilobytes));

			/* tmpParts.used */
			memset(part_disk_use, 0, 16);
			sprintf(part_disk_use, "%llu", follow_partition->used_kilobytes);
			json_object_object_add(part_item,"used", json_object_new_string(part_disk_use));
			json_object_object_add(part_item,"used_int", json_object_new_int(follow_partition->used_kilobytes));

			/* tmpParts.fsck */
#define MAX_ERROR_CODE 3
			int error_code, got_code;
			char file_name[32];
			FILE *fp;
			for(error_code = 0, got_code = 0; error_code <= MAX_ERROR_CODE; ++error_code){
				memset(file_name, 0, 32);
				sprintf(file_name, "/tmp/fsck_ret/%s.%d", follow_partition->device, error_code);

				if((fp = fopen(file_name, "r")) != NULL){
					fclose(fp);
					memset(error_code_s, 0, 16);
					sprintf(error_code_s, "%d", error_code);
					json_object_object_add(part_item,"fsck", json_object_new_string(error_code_s));
					got_code = 1;
					break;
				}
			}
			if(!got_code)
				json_object_object_add(part_item,"fsck", json_object_new_string(""));

			/* tmpDisk.hasErrPart */
			if(error_code == 1)
				json_object_object_add(item,"hasErrPart", json_object_new_int(1));
			else
				json_object_object_add(item,"hasErrPart", json_object_new_int(0));

			json_object_array_add(part_jarray,part_item);
		}

		json_object_object_add(item,"partition", part_jarray);	//add Partition to item

		/* tmpdisk.totalSize tmpdisk.totalUsed */
		int i, arraylen = 0, total_size = 0, total_use = 0;
		arraylen = json_object_array_length(part_jarray);
		for (i = 0; i < arraylen; i++){
			part_array_obj = json_object_array_get_idx(part_jarray, i);
			json_object_object_get_ex(part_array_obj, "size_int", &part_array_obj_size);
			json_object_object_get_ex(part_array_obj, "used_int", &part_array_obj_use);
			total_size += json_object_get_int(part_array_obj_size);
			total_use += json_object_get_int(part_array_obj_use);
		}
		memset(disk_size, 0, 16);
		sprintf(disk_size, "%d", total_size);
		memset(disk_use, 0, 16);
		sprintf(disk_use, "%d", total_use);
		json_object_object_add(item,"totalSize", json_object_new_string(disk_size));
		json_object_object_add(item,"totalUsed", json_object_new_string(disk_use));

		json_object_array_add(jarray,item);

		disk_num++;
	}

	/*** show_usb_path ***/
	DIR *bus_usb;
	struct dirent *interface;
	char usb_port[8], port_path[8], *ptr;
	int port_num, port_order, hub_order;
	char all_usb_path[MAX_USB_PORT][MAX_USB_HUB_PORT][3][16]; // MAX USB hub port number is 6.
	char tmp[100], prefix[32];

	if((bus_usb = opendir(USB_DEVICE_PATH)) == NULL) {
		free_disk_data(&disks_info);
		return -1;
	}

	memset(all_usb_path, 0, MAX_USB_PORT*MAX_USB_HUB_PORT*3*16);

	while((interface = readdir(bus_usb)) != NULL){
		if(interface->d_name[0] == '.')
			continue;

		if(!isdigit(interface->d_name[0]))
			continue;

		if(strchr(interface->d_name, ':'))
			continue;

		if(get_usb_port_by_string(interface->d_name, usb_port, 8) == NULL)
			continue;

		port_num = get_usb_port_number(usb_port);
		port_order = port_num-1;
		if((ptr = strchr(interface->d_name+strlen(usb_port), '.')) != NULL)
			hub_order = atoi(ptr+1);
		else
			hub_order = 0;

		if(get_path_by_node(interface->d_name, port_path, 8) == NULL)
			continue;

		strncpy(all_usb_path[port_order][hub_order][0], port_path, 16);
		snprintf(prefix, 32, "usb_path%s", port_path);
		strncpy(all_usb_path[port_order][hub_order][1], nvram_safe_get(prefix), 16);
		strncpy(all_usb_path[port_order][hub_order][2], nvram_safe_get(strcat_r(prefix, "_speed", tmp)), 16);
	}
	closedir(bus_usb);
	/*** show_usb_path end ***/

	/*** get_printer_info ***/
	int printer_num, got_printer;
	char printer_array[MAX_USB_PRINTER_NUM][4][64];
	char dev[8], usb_node[32];

	memset(printer_array, 0, MAX_USB_PRINTER_NUM*4*64);

	for(printer_num = 0, got_printer = 0; printer_num < MAX_USB_PRINTER_NUM; ++printer_num){
		snprintf(dev, sizeof(dev), "lp%d", printer_num);
		if(get_usb_node_by_device(dev, usb_node, sizeof(usb_node)) && strlen(usb_node) > 0){
			if(get_path_by_node(usb_node, port_path, 8) != NULL){
				snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

				strncpy(printer_array[got_printer][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
				strncpy(printer_array[got_printer][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
				strncpy(printer_array[got_printer][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
				strncpy(printer_array[got_printer][3], port_path, 64);

				++got_printer;
			}
		}
	}
	/*** get_printer_info end ***/

	/* get_modem_info */
	int i, j, got_modem;
	char modem_array[MAX_USB_PORT*MAX_USB_HUB_PORT][MAX_MODEMINFO_NUM][64];
#ifdef RTCONFIG_INTERNAL_GOBI
	char act_node[32], act_port_path[8];
	int modem_unit;
	char tmp2[100], prefix2[32];
#endif

	memset(modem_array, 0, MAX_USB_PORT*MAX_USB_HUB_PORT*MAX_MODEMINFO_NUM*64);

	got_modem = 0;
	for(i = 1; i <= MAX_USB_PORT; ++i){
		snprintf(prefix, 32, "usb_path%d", i);
		if(!strcmp(nvram_safe_get(prefix), "modem")){
			snprintf(port_path, 8, "%d", i);

			strncpy(modem_array[got_modem][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
			strncpy(modem_array[got_modem][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
			strncpy(modem_array[got_modem][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
			strncpy(modem_array[got_modem][3], port_path, 64);
#ifdef RTCONFIG_INTERNAL_GOBI
			for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
				usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

				snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
				if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL)
					memset(act_port_path, 0, 8);

				if(!strcmp(port_path, act_port_path)){
					strncpy(modem_array[got_modem][4], nvram_safe_get(strcat_r(prefix2, "act_signal", tmp2)), 64);
					strncpy(modem_array[got_modem][5], nvram_safe_get(strcat_r(prefix2, "act_operation", tmp2)), 64);
				}
				else{
					memset(modem_array[got_modem][4], 0, 64);
					memset(modem_array[got_modem][5], 0, 64);
				}
			}
#endif

			++got_modem;
		}
		else{
			for(j = 1; j <= MAX_USB_HUB_PORT; ++j){
				snprintf(prefix, 32, "usb_path%d.%d", i, j);

				if(!strcmp(nvram_safe_get(prefix), "modem")){
					snprintf(port_path, 8, "%d.%d", i, j);

					strncpy(modem_array[got_modem][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
					strncpy(modem_array[got_modem][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
					strncpy(modem_array[got_modem][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
					strncpy(modem_array[got_modem][3], port_path, 64);
#ifdef RTCONFIG_INTERNAL_GOBI
					for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
						usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

						snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
						if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL)
							memset(act_port_path, 0, 8);

						if(!strcmp(port_path, act_port_path)){
							strncpy(modem_array[got_modem][4], nvram_safe_get(strcat_r(prefix2, "act_signal", tmp2)), 64);
							strncpy(modem_array[got_modem][5], nvram_safe_get(strcat_r(prefix2, "act_operation", tmp2)), 64);
						}
						else{
							memset(modem_array[got_modem][4], 0, 64);
							memset(modem_array[got_modem][5], 0, 64);
						}
					}
#endif

					++got_modem;
				}
			}
		}
	}
	/* end get_modem_info */

	/* generate printer/modem json data */
	char manuf_device_name[128];
	for(port_order = 0; port_order < MAX_USB_PORT; ++port_order){
	   for(hub_order = 0; hub_order < MAX_USB_HUB_PORT; ++hub_order){
	     if(strlen(all_usb_path[port_order][hub_order][1]) > 0){
	        if(all_usb_path[port_order][hub_order][1] != NULL && !strstr(all_usb_path[port_order][hub_order][1],"storage")){
		   item = json_object_new_object();
		   if(strcmp(all_usb_path[port_order][hub_order][1], "printer") == 0){
		      for(printer_num = 0; printer_num < got_printer; ++printer_num){
		         if(strlen(printer_array[printer_num][3]) > 0
			    && (strlen(all_usb_path[port_order][hub_order][0]) == strlen(printer_array[printer_num][3]))
			    && strcmp(all_usb_path[port_order][hub_order][0], printer_array[printer_num][3]) == 0){
				/* tmpDisk.manufacturer */
				if(strlen(printer_array[printer_num][0]) > 0){
				   json_object_object_add(item,"manufacturer", json_object_new_string(printer_array[printer_num][0]));
				}else
				   json_object_object_add(item,"manufacturer", json_object_new_string(""));

				/* tmpDisk.deviceName */
				if(strlen(printer_array[printer_num][0]) > 0 && strlen(printer_array[printer_num][1]) > 0 && !strstr(printer_array[printer_num][1],printer_array[printer_num][0])){
					memset(manuf_device_name, 0, 128);
					sprintf(manuf_device_name, "%s %s", printer_array[printer_num][0], printer_array[printer_num][1]);
					json_object_object_add(item,"deviceName", json_object_new_string(manuf_device_name));
				}else
					json_object_object_add(item,"deviceName", json_object_new_string(printer_array[printer_num][1]));

				/* tmpDisk.serialNum */
				if(strlen(printer_array[printer_num][2]) > 0)
					json_object_object_add(item,"serialNum", json_object_new_string(printer_array[printer_num][2]));
				else
					json_object_object_add(item,"serialNum", json_object_new_string(""));
			 } // end strlen(printer_array[printer_num][3]) > 0
		      } //end printer_num for loop
		   }else if(strcmp(all_usb_path[port_order][hub_order][1], "modem") == 0){
		      for(i = 0; i < got_modem; ++i){
		         if(strlen(modem_array[i][3]) > 0
			    && (strlen(all_usb_path[port_order][hub_order][0]) == strlen(modem_array[i][3]))
			    && strcmp(all_usb_path[port_order][hub_order][0], modem_array[i][3]) == 0){
				if(strlen(modem_array[i][0]) > 0){
				   json_object_object_add(item,"manufacturer", json_object_new_string(modem_array[i][0]));
				}else
				   json_object_object_add(item,"manufacturer", json_object_new_string(""));

				/* tmpDisk.deviceName */
				if(strlen(modem_array[i][0]) > 0 && strlen(modem_array[i][1]) > 0 && !strstr(modem_array[i][1],modem_array[i][0])){
					memset(manuf_device_name, 0, 128);
					sprintf(manuf_device_name, "%s %s", modem_array[i][0], modem_array[i][1]);
					json_object_object_add(item,"deviceName", json_object_new_string(manuf_device_name));
				}else
					json_object_object_add(item,"deviceName", json_object_new_string(modem_array[i][1]));

				/* tmpDisk.serialNum */
				if(strlen(modem_array[i][2]) > 0)
					json_object_object_add(item,"serialNum", json_object_new_string(modem_array[i][2]));
				else
					json_object_object_add(item,"serialNum", json_object_new_string(""));
			 }
		      }
		   }//find printer/medom

		   /* tmpdisk.usbPath */
		   if(strlen(all_usb_path[port_order][hub_order][0]) > 0){
		   memset(usb_path_tmp, 0, 16);
		   sprintf(usb_path_tmp, "%c", all_usb_path[port_order][hub_order][0][0]);
		   json_object_object_add(item,"usbPath", json_object_new_string(usb_path_tmp));
		   }

		   /* tmpdisk.deviceIndex */
		   json_object_object_add(item,"deviceIndex", json_object_new_int(disk_num));

		   /* tmpdisk.node */
		   json_object_object_add(item,"node", json_object_new_string(all_usb_path[port_order][hub_order][0]));

		   /* tmpdisk.deviceType */
		   json_object_object_add(item,"deviceType", json_object_new_string(all_usb_path[port_order][hub_order][1]));

		   /* tmpdisk.hasAppDev */
		   json_object_object_add(item,"hasAppDev", json_object_new_int(0));

		   /* tmpdisk.hasTM */
		   json_object_object_add(item,"hasTM", json_object_new_int(0));

		   /* tmpdisk.hasErrPart */
		   json_object_object_add(item,"hasErrPart", json_object_new_int(0));

		   disk_num++;
		   json_object_array_add(jarray,item);
		} // end not storage device
	      }	//end all_usb_path[port_order][hub_order][1] if
	   }	//end hub_order for loop
	} //end port_order for loop

	//_dprintf ("The json object created: %s\n",json_object_to_json_string(jarray));

	websWrite(wp, "%s", json_object_to_json_string(jarray));

	json_object_put(jarray);

	free_disk_data(&disks_info);

	return 0;
}

static int ej_available_disk_names_and_sizes(int eid, webs_t wp, int argc, char_t **argv){
	disk_info_t *disks_info, *follow_disk;
	int first;
	char ascii_tag[PATH_MAX], ascii_vendor[PATH_MAX], ascii_model[PATH_MAX];

	websWrite(wp, "function available_disks(){ return [];}\n\n");
	websWrite(wp, "function available_disk_sizes(){ return [];}\n\n");
	websWrite(wp, "function claimed_disks(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_interface_names(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_model_info(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_total_size(){ return [];}\n\n");
	websWrite(wp, "function claimed_disk_total_mounted_number(){ return [];}\n\n");
	websWrite(wp, "function blank_disks(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_interface_names(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_model_info(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_total_size(){ return [];}\n\n");
	websWrite(wp, "function blank_disk_total_mounted_number(){ return [];}\n\n");

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "%s", initial_available_disk_names_and_sizes());
		return -1;
	}

	/* show name of the foreign disks */
	websWrite(wp, "function foreign_disks(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		memset(ascii_tag, 0, PATH_MAX);
		char_to_ascii_safe(ascii_tag, follow_disk->tag, PATH_MAX);
		websWrite(wp, "\"%s\"", ascii_tag);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show interface of the foreign disks */
	websWrite(wp, "function foreign_disk_interface_names(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

//		websWrite(wp, "\"USB\"");
		websWrite(wp, "\"%s\"", follow_disk->port);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show model info of the foreign disks */
	websWrite(wp, "function foreign_disk_model_info(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"");

		if (follow_disk->vendor != NULL){
			memset(ascii_vendor, 0, PATH_MAX);
			char_to_ascii_safe(ascii_vendor, follow_disk->vendor, PATH_MAX);
			websWrite(wp, "%s", ascii_vendor);
		}
		if (follow_disk->model != NULL){
			if (follow_disk->vendor != NULL)
				websWrite(wp, " ");

			memset(ascii_model, 0, PATH_MAX);
			char_to_ascii_safe(ascii_model, follow_disk->model, PATH_MAX);
			websWrite(wp, "%s", ascii_model);
		}
		websWrite(wp, "\"");
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total_size of the foreign disks */
	websWrite(wp, "function foreign_disk_total_size(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%llu\"", follow_disk->size_in_kilobytes);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total number of the partitions in this foreign disk */
	websWrite(wp, "function foreign_disk_pool_number(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%u\"", follow_disk->partition_number);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	/* show total number of the mounted partitions in this foreign disk */
	websWrite(wp, "function foreign_disk_total_mounted_number(){\n");
	websWrite(wp, "    return [");
	first = 1;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		if (first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%u\"", follow_disk->mounted_number);
	}
	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	free_disk_data(&disks_info);

	return 0;
}

static int ej_get_printer_info(int eid, webs_t wp, int argc, char_t **argv){
	int printer_num, got_printer;
	char tmp[100], prefix[32];
	char printer_array[MAX_USB_PRINTER_NUM][4][64];
	char dev[8], usb_node[32];
	char port_path[8];

	memset(printer_array, 0, MAX_USB_PRINTER_NUM*4*64);

	for(printer_num = 0, got_printer = 0; printer_num < MAX_USB_PRINTER_NUM; ++printer_num){
		snprintf(dev, sizeof(dev), "lp%d", printer_num);
		if(get_usb_node_by_device(dev, usb_node, sizeof(usb_node)) && strlen(usb_node) > 0){
			if(get_path_by_node(usb_node, port_path, 8) != NULL){
				snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);

				strncpy(printer_array[got_printer][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
				strncpy(printer_array[got_printer][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
				strncpy(printer_array[got_printer][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
				strncpy(printer_array[got_printer][3], port_path, 64);

				++got_printer;
			}
		}
	}

	websWrite(wp, "function printer_manufacturers(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][0]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][0]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_models(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][1]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][1]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_serialn(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][2]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][2]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function printer_pool(){\n");
	websWrite(wp, "    return [");

	for(printer_num = 0; printer_num < got_printer; ++printer_num){
		if(printer_num != 0)
			websWrite(wp, ", ");

		if(strlen(printer_array[printer_num][3]) > 0)
			websWrite(wp, "\"%s\"", printer_array[printer_num][3]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	return 0;
}

static int ej_get_modem_info(int eid, webs_t wp, int argc, char_t **argv){
	int i, j, got_modem;
	char tmp[100], prefix[32];
	char modem_array[MAX_USB_PORT*MAX_USB_HUB_PORT][MAX_MODEMINFO_NUM][64];
	char port_path[8];
#ifdef RTCONFIG_INTERNAL_GOBI
	char act_node[32], act_port_path[8];
	int modem_unit;
	char tmp2[100], prefix2[32];
#endif

	memset(modem_array, 0, MAX_USB_PORT*MAX_USB_HUB_PORT*MAX_MODEMINFO_NUM*64);

	got_modem = 0;
	for(i = 1; i <= MAX_USB_PORT; ++i){
		snprintf(prefix, 32, "usb_path%d", i);
		if(!strcmp(nvram_safe_get(prefix), "modem")){
			snprintf(port_path, 8, "%d", i);

			strncpy(modem_array[got_modem][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
			strncpy(modem_array[got_modem][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
			strncpy(modem_array[got_modem][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
			strncpy(modem_array[got_modem][3], port_path, 64);
#ifdef RTCONFIG_INTERNAL_GOBI
			for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
				usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

				snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
				if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL)
					memset(act_port_path, 0, 8);

				if(!strcmp(port_path, act_port_path)){
					strncpy(modem_array[got_modem][4], nvram_safe_get(strcat_r(prefix2, "act_signal", tmp2)), 64);
					strncpy(modem_array[got_modem][5], nvram_safe_get(strcat_r(prefix2, "act_operation", tmp2)), 64);
				}
				else{
					memset(modem_array[got_modem][4], 0, 64);
					memset(modem_array[got_modem][5], 0, 64);
				}
			}
#endif

			++got_modem;
		}
		else{
			for(j = 1; j <= MAX_USB_HUB_PORT; ++j){
				snprintf(prefix, 32, "usb_path%d.%d", i, j);

				if(!strcmp(nvram_safe_get(prefix), "modem")){
					snprintf(port_path, 8, "%d.%d", i, j);

					strncpy(modem_array[got_modem][0], nvram_safe_get(strcat_r(prefix, "_manufacturer", tmp)), 64);
					strncpy(modem_array[got_modem][1], nvram_safe_get(strcat_r(prefix, "_product", tmp)), 64);
					strncpy(modem_array[got_modem][2], nvram_safe_get(strcat_r(prefix, "_serial", tmp)), 64);
					strncpy(modem_array[got_modem][3], port_path, 64);
#ifdef RTCONFIG_INTERNAL_GOBI
					for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
						usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

						snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
						if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL)
							memset(act_port_path, 0, 8);

						if(!strcmp(port_path, act_port_path)){
							strncpy(modem_array[got_modem][4], nvram_safe_get(strcat_r(prefix2, "act_signal", tmp2)), 64);
							strncpy(modem_array[got_modem][5], nvram_safe_get(strcat_r(prefix2, "act_operation", tmp2)), 64);
						}
						else{
							memset(modem_array[got_modem][4], 0, 64);
							memset(modem_array[got_modem][5], 0, 64);
						}
					}
#endif

					++got_modem;
				}
			}
		}
	}

	websWrite(wp, "function modem_manufacturers(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][0]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][0]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_models(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][1]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][1]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_serialn(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][2]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][2]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_pool(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][3]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][3]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

#ifdef RTCONFIG_INTERNAL_GOBI
	websWrite(wp, "function modem_signal(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][4]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][4]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");

	websWrite(wp, "function modem_operation(){\n");
	websWrite(wp, "    return [");

	for(i = 0; i < got_modem; ++i){
		if(i != 0)
			websWrite(wp, ", ");

		if(strlen(modem_array[i][5]) > 0)
			websWrite(wp, "\"%s\"", modem_array[i][5]);
		else
			websWrite(wp, "\"\"");
	}

	websWrite(wp, "];\n");
	websWrite(wp, "}\n\n");
#endif

	return 0;
}

#if 0
static int modem_simstatus_hook(int eid, webs_t wp, int argc, char_t **argv){//Cherry Cho added in 2014/9/4.
#ifdef RTCONFIG_INTERNAL_GOBI
	char act_node[32], act_port_path[8];
	char *cmd_simsignal[] = {"/usr/sbin/modem_status.sh", "signal", NULL};
	char *cmd_simop[] = {"/usr/sbin/modem_status.sh", "operation", NULL};
	char *cmd_simbytes[] = {"/usr/sbin/modem_status.sh", "bytes", NULL};
	float rx_Gbytes, tx_Gbytes;
	int pid2, pid3, pid4;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	_eval(cmd_simsignal, NULL, 0, &pid2);
	_eval(cmd_simop, NULL, 0, &pid3);
	_eval(cmd_simbytes, NULL, 0, &pid4);
#endif
	return 0;
}

static int ej_check_modem_sim(int eid, webs_t wp, int argc, char_t **argv){
	char act_node[32], act_port_path[8];
	int status;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	status = nvram_get_int("usb_modem_act_sim");
	websWrite(wp, "%d", status);

	return 0;
}
#endif

static int ej_get_modem_fullsignal(int eid, webs_t wp, int argc, char_t **argv){//Cherry Cho added in 2015/12/2.
#if 0 // move this to wanduck.
#ifdef RTCONFIG_INTERNAL_GOBI
	char act_node[32], act_port_path[8];

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	eval("/usr/sbin/modem_status.sh", "fullsignal");
#endif
#endif

	return 0;
}

static int ej_get_isp_scan_results(int eid, webs_t wp, int argc, char_t **argv){
	int ret = 0;
#ifdef RTCONFIG_INTERNAL_GOBI
	char file_name[MAX_LINE_SIZE];

	memset(file_name, 0, MAX_LINE_SIZE);
	sprintf(file_name, "%s", nvram_safe_get("modem_roaming_scanlist"));
	if(strlen(file_name) >= 0)
		ret = dump_file(wp, file_name);
#endif

	return ret;
}

static int ej_get_simact_result(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_INTERNAL_GOBI
	char act_node[32], act_port_path[8];
	FILE *fp;
	char buf[256];
	int len = 0;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	if ((fp = fopen("/tmp/modem_action.ret", "r")) != NULL) {
		while(fgets(buf, sizeof(buf), fp) != NULL){
			len = strlen(buf) - 1;
			if(len > 0){
				if(buf[len] == '\n' || buf[len] == '\r')
					buf[len] = '\0';
				websWrite(wp, buf);
				break;
			}
		}
		fclose(fp);
	}
#endif
	return 0;
}

static int ej_modemuptime(int eid, webs_t wp, int argc, char_t **argv){
	int ret = 0;
	unsigned int now, start = atoi(nvram_safe_get("usb_modem_act_startsec"));
	char *str;

	if(start <= 0){
		ret = websWrite(wp, "0");
		return ret;
	}

	str = file2str("/proc/uptime");
	if(!str){
		ret = websWrite(wp, "0");
		return ret;
	}

	now = atoi(str);
	free(str);

	ret = websWrite(wp, "%u", (now-start));

	return ret;
}

#ifdef RTCONFIG_USB_MULTIMODEM
static int ej_get_simact1_result(int eid, webs_t wp, int argc, char_t **argv){
#ifdef RTCONFIG_INTERNAL_GOBI
	char act_node[32], act_port_path[8];
	FILE *fp;
	char buf[256];
	int len = 0;

	snprintf(act_node, 32, "%s", nvram_safe_get("usb_modem1_act_path"));
	if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
		return 0;
	}

	if ((fp = fopen("/tmp/modem_action1.ret", "r")) != NULL) {
		while(fgets(buf, sizeof(buf), fp) != NULL){
			len = strlen(buf) - 1;
			if(len > 0){
				if(buf[len] == '\n' || buf[len] == '\r')
					buf[len] = '\0';
				websWrite(wp, buf);
				break;
			}
		}
		fclose(fp);
	}
#endif
	return 0;
}

static int ej_modem1uptime(int eid, webs_t wp, int argc, char_t **argv){
	int ret = 0;
	unsigned int now, start = atoi(nvram_safe_get("usb_modem1_act_startsec"));
	char *str;

	if(start <= 0){
		ret = websWrite(wp, "0");
		return ret;
	}

	str = file2str("/proc/uptime");
	if(!str){
		ret = websWrite(wp, "0");
		return ret;
	}

	now = atoi(str);
	free(str);

	ret = websWrite(wp, "%u", (now-start));

	return ret;
}
#endif

#if defined(RTCONFIG_USB_SMS_MODEM) && !defined(RTCONFIG_USB_MULTIMODEM)
int ej_getSMSbyType(int eid, webs_t wp, int argc, char **argv){
	char ttynode[16];
	int sms_type;
	int sms_total, sms_num, sms_index;
	char sms_indexs[MAX_BUF_SIZE], *ptr, *token;
	char pdu[MAX_BUF_SIZE], data[MAX_BUF_SIZE];
	char OA[MAX_BUF_SIZE], SCTS[MAX_BUF_SIZE];

	if (ejArgs(argc, argv, "%s", &ptr) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	snprintf(ttynode, 16, "%s", nvram_safe_get("usb_modem_act_int"));
	sms_type = strtod(ptr, NULL);

	if((sms_total = getSMSPDUbyType(ttynode, sms_type, sms_indexs, MAX_BUF_SIZE)) < 0){
		printf("%s: Fail to list the %d type SMS.\n", __func__, sms_type);
		return -1;
	}

	websWrite(wp, "\t<sms_%s>\n", getSMSTypeStr(sms_type));

	if(sms_total <= 0){
		websWrite(wp, "\t</sms_%s>\n", getSMSTypeStr(sms_type));
		return 0;
	}

	websWrite(wp, "\t\t<sms_list>%s</sms_list>\n", sms_indexs);

	ptr = sms_indexs;
	token = strtok(ptr, ",");
	sms_index = strtod(token, NULL);

	if(getSMSPDUbyIndex(ttynode, sms_index, pdu, MAX_BUF_SIZE) < 0){
		printf("Failed to read the %dth SMS.\n", sms_index);
		return -1;
	}

	memset(OA, 0, MAX_BUF_SIZE);
	memset(SCTS, 0, MAX_BUF_SIZE);

	if(decomposeSMSPDU(sms_type, pdu, data, MAX_BUF_SIZE, OA, MAX_BUF_SIZE, SCTS, MAX_BUF_SIZE) < 0){
		printf("Failed to decompose the %dth SMS.\n", sms_index);
		return -1;
	}

	websWrite(wp, "\t\t<sms_%d>\n", sms_index);
	websWrite(wp, "\t\t\t<index>%d</index>\n", sms_index);
	websWrite(wp, "\t\t\t<scts>%s</scts>\n", SCTS);
	websWrite(wp, "\t\t\t<number>%s</number>\n", OA);
	websWrite(wp, "\t\t\t<string>%s</string>\n", data);
	websWrite(wp, "\t\t</sms_%d>\n", sms_index);

	for(sms_num = 1; sms_num < sms_total; ++sms_num){
		token = strtok(NULL, ",");
		sms_index = strtod(token, NULL);

		if(getSMSPDUbyIndex(ttynode, sms_index, pdu, MAX_BUF_SIZE) < 0){
			printf("Failed to read the %dth SMS.\n", sms_index);
			continue;
		}

		memset(OA, 0, MAX_BUF_SIZE);
		memset(SCTS, 0, MAX_BUF_SIZE);

		if(decomposeSMSPDU(sms_type, pdu, data, MAX_BUF_SIZE, OA, MAX_BUF_SIZE, SCTS, MAX_BUF_SIZE) < 0){
			printf("Failed to decompose the %dth SMS.\n", sms_index);
			continue;
		}

		websWrite(wp, "\t\t<sms_%d>\n", sms_index);
		websWrite(wp, "\t\t\t<index>%d</index>\n", sms_index);
		websWrite(wp, "\t\t\t<scts>%s</scts>\n", SCTS);
		websWrite(wp, "\t\t\t<number>%s</number>\n", OA);
		websWrite(wp, "\t\t\t<string>%s</string>\n", data);
		websWrite(wp, "\t\t</sms_%d>\n", sms_index);
	}

	websWrite(wp, "\t</sms_%s>\n", getSMSTypeStr(sms_type));

	return 0;
}

int ej_getPhonebook(int eid, webs_t wp, int argc, char **argv){
	char ttynode[16];
	int phone_total;
	char indexs[MAX_BUF_SIZE], phones[MAX_BUF_SIZE], names[MAX_BUF_SIZE];

	snprintf(ttynode, 16, "%s", nvram_safe_get("usb_modem_act_int"));

	if((phone_total = listPhonenum(ttynode, indexs, MAX_BUF_SIZE, phones, MAX_BUF_SIZE, names, MAX_BUF_SIZE)) < 0){
		printf("%s: Fail to list the Phone book.\n", __func__);
		return -1;
	}

	websWrite(wp, "<phonebook>\n");

	if(phone_total <= 0){
		websWrite(wp, "</phonebook>\n");
		return 0;
	}

	websWrite(wp, "\t<phone_index_list>%s</phone_index_list>\n", indexs);
	websWrite(wp, "\t<phone_num_list>%s</phone_num_list>\n", phones);
	websWrite(wp, "\t<phone_name_list>%s</phone_name_list>\n", names);

	websWrite(wp, "</phonebook>\n");

	return 0;
}
#endif

static int ej_usb_is_exist(int eid, webs_t wp, int argc, char_t **argv){
	DIR *bus_usb;
	struct dirent *interface;
	char usb_port[8], port_path[8];
	int ret = 0, usb_is_exist = 0;
	char prefix[32];

	if((bus_usb = opendir(USB_DEVICE_PATH)) == NULL)
		return -1;

	while((interface = readdir(bus_usb)) != NULL){
		if(interface->d_name[0] == '.')
			continue;

		if(!isdigit(interface->d_name[0]))
			continue;

		if(strchr(interface->d_name, ':'))
			continue;

		if(get_usb_port_by_string(interface->d_name, usb_port, 8) == NULL)
			continue;

		if(get_path_by_node(interface->d_name, port_path, 8) == NULL)
			continue;

		snprintf(prefix, 32, "usb_path%s", port_path);
		if(!strcmp(nvram_safe_get(prefix), "storage")){
			usb_is_exist = 1;
			break;
		}
	}
	closedir(bus_usb);

	ret += websWrite(wp, "\"%d\"", usb_is_exist);

	return ret;
}

#else
static int ej_show_usb_path(int eid, webs_t wp, int argc, char_t **argv){
	websWrite(wp, "[]");
	return 0;
}

int ej_apps_fsck_ret(int eid, webs_t wp, int argc, char **argv){
	websWrite(wp, "[]");
	return 0;
}

static int ej_usb_is_exist(int eid, webs_t wp, int argc, char_t **argv){
	websWrite(wp, "\"0\"");
	return 0;
}
#endif

int ej_shown_language_css(int eid, webs_t wp, int argc, char **argv){
	struct language_table *pLang = NULL;
	char lang[4];
	int len;
#ifdef RTCONFIG_AUTODICT
	unsigned char header[3] = { 0xef, 0xbb, 0xbf };
	FILE *fp = fopen("Lang_Hdr.txt", "r");
#else
	FILE *fp = fopen("Lang_Hdr", "r");
#endif
	char buffer[1024], key[30], target[30];
	char *follow_info, *follow_info_end;
	int offset = 0;

	if (fp == NULL){
		fprintf(stderr, "No English dictionary!\n");
		return 0;
	}

	memset(lang, 0, 4);
	strcpy(lang, nvram_safe_get("preferred_lang"));

	if(0){
		websWrite(wp, "<li style=\"visibility:hidden;\"><dl><a href=\"#\"><dt id=\"selected_lang\"></dt></a>\\n");
	}
	else{
		websWrite(wp, "<li><dl><a href=\"#\"><dt id=\"selected_lang\"></dt></a>\\n");
		while (1) {
			memset(buffer, 0, sizeof(buffer));
			if ((follow_info = fgets(buffer, sizeof(buffer), fp)) != NULL){
	#ifdef RTCONFIG_AUTODICT
				if (memcmp(buffer, header, 3) == 0) offset = 3;
	#endif
				if (strncmp(follow_info+offset, "LANG_", 5))    // 5 = strlen("LANG_")
					continue;

				follow_info += 5;
				follow_info_end = strstr(follow_info, "=");
				len = follow_info_end-follow_info;
				memset(key, 0, sizeof(key));
				strncpy(key, follow_info, len);

				for (pLang = language_tables; pLang->Lang != NULL; ++pLang){
					if (strcmp(key, pLang->Target_Lang))
						continue;
					follow_info = follow_info_end+1;
					follow_info_end = strstr(follow_info, "\n");
					len = follow_info_end-follow_info;
					memset(target, 0, sizeof(target));
					strncpy(target, follow_info, len);
					if (check_lang_support(key) && strcmp(key,lang))
						websWrite(wp, "<dd><a onclick=\"submit_language(this)\" id=\"%s\">%s</a></dd>\\n", key, target);
					break;
				}
			}
			else
				break;
		}
	}
	websWrite(wp, "</dl></li>\\n");
	fclose(fp);

	return 0;
}

//andi
char*
send_action(char *ftp_url,int port)
{
	char str[1024]={0};
	char buf[1024]={0};
	int my_fd;
	if ((my_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		return NULL;
	}

	struct sockaddr_in their_addr; /* connector's address information */
	bzero(&(their_addr), sizeof(their_addr)); /* zero the rest of the struct */
	their_addr.sin_family = AF_INET; /* host byte order */
	their_addr.sin_port = htons(port); /* short, network byte order */
	their_addr.sin_addr.s_addr = INADDR_ANY;
	//their_addr.sin_addr.s_addr = ((struct in_addr *)(he->h_addr))->s_addr;
	bzero(&(their_addr.sin_zero), sizeof(their_addr.sin_zero)); /* zero the rest of the struct */

	if (connect(my_fd, (struct sockaddr *)&their_addr,sizeof(struct sockaddr)) == -1) {
		perror("connect");
		close(my_fd);
		return NULL;
	}
	snprintf(str,sizeof(str),"refresh@%s",ftp_url);
	_dprintf("socket:%s\n",str);
	if (send(my_fd, str, strlen(str), 0) == -1) {
		perror("send");
		close(my_fd);
		return NULL;
	}

	int len;
	while ((len = recv(my_fd, buf, 1024, 0))) {
		//_dprintf("BUF:%s\n",buf);
		close(my_fd);
		return strdup(buf);
	}

	close(my_fd);
	return NULL;
}

//andi
static int
ftpServerTree_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *ftp_url;
	ftp_url = websGetVar(wp, "path","");
    	_dprintf("URL:%s\n",ftp_url);

	char *buf = send_action(ftp_url,3568);
	_dprintf("BUF:%s\n",buf);
	if(buf == NULL)
	{
		websWrite(wp,"NULL");
		return 0;
	}
	else
		websWrite(wp,buf);

	return 0;
}

#ifdef  __CONFIG_NORTON__
/* Trigger an NGA LiveUpdate (linux/netbsd - no support for ECOS) */
static int
nga_update(void)
{
	char *str = NULL;
	int pid;

	if ((str = file2str("/var/run/bootstrap.pid"))) {
		pid = atoi(str);
		free(str);
		return kill(pid, SIGHUP);
	}

	return -1;
}
#endif /* __CONFIG_NORTON__ */

void
json_unescape(char *s)
{
	char s_tmp[65535];
	unsigned int c;

	while ((s = strpbrk(s, "%+"))) {
		/* Parse %xx */
		if (*s == '%') {
			sscanf(s + 1, "%02x", &c);
			*s++ = (char) c;
			strlcpy(s_tmp, s + 2, sizeof(s_tmp));
			strncpy(s, s_tmp, strlen(s) + 1);
		}
		/* Space is special */
		else if (*s == '+')
			*s++ = ' ';
	}
}

void
decode_json_buffer(char *query)
{
	int len;
	char *q, *name, *value;

	/* Clear variables */
	if (!query) {
		//hdestroy_r(&htab);
		return;
	}

	/* Parse into individual assignments */
	q = query;
	len = strlen(query);

	for (q = query; q < (query + len);) {
		/* Unescape each assignment */
		json_unescape(name = value = q);

		/* Skip to next assignment */
		for (q += strlen(q); q < (query + len) && !*q; q++);
	}
}

void
do_json_decode(struct json_object **root)
{
	if(check_user_agent(user_agent) != 0){
		decode_json_buffer(post_json_buf);
		*root = json_tokener_parse(post_json_buf);
	}
}

static void
prepare_restore(webs_t wp){
	int offset = 10;
#ifdef RTCONFIG_RALINK
	if (get_model() == MODEL_RTN65U || get_model() == MODEL_RTAC85U || get_model() == MODEL_RTAC85P)
		offset = 15;
#endif

	/* Stop USB application prior to counting reboot_time.
	 * Don't stop 3G/4G here.  If yes and end-user connect to
	 * administrative page through 3G/4G, he/she can't see Restarting.asp
	 */
	if (!notify_rc_and_wait_2min("stop_app"))
		_dprintf("%s: send stop_app rc_service fail!\n", __func__);

	/* Enlarge reboot_time temporarily. */
	nvram_set_int("reboot_time", nvram_get_int("reboot_time") + offset);

	eval("/sbin/ejusb", "-1", "0");

	nvram_set("lan_ipaddr", nvram_default_safe_get("lan_ipaddr"));
	websApply(wp, "Restarting.asp");
	shutdown(fileno(wp), SHUT_RDWR);
	nvram_set("restore_defaults", "1");
	nvram_set("freeze_duck", "15");
}

static int
apply_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *action_mode;
	char *action_para;
	char *current_url;
	char *config_name;
	char command[128];
	memset(command, 0, sizeof(command));
	int i=0, j=0, len=0;
#ifdef RTCONFIG_LANTIQ
	wave_app_flag=0;
#endif

	struct json_object *root=NULL;

	do_json_decode(&root);

	action_mode = get_cgi_json("action_mode", root);
	current_url = get_cgi_json("current_page", root);
	_dprintf("apply: %s %s\n", action_mode, current_url);

#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] apply: %s %s\n", action_mode, current_url);
#endif

	if(!action_mode){
		_dprintf("action_mode get null\n");
		goto APPLY_FINISH;
	}

#ifdef RTCONFIG_USB_MODEM
	char *modem_unit_str;
	int modem_unit;
	char tmp2[100];
	char prefix2[32];

	modem_unit_str = get_cgi_json("modem_unit", root);
	if(!modem_unit_str)
		modem_unit = 0;
	else
		modem_unit = atoi(modem_unit_str);

	usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));
#endif

	if (!strcmp(action_mode, "apply")) {

		struct json_object *res=NULL;
		res = json_object_new_object();

		if (!validate_apply(wp,root)) {
			json_object_object_add(res, "modify", json_object_new_string("0"));
		}
		else {
			json_object_object_add(res, "modify", json_object_new_string("1"));
		}

		action_para = get_cgi_json("rc_service",root);
		config_name = get_cgi_json("nvram_config", root);

		if (config_name != NULL){
			//_dprintf("apply_cgi: nvram_config = %s\n", config_name);
			response_nvram_config(wp, config_name, res, root);
		}

		if(action_para && strlen(action_para) > 0) {
#ifdef RTCONFIG_CFGSYNC
			if (nvram_match("x_Setting", "1") && cfg_changed && pids("cfg_server") &&
				check_if_file_exist(CFG_SERVER_PID))
			{
				json_object *cfg_root = NULL;

				if ((cfg_root = json_object_from_file(CFG_JSON_FILE)) == NULL)
					_dprintf("cfg_root is null\n");
                                else /* add action_script */
                                        json_object_object_add(cfg_root, "action_script", json_object_new_string(action_para));

				/* save the changed nvram parameters */
				json_object_to_file(CFG_JSON_FILE, cfg_root);

				json_object_put(cfg_root);

				/* trigger cfg_server to send notification */
				kill_pidfile_s(CFG_SERVER_PID, SIGUSR2);
				cfg_changed = 0;
			}
			else
#endif                    
			notify_rc(action_para);
			json_object_object_add(res, "run_service", json_object_new_string(action_para));
		}
		websWrite(wp, "%s\n", json_object_to_json_string(res));
		json_object_put(res);
	}
	else if (!strcmp(action_mode," Refresh "))
	{
		char *system_cmd;
		system_cmd = get_cgi_json("SystemCmd",root);

		if(check_xss_blacklist(system_cmd, 0)){
			websRedirect_iframe(wp, current_url);
			goto APPLY_FINISH;
		}

		len = strlen(system_cmd);

		for(i=0;i<len;i++){
			if (isalnum(system_cmd[i]) != 0 || system_cmd[i] == ':' || system_cmd[i] == '-' || system_cmd[i] == '_' || system_cmd[i] == '.' || isspace(system_cmd[i]) != 0)
				j++;
			else{
				_dprintf("[httpd] Invalid SystemCmd!\n");
				strcpy(SystemCmd, "");
				websRedirect_iframe(wp, current_url);
				goto APPLY_FINISH;
			}
		}
		if(strstr(system_cmd,"\n") != NULL || strstr(system_cmd,"\r") != NULL){
			_dprintf("[httpd] Invalid SystemCmd!\n");
			strcpy(SystemCmd, "");
			websRedirect_iframe(wp, current_url);
			goto APPLY_FINISH;
		}
		if(!strcmp(current_url, "Main_Netstat_Content.asp") && (
			strncasecmp(system_cmd, "netstat", 7) == 0
		)){
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
		}
		else if(!strcmp(current_url, "Main_Analysis_Content.asp") && (
			   strncasecmp(system_cmd, "ping", 4) == 0
			|| strncasecmp(system_cmd, "traceroute", 10) == 0
			|| strncasecmp(system_cmd, "nslookup", 8) == 0
		)){
#if defined(RTCONFIG_DUALWAN)
			int sel = 1;
			char *p, *u, *wan, tmp[32] = "";

			u = get_cgi_json("wans_ntool_unit", root);
			if (!u || *u == '\0' || !nvram_match("wans_mode", "lb") ||
			    !strncasecmp(system_cmd, "nslookup", 8))
				sel = 0;
			p = strchr(system_cmd, ' ');
			if (sel && (get_nr_wan_unit() < 2 || !p))
				sel = 0;
			wan = get_wan_ifname(atoi(u));
			if (sel && (!wan || *wan == '\0')) {
				dbg("%s: Can't get WAN interface of unit %s\n", __func__, u? u : "NULL");
				sel = 0;
			}

			/* Insert "-I WAN_IFACE" or "-i WAN_IFACE" for ping and
			 * traceroute command respectively if necessary.
			 */
			if (sel) {
				*p = '\0';
				if (!strncasecmp(system_cmd, "ping", 4))
					snprintf(tmp, sizeof(tmp), "-I %s", wan);
				else if (!strncasecmp(system_cmd, "traceroute", 10))
					snprintf(tmp, sizeof(tmp), "-i %s", wan);
				snprintf(SystemCmd, sizeof(SystemCmd), "%s %s %s", system_cmd, tmp, p + 1);
			} else {
				strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
			}
#else
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
#endif
		}
		else if(!strcmp(current_url, "Main_WOL_Content.asp") && (
			strncasecmp(system_cmd, "ether-wake", 10) == 0
		)){
#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
			char dstmac[18];
			memset(dstmac, 0, sizeof(dstmac));
			strncpy(dstmac, system_cmd + strlen("ether-wake -i br0 "), sizeof(dstmac)-1);
#if defined(RTCONFIG_BCM_7114)
			eval("ether-wake", "-i", "eth1", dstmac);
			eval("ether-wake", "-i", "eth2", dstmac);
#if defined(RTAC5300)
			eval("ether-wake", "-i", "eth3", dstmac);
#endif
#elif defined(HND_ROUTER)
			eval("ether-wake", "-i", "eth3", dstmac);
			eval("ether-wake", "-i", "eth4", dstmac);
#endif
#endif
			strncpy(SystemCmd, system_cmd, sizeof(SystemCmd));
			sys_script("syscmd.sh");

		}
		else if(!strcmp(current_url, "Main_AdmStatus_Content.asp"))
		{
			system_cmd_test(system_cmd, SystemCmd, sizeof(SystemCmd));
		}
		else if(!strcmp(current_url, "Main_ConnStatus_Content.asp") &&
			(strncasecmp(system_cmd, "netstat-nat", 11) == 0)){
			strlcpy(SystemCmd, system_cmd, sizeof(SystemCmd));
		}

		else{
			_dprintf("[httpd] Invalid SystemCmd!\n");
			strcpy(SystemCmd, "");
		}
		websRedirect_iframe(wp, current_url);
	}
	else if (!strcmp(action_mode," Clear "))
	{
		unlink(get_syslog_fname(1));
		unlink(get_syslog_fname(0));
		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, " Restart ")||!strcmp(action_mode, "reboot"))
	{
		websApply(wp, "Restarting.asp");
		nvram_set("freeze_duck", "15");
		shutdown(fileno(wp), SHUT_RDWR);
		sys_reboot();
	}
	else if (!strcmp(action_mode, "Restore")||!strcmp(action_mode, "restore"))
	{
		prepare_restore(wp);
		sys_default();

#ifdef RTCONFIG_LETSENCRYPT
		if (f_exists(UPLOAD_CERT))
			unlink(UPLOAD_CERT);
		if (f_exists(UPLOAD_KEY))
			unlink(UPLOAD_KEY);
#endif
	}
	else if (!strcmp(action_mode, "restore_erase"))
	{
		prepare_restore(wp);
		sys_default_erase();

#ifdef RTCONFIG_LETSENCRYPT
		if (f_exists(UPLOAD_CERT))
			unlink(UPLOAD_CERT);
		if (f_exists(UPLOAD_KEY))
			unlink(UPLOAD_KEY);
#endif
	}
	else if (!strcmp(action_mode, "logout")) // but, every one can reset it by this call
	{
		websRedirect(wp, "Logout.asp");
	}
	else if (!strcmp(action_mode, "change_wl_unit"))
	{
		action_para = get_cgi_json("wl_unit",root);

		if(action_para)
			nvram_set("wl_unit", action_para);

		action_para = get_cgi_json("wl_subunit",root);

		if(action_para)
			nvram_set("wl_subunit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_wps_unit"))
	{
		action_para = get_cgi_json("wps_band",root);
		if(action_para)
			nvram_set("wps_band_x", action_para);
#if defined(RTCONFIG_WPSMULTIBAND)
		if ((action_para = get_cgi_json("wps_multiband",root)))
			nvram_set("wps_multiband", action_para);
#endif

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "wps_apply"))
	{
#ifdef RTCONFIG_LANTIQ
		wave_app_flag = wave_handle_app_flag(action_mode, wave_app_flag);
#endif
		action_para = get_cgi_json("wps_band",root);
		if(action_para)
			nvram_set("wps_band_x", action_para);
		else goto wps_finish;

		action_para = get_cgi_json("wps_enable",root);
		if(action_para) {
			nvram_set("wps_enable", action_para);
			nvram_set("wps_enable_x", action_para);
		}
		else goto wps_finish;

		action_para = get_cgi_json("wps_sta_pin",root);
		if(action_para)
			nvram_set("wps_sta_pin", action_para);
		else goto wps_finish;
#if defined(RTCONFIG_WPSMULTIBAND)
		if ((action_para = get_cgi_json("wps_multiband",root)))
			nvram_set("wps_multiband", action_para);
#endif

#ifdef RTCONFIG_WIFI_CLONE
		nvram_set("wps_enrollee", "0");
#endif

		notify_rc("start_wps_method");

wps_finish:
		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "wps_reset"))
	{
		action_para = get_cgi_json("wps_band",root);
		if(action_para)
			nvram_set("wps_band_x", action_para);
#if defined(RTCONFIG_WPSMULTIBAND)
		if ((action_para = get_cgi_json("wps_multiband",root)))
			nvram_set("wps_multiband", action_para);
#endif

		notify_rc("reset_wps");

		//websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_wan_unit"))
	{
		action_para = get_cgi_json("wan_unit", root);

		if(action_para)
			nvram_set("wan_unit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_dslx_transmode"))
	{
		action_para = get_cgi_json("dsltmp_transmode", root);

		if(action_para)
			nvram_set("dsltmp_transmode", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_lan_unit"))
	{
		action_para = get_cgi_json("lan_unit",root);

		if(action_para)
			nvram_set("lan_unit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "refresh_networkmap"))
	{
		if(pids("networkmap")) {
			nvram_set("refresh_networkmap", "1");

			doSystem("killall -%d networkmap", SIGUSR1);
#ifdef RTCONFIG_JFFS2USERICON
			notify_rc("start_lltdc");
#endif
#ifdef RTCONFIG_UPNPC
			notify_rc("start_miniupnpc");
#endif
			websRedirect(wp, current_url);
		}
	}
	else if (!strcmp(action_mode, "update_client_list"))
	{
		if(pids("networkmap")) {
			nvram_set("rescan_networkmap", "1");

			doSystem("killall -%d networkmap", SIGUSR1);

			websDone(wp, 200);
			//websRedirect(wp, current_url);
		}
	}
	else if (!strcmp(action_mode, "mfp_requeue")){
		unsigned int login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));

		if (login_ip == 0x100007f || login_ip == 0x0)
			nvram_set("mfp_ip_requeue", "");
		else{
			struct in_addr addr;

			addr.s_addr = login_ip;
			nvram_set("mfp_ip_requeue", inet_ntoa(addr));
		}

		int u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);

		write(u2ec_fifo, "q", 1);
		close(u2ec_fifo);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "mfp_monopolize")){
		unsigned int login_ip = (unsigned int)atoll(nvram_safe_get("login_ip"));

		if (login_ip==0x100007f || login_ip==0x0)
			nvram_set("mfp_ip_monopoly", "");
		else
		{
			struct in_addr addr;
			addr.s_addr=login_ip;
			nvram_set("mfp_ip_monopoly", inet_ntoa(addr));
		}
		int u2ec_fifo = open("/var/u2ec_fifo", O_WRONLY|O_NONBLOCK);
		write(u2ec_fifo, "m", 1);
		close(u2ec_fifo);

		websRedirect(wp, current_url);
	}
#ifdef ASUS_DDNS //2007.03.22 Yau add
	else if (!strcmp(action_mode, "ddnsclient"))
	{
		notify_rc("restart_ddns");

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "ddns_hostname_check"))
	{
		notify_rc("ddns_hostname_check");

		websRedirect(wp, current_url);
	}
#endif
#ifdef RTCONFIG_TRAFFIC_METER
	else if (!strcmp(action_mode, "reset_traffic_meter"))
	{
		printf("@@@ RESET Traffic Meter!!!\n");
		doSystem("killall -%d wanduck", SIGTSTP);
/*
		if (!validate_apply(wp)) {
			websWrite(wp, "NOT MODIFIED\n");
		}
		else {
			websWrite(wp, "MODIFIED\n");
		}
*/
		websRedirect(wp, current_url);
	}
#endif
//#ifdef RTCONFIG_CLOUDSYNC // get share link from lighttpd. Jerry5 added 2012.11.08
	else if (!strcmp(action_mode, "get_sharelink"))
	{
		FILE *fp;
		char buf[256];
		pid_t pid = 0;

		action_para = get_cgi_json("share_link_param", root);
		if(action_para){
			nvram_set("share_link_param", action_para);
			nvram_set("share_link_result", "");
		}

		action_para = get_cgi_json("share_link_host", root);
		if(action_para){
			nvram_set("share_link_host", action_para);
			nvram_commit();
		}

		if ((fp = fopen("/tmp/lighttpd/lighttpd.pid", "r")) != NULL) {
			if (fgets(buf, 256, fp) != NULL)
		   	pid = strtoul(buf, NULL, 0);
			fclose(fp);
			if (pid > 1 && kill(pid, SIGUSR2) == 0) {
				printf("[HTTPD] Signaling lighttpd OK!\n");
			}
			else{
				printf("[HTTPD] Signaling lighttpd FAIL!\n");
			}
		}
	}
//#endif
#ifdef RTCONFIG_OPENVPN
	else if (!strcmp(action_mode, "change_vpn_server_unit"))
	{
		action_para = get_cgi_json("vpn_server_unit", root);

		if(action_para)
			nvram_set("vpn_server_unit", action_para);

		action_para = websGetVar(wp, "VPNServer_mode", "");

		if(action_para)
			nvram_set("VPNServer_mode", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "change_vpn_client_unit"))
	{
		action_para = get_cgi_json("vpn_client_unit", root);

		if(action_para)
			nvram_set("vpn_client_unit", action_para);

		websRedirect(wp, current_url);
	}
	else if (!strcmp(action_mode, "refresh_vpn_ip"))
	{
		char tmp[20];

		snprintf(tmp, sizeof (tmp), "vpn_client%d_rip", nvram_get_int("vpn_client_unit"));
		nvram_unset(tmp);

		websDone(wp, 200);
	}
#endif
#ifdef  __CONFIG_NORTON__
	/* Trigger an NGA LiveUpdate */
	else if (!strcmp(action_mode, "NGAUpdate"))
		websWrite(wp, "Invoking LiveUpdate...");
		if (nga_update())
			websWrite(wp, "error<br>");
		else
			websWrite(wp, "done<br>");
	}
#endif /* __CONFIG_NORTON__ */
#ifdef RTCONFIG_USB_MODEM
	else if (!strcmp(action_mode, "restart_simauth"))
	{
		char act_node[32], act_port_path[8];

		snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			goto APPLY_FINISH;
		}

		snprintf(command, 128, "%s %d", action_mode, modem_unit);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "start_simpin"))
	{
		char act_node[32], act_port_path[8];
		char *pincode, *save_pin, *g3err_pin;
		int save_nvram = 0;

		pincode = get_cgi_json("sim_pincode", root);
		save_pin = get_cgi_json("save_pin", root);
		g3err_pin = get_cgi_json("g3err_pin", root);

		snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			goto APPLY_FINISH;
		}

		nvram_set("g3err_pin", g3err_pin);

		if(save_pin != NULL){
			if(!strcmp(save_pin, "1")){
				nvram_set("modem_pincode", pincode);
				save_nvram = 1;
			}
			else if(strcmp(nvram_safe_get("modem_pincode"),"") && !strcmp(save_pin, "0")){
				nvram_set("modem_pincode", "");
				save_nvram = 1;
			}
		}

		snprintf(command, 128, "%s %d %s", action_mode, modem_unit, pincode);
		notify_rc(command);

		if(save_nvram)
			nvram_commit();
	}
	else if (!strcmp(action_mode, "start_simpuk"))
	{
		char act_node[32], act_port_path[8];
		char *puk, *newpin, *g3err_pin;

		puk = get_cgi_json("sim_puk", root);
		newpin = get_cgi_json("sim_newpin", root);
		g3err_pin = get_cgi_json("g3err_pin", root);

		snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			goto APPLY_FINISH;
		}

		nvram_set("g3err_pin", g3err_pin);

		snprintf(command, 128, "%s %d %s %s", action_mode, modem_unit, puk, newpin);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "start_lockpin") || !strcmp(action_mode, "stop_lockpin"))
	{
		char act_node[32], act_port_path[8];
		char *pincode;

		pincode = get_cgi_json("sim_pincode", root);

		snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			goto APPLY_FINISH;
		}

		snprintf(command, 128, "%s %d %s", action_mode, modem_unit, pincode);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "start_pwdpin"))
	{
		char act_node[32], act_port_path[8];
		char *pincode, *newpin;

		pincode = get_cgi_json("sim_pincode", root);
		newpin = get_cgi_json("sim_newpin", root);

		snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			goto APPLY_FINISH;
		}

		snprintf(command, 128, "%s %d %s %s", action_mode, modem_unit, pincode, newpin);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "scan_isp"))
	{
		char act_node[32], act_port_path[8];

		snprintf(act_node, 32, "%s", nvram_safe_get(strcat_r(prefix2, "act_path", tmp2)));
		if(strlen(act_node) <= 0 || get_path_by_node(act_node, act_port_path, 8) == NULL){
			goto APPLY_FINISH;
		}

		snprintf(command, 128, "start_modemscan %d", modem_unit);
		notify_rc(command);
	}
	else if (!strcmp(action_mode, "start_simdetect"))
	{
		char *simdetect;

		simdetect = get_cgi_json("simdetect", root);
		snprintf(command, 128, "%s %s", action_mode, simdetect);
		notify_rc(command);
		websApply(wp, "Restarting.asp");
		nvram_set("freeze_duck", "15");
		shutdown(fileno(wp), SHUT_RDWR);
		sys_reboot();
	}
	else if(!strcmp(action_mode, "update_lte_fw")){
		notify_rc("start_gobi_update");
	}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	else if (!strcmp(action_mode, "restart_resetcount"))
	{
		notify_rc(action_mode);
	}
	else if (!strcmp(action_mode, "restart_sim_del"))
	{
		char *sim_order;

		sim_order = get_cgi_json("sim_order", root);

		snprintf(command, 128, "%s %s", action_mode, sim_order);
		notify_rc(command);
	}
#endif
#endif // RTCONFIG_USB_MODEM
#ifdef RTCONFIG_TRAFFIC_LIMITER
	else if (!strcmp(action_mode, "traffic_resetcount"))
	{
		char *ifname = get_cgi_json("interface", root);
		char ifmap[IFNAME_MAX];	// ifname after mapping
		char path[IFPATH_MAX];

		memset(ifmap, 0, sizeof(ifmap));
		ifname_mapping(ifname, ifmap);

		// write database
		doSystem("traffic_limiter -w");

		// delete file
		snprintf(path, sizeof(path), TL_PATH"%s/traffic.db", ifmap);
		doSystem("rm -f %s", path);

		// reset current traffic
		snprintf(path, sizeof(path), TL_PATH"%s/tmp", ifmap);
		f_write_string(path, "0", 0, 0);

		// clean tl_count / tl_alert / tl_limit
		snprintf(path, sizeof(path), TL_PATH"tl_count");
		f_write_string(path, "0", 0, 0);
		snprintf(path, sizeof(path), TL_PATH"tl_alert");
		f_write_string(path, "0", 0, 0);
		snprintf(path, sizeof(path), TL_PATH"tl_limit");
		f_write_string(path, "0", 0, 0);

		// update status for traffic limiter
		doSystem("traffic_limiter -q");

		// recover connection
		notify_rc("reset_traffic_limiter_force");
	}
#endif
#ifdef RTCONFIG_WTFAST
	else if (!strcmp(action_mode, "wtfast_logout")){
		char *wtf_rulelist = get_cgi_json("wtf_rulelist", root);

		nvram_set("wtf_rulelist", wtf_rulelist);
		nvram_set("wtf_username", "");
		nvram_set("wtf_passwd", "");
		nvram_commit();
		unlink("/tmp/wtf_status");
		notify_rc("stop_wtfast");
		_dprintf("httpd: wtfast_logout\n");
	}
	else if (!strcmp(action_mode, "wtfast_login")){
		char *wtf_username, *wtf_passwd;
		char *wtf_status;
		FILE *fp;

		wtf_username = get_cgi_json("wtf_username", root);
		wtf_passwd = get_cgi_json("wtf_passwd", root);
		wtf_status = get_cgi_json("wtf_status", root);

		nvram_set("wtf_username", wtf_username);
		nvram_set("wtf_passwd", wtf_passwd);
		if((fp = fopen("/tmp/wtf_status", "w")) != NULL) {
			fprintf(fp, "%s", wtf_status);
			fclose(fp);
		}

		notify_rc("start_wtfast");
		_dprintf("httpd: wtfast_login\n");
	}
#endif
#ifdef RTCONFIG_DISK_MONITOR
	else if (!strcmp(action_mode, "change_diskmon_unit"))
	{
		action_para = get_cgi_json("diskmon_usbport", root);

		if(action_para)
			nvram_set("diskmon_usbport", action_para);
	}
#endif	
#ifdef RTCONFIG_IPV6
	else if(!strcmp(action_mode, "change_ipv6_unit"))
	{
		action_para = get_cgi_json("ipv6_unit",root);

		if(action_para)
			nvram_set("ipv6_unit", action_para);
		websRedirect(wp, current_url);
	}
#endif
#ifdef RTCONFIG_NOTIFICATION_CENTER
	else if (!strcmp(action_mode, "nt_apply"))
	{
		struct list *event_list = NULL;
		char *nt_action =  safe_get_cgi_json("nt_action", root);
		char *nt_event =  safe_get_cgi_json("nt_event", root);
		char *tstamp =  safe_get_cgi_json("tstamp", root);
		char *nt_status =  safe_get_cgi_json("nt_status", root);
		int ret = -1;

		//_dprintf("nt_event = %s, tstamp = %s\n", nt_event, tstamp);

		/* initial */
		NOTIFY_DATABASE_T *input = initial_db_input();

		/* initial linked list */
		event_list = list_new();
		if(nt_action != NULL && (!strcmp(nt_action, "write") || !strcmp(nt_action, "delete"))){
			if(tstamp == NULL) input->tstamp = 0;
			else input->tstamp = atoi(tstamp);

			if(nt_event == NULL) input->event = 0;
			else input->event = strtol(nt_event, NULL, 16);

			if(nt_status == NULL) input->status = 0;
			else input->status = atoi(nt_status);

			ret = NT_DBAction(event_list, nt_action, input, NULL);

		}else if(nt_action != NULL && !strcmp(nt_action, "readall")){
			struct list *readall_event_list = NULL;

			/* initial */
			NOTIFY_DATABASE_T *readall_input = initial_db_input();

			/* initial readall_linked list */
			readall_event_list = list_new();

			/* database API */
			NT_DBAction(readall_event_list, "read", readall_input, "all");

			/* free input*/
			db_input_free(readall_input);

			/* print all linked list */
			NOTIFY_DATABASE_T *listevent;
			struct listnode *ln;

			LIST_LOOP(readall_event_list, listevent, ln)
			{
				if(listevent->status == 0){
					input->tstamp = listevent->tstamp;
					input->event = listevent->event;
					input->status = 1;
					ret = NT_DBAction(event_list, "write", input, NULL);
				}
			}
			/* free memory */
			NT_DBFree(readall_event_list);
		}else if(nt_action != NULL && !strcmp(nt_action, "readall")){

		}

		websWrite(wp, "{\"nt_result\":\"%s\"}",(ret==0)?"1":"0");

		/* free input*/
		db_input_free(input);

		/* free memory */
		NT_DBFree(event_list);
	}
#endif
#if defined(RTCONFIG_USB) && defined(RTCONFIG_PERMISSION_MANAGEMENT)
	else if(!strcmp(action_mode, "pms_apply"))
	{
		char para[1024];
		char *ptrArray[16]={0};
		char *pms_action = get_cgi_json("pms_action",root);
		char ascii_user[64], ascii_newuser[64];
		char ascii_passwd[64];
		char asciigroup[64], ascii_newgroup[64];

		memset(para, 0, sizeof (para));

		if(pms_action != NULL && !strncmp(pms_action, "account_create", 14)){

			not_ej_initial_folder_var_file();

			ptrArray[0] = get_cgi_json("pms_acc_active",root);
			ptrArray[1] = get_cgi_json("pms_acc_name",root);
			ptrArray[2] = get_cgi_json("pms_acc_passwd",root);
			ptrArray[3] = get_cgi_json("pms_acc_desc",root);
			ptrArray[4] = get_cgi_json("pms_acc_email",root);
			ptrArray[5] = get_cgi_json("pms_accgroup",root);	//accgroup1>accgroup2>
			ptrArray[6] = get_cgi_json("pms_accgroup_num",root);

			if (strlen(ptrArray[1]) <= 0 || strlen(ptrArray[2]) <= 0){
				goto APPLY_FINISH;
			}

			if (add_account(ptrArray[1], ptrArray[2]) < 0){
				goto APPLY_FINISH;
			}

			memset(ascii_user, 0, 64);
			char_to_ascii_safe(ascii_user, ptrArray[1], 64);
			memset(ascii_passwd, 0, 64);
			char_to_ascii_safe(ascii_passwd, ptrArray[2], 64);

			// setup account from
			PMS_ACCOUNT_INFO_T *tmp1 = NULL;
			if ((tmp1 = PMS_list_Account_new(5, ptrArray[0], ascii_user, ascii_passwd, ptrArray[3], ptrArray[4])) == NULL)
			{
				_dprintf("memory allocate failed\n");
			}else{
				//_dprintf("%s : %d>%s>%s>%s>%s\n", __FUNCTION__, tmp1->active, tmp1->name, tmp1->passwd, tmp1->desc, tmp1->email);
				PMS_ActionAccountInfo(PMS_ACTION_UPDATE, (void *)tmp1, 0);
			}
			PMS_list_ACCOUNT_free(tmp1);

			if(ptrArray[6] != NULL){
				// setup account / group matching table
				snprintf(para, sizeof(para), "%s>%s", ascii_user, ptrArray[5]);	// account>accgroup1>accgroup2>...
				//_dprintf("para = %s\n");
				PMS_ActAccMatchInfo(PMS_ACTION_UPDATE, atoi(ptrArray[6]), para);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "account_modify", 14)){
			char *org_account = get_cgi_json("org_account",root);
			ptrArray[0] = get_cgi_json("pms_acc_active",root);
			ptrArray[1] = get_cgi_json("pms_acc_name",root);
			ptrArray[2] = get_cgi_json("pms_acc_passwd",root);
			ptrArray[3] = get_cgi_json("pms_acc_desc",root);
			ptrArray[4] = get_cgi_json("pms_acc_email",root);
			ptrArray[5] = get_cgi_json("pms_accgroup",root);	//accgroup1>accgroup2>
			ptrArray[6] = get_cgi_json("pms_accgroup_num",root);

			if (strlen(org_account) <= 0 || strlen(ptrArray[1]) <= 0 || strlen(ptrArray[2]) <= 0){
				goto APPLY_FINISH;
			}

			if(ptrArray[1] != NULL && strlen(ptrArray[1]) > 0){
				memset(ascii_newuser, 0, 64);
				char_to_ascii_safe(ascii_newuser, ptrArray[1], 64);
			}

			memset(ascii_user, 0, 64);
			char_to_ascii_safe(ascii_user, org_account, 64);
			memset(ascii_passwd, 0, 64);
			char_to_ascii_safe(ascii_passwd, ptrArray[2], 64);

			if(strcmp(org_account,ptrArray[1]) != 0){
				if (mod_account(org_account, ptrArray[1], ptrArray[2]) < 0){
					goto APPLY_FINISH;
				}
			}
			// setup account from
			PMS_ACCOUNT_INFO_T *tmp1 = NULL;
			if ((tmp1 = PMS_list_Account_new(5, ptrArray[0], ascii_newuser, ascii_passwd, ptrArray[3], ptrArray[4])) == NULL)
			{
				_dprintf("memory allocate failed\n");
			}else{
				//_dprintf("%s : %d>%s>%s>%s>%s\n", __FUNCTION__, tmp1->active, tmp1->name, tmp1->passwd, tmp1->desc, tmp1->email);
				PMS_ActionAccountInfo(PMS_ACTION_MODIFY, (void *)tmp1, 0, ascii_user);
			}
			PMS_list_ACCOUNT_free(tmp1);

			if(ptrArray[6] != NULL){
				// setup account / group matching table
				snprintf(para, sizeof(para), "%s>%s", ascii_newuser, ptrArray[5]);	// account>accgroup1>accgroup2>...
				//_dprintf("para = %s\n");
				PMS_ActAccMatchInfo(PMS_ACTION_UPDATE, atoi(ptrArray[6]), para);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "account_delete", 14)){

			ptrArray[1] = get_cgi_json("pms_acc_name",root);

			not_ej_initial_folder_var_file();

			if (del_account(ptrArray[1]) < 0){
				goto APPLY_FINISH;
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "accgroup_create", 15)){

			not_ej_initial_folder_var_file();

			ptrArray[0] = get_cgi_json("pms_accgrp_active",root);
			ptrArray[1] = get_cgi_json("pms_accgrp_name",root);
			ptrArray[2] = get_cgi_json("pms_accgrp_desc",root);
			ptrArray[3] = get_cgi_json("pms_grpacc",root);	//acc1>acc2>...
			ptrArray[4] = get_cgi_json("pms_grpacc_num",root);

			if (strlen(ptrArray[1]) <= 0){
				goto APPLY_FINISH;
			}

			if (add_group(ptrArray[1]) < 0){
				goto APPLY_FINISH;
			}

			memset(asciigroup, 0, 64);
			char_to_ascii_safe(asciigroup, ptrArray[1], 64);

			// setup account group
			PMS_ACCOUNT_GROUP_INFO_T *tmp2 = NULL;
			if ((tmp2 = PMS_list_AccountGroup_new(3, ptrArray[0], asciigroup, ptrArray[2])) != NULL)
			{
				PMS_ActionAccountInfo(PMS_ACTION_UPDATE, (void *)tmp2, 1);
				PMS_list_ACCOUNT_GROUP_free(tmp2);
			}

			if(ptrArray[3] != NULL){
				// setup account / group matching table
				snprintf(para, sizeof(para), "%s>%s", asciigroup, ptrArray[3]);	// group>acc1>acc2>...
				//_dprintf("para = %s\n");
				PMS_ActAccGroupMatchInfo(PMS_ACTION_UPDATE, atoi(ptrArray[4]), para);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "accgroup_modify", 15)){

			char *org_accgrp_name = get_cgi_json("org_accgrp_name",root);
			ptrArray[0] = get_cgi_json("pms_accgrp_active",root);
			ptrArray[1] = get_cgi_json("pms_accgrp_name",root);
			ptrArray[2] = get_cgi_json("pms_accgrp_desc",root);
			ptrArray[3] = get_cgi_json("pms_grpacc",root);	//acc1>acc2>...
			ptrArray[4] = get_cgi_json("pms_grpacc_num",root);

			if (strlen(org_accgrp_name) <= 0 || strlen(ptrArray[1]) <= 0){
				goto APPLY_FINISH;
			}

			memset(asciigroup, 0, 64);
			char_to_ascii_safe(asciigroup, org_accgrp_name, 64);

			if(strncmp(org_accgrp_name, ptrArray[1], strlen(ptrArray[1])) != 0){
				if (mod_group(org_accgrp_name, ptrArray[1]) < 0){
					goto APPLY_FINISH;
				}
			}

			if(ptrArray[1] != NULL && strlen(ptrArray[1]) > 0){
				memset(ascii_newgroup, 0, 64);
				char_to_ascii_safe(ascii_newgroup, ptrArray[1], 64);
			}

			// setup account group
			PMS_ACCOUNT_GROUP_INFO_T *tmp2 = NULL;
			if ((tmp2 = PMS_list_AccountGroup_new(3, ptrArray[0], ascii_newgroup, ptrArray[2])) != NULL)
			{
				PMS_ActionAccountInfo(PMS_ACTION_MODIFY, (void *)tmp2, 1, asciigroup);
				PMS_list_ACCOUNT_GROUP_free(tmp2);
			}

			if(ptrArray[3] != NULL){
				// setup account / group matching table
				snprintf(para, sizeof(para), "%s>%s", ascii_newgroup, ptrArray[3]);	// group>acc1>acc2>...
				//_dprintf("para = %s\n");
				PMS_ActAccGroupMatchInfo(PMS_ACTION_UPDATE, atoi(ptrArray[4]), para);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "accgroup_delete", 15)){

			ptrArray[1] = get_cgi_json("pms_accgrp_name",root);

			//delete account
			not_ej_initial_folder_var_file();

			if (del_group(ptrArray[1]) < 0){
				goto APPLY_FINISH;
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "device_update", 13)){

			ptrArray[0] = get_cgi_json("pms_dev_active",root);
			ptrArray[1] = get_cgi_json("pms_dev_mac",root);
			ptrArray[2] = get_cgi_json("pms_dev_desc",root);
			ptrArray[3] = get_cgi_json("pms_dev_devname",root);
			ptrArray[4] = get_cgi_json("pms_dev_devtype",root);
			ptrArray[5] = get_cgi_json("pms_devgroup",root);	//devgroup1>devgroup2>
			ptrArray[6] = get_cgi_json("pms_devgroup_num",root);

			// setup account from
			PMS_DEVICE_INFO_T *tmp1 = NULL;
			if ((tmp1 = PMS_list_Device_new(5, ptrArray[0], ptrArray[1], ptrArray[2], ptrArray[3], ptrArray[4])) != NULL)
			{
				//_dprintf("%s : %d>%s>%s>%d>%s\n", __FUNCTION__, tmp1->active, tmp1->mac, tmp1->devname, tmp1->devtype, tmp1->desc);
				PMS_ActionDeviceInfo(PMS_ACTION_UPDATE, (void *)tmp1, 0);
				PMS_list_DEVICE_free(tmp1);
			}
			if(ptrArray[5] != NULL){
				// setup account / group matching table
				snprintf(para, sizeof(para), "%s>%s", ptrArray[1], ptrArray[5]);	// mac>devgroup1>devgroup2>...
				//_dprintf("para = %s\n");
				PMS_ActDevMatchInfo(PMS_ACTION_UPDATE, atoi(ptrArray[6]), para);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "device_delete", 13)){

			ptrArray[1] = get_cgi_json("pms_dev_mac",root);

			//delete account
			PMS_DEVICE_INFO_T *tmp1 = NULL;
			if ((tmp1 = PMS_list_Device_new(2, "", ptrArray[1])) != NULL)
			{
				_dprintf("account_delete %s, tmp1->name = %s\n", ptrArray[1], tmp1->mac);
				PMS_ActionDeviceInfo(PMS_ACTION_DELETE, (void *)tmp1, 0);
				PMS_list_DEVICE_free(tmp1);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "devgroup_update", 12)){

			ptrArray[0] = get_cgi_json("pms_devgrp_active",root);
			ptrArray[1] = get_cgi_json("pms_devgrp_name",root);
			ptrArray[2] = get_cgi_json("pms_grp_desc",root);
			ptrArray[3] = get_cgi_json("pms_owned_device",root);	//owned_device1>owned_device2>...
			ptrArray[4] = get_cgi_json("pms_owned_device_num",root);

			// setup account group
			PMS_DEVICE_GROUP_INFO_T *tmp2 = NULL;
			if ((tmp2 = PMS_list_DeviceGroup_new(3, ptrArray[0], ptrArray[1], ptrArray[2])) != NULL)
			{
				PMS_ActionDeviceInfo(PMS_ACTION_UPDATE, (void *)tmp2, 1);
				PMS_list_DEVICE_GROUP_free(tmp2);
			}

			if(ptrArray[3] != NULL){
				// setup account / group matching table
				snprintf(para, sizeof(para), "%s>%s", ptrArray[1], ptrArray[3]);// name>owned_device1>owned_device2>...
				//_dprintf("para = %s\n");
				PMS_ActDevGroupMatchInfo(PMS_ACTION_UPDATE, atoi(ptrArray[4]), para);
			}
		}
		else if(pms_action != NULL && !strncmp(pms_action, "devgroup_delete", 12)){

			ptrArray[1] = get_cgi_json("pms_devgrp_name",root);

			//delete account
			PMS_DEVICE_GROUP_INFO_T *tmp2 = NULL;
			if ((tmp2 = PMS_list_DeviceGroup_new(2, "", ptrArray[1])) != NULL)
			{
				//_dprintf("devgroup_delete %s, tmp2->name = %s\n", ptrArray[1], tmp2->name);
				PMS_ActionDeviceInfo(PMS_ACTION_DELETE, (void *)tmp2, 1);
				PMS_list_DEVICE_GROUP_free(tmp2);
			}
		}
	}
#endif
#ifdef RTCONFIG_CFGSYNC
        else if (!strcmp(action_mode, "firmware_upgrade") ||
		!strcmp(action_mode, "firmware_check")) {
		char event_msg[64] = {0};

		if (!strcmp(action_mode, "firmware_check"))
			snprintf(event_msg, sizeof(event_msg), HTTPD_GENERIC_MSG, EID_HTTPD_FW_CHECK);
		else if (!strcmp(action_mode, "firmware_upgrade"))
			snprintf(event_msg, sizeof(event_msg), HTTPD_GENERIC_MSG, EID_HTTPD_FW_UPGRADE);

		if (strlen(event_msg))
			send_cfgmnt_event(event_msg);
	}
	else if (!strcmp(action_mode, "remove_slave") ||
		!strcmp(action_mode, "reset_default")) {
		char event_msg[64] = {0};
		char *slave_mac = get_cgi_json("slave_mac", root);

		if (!strcmp(action_mode, "remove_slave"))
			snprintf(event_msg, sizeof(event_msg), HTTPD_SLAVE_MSG,
				EID_HTTPD_REMOVE_SLAVE, slave_mac);
		else if (!strcmp(action_mode, "reset_default"))
			snprintf(event_msg, sizeof(event_msg), HTTPD_SLAVE_MSG,
				EID_HTTPD_RESET_DEFAULT, slave_mac);

		if (strlen(event_msg))
			send_cfgmnt_event(event_msg);
	}
	else if (!strcmp(action_mode, "start_wps_registrar")) {
		char event_msg[64] = {0};
		struct in_addr login_ip_addr;
		login_ip_addr.s_addr = login_ip_tmp;

		snprintf(event_msg, sizeof(event_msg), HTTPD_IP_MSG,
			EID_HTTPD_START_WPS, inet_ntoa(login_ip_addr));

		if (strlen(event_msg))
			send_cfgmnt_event(event_msg);
	}
	else if (!strcmp(action_mode, "onboarding")) {
		char event_msg[128] = {0};
		char *re_mac = get_cgi_json("re_mac", root);
		char *new_re_mac = get_cgi_json("new_re_mac", root);

		if (!re_mac && !new_re_mac)
			snprintf(event_msg, sizeof(event_msg), HTTPD_OB_AVAILABLE_MSG,
				EID_HTTPD_ONBOARDING, OB_TYPE_AVAILABLE);
		else if (re_mac && new_re_mac)
			snprintf(event_msg, sizeof(event_msg), HTTPD_OB_LOCK_MSG,
				EID_HTTPD_ONBOARDING, OB_TYPE_LOCKED, re_mac, new_re_mac);

		if (strlen(event_msg))
			send_cfgmnt_event(event_msg);
	}
        else if (!strcmp(action_mode, "ob_selection")) {
		char event_msg[128] = {0};
		char *new_re_mac = get_cgi_json("new_re_mac", root);
		char *ob_path = get_cgi_json("ob_path", root);

		snprintf(event_msg, sizeof(event_msg), HTTPD_OB_SELECTION_MSG,
			EID_HTTPD_ONBOARDING, OB_TYPE_AVAILABLE, new_re_mac ? : "",
			ob_path ? atoi(ob_path) : 0);

		if (strlen(event_msg))
			send_cfgmnt_event(event_msg);
        }
	else if (!strcmp(action_mode, "config_changed")) {
		char event_msg[4096] = {0};
		char *re_mac = get_cgi_json("re_mac", root);
		char *config = get_cgi_json("config", root);
		if (re_mac && config) {
			snprintf(event_msg, sizeof(event_msg), HTTPD_CONFIG_CHANGED_MSG,
				EID_HTTPD_CONFIG_CHANGED, re_mac, config);
			send_cfgmnt_event(event_msg);
		}
		else
			_dprintf("re_mac or config invalid");
	}
	else if (!strcmp(action_mode, "release_note")) {
		char *model = get_cgi_json("model", root);
		char *version = get_cgi_json("version", root);

		if (model && version) {
			snprintf(command, sizeof(command), "start_release_note %s %s", model, version);
			_dprintf("command(%s)", command);
			notify_rc(command);
		}
	}
#endif
	goto APPLY_FINISH;

APPLY_FINISH:
	if(root != NULL)
		json_object_put(root);
	return 1;
}



static void
do_auth(char *userid, char *passwd, char *realm)
{
//	time_t tm;

	if (strcmp(ProductID,"")==0)
	{
		strcpy(ProductID, get_productid());
	}
	if (strcmp(UserID,"")==0 || reget_passwd == 1)
	{
	   	strcpy(UserID, nvram_safe_get("http_username"));
	}
// 2008.08 magic {
	if (strcmp(UserPass, "") == 0 || reget_passwd == 1)
	{
// 2008.08 magic }
		strcpy(UserPass, nvram_safe_get("http_passwd"));
	}

	reget_passwd = 0;

	strlcpy(userid, UserID, AUTH_MAX);

	if (!is_auth())
	{
		strcpy(passwd, "");
	}
	else
	{
		strlcpy(passwd, UserPass, AUTH_MAX);
	}
	strlcpy(realm, ProductID, AUTH_MAX);
}

//andi
static void
do_ftpServerTree_cgi(char *url, FILE *stream)
{
    ftpServerTree_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}

static void
do_apply_cgi(char *url, FILE *stream)
{
    apply_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}

/* Look for unquoted character within a string */
char *
unqstrstr_t(char *haystack, char *needle)
{
	char *cur;
	int q;

	for (cur = haystack, q = 0;
	     cur < &haystack[strlen(haystack)] && !(!q && !strncmp(needle, cur, strlen(needle)));
	     cur++) {
		if (*cur == '"')
			q ? q-- : q++;
	}
	return (cur < &haystack[strlen(haystack)]) ? cur : NULL;
}

char *
get_arg_t(char *args, char **next)
{
	char *arg, *end;

	/* Parse out arg, ... */
	if (!(end = unqstrstr_t(args, ","))) {
		end = args + strlen(args);
		*next = NULL;
	} else
		*next = end + 1;

	/* Skip whitespace and quotation marks on either end of arg */
	for (arg = args; isspace((int)*arg) || *arg == '"'; arg++);
	for (*end-- = '\0'; isspace((int)*end) || *end == '"'; end--)
		*end = '\0';

	return arg;
}

#ifdef TRANSLATE_ON_FLY
static int refresh_title_asp = 0;

static void
do_lang_cgi(char *url, FILE *stream)
{
	if (refresh_title_asp)  {
		// Request refreshing pages from browser.
		websHeader(stream);
		websWrite(stream, "<head></head><title>REDIRECT TO INDEX.ASP</title>");

		// The text between <body> and </body> content may be rendered in Opera browser.
		websWrite(stream, "<body onLoad='if (navigator.appVersion.indexOf(\"Firefox\")!=-1||navigator.appName == \"Netscape\"){top.location=%s;}else{top.location.reload(true);}'></body>", indexpage);
		websFooter(stream);
		websDone(stream, 200);
	} else {
		// Send redirect-page if and only if refresh_title_asp is true.
		// If we do not send Title.asp, Firefox reload web-pages again and again.
		// This trick had been deprecated due to compatibility issue with Netscape and Mozilla browser.
		websRedirect(stream, "Title.asp");
	}
}

static void
do_change_location_cgi(char *url, FILE *stream)
{
	int ret = 0;
	char *lang = NULL;
	struct json_object *root=NULL;

	do_json_decode(&root);
	lang = safe_get_cgi_json("lang", root);

#ifdef RTCONFIG_TCODE
	ret = change_location(lang);
#endif
	websWrite(stream, "{\"statusCode\":\"%d\"}", (ret)?200:400);
	json_object_put(root);
}

/*doesn't be used any more*/
static void
do_lang_post(char *url, FILE *stream, int len, char *boundary)
{
	int c;
	char *p, *p1;
	char orig_lang[4], new_lang[4];

	if (url == NULL)
		return;

	p = strstr (url, "preferred_lang_menu");
	if (p == NULL)
		return;
	memset (new_lang, 0, sizeof (new_lang));
	strncpy (new_lang, p + strlen ("preferred_lang_menu") + 1, 2);

	memset (orig_lang, 0, sizeof (orig_lang));
	p1 = nvram_safe_get_x ("", "preferred_lang");
	if (p1[0] != '\0') {
		strncpy (orig_lang, p1, 2);
	} else {
		strncpy (orig_lang, "EN", 2);
	}

	// read remain data
#if 0
	if (feof (stream)) {
		while ((c = fgetc(stream) != EOF)) {
			;	// fall through
		}
	}
#else
	char buf[1024];
	while ((c = fread(buf, 1, 1024, stream)) > 0)
		;		// fall through
#endif

	cprintf ("lang: %s --> %s\n", orig_lang, new_lang);
	refresh_title_asp = 0;
	if (strcmp (orig_lang, new_lang) != 0 || is_firsttime ()) {
		// If language setting is different or first change language
		nvram_set_x ("", "preferred_lang", new_lang);
		if (is_firsttime ()){
			cprintf ("set x_Setting --> 1\n");
			nvram_set("x_Setting", "1");
		}
		cprintf ("!!!!!!!!!Commit new language settings.\n");
		refresh_title_asp = 1;
		nvram_commit();
	}
}
#endif // TRANSLATE_ON_FLY

#define SWAP_LONG(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

int upgrade_err;
int stop_upgrade_once = 0;

#ifdef RTAC68A
static void
do_upgrade_post(char *url, FILE *stream, int len, char *boundary)
{
	upgrade_err = 1;
}
#else
static void
do_upgrade_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64

	do_html_get(url, len, boundary);
	char *autoreboot = safe_get_cgi_json("autoreboot",NULL);
	char *reset = safe_get_cgi_json("reset",NULL);

	char upload_fifo[64] = "/tmp/linux.trx";
	FILE *fifo = NULL;
	char buf[4096];
	int ch/*, ver_chk = 0*/;
	int count, cnt;
	long filelen;
	int offset;
#ifndef RTCONFIG_SMALL_FW_UPDATE
	struct sysinfo si;
#endif
	upgrade_err=1;
	/* workaround to RAM disk space issue */
	stop_upgrade_once = 0;
	nvram_set_int("upgrade_fw_status", FW_INIT);
	f_write_string("/tmp/detect_wrong.log", "", 0, 0);
	f_write_string("/tmp/usb.log", "", 0, 0);
#ifdef RTCONFIG_SMALL_FW_UPDATE
	eval("/sbin/ejusb", "-1", "0");
	upgrade_rc("stop", autoreboot, reset, 0);
	stop_upgrade_once = 1;
	sleep(10);
	/* Mount 16M ram disk to avoid out of memory */
	system("mkdir /tmp/mytmpfs");
	system("mount -t tmpfs -o size=16M,nr_inodes=10k,mode=700 tmpfs /tmp/mytmpfs");
	snprintf(upload_fifo, sizeof(upload_fifo), "/tmp/mytmpfs/linux.trx");
#endif

#if defined(RTCONFIG_LANTIQ) && defined(RTCONFIG_BWDPI)
	/* special case : free memory of dpi engine for INTEL */
	_dprintf("httpd: stop dpi engine to release memory\n");
	stop_dpi_engine_service(1);
#endif

	/* Look for our part */
	while (len > 0)
	{
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream))
		{
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20) && strstr(buf, "name=\"file\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream))
		{
			goto err;
		}
		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n"))
		{
			break;
		}
	}

#define BYTE_TO_KB(b) ((b >> 10) + ((b & 0x2ff)?1:0))
	free_caches(FREE_MEM_PAGE, 5, BYTE_TO_KB(len));

	if (!(fifo = fopen(upload_fifo, "a+"))) goto err;

#ifndef RTCONFIG_SMALL_FW_UPDATE
	sysinfo(&si);
	/* free memory should be 4 * TRX_size */
	if ((si.freeram * si.mem_unit)/4 < len)
	{
		eval("/sbin/ejusb", "-1", "0");
		upgrade_rc("stop", autoreboot, reset, 0);
		stop_upgrade_once = 1;
	}
#endif
#ifdef HND_ROUTER
	int boundary_len = ((boundary != NULL) ? strlen(boundary) : 0);
	int ex_len = 2 + 2 + boundary_len + 4;

	len = len - ex_len;

#if 0
	if(!nvram_match("fakelive", "1")) {
        	if ((count = safe_fwrite(&len, 1, sizeof(len), fifo)) != sizeof(len)) {
                	_dprintf("*** Failed to write %d bytes. Written bytes=%d\n",
                        	sizeof(len), count);
                	goto err;
        	}
		nvram_set("uup", "1");
	}
#endif
	_dprintf("\nfile len is %d\n", len);
#endif
	filelen = len;
	cnt = 0;
	offset = 0;

	/* Pipe the rest to the FIFO */
	while (len>0 && filelen>0)
	{

#ifdef RTCONFIG_HTTPS
		if(do_ssl){
			if (waitfor(ssl_stream_fd, (len >= 0x4000)? 3 : 1) <= 0)
				break;
		}
		else{
			if (waitfor (fileno(stream), 10) <= 0)
			{
				break;
			}
		}
#else
		if (waitfor (fileno(stream), 10) <= 0)
		{
			break;
		}
#endif

		count = fread(buf + offset, 1, MIN(len, sizeof(buf)-offset), stream);

		if(count <= 0)
			goto err;

		len -= count;

		if(cnt==0) {
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK)
#define HEADER_LEN (64)
#else
#define HEADER_LEN (8)
#endif
			if(count + offset < HEADER_LEN)
			{
				offset += count;
				continue;
			}

			count += offset;
			offset = 0;
			_dprintf("read from stream: %d\n", count);
			cnt++;

			if(!check_imageheader(buf, &filelen)) {
				goto err;
			}
		}
		filelen-=count;
		fwrite(buf, 1, count, fifo);
	}

#ifdef HND_ROUTER
	len += ex_len;
#endif

	/* Slurp anything remaining in the request */
	while (len-- > 0)
	{
		if((ch = fgetc(stream)) == EOF)
			break;

		if (filelen>0)
		{
			fputc(ch, fifo);
			filelen--;
		}
	}
	fclose(fifo);
	fifo = NULL;

#ifdef RTCONFIG_DSL
	int ret_val_sep;
	ret_val_sep = separate_tc_fw_from_trx(upload_fifo);
	// should router update tc fw?
	if (ret_val_sep)
	{
		if(check_tc_firmware_crc()) /* return 0 when pass */
			goto err;
#ifndef RTCONFIG_RALINK
		nvram_set_int("reboot_time", nvram_get_int("reboot_time")+100);
#endif
	}
#endif

#ifdef CONFIG_BCMWL5
#if !defined(RTAC3200) && !defined(RTAC87U) && !defined(RTAC56U)	// kludge
	if (fw_check() != 0)
		goto err;
#endif
#endif

	upgrade_err = check_imagefile(upload_fifo);

	if (upgrade_err) /* 0: legal image, 1: illegal image 2: new trx format validation failure */
		goto err;

err:
	nvram_set_int("upgrade_fw_status", FW_UPLOADING_ERROR);
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;
}
#endif

static void
do_upgrade_cgi(char *url, FILE *stream)
{
	/* Reboot if successful */
	char *autoreboot = safe_get_cgi_json("autoreboot",NULL);
	char *reset = safe_get_cgi_json("reset",NULL);

	if (upgrade_err == 0)
	{
#if defined(RTCONFIG_DSL) && defined(RTCONFIG_RALINK)
		int ret_val_comp;

		do_upgrade_adsldrv();
		ret_val_comp = compare_linux_image();
		printf("compare_linux_image ret=%d\n",ret_val_comp);
		if (ret_val_comp == 0)
		{
			// same trx
			unlink("/tmp/linux.trx");
		}
		else
		{
			// different trx
			// it will call rc_service automatically for firmware upgrading
		}
#endif
#ifndef RTCONFIG_SMALL_FW_UPDATE
		if (!stop_upgrade_once){
			eval("/sbin/ejusb", "-1", "0");
			upgrade_rc("stop", autoreboot, reset, 0);
			stop_upgrade_once = 1;
		}
#endif
		int etry = 3, err = 0;

#if (defined(PLN12) || defined(PLAC56))
		set_wifiled(6);
#endif
		websApply(stream, "Updating.asp");
		shutdown(fileno(stream), SHUT_RDWR);
		while(etry-- && (err = upgrade_rc("start", autoreboot, reset, 60)))
		{
			printf("%s, try agn upgrade...%d/3, err=%d\n", __FUNCTION__, etry, err);
			upgrade_rc("stop", autoreboot, reset, 10);
			stop_upgrade_once = 1;
		}
	}
	else
	{
		if(upgrade_err == 2)	/* 2: new trx format validation failure */
			nvram_set_int("upgrade_fw_status", FW_TRX_CHECK_ERROR);
		else	/* 1: illegal image */
			nvram_set_int("upgrade_fw_status", FW_WRITING_ERROR);
		unlink("/tmp/linux.trx");

		if (stop_upgrade_once) {
			websApply(stream, "UpdateError_reboot.asp");
			sys_reboot();
#if 0
#ifdef CONFIG_BCMWL5
		} else if (upgrade_err == 2) {
			websApply(stream, "UpdateError2.asp");
#endif
#endif
		} else {
			websApply(stream, "UpdateError.asp");
#if defined(RTCONFIG_LANTIQ) && defined(RTCONFIG_BWDPI) 
			/* special case : if upgrade fail, recover dpi engine for INTEL */
			_dprintf("httpd: start dpi engine because incorrect firmware\n");
			start_dpi_engine_service();
#endif
		}
	}
}

#if defined(RTCONFIG_SAVEJFFS)
/**
 * Get jffs cfgs from socket, that is used to upload setting file to DUT,
 * and write it to JFFS_CFGS in original form.
 * Jffs cfgs may not exist in setting file.
 * @stream:	socket that is provided by do_upload_post() in httpd
 * @len:	pointer to data length in socket.
 * @return:
 * 	0:	success
 *      -1:	invalid parameter
 *      -2:	can't open JFFS_CFGS for writing
 *      -3:	can't read header or header not found
 *      -4:	can't read all jffs cfgs from socket.
 */
static int get_jffs_cfgs(FILE *stream, int *len)
{
	uint8_t buf[1024];
	int ret = 0, ch, count, offset, flag, cmpHeader;
	FILE *fp_jcfg;
	long filelen, *filelenptr;

	if (!stream || !len || *len <= 0)
		return -1;

	fp_jcfg = fopen(JFFS_CFGS, "w");
	if (fp_jcfg == NULL)
		return -2;

	filelen = *len;
	flag = offset = 0;
	while (*len > 0 && filelen > 0) {
#ifdef RTCONFIG_HTTPS
		if (do_ssl) {
			if (waitfor(ssl_stream_fd, (*len >= 0x4000)? 3 : 1) <= 0)
				break;
		} else {
			if (waitfor(fileno(stream), 10) <= 0)
				break;
		}
#else
		if (waitfor(fileno(stream), 10) <= 0) {
			break;
		}
#endif
		count = fread(buf + offset, 1, MIN(filelen, sizeof(buf) - offset), stream);
		if (count <= 0)
			goto err;

		*len -= count;
		if (flag == 0) {
			if (count + offset < 8) {
				offset += count;
				continue;
			}
			count += offset;
			offset = 0;

			if (!strncmp(buf, JFFS_CFGS_HDR, 4)) {
				filelenptr = (long*)(buf + 4);
				filelen = le32_to_cpu(*filelenptr) + 8;	/* length is saved in little-endian. */
			} else {
				logmessage("savejffs", "Can't find header of jffs cfgs. (%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X)",
					buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
				ret = -3;
				goto err;
			}
			cmpHeader = 1;
			++flag;
		}

		filelen -= count;
		fwrite(buf, 1, count, fp_jcfg);
	}

	if (!cmpHeader) {
		ret = -3;
		goto err;
	}

	/* Slurp remaining jffs cfgs in the request */
	while (filelen > 0 && *len > 0) {
		if ((ch = fgetc(stream)) == EOF)
			break;

		(*len)--;
		fputc(ch, fp_jcfg);
		filelen--;
	}

	if (filelen > 0) {
		ret = -4;
		goto err;
	}

	fclose(fp_jcfg);

	/* Don't touch rest data in socket.  Let caller to slurp it. */

	return 0;

err:
	if (fp_jcfg)
		fclose(fp_jcfg);

	if (f_exists(JFFS_CFGS))
		unlink(JFFS_CFGS);

	return ret;
}
#endif	/* RTCONFIG_SAVEJFFS */

static void
do_upload_post(char *url, FILE *stream, int len, char *boundary)
{
	#define MAX_VERSION_LEN 64
	char upload_fifo[] = "/tmp/settings_u.prf";
	FILE *fifo = NULL;
	char buf[1024];
	int count, ret = EINVAL, ch;
	int /*eno, */cnt;
	uint32_t *filelenptr;
	long filelen;
	char /*version[MAX_VERSION_LEN], */cmpHeader;
	int offset;
#if defined(RTCONFIG_SAVEJFFS)
	int r;
#endif

	/* Look for our part */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20)
				&& strstr(buf, "name=\"file\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "w")))
		goto err;

	filelen = len;
	cnt = 0;
	offset = 0;

	/* Pipe the rest to the FIFO */
	cprintf("Upgrading %d\n", len);
	cmpHeader = 0;

	while (len > 0 && filelen > 0) {
#ifdef RTCONFIG_HTTPS
		if(do_ssl){
			if (waitfor(ssl_stream_fd, (len >= 0x4000)? 3 : 1) <= 0)
				break;
		}
		else{
			if (waitfor (fileno(stream), 10) <= 0)
			{
				break;
			}
		}
#else
		if (waitfor (fileno(stream), 10) <= 0)
		{
			break;
		}
#endif
		count = fread(buf + offset, 1, MIN(filelen, sizeof(buf)-offset), stream);
		if(count <= 0)
			goto err;

		len -= count;

		if (cnt == 0)
		{
			if(count + offset < 8)
			{
				offset += count;
				continue;
			}
			count += offset;
			offset = 0;

			if (!strncmp(buf, PROFILE_HEADER, 4))
			{
				filelenptr = (uint32_t*)(buf + 4);
				filelen = *filelenptr;

			}
			else if (!strncmp(buf, PROFILE_HEADER_NEW, 4))
			{
				filelenptr = (uint32_t*)(buf + 4);
#if defined(RTCONFIG_SAVEJFFS)
				filelen = le32_to_cpu(*filelenptr) & 0xffffff;
#else
				filelen = *filelenptr;
				filelen = filelen & 0xffffff;
#endif
				/* header length, 8, is not included in length field.*/
				filelen += 8;
			}
			else
			{
				_dprintf("\nupload: wrong header !\n");
				logmessage("httpd", "uplaod failed due wrong CFG file");
				goto err;
			}

			cmpHeader = 1;
			++cnt;
		}

		filelen -= count;
		fwrite(buf, 1, count, fifo);
	}

#if defined(RTCONFIG_SAVEJFFS)
	r = get_jffs_cfgs(stream, &len);
	logmessage("savejffs", "Read jffs cfgs from setting file. (return %d)", r);
#endif

	if (!cmpHeader)
		goto err;

	/* Slurp anything remaining in the request */
	while (len-- > 0) {
		if ((ch = fgetc(stream)) == EOF)
			break;

		if (filelen > 0) {
			fputc(ch, fifo);
			--filelen;
		}
	}

	ret = 0;

	fseek(fifo, 0, SEEK_END);
	fclose(fifo);
	fifo = NULL;
	/*printf("done\n");*/

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}

static void
do_upload_cgi(char *url, FILE *stream)
{
	int ret;
#if defined(RTCONFIG_SAVEJFFS)
	int r;
#endif

#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		ret = fcntl(ssl_stream_fd , F_GETOWN, 0);
	else
#endif
	ret = fcntl(fileno(stream), F_GETOWN, 0);

	/* Reboot if successful */
	if (ret == 0)
	{
		websApply(stream, "Uploading.asp");
#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		shutdown(ssl_stream_fd, SHUT_RDWR);
	else
#endif
		shutdown(fileno(stream), SHUT_RDWR);
#if defined(RTCONFIG_SAVEJFFS)
		r = restore_jffs_cfgs("/tmp/settings_u.prf");
		dbg("Restore jffs cfgs to /jffs, return %d\n", r);
		logmessage("savejffs", "Restore jffs cfgs to /jffs. (return %d)", r);
#endif
		sys_upload("/tmp/settings_u.prf");
#ifdef RTCONFIG_LANTIQ
		system("rm -f /jffs/db_*.tgz");
#endif
		nvram_commit();

#ifdef RTCONFIG_NVRAM_ENCRYPT
		start_enc_nvram();
#endif
		sys_reboot();
	}
	else
	{
		websApply(stream, "UploadError.asp");
	   	//unlink("/tmp/settings_u.prf");
	}
}

#ifdef RTCONFIG_OPENVPN

#define VPN_CLIENT_UPLOAD	"/tmp/openvpn_file"
#define JFFS_OPENVPN		"/jffs/openvpn/"
#define OPENVPN_EXPORT_FILE	"/tmp/server_ovpn.cert"
#define OPENVPN_UPLOAD_FLODER	"/tmp/server_ovpn_file"
#define OPENVPN_UPLOAD_FILE	"/tmp/server_ovpn_file/server_ovpn.tgz"

static void
do_vpnupload_post(char *url, FILE *stream, int len, char *boundary)
{
	char upload_fifo[] = VPN_CLIENT_UPLOAD;
	FILE *fifo = NULL;
	int ret = EINVAL, ch;
	int offset;
	char *name, *value, *p;

	memset(post_buf, 0, sizeof(post_buf));
	nvram_set("vpn_upload_type", "");
	nvram_set("vpn_upload_unit", "");

	/* Look for our part */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);

		if (!strncasecmp(post_buf, "Content-Disposition:", 20)) {
			if(strstr(post_buf, "name=\"file\""))
				break;
			else if(strstr(post_buf, "name=\"")) {
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				p = post_buf;
				name = strstr(p, "\"") + 1;
				p = strstr(name, "\"");
				strcpy(p++, "\0");
				value = strstr(p, "\r\n\r\n") + 4;
				p = strstr(value, "\r");
				strcpy(p, "\0");
				//printf("%s=%s\n", name, value);
				nvram_set(name, value);
			}
		}
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "w")))
		goto err;

	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}
		len -= strlen(post_buf);

		if(boundary) {
			if (strstr(post_buf, boundary))
				break;
		}

		fputs(post_buf, fifo);
	}

	ret = 0;

	fclose(fifo);
	fifo = NULL;
	/*printf("done\n");*/

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}

static void
do_vpnupload_cgi(char *url, FILE *stream)
{
	int ret, state;
	char *filetype = nvram_safe_get("vpn_upload_type");
	char *vpn_upload_unit = nvram_safe_get("vpn_upload_unit");
	long unit;

	if(!filetype || !vpn_upload_unit) {
		unlink(VPN_CLIENT_UPLOAD);
		return;
	}

	unit = strtol(vpn_upload_unit, NULL, 0);

#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		ret = fcntl(ssl_stream_fd , F_GETOWN, 0);
	else
#endif
	ret = fcntl(fileno(stream), F_GETOWN, 0);

	if (ret == 0)
	{
		//websApply(stream, "OvpnChecking.asp");

		if(!strcmp(filetype, "ovpn")) {
			reset_ovpn_setting(OVPN_TYPE_CLIENT, unit, 0);
			ret = read_config_file(VPN_CLIENT_UPLOAD, unit);
			nvram_set_int("vpn_upload_state", ret);
			nvram_commit();
		}
		else if(!strcmp(filetype, "ca")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_CA, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_CA_CERT));
		}
		else if(!strcmp(filetype, "cert")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_CERT, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_CERT));
		}
		else if(!strcmp(filetype, "extra")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_EXTRA, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_EXTRA));
		}
		else if(!strcmp(filetype, "key")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_KEY, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_KEY));
		}
		else if(!strcmp(filetype, "static")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_STATIC, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_STATIC));
		}
		else if(!strcmp(filetype, "ccrl")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_CRL, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_CRL));
		}
		else if(!strcmp(filetype, "scrl")) {
			set_ovpn_key(OVPN_TYPE_SERVER, unit, OVPN_SERVER_CRL, NULL, VPN_CLIENT_UPLOAD);
		}
		else if(!strcmp(filetype, "extra")) {
			set_ovpn_key(OVPN_TYPE_CLIENT, unit, OVPN_CLIENT_EXTRA, NULL, VPN_CLIENT_UPLOAD);
			state = nvram_get_int("vpn_upload_state");
			nvram_set_int("vpn_upload_state", state & (~VPN_UPLOAD_NEED_EXTRA));
		}

	}
	else
	{
		//websApply(stream, "OvpnError.asp");
	}
	unlink(VPN_CLIENT_UPLOAD);
}

static void
do_server_ovpn_file(char *url, FILE *stream)
{
	char cmd[1024];
	memset(cmd, 0, sizeof(cmd));
	if (check_if_dir_exist(JFFS_OPENVPN)) {
		int i, unit;
		char filename[32];
		char file_path[128];
		snprintf(cmd, sizeof(cmd), "tar czf %s -C %s", OPENVPN_EXPORT_FILE, JFFS_OPENVPN);
		char *lists[] = {
			"ca",
			"ca_key",
			"client_crt",
			"client_key",
			"crt",
			"dh",
			"key",
			"crl",
			"extra"
			"static",
			NULL
		};
		unit = nvram_get_int("vpn_server_unit");
		for (i = 0; i < ARRAY_SIZE(lists) && lists[i] != NULL; ++i) {
			memset(file_path, 0, sizeof(file_path));
			snprintf(filename, sizeof(filename), "vpn_crt_server%d_%s", unit, lists[i]);
			snprintf(file_path, sizeof(file_path), "%s%s", JFFS_OPENVPN, filename);
			if(check_if_file_exist(file_path)) {
				strlcat(cmd, " ", sizeof(cmd));
				strlcat(cmd, filename, sizeof(cmd));
			}
		}
		system(cmd);
		do_file(OPENVPN_EXPORT_FILE, stream);
		unlink(OPENVPN_EXPORT_FILE);
	}
}

static void
do_upload_server_ovpn_cert_post(char *url, FILE *stream, int len, char *boundary)
{
	char upload_fifo[] = OPENVPN_UPLOAD_FILE;
	FILE *fifo = NULL;
	int ret = EINVAL, ch, i;
	int iCurrentBufLen = 0;
	char post_buf[1024];
	int limit_file_size = 102400;//10kb

	memset(post_buf, 0, sizeof(post_buf));

	if (!check_if_dir_exist(OPENVPN_UPLOAD_FLODER))
		mkdir(OPENVPN_UPLOAD_FLODER, 0755);

	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			nvram_set("upload_server_ovpn_cert_temp", "0");
			goto err;
		}

		len -= strlen(post_buf);

		if (!strncasecmp(post_buf, "Content-Disposition:", 20)){
			if(strstr(post_buf, "name=\"import_cert_file\"")){
				break;
			}
		}
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			nvram_set("upload_server_ovpn_cert_temp", "0");
			goto err;
		}

		len -= strlen(post_buf);
		if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "wb"))) {
		nvram_set("upload_server_ovpn_cert_temp", "0");
		goto err;
	}

	if ( len > limit_file_size ) {
		nvram_set("upload_server_ovpn_cert_temp", "0");
		goto err;
	}

	while ( len > 0 ) {
		//get the current buffer length of stream
		iCurrentBufLen = fread(post_buf, 1, MIN(len, sizeof(post_buf)), stream);
		for (i=0; i<iCurrentBufLen; i++) {
			/* Skip last boundary, \r\n------ */
			if ((unsigned char)post_buf[i] == 0x0d && (unsigned char)post_buf[i+1] == 0x0a &&
				(unsigned char)post_buf[i+2] == 0x2d && (unsigned char)post_buf[i+3] == 0x2d) {
				break;
			}
			else {
				fputc(post_buf[i], fifo);
			}
		}

		len -= iCurrentBufLen;
	}
	nvram_set("upload_server_ovpn_cert_temp", "1");
err:
	if (fifo)
		fclose(fifo);

	fifo = NULL;

	/* Slurp anything remaining in the request */
	while (len-- > 0) {
		if((ch = fgetc(stream)) == EOF)
			break;
	}

	fcntl(fileno(stream), F_SETOWN, -ret);
}
static void
do_upload_server_ovpn_cert_cgi(char *url, FILE *stream)
{
	if (nvram_get_int("upload_server_ovpn_cert_temp")) {
		char cmd[1024];
		memset(cmd, 0, sizeof(cmd));
		if (check_if_dir_exist(OPENVPN_UPLOAD_FLODER)) {
			if(check_if_file_exist(OPENVPN_UPLOAD_FILE)) {
				int i, unit;
				char file_path[128];
				char *lists[] = {
					"ca",
					"ca_key",
					"client_crt",
					"client_key",
					"crt",
					"dh",
					"key",
					"crl",
					"extra",
					"static",
					NULL
				};
				snprintf(cmd, sizeof(cmd), "tar -xzf %s -C %s", OPENVPN_UPLOAD_FILE, OPENVPN_UPLOAD_FLODER);
				system(cmd);

				unit = nvram_get_int("vpn_server_unit");
				for (i = 0; i < ARRAY_SIZE(lists) && lists[i] != NULL; ++i) {
					memset(file_path, 0, sizeof(file_path));
					snprintf(file_path, sizeof(file_path), "%s/vpn_crt_server%d_%s", OPENVPN_UPLOAD_FLODER, unit, lists[i]);
					if(check_if_file_exist(file_path)) {
						eval("mv", "-f", file_path, JFFS_OPENVPN);
					}
				}
				websWrite(stream, "<script>parent.callback_upload_cert(1);</script>\n");
				snprintf(cmd, sizeof(cmd), "restart_vpnserver%d", unit);
				notify_rc(cmd);
				notify_rc("restart_chpass");
			}
			else {
				websWrite(stream, "<script>parent.callback_upload_cert(0);</script>\n");
			}
		}
		else {
			websWrite(stream, "<script>parent.callback_upload_cert(0);</script>\n");
		}
	}
	else {
		websWrite(stream, "<script>parent.callback_upload_cert(0);</script>\n");
	}
	nvram_unset("upload_server_ovpn_cert_temp");
	doSystem("rm -rf %s", OPENVPN_UPLOAD_FLODER);
}
#endif	//RTCONFIG_OPENVPN

#ifdef RTCONFIG_HTTPS
static void
upload_cert_check_dir()
{
	if(!d_exists(UPLOAD_CERT_FOLDER)) {
		mkdir(UPLOAD_CERT_FOLDER, 0600);
	}
}


static void
do_upload_cert_key(char *url, FILE *stream, int len, char *boundary)
{
	char upload_fifo[32];
	FILE *fifo = NULL;
	int ret = EINVAL, ch;
	char *filename, *p;
	char buf[1024];

	memset(buf, 0, sizeof(buf));
	upload_cert_check_dir();

	/* Key */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20)){
			if(strstr(buf, "name=\"file_key\"")){
				strcpy(upload_fifo, UPLOAD_KEY);
				break;
			}
		}
	}

	unlink(upload_fifo);
	p = buf;
	filename = strstr(p, "filename=\"") + strlen("filename=\"");
	p = strstr(filename, "\"");
	strcpy(p, "\0");
	//_dprintf("key filename = %s\n", filename);

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "w")))
		goto err;

	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}
		len -= strlen(buf);

		if(boundary) {
			if (strstr(buf, boundary))
				break;
		}

		fputs(buf, fifo);
		if(strstr(buf, END_KEY))
			break;
	}

	ret = 0;

	fclose(fifo);
	fifo = NULL;

	//_dprintf("done key\n");

	/* Certificate */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);

		if (!strncasecmp(buf, "Content-Disposition:", 20)){
			if(strstr(buf, "name=\"file_cert\"")){
				strcpy(upload_fifo, UPLOAD_CERT);
				break;
			}
		}
	}

	unlink(upload_fifo);

	p = buf;
	filename = strstr(p, "filename=\"") + strlen("filename=\"");
	p = strstr(filename, "\"");
	strcpy(p, "\0");
	//_dprintf("cert filename = %s\n", filename);

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}

		len -= strlen(buf);
		if (!strcmp(buf, "\n") || !strcmp(buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "w")))
		goto err;

	while (len > 0) {
		if (!fgets(buf, MIN(len + 1, sizeof(buf)), stream)) {
			goto err;
		}
		len -= strlen(buf);

		if(boundary) {
			if (strstr(buf, boundary))
				break;
		}

		fputs(buf, fifo);
		if(strstr(buf, END_CERT))
			break;
	}

	ret = 0;

	fclose(fifo);
	fifo = NULL;

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
	//_dprintf("do_upload_cert_key: end\n");

}


static void
do_upload_cert_key_cgi(char *url, FILE *stream)
{
	_dprintf("do_upload_cert_key_cgi\n");
}
#endif

static void
do_download_cert_key_cgi(char *url, FILE *stream)
{
	_dprintf("do_download_cert_key_cgi\n");
	eval("tar", "cf",
		"/tmp/cert_key.tar",
		"-C",
		"/etc",
		"cert.pem",
		"key.pem"
		);
	do_file("/tmp/cert_key.tar", stream);
}

// Viz 2010.08
static void
do_update_cgi(char *url, FILE *stream)
{
	struct ej_handler *handler;
	const char *pattern;
	int argc;
	char *argv[16];
	char s[32];

	if ((pattern = get_cgi("output")) != NULL) {
		for (handler = &ej_handlers[0]; handler->pattern; handler++) {
			if (strcmp(handler->pattern, pattern) == 0) {
				for (argc = 0; argc < 16; ++argc) {
					sprintf(s, "arg%d", argc);
					if ((argv[argc] = (char *)get_cgi(s)) == NULL) break;
				}
				handler->output(0, stream, argc, argv);
				break;
			}
		}
	}
}

//Traffic Monitor
void wo_bwmbackup(char *url, webs_t wp)
{
	static const char *hfn = "/var/lib/misc/rstats-history.gz";
	struct stat st;
	time_t t;
	int i;

	if (stat(hfn, &st) == 0) {
		t = st.st_mtime;
		sleep(1);
	}
	else {
		t = 0;
	}
	killall("rstats", SIGHUP);
	for (i = 10; i > 0; --i) {
		if ((stat(hfn, &st) == 0) && (st.st_mtime != t)) break;
		sleep(1);
	}
	if (i == 0) {
		//send_error(500, "Bad Request", (char*) 0, "Internal server error." );
		return;
	}
	//send_headers(200, NULL, mime_binary, 0);
	do_f((char *)hfn, wp);
}
// end Viz ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#ifdef RTCONFIG_QTN  //RT-AC87U
static void
do_qtn_diagnostics(char *url, FILE *stream)
{
	char qtn_rpc_client[20] = {0};

	unlink("/tmp/diagnostics_done");
	nvram_set("qtn_diagnostics", "1");
	printf("Do diagnostics\n");
	memset(qtn_rpc_client, 0, sizeof(qtn_rpc_client));
	snprintf(qtn_rpc_client, sizeof(qtn_rpc_client), "%s", nvram_safe_get("QTN_RPC_CLIENT"));
	eval("qcsapi_sockrpc", "run_script", "router_command.sh", "diagnostics", qtn_rpc_client);
	while(access("/tmp/diagnostics_done", R_OK ) == -1 ) {
		printf("run_script.log does not exist, wait\n");
		sleep(5);
	}
	do_file("/tmp/run_script.log", stream);
	unlink("/tmp/diagnostics_done");
	nvram_unset("qtn_diagnostics");
}
#endif

static void
prf_file(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
	struct json_object *root=NULL;

	do_json_decode(&root);

	char *ddns_mac;
	char ddns_hostname_tmp[128];
	char model_name;
#if defined(RTCONFIG_SOC_IPQ8064)
	unsigned char mac_buf[6], mac_buf_str[18];
#endif
#if defined(RTCONFIG_SAVEJFFS)
	char *lists[] = {
#if defined(RTCONFIG_PERMISSION_MANAGEMENT)
		".sys/Permission",
#endif
#if defined(RTCONFIG_COOVACHILLI)
		"customized_splash",
#endif
#if defined(RTCONFIG_JFFS2USERICON)
		"usericon",
#endif
		NULL
	};
#endif
	
	/* Some model use LAN MAC address to register ASUSDDNS account.
	 * To keep consistency, don't use get_wan_hwaddr() to rewrite below code.
	 */

	char *mode_flag = safe_get_cgi_json("mode", root);
	char *ddns_flag = safe_get_cgi_json("path", root);
	char *remove_passwd = safe_get_cgi_json("remove_passwd", root);
	int ddns_opt = 0, rm_passwd_opt = 0;

	if(isValidEnableOption(remove_passwd, 1))
		rm_passwd_opt = atoi(remove_passwd);
	else
		HTTPD_DBG("invalid remove_passwd option\n");

	if(isValidEnableOption(ddns_flag, 1))
		ddns_opt = atoi(ddns_flag);
	else
		HTTPD_DBG("invalid ddns_flag option\n");

	model_name = get_model();

	if(model_name == MODEL_RTN56U){
		ddns_mac = nvram_get("et1macaddr");
	}
	else{
		ddns_mac = get_lan_hwaddr();
	}

#if defined(RTCONFIG_SOC_IPQ8064)
	/* Make sure last bytes of MAC address is aligned to 4. */
	ether_atoe(ddns_mac, mac_buf);
	mac_buf[5] &= 0xFC;
	ether_etoa(mac_buf, mac_buf_str);
	ddns_mac = mac_buf_str;
#endif

	if(ddns_opt == 0){
		snprintf(ddns_hostname_tmp, sizeof(ddns_hostname_tmp), "%s", nvram_safe_get("ddns_hostname_x"));
		nvram_set("ddns_transfer", "");
		nvram_set("ddns_hostname_x", "");
	}
	else{
		nvram_set("ddns_transfer", ddns_mac);
	}

	nvram_unset("asus_device_list");
	nvram_commit();

	if(rm_passwd_opt == 1){
		sys_download_fb("/tmp/settings");
	}
	else if(!strcmp(mode_flag, "Router")){
		sys_download("/tmp/settings");
	}
	else if(!strcmp(mode_flag, "AP")){
		sys_download_ap("/tmp/settings");
	}
	else if(!strcmp(mode_flag, "Repeater_2G")){
		sys_download_rp_2g("/tmp/settings");
	}
	else if(!strcmp(mode_flag, "Repeater_5G")){
		sys_download_rp_5g("/tmp/settings");
	}
#if defined(RTAC3200) || defined(RTAC5300) || defined(GTAC5300)
	else if(!strcmp(mode_flag, "Repeater_5G2")){
		sys_download_rp_5g2("/tmp/settings");
	}
#endif
	else{
		sys_download("/tmp/settings");
	}

	if(ddns_opt == 0){
		nvram_set("ddns_hostname_x", ddns_hostname_tmp);
		nvram_commit();
	}

#if defined(RTCONFIG_SAVEJFFS)
	/* Consider to size of available RAM, /tmp, is limited.
	 * Don't save huge data, e.g. Traffic log of BWDPI which may up to 30MB,
	 * to setting file via this mechanism.
	 */
	if (lists[0] != NULL) {
		int i, r;
		FILE *fp;
		char cmd[512];

		snprintf(cmd, sizeof(cmd), "tar czvf - -C /jffs");
		for (i = 0; i < ARRAY_SIZE(lists) && lists[i] != NULL; ++i) {
			strlcat(cmd, " ", sizeof(cmd));
			strlcat(cmd, lists[i], sizeof(cmd));
		}
		fp = popen(cmd, "r");
		if (fp != NULL) {
			r = append_jffs_cfgs(fp, "/tmp/settings");
			pclose(fp);
			if (r != 0) {
				logmessage("savejffs", "append configurations in /jffs to setting file fail. (return %d)", r);
				dbg("append configurations in /jffs to setting file fail, return %d\n", r);
			} else {
				logmessage("savejffs", "backup configurations in /jffs to setting file done.");
			}
		}
	}
#endif

	do_file("/tmp/settings", wp);
	
	json_object_put(root);
}

static void
do_prf_file(char *url, FILE *stream)
{
    prf_file(stream, NULL, NULL, 0, url, NULL, NULL);
}

static void
do_uploadIconFile_file(char *url, FILE *stream)
{
	system("tar cvf /tmp/IconFile.tar /jffs/usericon /tmp/upnpicon");
	do_file("/tmp/IconFile.tar", stream);
	unlink("/tmp/IconFile.tar");
}

#ifdef RTCONFIG_IPSEC
static void
do_ipsec_file(char *url, FILE *stream) {
	do_file(FILE_PATH_IPSEC_LOG, stream);
}

#define JFFS_IPSEC		"/jffs/ipsec/"
#define JFFS_CA_FILES		"/jffs/ca_files/"
static void
do_caupload_post(char *url, FILE *stream, int len, char *boundary)
{
	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_CA_FILES))
		mkdir(JFFS_CA_FILES, 0755);	

	char upload_fifo[32];
	int ret = EINVAL, ch;
	int offset;
	char *name, *value, *p;
	memset(upload_fifo, 0, 32);
	memset(post_buf, 0, sizeof(post_buf));
	char upload_value[32];
	memset(upload_value, 0, 32);
	char org_file_name[64];
	memset(org_file_name, 0, 64);
	char file_name[64];
	memset(file_name, 0, 64);
	/* Look for our part */
	
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strncasecmp(post_buf, "Content-Disposition:", 20)) {
			if(strstr(post_buf, "name=\"file_ca\"")) {
				sprintf(org_file_name, "%s", strstr(post_buf, "filename="));
				substr(file_name, org_file_name, 10, (strlen(org_file_name)-13));
				nvram_set("ca_manage_file_name_ca", file_name);
				break;
			}		
			else if(strstr(post_buf, "name=\"file_private_key\"")) {
				sprintf(org_file_name, "%s", strstr(post_buf, "filename="));
				substr(file_name, org_file_name, 10, (strlen(org_file_name)-13));
				nvram_set("ca_manage_file_name_private_key", file_name);
				break;
			}
			else if(strstr(post_buf, "name=\"file_p12\"")) {
				sprintf(org_file_name, "%s", strstr(post_buf, "filename="));
				substr(file_name, org_file_name, 10, (strlen(org_file_name)-13));
				nvram_set("ca_manage_file_name_p12", file_name);
				break;
			}
			else if(strstr(post_buf, "name=\"ca_manage_upload_type\"")) {
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				p = post_buf;
				name = strstr(p, "\"") + 1;
				p = strstr(name, "\"");
				strcpy(p++, "\0");
				value = strstr(p, "\r\n\r\n") + 4;
				p = strstr(value, "\r");
				strcpy(p, "\0");
				sprintf(upload_value, "%s", value);
			}
		}
	}
	
	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
			break;
		}
	}


	char nvram_upload_value[5000];
	memset(nvram_upload_value, 0, 5000);
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}
		len -= strlen(post_buf);

		if(boundary) {
			if (strstr(post_buf, boundary))
				break;
		}
		strcat(nvram_upload_value, post_buf);	
	}

	ret = 0;
	if (strcmp(upload_value, "file_ca") == 0)  {
		nvram_set("ca_manage_upload_ca", nvram_upload_value);
	} 
	else if (strcmp(upload_value, "file_private_key") == 0) {
		nvram_set("ca_manage_upload_private_key", nvram_upload_value);
	}
	else if (strcmp(upload_value, "file_p12") == 0) {
		nvram_set("ca_manage_upload_p12", nvram_upload_value);
	}

err: 
	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);	
}
static void
do_ipsecupload_post(char *url, FILE *stream, int len, char *boundary)
{
	char *filetype = nvram_safe_get("ipsec_profile_item");

	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_IPSEC))
		mkdir(JFFS_IPSEC, 0755);	

	char upload_fifo[32];
	FILE *fifo = NULL;
	int ret = EINVAL, ch;
	int offset;
	char *name, *value, *p;
	char filename[32];
	memset(filename, 0, 32);
	memset(upload_fifo, 0, 32);

	sprintf(filename, "/jffs/ipsec/%s.crt", filetype);

	memset(post_buf, 0, sizeof(post_buf));
	nvram_set("ipsec_profile_item", "");
	
	/* Look for our part */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strncasecmp(post_buf, "Content-Disposition:", 20)) {
			if(strstr(post_buf, "name=\"file\""))
				break;
			else if(strstr(post_buf, "name=\"ipsec_profile_item\"")) {
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				p = post_buf;
				name = strstr(p, "\"") + 1;
				p = strstr(name, "\"");
				strcpy(p++, "\0");
				value = strstr(p, "\r\n\r\n") + 4;
				p = strstr(value, "\r");
				strcpy(p, "\0");
				_dprintf("%s=%s\n", name, value);
				nvram_set(name, value);
				sprintf(upload_fifo, "/jffs/ipsec/%s.crt", value);
			}
		}
	}
	
	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(upload_fifo, "w")))
		goto err;

	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}
		len -= strlen(post_buf);

		if(boundary) {
			if (strstr(post_buf, boundary))
				break;
		}
		fputs(post_buf, fifo);
	}

	ret = 0;

	fclose(fifo);
	fifo = NULL;
	
	/*printf("done\n");*/

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
	
}
static void
do_ipsecupload_cgi(char *url, FILE *stream) {
	int ret;
	char *index;
	char name[32];
	char ca_name[16];
	char ca_file_name[40];
	memset(name, 0, 32);
	memset(ca_name, 0, 16);
	memset(ca_file_name, 0, 40);

	sprintf(name, nvram_safe_get("ipsec_profile_item"));
#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		ret = fcntl(ssl_stream_fd , F_GETOWN, 0);
	else
#endif
	ret = fcntl(fileno(stream), F_GETOWN, 0);

	if (ret == 0) {
			index = strtok(name, "_");//ipsec
			index = strtok(NULL, "_");//profile
			index = strtok(NULL, "_");//index
		
			sprintf(ca_name, "ipsec_ca_%s", index);
			sprintf(ca_file_name, "/jffs/ipsec/ipsec_profile_%s.crt", index);
			set_crt_parsed(ca_name, ca_file_name);
	}
}

static void
do_clear_file_cgi(char *url, FILE *stream)
{
#ifdef RTCONFIG_IPSEC
	char *cmd[] = {"echo", "", NULL};
	int pid;
#endif
	char file_name[64];
	memset(file_name, 0, 64);
	
	char *clear_file;
	clear_file = websGetVar(wp, "clear_file_name", "");
#ifdef RTCONFIG_IPSEC
	if(strcmp(clear_file, "ipsec") == 0) {
		sprintf(file_name, FILE_PATH_IPSEC_LOG);
		if(check_if_file_exist(file_name)) {
			//unlink(file_name);
			_eval(cmd, ">"FILE_PATH_IPSEC_LOG, 0, &pid);
		}
	}
#endif
}
#endif

static void
do_networkmap_file(char *url, FILE *stream)
{
	system("cat nmp_client_list > /tmp/nmp_client_list.log");
	system("nvram get asus_device_list > /tmp/asus_dev_list.log");
	eval("tar", "cf",
		"/tmp/networkmap.tar",
		"/tmp/upnp.log",
		"/tmp/smb.log",
		"/tmp/syslog.log",
		"/tmp/nmp_client_list.log",
		"/tmp/asus_dev_list.log",
#ifdef RTCONFIG_UPNPC
		"/tmp/upnpc_xml.log",
#endif
#ifdef RTCONFIG_BONJOUR
		"/tmp/mDNSNetMonitor.log",
#endif
		"/jffs/usericon",
		"/tmp/upnpicon");
	do_file("/tmp/networkmap.tar", stream);
	unlink("/tmp/networkmap.tar");
}

static void
do_upnp_file(char *url, FILE *stream)
{
	do_file("/tmp/upnp.log", stream);
}

static void
do_upnpc_xml_file(char *url, FILE *stream)
{
#ifdef RTCONFIG_UPNPC
	do_file("/tmp/upnpc_xml.log", stream);
#endif
}

static void
do_dnsnet_file(char *url, FILE *stream)
{
#ifdef RTCONFIG_BONJOUR
	do_file("/tmp/mDNSNetMonitor.log", stream);
#endif
}

static void
do_prf_ovpn_file(char *url, FILE *stream)
{
	nvram_commit();
	do_file(url, stream);
}

#ifdef RTCONFIG_DSL_TCLINUX
static void
do_diag_log_file(char *url, FILE *stream)
{
	char path[128];
	snprintf(path, sizeof(path), "%s/asus_diagnostic/%s", nvram_safe_get("dsltmp_diag_log_path"), url);
	//_dprintf("Get log file %s\n", path);
	do_file(path, stream);
}
#endif

static void
deleteOfflineClient(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
	int ret=200;
	struct json_object *root=NULL;
	char *mac = NULL;
	char mac_str[13];

	do_json_decode(&root);

	mac = safe_get_cgi_json("delete_offline_client", root);

	if(!isValidMacAddress(mac)){
		HTTPD_DBG("invalid mac\n");
		ret = 4001;
		goto FINISH;
	}

	int i, shm_client_info_id;
	void *shared_client_info=(void *) 0;
	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
	int lock;

	i = 0;
	while((*mac) && (i < 12)) {
		if(*mac==':') {
			mac++;
			continue;
		}
		else {
			mac_str[i] = tolower(*mac);
			//add buffer protection
			if (i == 12) return;
			i++;
			mac++;
		}
	}
	if(i!=12)
		return;

	mac_str[i] = '\0';

	lock = file_lock("networkmap");
	shm_client_info_id = shmget((key_t)SHMKEY_LAN, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
		fprintf(stderr,"shmget failed\n");
		file_unlock(lock);
		ret = 4002;
		goto FINISH;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		file_unlock(lock);
		ret = 4002;
		goto FINISH;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	strlcpy(p_client_info_tab->delete_mac, mac_str, sizeof(p_client_info_tab->delete_mac));
	shmdt(shared_client_info);
	file_unlock(lock);

	doSystem("killall -%d networkmap", SIGUSR2);

FINISH:
	json_object_put(root);
	websWrite(wp, "{\"statusCode\":\"%d\"}", ret);
}

static void
do_deleteOfflineClient_cgi(char *url, FILE *stream)
{
	deleteOfflineClient(stream, NULL, NULL, 0, url, NULL, NULL);

}

#ifdef RTCONFIG_NETOOL
static void
do_netool_cgi(char *url, FILE *stream)
{
	netool(stream, NULL, NULL, 0, url, NULL, NULL);
}
#endif

#if 0 // obsoleted, RTCONFIG_RGBLED
#define WEB_AURA_SW_REQ     0
#define WEB_AURA_SW_SET     1
#define WEB_ROUTER_AURA_SET 2

void get_rgb_sta(const char *sta, int *staArray)
{
	char *tmp, *p;
	int i;
	tmp = p = sta;
	for(i = 0; i < RGB_MAX_GROUP; ++i)
	{
		if((tmp = strchr(tmp, ' ')))
		{
			staArray[i] = atoi(p);
			tmp++;
			p = tmp;
		}
		else if(p)
		{
			staArray[i] = atoi(p);
			break;
		}
	}
}

static int
aurargb_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg, char_t *url, char_t *path, char_t *query)
{
	int dType;
	char *type, *red, *blue, *green, *mode, *speed, *direction, group[4];
	int dRed[RGB_MAX_GROUP] = {0}, dBlue[RGB_MAX_GROUP] = {0}, dGreen[RGB_MAX_GROUP] = {0}, dMode[RGB_MAX_GROUP] = {0}, dSpeed[RGB_MAX_GROUP] = {0}, dDirection[RGB_MAX_GROUP] = {0};
	int i, ret=0;
	RGB_LED_STATUS_T status;
	struct json_object *sta=NULL;

	struct json_object *root=NULL;

	do_json_decode(&root);

	type = get_cgi_json("type", root);

	if(!type){
		_dprintf("aurargb error: no type\n");
		json_object_put(root);
		return 1;
	}
	else
		dType = atoi(type);
	_dprintf("########## %s rgb cmd type:%d\n", type, dType);

	switch(dType) {
	case WEB_AURA_SW_REQ:
		sta = json_object_new_object();
		snprintf(group, sizeof(group), "%d", RGB_MAX_GROUP);
		json_object_object_add(sta, "group", json_object_new_string(group));
		json_object_object_add(sta, "capability", json_object_new_string(AURA_CAP));
		struct json_object *json_red = json_object_new_array();
		struct json_object *json_blue = json_object_new_array();
		struct json_object *json_green = json_object_new_array();
		struct json_object *json_mode = json_object_new_array();
		struct json_object *json_speed = json_object_new_array();
		struct json_object *json_direction = json_object_new_array();

		for(i = 0; i < RGB_MAX_GROUP; ++i)
		{
			memset(&status, 0, sizeof(RGB_LED_STATUS_T));
			if((aura_rgb_led(WEB_AURA_SW_REQ, &status, i, 0) < 0))
			{
				_dprintf("type:%d set rgb error!\n", dType);
				ret = -1;
				break;
			}
			json_object_array_add(json_red, json_object_new_int(status.red));
			json_object_array_add(json_blue, json_object_new_int(status.blue));
			json_object_array_add(json_green, json_object_new_int(status.green));
			json_object_array_add(json_mode, json_object_new_int(status.mode));
			//handle signed char
			if(status.speed == 255)
				json_object_array_add(json_speed, json_object_new_int(-1));
			if(status.speed == 254)
				json_object_array_add(json_speed, json_object_new_int(-2));
			else
				json_object_array_add(json_speed, json_object_new_int(status.speed));
			json_object_array_add(json_direction, json_object_new_int(status.direction));
		}
		if (ret)
			break;

		json_object_object_add(sta, "red", json_red);
		json_object_object_add(sta, "blue", json_blue);
		json_object_object_add(sta, "green", json_green);
		json_object_object_add(sta, "mode", json_mode);
		json_object_object_add(sta, "speed", json_speed);
		json_object_object_add(sta, "direction", json_direction);

		_dprintf("#### rgb status %s\n", json_object_to_json_string(sta));
		websWrite(wp, "%s", json_object_to_json_string(sta));

		if(sta)
			json_object_put(sta);
		break;
	case WEB_AURA_SW_SET:
		break;
	case WEB_ROUTER_AURA_SET:
		if(red = get_cgi_json("red", root))
			get_rgb_sta(red, dRed);
		if(blue = get_cgi_json("blue", root))
			get_rgb_sta(blue, dBlue);
		if(green = get_cgi_json("green", root))
			get_rgb_sta(green, dGreen);
		if(mode = get_cgi_json("mode", root))
			get_rgb_sta(mode, dMode);
		if(speed = get_cgi_json("speed", root))
			get_rgb_sta(speed, dSpeed);
		if(direction = get_cgi_json("direction", root))
			get_rgb_sta(direction, dDirection);

		for(i = 0; i < RGB_MAX_GROUP; ++i)
		{
			_dprintf("### num %d: red:%d blue:%d green:%d mode:%d speed:%d direction:%d\n", i, dRed[i], dBlue[i], dGreen[i], dMode[i], dSpeed[i], dDirection[i]);
			status.red = dRed[i];
			status.blue = dBlue[i];
			status.green = dGreen[i];
			status.mode = dMode[i];
			status.speed = dSpeed[i];
			status.direction = dDirection[i];
			if((aura_rgb_led(WEB_ROUTER_AURA_SET, &status, i, 0) < 0))
			{
				_dprintf("type:%d set rgb error\n", dType);
				ret = -1;
				break;
			}
		}
		break;
	default:
		_dprintf("unknown command\n");
	}
	if(root)
		json_object_put(root);

	return 1;
}
static void
do_aurargb_cgi(char *url, FILE *stream)
{
	aurargb_cgi(stream, NULL, NULL, 0, url, NULL, NULL);

}
#endif

#ifdef RTCONFIG_QCA_PLC_UTILS
static void ApplyPNN(FILE *stream)
{
	char *pnn = websGetVar(stream, "plc_pnn", "");
	char *plc = websGetVar(stream, "plc_known", "");

	if (apply_private_name(pnn, plc) == -1)
		websWrite(stream, "0");
	else
		websWrite(stream, "1");
}

static void TriggerPair(FILE *stream)
{
	if (trigger_plc_pair() == -1)
		websWrite(stream, "0");
	else
		websWrite(stream, "1");
}

static void AddPLC(FILE *stream)
{
	char *mac = websGetVar(stream, "plc_add_mac", "");
	char *pwd = websGetVar(stream, "plc_add_pwd", "");

	if (add_remote_plc(mac, pwd) == -1)
		websWrite(stream, "0");
	else
		websWrite(stream, "1");
}

static void GetPhyRate(FILE *stream)
{
	struct remote_plc *plc = NULL;
	int cnt, i;
	int comma_need = 0;
	int fromapp_flag = 0;

	fromapp_flag = check_user_agent(user_agent);

	cnt = get_connected_plc(&plc);

	if (fromapp_flag != 0)
		websWrite(stream, "{\"plc_PhyRate\":");

	websWrite(stream, "[");
	for (i = 0; i < cnt; i++) {
		if (comma_need)
			websWrite(stream, ", ");
		websWrite(stream, "{\"MAC\":\"%s\", \"Tx\":\"%d\", \"Rx\":\"%d\"}", plc[i].mac, plc[i].tx, plc[i].rx);

		if (comma_need == 0)
			comma_need = 1;
	}
	websWrite(stream, "]");

	if (fromapp_flag != 0)
		websWrite(stream, "}");

	if (plc)
		free(plc);
}

static void
do_plc_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;

	do_json_decode(&root);

	char *action_mode = get_cgi_json("action_mode", root);

	dbg("%s: [%s]\n", __func__, action_mode);

	if (!strcmp(action_mode, "ApplyPNN")) {
		ApplyPNN(stream);
	}
	else if (!strcmp(action_mode, "TriggerPair")) {
		TriggerPair(stream);
	}
	else if (!strcmp(action_mode, "AddPLC")) {
		AddPLC(stream);
	}
	else if (!strcmp(action_mode, "GetPhyRate")) {
		GetPhyRate(stream);
	}
	json_object_put(root);
}
#endif

#if defined(RTCONFIG_LP5523) || defined(MAPAC1750)
static void do_lp55xx_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	do_json_decode(&root);
	char *action_mode = get_cgi_json("action_mode", root);
	int lp55xx_enable = safe_atoi(get_cgi_json("lp55xx_enable", root));
	int prestate = 0;

	logmessage("HTTPD","%s: [%s] [%s]\n", __func__, action_mode, lp55xx_enable?"Enable":"Disable");

	if (!strcmp(action_mode, "LedCtrlLC")) {
		nvram_set_int("lp55xx_lp5523_user_enable", lp55xx_enable);
		prestate = 1;
	}
	else if (!strcmp(action_mode, "LedCtrlSCH")) {
		if (!lp55xx_enable && nvram_get_int("lp55xx_lp5523_sch_enable"))
			prestate = 1;

		nvram_set_int("lp55xx_lp5523_sch_enable", lp55xx_enable);
	}
	nvram_commit();

	if (prestate)
#if defined(MAPAC1750)
		nvram_set("prelink_pap_status", "0");
#else
		lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_PREVIOUS_STATE);
#endif

	websWrite(stream, "1\n");

	json_object_put(root);
}
#endif /*LP5523*/

#if defined(RTCONFIG_DETWAN)
enum BLE_PYH_STATUS
{
	PHY_PORT0	= 0x01,
	PHY_PORT1	= 0x02
};
#endif

enum BLE_WAN_STATUS
{
	BLE_WAN_STATUS_ALL_DISCONN=0,
	BLE_WAN_STATUS_ALL_UNKNOWN,
	BLE_WAN_STATUS_PORT0_DHCP,
	BLE_WAN_STATUS_PORT0_PPPOE,
	BLE_WAN_STATUS_PORT0_UNKNOWN,
	BLE_WAN_STATUS_PORT1_DHCP,
	BLE_WAN_STATUS_PORT1_PPPOE,
	BLE_WAN_STATUS_PORT1_UNKNOWN,
	BLE_WAN_STATUS_PORT0_DHCP_PPPOE,
	BLE_WAN_STATUS_PORT1_DHCP_PPPOE,
	BLE_WAN_END
};

static void GetWanStatus(char *state)
{
	char prefix[CKN_STR64];
	int wanstatus=BLE_WAN_STATUS_ALL_DISCONN;
#if defined(RTCONFIG_DETWAN)
	char *detwan[] = {"detwan", NULL};
	int max_inf, value, conn=0;
	int wan_proto=-1, idx, conn_tmp=1;

	memset(prefix, '\0', sizeof(prefix));

	/* Check the port status */
	max_inf = nvram_get_int("detwan_max");
	for (idx = 0; idx < max_inf; idx++, conn_tmp=conn_tmp<<1) {
		snprintf(prefix, sizeof(prefix), "detwan_mask_%d", idx);
		if ((value = nvram_get_int(prefix)) != 0) {
			if (get_ports_status((unsigned int)value))
					conn |= conn_tmp;
		}
	}

	if ((conn&PHY_PORT0)&&(conn&PHY_PORT1))
		wanstatus = BLE_WAN_STATUS_ALL_UNKNOWN;
	else if (conn>0) {
		/* Check the link proto */
		nvram_unset("wan0_ifname");
		_eval(detwan, NULL, 0, NULL);
		idx = 0;
		while ((nvram_safe_get("wan0_ifname")[0] =='\0') && idx<10) {
			sleep(1);
			idx++;
		}
		memset(prefix, '\0', CKN_STR64);
		snprintf(prefix, sizeof(prefix), "%s", nvram_safe_get("wan0_ifname"));
		wan_proto = nvram_get_int("detwan_proto");

		/* Check the link status */
		notify_rc_and_wait("restart_wan_if 0");
		logmessage("BLUEZ", "wan:%s, proto:%d\n", prefix, wan_proto);

		if (strlen(prefix)) {
#if defined(RTCONFIG_QCA953X) || defined(RTCONFIG_QCA956X)
			if (!strncmp(prefix, "vlan2", strlen(prefix))) {
#else
			if (!strncmp(prefix, WAN_IF_ETH, strlen(prefix))) {
#endif
				if (wan_proto == 1)
					wanstatus = BLE_WAN_STATUS_PORT0_DHCP;
				else if (wan_proto == 2)
					wanstatus = BLE_WAN_STATUS_PORT0_PPPOE;
				else if (wan_proto == 3)
					wanstatus = BLE_WAN_STATUS_PORT0_DHCP_PPPOE;
				else
					wanstatus = BLE_WAN_STATUS_PORT0_UNKNOWN;
			}
			else {
				if (wan_proto == 1)
					wanstatus = BLE_WAN_STATUS_PORT1_DHCP;
				else if (wan_proto == 2)
					wanstatus = BLE_WAN_STATUS_PORT1_PPPOE;
				else if (wan_proto == 3)
					wanstatus = BLE_WAN_STATUS_PORT1_DHCP_PPPOE;
				else
					wanstatus = BLE_WAN_STATUS_PORT1_UNKNOWN;
			}
		}
		else {
			if (conn&PHY_PORT0)
				wanstatus = BLE_WAN_STATUS_PORT0_UNKNOWN;
			else if (conn&PHY_PORT1)
				wanstatus = BLE_WAN_STATUS_PORT1_UNKNOWN;
		}
	}
#else
	char prefix2[CKN_STR64];
	char tmp[CKN_STR64], tmp2[CKN_STR64];
	int unit/*, idx=0*/;

	memset(prefix, '\0', sizeof(prefix));
	memset(prefix2, '\0', sizeof(prefix2));

	// while (idx++<=5) sleep(1);

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
		if(get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_WAN && get_dualwan_by_unit(unit) != WANS_DUALWAN_IF_LAN)
			continue;

		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		if(unit == WAN_UNIT_FIRST)
			snprintf(prefix2, sizeof(prefix2), "autodet_");
		else
			snprintf(prefix2, sizeof(prefix2), "autodet%d_", unit);

		if (nvram_get_int(strcat_r(prefix2, "state", tmp2)) == AUTODET_STATE_FINISHED_NOLINK) {
			if(nvram_get_int(strcat_r(prefix, "auxstate_t", tmp))==1) {
				nvram_set("autodet_state", "0");
				notify_rc_after_period_wait("start_autodet", 0);
			}
			wanstatus = BLE_WAN_STATUS_ALL_DISCONN;
		}
		else if (nvram_get_int(strcat_r(prefix2, "state", tmp2)) == AUTODET_STATE_FINISHED_WITHPPPOE
			|| nvram_get_int(strcat_r(prefix2, "auxstate", tmp2)) == AUTODET_STATE_FINISHED_WITHPPPOE) {
			if( ( nvram_get_int(strcat_r(prefix, "state_t", tmp))==2
				&& nvram_get_int(strcat_r(prefix, "sbstate_t", tmp))==0
				&& nvram_get_int(strcat_r(prefix, "auxstate_t", tmp))==0 ) 
			    &&
			    ( nvram_get_int("link_internet")==2
				|| nvram_get_int(strcat_r(prefix, "realip_state", tmp))==2 )
			   ) {
				wanstatus = BLE_WAN_STATUS_PORT0_DHCP;
			}
			else
				wanstatus = BLE_WAN_STATUS_PORT0_PPPOE;
		}
		else if( nvram_get_int(strcat_r(prefix, "state_t", tmp))==2
			&& nvram_get_int(strcat_r(prefix, "sbstate_t", tmp))==0
			&& nvram_get_int(strcat_r(prefix, "auxstate_t", tmp))==0 ) 
			wanstatus = BLE_WAN_STATUS_PORT0_DHCP;
		else if( nvram_get_int(strcat_r(prefix2, "state", tmp2))==2) {
			if ( nvram_get_int(strcat_r(prefix, "auxstate_t", tmp))!=1)
				wanstatus = BLE_WAN_STATUS_PORT0_DHCP;
			else if( nvram_get_int(strcat_r(prefix, "state_t", tmp))==4
				&& nvram_get_int(strcat_r(prefix, "sbstate_t", tmp))==4
				&& nvram_get_int(strcat_r(prefix, "auxstate_t", tmp))==0 ) 
				wanstatus = BLE_WAN_STATUS_PORT0_UNKNOWN;
		}
		else if( nvram_get_int(strcat_r(prefix, "state_t", tmp))==4
			&& nvram_get_int(strcat_r(prefix, "sbstate_t", tmp))==4 )
			wanstatus = BLE_WAN_STATUS_PORT0_DHCP;
		else
			wanstatus = BLE_WAN_STATUS_PORT0_UNKNOWN;
	}

#if 0
		printf("%s, %s[%d], %s[%d], %s[%d], %s[%d]\n"
			"%s, %s[%d], %s[%d]\n"
			"%s, %s[%d]\n",
				prefix,
				"state", nvram_get_int(strcat_r(prefix, "state_t", tmp)),
				"sbstate", nvram_get_int(strcat_r(prefix, "sbstate_t", tmp)),
				"auxstate", nvram_get_int(strcat_r(prefix, "auxstate_t", tmp)),
				"realip_state", nvram_get_int(strcat_r(prefix, "realip_state", tmp)),
				prefix2,
				"state", nvram_get_int(strcat_r(prefix2, "state", tmp2)),
				"auxstate", nvram_get_int(strcat_r(prefix2, "auxstate", tmp2)),
				"None",
				"link_internet", nvram_get_int("link_internet")
		);
#endif
#endif

	snprintf(state, CKN_STR2, "%d", wanstatus);
	logmessage("HTTPD", "wan0_ifname:%s, link_internet:%d, state:%s\n", prefix, nvram_get_int("link_internet"), state);
}

static void do_detwan_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	do_json_decode(&root);
	char *action_mode = get_cgi_json("action_mode", root);
	json_object *new_root = json_object_new_object();
	char state[CKN_STR2];

	logmessage("HTTPD","%s: [%s]\n", __func__, action_mode);

	if (!strcmp(action_mode, "GetWanStatus")) {
		memset(state, '\0', sizeof(state));
		GetWanStatus(state);
		logmessage("HTTPD","%s: %s:%s\n", __func__, "wan state", state);
		json_object_object_add(new_root, "state", json_object_new_string(state));
		websWrite(stream, "%s\n", json_object_to_json_string(new_root));
		json_object_put(new_root);
	}

	json_object_put(root);
}

#if defined(RTCONFIG_WIFI_SON)
static void GetAthXStatus(char *state, int band, char *param)
{
	FILE *fp;
	char tmp[CKN_STR64], tmp_t[CKN_STR64], buf[CKN_STR256];
	char *pt1,*pt2;
	int len;

	memset(tmp, '\0', CKN_STR64);
	memset(buf, '\0', CKN_STR256);

	if (!strcmp(param, "channel")) {
		int cac=-1, freq=-2;

		snprintf(tmp, sizeof(tmp), "%d", freq);
		sprintf(buf, "iwpriv %s get_cac_state", get_wififname(band));

		fp = popen(buf, "r");
		if (fp) {
			memset(buf, 0, sizeof(buf));
			len = fread(buf, 1, sizeof(buf), fp);
			pclose(fp);
			if (len > 1) {
				buf[len-1] = '\0';
				pt1 = strstr(buf, "get_cac_state:");
				if (pt1) {
					pt2 = pt1 + strlen("get_cac_state: ");
					chomp(pt2);
					cac = safe_atoi(pt2);
				}
			}
		}

		if (cac)
			snprintf(tmp, sizeof(tmp), "%d", cac);
		else {
			memset(buf, '\0', CKN_STR256);
			sprintf(buf, "iwconfig %s", get_wififname(band));

			fp = popen(buf, "r");
			if (fp) {
				memset(buf, 0, sizeof(buf));
				len = fread(buf, 1, sizeof(buf), fp);
				pclose(fp);
				if (len > 1) {
					buf[len-1] = '\0';
					pt1 = strstr(buf, "Frequency:");
					if (pt1) {
						pt2 = strstr(pt1, "GHz");
						if(pt2) {
							memset(tmp, '\0', CKN_STR64);
							strncpy(tmp,pt1+strlen("Frequency:"),pt2-pt1-strlen("Frequency:"));
							chomp(tmp);
							freq=(int)(1000*atof(tmp));
							freq=(freq-5170)*2/10 + 34;
							memset(tmp, '\0', CKN_STR64);
							snprintf(tmp_t, sizeof(tmp), "%s%d", tmp, freq);
							strlcpy(tmp, tmp_t, sizeof(tmp));
						}
					}
				}
			}
		}
	}

	snprintf(state, sizeof(state), "%s", tmp);
	logmessage("HTTPD", "Get ath%d %s:%s\n", band, param, state);
}
static void SetAthXStatus(int band, char *param, char *value)
{
	char prefix[CKN_STR64], ifname[CKN_STR64], tmp[CKN_STR64];
	int success=0;

	memset(prefix, '\0', sizeof(prefix));
	memset(ifname, '\0', sizeof(ifname));
	memset(tmp, '\0', sizeof(tmp));

	snprintf(prefix, sizeof(prefix), "wl%d_", band);
	snprintf(ifname, sizeof(ifname), "ath%d", band);

	if (!strcmp(param, "channel")) {
		unsigned char countryCode[3];
		char chList[256]; 
		char *buf, *delim=",";
		int list=0;

		memset(countryCode, 0, sizeof(countryCode));
		strncpy(countryCode, nvram_safe_get(strcat_r(prefix, "country_code", tmp)), 2);
	 
		list = get_channel_list_via_driver(band, chList, sizeof(chList));
		if (list<=0 && countryCode[0] != 0xff && countryCode[1] != 0xff) {   // 0xffff is default
			list = get_channel_list_via_country(band, countryCode, chList, sizeof(chList));
		}

		if (list>0) {
			list=0;
			buf = strtok(chList, delim);
			while (buf!=NULL) {
				if (!strncmp(buf, value, strlen(buf))) {
					list=1;
					break;
				}
				buf = strtok(NULL, delim);
			}

			if (list) {
				success=1;
				nvram_set(strcat_r(prefix, param, tmp), value);
				eval("iwconfig", ifname, param, value);
			}
		}
	}

	logmessage("HTTPD", "Set ath%d %s:%s %s\n", band, param, value, success?"SUCCESS":"FAIL");
}

static void do_athX_state_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	do_json_decode(&root);
	char *action_mode = get_cgi_json("action_mode", root);
	int band = safe_atoi(get_cgi_json("band", root));
	char *param = get_cgi_json("param", root);
	char *value = get_cgi_json("value", root);

	logmessage("HTTPD","%s: [ATH%d] %s %s %s\n", __func__, band, action_mode, param, value);
	_dprintf("%s: [ATH%d] %s %s %s\n", __func__, band, action_mode, param, value);

	if (!strcmp(action_mode, "GET")) {
		json_object *new_root = json_object_new_object();

		char state[CKN_STR32];

		memset(state, '\0', sizeof(state));

		GetAthXStatus(state, band, param);
		json_object_object_add(new_root, "state", json_object_new_string(state));
		websWrite(stream, "%s\n", json_object_to_json_string(new_root));
		json_object_put(new_root);
	}
	if (!strcmp(action_mode, "SET")) {
		SetAthXStatus(band, param, value);
		websWrite(stream, "1\n");
	}

	json_object_put(root);
}
#endif

#ifdef RTCONFIG_NOTIFICATION_CENTER
static void
do_blocking_request_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	char *block_CName, *block_mac,*block_interval, *block_timestap;
	char *block_mac_t, *block_timestap_t;
	char nvramTmp[4096]={0};
	char *buf, *g, *p;
	int retStatus=0;
	char filename[128];
	memset(filename, 0, 128);

	do_json_decode(&root);
	block_CName = safe_get_cgi_json("CName", root);
	block_mac = safe_get_cgi_json("mac", root);
	block_interval = safe_get_cgi_json("interval", root);
	block_timestap = safe_get_cgi_json("timestap", root);


	time_t now = uptime();
	time(&now);

	//_dprintf("%s: block_CName = %s, block_mac = %s, block_timestr = \"%ld\", block_timestap = %s\n", __func__, block_CName, block_mac, now, block_timestap);
	if(abs((unsigned long)(now + 3600) - atol(block_timestap)) > 20 || strstr(nvram_safe_get("MULTIFILTER_MAC"), block_mac) == NULL){
		_dprintf("blocking_request_cgi: not valid blocking request\n");
		if (root != NULL) 
			json_object_put(root);
		return;
	}
	g = buf = strdup(nvram_safe_get("MULTIFILTER_TMP_T"));
	while (buf) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &block_mac_t, &block_timestap_t)) != 2) continue;
		if((now-atol(block_timestap_t)) > 0){
			continue;
		}else{
			if(!strcmp(block_mac_t, block_mac))
				retStatus = 1; //find blocking mac alive

			strcat(nvramTmp, "<");
			strcat(nvramTmp, block_mac_t);
			strcat(nvramTmp, ">");
			strcat(nvramTmp, block_timestap_t);
		}
	}
	free(buf);

	if(retStatus == 0){
		strcat(nvramTmp, "<");
		strcat(nvramTmp, block_mac);
		strcat(nvramTmp, ">");
		strcat(nvramTmp, block_timestap);
	}

	nvram_set("MULTIFILTER_TMP_T", nvramTmp);
	nvram_commit();

	json_object *nc_root = json_object_new_object();

	if (nc_root == NULL) {
		_dprintf("[%s(%d)]new json object error", __FUNCTION__, __LINE__);
		char nt_tmp[MAX_EVENT_INFO_LEN];
		snprintf(nt_tmp, sizeof(nt_tmp), "{\"CName\":\"%s\", \"MacAddress\":\"%s\", \"Interval\":\"%s\", \"TimeStamp\":\"%s\"}", block_CName, block_mac, block_interval, block_timestap);
		SEND_NT_EVENT(ADMIN_LOGIN_FAIL_LAN_WEB_EVENT, nt_tmp);
	} else {
		json_object_object_add(nc_root, "CName", json_object_new_string(block_CName));
		json_object_object_add(nc_root, "MacAddress", json_object_new_string(block_mac));
		json_object_object_add(nc_root, "Interval", json_object_new_string(block_interval));
		json_object_object_add(nc_root, "TimeStamp", json_object_new_string(block_timestap));
		SEND_NT_EVENT(PERMISSION_FROM_TIME_SCHEDULE_EVENT, json_object_to_json_string(nc_root));
	}

	json_object_put(root);
	json_object_put(nc_root);

	memset(filename, 0, 128);
	snprintf(filename, sizeof(filename), "blocking.asp?mac=%s", block_mac);
	websRedirect(stream, filename);
}

static void
do_blocking_cgi(char *url, FILE *stream)
{
	char nvramTmp[4096]={0};
	struct json_object *root=NULL;
	char *buf, *g, *p;
	char *block_CName, *block_mac,*block_interval, *block_timestap;
	char *block_enabled_t, *block_CName_t, *block_mac_t,*block_interval_t, *block_timestap_t;
	int retStatus = 0;
	time_t now = uptime();
	time(&now);

	do_json_decode(&root);

	block_CName = get_cgi_json("CName", root);
	block_mac = get_cgi_json("MacAddress", root);
	block_interval = get_cgi_json("Interval", root);
	block_timestap = get_cgi_json("TimeStamp", root);

	if(strstr(nvram_safe_get("MULTIFILTER_MAC"), block_mac) == NULL || now - atol(block_timestap) > 0){
		_dprintf("blocking_cgi: not valid blocking request\n");
		if (root != NULL) 
			json_object_put(root);
		return;
	}

	//_dprintf("%s: block_CName = %s, block_mac = %s, block_timestr = \"%ld\", block_timestap = %s\n", __func__, block_CName, block_mac, now, block_timestap);
	g = buf = strdup(nvram_safe_get("MULTIFILTER_TMP"));
	while (buf) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &block_enabled_t, &block_CName_t, &block_mac_t, &block_interval_t, &block_timestap_t)) != 5) continue;

		//_dprintf("%s: block_enabled_t = %s, block_CName_t = %s, block_mac_t = %s , block_interval_t =%s, block_timestap_t = %s\n", __func__, block_enabled_t, block_CName_t, block_mac_t, block_interval_t, block_timestap_t);
		if((now-atol(block_timestap_t)) > 0){
			continue;
		}else if(strstr(nvram_safe_get("MULTIFILTER_MAC"), block_mac_t) == NULL){
			continue;
		}else{
			if(!strcmp(block_mac_t, block_mac))
				retStatus = 1;

			strcat(nvramTmp, "<");
			strcat(nvramTmp, "1");
			strcat(nvramTmp, ">");
			strcat(nvramTmp, block_CName_t);
			strcat(nvramTmp, ">");
			strcat(nvramTmp, block_mac_t);
			strcat(nvramTmp, ">");
			strcat(nvramTmp, block_interval_t);
			strcat(nvramTmp, ">");
			strcat(nvramTmp, block_timestap_t);
		}
	}
	free(buf);
	if (root != NULL) 
		json_object_put(root);

	if(retStatus == 0){
		strcat(nvramTmp, "<");
		strcat(nvramTmp, "1");
		strcat(nvramTmp, ">");
		strcat(nvramTmp, block_CName);
		strcat(nvramTmp, ">");
		strcat(nvramTmp, block_mac);
		strcat(nvramTmp, ">");
		strcat(nvramTmp, block_interval);
		strcat(nvramTmp, ">");
		strcat(nvramTmp, block_timestap);
	}
	nvram_set("MULTIFILTER_TMP", nvramTmp);
	nvram_commit();
	notify_rc("restart_firewall");
}
#else
static void do_blocking_request_cgi(char *url, FILE *stream){}
static void do_blocking_cgi(char *url, FILE *stream){}
#endif

void do_get_timezone_cgi(char *url, FILE *stream){

	struct json_object *root = NULL;
	struct json_object *res = json_object_new_object();
	struct time_zone_list *t;
	char *tz_offset=NULL;
	char *tz_dst=NULL;
	char *lang=NULL;
	char *timezone_t="GMT0";	//default
	char timezone[16]={0};

	memset(timezone, 0, sizeof(timezone));

	do_json_decode(&root);

	tz_offset = safe_get_cgi_json("Timezone_Offset", root);
	tz_dst = safe_get_cgi_json("Timezones_Dst", root);
	lang = safe_get_cgi_json("Timezones_Lang", root);

	if(strcmp(tz_dst, "0") == 0)
		t = timezones;
	else
		t = timezones_dst;

	for (; t->offset; t++){
		if(match(t->offset, tz_offset))
		{
			timezone_t = t->tz_str;
			break;
		}
	}

	if(!strcmp(lang, "BR")){
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}
	else if(!strcmp(lang, "CN")){
		if(!strcmp(timezone_t, "CCT-8"))
			strlcpy(timezone, "CST-8", sizeof(timezone));
	}
	else if(!strcmp(lang, "CZ")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "UTC-1DST_1_1", sizeof(timezone));
	}
	else if(!strcmp(lang, "DA")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MET-1DST", sizeof(timezone));
	}
	else if(!strcmp(lang, "DE")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MEZ-1DST", sizeof(timezone));
	}
	else if(!strcmp(lang, "ES")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MET-1DST_1", sizeof(timezone));
	}
	else if(!strcmp(lang, "FI")){
		if(!strcmp(timezone_t, "UTC-2DST"))
			strlcpy(timezone, "UTC-2DST_3", sizeof(timezone));
	}
	else if(!strcmp(lang, "FR")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MET-1DST_1", sizeof(timezone));
	}
	else if(!strcmp(lang, "HU")){
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}
	else if(!strcmp(lang, "IT")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MEZ-1DST_1", sizeof(timezone));
	}
	else if(!strcmp(lang, "JP")){
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}
	else if(!strcmp(lang, "KR")){
		if(!strcmp(timezone_t, "JST"))
			strlcpy(timezone, "UTC-9_1", sizeof(timezone));
	}
	else if(!strcmp(lang, "MS")){
		if(!strcmp(timezone_t, "CCT-8"))
			strlcpy(timezone, "SST-8", sizeof(timezone));
	}
	else if(!strcmp(lang, "NL")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MEZ-1DST", sizeof(timezone));
	}
	else if(!strcmp(lang, "NO")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MEZ-1DST", sizeof(timezone));
	}
	else if(!strcmp(lang, "PL")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "UTC-1DST_2", sizeof(timezone));
	}
	else if(!strcmp(lang, "RO")){
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}
	else if(!strcmp(lang, "RU")){
		if(!strcmp(timezone_t, "UTC-3_1"))
			strlcpy(timezone, "UTC-3_4", sizeof(timezone));
	}
	else if(!strcmp(lang, "SL")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "UTC-1DST_1_1", sizeof(timezone));
	}
	else if(!strcmp(lang, "SV")){
		if(!strcmp(timezone_t, "UTC-1DST_1"))
			strlcpy(timezone, "MET-1DST", sizeof(timezone));
	}
	else if(!strcmp(lang, "TH")){
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}
	else if(!strcmp(lang, "TR")){
		if(!strcmp(timezone_t, "UTC-3_1"))
			strlcpy(timezone, "UTC-3_6", sizeof(timezone));
	}
	else if(!strcmp(lang, "TW")){
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}
	else if(!strcmp(lang, "UK")){
		if(!strcmp(timezone_t, "UTC-2DST"))
			strlcpy(timezone, "EET-2DST", sizeof(timezone));
	}
	else{
		strlcpy(timezone, timezone_t, sizeof(timezone));
	}

	json_object_object_add(res, "timezone", json_object_new_string(timezone));
	websWrite(stream, "%s\n", json_object_to_json_string(res));

	json_object_put(root);
	json_object_put(res);
}

// 2010.09 James. {
static char no_cache_IE7[] =
"Cache-Control: no-cache, no-store, must-revalidate\r\n"
"Pragma: no-cache\r\n"
"Expires: 0"
;
// 2010.09 James. }

static char no_cache[] =
"Cache-Control: no-cache, no-store, must-revalidate\r\n"
"Pragma: no-cache\r\n"
"Expires: 0"
;

static char syslog_txt[] =
"Content-Disposition: attachment;\r\n"
"filename=syslog.txt"
;

static char cache_object[] =
"Cache-Control: max-age=3600"
;

#ifdef RTCONFIG_USB_MODEM
static char modemlog_txt[] =
"Content-Disposition: attachment;\r\n"
"filename=modemlog.txt"
;

static void
do_modemlog_cgi(char *path, FILE *stream)
{
	char *cmd[] = {"/usr/sbin/3ginfo.sh", NULL};

	unlink("/tmp/3ginfo.txt");
	_eval(cmd, ">/tmp/3ginfo.txt", 0, NULL);

	dump_file(stream, get_modemlog_fname());
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}
#endif

static void
do_log_cgi(char *path, FILE *stream)
{
	dump_file(stream, get_syslog_fname(1));
	dump_file(stream, get_syslog_fname(0));
	fputs("\r\n", stream); /* terminator */
	fputs("\r\n", stream); /* terminator */
}

#ifdef RTCONFIG_FINDASUS
static int
findasus_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *action_mode;
	char *current_url;

	action_mode = websGetVar(wp, "action_mode","");
	current_url = websGetVar(wp, "current_page", "");
	_dprintf("apply: %s %s\n", action_mode, current_url);

	if (!strcmp(action_mode, "refresh_networkmap"))
	{
		printf("@@@ Signal to networkmap!!!\n");
		doSystem("killall -%d networkmap", SIGUSR1);

		websRedirect(wp, current_url);
	}
	return 1;
}


static void
do_findasus_cgi(char *url, FILE *stream)
{
    findasus_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}
#endif

#ifdef RTCONFIG_CAPTIVE_PORTAL
#define JFFS_CUSTOMIZED_SPLASH "/jffs/customized_splash/"
#define INCLUDE_JQUERY "<script type='text/javascript' src='jquery-1.7.1.min.js'></script>\n"
#define INCLUDE_UAM "<script type='text/javascript' src='uam.js'></script>\n"
#define TAG_START "<script>\n"
#define TAG_END "</script>\n"
static int
ej_get_customized_attribute(int eid, webs_t wp, int argc, char **argv) {
	char *profile_id = websGetVar(wp, "profile_id", "");
	char *profile_id_tmp = NULL;
	int from_app = 0;

	if (ejArgs(argc, argv, "%s", &profile_id_tmp) < 1) {
		//_dprintf("name = NULL\n");
	}else if(!strcmp(profile_id, "")){
		profile_id = profile_id_tmp;
		from_app = 1;
	}

	if(strcmp(profile_id, "")) {
		char file_name[64];
		memset(file_name, 0, 64);

		//Check folder exist or not
		if(!check_if_dir_exist(JFFS_CUSTOMIZED_SPLASH))
			mkdir(JFFS_CUSTOMIZED_SPLASH, 0755);

		//Write upload icon value
		sprintf(file_name, "/jffs/customized_splash/%s.json", profile_id);
		if(check_if_file_exist(file_name)) {
			dump_file(wp, file_name);
		}
		else {
			websWrite(wp, "NoData");
		}
	}
	else {
		websWrite(wp, "NoData");
	}
	return 0;
}
static void
do_splash_page_post(char *url, FILE *stream, int len, char *boundary) {
	char upload_fifo[64];
	char upload_html[64];
	char upload_css[64];
	char upload_error[64];
	char replace_script[128];
	FILE *fifo = NULL;
	int ret = EINVAL, ch;
	int offset;
	char *name, *value, *p;

	memset(upload_fifo, 0, sizeof(upload_fifo));
	memset(upload_html, 0, sizeof(upload_html));
	memset(upload_css, 0, sizeof(upload_css));
	memset(upload_error, 0, sizeof(upload_error));
	memset(post_buf, 0, sizeof(post_buf));

	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_CUSTOMIZED_SPLASH))
		mkdir(JFFS_CUSTOMIZED_SPLASH, 0755);

	/* Look for our part */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);

		if (!strncasecmp(post_buf, "Content-Disposition:", 20)) {
			if(strstr(post_buf, "name=\"splash_page_attribute\"")) {
				/* Skip boundary and headers */
				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}

					len -= strlen(post_buf);
					if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
						break;
					}
				}

				if (!(fifo = fopen(upload_fifo, "w"))) {
					goto err;
				}

				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}
					len -= strlen(post_buf);

					if(boundary) {
						if (strstr(post_buf, boundary))
							break;
					}

					fputs(post_buf, fifo);
				}

				ret = 0;

				fclose(fifo);
				fifo = NULL;
				//break;
			}
			else if(strstr(post_buf, "name=\"splash_page_html\"")) {
				/* Skip boundary and headers */
				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}

					len -= strlen(post_buf);
					if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
						break;
					}
				}

				if (!(fifo = fopen(upload_html, "w"))) {
					goto err;
				}

				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}
					len -= strlen(post_buf);

					if(boundary) {
						if (strstr(post_buf, boundary))
							break;
					}

					if(!strncmp(post_buf, "<_INCLUDE_JQUERY_>", 18)) {
						memset(replace_script, 0, sizeof(replace_script));
						snprintf(replace_script, sizeof(replace_script), "%s", INCLUDE_JQUERY);
						fputs(replace_script, fifo);
					}
					else if(!strncmp(post_buf, "<_INCLUDE_UAM_>", 15)) {
						memset(replace_script, 0, sizeof(replace_script));
						snprintf(replace_script, sizeof(replace_script), "%s", INCLUDE_UAM);
						fputs(replace_script, fifo);
					}
					else if(!strncmp(post_buf, "<_TAG_START_>", 13)) {
						memset(replace_script, 0, sizeof(replace_script));
						snprintf(replace_script, sizeof(replace_script), "%s", TAG_START);
						fputs(replace_script, fifo);
					}
					else if(!strncmp(post_buf, "<_TAG_END_>", 11)) {
						memset(replace_script, 0, sizeof(replace_script));
						snprintf(replace_script, sizeof(replace_script), "%s", TAG_END);
						fputs(replace_script, fifo);
					}
					else
						fputs(post_buf, fifo);
				}

				ret = 0;

				fclose(fifo);
				fifo = NULL;
				//break;
			}
			else if(strstr(post_buf, "name=\"splash_page_css\"")) {
				/* Skip boundary and headers */
				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}

					len -= strlen(post_buf);
					if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
						break;
					}
				}

				if (!(fifo = fopen(upload_css, "w"))) {
					goto err;
				}

				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}
					len -= strlen(post_buf);

					if(boundary) {
						if (strstr(post_buf, boundary))
							break;
					}

					fputs(post_buf, fifo);
				}

				ret = 0;

				fclose(fifo);
				fifo = NULL;
				//break;
			}
			else if(strstr(post_buf, "name=\"splash_page_error\"")) {
				/* Skip boundary and headers */
				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}

					len -= strlen(post_buf);
					if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
						break;
					}
				}

				if (!(fifo = fopen(upload_error, "w"))) {
					goto err;
				}

				while (len > 0) {
					if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
						goto err;
					}
					len -= strlen(post_buf);

					if(boundary) {
						if (strstr(post_buf, boundary))
							break;
					}

					fputs(post_buf, fifo);
				}

				ret = 0;

				fclose(fifo);
				fifo = NULL;
				//break;
			}
			else if(strstr(post_buf, "name=\"splash_page_id\"")) {
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				p = post_buf;
				name = strstr(p, "\"") + 1;
				p = strstr(name, "\"");
				strcpy(p++, "\0");
				value = strstr(p, "\r\n\r\n") + 4;
				p = strstr(value, "\r");
				strcpy(p, "\0");
				sprintf(upload_fifo, "/jffs/customized_splash/%s.json", value);
				sprintf(upload_html, "/jffs/customized_splash/%s.html", value);
				sprintf(upload_css, "/jffs/customized_splash/%s.css", value);
			}
		}
	}

	nvram_set("splash_page_status_temp", "1");
	goto normal;

err:
	nvram_set("splash_page_status_temp", "0");
normal:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}
static void
do_splash_page_cgi(char *url, FILE *stream)
{
	websWrite(stream, "<script>parent.call_back_to_save_config(%d);</script>\n", nvram_get_int("splash_page_status_temp"));
	nvram_unset("splash_page_status_temp");
}
static void
do_splash_page_del(char *url, FILE *stream, int len, char *boundary) {
	char del_fifo[64];
	int ret = EINVAL, ch;
	int offset;
	char *name, *value, *p;

	memset(del_fifo, 0, sizeof(del_fifo));
	memset(post_buf, 0, sizeof(post_buf));

	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_CUSTOMIZED_SPLASH))
		mkdir(JFFS_CUSTOMIZED_SPLASH, 0755);

	/* Look for our part */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);

		if (!strncasecmp(post_buf, "Content-Disposition:", 20)) {
			if(strstr(post_buf, "name=\"splash_page_id_del\"")) {
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				offset = strlen(post_buf);
				fgets(post_buf+offset, MIN(len + 1, sizeof(post_buf)-offset), stream);
				len -= strlen(post_buf) - offset;
				p = post_buf;
				name = strstr(p, "\"") + 1;
				p = strstr(name, "\"");
				strcpy(p++, "\0");
				value = strstr(p, "\r\n\r\n") + 4;
				p = strstr(value, "\r");
				strcpy(p, "\0");
				sprintf(del_fifo, "/jffs/customized_splash/%s.html", value);
				eval("rm", "-rf", del_fifo);
				memset(del_fifo, 0, sizeof(del_fifo));
				sprintf(del_fifo, "/jffs/customized_splash/%s.json", value);
				eval("rm", "-rf", del_fifo);
				memset(del_fifo, 0, sizeof(del_fifo));
				sprintf(del_fifo, "/jffs/customized_splash/%s.css", value);
				eval("rm", "-rf", del_fifo);
			}
		}
	}

	nvram_set("splash_page_status_temp", "1");
	goto normal;

err:
	nvram_set("splash_page_status_temp", "0");

normal:
	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}
#endif

/* Base-64 decoding.  This represents binary data as printable ASCII
** characters.  Three 8-bit binary bytes are turned into four 6-bit
** values, like so:
**
**   [11111111]  [22222222]  [33333333]
**
**   [111111] [112222] [222233] [333333]
**
** Then the 6-bit values are represented using the characters "A-Za-z0-9+/".
*/

static int b64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };

/* Do base-64 decoding on a string.  Ignore any non-base64 bytes.
** Return the actual number of bytes generated.  The decoded size will
** be at most 3/4 the size of the encoded, and may be smaller if there
** are padding characters (blanks, newlines).
*/
static int
b64_decode( const char* str, unsigned char* space, int size )
{
    const char* cp;
    int space_idx, phase;
    int d, prev_d=0;
    unsigned char c;

    space_idx = 0;
    phase = 0;
    for ( cp = str; *cp != '\0'; ++cp )
	{
	d = b64_decode_table[(int)*cp];
	if ( d != -1 )
	    {
	    switch ( phase )
		{
		case 0:
		++phase;
		break;
		case 1:
		c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
		if ( space_idx < size )
		    space[space_idx++] = c;
		++phase;
		break;
		case 2:
		c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
		if ( space_idx < size )
		    space[space_idx++] = c;
		++phase;
		break;
		case 3:
		c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
		if ( space_idx < size )
		    space[space_idx++] = c;
		phase = 0;
		break;
		}
	    prev_d = d;
	    }
	}
    return space_idx;
}

static void
do_set_ASUS_EULA_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	do_json_decode(&root);

	char *ASUS_EULA = safe_get_cgi_json("ASUS_EULA", root);
	time_t now;
	char timebuf[100];

	_dprintf("[%s(%d)]ASUS_EULA = %s\n", __FUNCTION__, __LINE__, ASUS_EULA);

	if(!strcmp(ASUS_EULA, "0") || !strcmp(ASUS_EULA, "1"))
	{
		nvram_set("ASUS_EULA", ASUS_EULA);
		now = time( (time_t*) 0 );
		sprintf(timebuf, rfctime(&now));
		nvram_set("ASUS_EULA_time", timebuf);
		if(!strcmp(ASUS_EULA, "0")){
			if(nvram_match("ddns_server_x", "WWW.ASUS.COM")){
				nvram_set("ddns_enable_x", "0");
				nvram_set("ddns_server_x", "");
				nvram_set("ddns_hostname_x", "");
			}

#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
			nvram_set("ifttt_token", "");
#endif
		}

		nvram_commit();
#if defined(RTCONFIG_AIHOME_TUNNEL)
		kill_pidfile_s(MASTIFF_PID_PATH, SIGUSR2);
#endif
	}
	json_object_put(root);
}

static void
do_unreg_ASUSDDNS_cgi(char *url, FILE *stream)
{
	_dprintf("[%s(%d)] notify_rc(start_asusddns_unregister)\n", __FUNCTION__, __LINE__);
	nvram_unset("asusddns_reg_result");
	notify_rc("stop_ddns;start_asusddns_unregister");
}

static void
do_set_TM_EULA_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	do_json_decode(&root);

	char *TM_EULA = safe_get_cgi_json("TM_EULA", root);
	time_t now;
	char timebuf[100];

	_dprintf("[%s(%d)]TM_EULA = %s\n", __FUNCTION__, __LINE__, TM_EULA);

	if(!strcmp(TM_EULA, "0") || !strcmp(TM_EULA, "1"))
	{
		nvram_set("TM_EULA", TM_EULA);
		now = time( (time_t*) 0 );
		sprintf(timebuf, rfctime(&now));
		nvram_set("TM_EULA_time", timebuf);

		if(!strncmp(TM_EULA, "0", 1))
		{
			nvram_set("wrs_protect_enable", "0");
			nvram_set("wrs_enable", "0");
			nvram_set("wrs_app_enable", "0");
			nvram_set("apps_analysis", "0");
			nvram_set("bwdpi_wh_enable", "0");
			nvram_set("bwdpi_db_enable", "0");
			if(nvram_match("qos_enable", "1") && nvram_match("qos_type", "1"))
				nvram_set("qos_enable", "0");
			notify_rc("restart_wrs;restart_qos;restart_firewall");
		}
		nvram_commit();
	}
	json_object_put(root);
}
 
#if defined(RTCONFIG_BWDPI)
static void
do_wrs_wbl_cgi(char *url, FILE *stream)
{
	struct json_object *root = NULL;
	struct json_object *resp = json_object_new_object();
	struct json_object *data = json_object_new_array();
	do_json_decode(&root);

	char *action = safe_get_cgi_json("action", root);
	char *type = safe_get_cgi_json("type", root);
	char *mac = safe_get_cgi_json("mac", root);
	char *url_t = safe_get_cgi_json("url_type", root);
	char *url_s = safe_get_cgi_json("url", root);

	char path[256] = {0};
	char buf[8] = {0};
	char tmp[256] = {0};
	char tmp2[256] = {0};
	int len = sizeof(path);
	int bflag = atoi(type);
	int status = 0;

	char *head = NULL;
	char *end = NULL;
	FILE *fp = NULL;

	_dprintf("[%s(%d)] %s, %d, %s, %s, %s\n", __FUNCTION__, __LINE__, action, bflag, mac, url_t, url_s);

	if (action == NULL) {
		strlcpy(buf, "0", sizeof(buf));
		goto END;
	}

	if (url_t == NULL) {
		url_t = "url";
	}

	if (mac == NULL) {
		mac = "";
	}


	if (!strcmp(action, "add") && url_s != NULL) {
		if (check_xss_blacklist(url_s, 0)) {
			goto END;
		}
		status = WRS_WBL_WRITE_LIST(bflag, mac, url_t, url_s);
		if (status) notify_rc("restart_wrs_wbl");
	}
	else if (!strcmp(action, "del") && url_s != NULL) {
		if (check_xss_blacklist(url_s, 0)) {
			goto END;
		}
		status = WRS_WBL_DEL_LIST(bflag, mac, url_t, url_s);
		if (status) notify_rc("restart_wrs_wbl");
	}
	else if (!strcmp(action, "get")) {
		status = WRS_WBL_GET_PATH(bflag, mac, path, len);
		if (status == 1) {
			if ((fp = fopen(path, "r")) != NULL) {
				while (fgets(tmp2, sizeof(tmp2), fp) != NULL) {
					head = strchr(tmp2, ' ')+1;
					end = strchr(tmp2, 0xa)+1;
					snprintf(tmp, end-head, "%s", head);
					json_object_array_add(data, json_object_new_string(tmp));
				}
			}
		}
		else {
			json_object_array_add(data, json_object_new_string(tmp));
		}
	}

	/* item : success */
	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf), "%d", status);

END:
	json_object_object_add(resp, "success", json_object_new_string(buf));
	json_object_object_add(resp, "data", data);
	websWrite(stream, "%s\n", json_object_to_json_string(resp));
	if (root) json_object_put(root);
	if (resp) json_object_put(resp);
	return;
}
#endif

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
static int
login_cgi(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
		char_t *url, char_t *path, char_t *query)
{
	char *authorization_t;
	char authinfo[500];
	char* authpass;
	int l;
	int authpass_fail = 0;
	char asus_token[32]={0};
	char *next_page=NULL;
	int fromapp_flag = 0;
	char filename[128];
	memset(filename, 0, sizeof(filename));
	memset(asus_token, 0, sizeof(asus_token));

	fromapp_flag = check_user_agent(user_agent);

	next_page = websGetVar(wp, "next_page", "");

	authorization_t = websGetVar(wp, "login_authorization","");

	HTTPD_DBG("authorization_t = %s\n", authorization_t);

	/* Decode it. */
	l = b64_decode( &(authorization_t[0]), (unsigned char*) authinfo, sizeof(authinfo) );
	authinfo[l] = '\0';

	authpass = strchr( authinfo, ':' );
	if ( authpass == (char*) 0 ) {
		authpass_fail = 1;
	}else
		*authpass++ = '\0';

	time_t now;
	time_t dt;
	char timebuf[100];
	now = time( (time_t*) 0 );

	struct in_addr temp_ip_addr;
	char *temp_ip_str;
	
	if(!cur_login_ip_type)
	{
		login_timestamp_tmp = uptime();
		dt = login_timestamp_tmp - last_login_timestamp;
		if(last_login_timestamp != 0 && dt > MAX_LOGIN_BLOCK_TIME){
			login_try = 0;
			last_login_timestamp = 0;
			lock_flag &= ~(LOCK_LOGIN_LAN);
			login_error_status = 0;
		}
		if(login_try >= DEFAULT_LOGIN_MAX_NUM){
			lock_flag |= LOCK_LOGIN_LAN;
			temp_ip_addr.s_addr = login_ip_tmp;
			temp_ip_str = inet_ntoa(temp_ip_addr);
			if(login_try%DEFAULT_LOGIN_MAX_NUM == 0)
				logmessage("httpd login lock", "Detect abnormal logins at %d times. The newest one was from %s in login lock.", login_try, temp_ip_str);

			send_login_page(fromapp_flag, LOGINLOCK, NULL, NULL, dt, LOGINTRY);
			return LOGINLOCK;
		}
	}
	else
	{
		login_timestamp_tmp_wan= uptime();
		dt = login_timestamp_tmp_wan- last_login_timestamp_wan;
		if(last_login_timestamp_wan!= 0 && dt > MAX_LOGIN_BLOCK_TIME){
			login_try_wan= 0;
			last_login_timestamp_wan= 0;
			lock_flag &= ~(LOCK_LOGIN_WAN);
			login_error_status = 0;
		}
		if(login_try_wan>= DEFAULT_LOGIN_MAX_NUM){
			lock_flag |= LOCK_LOGIN_WAN;
			temp_ip_addr.s_addr = login_ip_tmp;
			temp_ip_str = inet_ntoa(temp_ip_addr);
			if(login_try_wan%DEFAULT_LOGIN_MAX_NUM == 0)
				logmessage("httpd login lock", "Detect abnormal logins at %d times. The newest one was from %s in login lock.", login_try_wan, temp_ip_str);

			send_login_page(fromapp_flag, LOGINLOCK, NULL, NULL, dt, LOGINTRY);
			return LOGINLOCK;
		}
	}
	websWrite(wp,"%s %d %s\r\n", PROTOCOL, 200, "OK" );
	websWrite(wp,"Server: %s\r\n", SERVER_NAME );
	if (fromapp_flag != 0){
		websWrite(wp, "Cache-Control: no-store\r\n");
		websWrite(wp, "Pragma: no-cache\r\n");
		if(fromapp_flag == FROM_DUTUtil){
			websWrite(wp, "AiHOMEAPILevel: %d\r\n", EXTEND_AIHOME_API_LEVEL );
			websWrite(wp, "Httpd_AiHome_Ver: %d\r\n", EXTEND_HTTPD_AIHOME_VER );
			websWrite(wp, "Model_Name: %s\r\n", get_productid() );
		}else if(fromapp_flag == FROM_ASSIA){
			websWrite(wp, "ASSIA_API_Level: %d\r\n", EXTEND_ASSIA_API_LEVEL );
		}
	}
	(void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
	websWrite(wp,"Date: %s\r\n", timebuf );
	if (fromapp_flag == 0){
		websWrite(wp,"Content-Type: %s\r\n", "text/html");
	}else{
		websWrite(wp,"Content-Type: %s\r\n", "application/json;charset=UTF-8");
	}

	/* Is this the right user and password? */
	//if (!authpass_fail && nvram_match("http_username", authinfo) && nvram_match("http_passwd", authpass))
	if (!authpass_fail && nvram_match("http_username", authinfo) && compare_passwd_in_shadow(authinfo, authpass))
	{
		HTTPD_DBG("authpass!\n");
		if (fromapp_flag == FROM_BROWSER){
			if(!cur_login_ip_type)
			{
				login_try = 0;
				last_login_timestamp = 0;
			}
			else
			{
				login_try_wan = 0;
				last_login_timestamp_wan = 0;
			}
			set_referer_host();
		}

		generate_token(asus_token, sizeof(asus_token));
		add_asus_token(asus_token);

		HTTPD_DBG("asus_token = %s\n", asus_token);

		websWrite(wp,"Set-Cookie: asus_token=%s; HttpOnly;\r\n",asus_token);
		websWrite(wp,"Connection: close\r\n" );
		websWrite(wp,"\r\n" );
		if (fromapp_flag == 0){

			snprintf(filename, sizeof(filename), "/www/%s", next_page);

			websWrite(wp,"<HTML><HEAD>\n" );
#ifndef RTCONFIG_BCM_MFG
			if(is_passwd_default() && !nvram_match(ATE_FACTORY_MODE_STR(), "1"))
				websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=Main_Password.asp\">\r\n"));
			else
#endif
			{
				if(strncmp(next_page, "cloud_sync.asp", 14)==0)
					websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=%s?flag=%s\">\r\n"), next_page, cloud_file);
				else if(strcmp(next_page, "cfg_onboarding.cgi")==0){
					websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=cfg_onboarding.cgi?flag=AMesh&id=%s\">\r\n"), cloud_file);
				}
				else{
					if(check_xss_blacklist(next_page, 1) || !useful_redirect_page(next_page))
						websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=%s\">\r\n"), indexpage);
					else
						websWrite(wp, T("<meta http-equiv=\"refresh\" content=\"0; url=%s\">\r\n"), next_page);
				}
			}

			websWrite(wp,"</HEAD></HTML>\n" );
		}else{
			websWrite(wp,"{\n" );
			websWrite(wp,"\"asus_token\":\"%s\"\n", asus_token);
			websWrite(wp,"}\n" );
		}
		return 1;
	}else{
		websWrite(wp,"Connection: close\r\n" );
		websWrite(wp,"\r\n" );
		if(!cur_login_ip_type)
		{
			login_try++;
			last_login_timestamp = login_timestamp_tmp;
		}
		else
		{
			login_try_wan++;
			last_login_timestamp_wan = login_timestamp_tmp_wan;
		}
		if((cur_login_ip_type? login_try_wan: login_try) >= DEFAULT_LOGIN_MAX_NUM){
			lock_flag |= (cur_login_ip_type? LOCK_LOGIN_WAN: LOCK_LOGIN_LAN);
			temp_ip_addr.s_addr = login_ip_tmp;
			temp_ip_str = inet_ntoa(temp_ip_addr);
			logmessage("httpd login lock", "Detect abnormal logins at %d times. The newest one was from %s in login.", (cur_login_ip_type? login_try_wan: login_try), temp_ip_str);
#ifdef RTCONFIG_NOTIFICATION_CENTER
			json_object *root = NULL;
			root = json_object_new_object();
			if (root == NULL) {
				_dprintf("[%s(%d)]new json object error", __FUNCTION__, __LINE__);
				char nt_tmp[MAX_EVENT_INFO_LEN];
				snprintf(nt_tmp, sizeof(nt_tmp), "{\"IP\":\"%s\"}", temp_ip_str);
				SEND_NT_EVENT(ADMIN_LOGIN_FAIL_LAN_WEB_EVENT, nt_tmp);
			} else {
				json_object_object_add(root, "IP", json_object_new_string(temp_ip_str));
				SEND_NT_EVENT(ADMIN_LOGIN_FAIL_LAN_WEB_EVENT, json_object_to_json_string(root));
			}
			json_object_put(root);
#endif
			login_error_status = LOGINLOCK;
		}else{
			login_error_status = ACCOUNTFAIL;
		}

		HTTPD_DBG("authfail: login_error_status = %d\n", login_error_status);

		if(fromapp_flag != 0){
			if(login_error_status == LOGINLOCK)
				websWrite(wp, "{\n\"error_status\":\"%d\",\"remaining_lock_time\":\"%ld\"\n}\n", login_error_status, LOCKTIME - login_dt);
			else
				websWrite(wp, "{\n\"error_status\":\"%d\"\n}\n", login_error_status);
		}else{
			websWrite(wp,"<HTML><HEAD>\n" );
			websWrite(wp,"<script>parent.location.href='/Main_Login.asp';</script>\n");
			websWrite(wp,"</HEAD></HTML>\n" );
		}
		return 0;
	}
}

static void
do_login_cgi(char *url, FILE *stream)
{
    login_cgi(stream, NULL, NULL, 0, url, NULL, NULL);
}

#define JSON_MARK_DQ 0
#define JSON_MARK_CB 1

static void
app_call(char *func, FILE *stream, int first_row)
{
	char *args, *end, *next;
	int argc;
	char * argv[16]={NULL};
	struct ej_handler *handler;
	int json_mark = -1;

	/* Parse out ( args ) */
	if (!(args = strchr(func, '(')))
		return;
	if (!(end = unqstrstr_t(func, ")")))
		return;
	*args++ = *end = '\0';
	/* Set up argv list */
	for (argc = 0; argc < 16 && args && *args; argc++, args = next) {
		if (!(argv[argc] = get_arg_t(args, &next)))
			break;
	}

	/* Call handler */
	for (handler = &ej_handlers[0]; handler->pattern; handler++) {
		if (strcmp(handler->pattern, func) == 0){
			if(first_row == 0)
				websWrite(stream, ",\n");

			if(strcmp(func, "nvram_get") == 0 || strcmp(func, "nvram_default_get") == 0){
				if(check_xss_blacklist(argv[0], 0)){
					websWrite(stream,"\"ERROR_NAME\":\"\"");
					break;
				}else{
					websWrite(stream,"\"%s\":", argv[0]);
					json_mark = JSON_MARK_DQ;
				}
			}else if(strcmp(func, "nvram_char_to_ascii") == 0){
				if(check_xss_blacklist(argv[1], 0)){
					websWrite(stream,"\"ERROR_NAME\":\"\"");
					break;
				}else{
					websWrite(stream,"\"%s\":", argv[1]);
					json_mark = JSON_MARK_DQ;
				}

			}else if(argv[0] != NULL && strcmp(argv[0], "appobj") == 0 && strncmp(func, "get_clientlist", 14) != 0){
				websWrite(stream,"\"%s\":", func);
				json_mark = JSON_MARK_CB;
			}else if(argv[0] != NULL && strncmp(func, "get_clientlist", 14) != 0)
				websWrite(stream,"\"%s-%s\":", func, argv[0]);
			else
				websWrite(stream,"\"%s\":", func);

			if(json_mark == JSON_MARK_DQ)
				websWrite(stream,"\"" );
			else if(json_mark == JSON_MARK_CB)
				websWrite(stream,"{" );

			handler->output(0, stream, argc, argv);

			if(json_mark == JSON_MARK_DQ)
				websWrite(stream,"\"" );
			else if(json_mark == JSON_MARK_CB)
				websWrite(stream,"}" );
			break;
		}
	}
}

static void
do_appGet_cgi(char *url, FILE *stream)
{
	int firstRow = 1;
	char *substr = NULL;
	char * delim = ";";

	char *pattern = websGetVar(wp, "hook","");
	char *dup_pattern = strdup(pattern);
	char *sepstr = dup_pattern;

	substr = strsep(&sepstr, delim);

	websWrite(stream,"{\n" );
	while (substr){

		app_call(substr, stream, firstRow);

		if (firstRow == 1)
			firstRow = 0;

		substr = strsep(&sepstr, delim);
	}

	websWrite(stream,"\n}\n" );
	free(dup_pattern);
}

static void
do_appGet_image_path_cgi(char *url, FILE *stream)
{
	char file_path[128] = {0};
	char file_path1[128] = {0};

	websWrite(stream,"{\n" );

#ifdef RTAC68U
	if (is_ac66u_v2_series())
	{
		if(!strcmp(get_productid(), "RP-AC1900")){
			snprintf(file_path, sizeof(file_path), "/images/RP-AC1900");
			snprintf(file_path1, sizeof(file_path), "/images/RP-AC1900");
		}else{
			snprintf(file_path, sizeof(file_path), "/images/RT-AC66U_V2");
			snprintf(file_path1, sizeof(file_path), "/images/RT-AC66U_V2");
		}
	}
	else
#endif
	{
		snprintf(file_path, sizeof(file_path), "/images");
		snprintf(file_path1, sizeof(file_path), "/images/New_ui");
	}

	websWrite(stream, "\"IMAGE_MODEL_PRODUCT\":\"%s%s\",\n", file_path, IMAGE_MODEL_PRODUCT);
	websWrite(stream, "\"IMAGE_WANUNPLUG\":\"%s%s\",\n", file_path, IMAGE_WANUNPLUG);
	websWrite(stream, "\"IMAGE_ROUTER_MODE\":\"%s%s\",\n", file_path1, IMAGE_ROUTER_MODE);
	websWrite(stream, "\"IMAGE_REPEATER_MODE\":\"%s%s\",\n",file_path1, IMAGE_REPEATER_MODE);
	websWrite(stream, "\"IMAGE_AP_MODE\":\"%s%s\",\n",file_path1,  IMAGE_AP_MODE);
	websWrite(stream, "\"IMAGE_MEDIA_BRIDGE_MODE\":\"%s%s\"\n",file_path1, IMAGE_MEDIA_BRIDGE_MODE);

	websWrite(stream,"\n}\n" );
}

static void
do_qis_default(char *url, FILE *stream)
{
	char *flag;
	char redirect_url[128];
	flag = websGetVar(wp, "flag","");
	
	if(flag != NULL && strcmp(flag, "") != 0){
		snprintf(redirect_url, sizeof(redirect_url), "QIS_wizard.htm?flag=%s", flag);
		websRedirect(stream, redirect_url);
	}
	else
#if defined(MAPAC1300) || defined(MAPAC2200) || defined(MAPAC1750)
		websRedirect(stream, "QIS_wizard.htm");
#elif defined(VZWAC1300)
		websRedirect(stream, "message.htm");
#else
		websRedirect(stream, "QIS_wizard.htm?flag=welcome");
#endif
}

static void
do_page_default(char *url, FILE *stream)
{
	char *page;
	page = websGetVar(wp, "url","");
	websRedirect(stream, page);
}

char* reverse_str( char *str )
{
  int i, n;
  char c;

  n = strlen( str );
  for( i=0; i<n/2; i++ )
  {
    c = str[i];
    str[i] = str[n-i-1];
    str[n-i-1] = c;
  }

  return str;
}

#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
static void
do_get_IFTTTPincode_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_get_IFTTTPincode_cgi\n");
#endif
	char pincode[32]={0};
	gen_IFTTTPincode(pincode);
	websWrite(stream,"{\n" );
	websWrite(stream,"\"ifttt_pincode\":\"%s\"\n", pincode);
	websWrite(stream,"}\n" );
}

static void
do_send_IFTTTPincode_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_send_IFTTTPincode_cgi\n");
#endif
	char inviteCode[2104]={0};
	memset(inviteCode,0,sizeof(inviteCode));

	char *asus_flag = websGetVar(wp, "asus_flag","");

	gen_IFTTT_inviteCode(inviteCode, asus_flag);

	send_content_page( 200, "OK", (char*) 0, inviteCode, 0);
}

static void
do_get_IFTTTtoken_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_get_IFTTTtoken_cgi\n");
#endif
	char *stoken;
	char ifttt_token[32]={0};
	int ret = 0;

	memset(ifttt_token,0,sizeof(ifttt_token));

	stoken = websGetVar(wp, "shortToken","");

	 ret = gen_IFTTTtoken(stoken, ifttt_token);

	websWrite(stream,"{\n" );
	websWrite(stream,"\"ifttt_token\":\"%s\",\n", ifttt_token);
	websWrite(stream,"\"error_status\":\"%d\"\n", ret);
	websWrite(stream,"}\n" );
}

static void
do_alexa_block_internet_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_alexa_block_internet_cgi\n");
#endif
	struct json_object *root=NULL;
	char *block_internet = NULL;

	root = json_object_new_object();

	do_json_decode(&root);

	block_internet = safe_get_cgi_json("block_internet", root);

	if(!strcmp(block_internet, "1"))
		alexa_block_internet(1);
	else if(!strcmp(block_internet, "0"))
		alexa_block_internet(0);

	json_object_put(root);
	notify_rc("restart_firewall");
}

#ifdef RTCONFIG_NOTIFICATION_CENTER
static void
enable_nc_wifi_notice(int event_id)
{
	FILE *fp;
	char setConf[2048];
	char eInfo_name[8], tmp[20];
	char *nv, *nvp, *b;
	char *eInfo_value, *eInfo_action, *eInfo_eType;

	memset(setConf, 0, sizeof(setConf));
	nv = nvp = strdup(nvram_safe_get("nc_setting_conf"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			memset(tmp, 0, sizeof(tmp));
			memset(eInfo_name, 0, sizeof(eInfo_name));
			if ((vstrsep(b, ">", &eInfo_value, &eInfo_action, &eInfo_eType) != 3))
				continue;
			snprintf(eInfo_name, sizeof(eInfo_name), "%x",event_id);
			if(!strcmp(eInfo_value, eInfo_name)){
				int eInfo_action_set = atoi(eInfo_action);
				eInfo_action_set = (eInfo_action_set|1<<NC_ACT_IFTTT_BIT);
				snprintf(tmp, sizeof(tmp), "<%s>%d>%s", eInfo_value, eInfo_action_set, eInfo_eType);
			}
			else
				snprintf(tmp, sizeof(tmp), "<%s>%s>%s", eInfo_value, eInfo_action, eInfo_eType);
			strcat(setConf, tmp);
		}
		free(nv);
		nvram_set("nc_setting_conf", setConf);
	}

	if ((fp = fopen(NOTIFY_SETTING_CONF, "w")) == NULL){
		_dprintf("fail to open %s\n", NOTIFY_SETTING_CONF);
		return;
	}

	fprintf(fp,"%s", nvram_safe_get("nc_setting_conf"));
	fclose(fp);

	notify_rc("update_nc_setting_conf");
}

static void
do_nc_exist_wifi_notice_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_nc_exist_wifi_notice_cgi\n");
#endif
	enable_nc_wifi_notice(SYS_EXISTED_DEVICE_WIFI_CONNECTED_EVENT);
}

static void
do_nc_new_wifi_notice_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_nc_new_wifi_notice_cgi\n");
#endif
	enable_nc_wifi_notice(SYS_NEW_DEVICE_WIFI_CONNECTED_EVENT);
}
#endif
#endif

static void
do_enable_remote_control_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_enable_remote_control_cgi\n");
#endif
	int rc_ddns = 0, rc_httpd = 0;
	char ddns_hostname[128];
	char ddns_name[64];
	char service_buf[128];

	memset(ddns_hostname, 0, sizeof(ddns_hostname));
	memset(ddns_name, 0, sizeof(ddns_name));
	memset(service_buf, 0, sizeof(service_buf));

	if (nvram_match("ddns_enable_x", "0")){
		nvram_set("ddns_enable_x", "1");
		rc_ddns = 1;
	}
	if (nvram_match("ddns_hostname_x", "")){
		nvram_set("ddns_server_x", "WWW.ASUS.COM");
#ifdef RTCONFIG_HTTPS
		gen_ddns_hostname(ddns_name);
#endif
		snprintf(ddns_hostname, sizeof(ddns_hostname), "a%s.asuscomm.com", ddns_name);
		nvram_set("ddns_hostname_x", ddns_hostname);
		rc_ddns = 1;
	}

	if (nvram_match("misc_http_x", "0")){
		nvram_set("misc_http_x", "1");
		if (nvram_match("http_enable", "0"))
			nvram_set("http_enable", "2");
		rc_httpd = 1;
	}

#if defined(RTCONFIG_AIHOME_TUNNEL)
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	IFTTT_DEBUG("[HTTPD] do_enable_remote_control_cgi: notice mastiff update\n");
#endif
	kill_pidfile_s(MASTIFF_PID_PATH, SIGUSR1);
#endif

	sprintf(service_buf, "%s%s",
		(rc_ddns) ? "restart_ddns;" : "",
		(rc_httpd) ? "restart_usb_idle;restart_time;restart_httpd;restart_upnp;" : "");

	notify_rc(service_buf);
}

static int search_device_name_in_clientlist(const char *name, struct json_object *json_object_array){
	int ret=0;
	struct json_object *clients=NULL;
	struct json_object *client_name = NULL;
	struct json_object *customList = NULL, *custom_attr_get = NULL, *custom_client_name = NULL;
	int customList_status = 0;

	if(!pids("networkmap") || (clients = json_object_from_file(NMP_CL_JSON_FILE)) == NULL){
		HTTPD_DBG("search_device_name_in_clientlist fail\n");
		return 0;
	}

	customList = json_object_new_object();
	customList_status = get_custom_clientlist_info(customList);

	json_object_object_foreach(clients, key, val) {
		if(customList_status && json_object_object_get_ex(customList, key, &custom_attr_get) && json_object_object_get_ex(custom_attr_get, "name", &custom_client_name)){
			if(!strcasecmp(name, json_object_get_string(custom_client_name)))
				json_object_array_add(json_object_array, json_object_new_string(key));
		}else{
			if(json_object_object_get_ex(val, "name", &client_name)){
				if(!strcasecmp(name, json_object_get_string(client_name)))
					json_object_array_add(json_object_array, json_object_new_string(key));
			}
		}
	}

	ret = json_object_array_length(json_object_array);

	if(customList)
		json_object_put(customList);
	if(clients)
		json_object_put(clients);

	return ret;
}

static int
config_MULTIFILTER_setting(struct json_object *mac_array, char *option){

	struct json_object *multifilter_obj = NULL, *multifilter_attr = NULL;
	struct json_object *multifilter_mac = NULL, *multifilter_enable = NULL, *multifilter_devicename = NULL, *multifilter_daytime = NULL;
	struct json_object *mac_value = NULL, *enable_value = NULL, *name_value = NULL, *daytime_value = NULL;
	char word[4096], *word_next;
	char enable_list[32], mac_list[512] , name_list[1024], daytime_list[4096];
	char enable_list_tmp[32], mac_list_tmp[512] , name_list_tmp[1024], daytime_list_tmp[4096];
	int i=0, j=0, arraylen=0, mac_arraylen=0, ret=1;

	memset(enable_list, 0, sizeof(enable_list));
	memset(mac_list, 0, sizeof(mac_list));
	memset(name_list, 0, sizeof(name_list));
	memset(daytime_list, 0, sizeof(daytime_list));

	multifilter_obj =  json_object_new_object();
	multifilter_mac =  json_object_new_array();
	multifilter_enable =  json_object_new_array();
	multifilter_devicename =  json_object_new_array();
	multifilter_daytime = json_object_new_array();

	foreach_62(word, nvram_safe_get("MULTIFILTER_MAC"), word_next)
		json_object_array_add(multifilter_mac,json_object_new_string(word));
	foreach_62(word, nvram_safe_get("MULTIFILTER_ENABLE"), word_next)
		json_object_array_add(multifilter_enable,json_object_new_string(word));
	foreach_62(word, nvram_safe_get("MULTIFILTER_DEVICENAME"), word_next)
		json_object_array_add(multifilter_devicename,json_object_new_string(word));
	foreach_62(word, nvram_safe_get("MULTIFILTER_MACFILTER_DAYTIME"), word_next)
		json_object_array_add(multifilter_daytime,json_object_new_string(word));

	if(multifilter_mac){
		arraylen = json_object_array_length(multifilter_mac);
		for (i=0; i< arraylen; i++){
			multifilter_attr= json_object_new_object();
			mac_value = json_object_array_get_idx(multifilter_mac, i);
			json_object_object_add(multifilter_attr, "enable", json_object_array_get_idx(multifilter_enable, i));
			json_object_object_add(multifilter_attr, "name", json_object_array_get_idx(multifilter_devicename, i));
			json_object_object_add(multifilter_attr, "daytime", json_object_array_get_idx(multifilter_daytime, i));
			json_object_object_add(multifilter_obj, json_object_get_string(mac_value), multifilter_attr);
		}
	}

	mac_arraylen = json_object_array_length(mac_array);
	for (i=0; i< mac_arraylen; i++){
		multifilter_attr= json_object_new_object();
		mac_value = json_object_array_get_idx(mac_array, i);
		if(json_object_object_get_ex(multifilter_obj, json_object_get_string(mac_value), &multifilter_attr)){
			if(atoi(option) == 1){
				json_object_object_add(multifilter_attr, "daytime", json_object_new_string("<"));
				json_object_object_add(multifilter_attr, "enable", json_object_new_string("1"));
			}
			else if(atoi(option) == 0)
				json_object_object_del(multifilter_obj, json_object_get_string(mac_value));
		}
		else{
			if(atoi(option) == 1){
				multifilter_attr= json_object_new_object();
				json_object_object_add(multifilter_attr, "enable", json_object_new_string("1"));
				json_object_object_add(multifilter_attr, "name", json_object_new_string("New device"));
				json_object_object_add(multifilter_attr, "daytime", json_object_new_string("<"));
				json_object_object_add(multifilter_obj, json_object_get_string(mac_value), multifilter_attr);
			}
		}
	}

	if(json_object_object_length(multifilter_obj) > 16){
		ret = 0;
		goto FINISH;
	}

	json_object_object_foreach(multifilter_obj, key, val) {
		json_object_object_get_ex(val, "enable", &enable_value);
		json_object_object_get_ex(val, "name", &name_value);
		json_object_object_get_ex(val, "daytime", &daytime_value);
		if(j == 0){
			strlcpy(mac_list, key, sizeof(mac_list));
			strlcpy(enable_list, (enable_value)?json_object_get_string(enable_value):"1", sizeof(enable_list));
			strlcpy(name_list, (name_value)?json_object_get_string(name_value):"New device", sizeof(name_list));
			strlcpy(daytime_list, (daytime_value)?json_object_get_string(daytime_value):"<", sizeof(daytime_list));
		}else{
			snprintf(mac_list_tmp, sizeof(mac_list_tmp), "%s>%s", mac_list, key);
			strlcpy(mac_list, mac_list_tmp, sizeof(mac_list));
			snprintf(enable_list_tmp, sizeof(enable_list_tmp), "%s>%s", enable_list, (enable_value)?json_object_get_string(enable_value):"1");
			strlcpy(enable_list, enable_list_tmp, sizeof(enable_list));
			snprintf(name_list_tmp, sizeof(name_list_tmp), "%s>%s", name_list, (name_value)?json_object_get_string(name_value):"New device");
			strlcpy(name_list, name_list_tmp, sizeof(name_list));
			snprintf(daytime_list_tmp, sizeof(daytime_list_tmp), "%s>%s", daytime_list, (daytime_value)?json_object_get_string(daytime_value):"<");
			strlcpy(daytime_list, daytime_list_tmp, sizeof(daytime_list));
		}
		j++;
	}
	nvram_set("MULTIFILTER_ALL", "1");
	nvram_set("MULTIFILTER_MAC", mac_list);
	nvram_set("MULTIFILTER_ENABLE", enable_list);
	nvram_set("MULTIFILTER_DEVICENAME", name_list);
	nvram_set("MULTIFILTER_MACFILTER_DAYTIME", daytime_list);
	nvram_commit();

FINISH:
	if(multifilter_obj)
		json_object_put(multifilter_obj);
	if(multifilter_mac)
		json_object_put(multifilter_mac);
	if(multifilter_enable)
		json_object_put(multifilter_enable);
	if(multifilter_devicename)
		json_object_put(multifilter_devicename);
	if(multifilter_daytime)
		json_object_put(multifilter_daytime);

	return ret;
}

static void
do_block_device_internet_cgi(char *url, FILE *stream)
{
	struct json_object *root=NULL;
	struct json_object *block_mac_list=NULL;
	char *block_mac=NULL, *enabled=NULL, *block_name=NULL, *block_option = NULL;
	char word[4096], *word_next;
	int arraylen=0, ret=0;

	do_json_decode(&root);
	block_option = safe_get_cgi_json("block_option", root); //name/mac
	enabled = safe_get_cgi_json("enabled", root);

	if(!isValidEnableOption(enabled, 1)){
		HTTPD_DBG("invalid enabled option\n");
		ret = 4001;
		goto FINISH;
	}

	block_mac_list = json_object_new_array();

	if(!strcmp(block_option, "name")){
		block_name = safe_get_cgi_json("block_name", root);
		arraylen = search_device_name_in_clientlist(block_name, block_mac_list);
		if(arraylen == 0){
			ret = 4002;
			HTTPD_DBG("block name not found.\n");
			goto FINISH;
		}
		HTTPD_DBG("block_mac_list = %s\n", json_object_to_json_string(block_mac_list));

	}else if(!strcmp(block_option, "mac")){
		block_mac = safe_get_cgi_json("block_mac", root);
		foreach_62(word, block_mac, word_next){
			if(!isValidMacAddress(word)){
				ret = 4003;
				HTTPD_DBG("invalid mac\n");
				goto FINISH;
			}else
				json_object_array_add(block_mac_list,json_object_new_string(word));
		}
	}

	ret = config_MULTIFILTER_setting(block_mac_list, enabled);

	if(ret == 1)
		notify_rc("restart_firewall");
	else
		HTTPD_DBG("over max limit items\n");

FINISH:
	websWrite(stream,"{\n" );
	if(ret == 1 || ret == 0){
		websWrite(stream,"\"statusCode\":\"%d\"\n", (ret)?200:4004);
		websWrite(stream,",\"block_list\":%s\n", json_object_to_json_string(block_mac_list));
	}
	else
		websWrite(stream,"\"statusCode\":\"%d\"\n", ret);
	websWrite(stream,"}\n" );

	if(block_mac_list)
		json_object_put(block_mac_list);
	if(root)
		json_object_put(root);
}

static void
do_check_Auth_cgi(char *url, FILE *stream)
{
	websWrite(stream,"{\n" );
	websWrite(stream,"\"asus_auth\":\"OK\"\n");
	websWrite(stream,"}\n" );
}

static void
do_auto_guestnetwork_cgi(char *url, FILE *stream)
{
#if defined(RTCONFIG_NOTIFICATION_CENTER) && (defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA))
	if(check_user_agent(user_agent) == FROM_IFTTT || check_user_agent(user_agent) == FROM_ALEXA)
		IFTTT_DEBUG("[HTTPD] do_auto_guestnetwork_cgi\n");
#endif
	int unit = 0, subunit = -1, band_num = 0;
	int from_app = 0;
	char prefix[]="wlXXXXXX_", tmp[100]={0};
	unsigned char mac_binary[6]={0};
	unsigned char ssid[32]={0};
	char key[32]={0};
	char *macp = NULL, *wl_expire = NULL, *wl_bss_enabled = NULL;
	struct json_object *root=NULL, *res=NULL;
	char word[256]={0}, *next=NULL;

	res = json_object_new_object();
	root = json_object_new_object();
	do_json_decode(&root);

	from_app = check_user_agent(user_agent);
	wl_expire = safe_get_cgi_json("wl_expire", root);
	wl_bss_enabled = safe_get_cgi_json("wl_bss_enabled", root);
	band_num = num_of_wl_if();
	macp = get_2g_hwaddr();
	ether_atoe(macp, mac_binary);
	gen_guestnetwork_pass(key, sizeof(key));

	foreach(word, nvram_safe_get("wl_ifnames"), next) {

		if(nvram_contains_word("rc_support", "lyra_hide")){
			subunit = 1;
			if(unit > 0) break;
		}else
			subunit = num_of_mssid_support(unit);

		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);

		if(from_app == FROM_IFTTT)
			sprintf((char *) ssid, "%s_IFTTT%s_Guest", SSID_PREFIX, unit ? (unit == 2 ? "_5G-2" : (band_num > 2 ? "_5G-1" : "_5G")) : "");
		else if(from_app == FROM_ALEXA)
			sprintf((char *) ssid, "%s_Alexa%s_Guest", SSID_PREFIX, unit ? (unit == 2 ? "_5G-2" : (band_num > 2 ? "_5G-1" : "_5G")) : "");

		nvram_set(strcat_r(prefix, "ssid", tmp), (char *) ssid);
		nvram_set(strcat_r(prefix, "wpa_psk", tmp), key);
		nvram_set(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2");
		nvram_set(strcat_r(prefix, "crypto", tmp), "aes");
		nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), "3600");
		nvram_set(strcat_r(prefix, "lanaccess", tmp), "off");
		nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
		if(!strcmp(wl_expire, "")){
			nvram_set(strcat_r(prefix, "expire", tmp), "10800");
			nvram_set(strcat_r(prefix, "expire_tmp", tmp), "10800");
		}
		else{
			nvram_set(strcat_r(prefix, "expire", tmp), wl_expire);
			nvram_set(strcat_r(prefix, "expire_tmp", tmp), wl_expire);
		}

		if(!strcmp(wl_bss_enabled, ""))
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
		else
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), wl_bss_enabled);

		unit++;
	}

	response_nvram_config(stream, "auto_GuestNetwork", res, root);
	websWrite(stream, "{\"auto_guestnetwork\":%s}\n", json_object_to_json_string(res));

	if(res)
		json_object_put(res);
	if(root)
		json_object_put(root);

#ifdef RTCONFIG_LANTIQ
	wave_handle_app_flag("auto_guestnetwork", wave_app_flag);
#endif
	notify_rc("restart_wireless");
}

void
response_nvram_config(webs_t wp, char *config_name, json_object *res, json_object *root){

	if (!strcmp(config_name, "auto_GuestNetwork")) {

		int  unit=0, subunit=0;
		char *value = NULL;
		char word[256]={0}, *next=NULL;
		char prefix[32], tmp[100];
		struct nvram_config *t;

		foreach(word, nvram_safe_get("wl_ifnames"), next) {
			if(nvram_contains_word("rc_support", "lyra_hide")){
				subunit = 1;
				if(unit > 0) break;
			}else
				subunit = num_of_mssid_support(unit);

			snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
			for (t=guestnetwork_conf; t->name; t++){
				if(!strncmp(t->name, "wl_", 3))
					strcat_r(prefix, t->name+3, tmp);
				else
					strlcpy(tmp, t->name, sizeof(tmp));
				value = nvram_safe_get(tmp);
				//_dprintf("response_nvram_config: value = %s\n", value);
				json_object_object_add(res, tmp, json_object_new_string(value));
			}
			unit++;
		}
	}
}

#ifdef RTCONFIG_AMAS
static void
do_cfg_onboarding_cgi(char *url, FILE *stream)
{
	struct json_object *cfg=NULL;

	do_json_decode(&cfg);

	char url_str[128] = {0};
	char event_msg[128] = {0};
	char inviteCode[256]={0};
	char *re_mac = get_cgi_json("re_mac", cfg);
	char *new_re_mac = get_cgi_json("new_re_mac", cfg);
	char *id = safe_get_cgi_json("id", cfg);
	char *flag = get_cgi_json("flag", cfg);

	if(strcmp(id, "donot_search") != 0){
		if (!re_mac && !new_re_mac)
			snprintf(event_msg, sizeof(event_msg), HTTPD_OB_AVAILABLE_MSG,
				EID_HTTPD_ONBOARDING, OB_TYPE_AVAILABLE);
		else if (re_mac && new_re_mac)
			snprintf(event_msg, sizeof(event_msg), HTTPD_OB_LOCK_MSG,
				EID_HTTPD_ONBOARDING, OB_TYPE_LOCKED, re_mac, new_re_mac);

		if (strlen(event_msg))
			send_cfgmnt_event(event_msg);
	}
	snprintf(url_str, sizeof(url_str), "%s?flag=%s&id=%s", indexpage, (!check_xss_blacklist(flag, 0))?flag:"",(!check_xss_blacklist(id, 0))?id:"");

	if(cfg)	json_object_put(cfg);
	memset(cloud_file, 0, sizeof(cloud_file));

	snprintf(inviteCode, sizeof(inviteCode), "<meta http-equiv=\"refresh\" content=\"0; url=%s\">\r\n", url_str);
	send_content_page( 200, "OK", (char*) 0, inviteCode, 0);
}
#endif
static void
do_cleanlog_cgi(char *url, FILE *stream) {
	char *path;
	path = websGetVar(wp, "path","");
	if (strlen(path) <= 0) {
		printf("No \"path\"!\n");
	}
	else if(strcmp(path,"web_history") == 0 ) {
		notify_rc("clean_web_history");
	}
	else if(strcmp(path,"traffic_analyzer") == 0 ) {
		notify_rc("clean_traffic_analyzer");
	}
	else if (strcmp(path, "clean_backup_log") == 0) {
		// TODO : add path here
	}
}

void update_wlan_log(int sig){
	FILE *fp;

	if((fp = fopen("/tmp/wlanlog.txt", "w")) != NULL) {
		ej_wl_status_2g(0, fp, 0, NULL);
		fclose(fp);
		sig = 1;
		nvram_set("fb_wlanlog_done", "1");
	}
}

static void
do_update_wlanlog_cgi(char *url, FILE *stream) {

	int ret = 0;
	update_wlan_log(ret);
	websWrite(stream, "{\"statusCode\":\"%d\"}", (ret)?200:400);
}

static int
delete_qos_rulelist(char *rog_mac, char *qos_rulelist_tmp, size_t len, int *num){

	int ret = 0;
	char *desc, *addr, *port, *prio, *transferred, *proto;
	char *buf, *g, *p;
	g = buf = strdup(nvram_safe_get("qos_rulelist"));
	while (buf) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;

		if(!strcmp(addr, rog_mac)){
			ret=1;
			continue;
		}
		else{
			*num = *num + 1;
			strlcat(qos_rulelist_tmp, "<", len);
			strlcat(qos_rulelist_tmp, desc, len);
			strlcat(qos_rulelist_tmp, ">", len);
			strlcat(qos_rulelist_tmp, addr, len);
			strlcat(qos_rulelist_tmp, ">", len);
			strlcat(qos_rulelist_tmp, port, len);
			strlcat(qos_rulelist_tmp, ">", len);
			strlcat(qos_rulelist_tmp, proto, len);
			strlcat(qos_rulelist_tmp, ">", len);
			strlcat(qos_rulelist_tmp, transferred, len);
			strlcat(qos_rulelist_tmp, ">", len);
			strlcat(qos_rulelist_tmp, prio, len);
		}
	}
	free(buf);
	return ret;
}

static int
delete_rog_clientlist(char *rog_mac, char *rog_clientlist_tmp,  size_t len){

	int ret=0;
	char *word_next;
	char word[1024]={0};

	foreach_60(word, nvram_safe_get("rog_clientlist"), word_next){
		if(!strcmp(word, rog_mac)){
			ret = 1;
			continue;
		}
		else{
			strlcat(rog_clientlist_tmp, "<", len);
			strlcat(rog_clientlist_tmp, word, len);
		}
	}
	return ret;
}

static void
do_rog_first_qos_cgi(char *url, FILE *stream)
{
	int ret;
	struct json_object *root=NULL;
	char *action=NULL, *rog_mac=NULL;

	int rule_num=0, rog_clientlist_find=0, qos_rulelist_find=0;
	char rog_clientlist[1024]={0}, rog_clientlist_tmp[1024]={0}, qos_rulelist[1024]={0}, qos_rulelist_tmp[1024]={0};

	root = json_object_new_object();
	do_json_decode(&root);

	rog_mac = safe_get_cgi_json("rog_mac", root);
	action = safe_get_cgi_json("action", root);

	if(!isValidMacAddress(rog_mac)){
		HTTPD_DBG("invalid mac\n");
		ret = 4003;
		goto FINISH;
	}

	/* clear rog mac */
	qos_rulelist_find = delete_qos_rulelist(rog_mac, qos_rulelist_tmp, sizeof(qos_rulelist_tmp), &rule_num);
	rog_clientlist_find = delete_rog_clientlist(rog_mac, rog_clientlist_tmp, sizeof(rog_clientlist_tmp));
	if(!strcmp(action, "add")){
		if(rule_num <32){
			snprintf(rog_clientlist, sizeof(rog_clientlist), "<%s%s", rog_mac, rog_clientlist_tmp);
			snprintf(qos_rulelist, sizeof(qos_rulelist), "<>%s>>any>>0%s", rog_mac, qos_rulelist_tmp);
			nvram_set("rog_clientlist", rog_clientlist);
			nvram_set("qos_rulelist", qos_rulelist);
			nvram_set("qos_enable", "1");
			nvram_set("qos_type", "0");
			if(nvram_get_int("qos_obw") == 0)
				nvram_set("qos_obw", "1048576");
			if(nvram_get_int("qos_ibw") == 0)
				nvram_set("qos_ibw", "1048576");
			nvram_commit();
			ret = 2001;
		}else{
			HTTPD_DBG("Over Max Limit\n");
			ret = 4000;
			goto FINISH;
		}
	}else if(!strcmp(action, "delete")){
		if(rog_clientlist_find || qos_rulelist_find){
			nvram_set("rog_clientlist", rog_clientlist_tmp);
			nvram_set("qos_rulelist", qos_rulelist_tmp);
			nvram_commit();
			ret = 2002;
		}
		else{
			ret = 2003;
			goto FINISH;
		}
	}else{
		HTTPD_DBG("invalid action\n");
		ret = 4001;
		goto FINISH;
	}

	notify_rc("restart_qos;restart_firewall");
FINISH:
	json_object_put(root);
	websWrite(stream, "{\"statusCode\":\"%d\"}", ret);
}

static void
do_clean_offline_clientlist_cgi(char *url, FILE *stream)
{
	unlink(NMP_CL_JSON_FILE);
}

#if defined(RTCONFIG_AMAZON_WSS)
static void
do_amazon_wss_cgi(char *url, FILE *stream)
{
	int ret=200;
	struct json_object *root=NULL;
	char *wss_enable=NULL, *do_rc=NULL;

	do_json_decode(&root);

	wss_enable = safe_get_cgi_json("wss_enable", root);
	do_rc = safe_get_cgi_json("do_rc", root);

	if(!isValidEnableOption(wss_enable, 1)){
		HTTPD_DBG("invalid enabled option\n");
		ret = 4001;
		goto FINISH;
	}

	if(!isValidEnableOption(do_rc, 1)){
		HTTPD_DBG("invalid enabled option\n");
		ret = 4002;
		goto FINISH;
	}

	if(num_of_mssid_support(0) < 2){
		HTTPD_DBG("mssid is not support\n");
		ret = 4003;
		goto FINISH;
	}

	amazon_wss_enable(wss_enable, do_rc);

FINISH:
	if(root)
		json_object_put(root);
	websWrite(stream, "{\"statusCode\":\"%d\"}", ret);
}
#endif

#ifdef RTCONFIG_INTERNETCTRL
/* check timestamp */
static int
isValidtimestamp(char *timestamp)
{
	time_t ts;
	char *ptr=NULL;

	ts = (time_t)strtol(timestamp, &ptr, 10);
	if((0 < ts && ts < 2145888000L) && (ptr && !strcasecmp(ptr, "Z")))
		return 1;
	else
		return 0;
}

static void
do_internet_ctrl_cgi(char *url, FILE *stream) {

	struct json_object *ci_obj=NULL;
	char icfilter_mac_tmp[512]={0}, *icfilter_mac_next=NULL;
	char icfilter_daytime_tmp[512]={0}, *icfilter_daytime_next=NULL;
	char icfilter_mac[512]={0}, icfilter_macfilter_daytime[512]={0};
	char icfilter_mac_buf[512]={0}, icfilter_daytime_buf[512]={0}, daytime_rule[32]={0}, daytime_rule_tmp[32]={0};
	int i=0, ret=200, rule_num=0, mac_idx=0, daytime_idx=0;
	unsigned int schedule_start_tmp=0;

	do_json_decode(&ci_obj);

	char *device_mac = get_cgi_json("device_mac", ci_obj);
	char *networkAccess = get_cgi_json("networkAccess", ci_obj);
	char *schedule_start = safe_get_cgi_json("schedule_start", ci_obj);
	char *schedule_end = safe_get_cgi_json("schedule_end", ci_obj);
	char *schedule_duration = safe_get_cgi_json("schedule_duration", ci_obj);

	strlcpy(icfilter_mac, nvram_safe_get("ICFILTER_MAC"), sizeof(icfilter_mac));
	strlcpy(icfilter_macfilter_daytime, nvram_safe_get("ICFILTER_MACFILTER_DAYTIME"), sizeof(icfilter_macfilter_daytime));

	if(!isValidMacAddress(device_mac)){
		HTTPD_DBG("invalid mac\n");
		ret = 4002;
		goto FINISH;
	}

	if(!isValidEnableOption(networkAccess, 1)){
		HTTPD_DBG("invalid enabled option\n");
		ret = 4003;
		goto FINISH;
	}

	foreach_62(icfilter_mac_tmp, icfilter_mac, icfilter_mac_next)
		rule_num++;

	//generate daytime rule
	if(strlen(schedule_start)){
		if(isValidtimestamp(schedule_start))
			snprintf(daytime_rule, sizeof(daytime_rule), "T%s%s", networkAccess, schedule_start);
		else{
			ret = 4006;
			goto FINISH;
		}
	}

	if(strlen(schedule_end)){
		if(isValidtimestamp(schedule_end)){
			snprintf(daytime_rule_tmp, sizeof(daytime_rule), "<T%d%s", atoi(networkAccess)?0:1, schedule_end);
			strlcat(daytime_rule, daytime_rule_tmp, sizeof(daytime_rule));
		}else{
			ret = 4006;
			goto FINISH;
		}

	}else if(strlen(schedule_duration)){
		for (i=0 ; schedule_duration[i] != '\0'; i++){
			if (i>10080 && !isdigit(schedule_duration[i])){
				HTTPD_DBG("invalid schedule_duration\n");
				ret = 4006;
				goto FINISH;
			}
		}
		if(atoi(schedule_duration) != 0)
		{
			if (sscanf(schedule_start, "%uZ", &schedule_start_tmp) != 1){
				HTTPD_DBG("invalid schedule_start\n");
				ret = 4006;
				goto FINISH;
			}else{
				snprintf(daytime_rule_tmp, sizeof(daytime_rule_tmp), "<T%d%uZ", atoi(networkAccess)?0:1, schedule_start_tmp+atoi(schedule_duration)*60);
				strlcat(daytime_rule, daytime_rule_tmp, sizeof(daytime_rule));
			}
		}
	}

	if (strstr(icfilter_mac, device_mac)==NULL)	//add new device rule
	{
		if(rule_num > 16){	//over rule limit to 16
			HTTPD_DBG("over rule limit\n");
			ret = 4000;
			goto FINISH;
		}else{
			snprintf(icfilter_mac_buf, sizeof(icfilter_mac_buf), "%s%s%s", icfilter_mac, (rule_num>0)?">":"", device_mac);
			snprintf(icfilter_daytime_buf, sizeof(icfilter_daytime_buf), "%s%s%s", icfilter_macfilter_daytime, (rule_num>0)?">":"", daytime_rule);
			nvram_set("ICFILTER_MAC", icfilter_mac_buf);
			nvram_set("ICFILTER_MACFILTER_DAYTIME", icfilter_daytime_buf);
		}
	}else{	//device mac already exist in ICFILTER_MAC
		foreach_62(icfilter_mac_tmp, icfilter_mac, icfilter_mac_next)
		{
			mac_idx++;
			if(!strcmp(icfilter_mac_tmp, device_mac)){
				foreach_62(icfilter_daytime_tmp, icfilter_macfilter_daytime, icfilter_daytime_next){
					daytime_idx++;
					if(mac_idx == daytime_idx){
						if(daytime_idx > 1)
							strlcat(icfilter_daytime_buf, ">", sizeof(icfilter_daytime_buf));
						strlcat(icfilter_daytime_buf, daytime_rule, sizeof(icfilter_daytime_buf));
					}
					else{
						if(daytime_idx > 1)
							strlcat(icfilter_daytime_buf, ">", sizeof(icfilter_daytime_buf));
						strlcat(icfilter_daytime_buf, icfilter_daytime_tmp, sizeof(icfilter_daytime_buf));
					}
				}
				break;
			}
		}
		nvram_set("ICFILTER_MACFILTER_DAYTIME", icfilter_daytime_buf);
	}
	notify_rc("restart_firewall");
FINISH:
	websWrite(stream, "{\"statusCode\":\"%d\"}", ret);
	if(ci_obj)
		json_object_put(ci_obj);
}
#endif

//2008.08 magic{
struct mime_handler mime_handlers[] = {
	{ "Main_Login.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "Nologin.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "error_page.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "blocking.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
#ifdef RTCONFIG_WIFI_SON
	{ "message.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
#endif
	{ "gotoHomePage.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ure_success.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "ureip.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "remote.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "js/jquery.js", "text/javascript", cache_object, NULL, do_file, NULL }, // 2010.09 James.
	{ "js/ouiDB.js", "text/javascript", cache_object, NULL, do_file, NULL },
	{ "js/chart.min.js", "text/javascript", cache_object, NULL, do_file, NULL },
	{ "require/require.min.js", "text/javascript", no_cache_IE7, NULL, do_file, NULL },
	{ "httpd_check.xml", "text/xml", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "repage.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "chdom.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
#ifdef RTCONFIG_AMAS
	{ "chcap.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "cfg_onboarding.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_cfg_onboarding_cgi, do_auth },
#endif
	{ "wlc_status.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "get_webdavInfo.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "appGet_image_path.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_appGet_image_path_cgi, NULL },
	{ "login.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_login_cgi, NULL },
	{ "update_clients.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "update_networkmapd.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "update_customList.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "manifest.appcache", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "offline.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "wcdma_list.js", "text/javascript", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "help_content.js", "text/javascript", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "httpd_check.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "manifest.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "update_cloudstatus.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "update_applist.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "update_appstate.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "WAN_info.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "detwan.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_detwan_cgi, do_auth },
	{ "message.htm", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, NULL },
	{ "fonts/*.ttf", "application/x-font-ttf", no_cache_IE7, NULL, do_file, NULL },
	{ "fonts/*.otf", "application/x-font-otf", no_cache_IE7, NULL, do_file, NULL },
	{ "fonts/*.woff", "application/font-woff", no_cache_IE7, NULL, do_file, NULL },
#ifdef RTCONFIG_FINDASUS
	{ "findasus.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_findasus_cgi, do_auth },
	{ "find_device.asp", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
#endif
	{ "blocking_request.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_blocking_request_cgi, NULL },
	{ "blocking.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_blocking_cgi, do_auth },
	{ "get_timezone.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_get_timezone_cgi, do_auth },
	{ "**.xml", "text/xml", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.htm*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.asp*", "text/html", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.appcache", "text/cache-manifest", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "nt_content.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
#ifdef RTCONFIG_DSL_TCLINUX
	{ "TCC.log.*", "application/octet-stream", NULL, NULL, do_diag_log_file, do_auth },
#endif
	{ "**.part.*", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.gz", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.tgz", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.zip", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.ipk", "application/octet-stream", NULL, NULL, do_file, NULL },
	{ "**.css", "text/css", cache_object, NULL, do_file, NULL },
#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
	{ "images/New_ui/asustitle.png", "image/png", no_cache_IE7, do_html_post_and_get, do_file, NULL },
	{ "images/New_ui/smh_step_2_text.png", "image/png", no_cache_IE7, do_html_post_and_get, do_file, NULL },
	{ "images/New_ui/smh_ifttt.png", "image/png", no_cache_IE7, do_html_post_and_get, do_file, NULL },
#endif
	{ "**.png", "image/png", cache_object, NULL, do_file, NULL },
	{ "**.gif", "image/gif", cache_object, NULL, do_file, NULL },
	{ "**.jpg", "image/jpeg", cache_object, NULL, do_file, NULL },
	// Viz 2010.08
	{ "**.svg", "image/svg+xml", NULL, NULL, do_file, NULL },
	{ "**.swf", "application/x-shockwave-flash", NULL, NULL, do_file, NULL  },
	{ "**.htc", "text/x-component", NULL, NULL, do_file, NULL  },
	// end Viz
#ifdef TRANSLATE_ON_FLY
	/* Only general.js and quick.js are need to translate. (reduce translation time) */
	{ "general.js|quick.js",  "text/javascript", no_cache_IE7, NULL, do_ej, do_auth },
#endif //TRANSLATE_ON_FLY

	{ "**.js", "text/javascript", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.json", "application/json", no_cache_IE7, do_html_post_and_get, do_ej, do_auth },
	{ "**.cab", "text/txt", NULL, NULL, do_file, do_auth },
	{ "**.CFG", "application/force-download", NULL, do_html_post_and_get, do_prf_file, do_auth },
	{ "uploadIconFile.tar", "application/force-download", NULL, NULL, do_uploadIconFile_file, do_auth },
	{ "networkmap.tar", "application/force-download", NULL, NULL, do_networkmap_file, do_auth },
	{ "upnp.log", "application/force-download", NULL, NULL, do_upnp_file, do_auth },
	{ "upnpc_xml.log", "application/force-download", NULL, NULL, do_upnpc_xml_file, do_auth },
	{ "mDNSNetMonitor.log", "application/force-download", NULL, NULL, do_dnsnet_file, do_auth },
	{ "ftpServerTree.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_ftpServerTree_cgi, do_auth },//andi
	{ "**.ovpn", "application/force-download", NULL, NULL, do_prf_ovpn_file, do_auth },
	{ "QIS_default.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_qis_default, do_auth },
	{ "page_default.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_page_default, do_auth },
	{ "apply.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "applyapp.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_apply_cgi, do_auth },
	{ "appGet.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_appGet_cgi, do_auth },
	{ "upgrade.cgi*", "text/html", no_cache_IE7, do_upgrade_post, do_upgrade_cgi, do_auth},
	{ "upload.cgi*", "text/html", no_cache_IE7, do_upload_post, do_upload_cgi, do_auth },
	{ "set_ASUS_EULA.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_set_ASUS_EULA_cgi, do_auth },
	{ "set_TM_EULA.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_set_TM_EULA_cgi, do_auth },
#if defined(RTCONFIG_BWDPI)
	{ "wrs_wbl.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_wrs_wbl_cgi, do_auth },
#endif
	{ "unreg_ASUSDDNS.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_unreg_ASUSDDNS_cgi, do_auth },
#ifdef RTCONFIG_HTTPS
	{ "upload_cert_key.cgi*", "text/html", no_cache_IE7, do_upload_cert_key, do_upload_cert_key_cgi, do_auth },
#endif
	{ "cert_key.tar", "application/force-download", NULL, do_html_post_and_get, do_download_cert_key_cgi, do_auth },
#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
	{ "get_IFTTTPincode.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_get_IFTTTPincode_cgi, do_auth },
	{ "send_IFTTTPincode.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_send_IFTTTPincode_cgi, do_auth },
	{ "get_IFTTTtoken.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_get_IFTTTtoken_cgi, NULL },
	{ "alexa_block_internet.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_alexa_block_internet_cgi, do_auth },
#ifdef RTCONFIG_NOTIFICATION_CENTER
	{ "nc_new_wifi_notice.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_nc_new_wifi_notice_cgi, do_auth },
	{ "nc_exist_wifi_notice.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_nc_exist_wifi_notice_cgi, do_auth },
#endif
#endif
	{ "block_device_internet.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_block_device_internet_cgi, do_auth },
	{ "enable_remote_control.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_enable_remote_control_cgi, do_auth },
	{ "check_Auth.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_check_Auth_cgi, do_auth },
	{ "auto_guestnetwork.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_auto_guestnetwork_cgi, do_auth },
	{ "syslog.txt*", "application/force-download", syslog_txt, do_html_post_and_get, do_log_cgi, do_auth },
#ifdef RTCONFIG_QTN  //RT-AC87U
	{ "tmp/qtn_diagnostics.cgi*", "application/force-download", NULL, NULL, do_qtn_diagnostics, do_auth },
#endif
#ifdef RTCONFIG_USB_MODEM
	{ "modemlog.txt*", "application/force-download", modemlog_txt, do_html_post_and_get, do_modemlog_cgi, do_auth },
#endif
#ifdef RTCONFIG_TCPDUMP
	{ "udhcpc.pcap*", "application/force-download", NULL, NULL, do_file, NULL },
	{ "**.pcap*", "application/force-download", NULL, NULL, do_file, NULL },
#endif
#ifdef RTCONFIG_DSL
	{ "dsllog.cgi*", "text/txt", no_cache_IE7, do_html_post_and_get, do_adsllog_cgi, do_auth },
#endif
	// Viz 2010.08 vvvvv
	{ "update.cgi*", "text/javascript", no_cache_IE7, do_html_post_and_get, do_update_cgi, do_auth }, // jerry5
	{ "bwm/*.gz", NULL, no_cache, do_html_post_and_get, wo_bwmbackup, do_auth }, /* jerry5 */
	// end Viz  ^^^^^^^^
	{ "**.pac", "application/x-ns-proxy-autoconfig", NULL, NULL, do_file, NULL },
	{ "wpad.dat", "application/x-ns-proxy-autoconfig", NULL, NULL, do_file, NULL },
#ifdef TRANSLATE_ON_FLY
	{ "change_lang.cgi*", "text/html", no_cache_IE7, do_lang_post, do_lang_cgi, do_auth },
	{ "change_location.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_change_location_cgi, do_auth },
#endif //TRANSLATE_ON_FLY
#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2))
	{ "backup_jffs.tar", "application/force-download", NULL, NULL, do_jffs_file, do_auth },
	{ "jffsupload.cgi*", "text/html", no_cache_IE7, do_jffsupload_post, do_jffsupload_cgi, do_auth },
#endif
#ifdef RTCONFIG_OPENVPN
	{ "vpnupload.cgi*", "text/html", no_cache_IE7, do_vpnupload_post, do_vpnupload_cgi, do_auth },
	{ "server_ovpn.cert", "application/force-download", NULL, do_html_post_and_get, do_server_ovpn_file, do_auth },
	{ "upload_server_ovpn_cert.cgi*", "text/html", no_cache_IE7, do_upload_server_ovpn_cert_post, do_upload_server_ovpn_cert_cgi, do_auth },
#endif
#ifdef RTCONFIG_CAPTIVE_PORTAL
	{ "jffs/customized_splash/**.json", "application/json", NULL, NULL, do_file, NULL }, /* */
	{ "splash_page.cgi*", "text/html", no_cache_IE7, do_splash_page_post, do_splash_page_cgi, do_auth },
	{ "splash_page_del.cgi*", "text/html", no_cache_IE7, do_splash_page_del, do_splash_page_cgi, do_auth },
#endif
#ifdef RTCONFIG_IPSEC
	{ "ipsec.log", "application/force-download", NULL, NULL, do_ipsec_file, do_auth },
	{ "clear_file.cgi*", "text/javascript", no_cache_IE7, do_html_post_and_get, do_clear_file_cgi, do_auth },
	{ "ipsecupload.cgi*", "text/html", no_cache_IE7, do_ipsecupload_post, do_ipsecupload_cgi, do_auth },
	{ "caupload.cgi*", "text/html", no_cache_IE7, do_caupload_post, NULL, do_auth },
#endif
	{ "deleteOfflineClient.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_deleteOfflineClient_cgi, do_auth },
#ifdef RTCONFIG_QCA_PLC_UTILS
	{ "plc.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_plc_cgi, do_auth },
#endif
#if defined(RTCONFIG_LP5523) || defined(MAPAC1750)
	{ "lp55xx.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_lp55xx_cgi, do_auth },
#endif
#if defined(RTCONFIG_WIFI_SON)
	{ "athX_state.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_athX_state_cgi, do_auth },
#endif

#ifdef RTCONFIG_NETOOL
	{ "netool.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_netool_cgi, do_auth },
#endif
#if defined(HND_ROUTER) && defined(RTCONFIG_VISUALIZATION)
	{ "json.cgi*", "application/json", no_cache_IE7, (void *) vis_do_json_set, vis_do_json_get, do_auth },
	{ "visdata.db*", "application/force-download", NULL, (void *) vis_do_visdbdwnld_cgi, NULL, do_auth },
#endif
#if 0 // obsoleted, RTCONFIG_RGBLED
	{ "aurargb.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_aurargb_cgi, do_auth },
#endif
	{ "cleanlog.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_cleanlog_cgi, do_auth },
	{ "update_wlanlog.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_update_wlanlog_cgi, do_auth },
	{ "rog_first_qos.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_rog_first_qos_cgi, do_auth },
	{ "clean_offline_clientlist.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_clean_offline_clientlist_cgi, do_auth },
	{ "feedback_mail.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_feedback_mail_cgi, do_auth },
	{ "dfb_log.cgi", "application/force-download", NULL, do_html_post_and_get, do_dfb_log_file, do_auth },
	{ "set_fw_path.cgi", "text/html", no_cache_IE7, do_html_post_and_get, do_set_fw_path_cgi, do_auth },
#if defined(RTCONFIG_AMAZON_WSS)
	{ "amazon_wss.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_amazon_wss_cgi, do_auth },
#endif
#ifdef RTCONFIG_INTERNETCTRL
	{ "internet_ctrl.cgi*", "text/html", no_cache_IE7, do_html_post_and_get, do_internet_ctrl_cgi, do_auth },
#endif
	{ NULL, NULL, NULL, NULL, NULL, NULL }
};

struct except_mime_handler except_mime_handlers[] = {
	{ "QIS_default.cgi", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "page_default.cgi", MIME_EXCEPTION_NOAUTH_FIRST|MIME_EXCEPTION_NORESETTIME},
	{ "images/New_ui/login_bg.png", MIME_EXCEPTION_MAINPAGE},
	{ "images/New_ui/icon_titleName.png", MIME_EXCEPTION_MAINPAGE},
#ifdef RTCONFIG_WIFI_SON
	{ "QIS_*", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "qis/*", MIME_EXCEPTION_NOAUTH_FIRST}, /* */
	{ "*.css", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "state.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "popup.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "general.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "help.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "help_content.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "validator.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "form.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "alttxt.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "start_autodet.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "start_dsl_autodet.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "start_apply.htm", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "start_apply2.htm", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "apply.cgi", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "status.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "automac.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "result_of_get_changed_status_QIS.asp", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "js/support_site.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "client_function.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "notification.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "require/modules/timeZone.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "js/verge.min.js", MIME_EXCEPTION_NOAUTH_FIRST},
	{ "js/jquery.mobile.js", MIME_EXCEPTION_NOAUTH_FIRST},
#ifdef RTCONFIG_DETWAN
	{ "detwan.cgi", MIME_EXCEPTION_NOAUTH_FIRST},
#endif
	{ "athX_state.cgi", MIME_EXCEPTION_NOAUTH_FIRST},
#endif
	{ NULL, 0 }
};

int ej_get_all_accounts(int eid, webs_t wp, int argc, char **argv){
#if defined(RTCONFIG_PERMISSION_MANAGEMENT) || defined(RTCONFIG_USB)
	int acc_num, first;
#endif
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	PMS_ACCOUNT_INFO_T *account_list, *follow_account;
	int group_num;
	PMS_ACCOUNT_GROUP_INFO_T *group_list;
#elif defined(RTCONFIG_USB)
	int i;
	char **account_list = NULL;
	char ascii_user[64];
#endif
	websWrite(wp, "[");

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0){
		printf("Failed to get the account list!\n");
		PMS_FreeAccInfo(&account_list, &group_list);
		return -1;
	}

	first = 1;
	for(follow_account = account_list; follow_account != NULL; follow_account = follow_account->next){
		if(first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%s\"", follow_account->name);
	}

	PMS_FreeAccInfo(&account_list, &group_list);
#elif defined(RTCONFIG_USB)
	if(get_account_list(&acc_num, &account_list) < 0){
		printf("Failed to get the account list!\n");
		free_2_dimension_list(&acc_num, &account_list);
		return -1;
	}

	first = 1;
	for(i = 0; i < acc_num; ++i){
		if(first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		memset(ascii_user, 0, 64);
		char_to_ascii_safe(ascii_user, account_list[i], 64);

		websWrite(wp, "\"%s\"", ascii_user);
	}

	free_2_dimension_list(&acc_num, &account_list);
#else
	websWrite(wp, "\"%s\"", nvram_safe_get("http_username"));
#endif
	websWrite(wp, "]");
	return 0;
}

//2008.08 magic}
#ifdef RTCONFIG_USB
int ej_get_AiDisk_status(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int sh_num;
	char **folder_list = NULL;
	int first_pool, result, i;

	websWrite(wp, "function get_cifs_status(){\n");
	//websWrite(wp, "    return %d;\n", nvram_get_int("samba_running"));
	websWrite(wp, "    return %d;\n", nvram_get_int("enable_samba"));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_ftp_status(){\n");
	//websWrite(wp, "    return %d;\n", nvram_get_int("ftp_running"));
	websWrite(wp, "    return %d;\n", nvram_get_int("enable_ftp"));
	websWrite(wp, "}\n\n");

#ifdef RTCONFIG_WEBDAV_PENDING
	websWrite(wp, "function get_webdav_status(){\n");
	//websWrite(wp, "    return %d;\n", nvram_get_int("ftp_running"));
	websWrite(wp, "    return %d;\n", nvram_get_int("enable_webdav"));
	websWrite(wp, "}\n\n");
#endif

	websWrite(wp, "function get_dms_status(){\n");
	websWrite(wp, "    return %d;\n", pids("ushare"));
	websWrite(wp, "}\n\n");

	websWrite(wp, "function get_share_management_status(protocol){\n");
	websWrite(wp, "    if(protocol == \"cifs\")\n");
	websWrite(wp, "        return %d;\n", (nvram_get("st_samba_force_mode") == NULL && nvram_get_int("st_samba_mode") == 1)?4:nvram_get_int("st_samba_mode"));
	websWrite(wp, "    else if(protocol == \"ftp\")\n");
	websWrite(wp, "        return %d;\n", (nvram_get("st_ftp_force_mode") == NULL && nvram_get_int("st_ftp_mode") == 1)?2:nvram_get_int("st_ftp_mode"));
#ifdef RTCONFIG_WEBDAV_PENDING
	websWrite(wp, "    else if(protocol == \"webdav\")\n");
	websWrite(wp, "        return %d;\n", nvram_get_int("st_webdav_mode"));
#endif
	websWrite(wp, "    else\n");
	websWrite(wp, "        return -1;\n");
	websWrite(wp, "}\n\n");

	disks_info = read_disk_data();
	if (disks_info == NULL){
		websWrite(wp, "function get_sharedfolder_in_pool(poolName){}\n");
		return -1;
	}
	first_pool = 1;
	websWrite(wp, "function get_sharedfolder_in_pool(poolName){\n");
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				websWrite(wp, "    ");

				if (first_pool == 1)
					first_pool = 0;
				else
					websWrite(wp, "else ");

#ifdef OLD_AIDISK
				websWrite(wp, "if(poolName == \"%s\"){\n", rindex(follow_partition->mount_point, '/')+1);
#else
				websWrite(wp, "if(poolName == \"%s\"){\n", follow_partition->device);
#endif
				websWrite(wp, "        return [\"\"");

				result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
				if (result < 0){
					websWrite(wp, "];\n");
					websWrite(wp, "    }\n");

					printf("get_AiDisk_status: Can't get the folder list in \"%s\".\n", follow_partition->mount_point);

					free_2_dimension_list(&sh_num, &folder_list);

					continue;
				}

				for (i = 0; i < sh_num; ++i){
					websWrite(wp, ", ");

					websWrite(wp, "\"%s\"", folder_list[i]);

				}

				websWrite(wp, "];\n");
				websWrite(wp, "    }\n");
			}
		}

	websWrite(wp, "}\n\n");

	if (disks_info != NULL){
		free_2_dimension_list(&sh_num, &folder_list);
		free_disk_data(&disks_info);
	}

	return 0;
}

int
count_sddev_mountpoint()
{
	FILE *procpt = NULL;
	char line[PATH_MAX], devname[32], mpname[32], system_type[10], mount_mode[PATH_MAX];
	int dummy1, dummy2, count = 0;

	if((procpt = fopen(MOUNT_FILE, "r")) != NULL){
		while(fgets(line, sizeof(line), procpt)){
			if(sscanf(line, "%s %s %s %s %d %d", devname, mpname, system_type, mount_mode, &dummy1, &dummy2) != 6)
				continue;

			if(strstr(devname, "/dev/sd"))
				count++;
		}
	}

	if(procpt)
		fclose(procpt);

	return count;
}

static int notify_rc_for_nas(char *cmd)
{
	notify_rc_and_wait(cmd);
	return 0;
}

int stor_dev_busy(const char *devname)
{
	int busy = 1;
	FILE *fp = fopen( "/proc/diskstats", "r" );
	char buf[128];
	int major, minor;
	char name[32];
	unsigned long rio, rmerge, wio, wmerge;
	unsigned long long rsect, wsect;
	unsigned int ruse, wuse, running, use, aveq;
	int count = 3;
	unsigned int aveq_old = 0;

	if (fp) {
LOOP_AGAIN:
		memset(buf, 0, sizeof(buf));
		fseek(fp, 0, SEEK_SET);
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			if (sscanf(buf, "%4d %7d %s %lu %lu %llu %u %lu %lu %llu %u %u %u %u\n", &major, &minor, name, &rio, &rmerge, &rsect, &ruse, &wio, &wmerge, &wsect, &wuse, &running, &use, &aveq) != 14)
				break;

			if (!strcmp(devname, name)) {
				if (running) {
					break;
				} else if (--count > 0) {
					if (count == 2)
						aveq_old = aveq;
					usleep(100 * 1000);
					goto LOOP_AGAIN;
				} else if (aveq_old == aveq) {
					busy = 0;
					break;
				}
			}
		}
	
		fclose(fp);
	}

	return busy;
}

int usb_port_stor_busy(const char *port_path)
{
	int busy = 0;
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char d_port[16], *d_dot;

	if (atoi(port_path) == -1)
		return -1;

	disk_list = read_disk_data();
	if(disk_list == NULL) {
		printf("Can't get any disk's information.\n");
		return -1;
	}

	for (disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next) {
		/* If hub port number is not specified in port_path,
		 * don't compare it with hub port number in disk_info->port.
		 */
		strlcpy(d_port, disk_info->port, sizeof(d_port));
		d_dot = strchr(d_port, '.');
		if (!strchr(port_path, '.') && d_dot)
			*d_dot = '\0';

		if (strcmp(d_port, port_path))
			continue;

		for (partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next) {
			if (partition_info->mount_point != NULL) {
				if (stor_dev_busy(partition_info->device)) {
					busy = 1;
					break;
				}
			}
		}
	}
	free_disk_data(&disk_list);

	return busy;
}

int ej_usb_port_stor_act(int eid, webs_t wp, int argc, char **argv)
{
	int retval = 0;
	disk_info_t *disks_info, *follow_disk;
	int first = 1;

	retval += websWrite(wp, "[");

	disks_info = read_disk_data();
	if (disks_info != NULL) {
		for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
			if (first == 1)
				first = 0;
			else
				retval += websWrite(wp, ", ");
			retval += websWrite(wp, "\"%d\"", usb_port_stor_busy(follow_disk->port));
		}

		free_disk_data(&disks_info);
	}

	retval += websWrite(wp, "]");

	return retval;
}

static int ej_safely_remove_disk(int eid, webs_t wp, int argc, char_t **argv){

	int result;
	struct json_object *root=NULL;

	do_json_decode(&root);

	char *disk_port = get_cgi_json("disk", root);
//	disk_info_t *disks_info = NULL, *follow_disk = NULL;
//	int disk_num = 0;
	int part_num = 0;
	char *fn = "safely_remove_disk_error";

	csprintf("disk_port = %s\n", disk_port);

	if (usb_port_stor_busy(disk_port) == 1)
		csprintf("disk_port %s is busy\n", disk_port);

	if(!strcmp(disk_port, "all")){
		result = eval("/sbin/ejusb", "-1", "0");
	}
	else{
		result = eval("/sbin/ejusb", disk_port, "0");
	}

	if (result != 0){
		insert_hook_func(wp, fn, "alert_msg.Action9");
		json_object_put(root);
		return -1;
	}

/*
	disks_info = read_disk_data();
	for(follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_num)
		;
	free_disk_data(&disks_info);
	csprintf("disk_num = %d\n", disk_num);
*/

	part_num = count_sddev_mountpoint();
	csprintf("part_num = %d\n", part_num);

//	if (disk_num > 1)
	if (part_num > 0)
	{
		result = eval("/sbin/check_proc_mounts_parts");
		result = notify_rc_for_nas("restart_nasapps");
	}
	else
	{
		result = notify_rc_for_nas("stop_nasapps");
	}

	insert_hook_func(wp, "safely_remove_disk_success", "");

	json_object_put(root);
	return 0;
}

int draw_permissions_of_pms(webs_t wp, const char *const account, const disk_info_t *const disks_info, const int is_group){
	disk_info_t *follow_disk;
	partition_info_t *follow_partition;
	int sh_num = 0;
	char **folder_list;
	int samba_right, ftp_right;
#ifdef RTCONFIG_WEBDAV_PENDING
	int webdav_right;
#endif
	int result, j;
	int first_pool;
	char target[8];
	char *ptr_account = NULL;
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	char char_user[64];
#else
	char ascii_user[64];
#endif

	if(is_group)
		snprintf(target, 8, "group");
	else
		snprintf(target, 8, "account");

	if(account == NULL){ // share mode.
		websWrite(wp, "    if(%s == null){\n", target);
	}
	else{
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
		memset(char_user, 0, sizeof(char_user));
		ascii_to_char_safe(char_user, account, sizeof(char_user));
		ptr_account = char_user;

		websWrite(wp, "    else if(%s == \"%s\"){\n", target, account);
#else
		memset(ascii_user, 0, sizeof(ascii_user));
		char_to_ascii_safe(ascii_user, account, sizeof(ascii_user));
		ptr_account = (char *) account;

		websWrite(wp, "    else if(%s == \"%s\"){\n", target, ascii_user);
#endif
	}

	first_pool = 1;
	for(follow_disk = (disk_info_t *)disks_info; follow_disk != NULL; follow_disk = follow_disk->next){
		for(follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next){
			if(follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				if(first_pool == 1)
					first_pool = 0;
				else
					websWrite(wp, "else ");

#ifdef OLD_AIDISK
				websWrite(wp, "        if(pool == \"%s\"){\n", rindex(follow_partition->mount_point, '/')+1);
#else
				websWrite(wp, "        if(pool == \"%s\"){\n", follow_partition->device);
#endif

				websWrite(wp, "            return [");

				// Pool's permission.
				samba_right = get_permission(ptr_account, follow_partition->mount_point, NULL, "cifs", is_group);
				if(samba_right < 0 || samba_right > 3){
					printf("Can't get the CIFS permission abount the pool: %s!\n", follow_partition->device);

					if(ptr_account == NULL || !strcmp(ptr_account, nvram_safe_get("http_username")))
						samba_right = DEFAULT_SAMBA_RIGHT;
					else
						samba_right = 0;
				}

				ftp_right = get_permission(ptr_account, follow_partition->mount_point, NULL, "ftp", is_group);
				if(ftp_right < 0 || ftp_right > 3){
					printf("Can't get the FTP permission abount the pool: %s!\n", follow_partition->device);

					if(ptr_account == NULL || !strcmp(ptr_account, nvram_safe_get("http_username")))
						ftp_right = DEFAULT_FTP_RIGHT;
					else
						ftp_right = 0;
				}

#ifdef RTCONFIG_WEBDAV_PENDING
				webdav_right = get_permission(ptr_account, follow_partition->mount_point, NULL, "webdav", is_group);
				if(webdav_right < 0 || webdav_right > 3){
					printf("Can't get the WEBDAV  permission abount the pool: %s!\n", follow_partition->device);

					if(ptr_account == NULL || !strcmp(ptr_account, nvram_safe_get("http_username")))
						webdav_right = DEFAULT_WEBDAV_RIGHT;
					else
						webdav_right = 0;
				}

				websWrite(wp, "[\"\", %d, %d, %d]", samba_right, ftp_right, webdav_right);
#else
				websWrite(wp, "[\"\", %d, %d]", samba_right, ftp_right);
#endif

				result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
				if(result == 0 && sh_num > 0)
					websWrite(wp, ",\n");

				if(result != 0){
					websWrite(wp, "];\n");
					websWrite(wp, "        }\n");

					printf("get_permissions_of_%s1: Can't get all folders in \"%s\".\n", target, follow_partition->mount_point);

					free_2_dimension_list(&sh_num, &folder_list);
					continue;
				}

				// Folder's permission.
				for(j = 0; j < sh_num; ++j){
					samba_right = get_permission(ptr_account, follow_partition->mount_point, folder_list[j], "cifs", is_group);
					ftp_right = get_permission(ptr_account, follow_partition->mount_point, folder_list[j], "ftp", is_group);
#ifdef RTCONFIG_WEBDAV_PENDING
					webdav_right = get_permission(ptr_account, follow_partition->mount_point, folder_list[j], "webdav", is_group);
#endif

					if(samba_right < 0 || samba_right > 3){
						printf("Can't get the CIFS permission abount \"%s\"!\n", folder_list[j]);

						if(ptr_account == NULL || !strcmp(ptr_account, nvram_safe_get("http_username")))
							samba_right = DEFAULT_SAMBA_RIGHT;
						else
							samba_right = 0;
					}

					if(ftp_right < 0 || ftp_right > 3){
						printf("Can't get the FTP permission abount \"%s\"!\n", folder_list[j]);

						if(ptr_account == NULL || !strcmp(ptr_account, nvram_safe_get("http_username")))
							ftp_right = DEFAULT_FTP_RIGHT;
						else
							ftp_right = 0;
					}

#ifdef RTCONFIG_WEBDAV_PENDING
					if(webdav_right < 0 || webdav_right > 3){
						printf("Can't get the WEBDAV permission abount \"%s\"!\n", folder_list[j]);

						if(ptr_account == NULL || !strcmp(ptr_account, nvram_safe_get("http_username")))
							webdav_right = DEFAULT_WEBDAV_RIGHT;
						else
							webdav_right = 0;
					}
#endif

#ifdef RTCONFIG_WEBDAV_PENDING
					websWrite(wp, "                    [\"%s\", %d, %d, %d]", folder_list[j], samba_right, ftp_right, webdav_right);
#else
					websWrite(wp, "                    [\"%s\", %d, %d]", folder_list[j], samba_right, ftp_right);
#endif

					if(j != sh_num-1)
						websWrite(wp, ",\n");
				}
				free_2_dimension_list(&sh_num, &folder_list);

				websWrite(wp, "];\n");
				websWrite(wp, "        }\n");
			}
		}
	}

	websWrite(wp, "    }\n");

	return 0;
}

int ej_get_permissions_of_account(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info;
	int acc_num = 0;
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	PMS_ACCOUNT_INFO_T *account_list, *follow_account;
	int group_num;
	PMS_ACCOUNT_GROUP_INFO_T *group_list;
#else
	int i;
	char **account_list = NULL;
#endif

	disks_info = read_disk_data();
	if(disks_info == NULL){
		websWrite(wp, "function get_account_permissions_in_pool(account, pool){return [];}\n");
		return -1;
	}

	websWrite(wp, "function get_account_permissions_in_pool(account, pool){\n");

	// share mode.
	draw_permissions_of_pms(wp, NULL, disks_info, 0);

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0){
		printf("1. Can't get the account list.\n");
		free_disk_data(&disks_info);
		PMS_FreeAccInfo(&account_list, &group_list);
		return 0;
	}

	for(follow_account = account_list; follow_account != NULL; follow_account = follow_account->next){
		draw_permissions_of_pms(wp, follow_account->name, disks_info, 0);
	}
	PMS_FreeAccInfo(&account_list, &group_list);
#else
	if(get_account_list(&acc_num, &account_list) < 0){
		printf("1. Can't get the account list.\n");
		free_disk_data(&disks_info);
		free_2_dimension_list(&acc_num, &account_list);
		return 0;
	}

	for(i = 0; i < acc_num; ++i){
		draw_permissions_of_pms(wp, account_list[i], disks_info, 0);
	}
	free_2_dimension_list(&acc_num, &account_list);
#endif

	websWrite(wp, "}\n\n");

	if(disks_info != NULL)
		free_disk_data(&disks_info);

	return 0;
}

int ej_set_account_permission(int eid, webs_t wp, int argc, char **argv){
	char mount_path[PATH_MAX];
	char *ascii_user = websGetVar(wp, "account", NULL);
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", NULL);
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
#ifdef RTCONFIG_WEBDAV_PENDING
	char *webdavproxy = websGetVar(wp, "acc_webdavproxy", "");
#endif
	int right;
	char char_user[64];
	char *fn = "set_account_permission_error";

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_account(char_user) != 1){
		insert_hook_func(wp, fn, "alert_msg.Input6");
		return -1;
	}

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")
#ifdef RTCONFIG_WEBDAV_PENDING
			&& strcmp(protocol, "webdav")
#endif
			){
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (strlen(permission) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input12");
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		insert_hook_func(wp, fn, "alert_msg.Input13");
		return -1;
	}

	if (set_permission(char_user, mount_path, folder, protocol, right, 0) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else {
		logmessage("wedavproxy right", "%s %s %s %s %d %s", char_user, mount_path, folder, protocol, right, webdavproxy);
		// modify permission for webdav proxy
		nvram_set("acc_webdavproxy", webdavproxy);
	}
#endif

#ifdef RTCONFIG_WEBDAV_PENDING
	if(strcmp(protocol, "webdav")==0) {
		if(notify_rc_for_nas("restart_webdav") != 0) {
			insert_hook_func(wp, fn, "alert_msg.Action1");
			return -1;
		}
	}
	else
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}

	insert_hook_func(wp, "set_account_permission_success", "");

	return 0;
}

int ej_set_account_all_folder_permission(int eid, webs_t wp, int argc, char **argv)
{
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int i, result, sh_num;
	char **folder_list;
	char *ascii_user = websGetVar(wp, "account", NULL);
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
#ifdef RTCONFIG_WEBDAV_PENDING
	char *webdavproxy = websGetVar(wp, "acc_webdavproxy", "");
#endif
	int right;
	char char_user[64];
	char *fn = "set_account_all_folder_permission_error";

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_account(char_user) != 1){
		insert_hook_func(wp, fn, "alert_msg.Input6");
		return -1;
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")
#ifdef RTCONFIG_WEBDAV_PENDING
&& strcmp(protocol, "webdav")
#endif
){
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (strlen(permission) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input12");
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		insert_hook_func(wp, fn, "alert_msg.Input13");
		return -1;
	}


	disks_info = read_disk_data();
	if (disks_info == NULL){
		insert_hook_func(wp, fn, "alert_msg.System2");
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point == NULL || strlen(follow_partition->mount_point) <= 0)
				continue;

			result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
			if (result != 0) {
				insert_hook_func(wp, fn, "alert_msg.Action7");
				free_2_dimension_list(&sh_num, &folder_list);
				return -1;
			}
			for (i = 0; i < sh_num; ++i) {
				if (set_permission(char_user, follow_partition->mount_point, folder_list[i], protocol, right, 0) < 0){
					insert_hook_func(wp, fn, "alert_msg.Action1");
					free_2_dimension_list(&sh_num, &folder_list);
					return -1;
				}
#ifdef RTCONFIG_WEBDAV_PENDING
#error FIXME
				else {
					logmessage("wedavproxy right", "%s %s %s %s %d %s", char_user, mount_path, folder, protocol, right, webdavproxy);
					// modify permission for webdav proxy
					nvram_set("acc_webdavproxy", webdavproxy);
				}
#endif
			}
			free_2_dimension_list(&sh_num, &folder_list);
		}
	}

	free_disk_data(&disks_info);

#ifdef RTCONFIG_WEBDAV_PENDING
	if(strcmp(protocol, "webdav")==0) {
		if(notify_rc_for_nas("restart_webdav") != 0) {
			insert_hook_func(wp, fn, "alert_msg.Action1");
			return -1;
		}
	}
	else
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}

	insert_hook_func(wp, "set_account_all_folder_permission_success", "");

	return 0;
}

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
int ej_set_group_permission(int eid, webs_t wp, int argc, char **argv){
	char mount_path[PATH_MAX];
	char *ascii_user = websGetVar(wp, "account", NULL);
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", NULL);
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
#ifdef RTCONFIG_WEBDAV_PENDING
	char *webdavproxy = websGetVar(wp, "acc_webdavproxy", "");
#endif
	int right;
	char char_user[64];
	char *fn = "set_group_permission_error";

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_group(char_user) != 1){
		insert_hook_func(wp, fn, "alert_msg.Input6");
		return -1;
	}

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")
#ifdef RTCONFIG_WEBDAV_PENDING
&& strcmp(protocol, "webdav")
#endif
){
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (strlen(permission) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input12");
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		insert_hook_func(wp, fn, "alert_msg.Input13");
		return -1;
	}

	if (set_permission(char_user, mount_path, folder, protocol, right, 1) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else {
		logmessage("wedavproxy right", "%s %s %s %s %d %s", char_user, mount_path, folder, protocol, right, webdavproxy);
		// modify permission for webdav proxy
		nvram_set("acc_webdavproxy", webdavproxy);
	}
#endif

#ifdef RTCONFIG_WEBDAV_PENDING
	if(strcmp(protocol, "webdav")==0) {
		if(notify_rc_for_nas("restart_webdav") != 0) {
			insert_hook_func(wp, fn, "alert_msg.Action1");
			return -1;
		}
	}
	else
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}

	insert_hook_func(wp, "set_group_permission_success", "");

	return 0;
}

int ej_set_group_all_folder_permission(int eid, webs_t wp, int argc, char **argv)
{
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	int i, result, sh_num;
	char **folder_list;
	char *ascii_user = websGetVar(wp, "account", NULL);
	char *protocol = websGetVar(wp, "protocol", "");
	char *permission = websGetVar(wp, "permission", "");
#ifdef RTCONFIG_WEBDAV_PENDING
	char *webdavproxy = websGetVar(wp, "acc_webdavproxy", "");
#endif
	int right;
	char char_user[64];
	char *fn = "set_group_all_folder_permission_error";

	memset(char_user, 0, 64);
	ascii_to_char_safe(char_user, ascii_user, 64);

	if (test_if_exist_group(char_user) != 1){
		insert_hook_func(wp, fn, "alert_msg.Input6");
		return -1;
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strcmp(protocol, "cifs") && strcmp(protocol, "ftp") && strcmp(protocol, "dms")
#ifdef RTCONFIG_WEBDAV_PENDING
&& strcmp(protocol, "webdav")
#endif
){
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (strlen(permission) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input12");
		return -1;
	}
	right = atoi(permission);
	if (right < 0 || right > 3){
		insert_hook_func(wp, fn, "alert_msg.Input13");
		return -1;
	}


	disks_info = read_disk_data();
	if (disks_info == NULL){
		insert_hook_func(wp, fn, "alert_msg.System2");
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next) {
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next) {
			if (follow_partition->mount_point == NULL || strlen(follow_partition->mount_point) <= 0)
				continue;

			result = get_all_folder(follow_partition->mount_point, &sh_num, &folder_list);
			if (result != 0) {
				insert_hook_func(wp, fn, "alert_msg.Action7");
				free_2_dimension_list(&sh_num, &folder_list);
				return -1;
			}
			for (i = 0; i < sh_num; ++i) {
				if (set_permission(char_user, follow_partition->mount_point, folder_list[i], protocol, right, 1) < 0){
					insert_hook_func(wp, fn, "alert_msg.Action1");
					free_2_dimension_list(&sh_num, &folder_list);
					return -1;
				}
#ifdef RTCONFIG_WEBDAV_PENDING
#error FIXME
				else {
					logmessage("wedavproxy right", "%s %s %s %s %d %s", char_user, mount_path, folder, protocol, right, webdavproxy);
					// modify permission for webdav proxy
					nvram_set("acc_webdavproxy", webdavproxy);
				}
#endif
			}
			free_2_dimension_list(&sh_num, &folder_list);
		}
	}

	free_disk_data(&disks_info);

#ifdef RTCONFIG_WEBDAV_PENDING
	if(strcmp(protocol, "webdav")==0) {
		if(notify_rc_for_nas("restart_webdav") != 0) {
			insert_hook_func(wp, fn, "alert_msg.Action1");
			return -1;
		}
	}
	else
#endif
	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action1");
		return -1;
	}

	insert_hook_func(wp, "set_group_all_folder_permission_success", "");

	return 0;
}

#endif

void not_ej_initial_folder_var_file()
{
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	disks_info = read_disk_data();
	if (disks_info == NULL)
		return;

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0) {
				initial_folder_list(follow_partition->mount_point);
//				initial_all_var_file(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);
}

int ej_initial_folder_var_file(int eid, webs_t wp, int argc, char **argv)
{
//	not_ej_initial_folder_var_file();
	return 0;
}

int ej_initial_account(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;
	char *command;
	int len;
	char *fn = "initial_account_error";

	nvram_set("acc_num", "0");
	nvram_set("acc_list", "");
	nvram_commit();

	disks_info = read_disk_data();
	if (disks_info == NULL){
		insert_hook_func(wp, fn, "alert_msg.System2");
		return -1;
	}

	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next)
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next)
			if (follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				len = strlen("rm -f ")+strlen(follow_partition->mount_point)+strlen("/.__*");
				command = (char *)malloc(sizeof(char)*(len+1));
				if (command == NULL){
					insert_hook_func(wp, fn, "alert_msg.System1");
					return -1;
				}
				sprintf(command, "rm -f %s/.__*", follow_partition->mount_point);
				command[len] = 0;

				system(command);
				free(command);

				initial_folder_list(follow_partition->mount_point);
				initial_all_var_file(follow_partition->mount_point);
			}

	free_disk_data(&disks_info);

#if 0
	if (add_account(nvram_safe_get("http_username"), nvram_safe_get("http_passwd")) < 0)
#else
	// there are file_lock(), file_unlock() in add_account().
	// They would let the buffer of nvram_safe_get() be confused.
	char buf1[64], buf2[64];
	char *http_passwd = nvram_safe_get("http_passwd");
#ifdef RTCONFIG_NVRAM_ENCRYPT
	char dec_passwd[NVRAM_ENC_LEN];
	pw_dec(http_passwd, dec_passwd);
	http_passwd = dec_passwd;
#endif
	strlcpy(buf1, nvram_safe_get("http_username"), sizeof(buf1));
	strlcpy(buf2, http_passwd , sizeof(buf2));

	if(add_account(buf1, buf2) < 0)
#endif
	{
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(add_webdav_account(nvram_safe_get("http_username"))<0) {
		insert_hook_func(wp, "init_account_error", "alert_msg.Action2");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#endif
	insert_hook_func(wp, "initial_account_success", "");

	return 0;
}

int ej_create_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *password = websGetVar(wp, "password", "");
	char *fn = "create_account_error";

	if (strlen(account) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input5");
		return -1;
	}
	if (strlen(password) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input14");
		return -1;
	}

	not_ej_initial_folder_var_file();

	if (add_account(account, password) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(add_webdav_account(account) < 0) {
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action2");
		return -1;
	}
#endif

	insert_hook_func(wp, "create_account_success", "");
	return 0;
}

int ej_delete_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *fn = "delete_account_error";

	if (strlen(account) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input5");
		return -1;
	}

	not_ej_initial_folder_var_file();

	if (del_account(account) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(del_webdav_account(account)<0) {
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action3");
		return -1;
	}
#endif

	insert_hook_func(wp, "delete_account_success", "");

	return 0;
}

int ej_modify_account(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", "");
	char *new_account = websGetVar(wp, "new_account", "");
	char *new_password = websGetVar(wp, "new_password", "");
	char *fn = "modify_account_error";

	if (strlen(account) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input5");
		return -1;
	}
	if (strlen(new_account) <= 0 && strlen(new_password) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input16");
		return -1;
	}

	if (mod_account(account, new_account, new_password) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if(mod_webdav_account(account, new_account)<0) {
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}
#endif

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}

#ifdef RTCONFIG_WEBDAV_PENDING
	if(notify_rc_for_nas("restart_webdav") != 0) {
		insert_hook_func(wp, fn, "alert_msg.Action4");
		return -1;
	}
#endif

	insert_hook_func(wp, "modify_account_success", "");

	return 0;
}

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
int ej_get_all_groups(int eid, webs_t wp, int argc, char **argv){
	int acc_num;
	PMS_ACCOUNT_INFO_T *account_list;
	int group_num;
	PMS_ACCOUNT_GROUP_INFO_T *group_list, *follow_group;
	int first;

	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0){
		printf("Failed to get the account list!\n");
		PMS_FreeAccInfo(&account_list, &group_list);
		return -1;
	}

	first = 1;
	for(follow_group = group_list; follow_group != NULL; follow_group = follow_group->next){
		if(first == 1)
			first = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "\"%s\"", follow_group->name);
	}

	PMS_FreeAccInfo(&account_list, &group_list);

	return 0;
}

int ej_get_permissions_of_group(int eid, webs_t wp, int argc, char **argv){
	disk_info_t *disks_info;
	int acc_num = 0;
	PMS_ACCOUNT_INFO_T *account_list;
	int group_num;
	PMS_ACCOUNT_GROUP_INFO_T *group_list, *follow_group;

	disks_info = read_disk_data();
	if(disks_info == NULL){
		websWrite(wp, "function get_group_permissions_in_pool(group, pool){return [];}\n");
		return -1;
	}

	websWrite(wp, "function get_group_permissions_in_pool(group, pool){\n");

	// share mode.
	draw_permissions_of_pms(wp, NULL, disks_info, 1);

	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0){
		printf("1. Can't get the group list.\n");
		free_disk_data(&disks_info);
		PMS_FreeAccInfo(&account_list, &group_list);
		return 0;
	}

	for(follow_group = group_list; follow_group != NULL; follow_group = follow_group->next){
		draw_permissions_of_pms(wp, follow_group->name, disks_info, 1);
	}
	PMS_FreeAccInfo(&account_list, &group_list);

	websWrite(wp, "}\n\n");

	if(disks_info != NULL)
		free_disk_data(&disks_info);

	return 0;
}
#endif

int ej_get_folder_tree(int eid, webs_t wp, int argc, char **argv){
	char *layer_order = websGetVar(wp, "layer_order", "");
	char *follow_info, *follow_info_end, backup;
	int layer = 0, first;
	int disk_count, partition_count, folder_count;
	int disk_order = -1, partition_order = -1;
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	if (strlen(layer_order) <= 0){
		printf("No input \"layer_order\"!\n");
		return -1;
	}

	follow_info = index(layer_order, '_');
	while (follow_info != NULL && *follow_info != 0){
		++layer;
		++follow_info;
		if (*follow_info == 0)
			break;
		follow_info_end = follow_info;
		while (*follow_info_end != 0 && isdigit(*follow_info_end))
			++follow_info_end;
		backup = *follow_info_end;
		*follow_info_end = 0;

		if (layer == 1)
			disk_order = atoi(follow_info);
		else if (layer == 2)
			partition_order = atoi(follow_info);
		else{
			*follow_info_end = backup;
			printf("Input \"%s\" is incorrect!\n", layer_order);
			return -1;
		}

		*follow_info_end = backup;
		follow_info = follow_info_end;
	}

	disks_info = read_disk_data();
	if (disks_info == NULL){
		printf("Can't read the information of disks.\n");
		return -1;
	}

	first = 1;
	disk_count = 0;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_count){
		partition_count = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count){
			if (layer != 0 && follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				int i;
				char **folder_list;
				int result;
				result = get_all_folder(follow_partition->mount_point, &folder_count, &folder_list);
				if (result < 0){
					printf("get_disk_tree: Can't get the folder list in \"%s\".\n", follow_partition->mount_point);

					folder_count = 0;
				}

				if (layer == 2 && disk_count == disk_order && partition_count == partition_order){
					for (i = 0; i < folder_count; ++i){
						if (first == 1)
							first = 0;
						else
							websWrite(wp, ", ");

						websWrite(wp, "\"%s#%u#0\"", folder_list[i], i);
					}
				}
				else if (layer == 1 && disk_count == disk_order){
					if (first == 1)
						first = 0;
					else
						websWrite(wp, ", ");

					follow_info = rindex(follow_partition->mount_point, '/');
					websWrite(wp, "\"%s#%u#%u\"", follow_info+1, partition_count, folder_count);
				}

				free_2_dimension_list(&folder_count, &folder_list);
			}
		}
		if (layer == 0){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "'%s#%u#%u'", follow_disk->tag, disk_count, partition_count);
		}

		if (layer > 0 && disk_count == disk_order)
			break;
	}

	free_disk_data(&disks_info);

	return 0;
}

int ej_get_share_tree(int eid, webs_t wp, int argc, char **argv){
	char *layer_order = websGetVar(wp, "layer_order", "");
	char *follow_info, *follow_info_end, backup;
	int layer = 0, first;
	int disk_count, partition_count, share_count;
	int disk_order = -1, partition_order = -1;
	disk_info_t *disks_info, *follow_disk;
	partition_info_t *follow_partition;

	if (strlen(layer_order) <= 0){
		printf("No input \"layer_order\"!\n");
		return -1;
	}

	follow_info = index(layer_order, '_');
	while (follow_info != NULL && *follow_info != 0){
		++layer;
		++follow_info;
		if (*follow_info == 0)
			break;
		follow_info_end = follow_info;
		while (*follow_info_end != 0 && isdigit(*follow_info_end))
			++follow_info_end;
		backup = *follow_info_end;
		*follow_info_end = 0;

		if (layer == 1)
			disk_order = atoi(follow_info);
		else if (layer == 2)
			partition_order = atoi(follow_info);
		else{
			*follow_info_end = backup;
			printf("Input \"%s\" is incorrect!\n", layer_order);
			return -1;
		}

		*follow_info_end = backup;
		follow_info = follow_info_end;
	}

	disks_info = read_disk_data();
	if (disks_info == NULL){
		printf("Can't read the information of disks.\n");
		return -1;
	}

	first = 1;
	disk_count = 0;
	for (follow_disk = disks_info; follow_disk != NULL; follow_disk = follow_disk->next, ++disk_count){
		partition_count = 0;
		for (follow_partition = follow_disk->partitions; follow_partition != NULL; follow_partition = follow_partition->next, ++partition_count){
			if (layer != 0 && follow_partition->mount_point != NULL && strlen(follow_partition->mount_point) > 0){
				int i;
				char **share_list;
				int result;
				result = get_folder_list(follow_partition->mount_point, &share_count, &share_list);
				if (result < 0){
					printf("get_disk_tree: Can't get the share list in \"%s\".\n", follow_partition->mount_point);

					share_count = 0;
				}

				if (layer == 2 && disk_count == disk_order && partition_count == partition_order){
					for (i = 0; i < share_count; ++i){
						if (first == 1)
							first = 0;
						else
							websWrite(wp, ", ");

						websWrite(wp, "\"%s#%u#0\"", share_list[i], i);
					}
				}
				else if (layer == 1 && disk_count == disk_order){
					if (first == 1)
						first = 0;
					else
						websWrite(wp, ", ");

					follow_info = rindex(follow_partition->mount_point, '/');
					websWrite(wp, "\"%s#%u#%u\"", follow_info+1, partition_count, share_count);
				}

				free_2_dimension_list(&share_count, &share_list);
			}
		}
		if (layer == 0){
			if (first == 1)
				first = 0;
			else
				websWrite(wp, ", ");

			websWrite(wp, "'%s#%u#%u'", follow_disk->tag, disk_count, partition_count);
		}

		if (layer > 0 && disk_count == disk_order)
			break;
	}

	free_disk_data(&disks_info);

	return 0;
}

int ej_set_share_mode(int eid, webs_t wp, int argc, char **argv){

	struct json_object *root=NULL;
	root = json_tokener_parse(post_buf);

	int samba_mode = nvram_get_int("st_samba_mode");
	int samba_force_mode = nvram_get_int("st_samba_force_mode");
	int ftp_mode = nvram_get_int("st_ftp_mode");
	int ftp_force_mode = nvram_get_int("st_ftp_force_mode");
#ifdef RTCONFIG_WEBDAV_PENDING
	int webdav_mode = nvram_get_int("st_webdav_mode");
#endif
	char *dummyShareway = get_cgi_json("dummyShareway", root);
	char *protocol = get_cgi_json("protocol", root);
	char *mode = get_cgi_json("mode", root);
	int result;
	char *fn = "set_share_mode_error";

	if (!dummyShareway || strlen(dummyShareway) == 0){
		nvram_set("dummyShareway", "0");
	}else{
		nvram_set("dummyShareway", dummyShareway);
	}

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		json_object_put(root);
		return -1;
	}
	if (strlen(mode) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input3");
		json_object_put(root);
		return -1;
	}
	if (!strcmp(mode, "share")){
		if (!strcmp(protocol, "cifs")){
			if((samba_mode == 1 || samba_mode == 3) && samba_force_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "1");	// for test
		}
		else if (!strcmp(protocol, "ftp")){
			if (ftp_mode == 1 && ftp_force_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_ftp_mode", "1");
		}
#ifdef RTCONFIG_WEBDAV_PENDING
		else if (!strcmp(protocol, "webdav")){
			if (webdav_mode == 1)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_webdav_mode", "1");
		}
#endif
		else{
			insert_hook_func(wp, fn, "alert_msg.Input2");
			json_object_put(root);
			return -1;
		}
	}
	else if (!strcmp(mode, "account")){
		if (!strcmp(protocol, "cifs")){
			if((samba_mode == 2 || samba_mode == 4) && samba_force_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_samba_mode", "4");
		}
		else if (!strcmp(protocol, "ftp")){
			if(ftp_mode == 2 && ftp_force_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_ftp_mode", "2");
		}
#ifdef RTCONFIG_WEBDAV_PENDING
		else if (!strcmp(protocol, "webdav")){
			if (webdav_mode == 2)
				goto SET_SHARE_MODE_SUCCESS;

			nvram_set("st_webdav_mode", "2");
		}
#endif
		else {
			insert_hook_func(wp, fn, "alert_msg.Input2");
			json_object_put(root);
			return -1;
		}
	}
	else{
		insert_hook_func(wp, fn, "alert_msg.Input4");
		json_object_put(root);
		return -1;
	}

	nvram_commit();

	not_ej_initial_folder_var_file();

	if (!strcmp(protocol, "cifs")) {
		result = notify_rc_for_nas("restart_samba_force");
	}
	else if (!strcmp(protocol, "ftp")) {
		result = notify_rc_for_nas("restart_ftpd_force");
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if (!strcmp(protocol, "webdav")) {
		result = notify_rc_for_nas("restart_webdav");
	}
#endif
	else {
		insert_hook_func(wp, fn, "alert_msg.Input2");
		json_object_put(root);
		return -1;
	}

	if (result != 0){
		insert_hook_func(wp, fn, "alert_msg.Action8");
		json_object_put(root);
		return -1;
	}

SET_SHARE_MODE_SUCCESS:
	insert_hook_func(wp, "set_share_mode_success", "");
	json_object_put(root);
	return 0;
}


int ej_modify_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char *new_folder = websGetVar(wp, "new_folder", "");
	char mount_path[PATH_MAX];
	char *fn = "modify_sharedfolder_error";

	if (strlen(pool) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}
	if (strlen(folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input9");
		return -1;
	}
	if (strlen(new_folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input17");
		return -1;
	}
	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}

	if (mod_folder(mount_path, folder, new_folder) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action7");
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action7");
		return -1;
	}

	insert_hook_func(wp, "modify_sharedfolder_success", "");

	return 0;
}

int ej_delete_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char mount_path[PATH_MAX];
	char *fn = "delete_sharedfolder_error";

	if (strlen(pool) <= 0 || strlen(pool) >= 8){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}
	if (strlen(folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input9");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}
	if (del_folder(mount_path, folder) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action6");
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action6");
		return -1;
	}

	insert_hook_func(wp, "delete_sharedfolder_success", "");

	return 0;
}

int ej_create_sharedfolder(int eid, webs_t wp, int argc, char **argv){
	char *account = websGetVar(wp, "account", NULL);
	char *pool = websGetVar(wp, "pool", "");
	char *folder = websGetVar(wp, "folder", "");
	char mount_path[PATH_MAX];
	char *fn = "create_sharedfolder_error";

	if (strlen(pool) <= 0 || strlen(pool) >= 8){
		insert_hook_func(wp, fn, "alert_msg.Input7");
		return -1;
	}
	if (strlen(folder) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input9");
		return -1;
	}

	if (get_mount_path(pool, mount_path, PATH_MAX) < 0){
		fprintf(stderr, "Can't get the mount_path of %s.\n", pool);

		insert_hook_func(wp, fn, "alert_msg.System1");
		return -1;
	}
	if (add_folder(account, mount_path, folder) < 0){
		insert_hook_func(wp, fn, "alert_msg.Action5");
		return -1;
	}

	if (notify_rc_for_nas("restart_ftpsamba") != 0){
		insert_hook_func(wp, fn, "alert_msg.Action5");
		return -1;
	}
	insert_hook_func(wp, "create_sharedfolder_success", "");

	return 0;
}

int ej_set_AiDisk_status(int eid, webs_t wp, int argc, char **argv){
	char *protocol = websGetVar(wp, "protocol", "");
	char *flag = websGetVar(wp, "flag", "");
	int result = 0;
	char *fn = "set_AiDisk_status_error";

	if (strlen(protocol) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input1");
		return -1;
	}
	if (strlen(flag) <= 0){
		insert_hook_func(wp, fn, "alert_msg.Input18");
		return -1;
	}
	if (!strcmp(protocol, "cifs")){
		if (!strcmp(flag, "on")){
			nvram_set("enable_samba", "1");
			nvram_commit();
			result = notify_rc_for_nas("restart_samba");
		}
		else if (!strcmp(flag, "off")){
			nvram_set("enable_samba", "0");
			nvram_commit();
			if (!pids("smbd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = notify_rc_for_nas("stop_samba");
		}
		else{
			insert_hook_func(wp, fn, "alert_msg.Input19");
			return -1;
		}
	}
	else if (!strcmp(protocol, "ftp")){
		if (!strcmp(flag, "on")){
			nvram_set("enable_ftp", "1");
			nvram_commit();
			result = notify_rc_for_nas("restart_ftpd");
		}
		else if (!strcmp(flag, "off")){
			nvram_set("enable_ftp", "0");
			nvram_commit();
			if (!pids("vsftpd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = notify_rc_for_nas("stop_ftpd");
		}
		else{
			insert_hook_func(wp, fn, "alert_msg.Input19");
			return -1;
		}
	}
#ifdef RTCONFIG_WEBDAV_PENDING
	else if (!strcmp(protocol, "webdav")){
		if (!strcmp(flag, "on")){
			nvram_set("enable_webdav", "1");
			nvram_commit();
			result = notify_rc_for_nas("restart_webdav");
		}
		else if (!strcmp(flag, "off")){
			nvram_set("enable_webdav", "0");
			nvram_commit();
			if (!pids("vsftpd"))
				goto SET_AIDISK_STATUS_SUCCESS;

			result = notify_rc_for_nas("stop_webdav");
		}
		else{
			insert_hook_func(wp, fn, "alert_msg.Input19");
			return -1;
		}
	}
#endif
	else{
		insert_hook_func(wp, fn, "alert_msg.Input2");
		return -1;
	}

	if (result != 0){
		insert_hook_func(wp, fn, "alert_msg.Action8");
		return -1;
	}

SET_AIDISK_STATUS_SUCCESS:
	//insert_hook_func(wp, "set_AiDisk_status_success", "");
	insert_hook_func(wp, "parent.resultOfSwitchAppStatus", "");

	return 0;
}

#ifdef RTCONFIG_WEBDAV

#define DEFAULT_WEBDAVPROXY_RIGHT 0

int add_webdav_account(char *account)
{
	char *nv, *nvp, *b;
	char new[256];
	char *acc, *right;
	int i, found;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));

	if(nv) {
		i = found = 0;
		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)==0) {
				found = 1;
				break;
			}
			i++;
		}
		free(nv);

		if(!found) {
			if(i==0) sprintf(new, "%s>%d", account, DEFAULT_WEBDAVPROXY_RIGHT);
			else sprintf(new, "%s<%s>%d", nvram_safe_get("acc_webdavproxy"), account, DEFAULT_WEBDAVPROXY_RIGHT);

			nvram_set("acc_webdavproxy", new);
		}
	}

	return 0;
}


int del_webdav_account(char *account)
{
	char *nv, *nvp, *b;
	char new[256], new_t[256];
	char *acc, *right;
	int i;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));

	if(nv) {
		i = 0;

		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)!=0) {
				if(i==0)
					sprintf(new, "%s>%s", acc, right);
				else{
					sprintf(new_t, "%s<%s>%s", new, acc, right);
					strlcpy(new, new_t, sizeof(new));
				}
				i++;
			}
		}
		free(nv);

		if(i) nvram_set("acc_webdavproxy", new);
	}
	return 0;
}


int mod_webdav_account(char *account, char *newaccount)
{
	char *nv, *nvp, *b;
	char new[256], new_t[256];
	char *acc, *right;
	int i;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));

	if(nv) {
		i = 0;

		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)!=0) {
				if(i==0)
					sprintf(new, "%s>%s", acc, right);
				else{
					sprintf(new_t, "%s<%s>%s", new, acc, right);
					strlcpy(new, new_t, sizeof(new));
				}
			}
			else {
				if(i==0)
					sprintf(new, "%s>%s", newaccount, right);
				else{
					sprintf(new_t, "%s<%s>%s", new, newaccount, right);
					strlcpy(new, new_t, sizeof(new));
				}
			}
			i++;
		}
		free(nv);

		if(i) nvram_set("acc_webdavproxy", new);
	}
	return 0;
}

#endif

int ej_apps_fsck_ret(int eid, webs_t wp, int argc, char **argv)
{
#ifdef RTCONFIG_DISK_MONITOR
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	FILE *fp = NULL;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		websWrite(wp, "[]");
		return -1;
	}

	websWrite(wp, "[");
	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			websWrite(wp, "[\"%s\", ", partition_info->device);

#define MAX_ERROR_CODE 3
			int error_code, got_code;
			char file_name[32];

			for(error_code = 0, got_code = 0; error_code <= MAX_ERROR_CODE; ++error_code){
				memset(file_name, 0, 32);
				sprintf(file_name, "/tmp/fsck_ret/%s.%d", partition_info->device, error_code);

				if((fp = fopen(file_name, "r")) != NULL){
					fclose(fp);
					websWrite(wp, "\"%d\"", error_code);
					got_code = 1;
					break;
				}
			}

			if(!got_code)
				websWrite(wp, "\"\"");

			websWrite(wp, "]%s", (partition_info->next)?", ":"");
		}

		websWrite(wp, "%s", (disk_info->next)?", ":"");
	}
	websWrite(wp, "]");

	free_disk_data(&disk_list);

	return 0;
#endif

	websWrite(wp, "[]");
	return 0;
}

#ifdef RTCONFIG_DISK_MONITOR
int ej_apps_fsck_log(int eid, webs_t wp, int argc, char **argv)
{
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char file_name[32], d_port[16], *d_dot;
	char *port_path = websGetVar(wp, "diskmon_usbport", "-1");
	int ret, all_disk;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		return -1;
	}

	all_disk = (atoi(port_path) == -1)? 1 : 0;
	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		/* If hub port number is not specified in port_path,
		 * don't compare it with hub port number in disk_info->port.
		 */
		strlcpy(d_port, disk_info->port, sizeof(d_port));
		d_dot = strchr(d_port, '.');
		if (!strchr(port_path, '.') && d_dot)
			*d_dot = '\0';
		if (!all_disk && strcmp(d_port, port_path))
			continue;

		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			memset(file_name, 0, 32);
			sprintf(file_name, "/tmp/fsck_ret/%s.log", partition_info->device);
			ret = dump_file(wp, file_name);

			if(ret)
				websWrite(wp, "\n\n");
		}
	}

	free_disk_data(&disk_list);

	return 0;
}
int ej_get_disk_format_log(int eid, webs_t wp, int argc, char **argv)
{
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char file_name[32], d_port[16]/*, *d_dot*/;
	char *port_path = websGetVar(wp, "diskmon_usbport", "-1");
	int ret = 0, all_disk;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		return -1;
	}

	all_disk = (atoi(port_path) == -1)? 1 : 0;
	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		/* If hub port number is not specified in port_path,
		 * don't compare it with hub port number in disk_info->port.
		 */
		strlcpy(d_port, disk_info->port, sizeof(d_port));
		/*if (!strchr(port_path, '.') && d_dot)
			*d_dot = '\0';*/
		if (!all_disk && strcmp(d_port, port_path))
			continue;

		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			memset(file_name, 0, 32);
			sprintf(file_name, "/tmp/disk_format/%s.log", partition_info->device);
			if(check_if_file_exist(file_name)) {
				ret = dump_file(wp, file_name);
			}

			if(ret)
				websWrite(wp, "\n\n");
		}
	}

	free_disk_data(&disk_list);
	return 0;
}
#endif

// argv[0] = "all" or NULL: show all lists, "asus": only show the list of ASUS, "others": show other lists.
// apps_info: ["Name", "Version", "New Version", "Installed", "Enabled", "Source", "URL", "Description", "Depends", "Optional Utility", "New Optional Utility", "Help Path", "New File name"].
int ej_apps_info(int eid, webs_t wp, int argc, char **argv)
{
	apps_info_t *follow_apps_info, *apps_info_list;
	char *name;

	if (ejArgs(argc, argv, "%s", &name) < 1)
		name = APP_OWNER_ALL;
	if (strcmp(name, APP_OWNER_ALL) != 0 &&
	    strcmp(name, APP_OWNER_ASUS) != 0 &&
	    strcmp(name, APP_OWNER_OTHERS) != 0) {
		websWrite(wp, "[]");
		return 0;
	}

	apps_info_list = follow_apps_info = get_apps_list(name);
	websWrite(wp, "[");
	while (follow_apps_info != NULL) {
		websWrite(wp, "[");
		websWrite(wp, "\"%s\", ", follow_apps_info->name ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->version ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->new_version ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->installed ? : "" /* Why not FIELD_NO? */);
		websWrite(wp, "\"%s\", ", follow_apps_info->enabled ? : FIELD_YES);
		websWrite(wp, "\"%s\", ", follow_apps_info->source ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->url ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->description ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->depends ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->optional_utility ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->new_optional_utility ? : "");
		websWrite(wp, "\"%s\", ", follow_apps_info->help_path ? : "");
		websWrite(wp, "\"%s\"",   follow_apps_info->new_file_name ? : "");

		follow_apps_info = follow_apps_info->next;
		websWrite(wp, "]%s\n", follow_apps_info ? "," : "");
	}
	websWrite(wp, "]");
	free_apps_list(&apps_info_list);

	return 0;
}

int ej_apps_state_info(int eid, webs_t wp, int argc, char **argv){
	char *cmd[] = {"chk_app_state", NULL};
	int pid;

	_eval(cmd, NULL, 0, &pid);

	websWrite(wp, "apps_dev = \"%s\";", nvram_safe_get("apps_dev"));
	websWrite(wp, "apps_mounted_path = \"%s\";", nvram_safe_get("apps_mounted_path"));

	websWrite(wp, "apps_state_upgrade = \"%s\";", nvram_safe_get("apps_state_upgrade"));
	websWrite(wp, "apps_state_update = \"%s\";", nvram_safe_get("apps_state_update"));
	websWrite(wp, "apps_state_remove = \"%s\";", nvram_safe_get("apps_state_remove"));
	websWrite(wp, "apps_state_enable = \"%s\";", nvram_safe_get("apps_state_enable"));
	websWrite(wp, "apps_state_switch = \"%s\";", nvram_safe_get("apps_state_switch"));
	websWrite(wp, "apps_state_autorun = \"%s\";", nvram_safe_get("apps_state_autorun"));
	websWrite(wp, "apps_state_install = \"%s\";", nvram_safe_get("apps_state_install"));
	websWrite(wp, "apps_state_error = \"%s\";", nvram_safe_get("apps_state_error"));

	websWrite(wp, "apps_download_file = \"%s\";", nvram_safe_get("apps_download_file"));
	websWrite(wp, "apps_download_percent = \"%s\";", nvram_safe_get("apps_download_percent"));

	websWrite(wp, "apps_depend_do = \"%s\";", nvram_safe_get("apps_depend_do"));
	websWrite(wp, "apps_depend_action = \"%s\";", nvram_safe_get("apps_depend_action"));
	websWrite(wp, "apps_depend_action_target = \"%s\";", nvram_safe_get("apps_depend_action_target"));

	return 0;
}

int ej_apps_action(int eid, webs_t wp, int argc, char **argv){

	struct json_object *root=NULL;

	do_json_decode(&root);

	char *apps_action = get_cgi_json("apps_action", root);
	char *apps_name = get_cgi_json("apps_name", root);
	char *apps_flag = get_cgi_json("apps_flag", root);
	char command[128];

	if(!apps_action || strlen(apps_action) <= 0)
		goto SET_APPS_ACTION_FINISH;

	nvram_set("apps_state_action", apps_action);

	memset(command, 0, sizeof(command));

	if(!strcmp(apps_action, "install")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			goto SET_APPS_ACTION_FINISH;

		snprintf(command, sizeof(command), "start_apps_install %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "stop")){
		snprintf(command, sizeof(command), "start_apps_stop");
	}
	else if(!strcmp(apps_action, "update")){
		snprintf(command, sizeof(command), "start_apps_update");
	}
	else if(!strcmp(apps_action, "upgrade")){
		if(strlen(apps_name) <= 0)
			goto SET_APPS_ACTION_FINISH;

		snprintf(command, sizeof(command), "start_apps_upgrade %s", apps_name);
	}
	else if(!strcmp(apps_action, "remove")){
		if(strlen(apps_name) <= 0)
			goto SET_APPS_ACTION_FINISH;

		snprintf(command, sizeof(command), "start_apps_remove %s", apps_name);
	}
	else if(!strcmp(apps_action, "enable")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			goto SET_APPS_ACTION_FINISH;

		if(strcmp(apps_flag, "yes") && strcmp(apps_flag, "no"))
			goto SET_APPS_ACTION_FINISH;

		snprintf(command, sizeof(command), "start_apps_enable %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "switch")){
		if(strlen(apps_name) <= 0 || strlen(apps_flag) <= 0)
			goto SET_APPS_ACTION_FINISH;

		snprintf(command, sizeof(command), "start_apps_switch %s %s", apps_name, apps_flag);
	}
	else if(!strcmp(apps_action, "cancel")){
		snprintf(command, sizeof(command), "start_apps_cancel");
	}

	if(strlen(command) > 0)
		notify_rc(command);

	goto SET_APPS_ACTION_FINISH;

SET_APPS_ACTION_FINISH:
	json_object_put(root);
 	return 0;

}

#ifdef RTCONFIG_MEDIA_SERVER
// dms_info: ["Scanning"] or [ "" ]

int ej_dms_info(int eid, webs_t wp, int argc, char **argv){
	char *dms_dbcwd;
	char dms_scanfile[PATH_MAX], dms_status[32];
	FILE *fp;

	dms_dbcwd = nvram_safe_get("dms_dbcwd");

	strcpy(dms_status, "");

	if(nvram_get_int("dms_enable") && strlen(dms_dbcwd))
	{
		sprintf(dms_scanfile, "%s/scantag", dms_dbcwd);

		fp = fopen(dms_scanfile, "r");

		if(fp) {
			strcpy(dms_status, "Scanning");
			fclose(fp);
		}
	}

	websWrite(wp, "[\"%s\"]", dms_status);

	return 0;
}
#endif

//#ifdef RTCONFIG_CLOUDSYNC
static char *convert_cloudsync_status(const char *status_code){
	if(!strcmp(status_code, "STATUS:70"))
		return "INITIAL";
	else if(!strcmp(status_code, "STATUS:71"))
		return "SYNC";
	else if(!strcmp(status_code, "STATUS:72"))
		return "DOWNUP";
	else if(!strcmp(status_code, "STATUS:73"))
		return "UPLOAD";
	else if(!strcmp(status_code, "STATUS:74"))
		return "DOWNLOAD";
	else if(!strcmp(status_code, "STATUS:75"))
		return "STOP";
	else if(!strcmp(status_code, "STATUS:77"))
		return "INPUT CAPTCHA";
	else
		return "ERROR";
}

// cloud_sync = "content of nvram: cloud_sync"
// cloud_status = "string of status"
// cloud_obj = "handled object"
// cloud_msg = "error message"
int ej_cloud_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/asuswebstorage", "r");
	char line[PATH_MAX], buf[PATH_MAX];
	int line_num;
	char status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX];

	websWrite(wp, "cloud_sync=\"%s\";\n", nvram_safe_get("cloud_sync"));

	if(fp == NULL){
		websWrite(wp, "cloud_status=\"ERROR\";\n");
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		switch(line_num){
			case 1:
				strncpy(status, convert_cloudsync_status(line), 16);
				break;
			case 2:
				memset(buf, 0, PATH_MAX);
				char_to_ascii(buf, line);
				strcpy(mounted_path, buf);
				break;
			case 3:
				// memset(buf, 0, PATH_MAX);
				// char_to_ascii(buf, line);
				// strcpy(target_obj, buf);
				strcpy(target_obj, line); // support Chinese
				break;
			case 4:
				strcpy(error_msg, line);
				break;
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_status=\"ERROR\";\n");
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_status=\"%s\";\n", status);
		websWrite(wp, "cloud_obj=\"%s\";\n", target_obj);
		websWrite(wp, "cloud_msg=\"%s\";\n", error_msg);
	}

	return 0;
}

//Viz add to get partial string 2012.11.13
void substr(char *dest, const char* src, unsigned int start, unsigned int cnt)
{
	strncpy(dest, src + start, cnt);
	dest[cnt] = 0;
}

//use for UI to avoid variable 'cloud_sync' JavaScript error, Jieming added at 2012.09.11
int ej_UI_cloud_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/asuswebstorage", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], captcha_url[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
		websWrite(wp, "cloud_fullcapa=\"\";\n");
		websWrite(wp, "cloud_usedcapa=\"\";\n");
		websWrite(wp, "CAPTCHA_URL=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(captcha_url, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX-11);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX-9);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX-8);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX-12);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX-11);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "CAPTCHA_URL") != NULL){
			substr(dest, line, 12, PATH_MAX-12);
			strcpy(captcha_url, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_status=\"ERROR\";\n");
		websWrite(wp, "cloud_obj=\"\";\n");
		websWrite(wp, "cloud_msg=\"\";\n");
		websWrite(wp, "cloud_fullcapa=\"\";\n");
		websWrite(wp, "cloud_usedcapa=\"\";\n");
		websWrite(wp, "CAPTCHA_URL=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_status=\"%s\";\n", status);
		websWrite(wp, "cloud_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "cloud_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "CAPTCHA_URL=\"%s\";\n", captcha_url);
	}

	return 0;
}

int ej_UI_cloud_dropbox_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/dropbox", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_dropbox_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_dropbox_obj=\"\";\n");
		websWrite(wp, "cloud_dropbox_msg=\"\";\n");
		websWrite(wp, "cloud_dropbox_fullcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_usedcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_dropbox_status=\"ERROR\";\n");
		websWrite(wp, "cloud_dropbox_obj=\"\";\n");
		websWrite(wp, "cloud_dropbox_msg=\"\";\n");
		websWrite(wp, "cloud_dropbox_fullcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_usedcapa=\"\";\n");
		websWrite(wp, "cloud_dropbox_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_dropbox_status=\"%s\";\n", status);
		websWrite(wp, "cloud_dropbox_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "cloud_dropbox_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_dropbox_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_dropbox_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_dropbox_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_cloud_ftpclient_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/ftpclient", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_ftpclient_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_ftpclient_obj=\"\";\n");
		websWrite(wp, "cloud_ftpclient_msg=\"\";\n");
		websWrite(wp, "cloud_ftpclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_ftpclient_status=\"ERROR\";\n");
		websWrite(wp, "cloud_ftpclient_obj=\"\";\n");
		websWrite(wp, "cloud_ftpclient_msg=\"\";\n");
		websWrite(wp, "cloud_ftpclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_ftpclient_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_ftpclient_status=\"%s\";\n", status);
		websWrite(wp, "cloud_ftpclient_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "cloud_ftpclient_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_ftpclient_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_ftpclient_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_ftpclient_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_cloud_sambaclient_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/sambaclient", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_sambaclient_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_sambaclient_obj=\"\";\n");
		websWrite(wp, "cloud_sambaclient_msg=\"\";\n");
		websWrite(wp, "cloud_sambaclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_sambaclient_status=\"ERROR\";\n");
		websWrite(wp, "cloud_sambaclient_obj=\"\";\n");
		websWrite(wp, "cloud_sambaclient_msg=\"\";\n");
		websWrite(wp, "cloud_sambaclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_sambaclient_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_sambaclient_status=\"%s\";\n", status);
		websWrite(wp, "cloud_sambaclient_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "cloud_sambaclient_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_sambaclient_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_sambaclient_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_sambaclient_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_cloud_usbclient_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/smartsync/.logs/usbclient", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX], rule_num[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "cloud_usbclient_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "cloud_usbclient_obj=\"\";\n");
		websWrite(wp, "cloud_usbclient_msg=\"\";\n");
		websWrite(wp, "cloud_usbclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_usbclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_usbclient_rule_num=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);
	memset(rule_num, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX);
			strcpy(used_capa, dest);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX);
			strcpy(rule_num, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "cloud_usbclient_status=\"ERROR\";\n");
		websWrite(wp, "cloud_usbclient_obj=\"\";\n");
		websWrite(wp, "cloud_usbclient_msg=\"\";\n");
		websWrite(wp, "cloud_usbclient_fullcapa=\"\";\n");
		websWrite(wp, "cloud_usbclient_usedcapa=\"\";\n");
		websWrite(wp, "cloud_usbclient_rule_num=\"\";\n");
	}
	else{
		websWrite(wp, "cloud_usbclient_status=\"%s\";\n", status);
		websWrite(wp, "cloud_usbclient_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "cloud_usbclient_msg=\"%s\";\n", error_msg);
		websWrite(wp, "cloud_usbclient_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "cloud_usbclient_usedcapa=\"%s\";\n", used_capa);
		websWrite(wp, "cloud_usbclient_rule_num=\"%s\";\n", rule_num);
	}

	return 0;
}

int ej_UI_rs_status(int eid, webs_t wp, int argc, char **argv){
	FILE *fp = fopen("/tmp/Cloud/log/WebDAV", "r");
	char line[PATH_MAX], buf[PATH_MAX], dest[PATH_MAX];
	int line_num;
	char rulenum[PATH_MAX], status[17], mounted_path[PATH_MAX], target_obj[PATH_MAX], error_msg[PATH_MAX], full_capa[PATH_MAX], used_capa[PATH_MAX];

	if(fp == NULL){
		websWrite(wp, "rs_rulenum=\"\";\n");
		websWrite(wp, "rs_status=\"WAITING\";\n"); //gauss change status fromm 'ERROR' to 'WAITING' 2014.11.4
		websWrite(wp, "rs_obj=\"\";\n");
		websWrite(wp, "rs_msg=\"\";\n");
		websWrite(wp, "rs_fullcapa=\"\";\n");
		websWrite(wp, "rs_usedcapa=\"\";\n");
		return 0;
	}

	memset(status, 0, 17);
	memset(rulenum, 0, PATH_MAX);
	memset(mounted_path, 0, PATH_MAX);
	memset(target_obj, 0, PATH_MAX);
	memset(error_msg, 0, PATH_MAX);
	memset(full_capa, 0, PATH_MAX);
	memset(used_capa, 0, PATH_MAX);

	memset(line, 0, PATH_MAX);
	line_num = 0;
	while(fgets(line, PATH_MAX, fp)){
		++line_num;
		line[strlen(line)-1] = 0;

		if(strstr(line, "STATUS") != NULL){
			strncpy(status, convert_cloudsync_status(line), 16);
		}
		else if(strstr(line, "RULENUM") != NULL){
			substr(dest, line, 8, PATH_MAX-8);
			strcpy(rulenum, dest);
		}
		else if(strstr(line, "MOUNT_PATH") != NULL){
			memset(buf, 0, PATH_MAX);
			substr(dest, line, 11, PATH_MAX-11);
			char_to_ascii(buf, dest);
			strcpy(mounted_path, buf);
		}
		else if(strstr(line, "FILENAME") != NULL){
			substr(dest, line, 9, PATH_MAX-9);
			strcpy(target_obj, dest); // support Chinese
		}
		else if(strstr(line, "ERR_MSG") != NULL){
			substr(dest, line, 8, PATH_MAX-8);
			strcpy(error_msg, dest);
		}
		else if(strstr(line, "TOTAL_SPACE") != NULL){
			substr(dest, line, 12, PATH_MAX-12);
			strcpy(full_capa, dest);
		}
		else if(strstr(line, "USED_SPACE") != NULL){
			substr(dest, line, 11, PATH_MAX-11);
			strcpy(used_capa, dest);
		}

		memset(line, 0, PATH_MAX);
	}
	fclose(fp);

	if(!line_num){
		websWrite(wp, "rs_rulenum=\"\";\n");
		websWrite(wp, "rs_status=\"ERROR\";\n");
		websWrite(wp, "rs_obj=\"\";\n");
		websWrite(wp, "rs_msg=\"\";\n");
		websWrite(wp, "rs_fullcapa=\"\";\n");
		websWrite(wp, "rs_usedcapa=\"\";\n");
	}
	else{
		websWrite(wp, "rs_rulenum=\"%s\";\n", rulenum);
		websWrite(wp, "rs_status=\"%s\";\n", status);
		websWrite(wp, "rs_obj=\"%s\";\n", target_obj);
		if(!strcmp(status,"SYNC"))
		   strncpy(error_msg,"Sync has been completed",PATH_MAX);
		else if(!strcmp(status,"INITIAL"))
		   strncpy(error_msg,"Verifying",PATH_MAX);
		websWrite(wp, "rs_msg=\"%s\";\n", error_msg);
		websWrite(wp, "rs_fullcapa=\"%s\";\n", full_capa);
		websWrite(wp, "rs_usedcapa=\"%s\";\n", used_capa);
	}

	return 0;
}

#define WEBDEVINFO_VER 1 //log ej_webdavInfo
int ej_webdavInfo(int eid, webs_t wp, int argc, char **argv) {

	unsigned short ExtendCap=0;
#ifdef RTCONFIG_WEBDAV
	ExtendCap |= EXTEND_CAP_WEBDAV;
#else
	ExtendCap = 0;
	if(check_if_file_exist("/opt/etc/init.d/S50aicloud"))
		ExtendCap |= EXTEND_CAP_WEBDAV;
#endif
#ifdef RTCONFIG_TUNNEL
	ExtendCap |= EXTEND_CAP_AAE_BASIC;
#endif
	ExtendCap |= EXTEND_CAP_SWCTRL;
#ifdef RTCONFIG_AMAS
#ifdef RTCONFIG_SW_HW_AUTH
	if (getAmasSupportMode() != 0)
	{
#endif
		if (!repeater_mode()
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
			&& !psr_mode()
#endif
#ifdef RTCONFIG_DPSTA
			&& !(dpsta_mode() && nvram_get_int("re_mode") == 0)
#endif
		)
			ExtendCap |= EXTEND_CAP_AMAS;
#ifdef RTCONFIG_SW_HW_AUTH
        }
#endif
	if (nvram_get_int("amas_bdl"))
		ExtendCap |= EXTEND_CAP_AMAS_BDL;
#endif
#if defined(RTCONFIG_CFGSYNC) && defined(RTCONFIG_MASTER_DET)
	if (nvram_get_int("cfg_master"))
		ExtendCap |= EXTEND_CAP_MASTER;
#endif

	websWrite(wp, "// pktInfo=['PrinterInfo','SSID','NetMask','ProductID','FWVersion','OPMode','MACAddr','Regulation'];\n");
	websWrite(wp, "pktInfo=['','%s',", nvram_safe_get("wl0_ssid"));
	websWrite(wp, "'%s',", nvram_safe_get("lan_netmask"));
	websWrite(wp, "'%s',", get_productid());
	websWrite(wp, "'%s.%s',", nvram_safe_get("firmver"), nvram_safe_get("buildno"));
	websWrite(wp, "'%d',", get_sw_mode());
	websWrite(wp, "'%s',", get_lan_hwaddr());
	websWrite(wp, "''];\n");

	websWrite(wp, "// webdavInfo=['Webdav','HTTPType','HTTPPort','DDNS','HostName','WAN0IPAddr','','xSetting','HTTPSPort'];\n");
	websWrite(wp, "webdavInfo=['%s',", nvram_safe_get("enable_webdav"));
	websWrite(wp, "'%s',", nvram_safe_get("st_webdav_mode"));
	websWrite(wp, "'%s',", nvram_safe_get("webdav_http_port"));
	websWrite(wp, "'%s',", nvram_safe_get("ddns_enable_x"));
	websWrite(wp, "'%s',", nvram_safe_get("ddns_hostname_x"));
	websWrite(wp, "'%s',", nvram_safe_get("wan0_ipaddr"));
	websWrite(wp, "'%s',", nvram_safe_get(""));
	websWrite(wp, "'%s',", nvram_safe_get("x_Setting"));
	websWrite(wp, "'%s',", nvram_safe_get("webdav_https_port"));
#ifdef RTCONFIG_WEBDAV
 	websWrite(wp, "'1'");
#else
	if(check_if_file_exist("/opt/etc/init.d/S50aicloud")) websWrite(wp, "'1'");
	else websWrite(wp, "'0'");
#endif
	websWrite(wp, "];\n");

	websWrite(wp, "// toAPPInfo=['Ver','ExtendCap'];\n");
	websWrite(wp, "toAPPInfo=['%d'", WEBDEVINFO_VER);
	websWrite(wp, ",'%u',", ExtendCap);
	websWrite(wp, "'%s',", get_label_mac());
	websWrite(wp, "''];\n");

	return 0;
}
//#endif
#endif

// 2010.09 James. {
int start_autodet(int eid, webs_t wp, int argc, char **argv) {
	if(strcmp(nvram_safe_get("autodet_proceeding"), "1")){
		nvram_set("autodet_state", "");
		notify_rc_after_period_wait("start_autodet", 0);
	}
	return 0;
}

int start_force_autodet(int eid, webs_t wp, int argc, char **argv) {
        notify_rc_after_period_wait("start_autodet", 0);
        return 0;
}

#ifdef RTCONFIG_QCA_PLC_UTILS
int start_plcdet(int eid, webs_t wp, int argc, char **argv) {
	nvram_set("autodet_plc_state", "");
	notify_rc_after_period_wait("start_plcdet", 0);
	return 0;
}

int ej_plc_status(int eid, webs_t wp, int argc, char **argv)
{
	struct remote_plc *plc1 = NULL, *plc2 = NULL;
	int cnt1, cnt2;
	int i, j;
	int ret = 0, comma_need = 0;

	cnt1 = get_connected_plc(&plc1);
	cnt2 = get_known_plc(&plc2);
	for (i = 0; i < cnt1; i++) {
		for (j = 0; j < cnt2; j++) {
			if (plc2[j].status == 2 && strcmp(plc1[i].mac, plc2[j].mac) == 0) {
				strcpy(plc1[i].pwd, plc2[j].pwd);
				plc1[i].status = 1;
				plc2[j].status = 1;
				break;
			}
		}
	}

	ret = websWrite(wp, "[");
	for (i = 0; i < cnt1; i++) {
		if (plc1[i].status == 1) {
			if (comma_need)
				ret += websWrite(wp, ", ");
			ret += websWrite(wp, "[\"%s\", \"%s\", \"1\"]", plc1[i].mac, plc1[i].pwd);

			if (comma_need == 0)
				comma_need = 1;
		}
	}
	for (i = 0; i < cnt2; i++) {
		if (plc2[i].status == 2) {
			if (comma_need)
				ret += websWrite(wp, ", ");
			ret += websWrite(wp, "[\"%s\", \"%s\", \"2\"]", plc2[i].mac, plc2[i].pwd);

			if (comma_need == 0)
				comma_need = 1;
		}
	}
	for (i = 0; i < cnt1; i++) {
		if (plc1[i].status == 3) {
			if (comma_need)
				ret += websWrite(wp, ", ");
			ret += websWrite(wp, "[\"%s\", \"%s\", \"3\"]", plc1[i].mac, plc1[i].pwd);

			if (comma_need == 0)
				comma_need = 1;
		}
	}
	ret += websWrite(wp, "]");

	if (plc1)
		free(plc1);
	if (plc2)
		free(plc2);

	return ret;
}
#endif

#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
int start_wlcscan(int eid, webs_t wp, int argc, char **argv) {
	notify_rc("start_wlcscan");
	return 0;
}
#endif

// qos svg support 2010.08 Viz vvvvvvvvvvvv
void asp_ctcount(webs_t wp, int argc, char_t **argv)
{
	static const char *states[10] = {
		"NONE", "ESTABLISHED", "SYN_SENT", "SYN_RECV", "FIN_WAIT",
		"TIME_WAIT", "CLOSE", "CLOSE_WAIT", "LAST_ACK", "LISTEN" };
	int count[13];	// tcp(10) + udp(2) + total(1) = 13 / max classes = 10
	FILE *f;
	char s[512];
	char *p;
	int i;
	int n;
	int mode;
	unsigned long rip;
	unsigned long lan;
	unsigned long mask;
	int ret=0;

	if (argc != 1) return;
	mode = atoi(argv[0]);

	memset(count, 0, sizeof(count));

	  if ((f = fopen("/proc/net/ip_conntrack", "r")) != NULL) {
		// ctvbuf(f);	// if possible, read in one go

		if (nvram_match("t_hidelr", "1")) {
			mask = inet_addr(nvram_safe_get("lan_netmask"));
			rip = inet_addr(nvram_safe_get("lan_ipaddr"));
			lan = rip & mask;
		}
		else {
			rip = lan = mask = 0;
		}

		while (fgets(s, sizeof(s), f)) {
			if (rip != 0) {
				// src=x.x.x.x dst=x.x.x.x	// DIR_ORIGINAL
				if ((p = strstr(s + 14, "src=")) == NULL) continue;
				if ((inet_addr(p + 4) & mask) == lan) {
					if ((p = strstr(p + 13, "dst=")) == NULL) continue;
					if (inet_addr(p + 4) == rip) continue;
				}
			}

			if (mode == 0) {
				// count connections per state
				if (strncmp(s, "tcp", 3) == 0) {
					for (i = 9; i >= 0; --i) {
						if (strstr(s, states[i]) != NULL) {
							count[i]++;
							break;
						}
					}
				}
				else if (strncmp(s, "udp", 3) == 0) {
					if (strstr(s, "[UNREPLIED]") != NULL) {
						count[10]++;
					}
					else if (strstr(s, "[ASSURED]") != NULL) {
						count[11]++;
					}
				}
				count[12]++;
			}
			else {
				// count connections per mark
				if ((p = strstr(s, " mark=")) != NULL) {
					n = atoi(p + 6) & 0xFF;
					if (n <= 10) count[n]++;
				}
			}
		}

		fclose(f);
	}

	if (mode == 0) {
		p = s;
		for (i = 0; i < 12; ++i) {
			p += sprintf(p, ",%d", count[i]);
		}
		ret += websWrite(wp, "\nconntrack = [%d%s];\n", count[12], s);
	}
	else {
		p = s;
		for (i = 1; i < 11; ++i) {
			p += sprintf(p, ",%d", count[i]);
		}
		ret += websWrite(wp, "\nnfmarks = [%d%s];\n", count[0], s);
	}
}

int ej_qos_packet(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *f;
	char s[256];
	unsigned long rates[10];
	unsigned long u;
	char *e;
	int n;
	char comma;
	char *a[1];
	int ret=0, unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char wan_ifname[16];

	unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);
	memset(wan_ifname, 0, 16);
	strcpy(wan_ifname, strcat_r(prefix, "ifname", tmp));

	a[0] = "1";
	asp_ctcount(wp, 1, a);

	memset(rates, 0, sizeof(rates));
	sprintf(s, "tc -s class ls dev %s", nvram_safe_get(wan_ifname));
	if ((f = popen(s, "r")) != NULL) {
		n = 1;
		while (fgets(s, sizeof(s), f)) {
			if (strncmp(s, "class htb 1:", 12) == 0) {
				n = atoi(s + 12);
			}
			else if (strncmp(s, " rate ", 6) == 0) {
				if ((n % 10) == 0) {
					n /= 10;
					if ((n >= 1) && (n <= 10)) {
						u = strtoul(s + 6, &e, 10);
						if (*e == 'K') u *= 1000;
							else if (*e == 'M') u *= 1000 * 1000;
						rates[n - 1] = u;
						n = 1;
					}
				}
			}
		}
		pclose(f);
	}

	comma = ' ';
	ret += websWrite(wp, "\nqrates = [0,");
	for (n = 0; n < 10; ++n) {
		ret += websWrite(wp, "%c%lu", comma, rates[n]);
		comma = ',';
	}
	ret += websWrite(wp, "];");
	return 0;
}

int ej_ctdump(int eid, webs_t wp, int argc, char **argv)
{
	FILE *f = NULL;
	char s[512];
	char *p, *q;
	int mark;
	int findmark;
	unsigned int proto;
	unsigned int time;
	char src[16];
	char dst[16];
	char sport[16];
	char dport[16];
	unsigned long rip;
	unsigned long lan;
	unsigned long mask;
	char comma;
	int ret=0;

	if (argc != 1) return 0;

	findmark = atoi(argv[0]);

	mask = inet_addr(nvram_safe_get("lan_netmask"));
	rip = inet_addr(nvram_safe_get("lan_ipaddr"));
	lan = rip & mask;
	if (nvram_match("t_hidelr", "0")) rip = 0;	// hide lan -> router?

	ret += websWrite(wp, "\nctdump = [");
	comma = ' ';
	if ((f = fopen("/proc/net/ip_conntrack", "r")) != NULL) {
		//ctvbuf(f);
		while (fgets(s, sizeof(s), f)) {
			if ((p = strstr(s, " mark=")) == NULL) continue;
			if ((mark = (atoi(p + 6) & 0xFF)) > 10) mark = 0;
			if ((findmark != -1) && (mark != findmark)) continue;

			if (sscanf(s, "%*s %u %u", &proto, &time) != 2) continue;

			if ((p = strstr(s + 14, "src=")) == NULL) continue;		// DIR_ORIGINAL
			if ((inet_addr(p + 4) & mask) != lan) {
				// make sure we're seeing int---ext if possible
				if ((p = strstr(p + 41, "src=")) == NULL) continue;	// DIR_REPLY
			}
			else if (rip != 0) {
				if ((q = strstr(p + 13, "dst=")) == NULL) continue;
//				cprintf("%lx=%lx\n", inet_addr(q + 4), rip);
				if (inet_addr(q + 4) == rip) continue;
			}

			if ((proto == 6) || (proto == 17)) {
				if (sscanf(p + 4, "%s dst=%s sport=%s dport=%s", src, dst, sport, dport) != 4) continue;
			}
			else {
				if (sscanf(p + 4, "%s dst=%s", src, dst) != 2) continue;
				sport[0] = 0;
				dport[0] = 0;
			}
			ret += websWrite(wp, "%c[%u,%u,'%s','%s','%s','%s',%d]", comma, proto, time, src, dst, sport, dport, mark);
			comma = ',';
		}
		fclose(f);
	}
	ret += websWrite(wp, "];\n");
	return 0;
}

void ej_cgi_get(int eid, webs_t wp, int argc, char **argv)
{
	const char *v;
	int i;
	int ret=0;

	for (i = 0; i < argc; ++i) {
		v = get_cgi(argv[i]);
		if (v) ret += websWrite(wp, v);
	}
}

#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
uint32_t traffic_wanlan(char *ifname, uint32_t *rx, uint32_t *tx);
#endif

#ifdef RTCONFIG_LANTIQ
#define PPACMD_WAN_PATH "/tmp/ppacmd_getwan"
#define PPACMD_LAN_PATH "/tmp/ppacmd_getlan"
#define PPACMD_TRAFFIC_PATH "/tmp/ppacmd_traffic"
#endif

// traffic monitor
static int ej_netdev(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE * fp;
	char buf[256];
	unsigned long long rx = 0, tx = 0, curr_rx = 0, curr_tx = 0;
	unsigned long long rx2 = 0, tx2 = 0;
#ifdef RTCONFIG_LACP
	unsigned long long rx_lacp1=0, tx_lacp1=0;
	unsigned long long rx_lacp2=0, tx_lacp2=0;
#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
	unsigned long long rx2_lacp1=0, tx2_lacp1=0;
	unsigned long long rx2_lacp2=0, tx2_lacp2=0;
#endif
#endif
	unsigned long long wl0_all_rx = 0, wl0_all_tx = 0;
	unsigned long long wl1_all_rx = 0, wl1_all_tx = 0;
	unsigned long long wl2_all_rx = 0, wl2_all_tx = 0;
	unsigned long long wl3_all_rx = 0, wl3_all_tx = 0;
	unsigned long long wired_all_rx = 0, wired_all_tx = 0;
	ino_t inode;
	struct ifino_s *ifino;
	static struct ifname_ino_tbl ifstat_tbl = { 0 };
	char *p;
	char *ifname;
	char ifname_desc[12], ifname_desc2[12];
#if defined(RTCONFIG_BCM5301X_TRAFFIC_MONITOR) && defined(RTCONFIG_LACP)
	char ifname_desc2_lacp1[12];
	char ifname_desc2_lacp2[12];
#endif
	char comma;
	int wl0_valid = 0, wl1_valid = 0;
	int wl2_valid = 0, wl3_valid = 0;
	int ret=0;
	int from_app = 0;
	int wired_valid = 0;
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
	qcsapi_unsigned_int l_counter_value;
#endif
	char *nv_lan_ifname;
	char *nv_lan_ifnames;
#ifdef RTCONFIG_LANTIQ
	char ifname_buf[10];
#endif
#if defined(HND_ROUTER) && defined(RTCONFIG_LACP)
	char *lacp_ifs = nvram_safe_get("lacp_ifnames");
	char *lacp1_ifname = *lacp_ifs?strtok(lacp_ifs, " "):NULL;
	char *lacp2_ifname = *lacp_ifs?strtok(NULL, " "):NULL;
#endif

	nv_lan_ifname = nvram_safe_get("lan_ifname");
	nv_lan_ifnames = nvram_safe_get("lan_ifnames");
	from_app = check_user_agent(user_agent);

	if(from_app == 0)
		ret += websWrite(wp, "\nnetdev = {\n");

#ifdef RTCONFIG_LANTIQ
	if ((nvram_get_int("switch_stb_x") == 0 || nvram_get_int("switch_stb_x") > 6) && ppa_support(WAN_UNIT_FIRST)) {
		doSystem("ppacmd getwan > %s", PPACMD_WAN_PATH);
		doSystem("ppacmd getlan > %s", PPACMD_LAN_PATH);
		doSystem("cat %s %s > %s", PPACMD_WAN_PATH, PPACMD_LAN_PATH, PPACMD_TRAFFIC_PATH);
		fp = fopen(PPACMD_TRAFFIC_PATH, "r");
	}
	else
#endif
		fp = fopen("/proc/net/dev", "r");

	if (fp) {
#ifdef RTCONFIG_LANTIQ
		if ((nvram_get_int("switch_stb_x") > 0 && nvram_get_int("switch_stb_x") <= 6) || !ppa_support(WAN_UNIT_FIRST))
#endif
		{
			fgets(buf, sizeof(buf), fp);
			fgets(buf, sizeof(buf), fp);
		}
		comma = ' ';
			while (fgets(buf, sizeof(buf), fp)) {
#ifdef RTCONFIG_LANTIQ
				if ((nvram_get_int("switch_stb_x") > 0 && nvram_get_int("switch_stb_x") <= 6) || !ppa_support(WAN_UNIT_FIRST)) {
#endif
					if ((p = strchr(buf, ':')) == NULL) continue;
					*p = 0;
					if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
						else	++ifname;
					if (sscanf(p + 1, "%llu%*u%*u%*u%*u%*u%*u%*u%llu", &rx, &tx) != 2) continue;
#if defined(HND_ROUTER) && defined(RTCONFIG_LACP)
					if(nvram_get_int("lacp_enabled") == 1) {
						if(lacp1_ifname && strncmp(ifname, lacp1_ifname, strlen(ifname)) == 0) {
							tx_lacp1 = tx;
							rx_lacp1 = rx;
						} else if(lacp2_ifname && strncmp(ifname, lacp2_ifname, strlen(ifname)) == 0) {
							tx_lacp2 = tx;
							rx_lacp2 = rx;
						}
					}
#endif
#ifdef RTCONFIG_LANTIQ
				}
				else
				{
					printf("%s\n", buf);
					if ((p = strchr(buf, '[')) == NULL) continue;
					if (sscanf(buf, "%*s%*s%s%*s%*s%llu", ifname_buf, &rx) != 2) continue;
					if ((p = strchr(buf, ':')) == NULL) continue;
					sscanf(p + 1, "%llu", &tx);
					printf("%s, rx: %llu, tx: %llu\n", ifname_buf, rx, tx);
					ifname = &ifname_buf;
				}
#endif
#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR
				/* WAN1, WAN2, LAN */
				if(strncmp(ifname, "vlan", 4)==0){
					traffic_wanlan(ifname, (uint32_t *) &rx, (uint32_t *) &tx);
#ifdef RTCONFIG_LACP
					if(nvram_get_int("lacp_enabled") == 1 &&
							strcmp(ifname, "vlan1") == 0){
						traffic_trunk(1, (uint32_t *) &rx_lacp1, (uint32_t *) &tx_lacp1);
						netdev_calc("lacp1", ifname_desc,
								(long unsigned int *) &rx_lacp1, (long unsigned int *) &tx_lacp1,
								ifname_desc2_lacp1,
								(long unsigned int *) &rx2_lacp1, (long unsigned int *) &tx2_lacp1,
								nv_lan_ifname, nv_lan_ifnames);

						traffic_trunk(2, (uint32_t *) &rx_lacp2, (uint32_t *) &tx_lacp2);
						netdev_calc("lacp2", ifname_desc,
								(long unsigned int *) &rx_lacp2, (long unsigned int *) &tx_lacp2,
								ifname_desc2_lacp2,
								(long unsigned int *) &rx2_lacp2, (long unsigned int *) &tx2_lacp2,
								nv_lan_ifname, nv_lan_ifnames);
					}
#endif
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
					if (nvram_contains_word("lan_ifnames", ifname)){
						if (rpc_qtn_ready()) {
							qcsapi_interface_get_counter("eth1_1", qcsapi_total_bytes_received, &l_counter_value);
							rx += l_counter_value;
							qcsapi_interface_get_counter("eth1_1", qcsapi_total_bytes_sent,	&l_counter_value);
							tx += l_counter_value;
						}
					}
#endif
				}
				if(nvram_match("wans_dualwan", "wan none")){
					if(strcmp(ifname, WAN_IF_ETH)==0){
						traffic_wanlan(WAN0DEV, (uint32_t *) &rx, (uint32_t *) &tx);
					}
				}
#endif	/* RTCONFIG_BCM5301X_TRAFFIC_MONITOR */
				if (!netdev_calc(ifname, ifname_desc, (unsigned long *) &rx, (unsigned long *) &tx,
				                 ifname_desc2, (unsigned long *) &rx2, (unsigned long *) &tx2,
						 nv_lan_ifname, nv_lan_ifnames))
					continue;

				/* If inode of a interface changed, it means the interface was closed and reopened.
				 * In this case, we should calculate difference of old TX/RX bytes and new TX/RX
				 * bytes and shift from new TX/RX bytes to old TX/RX bytes.
				 */
				inode = get_iface_inode(ifname);
				curr_rx = rx;
				curr_tx = tx;
				if ((ifino = ifname_ino_ptr(&ifstat_tbl, ifname)) != NULL) {
					if (ifino->inode && ifino->inode != inode) {
						ifino->inode = inode;
						ifino->shift_rx = curr_rx - ifino->last_rx + ifino->shift_rx;
						ifino->shift_tx = curr_tx - ifino->last_tx + ifino->shift_tx;
					}
				} else {
					if ((ifstat_tbl.nr_items + 1) <= ARRAY_SIZE(ifstat_tbl.items)) {
						ifino = &ifstat_tbl.items[ifstat_tbl.nr_items];
						strlcpy(ifino->ifname, ifname, sizeof(ifino->ifname));
						ifino->inode = inode;
						ifino->last_rx = curr_rx;
						ifino->last_tx = curr_tx;
						ifino->shift_rx = ifino->shift_tx = 0;
						ifstat_tbl.nr_items++;
					}
				}

				if (ifino != NULL) {
					rx = curr_rx - ifino->shift_rx;
					tx = curr_tx - ifino->shift_tx;
					ifino->last_rx = curr_rx;
					ifino->last_tx = curr_tx;
				}

loopagain:
				if (!strncmp(ifname_desc, "WIRELESS0", 9)) {
					wl0_valid = 1;
					wl0_all_rx += rx;
					wl0_all_tx += tx;
				} else if (!strncmp(ifname_desc, "WIRELESS1", 9)) {
					wl1_valid = 1;
					wl1_all_rx += rx;
					wl1_all_tx += tx;
				} else if (!strncmp(ifname_desc, "WIRELESS2", 9)) {
					wl2_valid = 1;
					wl2_all_rx += rx;
					wl2_all_tx += tx;
				} else if (!strncmp(ifname_desc, "WIRELESS3", 9)) {
					wl3_valid = 1;
					wl3_all_rx += rx;
					wl3_all_tx += tx;
				} else if (!strncmp(ifname_desc, "WIRED", 5)) {
					wired_valid = 1;
					wired_all_rx += rx;
					wired_all_tx += tx;
				} else {
					if(from_app == 0){
#ifdef RTCONFIG_LANTIQ
						if ((nvram_get_int("switch_stb_x") > 0 && !strstr(ifname, "eth1.") && !strstr(ifname, "br1")) || (nvram_get_int("switch_stb_x") == 0))
#endif
						ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, ifname_desc, rx, tx);
					}else{
#ifdef RTCONFIG_LANTIQ
						if ((nvram_get_int("switch_stb_x") > 0 && !strstr(ifname, "eth1.") && !strstr(ifname, "br1")) || (nvram_get_int("switch_stb_x") == 0))
#endif
						ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, ifname_desc, rx, ifname_desc, tx);
					}
						comma = ',';
				}
				if(strlen(ifname_desc2)) {
					strcpy(ifname_desc, ifname_desc2);
					rx = rx2;
					tx = tx2;
					strcpy(ifname_desc2, "");
					goto loopagain;
				}

			}
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G)
			if (rpc_qtn_ready()) {
				qcsapi_interface_get_counter(WIFINAME, qcsapi_total_bytes_received, &l_counter_value);
				wl1_all_rx = l_counter_value;
				qcsapi_interface_get_counter(WIFINAME, qcsapi_total_bytes_sent, &l_counter_value);
				wl1_all_tx = l_counter_value;
				wl1_valid = 1;
			}
#endif
			if(wired_valid) {
				if(from_app == 0) {
					ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, "WIRED", wired_all_rx, wired_all_tx);
				}
				else {
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, "WIRED", wired_all_rx, "WIRED", wired_all_tx);
				}
				comma = ',';
			}
			if (wl0_valid) {
				if(from_app == 0){
					ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, "WIRELESS0", wl0_all_rx, wl0_all_tx);
				}else{
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, "WIRELESS0", wl0_all_rx, "WIRELESS0", wl0_all_tx);
				}
				comma = ',';
			}
			if (wl1_valid) {
				if(from_app == 0){
					ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, "WIRELESS1", wl1_all_rx, wl1_all_tx);
				}else{
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, "WIRELESS1", wl1_all_rx, "WIRELESS1",wl1_all_tx);
				}
				comma = ',';
			}
			if (wl2_valid) {
				if(from_app == 0){
					ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, "WIRELESS2", wl2_all_rx, wl2_all_tx);
				}else{
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, "WIRELESS2", wl2_all_rx, "WIRELESS2",wl2_all_tx);
				}
				comma = ',';
			}
			if (wl3_valid) {
				if(from_app == 0){
					ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, "WIRELESS3", wl3_all_rx, wl3_all_tx);
				}else{
					ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, "WIRELESS3", wl3_all_rx, "WIRELESS3",wl3_all_tx);
				}
				comma = ',';
			}

#ifdef RTCONFIG_LACP
	if(nvram_get_int("lacp_enabled") == 1){
		if(from_app == 0){
			ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, LACP_PORT1, rx_lacp1, tx_lacp1);
			ret += websWrite(wp, "%c'%s':{rx:0x%llx,tx:0x%llx}\n", comma, LACP_PORT2, rx_lacp2, tx_lacp2);
		}
		else{
			ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, LACP_PORT1, rx_lacp1, LACP_PORT1, tx_lacp1);
			ret += websWrite(wp, "%c\"%s_rx\":\"0x%llx\",\"%s_tx\":\"0x%llx\"", comma, LACP_PORT2, rx_lacp2, LACP_PORT2, tx_lacp2);
		}
		comma = ',';
	}
#endif
#ifdef RTCONFIG_LANTIQ
		if ((nvram_get_int("switch_stb_x") == 0 || nvram_get_int("switch_stb_x") > 6) && ppa_support(WAN_UNIT_FIRST)) {
			unlink(PPACMD_WAN_PATH);
			unlink(PPACMD_LAN_PATH);
			unlink(PPACMD_TRAFFIC_PATH);
		}
#endif
		fclose(fp);
			if(from_app == 0)
				ret += websWrite(wp, "}");
  }
  	return 0;
  }

int ej_bandwidth(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name;
	int sig;

	if (strcmp(argv[0], "speed") == 0) {
		sig = SIGUSR1;
		name = "/var/spool/rstats-speed.js";
	}
	else {
		sig = SIGUSR2;
		name = "/var/spool/rstats-history.js";
	}
	unlink(name);
	killall("rstats", sig);
//	eval("killall", sig, "rstats");
	f_wait_exists(name, 5);
	do_f(name, wp);
	unlink(name);
	return 0;
}

//Ren.B
#ifdef RTCONFIG_DSL

// 2014.02 Viz {
int start_dsl_autodet(int eid, webs_t wp, int argc, char **argv) {
	notify_rc("start_dsl_autodet");
	return 0;
}
// }

int ej_spectrum(int eid, webs_t wp, int argc, char_t **argv)
{
	int sig;

	if(nvram_match("spectrum_hook_is_running", "1"))
	{
		//on running status, skip.
		return 0;
	}

	sig = SIGUSR1;
	system("/usr/sbin/check_spectrum.sh"); //check if spectrum is running.
	sleep(1);

	killall("spectrum", sig);

	return 0;

}

int ej_getAnnexMode(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *logFile = fopen( "/tmp/adsl/adsllog.log", "r" );
	char buf[256] = {0};
	char *ptr = NULL;

	if( !logFile )
	{
		printf("Error: adsllog.log does not exist.\n");
		websWrite(wp, "Error: adsllog.log does not exist.");
		return -1;
	}
	while( fgets(buf, sizeof(buf), logFile) )
	{
		if( (ptr=strstr(buf, "Annex Mode :")) != NULL )
		{
			ptr += strlen("Annex Mode :")+1;
			if(strstr(ptr, "Annex A"))
			{
				websWrite(wp, "Annex A" );
			}
			else if(strstr(ptr, "Annex B"))
			{
				websWrite(wp, "Annex B" );
			}
			else
			{
				websWrite(wp, "Error : Unknown Annex Mode" );
			}
			break;
		}
	}

	fclose(logFile);
	return 0;
}

int ej_getADSLToneAmount(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *logFile = fopen( "/tmp/adsl/adsllog.log", "r" );
	char buf[256] = {0};
	char *ptr = NULL;
	int mode = 5;
	int tones = 512;

	if( !logFile )
	{
		printf("Error: adsllog.log does not exist.\n");
		websWrite(wp, "%d", tones);
		return -1;
	}
	while( fgets(buf, sizeof(buf), logFile) )
	{
		if( (ptr=strstr(buf, "Modulation :")) != NULL )
		{
			ptr += strlen("Annex Mode :")+1;
			mode = atoi(ptr);
			break;
		}
	}

	switch(mode)
	{
		case 0:
		case 1:
		case 2:
			tones = 256;
			break;
		case 3:
		case 4:
			tones = 512;
			break;
		default:
			tones = 512;
	}

	fclose(logFile);
	websWrite(wp, "%d", tones);
	return 0;
}

int show_file_content(int eid, webs_t wp, int argc, char_t **argv)
{
	char *name;
	char buffer[256];
	FILE *fp = NULL;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	fp = fopen(name, "r");
	if(!fp)
	{
		ret += websWrite(wp, "");
		return -2;
	}
	while (fgets(buffer, sizeof(buffer), fp))
	{
		ret += websWrite(wp, "%s", buffer);
	}

	fclose(fp);
	return ret;
}
#endif
//Ren.E

int ej_backup_nvram(int eid, webs_t wp, int argc, char_t **argv)
{
	char *list;
	char *p, *k;
	const char *v;

	if ((argc != 1) || ((list = strdup(argv[0])) == NULL)) return 0;
	websWrite(wp, "\nnvram = {\n");
	p = list;
	while ((k = strsep(&p, ",")) != NULL) {
		if (*k == 0) continue;
		v = nvram_get(k);
		if (!v) {
			v = "";
		}
		websWrite(wp, "\t%s: \"", k);
		websWrite(wp, v);
//		web_puts((p == NULL) ? "'\n" : "',\n");
		websWrite(wp, "\",\n");
	}
	free(list);
	websWrite(wp, "\thttp_id: \"");
	websWrite(wp, nvram_safe_get("http_id"));
	websWrite(wp, "\"};\n");
//	web_puts("};\n");
	return 0;
}
// end svg support by Viz ^^^^^^^^^^^^^^^^^^^^

static int
ej_radio_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	retval += websWrite(wp, "radio_2=%d;\nradio_5=%d;", get_radio(0,0), get_radio(1,0));
	return retval;
}

static int
ej_sysinfo(int eid, webs_t wp, int argc, char_t **argv)
{
	char *type;
	char result[2048];
	int retval = 0;

	strcpy(result, "-1");

	if (ejArgs(argc, argv, "%s", &type) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return retval;
	}
	//_dprintf("[sysinfo] type=%s\n", type);

	if(strncmp(type,"pid",3) == 0 ){
		char service[32];
		sscanf(type, "pid.%31s", service);

		if (strlen(service))
			sprintf(result, "%d", pidof(service));

		//_dprintf("[sysinfo] service=%s, result=%d\n", service, result);
	}

	retval += websWrite(wp, result);
	return retval;
}

static int
ej_memory_usage(int eid, webs_t wp, int argc, char_t **argv){
	unsigned long total, used, mfree  /*, shared, buffers, cached, driver occupied*/;
	char buf[80];
	int from_app = 0, i = 0;
	int memSize[] = {4,8,16,32,64,128,256,512,1024};
	int length = sizeof(memSize)/4;
	unsigned long  maxSize = 0, currentSize = 0;

	from_app = check_user_agent(user_agent);
	FILE *fp = NULL;
	fp = fopen("/proc/meminfo", "r");

	if(fp == NULL)
		return -1;

	fscanf(fp, "MemTotal: %lu %s\n", &total, buf);
	fscanf(fp, "MemFree: %lu %s\n", &mfree, buf);
	fclose(fp);

	for(i=0;i<length;i++){
		currentSize = memSize[i]*1024;
		if(currentSize > total){
			maxSize = currentSize;
			break;
		}
	}

	used = maxSize - mfree;	// (maxSize - total) + (total -mfree)
	if(from_app == 0){
		websWrite(wp, "{\"total\":\"%lu\",\"free\":\"%lu\",\"used\":\"%lu\"}", maxSize, mfree, used);
	}else{
		websWrite(wp, "\"mem_total\":\"%lu\",\"mem_free\":\"%lu\",\"mem_used\":\"%lu\"", maxSize, mfree, used);
	}

	return 0;
}

static int
ej_cpu_usage(int eid, webs_t wp, int argc, char_t **argv){
	unsigned long total, user, nice, system, idle, io, irq, softirq;
	char name[10];
	int from_app = 0;

	from_app = check_user_agent(user_agent);

	FILE *fp;
	fp = fopen("/proc/stat", "r");
	int i = 0, firstRow=1;

	if(fp == NULL)
		return -1;
	if(from_app == 0){
		websWrite(wp, "{");
	}

	while(fscanf(fp, "%s %lu %lu %lu %lu %lu %lu %lu \n", name, &user, &nice, &system, &idle, &io, &irq, &softirq) != EOF){
		if(strncmp(name, "cpu", 3) == 0){
			if(i == 0){
				i++;
				continue;
			}

			total = user + nice + system + idle + io + irq + softirq;
			if (firstRow == 1)
				firstRow = 0;
			else
				websWrite(wp, ",");

			if(from_app == 0){
				websWrite(wp, "\"cpu%d\":{\"total\":\"%lu\",\"usage\":\"%lu\"}", i-1, total, total - idle);
			}
			else{
				websWrite(wp, "\"cpu%d_total\":\"%lu\",\"cpu%d_usage\":\"%lu\"", i, total, i, total - idle);
			}
			i++;
		}
		else if(strncmp(name, "intr", 3) == 0){
			break;
		}
	}

	fclose(fp);
	if(from_app == 0)
		websWrite(wp, "}\n");

	return 0;
}

static int
ej_cpu_core_num(int eid, webs_t wp, int argc, char_t **argv){
	char buf[MAX_LINE_SIZE];
	FILE *fp;
	int count = 0;
	fp = fopen("/proc/cpuinfo", "r");

	if(fp == NULL) return -1;

	while(fgets(buf, MAX_LINE_SIZE, fp)!=NULL){
		if(strncmp(buf, "processor", 9) == 0){
			count++;
		}
	}

	fclose(fp);
	if(count == 0){		//for Braomcom ARM single core
		count = 1;
	}

	websWrite(wp, "%d", count);
	return 0;

}

static int
ej_check_pw(int eid, webs_t wp, int argc, char_t **argv)
{
#ifndef RTCONFIG_BCM_MFG
	if(is_passwd_default() && !nvram_match(ATE_FACTORY_MODE_STR(), "1"))
		return websWrite(wp, "1");
	else
#endif
		return websWrite(wp, "0");
}

static int
ej_check_acpw(int eid, webs_t wp, int argc, char_t **argv)
{
	if(!strcmp(nvram_default_get("http_username"), nvram_safe_get("http_username")) && is_passwd_default())
		return websWrite(wp, "1");
	else
		return websWrite(wp, "0");
}

static int
ej_check_acorpw(int eid, webs_t wp, int argc, char_t **argv)
{
	if(!strcmp(nvram_default_get("http_username"), nvram_safe_get("http_username")) || is_passwd_default())
		return websWrite(wp, "\"1\"");
	else
		return websWrite(wp, "\"0\"");
}

#if defined(RTCONFIG_BWDPI)
static int
ej_bwdpi_history(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *hwaddr;
	char *page;
	char *num;

	// get real-time traffic of someone.
	hwaddr = websGetVar(wp, "client", "");

	// get which page for listing
	page = websGetVar(wp, "page", "");

	// the number of data in each page
	num = websGetVar(wp, "num", "");

	//_dprintf("[httpd] history: hwaddr=%s, page=%s, num=%s.\n", hwaddr, page, num);
	get_web_hook(hwaddr, page, num, &retval, wp);

	return retval;
}


/*
	get_bwdpi_hook(type, mode, name, dura, date)

	mode : traffic / traffic_wan / app / client_apps / client_web
	name : NULL / appname / clientname
	dura : realtime / month / week / day
	date : NULL / day
*/

static int
ej_bwdpi_status(int eid, webs_t wp, int argc, char_t **argv)
{
	char *mode, *name, *dura, *date;
	int retval = 0;

	if (ejArgs(argc, argv, "%s %s %s %s", &mode, &name, &dura, &date) < 4) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	// get real-time traffic of someone.
	name = websGetVar(wp, "client", "");

	//_dprintf("[httpd] bwdpi: mode=%s, name=%s, dura=%s, date=%s.\n", mode, name, dura, date);
	 get_traffic_hook(mode, name, dura, date, &retval, wp);

	return retval;
}

static int
ej_bwdpi_redirect_page_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *cat_id;
	int catid;

	// get device info of someone.
	cat_id = websGetVar(wp, "cat_id", "");
	catid = atoi(cat_id);
	redirect_page_status(catid, &retval, wp);

	return retval;
}

static int
ej_bwdpi_appStat(int eid, webs_t wp, int argc, char_t **argv)
{
	char *client, *mode, *dura, *date;
	int retval = 0;

	client = websGetVar(wp, "client", "");
	mode = websGetVar(wp, "mode", "");
	dura = websGetVar(wp, "dura", "");
	date = websGetVar(wp, "date", "");

	// 0: app, 1: mac
	sqlite_Stat_hook(0, client, mode, dura, date, &retval, wp);

	return retval;
}

static int
ej_bwdpi_wanStat(int eid, webs_t wp, int argc, char_t **argv)
{
	char *client, *mode, *dura, *date;
	int retval = 0;

	client = websGetVar(wp, "client", "");
	mode = websGetVar(wp, "mode", "");
	dura = websGetVar(wp, "dura", "");
	date = websGetVar(wp, "date", "");

	// 0: app, 1: mac
	sqlite_Stat_hook(1, client, mode, dura, date, &retval, wp);

	return retval;
}

static int
ej_bwdpi_wanStat_detail(int eid, webs_t wp, int argc, char_t **argv)
{
	char *client, *mode, *dura, *date;
	int retval = 0;

	client = websGetVar(wp, "client", "");
	mode = websGetVar(wp, "mode", "");
	dura = websGetVar(wp, "dura", "");
	date = websGetVar(wp, "date", "");

	// 0: app, 1: mac
	sqlite_Stat_hook(2, client, mode, dura, date, &retval, wp);

	return retval;
}

static int
ej_bwdpi_engine_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	retval += websWrite(wp, "{");
	retval += websWrite(wp, "\"DpiEngine\":%d", check_bwdpi_nvram_setting());
	retval += websWrite(wp, "}");

	return retval;
}

static int
ej_bwdpi_monitor_stat(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	bwdpi_monitor_stat(&retval, wp);

	return retval;
}

static int
ej_bwdpi_monitor_info(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *type = NULL, *event = NULL;

	type = websGetVar(wp, "type", "");
	event = websGetVar(wp, "event", "");

	bwdpi_monitor_info(type, event, &retval, wp);

	return retval;
}

static int
ej_bwdpi_monitor_ips(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *type = NULL, *date = NULL;

	type = websGetVar(wp, "type", "");
	date = websGetVar(wp, "date", "");

	bwdpi_monitor_ips(type, date, &retval, wp);

	return retval;
}

static int
ej_bwdpi_monitor_nonips(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	char *type = NULL, *date = NULL;

	type = websGetVar(wp, "type", "");
	date = websGetVar(wp, "date", "");

	bwdpi_monitor_nonips(type, date, &retval, wp);

	return retval;
}
#else
static int
ej_bwdpi_engine_status(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	retval += websWrite(wp, "{");
	retval += websWrite(wp, "}");

	return retval;
}
#endif

#ifdef RTCONFIG_TRAFFIC_LIMITER
static int
ej_traffic_limiter_wanStat(int eid, webs_t wp, int argc, char_t **argv)
{
	char *ifname, *start, *end, *unit;
	int retval = 0;

	ifname = websGetVar(wp, "ifname", "");
	start = websGetVar(wp, "start", "");
	end = websGetVar(wp, "end", "");
	unit = websGetVar(wp, "unit", "");

	traffic_limiter_hook(ifname, start, end, unit, &retval, wp);

	return retval;
}
#endif

static int
ej_wl_nband_info(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit = 0, ret = 0, firstRow = 1;
	char *band, word[256], *next;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	ret += websWrite(wp, "[");
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		//SKIP_ABSENT_BAND_AND_INC_UNIT(unit);	/* Let UI skip absent 5G-2 */
		if (firstRow == 1)
			firstRow = 0;
		else
			ret += websWrite(wp, ", ");
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		band = nvram_safe_get(strcat_r(prefix, "nband", tmp));
	
		ret += websWrite(wp, "'%s'", band);

		unit++;
	}
	ret += websWrite(wp, "]");
	return ret;
}

#ifdef RTCONFIG_GEOIP
static int
ej_geoiplookup(int eid, webs_t wp, int argc, char_t **argv)
{
	int unit;
	char tmp[100], prefix[] = "wanXXXXXXXXXX_";
	char *wanip;

	unit = wan_primary_ifunit();
	wan_prefix(unit, prefix);
	wanip = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));

	return websWrite(wp, "%s", geoiplookup_by_ip(wanip));
}
#endif

#ifdef RTCONFIG_JFFS2USERICON
static int
ej_get_upload_icon(int eid, webs_t wp, int argc, char **argv) {
	char *client_mac = websGetVar(wp, "clientmac", "");
	int from_app = 0;

	from_app = check_user_agent(user_agent);

	if(client_mac != NULL) {
		if(strcmp(client_mac, "") && strlen(client_mac) == 12) {
			char file_name[32];
			memset(file_name, 0, 32);

			//Check folder exist or not
			if(!check_if_dir_exist(JFFS_USERICON))
				mkdir(JFFS_USERICON, 0755);

			//Write upload icon value
			sprintf(file_name, "/jffs/usericon/%s.log", client_mac);
			if(check_if_file_exist(file_name)) {
				if(from_app != 0)
					websWrite(wp, "\"");
				dump_file(wp, file_name);
				if(from_app != 0)
					websWrite(wp, "\"");
			}
			else {
				if(from_app != 0)
					websWrite(wp, "\"");
				websWrite(wp, "NoIcon");
				if(from_app != 0)
					websWrite(wp, "\"");
			}
		}
		else {
			if(from_app != 0)
				websWrite(wp, "\"");
			websWrite(wp, "NoIcon");
			if(from_app != 0)
				websWrite(wp, "\"");
		}
	}
	else {
		if(from_app != 0)
			websWrite(wp, "\"");
		websWrite(wp, "NoIcon");
		if(from_app != 0)
			websWrite(wp, "\"");
	}

	return 0;
}
static int
ej_get_upload_icon_count_list(int eid, webs_t wp, int argc, char **argv) {
	int file_count = 0;
	DIR *dirp;
	struct dirent * entry;
	char allMacList[1500];
	memset(allMacList, 0, 1500);
	int from_app = 0;

	from_app = check_user_agent(user_agent);

	//Check folder exist or not
	if(!check_if_dir_exist(JFFS_USERICON))
		mkdir(JFFS_USERICON, 0755);

	//Write /jffs/usericon/ file count and list
	if ((dirp = opendir(JFFS_USERICON)) == NULL)
		return 0;

	while ((entry = readdir(dirp)) != NULL) {
		if (entry->d_type == DT_REG) { /* If the entry is a regular file */
			strcat(allMacList, entry->d_name);
			strcat(allMacList, ">");
			file_count++;
		}
	}
	closedir(dirp);
	if(from_app == 0){
		websWrite(wp, "upload_icon_count=\"%d\";\n", file_count);
		websWrite(wp, "upload_icon_list=\"%s\";\n", allMacList);
	}else{
		websWrite(wp, "\"upload_icon_count\":\"%d\",\n", file_count);
		websWrite(wp, "\"upload_icon_list\":\"%s\"\n", allMacList);
	}

	return 0;
}
#endif

static int
ej_findasus(int eid, webs_t wp, int argc, char **argv) {
	char *buf, *g, *p;
	char strTmp[4096]={0}, retList[65536]={0};
	char *type, *name, *ip, *mac, *netmask, *opmode, *ssid, *submask;
	int isfirst=0;

	eval("asusdiscovery");	//find asus device

	g = buf = strdup(nvram_safe_get("asus_device_list"));
	strcat(retList, "[\n");
   	if(strcmp(buf, "") != 0){
		while (buf) {
			if ((p = strsep(&g, "<")) == NULL) break;

			if((vstrsep(p, ">",&type ,&name, &ip, &mac, &netmask, &ssid, &submask, &opmode)) != 8) continue;

			if(isfirst == 0){
				isfirst = 1;
			}else{
				strcat(retList, ",\n");
			}

			strcat(retList, "{");
			sprintf(strTmp, "modelName:\"%s\",", name);
			strcat(retList, strTmp);
			if(check_xss_blacklist(ssid, 0))
				sprintf(strTmp, "ssid:\"\",");
			else
				sprintf(strTmp, "ssid:\"%s\",", ssid);
			strcat(retList, strTmp);
			sprintf(strTmp, "ipAddr:\"%s\"", ip);
			strcat(retList, strTmp);
			strcat(retList, "}");
		}
	}else{
		sprintf(strTmp, "{modelName:\"%s\",ssid:\"%s\",ipAddr:\"%s\"}", get_productid(), nvram_safe_get("wl_ssid"), nvram_safe_get("lan_ipaddr"));
		strcat(retList, strTmp);
	}
	free(buf);
	strcat(retList, "\n]");
	return websWrite(wp, "%s", retList);
}

#ifdef RTCONFIG_WTFAST
static int
ej_wtfast_status(int eid, webs_t wp, int argc, char **argv) {
	int ret = 0;
	ret = dump_file(wp, "/tmp/wtf_status");
	if(!ret){
		char wtfast_status[4096] = {0};
		char tmp[1024] = {0};
		char val[512] = {0};

		strcat(wtfast_status, "{");

		sprintf(tmp, "\"eMail\":\"\",");
		strcat(wtfast_status, tmp);

		sprintf(tmp, "\"Account_Type\":\"\",");
		strcat(wtfast_status, tmp);

		sprintf(tmp, "\"Max_Computers\": 0,");
		strcat(wtfast_status, tmp);

		memset(val, 0, 512);
		sprintf(tmp, "\"Server_List\":[],");
		strcat(wtfast_status, tmp);

		sprintf(tmp, "\"Game_List\":[],");
		strcat(wtfast_status, tmp);

		sprintf(tmp, "\"Days_Left\": 0,");
		strcat(wtfast_status, tmp);

		sprintf(tmp, "\"Login_status\": 0,");
		strcat(wtfast_status, tmp);

		sprintf(tmp, "\"Session_Hash\":\"\"");
		strcat(wtfast_status, tmp);

		strcat(wtfast_status, "}");

		return websWrite(wp, "%s", wtfast_status);
	}

	return ret;
}

#endif

static int
ej_check_ftp_samba_anonymous(int eid, webs_t wp, int argc, char **argv){

	char *name = NULL;
	int samba_mode=0, ftp_mode=0, ret=0;

	if (ejArgs(argc, argv, "%s", &name) < 1) {
		//_dprintf("name = NULL\n");
		ret = websWrite(wp, "Not support");
		return ret;
	}

	if(!strcmp(name,"cifs")){
		if((nvram_get("st_samba_force_mode") == NULL && nvram_get_int("st_samba_mode") == 1)){
			samba_mode = 4;
		}else{
			samba_mode = nvram_get_int("st_samba_mode");
		}
		if(samba_mode == 2 || samba_mode == 4){
			ret = websWrite(wp, "\"1\"");
		}else{
			ret = websWrite(wp, "\"0\"");
		}
	}else if(!strcmp(name,"ftp")){
		if((nvram_get("st_ftp_force_mode") == NULL && nvram_get_int("st_ftp_mode") == 1)){
			ftp_mode = 2;
		}else{
			ftp_mode = nvram_get_int("st_ftp_mode");
		}
		if(ftp_mode == 2){
			ret = websWrite(wp, "\"1\"");
		}else{
			ret = websWrite(wp, "\"0\"");
		}
	}
	return ret;
}

static int
ej_check_passwd_strength(int eid, webs_t wp, int argc, char **argv){

	int ret=0;
	char *name = NULL;
	if (ejArgs(argc, argv, "%s", &name) < 1) {
		ret = websWrite(wp, "Not support");
		//_dprintf("name = NULL\n");
		return ret;
	}

	int unit = 0;
	int nScore_total=0, nScore=0;
	char *pwd = NULL, word[256]={0}, *next = NULL;
	char tmp[128]={0}, prefix[] = "wlXXXXXXXXXX_";
	int nLength=0, nConsecAlphaUC=0, nConsecCharType=0, nAlphaUC=0, nConsecAlphaLC=0, nAlphaLC=0, nMidChar=0, nConsecNumber=0, nNumber=0, nConsecSymbol=0, nSymbol=0, nRepChar=0, nUnqChar=0, nSeqAlpha=0, nSeqNumber=0, nSeqChar = 0, nSeqSymbol=0;
	int nMultMidChar=2, nMultLength=4, nMultNumber=4, nMultSymbol=6, nMultConsecAlphaUC=2, nMultConsecAlphaLC=2, nMultConsecNumber=2, nMultSeqAlpha=3, nMultSeqNumber=3, nMultSeqSymbol=3;
	int a=0, b=0, s=0, x=0;
	int nTmpAlphaUC = -1, nTmpAlphaLC = -1, nTmpNumber = -1, nTmpSymbol = -1;
	double nRepInc = 0.0;
	char sAlphas[] = "abcdefghijklmnopqrstuvwxyz";
	char sNumerics[] = "01234567890";
	char sSymbols[] = "~!@#$%^&*()_+";
	char pwd_s[128] = {0};
	char *pwd_st=NULL, *arrPwd=NULL;
	char *auth_mode=NULL;
	char sFwd[4], sFwd_t[4], sRev[4];
	if(!strcmp(name,"wl_key")){
		foreach (word, nvram_safe_get("wl_ifnames"), next) {
			SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			pwd = nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp));
			auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
			nLength=0; nConsecAlphaUC=0; nConsecCharType=0; nAlphaUC=0; nConsecAlphaLC=0; nAlphaLC=0; nMidChar=0; nConsecNumber=0; nNumber=0; nConsecSymbol=0; nSymbol=0; nRepChar=0; nUnqChar=0; nSeqAlpha=0; nSeqNumber=0; nSeqChar = 0; nSeqSymbol=0;
			nTmpAlphaUC = -1; nTmpAlphaLC = -1; nTmpNumber = -1; nTmpSymbol = -1;
		   if(pwd != NULL && (!strcmp(auth_mode,"psk2") || !strcmp(auth_mode,"pskpsk2") || !strcmp(auth_mode,"wpa2") || !strcmp(auth_mode,"wpawpa2") || !strcmp(auth_mode,"psk2sae") || !strcmp(auth_mode,"sae"))){
			nScore=0;
			nLength=0;
			pwd_st = pwd;
			arrPwd = pwd;
			nLength = strlen(pwd);
			nScore = nLength * nMultLength;

			/* Main calculation for strength:
					Loop through password to check for Symbol, Numeric, Lowercase and Uppercase pattern matches */
			for (a=0; a <nLength; a++){
				if(isupper(arrPwd[a])){
					if(nTmpAlphaUC != -1){
						if((nTmpAlphaUC + 1) == a){
							nConsecAlphaUC++;
							nConsecCharType++;
						}
					}
					nTmpAlphaUC = a;
					nAlphaUC++;
				}
				else if(islower(arrPwd[a])){
					if(nTmpAlphaLC != -1){
						if((nTmpAlphaLC + 1) == a){
							nConsecAlphaLC++;
							nConsecCharType++;
						}
					}
					nTmpAlphaLC = a;
					nAlphaLC++;
				}
				else if(isdigit(arrPwd[a])){
					if(a > 0 && a < (nLength - 1)){
						nMidChar++;
					}
					if(nTmpNumber != -1){
						if((nTmpNumber + 1) == a){
							nConsecNumber++;
							nConsecCharType++;
						}
					}
					nTmpNumber = a;
					nNumber++;
				}
				else if(!isalnum(arrPwd[a])){
					if(a > 0 && a < (nLength - 1))
					{
						nMidChar++;
					}
					if(nTmpSymbol != -1){
						if((nTmpSymbol + 1) == a){
							nConsecSymbol++;
							nConsecCharType++;
						}
					}
					nTmpSymbol = a;
					nSymbol++;
				}

				/* Internal loop through password to check for repeat characters */
				int bCharExists = 0;
				for (b=0; b < nLength; b++){
					if (arrPwd[a] == arrPwd[b] && a != b){ /* repeat character exists */
						bCharExists = 1;
						/*
						Calculate icrement deduction based on proximity to identical characters
						Deduction is incremented each time a new match is discovered
						Deduction amount is based on total password length divided by the
						difference of distance between currently selected match
						*/
						nRepInc += abs(nLength/(b-a));
					}
				}
				if (bCharExists == 1) {
					nRepChar++;
					nUnqChar = nLength-nRepChar;
					nRepInc = (nUnqChar > 0) ? ceil(nRepInc/(double)nUnqChar) : ceil(nRepInc);
				}
			}

			for(x = 0; x < nLength; x++){
				pwd_s[x]= tolower(*pwd_st);
				pwd_st++;
			}

			/* Check for sequential alpha string patterns (forward and reverse) */
			for (s=0; s < 23; s++){
				memset(sFwd, 0, sizeof(sFwd));
				memset(sFwd_t, 0, sizeof(sFwd_t));
				memset(sRev, 0, sizeof(sRev));
				if(sAlphas+s != '\0'){
					strncpy(sFwd, sAlphas+s, 3);
					strncpy(sFwd_t, sFwd, 3);
				}
				strcpy(sRev, reverse_str(sFwd));

				if(strstr(pwd_s, sFwd_t) != NULL || strstr(pwd_s, sRev) != NULL){
					nSeqAlpha++;
					nSeqChar++;
				}
			}
			/* Check for sequential numeric string patterns (forward and reverse) */
			for (s=0; s < 8; s++) {
				memset(sFwd, 0, sizeof(sFwd));
				memset(sFwd_t, 0, sizeof(sFwd_t));
				memset(sRev, 0, sizeof(sRev));
				if(sNumerics+s != '\0'){
					strncpy(sFwd, sNumerics+s, 3);
					strncpy(sFwd_t, sFwd, 3);
				}
				strcpy(sRev, reverse_str(sFwd));
				if(strstr(pwd_s, sFwd_t) != NULL || strstr(pwd_s, sRev) != NULL){
					nSeqNumber++;
					nSeqChar++;
				}
			}
			/* Check for sequential symbol string patterns (forward and reverse) */
			for (s=0; s < 8; s++) {
				memset(sFwd, 0, sizeof(sFwd));
				memset(sFwd_t, 0, sizeof(sFwd_t));
				memset(sRev, 0, sizeof(sRev));
				if(sSymbols+s != '\0'){
					strncpy(sFwd, sSymbols+s, 3);
					strncpy(sFwd_t, sFwd, 3);
				}
				strcpy(sRev, reverse_str(sFwd));
				if(strstr(pwd_s, sFwd_t) != NULL || strstr(pwd_s, sRev) != NULL){
					nSeqSymbol++;
					nSeqChar++;
				}
			}
			/* Modify overall score value based on usage vs requirements */

			/* General point assignment */
			if (nAlphaUC > 0 && nAlphaUC < nLength){
				nScore = nScore + ((nLength - nAlphaUC) * 2);
			}
			if (nAlphaLC > 0 && nAlphaLC < nLength){
				nScore = nScore + ((nLength - nAlphaLC) * 2);
			}
			if (nNumber > 0 && nNumber < nLength){
				nScore = nScore + (nNumber * nMultNumber);
			}
			if (nSymbol > 0){
				nScore = nScore + (nSymbol * nMultSymbol);
			}
			if (nMidChar > 0){
				nScore = nScore + (nMidChar * nMultMidChar);
			}
			/* Point deductions for poor practices */
			if ((nAlphaLC > 0 || nAlphaUC > 0) && nSymbol == 0 && nNumber == 0) {  // Only Letters
				nScore = nScore - nLength;
			}
			if (nAlphaLC == 0 && nAlphaUC == 0 && nSymbol == 0 && nNumber > 0) {  // Only Numbers
				nScore = nScore - nLength;
			}
			if (nRepChar > 0) {  // Same character exists more than once
				nScore = nScore - nRepInc;
			}
			if (nConsecAlphaUC > 0) {  // Consecutive Uppercase Letters exist
			nScore = nScore - (nConsecAlphaUC * nMultConsecAlphaUC);
			}
			if (nConsecAlphaLC > 0) {  // Consecutive Lowercase Letters exist
				nScore = nScore - (nConsecAlphaLC * nMultConsecAlphaLC);
			}
			if (nConsecNumber > 0) {  // Consecutive Numbers exist
				nScore = nScore - (nConsecNumber * nMultConsecNumber);
			}
			if (nSeqAlpha > 0) {  // Sequential alpha strings exist (3 characters or more)
				nScore = nScore - (nSeqAlpha * nMultSeqAlpha);
			}
			if (nSeqNumber > 0) {  // Sequential numeric strings exist (3 characters or more)
				nScore = nScore - (nSeqNumber * nMultSeqNumber);
			}
			if (nSeqSymbol > 0) {  // Sequential symbol strings exist (3 characters or more)
				nScore = nScore - (nSeqSymbol * nMultSeqSymbol);
			}

			/* Determine complexity based on overall score */
			if (nScore > 100)
			{
				nScore = 100;
			}else if(nScore < 0)
			{
				nScore = 0;
			}
			nScore_total = nScore + nScore_total;
		   }else{
			nScore = 0;
		   }
		   unit++;
		}
	}
	websWrite(wp, "\"%d\"", (int)(nScore_total/unit));
	return ret;
}

static int
ej_check_wireless_encryption(int eid, webs_t wp, int argc, char **argv){

	int unit=0;
	char *auth_mode=NULL;
	char word[256]={0}, *next = NULL;
	char tmp[128]={0}, prefix[] = "wlXXXXXXXXXX_";

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		auth_mode = nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp));
		if(!strcmp(auth_mode,"psk2") || !strcmp(auth_mode,"pskpsk2") || !strcmp(auth_mode,"wpa2") || !strcmp(auth_mode,"wpawpa2") ||
		   !strcmp(auth_mode,"psk2sae") || !strcmp(auth_mode,"sae"))
			;
		else
			return websWrite(wp, "\"0\"");
		unit++;
	}
	return websWrite(wp, "\"1\"");
}

static int
ej_get_next_lanip(int eid, webs_t wp, int argc, char **argv)
{
#if defined(RTCONFIG_TAGGED_BASED_VLAN)
#define NEXT_LANIP_INTERVAL	5	/* unit: seconds */
	static time_t t1 = 0;
	static char last_result[sizeof("192.168.100.200XXX")] = "";
	int r;
	uint32_t id;
	char ip_mask[32], new_ip[sizeof("192.168.100.200XXX")] = "", *p, *ip = ip_mask, *mask;
	struct in_addr net = { 0 }, host = { 0 }, nmask = { 0 }, hmask = { 0 };

	/* Cache last result for NEXT_LANIP_INTERVAL seconds. */
	if (t1 != 0 && (uptime() - t1) <= NEXT_LANIP_INTERVAL && *last_result != '\0') {
		return websWrite(wp, "{next_lanip: '%s'}", last_result);
	}

	snprintf(new_ip, sizeof(new_ip), "%s", nvram_safe_get("lan_ipaddr"));
	snprintf(ip_mask, sizeof(ip_mask), "%s/%s", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
	r = test_and_get_free_char_network(7, ip_mask, EXCLUDE_NET_LAN);
	if (r == 1) {
		/* Got free network segment. */
		if ((p = strchr(ip_mask, '/')) != NULL) {
			*p = '\0';
			mask = p + 1;
			if (inet_aton(ip, &net) == 1 &&
			    inet_aton(mask, &nmask) == 1 &&
			    (net.s_addr & nmask.s_addr) == net.s_addr)
			{
				/* ip_mask doesn't include host id, find first one. */
				hmask.s_addr = ~nmask.s_addr;
				host.s_addr = net.s_addr & hmask.s_addr;
				id = be32_to_cpu(host.s_addr) + 1;
				host.s_addr = cpu_to_be32(id);
				host.s_addr |= net.s_addr;
				strlcpy(new_ip, inet_ntoa(host), sizeof(new_ip));
			} else {
				strlcpy(new_ip, ip, sizeof(new_ip));
			}
		} else {
			dbg("%s: Invalid ip_mask [%s]!\n", __func__, ip_mask);
		}
		_dprintf("ej_get_next_lanip: new_ip = %s\n", new_ip);
	}
	strlcpy(last_result, new_ip, sizeof(last_result));
	t1 = uptime();

	return websWrite(wp, "{next_lanip: '%s'}", new_ip);

#else

	char tmp[100], prefix[sizeof("wanXXXXXXXXXX_")];
	char *lan_ipaddr, *lan_netmask;
	char *wan_ipaddr, *wan_netmask;
	struct in_addr addr;
	int unit;

	lan_ipaddr = nvram_safe_get("lan_ipaddr");
	lan_netmask = nvram_safe_get("lan_netmask");

	if(nvram_match("wans_mode", "lb")){
		for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit){
			if (!dualwan_unit__usbif(unit)) {
				snprintf(prefix, sizeof(prefix), "wan%d_", unit);
				wan_ipaddr = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
				wan_netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
			}
			else{
				/* force conflict per original design */
				wan_ipaddr = lan_ipaddr;
				wan_netmask = lan_netmask;
			}

			if (inet_deconflict(lan_ipaddr, lan_netmask, wan_ipaddr, wan_netmask, &addr))
				lan_ipaddr = inet_ntoa(addr);
		}
	}
	else{
		unit = get_primaryif_dualwan_unit();
		if (unit < 0)
			goto error;

		if (!dualwan_unit__usbif(unit)) {
			snprintf(prefix, sizeof(prefix), "wan%d_", unit);
			wan_ipaddr = nvram_safe_get(strcat_r(prefix, "ipaddr", tmp));
			wan_netmask = nvram_safe_get(strcat_r(prefix, "netmask", tmp));
		} else {
			/* force conflict per original design */
			wan_ipaddr = lan_ipaddr;
			wan_netmask = lan_netmask;
		}

		if (inet_deconflict(lan_ipaddr, lan_netmask, wan_ipaddr, wan_netmask, &addr))
			lan_ipaddr = inet_ntoa(addr);
	}

error:
	return websWrite(wp, "{next_lanip: '%s'}", lan_ipaddr);

#endif
}

#ifdef RTCONFIG_INTERNAL_GOBI
static int
ej_chk_lte_fw(int eid, webs_t wp, int argc, char **argv) {
	int i;
	char *dev_path;
	char buf[256], tmpstr[128], version[128];
	FILE *fp;
	char GOBI_FW_PATH[32];

	for(i = 1; i < 4; i++)
	{
		FILE *aFile;

		sprintf(buf, "usb_path%d", i);

		if (strcmp(nvram_safe_get(buf), "storage") != 0)
			continue;

		sprintf(buf, "usb_path%d_fs_path0", i);
		dev_path = nvram_get(buf);
		if(dev_path == NULL || strlen(dev_path) <= 0)
			continue;

		do {
			struct mntent *ent;

			aFile = setmntent("/proc/mounts", "r");
			if (aFile == NULL) {
				perror("setmntent");
				break;
			}
			while (NULL != (ent = getmntent(aFile))) {
				char *p;
				p = strstr(ent->mnt_fsname, dev_path);
				if (p != NULL && *(p-1) == '/' && *(p + strlen(dev_path)) == '\0')
				{
					snprintf(GOBI_FW_PATH, sizeof(GOBI_FW_PATH), "%s", nvram_safe_get("modem_gobi_path"));
					snprintf(buf, sizeof(buf), "%s/%s/version", ent->mnt_dir, GOBI_FW_PATH);
					if(!check_if_file_exist(buf))
						continue;
#if 1
					snprintf(buf, sizeof(buf), "%s/%s/update.md5", ent->mnt_dir, GOBI_FW_PATH);
					if(!check_if_file_exist(buf))
						continue;
					snprintf(buf, sizeof(buf), "%s/%s/update.zip", ent->mnt_dir, GOBI_FW_PATH);
					if(!check_if_file_exist(buf))
						continue;
#else
					snprintf(buf, sizeof(buf), "cd %s/%s && md5sum -c update.md5", ent->mnt_dir, GOBI_FW_PATH);
					if(system(buf) == 0)
#endif
					{
						snprintf(buf, sizeof(buf), "%s/%s/version", ent->mnt_dir, GOBI_FW_PATH);
						//do_file(buf, wp);
						if((fp = fopen(buf, "r")) != NULL){
							while (fgets(tmpstr, sizeof(tmpstr), fp) != NULL)
							{
								strncpy(version, tmpstr, strlen(tmpstr)-1);
								version[strlen(tmpstr)-1] = '\0';
							}
							fclose(fp);
						}
					}

					endmntent(aFile);
					return websWrite(wp, "%s", version);
				}
			}
		}while(0);
		endmntent(aFile);
	}
	return 0;
}
#endif	// RTCONFIG_INTERNAL_GOBI

static int
ej_check_asus_model(int eid, webs_t wp, int argc, char **argv)
{
	return websWrite(wp, "\"1\"");
}

#ifdef RTCONFIG_IPSEC

#define PROFILE_NUM 5
#define PROFILE_NAME_LENTH 64    

typedef struct ipsec_conn_token_table ipsec_conn_token_t;
struct ipsec_conn_token_table{
	char profile_name[PROFILE_NAME_LENTH];
	char ipaddr[16];
	int conn_status;
	char conn_period[16];
	char xauth_account[32];
	char psk_reauth_time[16];
	ipsec_conn_token_t *next;
};

static int
ej_get_ipsec_conn(int eid, webs_t wp, int argc, char_t **argv)
{
	char *ipsec_argv[] = {"ipsec", "statusall", NULL};
	FILE *fp;
    char line[256];
    int first_time=1;
    char *tmp_ip_start, *tmp_name, *tmp_ip_end, *tmp_xauth,*tmp_period_s,*tmp_period_e,*tmp_reauth;
    char dest_name[32],dest_ip[32],dest_xauth[32],dest_period[32],dest_reauth[32];
    int profile_count=0;
    int i=0;
    int new_profile=0;
    int ret = 0;
    int len=0;
	fpos_t pos;
	ipsec_conn_token_t *profile[PROFILE_NUM];
	
	memset(dest_name,0,sizeof(dest_name));
	memset(dest_ip,0,sizeof(dest_ip));
	memset(dest_xauth,0,sizeof(dest_xauth));
	memset(dest_period,0,sizeof(dest_period));
	memset(dest_reauth,0,sizeof(dest_reauth));
	
	_eval(ipsec_argv, ">/tmp/ipsec_conn", 0, NULL);
	
	ret += websWrite(wp, "[");
	if ((fp = fopen("/tmp/ipsec_conn", "r")) != NULL) {
		while (fgets(line, sizeof(line), fp)) {
			new_profile = 1;
			if(strstr(line,"ESTABLISHED") != NULL || strstr(line,"CONNECTING") != NULL){
				/* profile_name */
				ipsec_conn_token_t *data = (ipsec_conn_token_t *)malloc(sizeof(ipsec_conn_token_t));
				data->next=NULL;
				tmp_name = index(line,'[');
				len = strlen(line)-strlen(tmp_name);
				strncpy(dest_name,line,len);
				dest_name[len] = '\0';
				while(strchr(dest_name,' ')!=NULL)
					strcpy(dest_name,dest_name+1);
				strcpy(data->profile_name,dest_name);
				
				/* ipaddr */
				tmp_ip_start = index(line,',');
				tmp_ip_start = index(tmp_ip_start,']');
				tmp_ip_end = rindex(line,'[');
				len = strlen(tmp_ip_start) - strlen(tmp_ip_end);
				strncpy(dest_ip,tmp_ip_start,len);
				dest_ip[len] = '\0';
				strcpy(dest_ip,dest_ip+4);
				strcpy(data->ipaddr,dest_ip);
					
				if(strstr(line,"ESTABLISHED") != NULL) 
					data->conn_status = 3;	
				if(strstr(line,"CONNECTING") != NULL)
					data->conn_status = 2;
				
				if(strstr(line,"ESTABLISHED") != NULL) {
					/* connected period */	
					tmp_period_s = index(line,'D')+2;
					tmp_period_e = index(line,',')-4;
					len = strlen(tmp_period_s)-strlen(tmp_period_e);
					strncpy(dest_period,tmp_period_s,len);
					dest_period[len] = '\0';
					strcpy(data->conn_period,dest_period);
				}
						
				/* xauth users */
				fgets(line, sizeof(line), fp);
				if(strstr(line,"Remote XAuth identity:") != NULL){
					tmp_xauth = rindex(line,' ') + 1;
					len = strlen(tmp_xauth);
					strncpy(dest_xauth,tmp_xauth,len);
				dest_xauth[len-1] = '\0';
				strcpy(data->xauth_account,dest_xauth);
					fgets(line, sizeof(line), fp);
				}else
					strcpy(data->xauth_account,"");
				
				/* reauthentication times */
				if(strstr(line,"reauthentication") != NULL){
					tmp_reauth = index(line,',')+37;
					len = strlen(tmp_reauth);
					strncpy(dest_reauth,tmp_reauth,len);
				dest_reauth[len-1] = '\0';
				strcpy(data->psk_reauth_time,dest_reauth);
				}else
					strcpy(data->psk_reauth_time,"");


				/* 1: connected, 2: connecting */
				fgets(line, sizeof(line), fp);
				fgetpos(fp, &pos);
				fgets(line, sizeof(line), fp);
				if(strstr(line,"ESTABLISHED") != NULL || strstr(line,"CONNECTING") != NULL)
					fsetpos(fp, &pos);
				
				if(strstr(line,"INSTALLED") != NULL || strstr(line,"REKEYING") != NULL)
					data->conn_status = 1;
				if(first_time==1)
				{
					profile[profile_count] = data;
					profile_count++;
				}

				for(i=0;i<profile_count;i++)
					{	
					if(strncmp(profile[i]->profile_name,data->profile_name,strlen(data->profile_name)) == 0) {
						new_profile = 0;
						break;
					}
				}
					
				if(new_profile == 1) {
					profile[profile_count] = data;
					profile_count++;
				}
				else {
					ipsec_conn_token_t *find_p;
					find_p=profile[i];
					while(1) {
						if(find_p->next != NULL) {
							find_p=find_p->next;
						}
						else {
							if(first_time==1) {
								first_time=0;
								break;
							}
							find_p->next=data;
							break;
						}
					}
				}
			}
		}
		
		for(i=0;i<profile_count;i++)
		{
			ipsec_conn_token_t *find_p;
			find_p=profile[i];
						
			if(i!=0)	
				ret += websWrite(wp, ",");
			ret += websWrite(wp, "[\'%s\',\'", find_p->profile_name);
			while(1)
			{
				ret += websWrite(wp, "<%s>%d>%s>%s>%s",find_p->ipaddr,find_p->conn_status,find_p->conn_period,find_p->xauth_account,find_p->psk_reauth_time);
				//_dprintf("<%s>%d>%s>%s>%s\n", find_p->ipaddr,find_p->conn_status,find_p->conn_period,find_p->xauth_account,find_p->psk_reauth_time);

				if(find_p->next != 0)
				{
					free(find_p);
					find_p=find_p->next;
				}
				else
				{	
					free(find_p);
					break;
				}	
			}
			ret += websWrite(wp, "\']");
		}
		fclose(fp);
	}
	
    unlink("/tmp/ipsec_conn");
	ret += websWrite(wp, "]");
	
	return ret;
}
#endif

#ifdef RTCONFIG_CAPTIVE_PORTAL
static int ej_get_CPInfo(int eid, webs_t wp, int argc, char_t **argv)
{
	char out[256];
	char cmd[64];
	int ret=0;
	FILE *p_fp;
	char *empty_str="{ \"pass\":[], \"failed\":[] }";
	memset(out, 0, sizeof(out));
	memset(cmd, 0, sizeof(cmd));

	sprintf(cmd, "%s", "chilli_query -P 42425 -json list");
	if((p_fp=popen(cmd, "r")) != NULL){
		while (!feof(p_fp)){
			if(fgets(out, sizeof(out), p_fp)){
				if (strlen(out) > 0){
					ret=websWrite(wp, "%s",out);
				}else{
					ret=websWrite(wp, "%s", empty_str);
				}
			}
		}
		pclose(p_fp);
	}
	return ret;
}
#endif

static int
ej_chdom(int eid, webs_t wp, int argc, char **argv)
{
	char str[32];
	memset(str, 0, sizeof(str));
	char *hostname = websGetVar(wp, "hostname", "");

	ipisdomain(hostname,str);

	websWrite(wp, "%s", str);

	return 0;
}

#ifdef RTCONFIG_AMAS
static int
ej_chcap(int eid, webs_t wp, int argc, char **argv)
{
	char str[32];
	memset(str, 0, sizeof(str));

	iscap(str);

	websWrite(wp, "%s", str);

	return 0;
}

#ifdef RTCONFIG_ADV_RAST
static int ej_chksta(int eid, webs_t wp, int argc, char **argv){
	nvram_set("enable_diag", "1");
	//kill_pidfile_s("/var/run/conn_diag.pid", SIGUSR1);
	//sleep(1);

	return 0;
}

#define LOG_DIR "/tmp/asusdebuglog"
#define WL_NBAND_2G 2
#define WL_NBAND_5G 1

// node number is 0: cap, 1: re1, 2: re2, ...etc.
char *convert_nodenum_to_str(int node, char *str, int len){
	if(node < 0)
		return NULL;

	if(node == 0)
		snprintf(str, len, "cap");
	else
		snprintf(str, len, "re%d", node);

	return str;
}

static char *get_node_band(int idx, char *buf, int buflen){
	if(idx > 1)
		snprintf(buf, buflen, "5G%d", idx);
	else if(idx == 1)
		snprintf(buf, buflen, "5G");
	else
		snprintf(buf, buflen, "2G");

	return buf;
}

static int ej_show_info_between_nodes(int eid, webs_t wp, int argc, char **argv){
	int re_num = 0;
	int i, j;
	char band[4];
	char fname[64], data[128];
	char src_str[8], dst_str[8];

	int lock;
	int shm_client_tbl_id;
	P_CM_CLIENT_TABLE p_client_tbl;
	void *shared_client_info = (void *)0;

	lock = file_lock(CFG_FILE_LOCK);
	shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
	if (shm_client_tbl_id == -1){
		fprintf(stderr, "shmget failed\n");
		file_unlock(lock);
		return 0;
	}

	shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr, "shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
	re_num = p_client_tbl->count;

	shmdt(shared_client_info);
	file_unlock(lock);

	get_node_band(nvram_get_int("chksta_band"), band, sizeof(band));

	// read cap's RSSI
	for(i = 0; i <= re_num; ++i){ // row
		convert_nodenum_to_str(i, src_str, sizeof(src_str));

		websWrite(wp, "\t\t\t[");
		for(j = 0; j <= re_num; ++j){ // col
			if(i == j){
				websWrite(wp, "null");
				if(j < re_num)
					websWrite(wp, ", ");

				continue;
			}

			convert_nodenum_to_str(j, dst_str, sizeof(dst_str));

			snprintf(fname, sizeof(fname), "%s/sta_%s_%s_%s", LOG_DIR, dst_str, src_str, band);
			f_read_string(fname, data, sizeof(data));
			if(*data)
				websWrite(wp, "\"%s\"", data);
			else
				websWrite(wp, "\"0>0>0\"");
			if(j < re_num)
				websWrite(wp, ", ");
		}
		websWrite(wp, "]");
		if(i < re_num)
			websWrite(wp, ",");
		websWrite(wp, "\n");
	}

	return 0;
}
#endif
#endif

#ifdef RTCONFIG_CFGSYNC
void save_changed_param(json_object *cfg_root, char *param)
{
	if (nvram_match("x_Setting", "1") && pids("cfg_server")) {
		json_object *tmp = NULL;
		struct param_mapping_s *pParam = &param_mapping_list[0];

		json_object_object_get_ex(cfg_root, param, &tmp);
		if (tmp == NULL) {
			for (pParam = &param_mapping_list[0]; pParam->param != NULL; pParam++) {
				if (!strcmp(param, pParam->param)) {
					json_object_object_add(cfg_root, param,
						json_object_new_string(""));
					break;
				}
			}
		}
	}
} 

int check_cfg_changed(json_object *root)
{
	json_object *paramObj = NULL;
	struct param_mapping_s *pParam = &param_mapping_list[0];

	if (!root)
		return 0;

	for (pParam = &param_mapping_list[0]; pParam->param != NULL; pParam++) {
		json_object_object_get_ex(root, pParam->param, &paramObj);
		if (paramObj)
			return 1;
	}

	return 0;
}

static int ej_get_cfg_client_info(int eid, webs_t wp, int argc, char_t **argv){
	int shm_client_tbl_id;
	int lock;
	P_CM_CLIENT_TABLE p_client_tbl;
	void *shared_client_info = (void *) 0;
	int i = 0;
	char output_buf[128] = {0};
	char pap2g_buf[32] = {0};
	char pap5g_buf[32] = {0};
	char rssi2g_buf[8] = {0};
	char rssi5g_buf[8] = {0};

	lock = file_lock(CFG_FILE_LOCK);
	shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
	if (shm_client_tbl_id == -1){
		fprintf(stderr, "shmget failed\n");
		file_unlock(lock);
		return 0;
	}

	shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr, "shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
	for(i = 0 ; i < p_client_tbl->count; i++) {
		memset(output_buf, 0, sizeof(output_buf));
		memset(pap2g_buf, 0, sizeof(pap2g_buf));
		memset(pap5g_buf, 0, sizeof(pap5g_buf));
		memset(rssi2g_buf, 0, sizeof(rssi2g_buf));
		memset(rssi5g_buf, 0, sizeof(rssi5g_buf));

		if (p_client_tbl->rssi2g[i] != 0) {
			snprintf(pap2g_buf, sizeof(pap2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
					p_client_tbl->pap2g[i][0], p_client_tbl->pap2g[i][1],
					p_client_tbl->pap2g[i][2], p_client_tbl->pap2g[i][3],
					p_client_tbl->pap2g[i][4], p_client_tbl->pap2g[i][5]);
			snprintf(rssi2g_buf, sizeof(rssi2g_buf), "%d", p_client_tbl->rssi2g[i]);
		}

		if (p_client_tbl->rssi5g[i] != 0) {
			snprintf(pap5g_buf, sizeof(pap5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
					p_client_tbl->pap5g[i][0], p_client_tbl->pap5g[i][1],
					p_client_tbl->pap5g[i][2], p_client_tbl->pap5g[i][3],
					p_client_tbl->pap5g[i][4], p_client_tbl->pap5g[i][5]);
			snprintf(rssi5g_buf, sizeof(rssi5g_buf), "%d", p_client_tbl->rssi5g[i]);
		}
			
		sprintf(output_buf, "<%d.%d.%d.%d>%02X:%02X:%02X:%02X:%02X:%02X>%d>%02X:%02X:%02X:%02X:%02X:%02X>%02X:%02X:%02X:%02X:%02X:%02X>%s>%s>%s>%s",
		p_client_tbl->ipAddr[i][0], p_client_tbl->ipAddr[i][1],
		p_client_tbl->ipAddr[i][2], p_client_tbl->ipAddr[i][3],
		//p_client_tbl->macAddr[i][0], p_client_tbl->macAddr[i][1],
		//p_client_tbl->macAddr[i][2], p_client_tbl->macAddr[i][3],
		//p_client_tbl->macAddr[i][4], p_client_tbl->macAddr[i][5],
		p_client_tbl->realMacAddr[i][0], p_client_tbl->realMacAddr[i][1],
		p_client_tbl->realMacAddr[i][2], p_client_tbl->realMacAddr[i][3],
		p_client_tbl->realMacAddr[i][4], p_client_tbl->realMacAddr[i][5],
		((int) difftime(time(NULL), p_client_tbl->reportStartTime[i]) < OFFLINE_THRESHOLD) ? 1 : 0,
		p_client_tbl->ap2g[i][0], p_client_tbl->ap2g[i][1],
		p_client_tbl->ap2g[i][2], p_client_tbl->ap2g[i][3],
		p_client_tbl->ap2g[i][4], p_client_tbl->ap2g[i][5],
		p_client_tbl->ap5g[i][0], p_client_tbl->ap5g[i][1],
		p_client_tbl->ap5g[i][2], p_client_tbl->ap5g[i][3],
		p_client_tbl->ap5g[i][4], p_client_tbl->ap5g[i][5],
		strlen(pap2g_buf) ? pap2g_buf : "",
		strlen(rssi2g_buf) ? rssi2g_buf : "",
		strlen(pap5g_buf) ? pap5g_buf : "",
		strlen(rssi5g_buf) ? rssi5g_buf : "");

		websWrite(wp, output_buf);
	}
	shmdt(shared_client_info);
	file_unlock(lock);

	return 0;
}

void escape_json_char(char *orig, char *conv, int percent_only)
{
	int i = 0, j = 0;

	while(orig[i] != '\0')
	{
		if (percent_only) {
			switch(orig[i])
			{
				case '%':
					conv[j]='%';
					++j;
					conv[j]='%';
					break;
				default:
					conv[j] = orig[i];
					break;
			}
		}
		else
		{
			switch(orig[i])
			{
				case '"':
					conv[j] = '\\';
					++j;
					conv[j] = '\"';
					break;
				case '\\':
					conv[j]='\\';
					++j;
					conv[j]='\\';
					break;
				case '%':
					conv[j]='%';
					++j;
					conv[j]='%';
					break;
				default:
					conv[j] = orig[i];
					break;
			}
		}
		++i;
		++j;
	}

	conv[j] = '\0';
}

static int
ej_get_cfg_clientlist(int eid, webs_t wp, int argc, char **argv){

	int shm_client_tbl_id;
	int lock;
	P_CM_CLIENT_TABLE p_client_tbl;
	void *shared_client_info=(void *) 0;
	char output_buf[8192] = {0};
	int i = 0;
	int j = 0;
	char ip_buf[16] = {0};
	char alias_buf[33] = {0};
	char rmac_buf[32] = {0};
	char ap2g_buf[32] = {0};
	char ap5g_buf[32] = {0};
	char ap5g1_buf[32] = {0};
	char apdwb_buf[32] = {0};
	char pap2g_buf[32] = {0};
	char pap5g_buf[32] = {0};
	char rssi2g_buf[8] = {0};
	char rssi5g_buf[8] = {0};
	char model_name_buf[33] = {0};
	char fwver_buf[33] = {0};
	char newfwver_buf[33] = {0};
	char re_mac_file_name[32] = {0};
	char alias_conv_buf[65];
	int first_info = 1;
	json_object *allBrMacListObj = NULL;
	json_object *macEntryObj = NULL;
	json_object *reMacFileObj = NULL, *reMac_misc_obj = NULL, *reMac_misc_cfg_alias = NULL;
	json_object *capabilityObj = NULL;
	int online = 0;
	int level = 0;
	int rePath = 0;
	char config_buf[4096], config_conv_buf[5120];
	char macList[1024] = {0};
	char *p = NULL;
	struct json_object *entry;
	char sta2g_buf[32] = {0};
	char sta5g_buf[32] = {0};
	char capability_file_name[64] = {0};
	char capability_buf[512] = {0};

	lock = file_lock(CFG_FILE_LOCK);
	shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
	if (shm_client_tbl_id == -1){
		fprintf(stderr, "shmget failed\n");
		file_unlock(lock);
		return 0;
	}

	shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr, "shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	allBrMacListObj = json_object_from_file(MAC_LIST_JSON_FILE);

	websWrite(wp, "[");
	p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
	for(i = 0; i < p_client_tbl->count; i++) {
		p = NULL;
		memset(macList, 0, sizeof(macList));
		memset(output_buf, 0, sizeof(output_buf));
		memset(alias_buf, 0, sizeof(alias_buf));
		memset(ip_buf, 0, sizeof(ip_buf));
		memset(rmac_buf, 0, sizeof(rmac_buf));
		memset(ap2g_buf, 0, sizeof(ap2g_buf));
		memset(ap5g_buf, 0, sizeof(ap5g_buf));
		memset(ap5g1_buf, 0, sizeof(ap5g1_buf));
		memset(apdwb_buf, 0, sizeof(apdwb_buf));
		memset(pap2g_buf, 0, sizeof(pap2g_buf));
		memset(pap5g_buf, 0, sizeof(pap5g_buf));
		memset(rssi2g_buf, 0, sizeof(rssi2g_buf));
		memset(rssi5g_buf, 0, sizeof(rssi5g_buf));
		memset(re_mac_file_name, 0, sizeof(re_mac_file_name));
		memset(sta2g_buf, 0, sizeof(sta2g_buf));
		memset(sta5g_buf, 0, sizeof(sta5g_buf));
		memset(alias_conv_buf, 0, sizeof(alias_conv_buf));

		if (i == 0) /* master */
			strlcpy(alias_buf, nvram_safe_get("cfg_alias"), sizeof(alias_buf));
		else
			strlcpy(alias_buf, p_client_tbl->alias[i], sizeof(alias_buf));
		escape_json_char(alias_buf, alias_conv_buf, 0);

		snprintf(ip_buf, sizeof(ip_buf), "%d.%d.%d.%d", p_client_tbl->ipAddr[i][0], p_client_tbl->ipAddr[i][1],
			p_client_tbl->ipAddr[i][2], p_client_tbl->ipAddr[i][3]);

		snprintf(rmac_buf, sizeof(rmac_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->realMacAddr[i][0], p_client_tbl->realMacAddr[i][1],
			p_client_tbl->realMacAddr[i][2], p_client_tbl->realMacAddr[i][3],
			p_client_tbl->realMacAddr[i][4], p_client_tbl->realMacAddr[i][5]);

		memset(config_buf, 0, sizeof(config_buf));	/* reset buffer private config */
		memset(config_conv_buf, 0, sizeof(config_conv_buf));
		snprintf(re_mac_file_name, sizeof(re_mac_file_name), "/tmp/%s.json", rmac_buf);
		reMacFileObj = json_object_from_file(re_mac_file_name);
		if (reMacFileObj) {
			json_object_object_foreach(reMacFileObj, key, val) {
				reMac_misc_obj = val;
				json_object_object_del(reMac_misc_obj, "action_script"); /* filter unnecessary info */
				json_object_object_get_ex(reMac_misc_obj, "cfg_alias", &reMac_misc_cfg_alias);
				if (reMac_misc_cfg_alias) {
					if (strcmp(json_object_get_string(reMac_misc_cfg_alias), "")) {
						memset(alias_buf, 0, sizeof(alias_buf));
						strlcpy(alias_buf, json_object_get_string(reMac_misc_cfg_alias), sizeof(alias_buf));
						memset(alias_conv_buf, 0, sizeof(alias_conv_buf));
						escape_json_char(alias_buf, alias_conv_buf, 0);
					}
				}
			}

			/* save private config to buffer for using later */
			strlcpy(config_buf, json_object_to_json_string_ext(reMacFileObj, 0), sizeof(config_buf));
			escape_json_char(config_buf, config_conv_buf, 1);
			json_object_put(reMacFileObj);
		}

		/* get capability */
		memset(capability_buf, 0, sizeof(capability_buf));
		snprintf(capability_file_name, sizeof(capability_file_name), "/tmp/%s.cap", rmac_buf);
		capabilityObj = json_object_from_file(capability_file_name);
		if (capabilityObj) {
			snprintf(capability_buf, sizeof(capability_buf), "%s", json_object_to_json_string_ext(capabilityObj, 0));
			json_object_put(capabilityObj);
		}

		if (p_client_tbl->rssi2g[i] != 0) {
			snprintf(pap2g_buf, sizeof(pap2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->pap2g[i][0], p_client_tbl->pap2g[i][1],
				p_client_tbl->pap2g[i][2], p_client_tbl->pap2g[i][3],
				p_client_tbl->pap2g[i][4], p_client_tbl->pap2g[i][5]);
			snprintf(rssi2g_buf, sizeof(rssi2g_buf), "%d", p_client_tbl->rssi2g[i]);
		}

		if (p_client_tbl->rssi5g[i] != 0) {
			snprintf(pap5g_buf, sizeof(pap5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->pap5g[i][0], p_client_tbl->pap5g[i][1],
				p_client_tbl->pap5g[i][2], p_client_tbl->pap5g[i][3],
				p_client_tbl->pap5g[i][4], p_client_tbl->pap5g[i][5]);
			snprintf(rssi5g_buf, sizeof(rssi5g_buf), "%d", p_client_tbl->rssi5g[i]);
		}

		snprintf(ap2g_buf, sizeof(ap2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->ap2g[i][0], p_client_tbl->ap2g[i][1],
			p_client_tbl->ap2g[i][2], p_client_tbl->ap2g[i][3],
			p_client_tbl->ap2g[i][4], p_client_tbl->ap2g[i][5]);

		snprintf(ap5g_buf, sizeof(ap5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->ap5g[i][0], p_client_tbl->ap5g[i][1],
			p_client_tbl->ap5g[i][2], p_client_tbl->ap5g[i][3],
			p_client_tbl->ap5g[i][4], p_client_tbl->ap5g[i][5]);

		snprintf(ap5g1_buf, sizeof(ap5g1_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->ap5g1[i][0], p_client_tbl->ap5g1[i][1],
			p_client_tbl->ap5g1[i][2], p_client_tbl->ap5g1[i][3],
			p_client_tbl->ap5g1[i][4], p_client_tbl->ap5g1[i][5]);

		snprintf(apdwb_buf, sizeof(apdwb_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->apDwb[i][0], p_client_tbl->apDwb[i][1],
			p_client_tbl->apDwb[i][2], p_client_tbl->apDwb[i][3],
			p_client_tbl->apDwb[i][4], p_client_tbl->apDwb[i][5]);

		snprintf(sta2g_buf, sizeof(sta2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->sta2g[i][0], p_client_tbl->sta2g[i][1],
			p_client_tbl->sta2g[i][2], p_client_tbl->sta2g[i][3],
			p_client_tbl->sta2g[i][4], p_client_tbl->sta2g[i][5]);

		snprintf(sta5g_buf, sizeof(sta5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->sta5g[i][0], p_client_tbl->sta5g[i][1],
			p_client_tbl->sta5g[i][2], p_client_tbl->sta5g[i][3],
			p_client_tbl->sta5g[i][4], p_client_tbl->sta5g[i][5]);

		/* modle name */
		strlcpy(model_name_buf, p_client_tbl->modelName[i], sizeof(model_name_buf));

		/* firmware version */
		strlcpy(fwver_buf, p_client_tbl->fwVer[i], sizeof(fwver_buf));

		/* new firmware version */
		strlcpy(newfwver_buf, p_client_tbl->newFwVer[i], sizeof(newfwver_buf));

		if (allBrMacListObj) {
			json_object_object_get_ex(allBrMacListObj, rmac_buf, &macEntryObj);
			if (macEntryObj) {
				int macEntryLen = json_object_array_length(macEntryObj);

				if (macEntryLen) {
					memset(macList, 0, sizeof(macList));
					p = macList;
					p += sprintf(p, "[");
					for (j = 0; j < macEntryLen; j++) {
						entry = json_object_array_get_idx(macEntryObj, j);
						if(strlen(macList)+3+strlen(json_object_get_string(entry)) > sizeof(macList) -2)
						{
							_dprintf("too many macList entries. \n");
							break;
						}
						if (j) p += sprintf(p, ",");
						p += sprintf(p, "\"%s\"", json_object_get_string(entry));	
					}
					p += sprintf(p, "]");
				}
			}
		}

		if(first_info == 1){
			first_info = 0;
		}else{
			websWrite(wp, ",\n");
		}

		if (i == 0)	/* DUT info */
			online = 1;
		else
			online = ((int) difftime(time(NULL), p_client_tbl->reportStartTime[i]) < OFFLINE_THRESHOLD) ? 1 : 0;

		/* level */
		level = p_client_tbl->level[i];

		/* re path */
		rePath = p_client_tbl->activePath[i];

		snprintf(output_buf, sizeof(output_buf), "{\"alias\":\"%s\",\"model_name\":\"%s\",\"fwver\":\"%s\",\"newfwver\":\"%s\",\"ip\":\"%s\",\"mac\":\"%s\",\"online\":\"%d\",\"ap2g\":\"%s\",\"ap5g\":\"%s\",\"ap5g1\":\"%s\",\"apdwb\":\"%s\",\"wired_mac\":%s,\"pap2g\":\"%s\",\"rssi2g\":\"%s\",\"pap5g\":\"%s\",\"rssi5g\":\"%s\",\"level\":\"%d\",\"re_path\":\"%d\",\"config\":%s,\"sta2g\":\"%s\",\"sta5g\":\"%s\",\"capability\":%s}",
		strlen(alias_conv_buf) ? alias_conv_buf : rmac_buf,
		model_name_buf,
		fwver_buf,
		newfwver_buf,
		ip_buf,
		rmac_buf,
		online,
		strcmp(ap2g_buf, "00:00:00:00:00:00") ? ap2g_buf : "",
		strcmp(ap5g_buf, "00:00:00:00:00:00") ? ap5g_buf : "",
		strcmp(ap5g1_buf, "00:00:00:00:00:00") ? ap5g1_buf : "",
		strcmp(apdwb_buf, "00:00:00:00:00:00") ? apdwb_buf : "",
		strlen(macList) ? macList : "[]",
		strlen(pap2g_buf) ? pap2g_buf : "",
		strlen(rssi2g_buf) ? rssi2g_buf : "",
		strlen(pap5g_buf) ? pap5g_buf : "",
		strlen(rssi5g_buf) ? rssi5g_buf : "",
		level,
		rePath,
		strlen(config_conv_buf) ? config_conv_buf : "{}",
		strcmp(sta2g_buf, "00:00:00:00:00:00") ? sta2g_buf : "",
		strcmp(sta5g_buf, "00:00:00:00:00:00") ? sta5g_buf : "",
		strlen(capability_buf) ? capability_buf : "{}");

		websWrite(wp, output_buf);
	}
	websWrite(wp, "]");
	shmdt(shared_client_info);

	if (allBrMacListObj)
		json_object_put(allBrMacListObj);

	file_unlock(lock);
 
	return 0;
}

static int
ej_get_wclientlist(int eid, webs_t wp, int argc, char **argv){
	int shm_client_tbl_id;
	int lock;
	void *shared_client_info = (void *) 0;
	json_object *wClietListObj = NULL;
	json_object *brMacObj = NULL;
	json_object *bandObj = NULL;
	json_object *staObj = NULL;

	lock = file_lock(ALLWEVENT_FILE_LOCK);
	wClietListObj = json_object_from_file(ALLWCLIENT_LIST_JSON_PATH);
	file_unlock(lock);

	if (wClietListObj) {
		P_CM_CLIENT_TABLE p_client_tbl;
		int i = 0;
		char sta2g_buf[32] = {0};
		char sta5g_buf[32] = {0};
		int brmac_first = 0;
		int band_first = 0;
		int sta_first = 0;

		lock = file_lock(CFG_FILE_LOCK);
		shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
		if (shm_client_tbl_id == -1){
			fprintf(stderr, "shmget failed\n");
			file_unlock(lock);
			websWrite(wp, "{}");
			json_object_put(wClietListObj);
			return 0;
		}

		shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
		if (shared_client_info == (void *)-1){
			fprintf(stderr, "shmat failed\n");
			file_unlock(lock);
			websWrite(wp, "{}");
			json_object_put(wClietListObj);
			return 0;
		}

		p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;
		for(i = 0; i < p_client_tbl->count; i++) {
			memset(sta2g_buf, 0, sizeof(sta2g_buf));
			memset(sta5g_buf, 0, sizeof(sta5g_buf));

			snprintf(sta2g_buf, sizeof(sta2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->sta2g[i][0], p_client_tbl->sta2g[i][1],
				p_client_tbl->sta2g[i][2], p_client_tbl->sta2g[i][3],
				p_client_tbl->sta2g[i][4], p_client_tbl->sta2g[i][5]);

			snprintf(sta5g_buf, sizeof(sta5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_tbl->sta5g[i][0], p_client_tbl->sta5g[i][1],
				p_client_tbl->sta5g[i][2], p_client_tbl->sta5g[i][3],
				p_client_tbl->sta5g[i][4], p_client_tbl->sta5g[i][5]);

			/* filter ASUS device first */
			json_object_object_foreach(wClietListObj, key, val) {
				brMacObj = val;
				json_object_object_foreach(brMacObj, key, val) {
					bandObj = val;
					/* filter sta for 2G */
					if (strlen(sta2g_buf)) {
						json_object_object_get_ex(bandObj, sta2g_buf, &staObj);
						if (staObj) json_object_object_del(bandObj, sta2g_buf);
					}

					/* filter sta for 5G */
					if (strlen(sta5g_buf)) {
						json_object_object_get_ex(bandObj, sta5g_buf, &staObj);
						if (staObj) json_object_object_del(bandObj, sta5g_buf);
					}
				}
			}
		}

		/* assemble output */
		websWrite(wp, "{");
		json_object_object_foreach(wClietListObj, key, val) {
			if (!brmac_first)
				brmac_first = 1;
			else
				websWrite(wp, ",");

			band_first = 0;
			brMacObj = val;
			websWrite(wp, "\"%s\":{", key);
			json_object_object_foreach(brMacObj, key, val) {
				if (!band_first)
					band_first = 1;
				else
					websWrite(wp, ",");

				sta_first = 0;
				bandObj = val;
				websWrite(wp, "\"%s\":[", key);
				json_object_object_foreach(bandObj, key, val) {
					if (!sta_first)
						sta_first = 1;
					else
						websWrite(wp, ",");

					websWrite(wp, "\"%s\"", key);
				}
				websWrite(wp, "]");
			}
			websWrite(wp, "}");
		}
		websWrite(wp, "}");
		shmdt(shared_client_info);
		file_unlock(lock);
	}
	else
		websWrite(wp, "{}");

	json_object_put(wClietListObj);

	return 0;
}

static int
ej_get_allclientlist(int eid, webs_t wp, int argc, char **argv){
	int lock;
	json_object *clietListObj = NULL;
	json_object *brMacObj = NULL;
	json_object *clientObj = NULL;
	json_object *infoObj = NULL;
	int brmac_first = 0;
	int type_first = 0;
	int client_first = 0;
	int info_first = 0;

	lock = file_lock(CLIENTLIST_FILE_LOCK);

	clietListObj = json_object_from_file(CLIENT_LIST_JSON_PATH);
	if (clietListObj) {
		/* assemble output */
		websWrite(wp, "{");
		json_object_object_foreach(clietListObj, key, val) {
			if (!brmac_first)
				brmac_first = 1;
			else
				websWrite(wp, ",");

			type_first = 0;
			brMacObj = val;
			websWrite(wp, "\"%s\":{", key);
			json_object_object_foreach(brMacObj, key, val) {
				if (!type_first)
					type_first = 1;
				else
					websWrite(wp, ",");

				client_first = 0;
				clientObj = val;
				websWrite(wp, "\"%s\":{", key);
				json_object_object_foreach(clientObj, key, val) {
					if (!client_first)
						client_first = 1;
					else
						websWrite(wp, ",");

					info_first = 0;
					infoObj = val;
					websWrite(wp, "\"%s\":{", key);
					json_object_object_foreach(infoObj, key, val) {
						if (!info_first)
							info_first = 1;
						else
							websWrite(wp, ",");
						websWrite(wp, "\"%s\":\"%s\"", key, json_object_get_string(val));
					}
					websWrite(wp, "}");
				}
				websWrite(wp, "}");
			}
			websWrite(wp, "}");
		}
		websWrite(wp, "}");
	}
	else
		websWrite(wp, "{}");

	json_object_put(clietListObj);
	file_unlock(lock);

	return 0;
}

static int is_wireless_client(char *mac)
{
	json_object *wClietListObj = NULL;
	json_object *brMacObj = NULL;
	json_object *bandObj = NULL;
	json_object *staObj = NULL;
	int ret = 0;
	int lock;

	lock = file_lock(ALLWEVENT_FILE_LOCK);
	wClietListObj = json_object_from_file(ALLWCLIENT_LIST_JSON_PATH);
	file_unlock(lock);

	if (wClietListObj) {
		json_object_object_foreach(wClietListObj, key, val) {
			brMacObj = val;
			json_object_object_foreach(brMacObj, key, val) {
				bandObj = val;
				json_object_object_get_ex(bandObj, mac, &staObj);
				if (staObj) {
					ret = 1;
					break;
				}
			}

			if (ret) break;
		}
	}

	json_object_put(wClietListObj);

	return ret;
}

static int is_cfg_client_by_unique_mac(P_CM_CLIENT_TABLE p_client_tbl, char *mac)
{
	int i = 0;
	int ret = 0;
	char rmac_buf[18] = {0};

	for(i = 0; i < p_client_tbl->count; i++) {
		memset(rmac_buf, 0, sizeof(rmac_buf));
		snprintf(rmac_buf, sizeof(rmac_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->realMacAddr[i][0], p_client_tbl->realMacAddr[i][1],
			p_client_tbl->realMacAddr[i][2], p_client_tbl->realMacAddr[i][3],
			p_client_tbl->realMacAddr[i][4], p_client_tbl->realMacAddr[i][5]);

		if (strcasecmp(rmac_buf, mac) == 0) {
			ret = 1;
			break;
		}
	}

	return ret;
}

static int is_cfg_client_by_sta_mac(P_CM_CLIENT_TABLE p_client_tbl, char *staMac)
{
	int i = 0;
	int ret = 0;
	char sta2g_buf[18] = {0};
	char sta5g_buf[18] = {0};

	for(i = 0; i < p_client_tbl->count; i++) {
		memset(sta2g_buf, 0, sizeof(sta2g_buf));
		memset(sta5g_buf, 0, sizeof(sta5g_buf));

		snprintf(sta2g_buf, sizeof(sta2g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->sta2g[i][0], p_client_tbl->sta2g[i][1],
			p_client_tbl->sta2g[i][2], p_client_tbl->sta2g[i][3],
			p_client_tbl->sta2g[i][4], p_client_tbl->sta2g[i][5]);

		snprintf(sta5g_buf, sizeof(sta5g_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
			p_client_tbl->sta5g[i][0], p_client_tbl->sta5g[i][1],
			p_client_tbl->sta5g[i][2], p_client_tbl->sta5g[i][3],
			p_client_tbl->sta5g[i][4], p_client_tbl->sta5g[i][5]);

		if (strcmp(sta2g_buf, staMac) == 0 || strcmp(sta5g_buf, staMac) == 0) {
			ret = 1;
			break;
		}
	}

	return ret;
}

static void gen_wired_client_info(P_CM_CLIENT_TABLE p_client_tbl, json_object *wiredClietListObj, json_object *wiredInfoObj)
{
	int i = 0;
	int wiredClientListLen = 0;
	json_object *wiredClientEntry = NULL;
	int count = 0;

	if (!wiredClietListObj && !wiredInfoObj) {
		fprintf(stderr, "wiredClietListObj/wiredInfoObj is NULL\n");
		return;
	}

	json_object_object_foreach(wiredClietListObj, key, val) {
		count = 0;
		wiredClientListLen = json_object_array_length(val);
		for (i = 0; i < wiredClientListLen; i++) {
			wiredClientEntry = json_object_array_get_idx(val, i);

			if (is_cfg_client_by_unique_mac(p_client_tbl, (char *)json_object_get_string(wiredClientEntry)))
				count++;
		}

		json_object_object_add(wiredInfoObj, key, json_object_new_int(count));
	}
}

static int check_wired_client_connected_node(char *nodeMac, char *wiredMac, json_object *wiredClietListObj, json_object *wiredInfoObj)
{
	int i = 0;
	int wiredClientListLen = 0;
	json_object *wiredInfoEntry = NULL;
	json_object *wiredClientEntry = NULL;
	int count = 100;
	char selectedNode[18] = {0};

	if (!wiredClietListObj && !wiredInfoObj) {
		fprintf(stderr, "wiredClietListObj/wiredInfoObj is NULL\n");
		return 0;
	}

	json_object_object_foreach(wiredClietListObj, key, val) {
		json_object_object_get_ex(wiredInfoObj, key, &wiredInfoEntry);

		if (!wiredInfoEntry) {
			fprintf(stderr, "wiredInfoEntry is NULL\n");
			continue;
		}

		wiredClientListLen = json_object_array_length(val);
		for (i = 0; i < wiredClientListLen; i++) {
			wiredClientEntry = json_object_array_get_idx(val, i);

			if (strcasecmp(json_object_get_string(wiredClientEntry), wiredMac) == 0) {
				if (json_object_get_int(wiredInfoEntry) < count) {
					count = json_object_get_int(wiredInfoEntry);
					memset(selectedNode, 0, sizeof(selectedNode));
					snprintf(selectedNode, sizeof(selectedNode), "%s", key);
				}
				break;
			}
		}
	}

	if (strcmp(nodeMac, selectedNode) == 0)
		return 1;
	else
		return 0;
}

static int
ej_get_wiredclientlist(int eid, webs_t wp, int argc, char **argv){
	int shm_client_tbl_id;
	int lock;
	void *shared_client_info = (void *) 0;
	json_object *wiredClietListObj = NULL;

	lock = file_lock(WIREDCLIENTLIST_FILE_LOCK);
	wiredClietListObj = json_object_from_file(WIRED_CLIENT_LIST_JSON_PATH);
	file_unlock(lock);

	if (wiredClietListObj) {
		P_CM_CLIENT_TABLE p_client_tbl;
		int i = 0;
		int brmac_first = 0;
		int wired_client_first = 0;
		int wiredClientListLen = 0;
		json_object *wiredClientEntry = NULL;
		json_object *wiredInfoObj = NULL;

		lock = file_lock(CFG_FILE_LOCK);
		shm_client_tbl_id = shmget((key_t)KEY_SHM_CFG, sizeof(CM_CLIENT_TABLE), 0666|IPC_CREAT);
		if (shm_client_tbl_id == -1){
			fprintf(stderr, "shmget failed\n");
			file_unlock(lock);
			websWrite(wp, "{}");
			json_object_put(wiredClietListObj);
			return 0;
		}

		shared_client_info = shmat(shm_client_tbl_id,(void *) 0,0);
		if (shared_client_info == (void *)-1){
			fprintf(stderr, "shmat failed\n");
			file_unlock(lock);
			websWrite(wp, "{}");
			json_object_put(wiredClietListObj);
			return 0;
		}

		p_client_tbl = (P_CM_CLIENT_TABLE)shared_client_info;

		wiredInfoObj = json_object_new_object();

		if (!wiredInfoObj) {
			fprintf(stderr, "wiredInfoObj is NULL\n");
			file_unlock(lock);
			websWrite(wp, "{}");
			json_object_put(wiredClietListObj);
			return 0;
		}

		/* gen the info of wired client for temp */
		gen_wired_client_info(p_client_tbl, wiredClietListObj, wiredInfoObj);

		/* assemble output */
		websWrite(wp, "{");

		json_object_object_foreach(wiredClietListObj, key, val) {
			if (!brmac_first)
				brmac_first = 1;
			else
				websWrite(wp, ",");

			wired_client_first = 0;
			websWrite(wp, "\"%s\":[", key);

			/* process for filtering device */
			wiredClientListLen = json_object_array_length(val);
			for (i = 0; i < wiredClientListLen; i++) {
				wiredClientEntry = json_object_array_get_idx(val, i);

				/* filter wireless client */
				if (is_wireless_client((char *)json_object_get_string(wiredClientEntry)))
					continue;

				/* filter cfg client by unique mac */
				if (is_cfg_client_by_unique_mac(p_client_tbl, (char *)json_object_get_string(wiredClientEntry)))
					continue;

				/* filter cfg client by sta mac */
				if (is_cfg_client_by_sta_mac(p_client_tbl, (char *)json_object_get_string(wiredClientEntry)))
					continue;

				/* filter not connected to correct node */
				if (!check_wired_client_connected_node(key, (char *)json_object_get_string(wiredClientEntry), wiredClietListObj, wiredInfoObj))
					continue;

				if (!wired_client_first)
					wired_client_first = 1;
				else
					websWrite(wp, ",");
				websWrite(wp, "\"%s\"", json_object_get_string(wiredClientEntry));
			}

			websWrite(wp, "]");
		}

		websWrite(wp, "}");
		shmdt(shared_client_info);
		file_unlock(lock);
		json_object_put(wiredInfoObj);
	}
	else
		websWrite(wp, "{}");

	json_object_put(wiredClietListObj);

	return 0;
}

static char *select_best_onboarding_re(char *newReMac, int *rssi, int *source)
{
	json_object *obListObj = NULL;
	json_object *reObj = NULL;
	json_object *newReObj = NULL;
	json_object *rssiObj = NULL;
	json_object *sourceObj = NULL;
	static char reMac[18];

	obListObj = json_object_from_file(ONBOARDING_LIST_JSON_PATH);
	memset(reMac, 0, sizeof(reMac));

	if (obListObj) {
		json_object_object_foreach(obListObj, key, val) {
			reObj = val;

			json_object_object_get_ex(reObj, newReMac, &newReObj);
			if (newReObj) {
				json_object_object_get_ex(newReObj, "rssi", &rssiObj);
				json_object_object_get_ex(newReObj, "source", &sourceObj);
				if (*rssi <= json_object_get_int(rssiObj)) {
					snprintf(reMac, sizeof(reMac), "%s", key);
					*rssi = json_object_get_int(rssiObj);
				}

				if (sourceObj)
					*source |= json_object_get_int(sourceObj);
			}
		}
	}

	json_object_put(obListObj);

	return reMac;
}

static void update_onboarding_best_re(json_object *rootObj, char *reMac, char *newReMac, char *modelName, int rssi, int source)
{
	json_object *reMacObj = NULL;
	json_object *newReObj = NULL;
	int found = 0;

	if (rootObj != NULL) {
		json_object_object_foreach(rootObj, key, val) {
			if (strcmp(reMac, key))
				continue;
			reMacObj = val;

			json_object_object_foreach(reMacObj, key, val) {
				if (!strcmp(newReMac, key)) {
					found = 1;
					break;
				}
			}
		}

		if (!found)
		{
			//{"D8:50:E6:5A:3F:C0":{"78:24:AF:D3:3F:C0":{"model_name":"", "rssi": -20}}}
			newReObj = json_object_new_object();
			if (newReObj) {
				json_object_object_add(newReObj, "model_name",
					json_object_new_string(modelName));
				json_object_object_add(newReObj, "rssi",
					json_object_new_int(rssi));
				json_object_object_add(newReObj, "source",
					json_object_new_int((source & FROM_ETHERNET) ? FROM_ETHERNET: FROM_WIRELESS));
			}
			else
				return;

			if (reMacObj)
				json_object_object_add(reMacObj, newReMac, newReObj);
			else
			{
				reMacObj = json_object_new_object();
				if (reMacObj) {
					json_object_object_add(reMacObj, newReMac, newReObj);
					json_object_object_add(rootObj, reMac, reMacObj);
				}
				else
					json_object_put(newReObj);
			}	
		}
	}
}

static int
ej_get_onboardinglist(int eid, webs_t wp, int argc, char **argv){
	int lock;
	json_object *obListObj = NULL;
	json_object *reObj = NULL;
	json_object *newReObj = NULL;
	json_object *rssiObj = NULL;
	json_object *bestReObj = NULL;
	json_object *modelNameObj = NULL;
	int reFirst = 0;
	int newReFirst = 0;
	int contentFirst = 0;
	char newReMac[18] = {0};
	char reMac[18] = {0};
	int rssi = 0;
	char modelName[32] = {0};
	int source = 0;

	lock = file_lock(ONBOARDING_FILE_LOCK);

	obListObj = json_object_from_file(ONBOARDING_LIST_JSON_PATH);
	if (obListObj) {
		/* select best re for onboarding */
		json_object_object_foreach(obListObj, key, val) {
			reObj = val;

			json_object_object_foreach(reObj, key, val) {
				memset(newReMac, 0, sizeof(newReMac));
				snprintf(newReMac, sizeof(newReMac), "%s", key);
				newReObj = val;

				json_object_object_get_ex(newReObj, "rssi", &rssiObj);
				json_object_object_get_ex(newReObj, "model_name", &modelNameObj);
				if (rssiObj && modelNameObj) {
					rssi = json_object_get_int(rssiObj);
					source = 0;
					memset(modelName, 0, sizeof(modelName));
					snprintf(modelName, sizeof(modelName), "%s",
						json_object_get_string(modelNameObj));
					memset(reMac, 0, sizeof(reMac));
					snprintf(reMac, sizeof(reMac), "%s",
						select_best_onboarding_re(newReMac, &rssi, &source));

					if (strlen(reMac)) {
						if (!bestReObj)
							bestReObj = json_object_new_object();

						if (bestReObj)
							update_onboarding_best_re(bestReObj, reMac, newReMac, modelName, rssi, source);
					}
				}
			}
		}

		/* output best onboarding list */
		if (bestReObj) {
			websWrite(wp, "{");
			json_object_object_foreach(bestReObj, key, val) {
				if (!reFirst)
					reFirst = 1;
				else
					websWrite(wp, ",");
	
				reObj = val;
				newReFirst = 0;
				websWrite(wp, "\"%s\":{", key);
				json_object_object_foreach(reObj, key, val) {
					if (!newReFirst)
						newReFirst = 1;
					else
						websWrite(wp, ",");
	
					newReObj = val;
					contentFirst = 0;
					websWrite(wp, "\"%s\":{", key);
					json_object_object_foreach(newReObj, key, val) {
						if (!strcmp(key, "ts"))
							continue;
	
						if (!contentFirst)
							contentFirst = 1;
						else
							websWrite(wp, ",");
	
						if (json_object_get_type(val) == json_type_int)
							websWrite(wp, "\"%s\":%d", key, json_object_get_int(val));
						else if (json_object_get_type(val) == json_type_string)
							websWrite(wp, "\"%s\":\"%s\"", key, json_object_get_string(val));
					}
					websWrite(wp, "}");
				}
				websWrite(wp, "}");
			}
			websWrite(wp, "}");
		}
		else
			websWrite(wp, "{}");
	}
	else
		websWrite(wp, "{}");

	json_object_put(obListObj);
	json_object_put(bestReObj);

	file_unlock(lock);

	return 0;
}

static int
ej_get_onboardingstatus(int eid, webs_t wp, int argc, char **argv){
	websWrite(wp, "{");
	websWrite(wp, "\"cfg_obstatus\":\"%s\",", nvram_safe_get("cfg_obstatus"));
	websWrite(wp, "\"cfg_obresult\":\"%s\",", nvram_safe_get("cfg_obresult"));
	websWrite(wp, "\"cfg_newre\":\"%s\",", nvram_safe_get("cfg_newre"));
	websWrite(wp, "\"cfg_wifi_quality\":\"%s\",", nvram_safe_get("cfg_wifi_quality"));
	websWrite(wp, "\"cfg_obstart\":\"%s\",", nvram_safe_get("cfg_obstart"));
	websWrite(wp, "\"cfg_obcurrent\":\"%ld\",", time((time_t*)NULL));
	websWrite(wp, "\"cfg_obtimeout\":\"%s\",", nvram_safe_get("cfg_obtimeout"));
	websWrite(wp, "\"cfg_obmodel\":\"%s\",", nvram_safe_get("cfg_obmodel"));
	websWrite(wp, "\"cfg_obrssi\":\"%s\",", nvram_safe_get("cfg_obrssi"));
	websWrite(wp, "\"cfg_obcount\":\"%s\",", nvram_safe_get("cfg_obcount"));
	websWrite(wp, "\"cfg_ready\":\"%d\"", check_if_file_exist(CFG_SERVER_PID) ? 1 : 0);
	websWrite(wp, "}");
	return 0;
}
#endif

#ifdef RTCONFIG_CONCURRENTREPEATER
static int
ej_get_default_ssid(int eid, webs_t wp, int argc, char_t **argv)
{
	int band_num = MAX_NR_WL_IF, unit = 0;
	char word[256], *next;

	websWrite(wp, "[");
#if defined(RTCONFIG_NEWSSID_REV2) || defined(RTCONFIG_NEWSSID_REV4)
	while (unit < band_num){
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit)
		if(unit != 0) websWrite(wp, ", ");
		websWrite(wp, "'%s'", (char *) get_default_ssid(unit, 0));
		unit++;
	}
#endif
	websWrite(wp, "]");

	return 0;
}
#endif

#ifdef RTCONFIG_NOTIFICATION_CENTER
static int
get_nt_db_type(webs_t wp, int mode)
{
	struct list *event_list = NULL;
	int first_row = 0;
	int type = 0;
	int ret = -1;
	char msg[512];
	char *num = NULL;
	char *page = NULL;
	char *count = NULL;

	// if mode = 0, gui mode
	// if mode = 1. app mode
	if (mode == 0) {
		num = websGetVar(wp, "num", "");
		if (!strcmp(num, "")) num = NULL;
	}
	else if (mode == 1) {
		page = websGetVar(wp, "page", "");
		if (!strcmp(page, "")) page = NULL;

		count = websGetVar(wp, "count", "");
		if (!strcmp(count, "")) count = NULL;
	}
	else {
		return 0;
	}

	/* initial */
	NOTIFY_DATABASE_T *input = initial_db_input();

	/* initial linked list */
	event_list = list_new();

	/* database API */
	if (mode == 0) {
		ret = NT_DBAction(event_list, "read", input, num);
	}
	else if (mode == 1) {
		ret = NT_DBActionAPP(event_list, "read", input, page, count);
	}

	/* free input*/
	db_input_free(input);

	if(ret == 0){
		/* print all linked list */
		NOTIFY_DATABASE_T *listevent;
		struct listnode *ln;

		websWrite(wp, "[");
		LIST_LOOP(event_list, listevent, ln)
		{
			if(first_row == 0)
				first_row = 1;
			else
				websWrite(wp,",");

			if(listevent->event != 0)
				type = ((listevent->event) >> TYPE_SHIFT);

			if(listevent->msg != NULL && strstr(listevent->msg,"{") != (char*) 0)
				sprintf(msg, "%s", listevent->msg);
			else
				sprintf(msg, "\"%s\"", listevent->msg);

			websWrite(wp, "{\"tstamp\":\"%ld\", \"event_id\":\"%8x\", \"group_type\":\"%x\", \"msg\":%s, \"eName\":\"%s\", \"status\":\"%d\", \"event_type\":\"%d\"}\n", listevent->tstamp, listevent->event, type, msg, eInfo_get_eName(listevent->event), listevent->status, eInfo_get_eType(listevent->event));
		}
		websWrite(wp, "]");
	}
	else{
		websWrite(wp, "[]");
	}

	/* free memory */
	NT_DBFree(event_list);
	return 0;
}

static int
ej_get_nt_db_app(int eid, webs_t wp, int argc, char **argv)
{
	get_nt_db_type(wp, 1); // app mode
	return 0;
}

static int
ej_get_nt_db(int eid, webs_t wp, int argc, char **argv)
{
	get_nt_db_type(wp, 0); // gui mode
	return 0;
}
#endif

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
static int
ej_pms_account_info(int eid, webs_t wp, int argc, char **argv)
{
	struct json_object *item = NULL;
	json_object *part_jarray = NULL;
	json_object *jarray = json_object_new_array();

	int acc_num, group_num, owned_group_num = 0;
	PMS_ACCOUNT_INFO_T *account_list=NULL, *follow_account=NULL;
	PMS_ACCOUNT_GROUP_INFO_T *group_list=NULL;

	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0)
	{
		_dprintf("Can't read account / group list");
		return 0;
	}

	for (follow_account = account_list; follow_account != NULL; follow_account = follow_account->next)
	{
		//_dprintf("[%s] active=%d, passwd=%s mail=%s\n", follow_account->name, follow_account->active, follow_account->passwd, follow_account->email);

		owned_group_num = 0;
		item = json_object_new_object();
		part_jarray = json_object_new_array();

		json_object_object_add(item,"active", json_object_new_int(follow_account->active));
		json_object_object_add(item,"name", json_object_new_string(follow_account->name));
		json_object_object_add(item,"passwd", json_object_new_string(follow_account->passwd));
		json_object_object_add(item,"email", json_object_new_string(follow_account->email));
		json_object_object_add(item,"desc", json_object_new_string(follow_account->desc));

		PMS_OWNED_INFO_T *owned_group = follow_account->owned_group;
		while (owned_group != NULL)
		{
			PMS_ACCOUNT_GROUP_INFO_T *Group_owned = (PMS_ACCOUNT_GROUP_INFO_T *)owned_group->member;
			//_dprintf("[%s] owned: %s\n", follow_account->name, Group_owned->name);
			owned_group = owned_group->next;
			json_object_array_add(part_jarray,json_object_new_string(Group_owned->name));
			owned_group_num++;
		}
		json_object_object_add(item,"owned_group", part_jarray);	//add Partition to item

		json_object_object_add(item,"owned_group_num", json_object_new_int(owned_group_num));
		json_object_array_add(jarray,item);
	}

	websWrite(wp, "%s", json_object_to_json_string(jarray));
	json_object_put(item);

	PMS_FreeAccInfo(&account_list, &group_list);
	return 0;
}

static int
ej_pms_accgroup_info(int eid, webs_t wp, int argc, char **argv)
{
	struct json_object *item = NULL;
	json_object *part_jarray = NULL;
	json_object *jarray = json_object_new_array();

	int acc_num, group_num, owned_account_num = 0;;
	PMS_ACCOUNT_INFO_T *account_list=NULL;
	PMS_ACCOUNT_GROUP_INFO_T *group_list=NULL, *follow_group=NULL;

	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0)
	{
		_dprintf("Can't read account / group list");
		return 0;
	}

	for (follow_group = group_list; follow_group != NULL; follow_group = follow_group->next)
	{
		owned_account_num = 0;
		item = json_object_new_object();
		part_jarray = json_object_new_array();

		json_object_object_add(item,"active", json_object_new_int(follow_group->active));
		json_object_object_add(item,"name", json_object_new_string(follow_group->name));
		json_object_object_add(item,"desc", json_object_new_string(follow_group->desc));

		//_dprintf("[%s] active=%d\n", follow_group->name, follow_group->active);
		PMS_OWNED_INFO_T *owned_account = follow_group->owned_account;
		while (owned_account != NULL)
		{
			PMS_ACCOUNT_GROUP_INFO_T *Account_owned = (PMS_ACCOUNT_GROUP_INFO_T *)owned_account->member;
			//_dprintf("[%s] owned: %s\n", follow_group->name, Account_owned->name);
			owned_account = owned_account->next;
			json_object_array_add(part_jarray,json_object_new_string(Account_owned->name));
			owned_account_num++;
		}
		json_object_object_add(item,"owned_account", part_jarray);	//add Partition to item
		json_object_object_add(item,"owned_account_num", json_object_new_int(owned_account_num));
		json_object_array_add(jarray,item);
	}

	websWrite(wp, "%s", json_object_to_json_string(jarray));
	json_object_put(item);

	PMS_FreeAccInfo(&account_list, &group_list);
	return 0;
}

static int
ej_pms_device_info(int eid, webs_t wp, int argc, char **argv)
{
	struct json_object *item = NULL;
	json_object *part_jarray = NULL;
	json_object *jarray = json_object_new_array();

	int ret=0;
	int dev_num, group_num, owned_group_num = 0;
	PMS_DEVICE_INFO_T *device_list, *follow_account;
	PMS_DEVICE_GROUP_INFO_T *group_list;

	// get the device list
	if(( ret = PMS_GetDeviceInfo(PMS_ACTION_GET_FULL, &device_list, &group_list, &dev_num, &group_num)) < 0){
		_dprintf("Can't read the account list.\n");
		return 0;
	}

	for(follow_account = device_list; follow_account != NULL; follow_account = follow_account->next){
		owned_group_num = 0;
		//_dprintf("%d\t   %s\t   %s\n", follow_account->active, follow_account->mac, follow_account->desc);
		item = json_object_new_object();
		part_jarray = json_object_new_array();

		json_object_object_add(item,"active", json_object_new_int(follow_account->active));
		json_object_object_add(item,"mac", json_object_new_string(follow_account->mac));
		json_object_object_add(item,"devname", json_object_new_string(follow_account->devname));
		json_object_object_add(item,"devtype", json_object_new_int(follow_account->devtype));
		json_object_object_add(item,"desc", json_object_new_string(follow_account->desc));

		PMS_OWNED_INFO_T *owned_group=follow_account->owned_group;
		while(owned_group!=NULL){
			PMS_DEVICE_GROUP_INFO_T *Group_owned=(PMS_DEVICE_GROUP_INFO_T *)owned_group->member;
			//_dprintf("Owned Group: %s\t", Group_owned->name);
			owned_group=owned_group->next;
			json_object_array_add(part_jarray,json_object_new_string(Group_owned->name));
			owned_group_num++;
		}
		json_object_object_add(item,"device_group", part_jarray);	//add Partition to item
		json_object_object_add(item,"owned_group_num", json_object_new_int(owned_group_num));
		json_object_array_add(jarray,item);
	}

	websWrite(wp, "%s", json_object_to_json_string(jarray));
	json_object_put(item);

	PMS_FreeDevInfo(&device_list, &group_list);
	return 0;
}

static int
ej_pms_devgroup_info(int eid, webs_t wp, int argc, char **argv)
{
	struct json_object *item = NULL;
	json_object *part_jarray = NULL;
	json_object *jarray = json_object_new_array();

	int ret=0;
	int dev_num, group_num, owned_device_num = 0;
	PMS_DEVICE_INFO_T *device_list;
	PMS_DEVICE_GROUP_INFO_T *group_list, *follow_group;

	// get the device list
	if(( ret = PMS_GetDeviceInfo(PMS_ACTION_GET_FULL, &device_list, &group_list, &dev_num, &group_num)) < 0){
		_dprintf("Can't read the account list.\n");
		return 0;
	}

	for(follow_group = group_list; follow_group != NULL; follow_group = follow_group->next){

		//_dprintf("%d\t   %s\t   %s\n", follow_group->active, follow_group->name, follow_group->desc);
		owned_device_num = 0;
		item = json_object_new_object();
		part_jarray = json_object_new_array();

		json_object_object_add(item,"active", json_object_new_int(follow_group->active));
		json_object_object_add(item,"name", json_object_new_string(follow_group->name));
		json_object_object_add(item,"desc", json_object_new_string(follow_group->desc));

		PMS_OWNED_INFO_T *owned_device=follow_group->owned_device;
		while(owned_device!=NULL){
			PMS_DEVICE_INFO_T *Device_owned=(PMS_DEVICE_INFO_T *)owned_device->member;
			//_dprintf("Owned Device: %s\t", Device_owned->mac);
			owned_device=owned_device->next;
			json_object_array_add(part_jarray,json_object_new_string(Device_owned->mac));
			owned_device_num++;
		}
		json_object_object_add(item,"owned_device", part_jarray);	//add Partition to item
		json_object_object_add(item,"owned_device_num", json_object_new_int(owned_device_num));
		json_object_array_add(jarray,item);
	}

	websWrite(wp, "%s", json_object_to_json_string(jarray));
	json_object_put(item);

	PMS_FreeDevInfo(&device_list, &group_list);
	return 0;
}
#endif

#ifdef RTCONFIG_HTTPS
static char* _get_common_name(const char *src, char *dst, size_t len)
{
	char *cp;

	if((cp = strstr(src, "CN=")) == NULL)
	{
		return NULL;
	}

	strlcpy(dst, cp+3, len);

	if((cp = strchr(dst, '/')) != NULL)
	{
		*cp = '\0';
	}

	return dst;
}

//not handle time zone
static void ASN1_TimeToTM(ASN1_TIME* time, struct tm *t)
{
	unsigned char* data = time->data;
	int i = 0;

	if (time->type == V_ASN1_UTCTIME)
	{
		t->tm_year =
			(data[0] - '0') * 10 +
			(data[1] - '0');
		if(t->tm_year < 70)
			t->tm_year += 100;
		i = 2;
	}
	else if (time->type == V_ASN1_GENERALIZEDTIME)
	{
		t->tm_year =
			(data[0] - '0') * 1000 +
			(data[1] - '0') * 100 +
			(data[2] - '0') * 10 +
			(data[3] - '0');
		t->tm_year -= 1900;
		i = 4;
	}
	t->tm_mon = (data[i] - '0') * 10 + (data[i+1] - '0') - 1;
	i += 2;
	t->tm_mday = (data[i] - '0') * 10 + (data[i+1] - '0');
	i += 2;
	t->tm_hour = (data[i] - '0') * 10 + (data[i+1] - '0');
	i += 2;
	t->tm_min  = (data[i] - '0') * 10 + (data[i+1] - '0');
	i += 2;
	t->tm_sec  = (data[i] - '0') * 10 + (data[i+1] - '0');
}

static int
ej_httpd_cert_info(int eid, webs_t wp, int argc, char **argv)
{
	FILE *fp;
	X509 *x509data = NULL;
	char buf[256] = {0};
	char issuer[64] = {0};
	char subject[64] = {0};
	char notBefore[64] = {0};
	char notAfter[64] = {0};
	struct tm tm;
	char cert_path[128] = {0};
#ifdef RTCONFIG_LETSENCRYPT
	int le_enable = nvram_get_int("le_enable");
	int http_enable = nvram_get_int("http_enable");

	if(http_enable == 0)    //http only
	{
		if(le_enable == 1)
#ifdef RTCONFIG_OPENSSL11      // Kludge as we can't link against libletsencrypt due to different OpenSSL versions
			snprintf(cert_path, sizeof(cert_path), "/jffs/.le/%s/cert.pem", nvram_safe_get("ddns_hostname_x"));
#else
			get_path_le_domain_cert(cert_path, sizeof(cert_path));
#endif
		else if(le_enable == 2)
			snprintf(cert_path, sizeof(cert_path), "%s", UPLOAD_CERT);
		else
			snprintf(cert_path, sizeof(cert_path), "%s", HTTPD_CERT);
	}
	else //enable https, show current certificate content
#endif
	{
		snprintf(cert_path, sizeof(cert_path), "%s", HTTPD_CERT);
//		strcpy(cert_path, UPLOAD_CERT);
	}

	fp = fopen(cert_path, "r");

	if (fp == NULL){
		websWrite(wp, "{\"issueTo\":\"\",\"issueBy\":\"\",\"from\":\"\",\"expire\":\"\"}");
		return FALSE;
	}
	if(!PEM_read_X509(fp, &x509data, NULL, NULL))
	{
		fseek(fp, 0, SEEK_SET);
		d2i_X509_fp(fp, &x509data);
	}
	fclose(fp);
	if(x509data == NULL){
		websWrite(wp, "{\"issueTo\":\"\",\"issueBy\":\"\",\"from\":\"\",\"expire\":\"\"}");
		return FALSE;
	}

	X509_NAME_oneline(X509_get_issuer_name(x509data), buf, sizeof(buf));
	_get_common_name(buf, issuer, sizeof(issuer));
	X509_NAME_oneline(X509_get_subject_name(x509data), buf, sizeof(buf));
	_get_common_name(buf, subject, sizeof(subject));
	ASN1_TimeToTM(X509_getm_notBefore(x509data), &tm);
	snprintf(notBefore, sizeof(notBefore), "%d/%d/%d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
	ASN1_TimeToTM(X509_getm_notAfter(x509data), &tm);
	snprintf(notAfter, sizeof(notAfter), "%d/%d/%d", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);

	websWrite(wp, "{");
	websWrite(wp, "\"issueTo\":\"%s\",", subject);

	websWrite(wp, "\"SAN\":\"");

// Dump SANs
	GENERAL_NAMES* names = NULL;
	unsigned char* utf8 = NULL;

	do
	{
		names = X509_get_ext_d2i(x509data, NID_subject_alt_name, 0, 0 );
		if(!names) break;

		int i = 0, count = sk_GENERAL_NAME_num(names);
		if(!count) break; /* failed */

		for( i = 0; i < count; ++i )
		{
			GENERAL_NAME* entry = sk_GENERAL_NAME_value(names, i);
			if(!entry) continue;

			if(GEN_DNS == entry->type)
			{
				int len1 = 0, len2 = -1;

				len1 = ASN1_STRING_to_UTF8(&utf8, entry->d.dNSName);
				if(utf8) {
					len2 = (int)strlen((const char*)utf8);
				}

				if(utf8 && len1 && len2 && (len1 == len2)) {
					websWrite(wp, "%s ", utf8);
				}

				if(utf8) {
					OPENSSL_free(utf8), utf8 = NULL;
				}

			}
		}
	} while (0);

	if(names)
		GENERAL_NAMES_free(names);

	if(utf8)
		OPENSSL_free(utf8);

	websWrite(wp, "\",");

	websWrite(wp, "\"issueBy\":\"%s\",", issuer);
	websWrite(wp, "\"from\":\"%s\",", notBefore);
	websWrite(wp, "\"expire\":\"%s\"", notAfter);
	websWrite(wp, "}");
	if(x509data)
		X509_free(x509data);
	return 0;
}
#endif

#ifdef RTCONFIG_GETREALIP
static int
ej_get_realip(int eid, webs_t wp, int argc, char **argv)
{
	eval("/usr/sbin/getrealip.sh");
	return 0;
}
#endif

static int
ej_get_header_info(int eid, webs_t wp, int argc, char **argv)
{
	struct json_object *item = json_object_new_object();

	char host_name_temp[64];
	char current_page_name_temp[128];
	char *host_name_copy = NULL, *host_name_copy_temp = NULL;

	memset(host_name_temp, 0, sizeof(host_name_temp));
	memset(current_page_name_temp, 0, sizeof(current_page_name_temp));

	if(strncmp(DUT_DOMAIN_NAME, host_name, strlen(DUT_DOMAIN_NAME)) == 0) {
		strcpy(host_name_temp, DUT_DOMAIN_NAME);
	}
#ifdef RTAC68U
	else if (is_dpsta_repeater() && nvram_get_int("re_mode") == 0
		&& strncmp("repeater.asus.com", host_name, strlen("repeater.asus.com")) == 0) {
		strcpy(host_name_temp, "repeater.asus.com");
	}
#endif
	else {
		host_name_copy = strdup(host_name);
		host_name_copy_temp = strtok(host_name_copy,":");
		snprintf(host_name_temp, sizeof(host_name_temp), "%s", host_name_copy_temp);
	}

	if(check_xss_blacklist(current_page_name, 1)) {
		strcpy(current_page_name_temp, indexpage);
	}
	else {
		snprintf(current_page_name_temp, sizeof(current_page_name_temp), "%s", current_page_name);
	}

	json_object_object_add(item,"host", json_object_new_string(host_name_temp));
	json_object_object_add(item,"current_page", json_object_new_string(current_page_name_temp));

	if(do_ssl){
		json_object_object_add(item,"protocol", json_object_new_string("https"));
		json_object_object_add(item,"port", json_object_new_int(nvram_get_int("https_lanport")));
	}
	else{
		json_object_object_add(item,"protocol", json_object_new_string("http"));
		json_object_object_add(item,"port", json_object_new_int(nvram_get_int("http_lanport")));
	}

	websWrite(wp, "%s", json_object_to_json_string(item));

	free(host_name_copy);
	json_object_put(item);

	return 0;
}

static int
ej_login_error_info(int eid, webs_t wp, int argc, char **argv)
{
	struct json_object *item = json_object_new_object();

	/* lock time */
	json_object_object_add(item,"lock_time", json_object_new_int(LOCKTIME - login_dt));

	/* error status */
	json_object_object_add(item,"error_status", json_object_new_int(login_error_status));

	/* url */
	if(check_xss_blacklist(login_url, 1)){
		json_object_object_add(item,"page", json_object_new_string(indexpage));
	}else{
		json_object_object_add(item,"page", json_object_new_string(login_url));
	}
	websWrite(wp, "%s", json_object_to_json_string(item));
	json_object_put(item);
	login_error_status = 0;	//reset error status
	return 0;
}

#if defined(RTCONFIG_VPN_FUSION)
#define VPNC_LOAD_CLIENT_LIST		0x01
#define VPNC_LOAD_PPTP_OPT			0x02

static int _vpnc_set_basic_conf(const char *server, const char *username, const char *passwd, VPNC_BASIC_CONF *basic_conf)
{
	if(!basic_conf)
		return -1;
	
	memset(basic_conf, 0, sizeof(VPNC_BASIC_CONF));

	if(server)
		snprintf(basic_conf->server, sizeof(basic_conf->server), "%s", server);
	if(username)
		snprintf(basic_conf->username, sizeof(basic_conf->username), "%s", username);
	if(passwd)
		snprintf(basic_conf->password, sizeof(basic_conf->password), "%s", passwd);

	return 0;
}

static int _vpnc_load_profile(VPNC_PROFILE *list, const int list_size, const int load_flag)
{
	char *nv = NULL, *nvp = NULL, *b = NULL;
	int cnt = 0, i = 0;
	char * desc, *proto, *server, *username, *passwd, *active, *vpnc_idx;

	if(!list || list_size <= 0)
		return -1;

	if(load_flag & VPNC_LOAD_CLIENT_LIST)
	{
		// load "vpnc_clientlist" to set username, password and server ip
		nv = nvp = strdup(nvram_safe_get("vpnc_clientlist"));

		cnt = 0;
		memset(list, 0, sizeof(VPNC_PROFILE)*list_size);
		while (nv && (b = strsep(&nvp, "<")) != NULL && cnt <= list_size) {
			if (vstrsep(b, ">", &desc, &proto, &server, &username, &passwd, &active, &vpnc_idx) < 4)
				continue;
				
			if(!active || !vpnc_idx)
				continue;

			list[cnt].active = atoi(active);
			list[cnt].vpnc_idx = atoi(vpnc_idx);		

			if(proto && server)
			{
				_vpnc_set_basic_conf(server, username, passwd, &(list[cnt].basic));
				
				if(!strcmp(proto, PROTO_PPTP))
				{
					list[cnt].protocol = VPNC_PROTO_PPTP;
				}
				else if(!strcmp(proto, PROTO_L2TP))
				{
					list[cnt].protocol = VPNC_PROTO_L2TP;
				}
				else if(!strcmp(proto, PROTO_OVPN))
				{
					list[cnt].protocol = VPNC_PROTO_OVPN;
					list[cnt].config.ovpn.ovpn_idx = atoi(server);
				}
				++cnt;
			}
		}
		SAFE_FREE(nv);
	}

	if(load_flag & VPNC_LOAD_PPTP_OPT)
	{
		//load "vpnc_pptp_options_x_list" to set pptp option
		nv = nvp = strdup(nvram_safe_get("vpnc_pptp_options_x_list"));
		i = 0;
		while (nv && (b = strsep(&nvp, "<")) != NULL && i <= list_size) {

			if(i > 0 && VPNC_PROTO_PPTP == list[i - 1].protocol)
			{
				if(!strcmp(b, "auto"))
					list[i - 1].config.pptp.option = VPNC_PPTP_OPT_AUTO;
				else if(!strcmp(b, "-mppc"))
					list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPC;
				else if(!strcmp(b, "+mppe-40"))
					list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPE40;
				else if(!strcmp(b, "+mppe-56"))
					list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPE56;
				else if(!strcmp(b, "+mppe-128"))
					list[i - 1].config.pptp.option = VPNC_PPTP_OPT_MPPE128;
				else
					list[i - 1].config.pptp.option = VPNC_PPTP_OPT_UNDEF;
			}
			++i;	
	}
	SAFE_FREE(nv);
		if(!(load_flag & VPNC_LOAD_CLIENT_LIST))
			cnt = i - 1;
		else if(i != cnt + 1)
			_dprintf("[%s, %d]the numbers of vpnc_clientlist(%d) and vpnc_pptp_options_x_list(%d) are different!\n", __FUNCTION__, __LINE__, cnt, i);
	}
	return cnt;
}


static int
ej_get_new_vpnc_index(int eid, webs_t wp, int argc, char **argv)
{
	VPNC_PROFILE prof[MAX_VPNC_PROFILE];
	unsigned char idx_array[MAX_VPNC_PROFILE] ;
	int prof_cnt = 0, i;

	prof_cnt = _vpnc_load_profile(prof, MAX_VPNC_PROFILE, VPNC_LOAD_CLIENT_LIST);

	memset(idx_array, 0, sizeof(idx_array));

	for(i = 0; i < prof_cnt; ++i)
	{
		if((prof[i].vpnc_idx - VPNC_UNIT_BASIC) >= MAX_VPNC_PROFILE || 
			(prof[i].vpnc_idx - VPNC_UNIT_BASIC) < 0)
			continue;
		idx_array[prof[i].vpnc_idx - VPNC_UNIT_BASIC] = 1;
	}

	for(i = 0; i < MAX_VPNC_PROFILE; ++i)
	{
		if(!idx_array[i])
		{
			websWrite(wp, "%d", i + VPNC_UNIT_BASIC);
			break;
		}
	}
	return 0;
}

static int
ej_get_vpnc_status(int eid, webs_t wp, int argc, char **argv)
{
	char attr_name1[20], attr_name2[20], tmp[256];
	VPNC_PROFILE prof[MAX_VPNC_PROFILE];
	int prof_cnt = 0, i;
	int state_t = 0, sb_state_t = 0;
		
	prof_cnt = _vpnc_load_profile(prof, MAX_VPNC_PROFILE, VPNC_LOAD_CLIENT_LIST);

	for(i = 0; i < prof_cnt; ++i)
		{
		if(!prof[i].active)
		{
			snprintf(tmp, sizeof(tmp), i? "<5>0>%d": "5>0>%d", prof[i].vpnc_idx);
			websWrite(wp, tmp);
		}
		else
		{
			if(prof[i].protocol == VPNC_PROTO_OVPN)
			{
				snprintf(attr_name1, sizeof(attr_name1), "vpn_client%d_state", prof[i].config.ovpn.ovpn_idx);
				snprintf(attr_name2, sizeof(attr_name2), "vpn_client%d_errno", prof[i].config.ovpn.ovpn_idx);

				state_t = atoi(nvram_safe_get(attr_name1));
				if(state_t == -1)	//error
				{
					state_t = 4;
					switch(atoi(nvram_safe_get(attr_name2)))
					{
						case 1:	//ip or route conflict
						case 2:
						case 3:
							sb_state_t = 1;
							break;
						case 4:	//authentication failed
						case 5:
						case 6:
							sb_state_t = 2;
							break;
					}
				}				
			else
				{
					sb_state_t = 0;
				}				
				snprintf(tmp, sizeof(tmp), i? "<%d>%d>%d": "%d>%d>%d", state_t, sb_state_t, prof[i].vpnc_idx);
				websWrite(wp, tmp);
			}
			else if(prof[i].protocol == VPNC_PROTO_PPTP || prof[i].protocol == VPNC_PROTO_L2TP)
			{
				snprintf(attr_name1, sizeof(attr_name1), "vpnc%d_state_t", prof[i].vpnc_idx);
				snprintf(attr_name2, sizeof(attr_name2), "vpnc%d_sbstate_t", prof[i].vpnc_idx);

				state_t = atoi(nvram_safe_get(attr_name1));
				sb_state_t = atoi(nvram_safe_get(attr_name2));

				snprintf(tmp, sizeof(tmp), i? "<%d>%d>%d": "%d>%d>%d", state_t, sb_state_t, prof[i].vpnc_idx);
				websWrite(wp, tmp);
			}
		}
	}
	return 0;
	
}

static int
ej_get_vpnc_nondef_wan_prof_list(int eid, webs_t wp, int argc, char **argv)
{
	char attr_name[20], buf[256] = {0};
	VPNC_PROFILE prof[MAX_VPNC_PROFILE];
	int prof_cnt = 0, i, flag;

	prof_cnt = _vpnc_load_profile(prof, MAX_VPNC_PROFILE, VPNC_LOAD_CLIENT_LIST);

	for(i = 0; i < prof_cnt; ++i)
	{
		flag = 0;
		if(prof[i].protocol == VPNC_PROTO_OVPN)
		{
			snprintf(attr_name, sizeof(attr_name), "vpn_client%d_if", prof[i].config.ovpn.ovpn_idx);
			if(nvram_match(attr_name, "tap"))
			{
				flag = 1;
			}
		}
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), i? "<%d>%d":"%d>%d", prof[i].vpnc_idx, flag);	
	}
	websWrite(wp, buf);
	return 0;
}
#endif

static int
ej_abs_index_page(int eid, webs_t wp, int argc, char **argv)
{
	websWrite(wp, "/%s", indexpage);
	return 0;
}

static int
ej_rel_index_page(int eid, webs_t wp, int argc, char **argv)
{
	websWrite(wp, "%s", indexpage);
	return 0;
}

static int
ej_networkmap_page(int eid, webs_t wp, int argc, char **argv)
{
	websWrite(wp, "%s", NETWORKMAP_PAGE);
	return 0;
}

static int
ej_abs_networkmap_page(int eid, webs_t wp, int argc, char **argv)
{
	websWrite(wp, "/%s", NETWORKMAP_PAGE);
	return 0;
}

static int
ej_get_fwdl_percent(int eid, webs_t wp, int argc, char **argv)
{
	FILE *fp;
	char line[256];
	char fwdl_percent[8];
	char *next;
	char *str;
	int ret = 0;

	/* Read leases file */
	if (!(fp = fopen("/tmp/fwget_log", "r")))
		return ret;

	while ((next = fgets(line, sizeof(line), fp)) != NULL) {
		/* line should start from numeric value */
		if (strstr(next,"%") == NULL)
			continue;

		str = strsep(&next, " ");
		while(str != NULL){
			if (strstr(str,"%") != NULL){
				strlcpy(fwdl_percent, str, sizeof(fwdl_percent));
			}
			str = strsep(&next, " ");
		}
	}
	fclose(fp);
	ret += websWrite(wp,"%s", fwdl_percent);

	return ret;
}

static int
ej_get_wan_lan_status(int eid, webs_t wp, int argc, char **argv)
{
	FILE *fp;
	char line[128], name[sizeof("WAN XXXXXXXXXX")], *ptr, *item, *port, *speed;
	int wan_count, lan_count, ret = 0;
	struct json_object *wanLanLinkSpeed = NULL;
	struct json_object *wanLanCount = NULL;
	struct json_object *wanLanStatus = NULL;

	fp = popen("ATE Get_WanLanStatus", "r");
	if (fp == NULL)
		goto error;

	ptr = fgets(line, sizeof(line), fp);
	pclose(fp);
	if (ptr == NULL)
		goto error;
	ptr = strsep(&ptr, "\r\n");

	wanLanStatus = json_object_new_object();
	wanLanLinkSpeed = json_object_new_object();
	wanLanCount = json_object_new_object();
	if (wanLanStatus == NULL || wanLanLinkSpeed == NULL || wanLanCount == NULL)
		goto error_put;

	wan_count = lan_count = 0;
	while ((item = strsep(&ptr, ";")) != NULL) {
		if (vstrsep(item, "=", &port, &speed) < 2)
			continue;
#if defined(DSL_AC68U)
		if(port[0] == 'W') {
			continue;
		}
#endif
		switch (*port++) {
		case 'W':
			snprintf(name, sizeof(name), "%s%s%s", "WAN", *port ? " " : "", port);
			wan_count++;
			break;
		case 'L':
			snprintf(name, sizeof(name), "%s%s%s", "LAN", *port ? " " : "", port);
			lan_count++;
			break;
		default:
			continue;
		}
		json_object_object_add(wanLanLinkSpeed, name, json_object_new_string(speed));
	}
	json_object_object_add(wanLanCount, "wanCount", json_object_new_int(wan_count));
	json_object_object_add(wanLanCount, "lanCount", json_object_new_int(lan_count));
	json_object_object_add(wanLanStatus, "portSpeed", wanLanLinkSpeed);
	json_object_object_add(wanLanStatus, "portCount", wanLanCount);
	wanLanLinkSpeed = wanLanCount = NULL;

	ptr = (char *)json_object_get_string(wanLanStatus);
	if (ptr == NULL)
		goto error_put;

	ret = websWrite(wp, "%s", ptr);

error_put:
	if (wanLanStatus)
		json_object_put(wanLanStatus);
	if (wanLanLinkSpeed)
		json_object_put(wanLanLinkSpeed);
	if (wanLanCount)
		json_object_put(wanLanCount);
error:
	if (ret == 0)
		ret = websWrite(wp, "{}");

	return ret;
}

static int ej_get_wifi_probe_status(int eid, webs_t wp, int argc, char **argv){

#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
	websWrite(wp, "\"%d\"", get_wifi_probe_result());
#else
	websWrite(wp, "\"\"");
#endif
	return 0;
}

static int ej_get_encrypt_wifi_result(int eid, webs_t wp, int argc, char **argv){

#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
	char wifi_status[100];
	memset(wifi_status, 0, sizeof(wifi_status));
	get_encrypt_wifi_status(wifi_status, sizeof(wifi_status));

	websWrite(wp, "\"%s\"", wifi_status);
#else
	websWrite(wp, "\"\"");
#endif
	return 0;
}


static int ej_get_label_mac(int eid, webs_t wp, int argc, char **argv){

	websWrite(wp, "\"%s\"", get_label_mac());

	return 0;
}

static int ej_get_lan_hwaddr(int eid, webs_t wp, int argc, char **argv){

	if(check_user_agent(user_agent) == FROM_BROWSER)
		websWrite(wp, "%s", get_lan_hwaddr());
	else
		websWrite(wp, "\"%s\"", get_lan_hwaddr());

	return 0;
}

#ifdef RTCONFIG_PUSH_EMAIL
static int
ej_generate_trans_id(int eid, webs_t wp, int argc, char **argv) {
	int idx = 0;
	char transId[] = "0123456789ABCDEF";
	unsigned int rd = 0;

	for(idx = 0; idx < 16; ++idx) {
		f_read("/dev/urandom", &rd, sizeof(unsigned int));
		snprintf(&transId[idx], 2, "%X", rd%16);
	}
	websWrite(wp, "%s", transId);

	return 0;
}
#endif

static int filter_5g_channel_by_bw(struct json_object *output_channel_array, struct json_object *channel_array, int bw){

	int i=0, j=0;
	int del=0, d=0, nr_ch=0, cn_len=0, channel_tmp_int=0;
	int ch[12]={0}, cnt[12]={0};
	struct json_object *channel_tmp=NULL;

	if(bw == 160){
		int ch_t[2]={36,100};
		d = 28;
		nr_ch=8;
		cn_len = sizeof(ch_t)/sizeof(int);
		for(i=0;i<cn_len;i++) ch[i] = ch_t[i];
	}else if(bw == 80){
		int ch_t[6]={36,52,100,116,132,149};
		d=12;
		nr_ch=4;
		cn_len = sizeof(ch_t)/sizeof(int);
		for(i=0;i<cn_len;i++) ch[i] = ch_t[i];
	}else if(bw == 40){
		int ch_t[12]={36,44,52,60,100,108,116,124,132,140,149,157};
		d=4;
		nr_ch=2;
		cn_len = sizeof(ch_t)/sizeof(int);
		for(i=0;i<cn_len;i++) ch[i] = ch_t[i];
	}else{
		for (i = 0; i < json_object_array_length(channel_array); i++){
			channel_tmp = json_object_array_get_idx(channel_array, i);
			json_object_array_add(output_channel_array, channel_tmp);
		}
	}

	//ary = ch_ary.slice();
	for (i = 0; i < json_object_array_length(channel_array); i++){
		channel_tmp = json_object_array_get_idx(channel_array, i);
		channel_tmp_int = safe_atoi(json_object_get_string(channel_tmp));
		for(j=0; j<cn_len; j++){
			if((channel_tmp_int - ch[j]) >= 0 && (channel_tmp_int - ch[j]) <= d)
				cnt[j]++;
		}
	}

	for (i = 0; i < json_object_array_length(channel_array); i++){
		del=1;
		channel_tmp = json_object_array_get_idx(channel_array, i);
		channel_tmp_int = safe_atoi(json_object_get_string(channel_tmp));
		for(j=0; j<cn_len; j++){
			if((channel_tmp_int - ch[j]) >= 0 && (channel_tmp_int - ch[j]) <= d && cnt[j] == nr_ch)
				del=0;
		}
		if(!del){
			json_object_array_add(output_channel_array, channel_tmp);
		}
	}
	dbg("%d: output_channel_array: %s\n", bw, json_object_to_json_string(output_channel_array));
	return 0;
}

static int is_avaiable_channel(struct json_object *filter_channel, struct json_object *chan_compare_tmp){

	int i=0, ret=0;
	struct json_object *avaiable_channel=NULL;

	for (i = 0; i < json_object_array_length(filter_channel); i++){
		avaiable_channel = json_object_array_get_idx(filter_channel, i);
		if (!strcmp(json_object_get_string(avaiable_channel), json_object_get_string(chan_compare_tmp))){
			ret=1;
			break;
		}
	}

	return ret;
}

static int
ej_get_wl_channel_list(int eid, webs_t wp, int argc, char **argv, int unit) {

	int i=0, j=0;
#ifdef RTCONFIG_CFGSYNC
	int avlbl_bw=1;
#endif
	int avlbl_bw20=1, avlbl_bw40=1, avlbl_bw80=1, avlbl_bw160=1;
	char word[256]={0}, *next=NULL;
	char chanspec_buf[8]={0},  chanspec_auto_buf[8]={0}, *chanspec=NULL;
	char *wl_chansps = NULL;
	char tmp[1024], prefix[] = "wlXXXXXXXXXX_";
#ifdef RTCONFIG_CFGSYNC
	AVBL_CHANSPEC_T avbl_chanspec;
	int cfg_rejoin = 0;
	char buf[8];
#ifdef RTCONFIG_AVBLCHAN
	int count_wl_if = 0;
	foreach (word, nvram_safe_get("wl_ifnames"), next)
		count_wl_if++;
#endif
#endif
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	wl_chansps = nvram_safe_get(strcat_r(prefix, "chansps", tmp));
	if(!strcmp(wl_chansps, "")){
		websWrite(wp,"{}");
		return 0;
	}

	struct json_object *chan_tmp=NULL, *chan_compare_tmp=NULL;
	struct json_object *chan_above_array = json_object_new_array();
	struct json_object *chan_below_array = json_object_new_array();
	struct json_object *chan_80m_tmp = json_object_new_array();
	struct json_object *chan_160m_tmp = json_object_new_array();
	struct json_object *chan_20m_array = json_object_new_array();
	struct json_object *chan_40m_array = json_object_new_array();
	struct json_object *chan_80m_array = json_object_new_array();
	struct json_object *chan_160m_array = json_object_new_array();
	struct json_object *chan_auto_array = json_object_new_array();
	struct json_object *chanspec_40m_array = json_object_new_array();
	struct json_object *chanspec_80m_array = json_object_new_array();
	struct json_object *chanspec_160m_array = json_object_new_array();
	struct json_object *chanspec_auto_array = json_object_new_array();
	struct json_object *chan_160m_cfg_tmp = json_object_new_array();
	struct json_object *filter_channel_40 = json_object_new_array();
	struct json_object *filter_channel_80 = json_object_new_array();
	struct json_object *filter_channel_160 = json_object_new_array();
	struct json_object *chan_20m_obj = json_object_new_object();
	struct json_object *chan_40m_obj = json_object_new_object();
	struct json_object *chan_80m_obj = json_object_new_object();
	struct json_object *chan_160m_obj = json_object_new_object();
	struct json_object *chan_auto_obj = json_object_new_object();
	struct json_object *chanspec_obj = json_object_new_object();

	json_object_array_add(chan_20m_array, json_object_new_string("0"));
	json_object_array_add(chan_40m_array, json_object_new_string("0"));
	json_object_array_add(chanspec_40m_array, json_object_new_string("0"));
	json_object_array_add(chan_80m_array, json_object_new_string("0"));
	json_object_array_add(chanspec_80m_array, json_object_new_string("0"));
	json_object_array_add(chan_160m_array, json_object_new_string("0"));
	json_object_array_add(chanspec_160m_array, json_object_new_string("0"));

#ifdef RTCONFIG_CFGSYNC
	/* get onbonding avaiable channel */
	struct json_object *cfg_chan_20m_array = json_object_new_array();
	cfg_rejoin = nvram_get_int("cfg_rejoin");
	if (get_chanspec_info(&avbl_chanspec) == 1 && cfg_rejoin > 0) {
		if(unit == 0){
			avlbl_bw = avbl_chanspec.bw2g;
			for (i = 0; i < MAX_2G_CHANNEL_LIST_NUM && avbl_chanspec.channelList2g[i] != 0; i++) {
				snprintf(buf, sizeof(buf), "%d", avbl_chanspec.channelList2g[i]);
				json_object_array_add(cfg_chan_20m_array, json_object_new_string(buf));
			}
		}else{
			avlbl_bw = avbl_chanspec.bw5g;
#ifdef RTCONFIG_AVBLCHAN
			int exist5ghighch = 0;
			if ((num_of_wl_if() == 2) && avbl_chanspec.existTribandRe) {
				for (i = 0; i < MAX_5G_CHANNEL_LIST_NUM; ++i) {
					if (avbl_chanspec.channelList5g[i] >= 100) {
						exist5ghighch = 1;
						break;
					}
				}
			}
#endif

			for (i = 0; i < MAX_5G_CHANNEL_LIST_NUM && avbl_chanspec.channelList5g[i] != 0; i++) {
#ifdef RTCONFIG_AVBLCHAN
				if (unit == 1 && exist5ghighch && avbl_chanspec.channelList5g[i] < 100) continue; //filter dual band <100 channel
#endif
				snprintf(buf, sizeof(buf), "%d", avbl_chanspec.channelList5g[i]);
				json_object_array_add(cfg_chan_20m_array, json_object_new_string(buf));
			}
		}
		/* avaiable bandwidth */
		avlbl_bw20 = (avlbl_bw & AVLBL_BW20);
		avlbl_bw40 = (avlbl_bw & AVLBL_BW40);
		avlbl_bw80 = (avlbl_bw & AVLBL_BW80);
		avlbl_bw160 = (avlbl_bw & AVLBL_BW160);
	}
	dbg("cfg_chan_20m_array: %s\n", json_object_to_json_string(cfg_chan_20m_array));
#endif

	foreach(word, wl_chansps, next){
		if((chanspec = strchr(word, 'l')) != NULL){
			*chanspec='\0';
			json_object_array_add(chan_above_array, json_object_new_string(word));
		}else if((chanspec = strchr(word, 'u')) != NULL){
			*chanspec='\0';
			json_object_array_add(chan_below_array, json_object_new_string(word));
		}else if((chanspec = strstr(word, "/80")) != NULL){
			*chanspec='\0';
			json_object_array_add(chan_80m_tmp, json_object_new_string(word));
		}else if((chanspec = strstr(word, "/160")) != NULL){
			*chanspec='\0';
			json_object_array_add(chan_160m_tmp, json_object_new_string(word));
		}else{
#ifdef RTCONFIG_CFGSYNC
			/* filter cfg avbl_chanspec base on channel of band */
			if(cfg_rejoin > 0){
				for (i = 0; i < json_object_array_length(cfg_chan_20m_array); i++){
					chan_tmp = json_object_array_get_idx(cfg_chan_20m_array, i);
					if(!strcmp(word, json_object_get_string(chan_tmp)))
						json_object_array_add(chan_20m_array, json_object_new_string(word));
				}
			}else
#endif
				json_object_array_add(chan_20m_array, json_object_new_string(word));
		}
	}
	if(unit> 0){
		filter_5g_channel_by_bw(filter_channel_40, chan_20m_array, 40);
		filter_5g_channel_by_bw(filter_channel_80, chan_20m_array, 80);
		filter_5g_channel_by_bw(filter_channel_160, chan_20m_array, 160);
	}

	for (i = 0; i < json_object_array_length(chan_20m_array); i++)
	{
		memset(chanspec_buf, 0, sizeof(chanspec_buf));
		memset(chanspec_auto_buf, 0, sizeof(chanspec_auto_buf));
		chan_tmp = json_object_array_get_idx(chan_20m_array, i);
		snprintf(chanspec_buf, sizeof(chanspec_buf), "%s", json_object_get_string(chan_tmp));
#ifdef RTCONFIG_CFGSYNC
		if(avlbl_bw20)
			strlcpy(chanspec_auto_buf, chanspec_buf, sizeof(chanspec_auto_buf));
#endif
		/* find 40m above */
		for (j = 0; j < json_object_array_length(chan_above_array); j++)
		{
			chan_compare_tmp = json_object_array_get_idx(chan_above_array, j);
			if(unit> 0 && !is_avaiable_channel(filter_channel_40, chan_compare_tmp))
				continue;

			if (!strcmp(json_object_get_string(chan_tmp), json_object_get_string(chan_compare_tmp)))
			{
				snprintf(chanspec_buf, sizeof(chanspec_buf), "%sl", json_object_get_string(chan_compare_tmp));
#ifdef RTCONFIG_CFGSYNC
				if(cfg_rejoin > 0 && avlbl_bw40)
					strlcpy(chanspec_auto_buf, chanspec_buf, sizeof(chanspec_auto_buf));
#endif
				json_object_array_add(chan_40m_array, chan_tmp);
				json_object_array_add(chanspec_40m_array, json_object_new_string(chanspec_buf));
				if(unit == 0){
					json_object_array_add(chan_auto_array, chan_tmp);
					json_object_array_add(chanspec_auto_array, json_object_new_string(chanspec_buf));
				}
			}
		}
		/* find 40m below */
		for (j = 0; j < json_object_array_length(chan_below_array); j++)
		{
			chan_compare_tmp = json_object_array_get_idx(chan_below_array, j);
			if(unit> 0 && !is_avaiable_channel(filter_channel_40, chan_compare_tmp))
				continue;

			if (!strcmp(json_object_get_string(chan_tmp), json_object_get_string(chan_compare_tmp)))
			{
				snprintf(chanspec_buf, sizeof(chanspec_buf), "%su", json_object_get_string(chan_compare_tmp));
#ifdef RTCONFIG_CFGSYNC
				if(avlbl_bw40)
					strlcpy(chanspec_auto_buf, chanspec_buf, sizeof(chanspec_auto_buf));
#endif
				json_object_array_add(chan_40m_array, chan_tmp);
				json_object_array_add(chanspec_40m_array, json_object_new_string(chanspec_buf));
				if(unit == 0){
					json_object_array_add(chan_auto_array, chan_tmp);
					json_object_array_add(chanspec_auto_array, json_object_new_string(chanspec_buf));
				}
			}
		}
		/* find 80m */
		for (j = 0; j < json_object_array_length(chan_80m_tmp); j++)
		{
			chan_compare_tmp = json_object_array_get_idx(chan_80m_tmp, j);
			if(unit> 0 && !is_avaiable_channel(filter_channel_80, chan_compare_tmp))
				continue;

			if (!strcmp(json_object_get_string(chan_tmp), json_object_get_string(chan_compare_tmp)))
			{
				snprintf(chanspec_buf, sizeof(chanspec_buf), "%s/80", json_object_get_string(chan_compare_tmp));
#ifdef RTCONFIG_CFGSYNC
				if(avlbl_bw80)
					strlcpy(chanspec_auto_buf, chanspec_buf, sizeof(chanspec_auto_buf));
#endif
				json_object_array_add(chan_80m_array, chan_tmp);
				json_object_array_add(chanspec_80m_array, json_object_new_string(chanspec_buf));
			}
		}
		/* find 160m */
		for (j = 0; j < json_object_array_length(chan_160m_tmp); j++)
		{
			chan_compare_tmp = json_object_array_get_idx(chan_160m_tmp, j);
			if(unit> 0 && !is_avaiable_channel(filter_channel_160, chan_compare_tmp))
				continue;

			if (!strcmp(json_object_get_string(chan_tmp), json_object_get_string(chan_compare_tmp)))
			{
				snprintf(chanspec_buf, sizeof(chanspec_buf), "%s/160", json_object_get_string(chan_compare_tmp));
#ifdef RTCONFIG_CFGSYNC
				if(avlbl_bw160)
					strlcpy(chanspec_auto_buf, chanspec_buf, sizeof(chanspec_auto_buf));
#endif
				json_object_array_add(chan_160m_array, chan_tmp);
				json_object_array_add(chanspec_160m_array, json_object_new_string(chanspec_buf));
			}
		}

		if((unit == 0 && strpbrk(chanspec_buf, "lu") == NULL) || unit > 0){
			json_object_array_add(chan_auto_array, chan_tmp);
#ifdef RTCONFIG_CFGSYNC
			if(cfg_rejoin > 0)
				json_object_array_add(chanspec_auto_array, json_object_new_string(chanspec_auto_buf));
			else
#endif
				json_object_array_add(chanspec_auto_array, json_object_new_string(chanspec_buf));
		}
	}

	json_object_object_add(chan_auto_obj, "chanlist", chan_auto_array);
	json_object_object_add(chan_auto_obj, "chanspec", chanspec_auto_array);
	json_object_object_add(chanspec_obj, "auto", chan_auto_obj);

	json_object_object_add(chan_20m_obj, "chanlist", chan_20m_array);
	json_object_object_add(chan_20m_obj, "chanspec", chan_20m_array);
	if(json_object_array_length(chan_20m_array)>1 && avlbl_bw20){
		json_object_object_add(chanspec_obj, "chan_20m", chan_20m_obj);
	}

	json_object_object_add(chan_40m_obj, "chanlist", chan_40m_array);
	json_object_object_add(chan_40m_obj, "chanspec", chanspec_40m_array);
	if(json_object_array_length(chan_40m_array)>1 && avlbl_bw40){
		json_object_object_add(chanspec_obj, "chan_40m", chan_40m_obj);
	}

	json_object_object_add(chan_80m_obj, "chanlist", chan_80m_array);
	json_object_object_add(chan_80m_obj, "chanspec", chanspec_80m_array);
	if(json_object_array_length(chan_80m_array)>1 && avlbl_bw80){
		json_object_object_add(chanspec_obj, "chan_80m", chan_80m_obj);
	}

	if(json_object_array_length(chan_160m_tmp)>1){
		if(avlbl_bw160){
			json_object_object_add(chan_160m_obj, "chanlist", chan_160m_array);
			json_object_object_add(chan_160m_obj, "chanspec", chanspec_160m_array);
			json_object_object_add(chanspec_obj, "chan_160m", chan_160m_obj);
		}
		else{
			json_object_array_add(chan_160m_cfg_tmp, json_object_new_string("0"));
			json_object_object_add(chan_160m_obj, "chanlist", chan_160m_cfg_tmp);
			json_object_object_add(chan_160m_obj, "chanspec", chan_160m_cfg_tmp);
			json_object_object_add(chanspec_obj, "chan_160m", chan_160m_obj);
		}
	}

	websWrite(wp,"%s", json_object_to_json_string(chanspec_obj));

	if(chan_above_array)	json_object_put(chan_above_array);
	if(chan_below_array)	json_object_put(chan_below_array);
	if(chan_80m_tmp)	json_object_put(chan_80m_tmp);
	if(chan_160m_tmp)	json_object_put(chan_160m_tmp);
	if(chan_160m_array)	json_object_put(chan_160m_array);
	if(chanspec_160m_array) json_object_put(chanspec_160m_array);
	if(chan_160m_cfg_tmp) json_object_put(chan_160m_cfg_tmp);
	if(filter_channel_40)	json_object_put(filter_channel_40);
	if(filter_channel_80)	json_object_put(filter_channel_80);
	if(filter_channel_160)	json_object_put(filter_channel_160);
	if(chan_20m_obj)	json_object_put(chan_20m_obj);
	if(chan_40m_obj)	json_object_put(chan_40m_obj);
	if(chan_80m_obj)	json_object_put(chan_80m_obj);
	if(chan_160m_obj) 	json_object_put(chan_160m_obj);
	if(chan_auto_obj) 	json_object_put(chan_auto_obj);
	if(chanspec_obj)	json_object_put(chanspec_obj);
#ifdef RTCONFIG_CFGSYNC
	if(cfg_chan_20m_array) json_object_put(cfg_chan_20m_array);
#endif

	return 0;
}

static int
ej_get_wl_channel_list_2g(int eid, webs_t wp, int argc, char **argv)
{
#ifdef CONFIG_BCMWL5
	ej_wl_chanspecs_2g(0, NULL, 1, NULL);
	return ej_get_wl_channel_list(eid, wp, argc, argv, 0);
#else
	return 0;
#endif
}

static int
ej_get_wl_channel_list_5g(int eid, webs_t wp, int argc, char **argv) {
#ifdef CONFIG_BCMWL5
	ej_wl_chanspecs_5g(0, NULL, 1, NULL);
	return ej_get_wl_channel_list(eid, wp, argc, argv, 1);
#else
	return 0;
#endif
}

static int
ej_get_wl_channel_list_5g_2(int eid, webs_t wp, int argc, char **argv) {
#ifdef CONFIG_BCMWL5
	ej_wl_chanspecs_5g_2(0, NULL, 1, NULL);
	return ej_get_wl_channel_list(eid, wp, argc, argv, 2);
#else
	return 0;
#endif
}

static int ej_get_sw_mode(int eid, webs_t wp, int argc, char **argv) {

	websWrite(wp, "\"%d\"", get_sw_mode());

	return 0;
}

#ifdef RTCONFIG_CONNDIAG
static int ej_get_diag_db(int eid, webs_t wp, int argc, char **argv) {
	int rows = 0;
	int cols = 0;
	char **result = NULL;
	int ret = -1;
	int r = 0, c = 0, i;
	//char event_name[16];

	ret = get_json_on_day(0, NULL, NULL, NULL, &rows, &cols, &result);
	if(ret == SQLITE_OK){
#if 0
		snprintf(event_name, sizeof(event_name), "All");

		websWrite(wp, "**************************************************\n");
		websWrite(wp, "%s events, rows=%d, cols=%d:\n", event_name, rows, cols);
		websWrite(wp, "**************************************************\n");
		for(i = 0; i < cols; ++i){
			if(i != 0)
				websWrite(wp, "|");
			websWrite(wp, "%s", result[i]);
		}
		websWrite(wp, "\n");
#else
		i = cols;
#endif

		for(r = 0; r < rows; ++r){
			if(r != 0)
				websWrite(wp, ", ");

			//websWrite(wp, "--------------------------------------------------\n");
			for(c = 0; c < cols; ++c, ++i){
				if(c != 0)
					//websWrite(wp, "|");
					websWrite(wp, ", ");
				websWrite(wp, "%s", result[i]);
			}

			//websWrite(wp, "\n");
		}
		//websWrite(wp, "**************************************************\n");
		sqlite3_free_table(result);
	}
	else
		//websWrite(wp, "Wrong!\n");
		websWrite(wp, "{}");

	return 0;
}
#endif

static int ej_wan_bonding_speed(int eid, webs_t wp, int argc, char **argv) { //bond0: LAN aggregation bond1: WAN aggregation
#ifdef RTCONFIG_BONDING_WAN
	if(check_user_agent(user_agent) == FROM_BROWSER)
		websWrite(wp, "%d", get_bonding_speed("bond1"));//bond0: LAN aggregation bond1: WAN aggregation
	else
		websWrite(wp, "\"%d\"", get_bonding_speed("bond1"));
#else
	websWrite(wp, "\"0\"");
#endif
	return 0;
}

static int ej_wan_bonding_p1_status(int eid, webs_t wp, int argc, char **argv)
{
#ifdef RTCONFIG_BONDING_WAN
	if(check_user_agent(user_agent) == FROM_BROWSER)
		websWrite(wp, "%d", get_bonding_port_status(0));
	else
		websWrite(wp, "\"%d\"", get_bonding_port_status(0));
#else
	websWrite(wp, "\"0\"");
#endif
	return 0;
}

static int ej_wan_bonding_p2_status(int eid, webs_t wp, int argc, char **argv)
{
#ifdef RTCONFIG_BONDING_WAN
	if(check_user_agent(user_agent) == FROM_BROWSER)
		websWrite(wp, "%d", get_bonding_port_status(4));
	else
		websWrite(wp, "\"%d\"", get_bonding_port_status(4));
#else
	websWrite(wp, "\"0\"");
#endif
	return 0;
}

struct ej_handler ej_handlers[] = {
	{ "nvram_get", ej_nvram_get},
	{ "nvram_default_get", ej_nvram_default_get},
	{ "nvram_match", ej_nvram_match},
	{ "nvram_double_match", ej_nvram_double_match},
	{ "nvram_show_chinese_char", ej_nvram_show_chinese_char}, // 2012.05 Jieming add to show chinese char in mediaserver.asp
	// the follwoing will be removed
	{ "nvram_get_x", ej_nvram_get_x},
	{ "nvram_get_f", ej_nvram_get_f},
	{ "nvram_get_list_x", ej_nvram_get_list_x},
	{ "nvram_get_buf_x", ej_nvram_get_buf_x},
	{ "nvram_match_x", ej_nvram_match_x},
	{ "nvram_double_match_x", ej_nvram_double_match_x},
	{ "nvram_match_both_x", ej_nvram_match_both_x},
	{ "nvram_match_list_x", ej_nvram_match_list_x},
	{ "select_channel", ej_select_channel},
	{ "uptime", ej_uptime},
	{ "sysuptime", ej_sysuptime},
	{ "nvram_dump", ej_dump},
	//{ "load_script", ej_load},
	{ "dhcpLeaseInfo", ej_dhcpLeaseInfo},
	{ "dhcpLeaseMacList", ej_dhcpLeaseMacList},
	{ "IP_dhcpLeaseInfo", ej_IP_dhcpLeaseInfo},
//tomato qosvvvvvvvvvvv 2010.08 Viz
	{ "qrate", ej_qos_packet},
	{ "ctdump", ej_ctdump},
	{ "netdev", ej_netdev},

#if !defined(HND_ROUTER)
	{ "iptraffic", ej_iptraffic},
	{ "iptmon", ej_iptmon},
	{ "ipt_bandwidth", ej_ipt_bandwidth},
#endif
#ifdef RTCONFIG_BWDPI
	{ "bwdpi_conntrack", ej_bwdpi_conntrack},
#endif
	{ "bandwidth", ej_bandwidth},
#ifdef RTCONFIG_DSL
	{ "spectrum", ej_spectrum}, //Ren
	{ "get_annexmode", ej_getAnnexMode}, //Ren
	{ "get_adsltoneamount", ej_getADSLToneAmount}, //Ren
	{ "show_file_content", show_file_content}, //Ren
#endif
	{ "backup_nvram", ej_backup_nvram},
//tomato qos^^^^^^^^^^^^ end Viz
	{ "wl_get_parameter", ej_wl_get_parameter},
	{ "wl_get_guestnetwork", ej_wl_get_guestnetwork},
	{ "wan_get_parameter", ej_wan_get_parameter},
	{ "lan_get_parameter", ej_lan_get_parameter},
#ifdef RTCONFIG_DSL
	{ "dsl_get_parameter", ej_dsl_get_parameter},
#endif

#ifdef ASUS_DDNS //2007.03.27 Yau add
	{ "nvram_get_ddns", ej_nvram_get_ddns},
	{ "nvram_char_to_ascii", ej_nvram_char_to_ascii},
//	{ "load_clientlist_char_to_ascii", ej_load_clientlist_char_to_ascii},
	{ "get_clientlist_from_json_database", ej_get_clientlist_from_json_database},
#endif
	{ "get_basic_clientlist", ej_get_basic_clientlist},
	{ "get_basic_clientlist_count", ej_get_basic_clientlist_count},
	{ "get_all_basic_clientlist", ej_get_all_basic_clientlist},
	{ "get_wol_clientlist", ej_get_wol_clientlist},
#ifdef RTCONFIG_INTERNETCTRL
	{ "get_clientlist_reportstate", ej_get_clientlist_reportstate},
#endif
//2008.08 magic{
	{ "update_variables", ej_update_variables},
#ifdef RTCONFIG_ROG
	{ "set_variables", ej_set_variables},
#endif
	{ "convert_asus_variables", convert_asus_variables},
	{ "asus_nvram_commit", asus_nvram_commit},
	{ "notify_services", ej_notify_services},
	{ "wanstate", wanstate_hook},
	{ "dual_wanstate", dual_wanstate_hook},
	{ "ajax_dualwanstate", ajax_dualwanstate_hook},
	{ "ajax_wanstate", ajax_wanstate_hook},
	{ "secondary_ajax_wanstate", secondary_ajax_wanstate_hook},
#ifdef RTCONFIG_DSL
	{ "wanlink_dsl", wanlink_hook_dsl},
#endif
	{ "wanlink", wanlink_hook},
	{ "first_wanlink", first_wanlink_hook},
	{ "secondary_wanlink", secondary_wanlink_hook},
	{ "wanlink_state", wanlink_state_hook},
	{ "wan_action", wan_action_hook},
	{ "get_wan_unit", get_wan_unit_hook},
	{ "get_parameter", ej_get_parameter},
	{ "get_ascii_parameter", ej_get_ascii_parameter},
	{ "login_state_hook", login_state_hook},
	{ "is_logined_hook", ej_is_logined_hook},
#ifdef RTCONFIG_FANCTRL
	{ "get_fanctrl_info", get_fanctrl_info},
#endif
#ifdef RTCONFIG_BCMARM
	{ "get_cpu_temperature", get_cpu_temperature},
#endif
	{ "get_machine_name" , get_machine_name},
	{ "dhcp_leases", ej_dhcp_leases},
	{ "get_arp_table", ej_get_arp_table},
	{ "get_static_client", ej_get_static_client},
	{ "yadns_servers", yadns_servers_hook},
	{ "yadns_clients", yadns_clients_hook},
	{ "get_changed_status", ej_get_changed_status},
	{ "shown_language_css", ej_shown_language_css},
	{ "memory_usage", ej_memory_usage},
	{ "cpu_usage", ej_cpu_usage},
	{ "cpu_core_num", ej_cpu_core_num},
#if defined(RTCONFIG_REALTEK) || defined(RTCONFIG_WIRELESSWAN)
	/* MUST: Need to clarify how does the RP-AC87's ej_SiteSurvey */
	{ "sitesurvey", ej_SiteSurvey},
#endif
#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
	{ "get_ap_info", ej_get_ap_info},
#endif
	{ "ddns_info", ej_ddnsinfo},
	{ "show_usb_path", ej_show_usb_path},
	{ "usb_is_exist", ej_usb_is_exist},
	{ "apps_fsck_ret", ej_apps_fsck_ret},
	{ "get_all_accounts", ej_get_all_accounts},
#ifdef RTCONFIG_USB
	{ "disk_pool_mapping_info", ej_disk_pool_mapping_info},
	{ "available_disk_names_and_sizes", ej_available_disk_names_and_sizes},
	{ "get_printer_info", ej_get_printer_info},
	{ "get_modem_info", ej_get_modem_info},
	{ "get_modem_fullsignal", ej_get_modem_fullsignal},
	{ "get_isp_scan_results", ej_get_isp_scan_results},
	{ "get_simact_result", ej_get_simact_result},
	{ "get_modemuptime", ej_modemuptime},
#ifdef RTCONFIG_USB_MULTIMODEM
	{ "get_simact1_result", ej_get_simact1_result},
	{ "get_modem1uptime", ej_modem1uptime},
#endif
#if defined(RTCONFIG_USB_SMS_MODEM) && !defined(RTCONFIG_USB_MULTIMODEM)
	{ "getSMSbyType", ej_getSMSbyType},
	{ "getPhonebook", ej_getPhonebook},
#endif
	{ "get_AiDisk_status", ej_get_AiDisk_status},
	{ "set_AiDisk_status", ej_set_AiDisk_status},
	{ "safely_remove_disk", ej_safely_remove_disk},
	{ "get_permissions_of_account", ej_get_permissions_of_account},
	{ "set_account_permission", ej_set_account_permission},
	{ "set_account_all_folder_permission", ej_set_account_all_folder_permission},
	{ "initial_account", ej_initial_account},
	{ "create_account", ej_create_account},	/*no ccc*/
	{ "delete_account", ej_delete_account}, /*n*/
	{ "modify_account", ej_modify_account}, /*n*/
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	{ "get_all_groups", ej_get_all_groups},
	{ "get_permissions_of_group", ej_get_permissions_of_group},
	{ "set_group_permission", ej_set_group_permission},
	{ "set_group_all_folder_permission", ej_set_group_all_folder_permission},
#endif
	{ "get_folder_tree", ej_get_folder_tree},
	{ "get_share_tree", ej_get_share_tree},
	{ "create_sharedfolder", ej_create_sharedfolder},	/*y*/
	{ "delete_sharedfolder", ej_delete_sharedfolder},	/*y*/
	{ "modify_sharedfolder", ej_modify_sharedfolder},	/* no ccc*/
	{ "set_share_mode", ej_set_share_mode},
	{ "initial_folder_var_file", ej_initial_folder_var_file},
	{ "usb_port_stor_act", ej_usb_port_stor_act},
	{ "get_usb_info", ej_get_usb_info},
#ifdef RTCONFIG_DISK_MONITOR
	{ "apps_fsck_log", ej_apps_fsck_log},
	{ "get_disk_format_log", ej_get_disk_format_log},
#endif
	{ "apps_info", ej_apps_info},
	{ "apps_state_info", ej_apps_state_info},
	{ "apps_action", ej_apps_action},
#ifdef RTCONFIG_MEDIA_SERVER
	{ "dms_info", ej_dms_info},
#endif
//#ifdef RTCONFIG_CLOUDSYNC
	{ "cloud_status", ej_cloud_status},
	{ "UI_cloud_status", ej_UI_cloud_status},
	{ "UI_cloud_dropbox_status", ej_UI_cloud_dropbox_status},
	{ "UI_cloud_ftpclient_status", ej_UI_cloud_ftpclient_status},
	{ "UI_cloud_sambaclient_status", ej_UI_cloud_sambaclient_status},
	{ "UI_cloud_usbclient_status", ej_UI_cloud_usbclient_status},
	{ "UI_rs_status", ej_UI_rs_status},
	{ "getWebdavInfo", ej_webdavInfo},
//#endif
#endif
	{ "start_autodet", start_autodet},
	{ "start_force_autodet", start_force_autodet},
#ifdef RTCONFIG_QCA_PLC_UTILS
	{ "start_plcdet", start_plcdet},
	{ "plc_status", ej_plc_status},
#endif
#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
	{ "start_wlcscan", start_wlcscan},
#endif
	// system or solution dependant part start from here
	{ "wl_sta_list_2g", ej_wl_sta_list_2g},
	{ "wl_sta_list_5g", ej_wl_sta_list_5g},
#if defined(CONFIG_BCMWL5) || defined(MAPAC2200)
#ifndef RTCONFIG_QTN
	{ "wl_sta_list_5g_2", ej_wl_sta_list_5g_2},
#endif
#endif
#ifdef RTCONFIG_STAINFO
	{ "wl_stainfo_list_2g", ej_wl_stainfo_list_2g},
	{ "wl_stainfo_list_5g", ej_wl_stainfo_list_5g},
#if !defined(RTCONFIG_QTN) && !defined(RTCONFIG_RALINK) && ((!defined(RTCONFIG_QCA)) || defined(MAPAC2200)) && !defined(RTCONFIG_ALPINE) && !defined(RTCONFIG_LANTIQ)
	{ "wl_stainfo_list_5g_2", ej_wl_stainfo_list_5g_2},
#endif
#endif
	{ "wl_auth_list", ej_wl_auth_list},
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ)
	{ "wl_control_channel", ej_wl_control_channel},
	{ "wl_extent_channel", ej_wl_extent_channel},
#endif
	{ "get_wlstainfo_list", ej_get_wlstainfo_list},
#ifdef RTCONFIG_DSL
	{ "start_dsl_autodet", start_dsl_autodet},
	{ "get_isp_list", ej_get_isp_list},
	{ "get_isp_dhcp_opt_list", ej_get_isp_dhcp_opt_list},
	{ "get_DSL_WAN_list", ej_get_DSL_WAN_list},
#endif
	{ "wl_scan_2g", ej_wl_scan_2g},
	{ "wl_scan_5g", ej_wl_scan_5g},
#if defined(CONFIG_BCMWL5) || defined(MAPAC2200)
	{ "wl_scan_5g_2", ej_wl_scan_5g_2},
#endif
	{ "channel_list_2g", ej_wl_channel_list_2g},
	{ "channel_list_5g", ej_wl_channel_list_5g},
#if defined(CONFIG_BCMWL5) || defined(MAPAC2200)
	{ "channel_list_5g_2", ej_wl_channel_list_5g_2},
#endif
#if defined(RTCONFIG_QTN) || defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
	{ "channel_list_5g_20m", ej_wl_channel_list_5g_20m},
	{ "channel_list_5g_40m", ej_wl_channel_list_5g_40m},
	{ "channel_list_5g_80m", ej_wl_channel_list_5g_80m},
#else
	{ "channel_list_5g_20m", ej_wl_channel_list_5g},
	{ "channel_list_5g_40m", ej_wl_channel_list_5g},
	{ "channel_list_5g_80m", ej_wl_channel_list_5g},
#endif
#if defined(RTCONFIG_WIGIG)
	{ "channel_list_60g", ej_wl_channel_list_60g},
#endif
#ifdef CONFIG_BCMWL5
	{ "chanspecs_2g", ej_wl_chanspecs_2g},
	{ "chanspecs_5g", ej_wl_chanspecs_5g},
	{ "chanspecs_5g_2", ej_wl_chanspecs_5g_2},
#endif
#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_REALTEK)
	{ "wl_rssi_2g", ej_wl_rssi_2g},
	{ "wl_rssi_5g", ej_wl_rssi_5g},
	{ "wl_rssi_5g_2", ej_wl_rssi_5g_2},
#endif
#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G)
	{ "wl_rate_2g", ej_wl_rate_2g},
	{ "wl_rate_5g", ej_wl_rate_5g},
#if defined(CONFIG_BCMWL5) || defined(MAPAC2200)
	{ "wl_rate_5g_2", ej_wl_rate_5g_2},
#endif
#endif
	{ "nat_accel_status", ej_nat_accel_status },
	{ "get_wl_channel_list_2g", ej_get_wl_channel_list_2g },
	{ "get_wl_channel_list_5g", ej_get_wl_channel_list_5g },
	{ "get_wl_channel_list_5g_2", ej_get_wl_channel_list_5g_2 },
#ifdef RTCONFIG_PROXYSTA
	{ "wlc_psta_state", ej_wl_auth_psta},
#endif
#ifdef RTCONFIG_CONCURRENTREPEATER
	{ "get_default_ssid", ej_get_default_ssid},
#endif
	{ "get_default_reboot_time", ej_get_default_reboot_time},
	{ "sysinfo",  ej_show_sysinfo},
#ifdef RTCONFIG_IPV6
#ifdef RTCONFIG_IGD2
	{ "ipv6_pinholes",  ej_ipv6_pinhole_array},
#endif
	{ "get_ipv6net_array", ej_lan_ipv6_network_array},
#endif
	{ "get_leases_array", ej_get_leases_array},
	{ "get_vserver_array", ej_get_vserver_array},
	{ "get_upnp_array", ej_get_upnp_array},
	{ "get_route_array", ej_get_route_array},
	{ "get_tcclass_array", ej_tcclass_dump_array},
	{ "get_connlist_array", ej_connlist_array},
	{ "get_custom_settings", ej_get_custom_settings},
#ifdef RTCONFIG_BCMWL6
	{ "get_wl_status", ej_wl_status_2g_array},
#endif
	{ "radio_status", ej_radio_status},
	{ "asus_sysinfo", ej_sysinfo},
#ifdef RTCONFIG_OPENVPN
	{ "vpn_server_get_parameter", ej_vpn_server_get_parameter},
	{ "vpn_client_get_parameter", ej_vpn_client_get_parameter},
	{ "vpn_crt_server", ej_vpn_crt_server},
	{ "vpn_crt_client", ej_vpn_crt_client},
#endif
	{ "nvram_clean_get", ej_nvram_clean_get},
	{ "check_pw", ej_check_pw},
	{ "check_acpw", ej_check_acpw},
	{ "check_acorpw", ej_check_acorpw},
	{ "check_ftp_samba_anonymous", ej_check_ftp_samba_anonymous},
	{ "check_passwd_strength", ej_check_passwd_strength},
	{ "check_wireless_encryption", ej_check_wireless_encryption},
	{ "get_clientlist", ej_get_clientlist},
	{ "get_clientlist_maclist", ej_get_clientlist_maclist},
#if defined(RTCONFIG_BWDPI)
	{ "bwdpi_status", ej_bwdpi_status},
	{ "bwdpi_history", ej_bwdpi_history},
	{ "bwdpi_redirect_info", ej_bwdpi_redirect_page_status},
	{ "bwdpi_appStat", ej_bwdpi_appStat},
	{ "bwdpi_wanStat", ej_bwdpi_wanStat},
	{ "bwdpi_wanStat_detail", ej_bwdpi_wanStat_detail},
	{ "bwdpi_engine_status", ej_bwdpi_engine_status},
	{ "bwdpi_monitor_stat", ej_bwdpi_monitor_stat},
	{ "bwdpi_monitor_info", ej_bwdpi_monitor_info},
	{ "bwdpi_monitor_ips", ej_bwdpi_monitor_ips},
	{ "bwdpi_monitor_nonips", ej_bwdpi_monitor_nonips},
#else
	{ "bwdpi_engine_status", ej_bwdpi_engine_status},
#endif
#ifdef RTCONFIG_TRAFFIC_LIMITER
	{ "traffic_limiter_wanStat", ej_traffic_limiter_wanStat},
#endif
	{ "wl_nband_info", ej_wl_nband_info},
#ifdef RTCONFIG_GEOIP
	{ "geoiplookup", ej_geoiplookup},
#endif
#ifdef RTCONFIG_JFFS2USERICON
	{ "get_upload_icon", ej_get_upload_icon},
	{ "get_upload_icon_count_list", ej_get_upload_icon_count_list},
#endif
	{ "findasus", ej_findasus},
#ifdef RTCONFIG_WTFAST
	{ "wtfast_status", ej_wtfast_status },
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	{ "chk_lte_fw", ej_chk_lte_fw},
#endif
	{ "check_asus_model", ej_check_asus_model},
#ifdef RTCONFIG_IPSEC	
	{ "get_ipsec_conn", ej_get_ipsec_conn},
#endif	
#ifdef RTCONFIG_CAPTIVE_PORTAL
	{ "get_customized_attribute", ej_get_customized_attribute},
	{ "get_CPInfo", ej_get_CPInfo},
#endif
	{ "generate_region", ej_generate_region},
	{ "get_support_region_list", ej_get_support_region_list},
	{ "get_next_lanip", ej_get_next_lanip},
	{ "chdom", ej_chdom},
#ifdef RTCONFIG_AMAS
	{ "chcap", ej_chcap},
#ifdef RTCONFIG_ADV_RAST
	{ "act_chksta", ej_chksta},
	{ "show_info_between_nodes", ej_show_info_between_nodes},
#endif
#endif
#ifdef RTCONFIG_CFGSYNC
	{ "get_cfg_client_info", ej_get_cfg_client_info},
	{ "get_cfg_clientlist", ej_get_cfg_clientlist},
	{ "get_wclientlist", ej_get_wclientlist},
	{ "get_allclientlist", ej_get_allclientlist},
        { "get_wiredclientlist", ej_get_wiredclientlist},
	{ "get_onboardinglist", ej_get_onboardinglist},
	{ "get_onboardingstatus", ej_get_onboardingstatus},
#endif
#ifdef RTCONFIG_NOTIFICATION_CENTER
	{ "get_nt_db", ej_get_nt_db},
	{ "get_nt_db_app", ej_get_nt_db_app},
#endif
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	{ "pms_account_info", ej_pms_account_info},
	{ "pms_accgroup_info", ej_pms_accgroup_info},
	{ "pms_device_info", ej_pms_device_info},
	{ "pms_devgroup_info", ej_pms_devgroup_info},
#endif
#ifdef RTCONFIG_GETREALIP
	{ "get_realip", ej_get_realip},
#endif
#ifdef RTCONFIG_HTTPS
	{ "httpd_cert_info", ej_httpd_cert_info},
#endif

	{ "get_header_info", ej_get_header_info},
	{ "login_error_info", ej_login_error_info},
#if defined(RTCONFIG_VPN_FUSION)
	{ "get_new_vpnc_index", ej_get_new_vpnc_index},
	{ "get_vpnc_status", ej_get_vpnc_status},
	{ "get_vpnc_nondef_wan_prof_list", ej_get_vpnc_nondef_wan_prof_list},
#endif
	{ "abs_index_page", ej_abs_index_page},
	{ "rel_index_page", ej_rel_index_page},
	{ "networkmap_page", ej_networkmap_page},
	{ "abs_networkmap_page", ej_abs_networkmap_page},
	{ "get_fwdl_percent", ej_get_fwdl_percent},
	{ "get_wan_lan_status", ej_get_wan_lan_status},
	{ "get_wifi_probe_status", ej_get_wifi_probe_status},
	{ "get_encrypt_wifi_result", ej_get_encrypt_wifi_result},
	{ "get_lan_hwaddr", ej_get_lan_hwaddr},
	{ "get_label_mac", ej_get_label_mac},
	{ "get_ui_support", ej_get_ui_support},
#ifdef RTCONFIG_PUSH_EMAIL
	{ "generate_trans_id", ej_generate_trans_id},
#endif
	{ "get_sw_mode", ej_get_sw_mode},
#ifdef RTCONFIG_CONNDIAG
	{ "get_diag_db", ej_get_diag_db},
#endif
	{ "get_iptvSettings", ej_get_iptvSettings },
	{ "wan_bonding_speed", ej_wan_bonding_speed },
	{ "wan_bonding_p1_status", ej_wan_bonding_p1_status},
	{ "wan_bonding_p2_status", ej_wan_bonding_p2_status},
#ifdef RTCONFIG_DNSPRIVACY
	{ "get_dnsprivacy_presets", ej_get_dnsprivacy_presets },
#endif
	{ NULL, NULL }
};

void websSetVer(void)
{
	FILE *fp;
	unsigned long *imagelen;
	char dataPtr[4];
	char verPtr[64];
	char productid[13];
	char fwver[12];

	strcpy(productid, "WLHDD");
	strcpy(fwver, "0.1.0.1");

	if ((fp = fopen("/dev/mtd3", "rb"))!=NULL)
	{
		if (fseek(fp, 4, SEEK_SET)!=0) goto write_ver;
		fread(dataPtr, 1, 4, fp);
		imagelen = (unsigned long *)dataPtr;

		cprintf("image len %x\n", *imagelen);
		if (fseek(fp, *imagelen - 64, SEEK_SET)!=0) goto write_ver;
		cprintf("seek\n");
		if (!fread(verPtr, 1, 64, fp)) goto write_ver;
		cprintf("ver %x %x", verPtr[0], verPtr[1]);
		strncpy(productid, verPtr + 4, 12);
		productid[12] = 0;
		sprintf(fwver, "%d.%d.%d.%d", verPtr[0], verPtr[1], verPtr[2], verPtr[3]);

		cprintf("get fw ver: %s\n", productid);
		fclose(fp);
	}
write_ver:
	nvram_set_f("general.log", "productid", productid);
	nvram_set_f("general.log", "firmver", fwver);
}

#if 0	// Moved to data_array
int
get_nat_vserver_table(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char *nat_argv[] = {"iptables", "-t", "nat", "-nxL", NULL};
	char line[256], tmp[256];
	char target[16], proto[16];
	char src[sizeof("255.255.255.255")];
	char dst[sizeof("255.255.255.255")];
	char *range, *host, *port, *ptr, *val;
	int ret = 0;

	/* dump nat table including VSERVER and VUPNP chains */
	_eval(nat_argv, ">/tmp/vserver.log", 10, NULL);

	ret += websWrite(wp,
#ifdef NATSRC_SUPPORT
		"Source          "
#endif
		"Destination     Proto. Port range  Redirect to     Local port\n");
	/*	 255.255.255.255 other  65535:65535 255.255.255.255 65535:65535 */

	fp = fopen("/tmp/vserver.log", "r");
	if (fp == NULL)
		return ret;

	while (fgets(line, sizeof(line), fp) != NULL)
	{
		_dprintf("HTTPD: %s\n", line);

		tmp[0] = '\0';
		if (sscanf(line,
		    "%15s%*[ \t]"		// target
		    "%15s%*[ \t]"		// prot
		    "%*s%*[ \t]"		// opt
		    "%15[^/]/%*d%*[ \t]"	// source
		    "%15[^/]/%*d%*[ \t]"	// destination
		    "%255[^\n]",		// options
		    target, proto, src, dst, tmp) < 4) continue;

		/* TODO: add port trigger, portmap, etc support */
		if (strcmp(target, "DNAT") != 0)
			continue;

		/* uppercase proto */
		for (ptr = proto; *ptr; ptr++)
			*ptr = toupper(*ptr);
#ifdef NATSRC_SUPPORT
		/* parse source */
		if (strcmp(src, "0.0.0.0") == 0)
			strcpy(src, "ALL");
#endif
		/* parse destination */
		if (strcmp(dst, "0.0.0.0") == 0)
			strcpy(dst, "ALL");

		/* parse options */
		port = host = range = "";
		ptr = tmp;
		while ((val = strsep(&ptr, " ")) != NULL) {
			if (strncmp(val, "dpt:", 4) == 0)
				range = val + 4;
			else if (strncmp(val, "dpts:", 5) == 0)
				range = val + 5;
			else if (strncmp(val, "to:", 3) == 0) {
				port = host = val + 3;
				strsep(&port, ":");
			}
		}

		ret += websWrite(wp,
#ifdef NATSRC_SUPPORT
			"%-15s "
#endif
			"%-15s %-6s %-11s %-15s %-11s\n",
#ifdef NATSRC_SUPPORT
			src,
#endif
			dst, proto, range, host, port ? : range);
	}
	fclose(fp);
	unlink("/tmp/vserver.log");

	return ret;
}
#endif

/* remove space in the end of string */
char *trim_r(char *str)
{
	int i;

	i = strlen(str);

	while (i >= 1)
	{
		if (*(str+i-1) == ' ' || *(str+i-1) == 0x0a || *(str+i-1) == 0x0d)
			*(str+i-1)=0x0;
		else
			break;

		i--;
	}

	return (str);
}

int
ej_get_default_reboot_time(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;
	int reboot_time = nvram_get_int("reboot_time");

#if defined(RTCONFIG_QCA)
	reboot_time += 3 * get_nr_guest_network(-1);
#endif

#if defined(RTCONFIG_INTERNAL_GOBI)
	reboot_time += 25;
#endif

	retval += websWrite(wp, "%d", reboot_time);
	return retval;
}

int is_wlif_up(const char *ifname)
{
	struct ifreq ifr;
	int skfd;

	if (!ifname)
		return -1;

	skfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (skfd == -1)
	{
		perror("socket");
		return -1;
	}

	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	{
		perror("ioctl");
		close(skfd);
		return -1;
	}
	close(skfd);

	if (ifr.ifr_flags & IFF_UP)
		return 1;
	else
		return 0;
}

struct useful_redirect_list useful_redirect_lists[] = {
	{ "Main_**.asp", NULL },
	{"AdaptiveQoS_**.asp", NULL},
	{"Advanced_**.asp", NULL},
	{"aicloud_qis.asp", NULL},
	{"aidisk.asp", NULL},
	{"AiProtection_**.asp", NULL},
	{"APP_Installation.asp", NULL},
	{"cloud_main.asp", NULL},
	{"cloud_router_sync.asp", NULL},
	{"cloud_settings.asp", NULL},
	{"cloud_syslog.asp", NULL},
	{"cloud_sync.asp", NULL},
	{"Feedback_Info.asp", NULL},
	{"GameBoost.asp", NULL},
	{"Guest_network**.asp", NULL},
	{"index.asp", NULL},
	{"ParentalControl.asp", NULL},
	{"PrinterServer.asp", NULL},
	{"QIS_wizard**.htm", NULL},
	{"QoS_EZQoS.asp", NULL},
	{"TrafficAnalyzer_Statistic.asp", NULL},
	{"updateSubnet.htm", NULL},
#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
	{"send_IFTTTPincode.cgi", NULL},
#endif
#ifdef RTCONFIG_AMAS
	{"AiMesh_Node_FirmwareUpgrade.asp", NULL},
#endif
	{ NULL, NULL }
};

#ifdef RTCONFIG_AMAS
struct AiMesh_whitelist AiMesh_whitelists[] = {
	{"AiMesh_Node_FirmwareUpgrade.asp", NULL},
	{"upgrade.cgi", NULL},
	{"Updating.asp", NULL},
	{"UpdateError_reboot.asp", NULL},
	{"UpdateError.asp", NULL},
	{"message.htm", NULL},
	{"error_page.htm", NULL},
	{"Main_Login.asp", NULL},
	{ NULL, NULL }
};
#endif

struct log_pass_url_list log_pass_handlers[] = {
	{ "**.gz", NULL },
	{ "**.tgz", NULL },
	{ "**.zip", NULL },
	{ "**.ipk", NULL },
	{ "**.css", NULL },
	{ "**.png", NULL },
	{ "**.gif", NULL },
	{ "**.jpg", NULL },
	{ "**.svg", NULL },
	{ "**.swf", NULL  },
	{ "**.htc", NULL  },
	{ "fonts/*.ttf", NULL },
	{ "fonts/*.otf", NULL },
	{ "fonts/*.woff", NULL },
	{ "**.js", NULL },
	{ "**.xml", NULL },
	{ NULL, NULL }
};

void write_encoded_crt(char *name, char *value){
	int len, i, j;
	char tmp[3500];

	if (!value) return;

	len = strlen(value);
	if (len > (sizeof(tmp) - 1)) len = sizeof(tmp) - 1;

	i = 0;
	for (j=0; (j < len); j++) {
		if (value[j] == '\n') tmp[i++] = '>';
		else if (value[j] != '\r') tmp[i++] = value[j];
	}

	tmp[i] = '\0';
	nvram_set(name, tmp);
}


#if (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2))

#define JFFS_BACKUP_FILE "/tmp/backup_jffs.tar"

static void
do_jffs_file(char *url, FILE *stream)
{
        system("tar cf /tmp/backup_jffs.tar -C /jffs .");
        do_file(JFFS_BACKUP_FILE, stream);
        unlink(JFFS_BACKUP_FILE);
}

static void
do_jffsupload_cgi(char *url, FILE *stream)
{
	int ret;

#ifdef RTCONFIG_HTTPS
	if(do_ssl)
		ret = fcntl(ssl_stream_fd , F_GETOWN, 0);
	else
#endif
	ret = fcntl(fileno(stream), F_GETOWN, 0);

	if (ret == 0)
	{
		if (f_size(JFFS_BACKUP_FILE) > 10) {
			logmessage("httpd", "Restoring JFFS backup...");
			system("rm -rf /jffs/*"); /* */
			system("/bin/tar -xf /tmp/backup_jffs.tar -C /jffs");
			unlink(JFFS_BACKUP_FILE);
			logmessage("httpd", "JFFS restore completed");
			websApply(stream, "UploadingJFFS.asp");
#ifdef RTCONFIG_HTTPS
			if(do_ssl)
				shutdown(ssl_stream_fd, SHUT_RDWR);
			else
#endif
				shutdown(fileno(stream), SHUT_RDWR);
		} else {
			logmessage("httpd", "Error while restoring JFFS backup - no change made");
			unlink(JFFS_BACKUP_FILE);
		}
	}
	else
	{
		websApply(stream, "UploadError.asp");
		unlink(JFFS_BACKUP_FILE);
	}
}

static void
do_jffsupload_post(char *url, FILE *stream, int len, char *boundary)
{
	FILE *fifo = NULL;
	int count, ret = EINVAL, ch;
	long filelen = 0;

	/* Look for our part */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);

		if (!strncasecmp(post_buf, "Content-Disposition:", 20)
				&& strstr(post_buf, "name=\"file2\""))
			break;
	}

	/* Skip boundary and headers */
	while (len > 0) {
		if (!fgets(post_buf, MIN(len + 1, sizeof(post_buf)), stream)) {
			goto err;
		}

		len -= strlen(post_buf);
		if (!strcmp(post_buf, "\n") || !strcmp(post_buf, "\r\n")) {
			break;
		}
	}

	if (!(fifo = fopen(JFFS_BACKUP_FILE, "w")))
		goto err;

	while (len > 0) {
		count = fread(post_buf, 1, MIN(len, sizeof(post_buf)), stream);
		if(count <= 0)
			goto err;

		len -= count;
		filelen += count;
		fwrite(post_buf, 1, count, fifo);
	}

	ret = 0;
	fclose(fifo);
	fifo = NULL;

	/* Strip boundary from file */
	if (boundary)
		truncate(JFFS_BACKUP_FILE, filelen - strlen(boundary) - 8);

err:
	if (fifo)
		fclose(fifo);

	/* Slurp anything remaining in the request */
	while (len-- > 0)
		if((ch = fgetc(stream)) == EOF)
			break;

	fcntl(fileno(stream), F_SETOWN, -ret);
}
#endif // (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2)

#ifdef RTCONFIG_DNSPRIVACY
int
ej_get_dnsprivacy_presets(int eid, webs_t wp, int argc, char_t **argv)
{
	FILE *fp;
	char buf[256], *type, *datafile, *ptr, *item, *lsep, *fsep;
	int ret = 0;

	if (ejArgs(argc, argv, "%s", &type) < 1) {
		websError(wp, 400, "Insufficient args\n");
		return -1;
	}

	if (!strcmp(type, "dot"))
		datafile = "/rom/dot-servers.dat";
	else {
		websError(wp, 400, "Invalid argument\n");
		return -1;
	}

	if (!(fp = fopen(datafile, "r")))
		return 0;

	for (lsep = ""; (ptr = fgets(buf, sizeof(buf), fp)) != NULL;) {
		buf[sizeof(buf) - 1] = '\0';
		ptr = strsep(&ptr, "#\n");
		if (*ptr == '\0')
			continue;

		ret += websWrite(wp, "%s[", lsep);
		for (fsep = ""; (item = strsep(&ptr, ",")) != NULL;) {
			ret += websWrite(wp, "%s\"%s\"", fsep, item);
			fsep = ",";
		}
		ret += websWrite(wp, "]");
		lsep = ",";
	}
	fclose(fp);

	return ret;
}
#endif
