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
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
#include <rc.h>

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <shutils.h>
#include <stdarg.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>
#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif
#ifdef RTCONFIG_QCA
#include <qca.h>
#endif
#ifdef RTCONFIG_REALTEK
#include "../shared/sysdeps/realtek/realtek.h"
#endif
#include <shared.h>

#include <syslog.h>
#include <bcmnvram.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/sysinfo.h>
#ifdef RTCONFIG_USER_LOW_RSSI
#if defined(RTCONFIG_RALINK)
#include <typedefs.h>
#else
#include <wlioctl.h>
#include <wlutils.h>
#endif
#endif

#if defined(RTCONFIG_NOTIFICATION_CENTER)
#include <libnt.h>
#endif
#if defined(RTCONFIG_LP5523)
#include <lp5523led.h>
#endif

#ifdef RTCONFIG_CFGSYNC
#include <cfg_event.h>
#endif

#define BCM47XX_SOFTWARE_RESET	0x40		/* GPIO 6 */
#define RESET_WAIT		2		/* seconds */
#define RESET_WAIT_COUNT	RESET_WAIT * 10 /* 10 times a second */

#define TEST_PERIOD		100		/* second */
#define NORMAL_PERIOD		1		/* second */
#define URGENT_PERIOD		100 * 1000	/* microsecond */
#define RUSHURGENT_PERIOD	50 * 1000	/* microsecond */
#define DAY_PERIOD		2 * 60 * 24	/* 1 day (in 30 sec periods) */

#define WPS_TIMEOUT_COUNT	121 * 20
#ifdef RTCONFIG_WPS_LED
#define WPS_SUCCESS_COUNT	3
#endif
#define WPS_WAIT		1		/* seconds */
#define WPS_WAIT_COUNT		WPS_WAIT * 20	/* 20 times a second */
#ifdef BLUECAVE
static int bc_wps_led = 0;
#endif

#ifdef RTCONFIG_AMAS
#define AMESH_TIMEOUT_COUNT	30 * 20		/* 30 secnods */
#define ONBOARDING_TIMEOUT	120		/* 120 seconds */
#endif

#ifdef RTCONFIG_WPS_RST_BTN
#define WPS_RST_DO_WPS_COUNT		( 1*10)	/* 1 seconds */
#define WPS_RST_DO_RESTORE_COUNT	(10*10)	/* 10 seconds */
#undef RESET_WAIT_COUNT
#define RESET_WAIT_COUNT		WPS_RST_DO_RESTORE_COUNT
#endif	/* RTCONFIG_WPS_RST_BTN */

#ifdef RTCONFIG_WPS_ALLLED_BTN
#define WPS_LED_WAIT_COUNT		1
#endif

#if defined(RTCONFIG_EJUSB_BTN)
#define EJUSB_WAIT_COUNT	2		/* 2 seconds */
#endif

//#if defined(RTCONFIG_JFFS2LOG) && defined(RTCONFIG_JFFS2)
#if defined(RTCONFIG_JFFS2LOG) && (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
#define LOG_COMMIT_PERIOD	2		/* 2 x 30 seconds */
static int log_commit_count = 0;
#endif
#if defined(RTCONFIG_USB_MODEM)
#define LOG_MODEM_PERIOD	20		/* 10 minutes */
static int log_modem_count = 0;
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
#define MODEM_FLOW_PERIOD	1
static int modem_flow_count = 0;
static int modem_data_save = 0;
#endif
#endif
#if 0	//defined(RTCONFIG_TOR) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2))
#define TOR_CHECK_PERIOD	10		/* 10 x 30 seconds */
unsigned int tor_check_count = 0;
#endif

#ifdef RTCONFIG_REALTEK
#define REINIT_WEB_FILE		("/tmp/reinit_web")
#define PARAM_TEMP_FILE		("/tmp/flash_param")
#define PARAM_TEMP_FILE2	("/tmp/flash_param2")
#endif

static sigset_t sigs_to_catch;

static struct itimerval itv;
/* to check watchdog alive */
#if ! (defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
static struct itimerval itv02;
#endif
static int watchdog_period = 0;
#ifdef WATCHDOG_PERIOD2
static int watchdog_period2 = 0;
#endif
#ifdef RTCONFIG_BCMARM
static int chkusb3_period = 0;
static int u3_chk_life = 6;
#endif
static int btn_pressed = 0;
static int btn_count = 0;
#ifdef BTN_SETUP
static int btn_pressed_setup = 0;
static int btn_count_setup = 0;
static int wsc_timeout = 0;
static int btn_count_setup_second = 0;
static int btn_pressed_toggle_radio = 0;
#endif
static long ddns_update_timer = 0;

#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_DSL)
// for WLAN sw init, only for slide switch
static int wlan_sw_init = 0;
#elif defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_QCA)
static int wifi_sw_old = -1;
#endif
#ifdef RTCONFIG_LED_BTN
static int LED_status_old = -1;
static int LED_status = -1;
static int LED_status_changed = 0;
static int LED_status_first = 1;
static int LED_status_on = -1;
#ifdef RTAC87U
static int LED_switch_count = 0;
static int BTN_pressed_count = 0;
#endif
#endif

#if defined(RTCONFIG_WPS_ALLLED_BTN)
static int LED_status_old = -1;
static int LED_status = -1;
static int LED_status_changed = 0;
static int LED_status_on = -1;
static int BTN_pressed_count = 0;
#endif

#ifdef RTCONFIG_BCMWL6
static int wlonunit = -1;
#endif

extern int g_wsc_configured;

static unsigned int sigbones = 0;

void watch_sig(int signo) {
	sigbones |= 1<<signo;
}

static void *fn_acts[_NSIG];

#define REGULAR_DDNS_CHECK	10 //10x30 sec
static int ddns_check_count = 0;
static int freeze_duck_count = 0;

static const struct mfg_btn_s {
	enum btn_id id;
	char *name;
	char *nv;
} mfg_btn_table[] = {
#ifndef RTCONFIG_N56U_SR2
	{ BTN_RESET,	"RESET", 	"btn_rst" },
#endif
	{ BTN_WPS,	"WPS",		"btn_ez" },
#if defined(RTCONFIG_WIFI_TOG_BTN)
	{ BTN_WIFI_TOG,	"WIFI_TOG",	"btn_wifi_toggle" },
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	{ BTN_WIFI_SW,	"WIFI_SW",	"btn_wifi_sw" },
#endif
#if defined(RTCONFIG_EJUSB_BTN)
	{ BTN_EJUSB1,	"EJECT USB1",	"btn_ejusb_btn1" },
	{ BTN_EJUSB2,	"EJECT USB2",	"btn_ejusb_btn2" },
#endif

	{ BTN_ID_MAX,	NULL,		NULL },
};

/* ErP Test */
#ifdef RTCONFIG_ERP_TEST
#define MODE_NORMAL 0
#define MODE_PWRSAVE 1
#define NO_ASSOC_CHECK 6 // 6x30 sec
static int pwrsave_status = MODE_NORMAL;
static int no_assoc_check = 0;
#endif

void led_table_ctrl(int on_off);

#if defined(RTAC1200G) || defined(RTAC1200GP)
#define WDG_MONITOR_PERIOD 60 /* second */
static int wdg_timer_alive = 1;
#endif

#ifdef HND_ROUTER
const char *dw_aggled[]={"dw", "0x800c00b8", NULL};
const char *sw_aggled_alloff[]={"sw", "0x800c00b8", "0x40000", NULL};
const char *sw_aggled_allon[]={"sw", "0x800c00b8", "0x4008f", NULL};
const char *sw_aggled_extoff[]={"sw", "0x800c00b8", "0x4000f", NULL};
#ifdef RTCONFIG_BONDING
static int bs=-1, bs_pre=-1;
extern char *bs_desc[];
#endif
#endif

/* DEBUG DEFINE */
#define SCHED_DEBUG	"/tmp/SCHED_DEBUG"
#define WL_SCHED_DBG(fmt,args...) \
	if(f_exists(SCHED_DEBUG) > 0) { \
	printf("[SCHED][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

void watchdog(int sig);

int
elm_of_strr(const char *strr[])
{
	int num = 0;
	while (strr[num++]);
	return num-1;
}

void
sys_exit()
{
	printf("[watchdog] sys_exit");
	set_action(ACT_REBOOT);
	kill(1, SIGTERM);
}

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

/* to check watchdog alive */
#if ! (defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
static void
alarmtimer02(unsigned long sec, unsigned long usec)
{
	itv02.it_value.tv_sec = sec;
	itv02.it_value.tv_usec = usec;
	itv02.it_interval = itv02.it_value;
	setitimer(ITIMER_REAL, &itv02, NULL);
}
#endif

extern int no_need_to_start_wps();
extern void uptime_wait(int);

void led_control_normal(void)
{
#ifdef BLUECAVE
	if(nvram_match("bc_ledbh", "wps_done"))
		kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
	return;
#endif

#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
	return;
#endif
#ifdef RTAC87U
	LED_switch_count = nvram_get_int("LED_switch_count");
#endif

#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	if (!nvram_get_int("AllLED")) return;
#endif

#ifdef RTCONFIG_WPS_LED
	int v = LED_OFF;
	// the behavior in normal when wps led != power led
	// wps led = off, power led = on

	if (nvram_match("wps_success", "1"))
		v = LED_ON;
	__wps_led_control(v);
#endif

#if !defined(RTCONFIG_CONCURRENTREPEATER)
	led_control(LED_POWER, LED_ON);
#endif
#ifdef RTCONFIG_LOGO_LED
	led_control(LED_LOGO, LED_ON);
#endif

#if defined(RTN11P) || defined(RTN300) || defined(RTN11P_B1)
	led_control(LED_WPS, LED_ON);	//wps led is also 2g led. and NO power led.
#else
	if (nvram_get_int("led_pwr_gpio") != nvram_get_int("led_wps_gpio"))
		led_control(LED_WPS, LED_OFF);
#endif
}

#ifndef HND_ROUTER
void erase_nvram(void)
{
	switch (get_model()) {
		case MODEL_BLUECAVE:
		case MODEL_GTAC9600:
			eval("cp", "-f", "/sbin/nvram.txt", "/tmp/nvram.txt");
			eval("cp", "-f", "/sbin/nvram.txt", "/jffs/nvram.txt");
		default:
#ifdef RTCONFIG_BCMARM
			eval("mtd-erase2", "nvram");
#else
			eval("mtd-erase","-d","nvram");
#endif
	}
}
#endif

int init_toggle(void)
{
	switch (get_model()) {
#ifdef RTCONFIG_WIFI_TOG_BTN
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
		case MODEL_RTAC3200:
		case MODEL_RTAC68U:
		case MODEL_DSLAC68U:
		case MODEL_RTAC87U:
		case MODEL_RTAC5300:
		case MODEL_GTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC86U:
		case MODEL_RTAC3100:
			nvram_set("btn_ez_radiotoggle", "1");
			return BTN_WIFI_TOG;
#endif
		default:
			return BTN_WPS;
	}
}

#ifdef RTCONFIG_REALTEK
/* [MUST]: Need to re-coding this part */
typedef struct rtk_wps_result{
	int band;
	char ssid[256];
	char wsc_ssid[256];
	int auth_type;
	int encrypt;
	int wsc_auth;
	int wpa_auth;
	char wpa_psk[256];
	int psk_format;
	char wsc_psk[256];
	int wpa_suite;
	int wpa2_suite;
	int wep;
	int wsc_enc;
}rtk_wps_result;
//#define SET_WPS_RESULT_TO_ROOT
#ifdef RTCONFIG_CONCURRENTREPEATER
#define RTK_SCAN_LIST_2G "/tmp/rtk_ss_list_2g"
#define RTK_SCAN_LIST_5G "/tmp/rtk_ss_list_5g"
#define MAX_WAIT_COUNT ((15*1000*1000)/(RUSHURGENT_PERIOD))

static int wait_5g_time = 0;
static int wait_2g_time = 0;
static int wps_2g_done = 0;
static int wps_5g_done = 0;
static rtk_wps_result wps_res_2g,wps_res_5g;

struct save_fuple {
	int length;
	char *cmppart;
	char *setpart1;
	char *setpart2;
};
static int rtk_comparetmp( char *arraylist[], int sizelist, char ssidptr1[], char *ssidptr2) {
	int sizetmp = 0;
	char ssidcat[128];

	strcpy ( ssidcat, ssidptr1 );
	strcat ( ssidcat, ssidptr2 );
	while( sizetmp < sizelist) {
		if( !strcmp( arraylist[sizetmp], ssidcat ) ) {
			strcat ( ssidptr1, ssidptr2 );
			return 1;
		}
		sizetmp ++;
	}
	return 0;
}
static char *rtk_readfile(char *fname,int *fsize)
{
 FILE *fp;
 unsigned long size,lsize;
 char *pt;
 int len;
 char buf[100];

 size=0;
 pt=NULL;
 fp=fopen(fname,"r");
 if (!fp) return NULL;
 while (1)
  {
   len=fread(buf,1,100,fp);
   if (len==-1)
    goto sysfail;
   lsize=size;
   size+=len;
   pt=(char *)realloc(pt,size+1);
   if (len==0)
    {
     pt[size]='\0';
     break;
    }
   if (!pt)
    goto sysfail;
   memcpy(pt+lsize,buf,len);
  }
 fclose(fp);
 pt[size]='\0';
 *fsize=size;
 return pt;

sysfail:
 fclose(fp);
 if (pt)
  free(pt);
 return NULL;
}

static int rtk_auto_detect_ssid(int band_chk,char* ssid_buf, char* result)
{
	char file_name[128], substrl[128], strNULL[]="";
	char *getptr1, *getptr2, *substrr, *gettmp[128];
	int fsize, idlength=0, cmpresult=0;
	struct stat status;
	struct save_fuple *bandlist;
	struct save_fuple getSsidRule0[] = {
		{ 5	, "-2.4G"  , "-5G"	, ""  },
		{ 5	, "_2.4G"  , "_5G"	, ""  },
		{ 5	, ".2.4G"  , ".5G"	, ""  },
		{ 5	, " 2.4G"  , " 5G"	, ""  },
		{ 5	, "-2.4g"  , "-5g"	, ""  },
		{ 5	, "_2.4g"  , "_5g"	, ""  },
		{ 5	, ".2.4g"  , ".5g"      , ""  },
		{ 5	, " 2.4g"  , " 5g"      , ""  },
		{ 4     , "2.4G"   , "5G"       , ""  },
		{ 4	, "2.4g"   , "5g"  	, ""  },
		{ 4	, "-2.4"   , "-5"       , ""  },
		{ 4	, "_2.4"   , "_5"       , ""  },
		{ 4	, ".2.4"   , ".5"       , ""  },
		{ 4	, " 2.4"   , " 5"       , ""  },
		{ 3	, "2.4"    , "5"	, ""  },
		{ 3	, "-2G"    , "-5G"      , ""  },
		{ 3	, "_2G"    , "_5G"      , ""  },
		{ 3	, ".2G"    , ".5G"      , ""  },
		{ 3	, " 2G"    , " 5G"      , ""  },
		{ 3     , "-2g"    , "-5g"      , ""  },
		{ 0     , "_2g"    , "_5g"      , ""  },
		{ 3     , ".2g"    , ".5g"      , ""  },
		{ 3     , " 2g"    , " 5g"      , ""  },
		{ 2	, "2G"     , "5G"       , ""  },
		{ 2     , "2g"     , "5g"       , ""  },
		{ 2	, "-2"     , "-5"       , ""  },
		{ 2	, "_2"     , "_5"       , ""  },
		{ 2	, ".2"     , ".5"       , ""  },
		{ 2	, " 2"     , " 5"       , ""  },
		{ 1	, "2"      , "5"	, ""  },
		{ 0	, ""	   , "-5G"      , ""  },
		{ 0	, ""	   , "_5G"      , ""  },
		{ 0	, ""	   , ".5G"      , ""  },
		{ 0	, ""	   , " 5G"      , ""  },
		{ 0	, ""	   , "-5g"      , ""  },
		{ 0	, ""	   , "_5g"      , ""  },
		{ 0	, ""	   , ".5g"      , ""  },
		{ 0	, ""	   , " 5g"      , ""  },
		{ 0	, ""	   , "5G"       , ""  },
		{ 0	, ""	   , "5g"       , ""  },
		{ 0	, ""	   , "-5"       , ""  },
		{ 0	, ""	   , "_5"       , ""  },
		{ 0	, ""	   , ".5"       , ""  },
		{ 0	, ""	   , " 5"       , ""  },
		{ 0	, ""	   , "5"	, ""  },
		{ 0	, ""	   , ""		, ""  },
		{ 99	, ""	   , ""		, ""  }
	};
	struct save_fuple getSsidRule1[] = {
		{ 3	, "-5G"	, "-2G"	, "-2.4G"  },
		{ 3	, "_5G"	, "_2G"	, "_2.4G"  },
		{ 3	, ".5G"	, ".2G"	, ".2.4G"  },
		{ 3	, " 5G"	, " 2G"	, " 2.4G"  },
		{ 3	, "-5g"	, "-2g"	, "-2.4g"  },
		{ 3	, "_5g"	, "_2g"	, "_2.4g"  },
		{ 3	, ".5g"	, ".2g"	, ".2.4g"  },
		{ 3	, " 5g"	, " 2g"	, " 2.4g"  },
		{ 2	, "5G"	, "2G"	, "2.4G"   },
		{ 2	, "5g"	, "2g" 	, "2.4g"   },
		{ 2	, "-5"	, "-2"	, "-2.4"   },
		{ 2	, "_5"	, "_2"	, "_2.4"   },
		{ 2	, ".5"	, ".2"	, ".2.4"   },
		{ 2	, " 5"	, " 2"	, " 2.4"   },
		{ 1	, "5"	, "2" 	, "2.4"    },
		{ 0	, ""	, "-2G"	, "-2.4G"  },
		{ 0     , ""    , "_2G" , "_2.4G"  },
		{ 0     , ""    , ".2G" , ".2.4G"  },
		{ 0     , ""    , " 2G" , " 2.4G"  },
		{ 0     , ""    , "-2g" , "-2.4g"  },
		{ 0     , ""    , "_2g" , "_2.4g"  },
		{ 0     , ""    , ".2g" , ".2.4g"  },
		{ 0     , ""    , " 2g" , " 2.4g"  },
		{ 0     , ""    , "2G" 	, "2.4G"   },
		{ 0     , ""    , "2g"  , "2.4g"   },
		{ 0     , ""    , "-2"  , "-2.4"   },
		{ 0     , ""    , "_2"  , "_2.4"   },
		{ 0     , ""    , ".2"  , ".2.4"   },
		{ 0     , ""    , " 2"  , " 2.4"   },
		{ 0     , ""    , "2"  	, "2.4"    },
		{ 0     , ""    , ""  	, ""       },
		{ 99    , ""    , ""    , ""       }
	};

	//Get another band's SSID list
	if(band_chk == 0)
	{
		sprintf(file_name,RTK_SCAN_LIST_2G);
		while(!stat("/var/pbc_state1_cancel", &status) != 0); // file exists, wps not stopped
	}
	else if(band_chk == 1)
	{
		sprintf(file_name,RTK_SCAN_LIST_5G);
		while(!stat("/var/pbc_state2_cancel", &status) != 0); // file exists, wps not stopped
	}
	if( access( file_name, F_OK ) != -1 ) {
		getptr1 = rtk_readfile(file_name, &fsize);
		getptr2 = strstr(getptr1, "\n");
		while( getptr2 != NULL ) {
			*getptr2 = '\0';
			gettmp[idlength] = getptr1;
			getptr1 = getptr2 + 1;
			getptr2 = strstr(getptr1, "\n");
			idlength ++;
		}
	}

	if ( band_chk )
		bandlist = getSsidRule0;
	else
		bandlist = getSsidRule1;

	//compare the SSID with SCAN LIST
	while( bandlist->length != 99 ) {
		if ( strlen(ssid_buf) > bandlist->length )
			substrr = ssid_buf + strlen(ssid_buf) - bandlist->length;
		else {
			bandlist++;
			continue;
		}

		if( bandlist->length == 0 ) {
			strcpy( substrl, ssid_buf );
			if ( rtk_comparetmp( gettmp, idlength, substrl, bandlist->setpart1 ) ) {
				cmpresult=1;
				break ;
			}
			if ( strcmp(bandlist->setpart2, "")) {
				if ( rtk_comparetmp( gettmp, idlength, substrl, bandlist->setpart2 ) ) {
					cmpresult=1;
					break;
				}
			}
		}
		else if( !strcmp(substrr, bandlist->cmppart) ){
			strncpy( substrl, ssid_buf, strlen(ssid_buf)-bandlist->length );
			if( rtk_comparetmp( gettmp, idlength, substrl, bandlist->setpart1 ) ) {
				cmpresult=1;
				break;
			}
			if ( strcmp(bandlist->setpart2, "")) {
				if ( rtk_comparetmp( gettmp, idlength, substrl, bandlist->setpart2 ) ) {
					cmpresult=1;
					break;
				}
			}
			if ( rtk_comparetmp( gettmp, idlength, substrl, strNULL ) ) {
				cmpresult = 1;
				break;
			}
		}
		bandlist++;
	}

	if( cmpresult == 1 ) {
		TRACE_PT("=== Find the SSID : [ %s ]\n", substrl);
		strcpy(result,substrl);
	}
	else
		TRACE_PT("=== Can't find the SSID : [  ]\n");

	return cmpresult;
}
#endif

static char *rtk_get_token(char *data, char *token)
{
	char *ptr=data;
	int len=0, idx=0;

	while (*ptr && *ptr != '\n' ) {
		if (*ptr == '=') {
			if (len <= 1)
				return NULL;
			memcpy(token, data, len);

			/* delete ending space */
			for (idx=len-1; idx>=0; idx--) {
				if (token[idx] != ' ')
					break;
			}
			token[idx+1] = '\0';

			return ptr+1;
		}
		len++;
		ptr++;
	}
	return NULL;
}
static int rtk_get_value(char *data, char *value)
{
	char *ptr=data;
	int len=0, idx, i;

	while (*ptr && *ptr != '\n' && *ptr != '\r') {
		len++;
		ptr++;
	}

	/* delete leading space */
	idx = 0;
	while (len-idx > 0) {
		if (data[idx] != ' ')
			break;
		idx++;
	}
	len -= idx;

	/* delete bracing '"' */
	if (data[idx] == '"') {
		for (i=idx+len-1; i>idx; i--) {
			if (data[i] == '"') {
				idx++;
				len = i - idx;
			}
			break;
		}
	}

	if (len > 0) {
		memcpy(value, &data[idx], len);
		value[len] = '\0';
	}
	return len;
}

int rtk_get_wps_result(char* result_file,rtk_wps_result* res)
{
	FILE* fp;
	char buffer[256] = {'\0'};
	char value[100],token[40];
	char* ptr;
	if(result_file == NULL || res == NULL)
	{
		return -1;
	}
	fp = fopen(result_file,"r");
	if(fp != NULL)
	{
		while(fgets(buffer,sizeof(buffer),fp))
		{
			ptr = rtk_get_token(buffer, token);
			if (ptr == NULL)
				continue;
			if (rtk_get_value(ptr, value)==0)
				continue;
			if(strncmp(token,"INTERFACE",strlen("INTERFACE")) == 0)
			{
				if (!strcmp(value, "wl0") || !strcmp(value, "wl0-vxd"))
					res->band = 0;
				else
					res->band = 1;

				TRACE_PT("band=%d\n",res->band);
				continue;
			}
			if(strncmp(token,"SSID",strlen("SSID")) == 0)
			{
				strcpy(res->ssid,value);
				TRACE_PT("SSID=%s\n",res->ssid);
				continue;
			}
			if(strncmp(token,"WSC_SSID",strlen("WSC_SSID")) == 0)
			{
				strcpy(res->wsc_ssid,value);
				TRACE_PT("WSC_SSID=%s\n",res->wsc_ssid);
				continue;
			}
			if(strncmp(token,"AUTH_TYPE",strlen("AUTH_TYPE")) == 0)
			{
				res->auth_type = atoi(value);
				TRACE_PT("auth_type:%d\n",res->auth_type);
				continue;
			}
			if(strncmp(token,"ENCRYPT",strlen("ENCRYPT")) == 0)
			{
				res->encrypt = atoi(value);
				TRACE_PT("ENCRYPT:%d\n",res->encrypt);
				continue;
			}
			if(strncmp(token,"WSC_AUTH",strlen("WSC_AUTH")) == 0)
			{
				res->wsc_auth = atoi(value);
				TRACE_PT("WSC_AUTH:%d\n",res->wsc_auth);
				continue;
			}
			if(strncmp(token,"WPA_AUTH",strlen("WPA_AUTH")) == 0)
			{
				res->wpa_auth = atoi(value);
				TRACE_PT("WPA_AUTH:%d\n",res->wpa_auth);
				continue;
			}
			if(strncmp(token,"WPA_PSK",strlen("WPA_PSK")) == 0)
			{
				strcpy(res->wpa_psk,value);
				TRACE_PT("WPA_PSK:%s\n",res->wpa_psk);
				continue;
			}
			if(strncmp(token,"PSK_FORMAT",strlen("PSK_FORMAT")) == 0)
			{
				res->psk_format = atoi(value);
				TRACE_PT("PSK_FORMAT:%d\n",res->psk_format);
				continue;
			}
			if(strncmp(token,"WSC_PSK",strlen("WSC_PSK")) == 0)
			{
				strcpy(res->wsc_psk,value);
				TRACE_PT("WSC_PSK:%s\n",res->wsc_psk);
				continue;
			}
			if(strncmp(token,"WPA_CIPHER_SUITE",strlen("WPA_CIPHER_SUITE")) == 0)
			{
				res->wpa_suite = atoi(value);
				TRACE_PT("WPA_CIPHER_SUITE:%d\n",res->wpa_suite);
				continue;
			}
			if(strncmp(token,"WPA2_CIPHER_SUITE",strlen("WPA2_CIPHER_SUITE")) == 0)
			{
				res->wpa2_suite = atoi(value);
				TRACE_PT("WPA2_CIPHER_SUITE:%d\n",res->wpa2_suite);
				continue;
			}
			if(strncmp(token,"WEP",strlen("WEP")) == 0)
			{
				res->wep = atoi(value);
				TRACE_PT("wep:%d\n",res->wep);
				continue;
			}
			if(strncmp(token,"WSC_ENC",strlen("WSC_ENC")) == 0)
			{
				res->wsc_enc = atoi(value);
				TRACE_PT("WSC_ENC:%d\n",res->wsc_enc);
				continue;
			}
		}
		fclose(fp);
	}
	return 0;
}

/* mode=0: Both get WiFi profile. mode=1: Only one band get WiFi profile. */
int set_wps_result_to_ap_nvram(int mode, int src_unit, int dst_unit)
{
	char prefix[]="wlXXXXXX_";
	char prefix_1[]="wlXXXXX.1";
	char wlc_prefix[]="wlcXXXXX_";
	char tmp[32] = {0}, buf[128] = {0};
	int ncount;
#ifdef RTCONFIG_CONCURRENTREPEATER
	snprintf(prefix, sizeof(prefix), "wl%d_", dst_unit);
	snprintf(prefix_1, sizeof(prefix_1), "wl%d.1_", dst_unit);
	snprintf(wlc_prefix, sizeof(wlc_prefix), "wlc%d_", src_unit);
#else
	strcpy(prefix, "wl_");
	strcpy(wlc_prefix, "wlc_");
#endif

	/* Set SSID */
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "ssid", tmp)));
	ncount = sizeof(buf) - strlen(buf) - 1;
	if (src_unit == 0 && dst_unit == 0) // 2.4G to 2.4G
		strncat(buf, "_RPT", ncount);
	else if (src_unit == 0 && dst_unit == 1) // 2.4G to 5G
		strncat(buf, "_RPT5G", ncount);
	else if (src_unit == 1 && dst_unit == 0) // 5G to 2.4G
		strncat(buf, "_RPT", ncount);
	else if (src_unit == 1 && dst_unit == 1 && mode == 0) // 5G to 5G
		strncat(buf, "_RPT", ncount);
	else if (src_unit == 1 && dst_unit == 1 && mode != 0) // 5G to 5G
		strncat(buf, "_RPT5G", ncount);

	nvram_set(strcat_r(prefix, "ssid", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "ssid", tmp), buf);
#endif

	/* Set WPS PSK */
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "wpa_psk", tmp)));
	nvram_set(strcat_r(prefix, "wpa_psk", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "wpa_psk", tmp), buf);
#endif

	/* Set auth node*/
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "auth_mode", tmp)));
	nvram_set(strcat_r(prefix, "auth_mode_x", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "auth_mode_x", tmp), buf);
#endif

	/* Set crypto */
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "crypto", tmp)));
	nvram_set(strcat_r(prefix, "crypto", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "crypto", tmp), buf);
#endif

	return 0;
}

int rtk_set_wps_result_to_nvram(rtk_wps_result res)
{
	int sw_mode = -1;
	int wps_band = -1;
	char temp_str[64] = {'\0'};
	char auth_mode_str[32] = {'\0'};
	char crypto_str[32] = {'\0'};

	char wlc_auth_mode[64];
	char wlc_crypto[64];
	sw_mode = sw_mode();
	if(sw_mode == SW_MODE_REPEATER || (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1)) /* repeater mode & media bridge */
	{
		if(nvram_match("x_Setting","0"))
		{
			nvram_set("x_Setting","1");
		}
#ifdef RTCONFIG_CONCURRENTREPEATER
		wps_band = res.band;

		sprintf(wlc_auth_mode,"wlc%d_auth_mode",wps_band);
		sprintf(wlc_crypto,"wlc%d_crypto",wps_band);

		sprintf(temp_str,"wlc%d_ssid",wps_band);
		nvram_set(temp_str,res.wsc_ssid);

		sprintf(temp_str,"wlc%d_wpa_psk",wps_band);
		nvram_set(temp_str,res.wpa_psk);
#else
		strcpy(wlc_auth_mode,"wlc_auth_mode");
		strcpy(wlc_crypto,"wlc_crypto");

		nvram_set("wlc_ssid",res.wsc_ssid);
		nvram_set("wlc_wpa_psk",res.wpa_psk);
#endif
#ifdef SET_WPS_RESULT_TO_ROOT
		sprintf(temp_str,"wl%d_wpa_psk",wps_band);
		nvram_set(temp_str,res.wsc_psk);
		sprintf(temp_str,"wl%d.1_wpa_psk",wps_band);
		nvram_set(temp_str,res.wsc_psk);
		sprintf(temp_str,"%d",res.wep);
		nvram_set("wlc_wep",temp_str);
#endif
#ifdef SET_WPS_RESULT_TO_ROOT
		sprintf(auth_mode_str,"wl%d_auth_mode_x",wps_band);
		sprintf(crypto_str,"wl%d_crypto",wps_band);
#endif
		switch(res.wsc_auth)/*set authentication nvram*/
		{
			case 1:/*open*/
			{
//				nvram_set("wsc_auth","open");
//				nvram_set("wsc_crypto","none");
				nvram_set(wlc_auth_mode,"open");
				nvram_set(wlc_crypto,"none");
#ifdef SET_WPS_RESULT_TO_ROOT
				nvram_set(auth_mode_str,"open");
				nvram_set(crypto_str,"none");
#endif
				break;
			}
			case 2:
			{
				if(res.wsc_enc == 8)/*wpa:aes*/
				{
					nvram_set(wlc_auth_mode,"pskpsk2");
					nvram_set(wlc_crypto,"aes");
#ifdef SET_WPS_RESULT_TO_ROOT
					nvram_set(auth_mode_str,"pskpsk2");
					nvram_set(crypto_str,"aes");
#endif
				}
				break;
			}
			case 32:
			{
				if(res.wsc_enc == 8)/*wpa2:aes*/
				{
					nvram_set(wlc_auth_mode,"psk2");
					nvram_set(wlc_crypto,"aes");
#ifdef SET_WPS_RESULT_TO_ROOT
					nvram_set(auth_mode_str,"psk2");
					nvram_set(crypto_str,"aes");
#endif
				}
				break;
			}
			case 34:
			{
				if(res.wsc_enc == 12)/*wpa:tkip+aes*/
				{
					nvram_set(wlc_auth_mode,"pskpsk2");
					nvram_set(wlc_crypto,"tkip+aes");
#ifdef SET_WPS_RESULT_TO_ROOT
					nvram_set(auth_mode_str,"pskpsk2");
					nvram_set(crypto_str,"tkip+aes");
#endif
				}
				break;
			}
			default:
				break;
		}

	}
	else/*not repeater mode*/
	{
		wps_band = res.band;
		sprintf(auth_mode_str,"wl%d_auth_mode_x",wps_band);
		sprintf(crypto_str,"wl%d_crypto",wps_band);

		sprintf(temp_str,"wl%d_ssid",wps_band);
		nvram_set(temp_str,res.ssid);
		sprintf(temp_str,"wl%d_wpa_psk",wps_band);
		nvram_set(temp_str,res.wpa_psk);
		switch(res.wsc_auth)/*set authentication nvram*/
		{
			case 1:/*open*/
			{
				nvram_set(auth_mode_str,"open");
				nvram_set(crypto_str,"none");
				break;
			}
			case 2:
			{
				if(res.wsc_enc == 8)/*wpa:aes*/
				{
					nvram_set(auth_mode_str,"pskpsk2");
					nvram_set(crypto_str,"aes");
				}
				break;
			}
			case 32:
			{
				if(res.wsc_enc == 8)/*wpa2:aes*/
				{
					nvram_set(auth_mode_str,"psk2");
					nvram_set(crypto_str,"aes");
				}
				break;
			}
			case 34:
			{
				if(res.wsc_enc == 12)/*wpa:tkip+aes*/
				{
					nvram_set(auth_mode_str,"pskpsk2");
					nvram_set(crypto_str,"tkip+aes");
				}
				break;
			}
			default:
				break;
		}
	}
	nvram_commit();
	return 0;
}

#define WSCD_CONFIG_STATUS "/tmp/wscd_status"

enum {
	NOT_USED = -1,
	PROTOCOL_START = 0,
	PROTOCOL_SUCCESS = 3
};

static void wps_processing_check(void)
{
	FILE *fp = NULL;
	int status = -1;
	static int skip_flag = 0;

	fp = fopen(WSCD_CONFIG_STATUS, "r");
	if (fp != NULL) {

		fscanf(fp, "%d", &status);
		if (status != NOT_USED || status != PROTOCOL_START || status != PROTOCOL_SUCCESS) {

			if (!skip_flag) {
#ifdef RPAC68U
				set_led(LED_BLINK_QUICK, LED_BLINK_QUICK);
#else
				nvram_set_int("led_status", LED_WPS_PROCESSING);
#endif
				skip_flag = 1;
			}
		}
		else {
			skip_flag = 0;
		}
		fclose(fp);
	}
}



static int wps_process_finish()
{
	FILE *fp = NULL;
	int status = -1;

	fp = fopen(WSCD_CONFIG_STATUS, "r");
	if (fp != NULL) {

		fscanf(fp, "%d", &status);
		fclose(fp);
		if (status == PROTOCOL_SUCCESS)
			return 1;
	}
	return 0;
}

void rtk_wl_led(void)
{
	int sw_mode = sw_mode();
	if (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") != 1) // AP mode
		return;
	if (!is_wps_stopped())
		return;
	if (btn_pressed == 2) // Ready process button event.
		return;
	if (nvram_get_int("restore_defaults") == 1)
		return;
	if (nvram_invmatch("Ate_power_on_off_enable", "0"))	/* avoid run in test to let all led off */
		return;

	static int p_wlc0_state = -99;
	static int p_wlc1_state = -99;

	int wlc0_led = 0, wlc1_led = 0;
	int wlc0_state = 0, wlc1_state = 0;
	int wlc_express = nvram_get_int("wlc_express");

	wlc0_state = nvram_get_int("wlc0_state");
	if (wlc0_state == WLC_STATE_CONNECTED) {
		if (sw_mode == SW_MODE_REPEATER && wlc_express == 0)
			wlc0_led = LED_SHOW_SIG_STR;
		else
			wlc0_led = LED_SHOW_SIG_STR2;
	}
	else
		wlc0_led = LED_OFF_ALL;

	wlc1_state = nvram_get_int("wlc1_state");
	if (wlc1_state == WLC_STATE_CONNECTED) {
		if (sw_mode == SW_MODE_REPEATER && wlc_express == 0)
			wlc1_led = LED_SHOW_SIG_STR;
		else
			wlc1_led = LED_SHOW_SIG_STR2;
	}
	else
		wlc1_led = LED_OFF_ALL;

	if (p_wlc0_state != wlc0_state || p_wlc1_state != wlc1_state) {
		set_led(wlc0_led, wlc1_led);
		p_wlc0_state = wlc0_state;
		p_wlc1_state = wlc1_state;
	}
	else if (wlc0_led == LED_SHOW_SIG_STR || wlc1_led == LED_SHOW_SIG_STR ||
		 wlc0_led == LED_SHOW_SIG_STR2 || wlc1_led == LED_SHOW_SIG_STR2)
		set_led(wlc0_led, wlc1_led);
}

void rtk_wps_state_check(void)
{
	struct stat status;
	int sw_mode = 0;
	int need_restart = 0;
	rtk_wps_result wps_result;
	int detect_result = 0;
#ifndef RTCONFIG_CONCURRENTREPEATER
	int wlc_band = -1;
	char res_file[64] = {0};
#endif
	sw_mode = sw_mode();
#ifdef RTCONFIG_CONCURRENTREPEATER
	short int wlc_express = nvram_get_int("wlc_express");
#endif
	if(stat(REINIT_WEB_FILE, &status) != 0) // file does not exist
	{
		if(sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 0) { // AP mode.
			if (nvram_get_int("w_Setting") == 1) { // CONFIGURED
				if (wps_process_finish()) { // Special case!! Only for wscd process.
					stop_wps_method();
					return;
				}
			}
		}
		return;
	}
	if(sw_mode == SW_MODE_REPEATER || (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1))/*repeater mode*/ /* media bridge */
	{
		wps_processing_check();
#ifdef RTCONFIG_CONCURRENTREPEATER
		if(stat(PARAM_TEMP_FILE,&status) == 0)/*get wl0-vxd result*/
		{
			if(rtk_get_wps_result(PARAM_TEMP_FILE,&wps_res_2g) == 0)
			{
				rtk_set_wps_result_to_nvram(wps_res_2g);
				unlink(PARAM_TEMP_FILE);
				wps_2g_done = 1;
			}
		}
		if(stat(PARAM_TEMP_FILE2,&status) == 0)/*get wl1-vxd result*/
		{
			if(rtk_get_wps_result(PARAM_TEMP_FILE2,&wps_res_5g) == 0)
			{
				rtk_set_wps_result_to_nvram(wps_res_5g);
				unlink(PARAM_TEMP_FILE2);
				wps_5g_done = 1;
			}
		}
		if(wps_2g_done)
		{
			if (wlc_express == 0 || (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1)) {
				if(wps_5g_done || wait_5g_time >= MAX_WAIT_COUNT)
				{
					if(!wps_5g_done)
					{
						TRACE_PT("5g wps not done, guess the ssid...\n");
						memcpy(&wps_res_5g,&wps_res_2g,sizeof(rtk_wps_result));
						wps_res_5g.band = 1;
						stop_wps_method();
						if(rtk_auto_detect_ssid(1,wps_res_2g.wsc_ssid,wps_res_5g.wsc_ssid))
						{
							rtk_set_wps_result_to_nvram(wps_res_5g);
							detect_result = 1;
						}
					}

					if (wps_5g_done) { // Both
						set_wps_result_to_ap_nvram(0, 1, 1);
						set_wps_result_to_ap_nvram(0, 0, 0);
					}
					else if (!wps_5g_done && detect_result) { // Both
						set_wps_result_to_ap_nvram(0, 1, 1);
						set_wps_result_to_ap_nvram(0, 0, 0);
					}
					else { // Only 2.4G
						set_wps_result_to_ap_nvram(1, 0, 1);
						set_wps_result_to_ap_nvram(1, 0, 0);
					}

					unlink(REINIT_WEB_FILE);
					TRACE_PT("2.4g done. WPS need to reinit wireless.\n");
					notify_rc("restart_wireless");
#ifdef RPAC68U
					set_led(LED_BLINK_NORNAL, LED_BLINK_NORNAL);
#else
					nvram_set_int("led_status", LED_WPS_RESTART_WL);
#endif
				}
				else
				{
					wait_5g_time ++;
				}
			}
			else {
				set_wps_result_to_ap_nvram(1, 0, 1);
				unlink(REINIT_WEB_FILE);
				TRACE_PT("2.4g done. WPS need to reinit wireless.\n");
				notify_rc("restart_wireless");
#ifdef RPAC68U
				set_led(LED_BLINK_NORNAL, LED_BLINK_NORNAL);
#else
				nvram_set_int("led_status", LED_WPS_RESTART_WL);
#endif
			}
		}
		if(wps_5g_done)
		{
			if (wlc_express == 0 || (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1)) {
				if(wps_2g_done || wait_2g_time >= MAX_WAIT_COUNT)
				{
					if(!wps_2g_done)
					{
						TRACE_PT("2.4g wps not done, guess the ssid...\n");
						memcpy(&wps_res_2g,&wps_res_5g,sizeof(rtk_wps_result));
						wps_res_2g.band = 0;
						stop_wps_method();
						if(rtk_auto_detect_ssid(0,wps_res_5g.wsc_ssid,wps_res_2g.wsc_ssid))
						{
							rtk_set_wps_result_to_nvram(wps_res_2g);
							detect_result = 1;
						}
					}

					if (wps_2g_done) { // Both
						set_wps_result_to_ap_nvram(0, 1, 1);
						set_wps_result_to_ap_nvram(0, 0, 0);
					}
					else if (!wps_2g_done && detect_result) { // Both
						set_wps_result_to_ap_nvram(0, 1, 1);
						set_wps_result_to_ap_nvram(0, 0, 0);
					}
					else { // Only 5G
						set_wps_result_to_ap_nvram(1, 1, 1);
						set_wps_result_to_ap_nvram(1, 1, 0);
					}

					unlink(REINIT_WEB_FILE);
					TRACE_PT("5g done. WPS need to reinit wireless.\n");
					notify_rc("restart_wireless");
#ifdef RPAC68U
					set_led(LED_BLINK_NORNAL, LED_BLINK_NORNAL);
#else
					nvram_set_int("led_status", LED_WPS_RESTART_WL);
#endif
				}
				else
				{
					wait_2g_time ++;
				}
			}
			else {
				set_wps_result_to_ap_nvram(1, 1, 0);
				unlink(REINIT_WEB_FILE);
				TRACE_PT("5g done. WPS need to reinit wireless.\n");
				notify_rc("restart_wireless");
#ifdef RPAC68U
				set_led(LED_BLINK_NORNAL, LED_BLINK_NORNAL);
#else
				nvram_set_int("led_status", LED_WPS_RESTART_WL);
#endif
			}
		}
#else
		wlc_band = nvram_get_int("wlc_band");
		if(wlc_band == 0)
		{
			strcpy(res_file,PARAM_TEMP_FILE);
		}
		else if(wlc_band == 1)
		{
			strcpy(res_file,PARAM_TEMP_FILE2);
		}
		if(rtk_get_wps_result(res_file,&wps_result) == 0)
		{
			rtk_set_wps_result_to_nvram(wps_result);
			unlink(res_file);
			unlink(REINIT_WEB_FILE);
			TRACE_PT("Done. WPS need to reinit wireless.\n");
			notify_rc("restart_wireless");
#ifdef RPAC68U
			set_led(LED_BLINK_NORNAL, LED_BLINK_NORNAL);
#else
			nvram_set_int("led_status", LED_WPS_RESTART_WL);
#endif
		}
#endif
	}
	else /*AP mode*/
	{
		if(stat(PARAM_TEMP_FILE,&status) == 0)/*get wps result*/
		{
			if(rtk_get_wps_result(PARAM_TEMP_FILE,&wps_result) == 0)
			{
				rtk_set_wps_result_to_nvram(wps_result);
				if(nvram_get_int("w_Setting") == 0)
				{
					need_restart = 1;
					nvram_set("w_Setting","1");
					nvram_commit();
					TRACE_PT("WPS need to reinit wireless.\n");
					stop_wps_method();
					notify_rc("restart_wireless");
				}
			}
			unlink(REINIT_WEB_FILE);
			unlink(PARAM_TEMP_FILE);
		}
	}
}
#else /* #endif RTCONFIG_REALTEK */

#ifdef RTCONFIG_CONCURRENTREPEATER

#define MAX_WAIT_COUNT ((15*1000*1000)/(RUSHURGENT_PERIOD))

static int wait_5g_time = 0;
static int wait_2g_time = 0;
static int wps_5g_done = 0;
static int wps_2g_done = 0;

static int qca_comparetmp( char *arraylist[], int sizelist, char ssidptr1[], char *ssidptr2) {
	int sizetmp = 0;
	char ssidcat[128];

	strcpy ( ssidcat, ssidptr1 );
	strcat ( ssidcat, ssidptr2 );
	while( sizetmp < sizelist) {
		if( !strcmp( arraylist[sizetmp], ssidcat ) ) {
			strcat ( ssidptr1, ssidptr2 );
			return 1;
		}
		sizetmp ++;
	}
	return 0;
}

/* mode=0: Both get WiFi profile. mode=1: Only one band get WiFi profile. */
int set_wps_result_to_ap_nvram(int mode, int src_unit, int dst_unit)
{
	char prefix[]="wlXXXXXX_";
	char prefix_1[]="wlXXXXX.1";
	char wlc_prefix[]="wlcXXXXX_";
	char tmp[32] = {0}, buf[128] = {0};
	int ncount;
#ifdef RTCONFIG_CONCURRENTREPEATER
	snprintf(prefix, sizeof(prefix), "wl%d_", dst_unit);
	snprintf(prefix_1, sizeof(prefix_1), "wl%d.1_", dst_unit);
	snprintf(wlc_prefix, sizeof(wlc_prefix), "wlc%d_", src_unit);
#else
	strcpy(prefix, "wl_");
	strcpy(wlc_prefix, "wlc_");
#endif

	/* Set SSID */
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "ssid", tmp)));
	ncount = sizeof(buf) - strlen(buf) - 1;
	if (src_unit == 0 && dst_unit == 0) // 2.4G to 2.4G
		strncat(buf, "_RPT", ncount);
	else if (src_unit == 0 && dst_unit == 1) // 2.4G to 5G
		strncat(buf, "_RPT5G", ncount);
	else if (src_unit == 1 && dst_unit == 0) // 5G to 2.4G
		strncat(buf, "_RPT", ncount);
	else if (src_unit == 1 && dst_unit == 1 && mode == 0) // 5G to 5G
		strncat(buf, "_RPT", ncount);
	else if (src_unit == 1 && dst_unit == 1 && mode != 0) // 5G to 5G
		strncat(buf, "_RPT5G", ncount);

	nvram_set(strcat_r(prefix, "ssid", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "ssid", tmp), buf);
#endif

	/* Set WPS PSK */
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "wpa_psk", tmp)));
	nvram_set(strcat_r(prefix, "wpa_psk", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "wpa_psk", tmp), buf);
#endif

	/* Set auth node*/
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "auth_mode", tmp)));
	nvram_set(strcat_r(prefix, "auth_mode_x", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "auth_mode_x", tmp), buf);
#endif

	/* Set crypto */
	strcpy(buf, nvram_safe_get(strcat_r(wlc_prefix, "crypto", tmp)));
	nvram_set(strcat_r(prefix, "crypto", tmp), buf);
#ifdef RTCONFIG_CONCURRENTREPEATER
	nvram_set(strcat_r(prefix_1, "crypto", tmp), buf);
#endif

	return 0;
}

int qca_set_wps_result(int config_index, int band)
{
	FILE *fp = NULL;
	FILE *pfp = NULL;
	char cmd[64] = {0}, tmp_str[64] = {0}, tmp[32] = {0}, result_file[64] = {0};
	char *buf = NULL;

	int id = 0;

	struct wlc_setting {

		char ssid[32];
		char key_mgmt[16];
		char auth_alg[16];
		char proto[16];
		char pairwise[16];
		char group[16];
		char psk[32];
		char wep_tx_keyidx[4];
		char wep_key[32];

	} *wlc_s;

	wlc_s = malloc(sizeof(struct wlc_setting));
	memset(wlc_s, 0, sizeof(wlc_s));

	buf = malloc(sizeof(char) * 128);
	memset(buf, 0, 128);

	/* Get current station ID */
	sprintf(cmd, "wpa_cli -p /var/run/wpa_supplicant-sta%d -i sta%d list_networks", config_index, config_index);
	pfp = popen(cmd, "r");

	if (pfp != NULL) {
		while(fgets(buf, 128, pfp) != NULL) {
			if (strstr(buf, "CURRENT")) {
				sscanf(buf, "%d", &id);
			}
		}
	}
	pclose(pfp);

	sleep(3); // Wait update config file.
	sprintf(result_file, "/etc/Wireless/conf/wpa_supplicant-sta%d.conf", config_index);
	fp = fopen(result_file, "r");

	if (fp != NULL) {
		int network_index = 0;
		int found = 0;
		while (fgets(buf, 128, fp) != NULL) {

			if (strstr(buf, "network={")) {
				if (network_index == id) {
					found = 1;
				}
				else {
					network_index++;
					continue;
				}
			}

			if (found && !strstr(buf, "}")) {

				if (strstr(buf, "ssid")) {
					sscanf(buf, "%*[^\"]\"%[^\"]", tmp);
					strcpy(wlc_s->ssid, tmp);
				}
				else if (strstr(buf, "key_mgmt")) {
					sscanf(buf, "%*[^=]=%s", tmp);
					strcpy(wlc_s->key_mgmt, tmp);
				}
				else if (strstr(buf, "auth_alg")) {
					sscanf(buf, "%*[^=]=%s", tmp);
					strcpy(wlc_s->auth_alg, tmp);
				}
				else if (strstr(buf, "proto")) {
					sscanf(buf, "%*[^=]=%s", tmp);
					strcpy(wlc_s->proto, tmp);
				}
				else if (strstr(buf, "pairwise")) {
					sscanf(buf, "%*[^=]=%s", tmp);
					strcpy(wlc_s->pairwise, tmp);
				}
				else if (strstr(buf, "group")) {
					sscanf(buf, "%*[^=]=%s", tmp);
					strcpy(wlc_s->group, tmp);
				}
				else if (strstr(buf, "psk")) {
					sscanf(buf, "%*[^\"]\"%[^\"]", tmp);
					strcpy(wlc_s->psk, tmp);
				}
				else if (strstr(buf, "wep_tx_keyidx")) {
					sscanf(buf, "%*[^=]=%s", tmp);
					strcpy(wlc_s->wep_tx_keyidx, tmp);
				}
				else if (strstr(buf, "wep_key")) {
					sscanf(buf, "%*[^\"]\"%[^\"]", tmp);
					strcpy(wlc_s->wep_key, tmp);
				}
			}
			else if (found && strstr(buf, "}"))
				break;
		}
	}

	fclose(fp);
	free(buf);

	/* Set to nvram */
	/* ssid */
	if (strlen(wlc_s->ssid)) {
		if (config_index == band) {
			sprintf(tmp_str, "wlc%d_ssid", band);
			nvram_set(tmp_str, wlc_s->ssid);
		}
		else // Copy mode. So skip process SSID.
			;
	}
	else {
		free(wlc_s);
		return -1;
	}

	/* auth_mode */
	/* open, shared, psk, psk2, */
	int wep_flag = 0;
	if (strlen(wlc_s->key_mgmt)) {
		if (!strcmp(wlc_s->key_mgmt, "NONE")) {

			if (!strcmp(wlc_s->auth_alg, "OPEN")) {
				wep_flag = 1;
				sprintf(tmp_str, "wlc%d_auth_mode", band);
				nvram_set(tmp_str, "open");
				sprintf(tmp_str, "wlc%d_wep", band);
				nvram_set(tmp_str, "1");
			}
			else if (!strcmp(wlc_s->auth_alg, "SHARED")) {
				wep_flag = 1;
				sprintf(tmp_str, "wlc%d_auth_mode", band);
				nvram_set(tmp_str, "shared");
			}
			else {
				sprintf(tmp_str, "wlc%d_auth_mode", band);
				nvram_set(tmp_str, "open");
				sprintf(tmp_str, "wlc%d_wep", band);
				nvram_set(tmp_str, "0");
			}
		}
		else if (!strcmp(wlc_s->key_mgmt, "WPA-PSK")) {
			if (!strcmp(wlc_s->proto, "WPA")) {
				sprintf(tmp_str, "wlc%d_auth_mode", band);
				nvram_set(tmp_str, "psk");
			}
			else if (!strcmp(wlc_s->proto, "RSN")) {
				sprintf(tmp_str, "wlc%d_auth_mode", band);
				nvram_set(tmp_str, "psk2");
			}

			if (!strcmp(wlc_s->pairwise, "TKIP")) {
				sprintf(tmp_str, "wlc%d_crypto", band);
				nvram_set(tmp_str, "tkip");
			}
			else if (!strcmp(wlc_s->pairwise, "CCMP TKIP") || !strcmp(wlc_s->pairwise, "CCMP")) {
				sprintf(tmp_str, "wlc%d_crypto", band);
				nvram_set(tmp_str, "aes");
			}

			sprintf(tmp_str, "wlc%d_wpa_psk", band);
			nvram_set(tmp_str, wlc_s->psk);
		}
		else
		{
			sprintf(tmp_str, "wlc%d_auth_mode", band);
			nvram_set(tmp_str, "open");
		}
	}
	else {
		sprintf(tmp_str, "wlc%d_auth_mode", band);
		nvram_set(tmp_str, "open");
	}

	if (wep_flag) {

		sprintf(tmp_str, "wlc%d_wep_key", band);
		nvram_set(tmp_str, wlc_s->wep_key);

		sprintf(tmp_str, "wlc_key");
		sprintf(tmp, "%d", atoi(wlc_s->wep_tx_keyidx) + 1);
		nvram_set(tmp_str, tmp);
	}

	/* Finish WPS while dut is default, change the x_Setting as 1 */
	if(nvram_match("x_Setting", "0")) {
		nvram_set("x_Setting", "1");
		nvram_commit();
	}

	free(wlc_s);
	return 0;
}

static char *qca_readfile(FILE *fp, int *fsize)
{
	int size = 0;
	char *pt = NULL;

	if (fp == NULL)
		return NULL;

	char buf[128] = {0}, str[128] = {0};

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strstr(buf, "Failed"))
			return NULL;

		memset(str, 0, sizeof(str));
		sscanf(buf, "%*s%*s%*s%*s%s", str);

		str[strlen(str)] = '\n';

		size += strlen(str);

		pt=(char *)realloc(pt,size + 1);

		memcpy(pt+(size - strlen(str)), str, strlen(str));
	}

	if (pt != NULL)
		pt[size] = '\0';

	*fsize = size;

	return pt;
}

struct save_fuple {
	int length;
	char *cmppart;
	char *setpart1;
	char *setpart2;
};

static int qca_auto_detect_ssid(int band_chk,char* ssid_buf, char* result)
{
	char substrl[128], strNULL[]="";
	char *getptr1, *getptr2, *substrr, *gettmp[128];
	int fsize = 0, idlength=0, cmpresult=0;

	struct save_fuple *bandlist;
	struct save_fuple getSsidRule0[] = {
		{ 5	, "-2.4G"  , "-5G"	, ""  },
		{ 5	, "_2.4G"  , "_5G"	, ""  },
		{ 5	, ".2.4G"  , ".5G"	, ""  },
		{ 5	, " 2.4G"  , " 5G"	, ""  },
		{ 5	, "-2.4g"  , "-5g"	, ""  },
		{ 5	, "_2.4g"  , "_5g"	, ""  },
		{ 5	, ".2.4g"  , ".5g"      , ""  },
		{ 5	, " 2.4g"  , " 5g"      , ""  },
		{ 4     , "2.4G"   , "5G"       , ""  },
		{ 4	, "2.4g"   , "5g"  	, ""  },
		{ 4	, "-2.4"   , "-5"       , ""  },
		{ 4	, "_2.4"   , "_5"       , ""  },
		{ 4	, ".2.4"   , ".5"       , ""  },
		{ 4	, " 2.4"   , " 5"       , ""  },
		{ 3	, "2.4"    , "5"	, ""  },
		{ 3	, "-2G"    , "-5G"      , ""  },
		{ 3	, "_2G"    , "_5G"      , ""  },
		{ 3	, ".2G"    , ".5G"      , ""  },
		{ 3	, " 2G"    , " 5G"      , ""  },
		{ 3     , "-2g"    , "-5g"      , ""  },
		{ 0     , "_2g"    , "_5g"      , ""  },
		{ 3     , ".2g"    , ".5g"      , ""  },
		{ 3     , " 2g"    , " 5g"      , ""  },
		{ 2	, "2G"     , "5G"       , ""  },
		{ 2     , "2g"     , "5g"       , ""  },
		{ 2	, "-2"     , "-5"       , ""  },
		{ 2	, "_2"     , "_5"       , ""  },
		{ 2	, ".2"     , ".5"       , ""  },
		{ 2	, " 2"     , " 5"       , ""  },
		{ 1	, "2"      , "5"	, ""  },
		{ 0	, ""	   , "-5G"      , ""  },
		{ 0	, ""	   , "_5G"      , ""  },
		{ 0	, ""	   , ".5G"      , ""  },
		{ 0	, ""	   , " 5G"      , ""  },
		{ 0	, ""	   , "-5g"      , ""  },
		{ 0	, ""	   , "_5g"      , ""  },
		{ 0	, ""	   , ".5g"      , ""  },
		{ 0	, ""	   , " 5g"      , ""  },
		{ 0	, ""	   , "5G"       , ""  },
		{ 0	, ""	   , "5g"       , ""  },
		{ 0	, ""	   , "-5"       , ""  },
		{ 0	, ""	   , "_5"       , ""  },
		{ 0	, ""	   , ".5"       , ""  },
		{ 0	, ""	   , " 5"       , ""  },
		{ 0	, ""	   , "5"	, ""  },
		{ 0	, ""	   , ""		, ""  },
		{ 99	, ""	   , ""		, ""  }
	};
	struct save_fuple getSsidRule1[] = {
		{ 3	, "-5G"	, "-2G"	, "-2.4G"  },
		{ 3	, "_5G"	, "_2G"	, "_2.4G"  },
		{ 3	, ".5G"	, ".2G"	, ".2.4G"  },
		{ 3	, " 5G"	, " 2G"	, " 2.4G"  },
		{ 3	, "-5g"	, "-2g"	, "-2.4g"  },
		{ 3	, "_5g"	, "_2g"	, "_2.4g"  },
		{ 3	, ".5g"	, ".2g"	, ".2.4g"  },
		{ 3	, " 5g"	, " 2g"	, " 2.4g"  },
		{ 2	, "5G"	, "2G"	, "2.4G"   },
		{ 2	, "5g"	, "2g" 	, "2.4g"   },
		{ 2	, "-5"	, "-2"	, "-2.4"   },
		{ 2	, "_5"	, "_2"	, "_2.4"   },
		{ 2	, ".5"	, ".2"	, ".2.4"   },
		{ 2	, " 5"	, " 2"	, " 2.4"   },
		{ 1	, "5"	, "2" 	, "2.4"    },
		{ 0	, ""	, "-2G"	, "-2.4G"  },
		{ 0     , ""    , "_2G" , "_2.4G"  },
		{ 0     , ""    , ".2G" , ".2.4G"  },
		{ 0     , ""    , " 2G" , " 2.4G"  },
		{ 0     , ""    , "-2g" , "-2.4g"  },
		{ 0     , ""    , "_2g" , "_2.4g"  },
		{ 0     , ""    , ".2g" , ".2.4g"  },
		{ 0     , ""    , " 2g" , " 2.4g"  },
		{ 0     , ""    , "2G" 	, "2.4G"   },
		{ 0     , ""    , "2g"  , "2.4g"   },
		{ 0     , ""    , "-2"  , "-2.4"   },
		{ 0     , ""    , "_2"  , "_2.4"   },
		{ 0     , ""    , ".2"  , ".2.4"   },
		{ 0     , ""    , " 2"  , " 2.4"   },
		{ 0     , ""    , "2"  	, "2.4"    },
		{ 0     , ""    , ""  	, ""       },
		{ 99    , ""    , ""    , ""       }
	};

	//Get another band's SSID list
	FILE *pfp = NULL;
	if(band_chk == 0)
	{
		eval("wpa_cli", "-p", "/var/run/wpa_supplicant-sta0", "scan");
		sleep(3);
		pfp = popen("wpa_cli -p /var/run/wpa_supplicant-sta0 -i sta0 scan_results", "r");
	}
	else if(band_chk == 1)
	{
		eval("wpa_cli", "-p", "/var/run/wpa_supplicant-sta1", "scan");
		sleep(3);
		pfp = popen("wpa_cli -p /var/run/wpa_supplicant-sta1 -i sta1 scan_results", "r");
	}

	if (pfp != NULL) {
		getptr1 = qca_readfile(pfp, &fsize);
		pclose(pfp);

		if (getptr1 == NULL)
			return 0;

		getptr2 = strstr(getptr1, "\n");

		while (getptr2 != NULL) {
			*getptr2 = '\0';
			gettmp[idlength] = getptr1;
			getptr1 = getptr2 + 1;
			getptr2 = strstr(getptr1, "\n");
			idlength ++;
		}
	}
	else
		return 0;

	if ( band_chk )
		bandlist = getSsidRule0;
	else
		bandlist = getSsidRule1;

	//compare the SSID with SCAN LIST
	while( bandlist->length != 99 ) {
		if ( strlen(ssid_buf) > bandlist->length )
			substrr = ssid_buf + strlen(ssid_buf) - bandlist->length;
		else {
			bandlist++;
			continue;
		}

		if( bandlist->length == 0 ) {
			strcpy( substrl, ssid_buf );
			if ( qca_comparetmp( gettmp, idlength, substrl, bandlist->setpart1 ) ) {
				cmpresult=1;
				break ;
			}
			if ( *bandlist->setpart2 != '\0' ) {
				if ( qca_comparetmp( gettmp, idlength, substrl, bandlist->setpart2 ) ) {
					cmpresult=1;
					break;
				}
			}
		}
		else if( !strcmp(substrr, bandlist->cmppart) ){
			strncpy( substrl, ssid_buf, strlen(ssid_buf)-bandlist->length );
			substrl[strlen(ssid_buf)-bandlist->length] = '\0';
			if( qca_comparetmp( gettmp, idlength, substrl, bandlist->setpart1 ) ) {
				cmpresult=1;
				break;
			}
			if ( *bandlist->setpart2 != '\0' ) {
				if ( qca_comparetmp( gettmp, idlength, substrl, bandlist->setpart2 ) ) {
					cmpresult=1;
					break;
				}
			}
			if ( qca_comparetmp( gettmp, idlength, substrl, strNULL ) ) {
				cmpresult = 1;
				break;
			}
		}
		bandlist++;
	}

	if( cmpresult == 1 ) {
		TRACE_PT("=== Find the SSID : [ %s ]\n", substrl);
		strcpy(result,substrl);
	}
	else
		TRACE_PT("=== Can't find the SSID : [  ]\n");

	return cmpresult;
}

void qca_wps_state_check(void)
{

	if (nvram_get_int("wps_enable") != 1)
		return;

	if (nvram_get_int("wps_cli_state") != 1)
		return;

	if (sw_mode() != SW_MODE_REPEATER)
		return;

	/* Get 2.4G/5G WPS Status */
	char cmd[64] = {0}, wps_2g_ssid[32] = {0}, wps_5g_ssid[32] = {0};
	char *buf = malloc(sizeof(char) * 64);
	char *wpa_state = NULL, *wps_ssid = NULL;
	FILE *fp1 = NULL, *fp2 = NULL;
	int detect_result = 0;

	/* 0: Repeater. 1: Express way 2.4G 2: Express way 5G */
	int wlc_express = nvram_get_int("wlc_express");
	if (wlc_express < 0 || wlc_express > 2)
		wlc_express = 0;

	memset(buf, 0, sizeof(buf));

	if (wlc_express == 0 || wlc_express == 1) {
		sprintf(cmd, "wpa_cli -p /var/run/wpa_supplicant-sta0 -i sta0 status");
		fp1 = popen(cmd, "r");
	}
	if (wlc_express == 0 || wlc_express == 2) {
		sprintf(cmd, "wpa_cli -p /var/run/wpa_supplicant-sta1 -i sta1 status");
		fp2 = popen(cmd, "r");
	}
	if (fp1 != NULL) {
		while (fgets(buf, 64, fp1) != NULL) {
			if (strstr(buf, "ssid") && !strstr(buf, "bssid")) {
				wps_ssid = buf + 5;
				sprintf(wps_2g_ssid, "%s", wps_ssid);
				wps_2g_ssid[strlen(wps_2g_ssid)-1] = '\0';
			}

			if (strstr(buf, "wpa_state")) {
				wpa_state = buf + 10;

				if (strstr(wpa_state, "AUTHENTICATING")||
				     strstr(wpa_state, "ASSOCIATING") ||
				     strstr(wpa_state, "ASSOCIATED") ||
				     strstr(wpa_state, "ASSOCIATED") ||
				     strstr(wpa_state, "4WAY_HANDSHAKE") ||
				     strstr(wpa_state, "GROUP_HANDSHAKE"))
					nvram_set_int("led_status", LED_WPS_PROCESSING);

				if (strstr(wpa_state, "COMPLETED") && nvram_get_int("led_status") == LED_WPS_PROCESSING) {
					if (!wps_2g_done)
						qca_set_wps_result(0, 0);
					wps_2g_done = 1;
				}
			}
		}
	}

	if (fp2 != NULL) {
		while (fgets(buf, 64, fp2) != NULL) {

			if (strstr(buf, "ssid") && !strstr(buf, "bssid")) {
				wps_ssid = buf + 5;
				sprintf(wps_5g_ssid, "%s", wps_ssid);
				wps_5g_ssid[strlen(wps_5g_ssid)-1] = '\0';
			}

			if (strstr(buf, "wpa_state")) {
				wpa_state = buf + 10;

				if (strstr(wpa_state, "AUTHENTICATING")||
				     strstr(wpa_state, "ASSOCIATING") ||
				     strstr(wpa_state, "ASSOCIATED") ||
				     strstr(wpa_state, "ASSOCIATED") ||
				     strstr(wpa_state, "4WAY_HANDSHAKE") ||
				     strstr(wpa_state, "GROUP_HANDSHAKE"))
					nvram_set_int("led_status", LED_WPS_PROCESSING);

				if (strstr(wpa_state, "COMPLETED") && nvram_get_int("led_status") == LED_WPS_PROCESSING) {
					if (!wps_5g_done)
						qca_set_wps_result(1, 1);
					wps_5g_done = 1;
				}
			}
		}
	}

	if (wps_2g_done) {
		if (wlc_express == 0) {
			if(wps_5g_done || wait_5g_time >= MAX_WAIT_COUNT) {

				if (!wps_5g_done) {
					stop_wps_method();

					if (qca_auto_detect_ssid(1, wps_2g_ssid, wps_5g_ssid))
					{
						qca_set_wps_result(0, 1);
						nvram_set("wlc1_ssid", wps_5g_ssid);
						detect_result = 1;
					}
				}

				if (wps_5g_done) { // Both
					set_wps_result_to_ap_nvram(0, 1, 1);
					set_wps_result_to_ap_nvram(0, 0, 0);
				}
				else if (!wps_5g_done && detect_result) { // Both
					set_wps_result_to_ap_nvram(0, 1, 1);
					set_wps_result_to_ap_nvram(0, 0, 0);
				}
				else { // Only 2.4G
					set_wps_result_to_ap_nvram(1, 0, 1);
					set_wps_result_to_ap_nvram(1, 0, 0);
				}

				nvram_set("wps_cli_state", "2");
				wps_2g_done = 0;
				wps_5g_done = 0;
				wait_2g_time = 0;
				wait_5g_time = 0;
				wait_5g_time = 0;
				nvram_commit();
				notify_rc("restart_wireless");
				nvram_set_int("led_status", LED_WPS_RESTART_WL);
			}
			else
				 wait_5g_time++;
		}
		else {
			stop_wps_method();
			set_wps_result_to_ap_nvram(1, 0, 1);
			nvram_set("wps_cli_state", "2");
			wps_2g_done = 0;
			wps_5g_done = 0;
			wait_2g_time = 0;
			wait_5g_time = 0;
			wait_5g_time = 0;
			nvram_commit();
			notify_rc("restart_wireless");
			nvram_set_int("led_status", LED_WPS_RESTART_WL);
		}
	}

	if (wps_5g_done) {
		if (wlc_express == 0) {
			if(wps_2g_done || wait_2g_time >= MAX_WAIT_COUNT) {
				if (!wps_2g_done) {
					stop_wps_method();

					if (qca_auto_detect_ssid(0, wps_5g_ssid, wps_2g_ssid))
					{
						qca_set_wps_result(1, 0);
						nvram_set("wlc0_ssid", wps_2g_ssid);
						detect_result = 1;
					}
				}

				if (wps_2g_done) { // Both
					set_wps_result_to_ap_nvram(0, 1, 1);
					set_wps_result_to_ap_nvram(0, 0, 0);
				}
				else if (!wps_2g_done && detect_result) { // Both
					set_wps_result_to_ap_nvram(0, 1, 1);
					set_wps_result_to_ap_nvram(0, 0, 0);
				}
				else { // Only 5G
					set_wps_result_to_ap_nvram(1, 1, 1);
					set_wps_result_to_ap_nvram(1, 1, 0);
				}

				nvram_set("wps_cli_state", "2");
				wps_2g_done = 0;
				wps_5g_done = 0;
				wait_2g_time = 0;
				wait_5g_time = 0;
				nvram_commit();
				notify_rc("restart_wireless");
				nvram_set_int("led_status", LED_WPS_RESTART_WL);
			}
			else
				 wait_2g_time++;
		}
		else {
			stop_wps_method();
			set_wps_result_to_ap_nvram(1, 1, 0);
			nvram_set("wps_cli_state", "2");
			wps_2g_done = 0;
			wps_5g_done = 0;
			wait_2g_time = 0;
			wait_5g_time = 0;
			wait_5g_time = 0;
			nvram_commit();
			notify_rc("restart_wireless");
			nvram_set_int("led_status", LED_WPS_RESTART_WL);
		}
	}

	pclose(fp1);
	pclose(fp2);
	free(buf);

	return;
}
#endif /* RTCONFIG_CONCURRENTREPEATER */
#endif /* RTCONFIG_REALTEK */

void service_check(void)
{
	static int boot_ready = 0;

	if (boot_ready > 6)
		return;

	if (!nvram_match("success_start_service", "1"))
		return;

	led_control(LED_POWER, ++boot_ready%2);
}

/* @return:
 * 	0:	not in MFG mode.
 *  otherwise:	in MFG mode.
 */
static int handle_btn_in_mfg(void)
{
	char msg[64];
	const struct mfg_btn_s *p;

	if (!nvram_match("asus_mfg", "1"))
		return 0;

	// TRACE_PT("asus mfg btn check!!!\n");
	for (p = &mfg_btn_table[0]; p->id < BTN_ID_MAX; ++p) {
		if (button_pressed(p->id)) {
			if (p->name) {
				snprintf(msg, sizeof(msg), "button %s pressed\n", p->name);
				TRACE_PT("%s", msg);
			}

			nvram_set(p->nv, "1");
		} else {
			/* TODO: handle button release. */
		}
	}

#ifdef RTCONFIG_WIRELESS_SWITCH
	if (!button_pressed(BTN_WIFI_SW)) {
		nvram_set("btn_wifi_sw", "0");
	}
#endif

#ifdef RTCONFIG_LED_BTN /* currently for RT-AC68U only */
#if defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER) || defined(GTAC9600)
	if (!button_pressed(BTN_LED))
#elif defined(RTAC68U)
	if (is_ac66u_v2_series())
		;
	else if (((!nvram_match("cpurev", "c0") || nvram_get_int("PA") == 5023) && button_pressed(BTN_LED)) ||
		   (nvram_match("cpurev", "c0") && nvram_get_int("PA") != 5023 && !button_pressed(BTN_LED)))
#endif
	{
		TRACE_PT("button LED pressed\n");
		nvram_set("btn_led", "1");
	}
#if defined(RTAC68U)
	else if (!nvram_match("cpurev", "c0") || nvram_get_int("PA") == 5023)
	{
		TRACE_PT("button LED released\n");
		nvram_set("btn_led", "0");
	}
#endif
#endif

#ifdef RTCONFIG_SWMODE_SWITCH
#if defined(PLAC66U)
	if (button_pressed(BTN_SWMODE_SW_ROUTER) != nvram_get_int("swmode_switch"))
	{
		TRACE_PT("Switch changeover\n");
		nvram_set("switch_mode", "1");
	}
#endif
#endif

	return 1;
}

#if defined(RTCONFIG_EJUSB_BTN) && defined(RTCONFIG_BLINK_LED)
struct ejusb_btn_s {
	enum btn_id btn;
	char *name;
	int state;
	float t1;
	unsigned int port_mask;
	unsigned int nr_pids;
	pid_t pids[8];
	char *led[2];
};


/**
 * Set USB LED blink pattern and enable it for eject USB button.
 * @p:
 * @mode:
 * 	2:	user defined pattern mode,
 * 		OFF 0.15s, ON 0.15s, OFF 0.05s, ON 0.05s, OFF 0.05s, ON 0.05s, OFF 0.05s, ON 0.05s
 * 	1:	user defined pattern mode,
 * 		OFF 0.7s, ON 0.7s
 *  otherwise:	normal mode
 */
static inline void set_usbled_for_ejusb_btn(struct ejusb_btn_s *p, int mode)
{
	int i;

	if (!p)
		return;

	switch (mode) {
	case 2:
		/* user defined pattern mode */
		for (i = 0 ; i < ARRAY_SIZE(p->led); ++i) {
			if (!p->led[i])
				break;

			set_bled_udef_pattern(p->led[i], 50, "0 0 0 1 1 1 0 1 0 1 0 1");
			set_bled_udef_pattern_mode(p->led[i]);
		}
		break;
	case 1:
		/* user defined pattern mode */
		for (i = 0 ; i < ARRAY_SIZE(p->led); ++i) {
			if (!p->led[i])
				break;

			set_bled_udef_pattern(p->led[i], 700, "0 1");
			set_bled_udef_pattern_mode(p->led[i]);
		}
		break;
	default:
		/* normal mode */
		for (i = 0 ; i < ARRAY_SIZE(p->led); ++i) {
			if (!p->led[i])
				break;

			set_bled_normal_mode(p->led[i]);
		}
	}
}


/**
 * Handle eject USB button.
 */
static void handle_eject_usb_button(void)
{
	int i, v, port, m, model;
	unsigned int c;
	char *gpio, nv[32], port_str[5] = "-1";
	char *ejusb_argv[] = { "ejusb", port_str, "1", "-u", "1", NULL };
	struct ejusb_btn_s *p;
	static unsigned int first = 1, nr_ejusb_btn = 0;
	static struct ejusb_btn_s ejusb_btn[] = {
		{
			.btn = BTN_EJUSB1,
			.name = "Eject USB button1",
			.state = 0,
			.port_mask = 0,
			.led = { NULL, NULL },
		},
		{
			.btn = BTN_EJUSB2,
			.name = "Eject USB button2",
			.state = 0,
			.port_mask = 0,
			.led = { NULL, NULL },
		}
	};

	/* If RESET button is triggered, don't handle eject USB button. */
	if (btn_pressed)
		return;

	if (first) {
		first = 0;
		p = &ejusb_btn[0];

		for (i = 0; i < ARRAY_SIZE(ejusb_btn); ++i) {
			sprintf(nv, "btn_ejusb%d_gpio", i + 1);
			if (!(gpio = nvram_get(nv)))
				continue;
			if (((v = atoi(gpio)) & GPIO_PIN_MASK) == GPIO_PIN_MASK)
				continue;

			nr_ejusb_btn++;
			p[i].port_mask = (v & GPIO_EJUSB_MASK) >> GPIO_EJUSB_SHIFT;
		}

		/* If DUT has two USB LED, associate 2-nd USB LED with eject USB button
		 * based on number of eject USB button.
		 */
		model = get_model();
		switch (nr_ejusb_btn) {
		case 2:
			if (have_usb3_led(model)) {
				p[0].led[0] = "led_usb_gpio";
				p[1].led[0] = "led_usb3_gpio";
			} else {
				p[0].led[0] = "led_usb_gpio";
				p[1].led[0] = "led_usb_gpio";
			}
			break;
		case 1:
			if (have_usb3_led(model)) {
				p[0].led[0] = "led_usb_gpio";
				p[0].led[1] = "led_usb3_gpio";
			} else {
				p[0].led[0] = "led_usb_gpio";
				p[0].led[1] = NULL;
			}
			break;
		default:
			nr_ejusb_btn = 0;
		}
	}
	if (!nr_ejusb_btn)
		return;

	for (i = 0, p = &ejusb_btn[0]; i < ARRAY_SIZE(ejusb_btn); ++i, ++p) {
		if (button_pressed(p->btn)) {
			TRACE_PT("%s pressed\n", p->name);
			/* button pressed */
			switch (p->state) {
			case 0:
				p->state = 1;
				p->t1 = uptime2();
				break;
			case 1:
				if ((int)(uptime2() - p->t1) >= EJUSB_WAIT_COUNT) {
					_dprintf("You can release %s now!\n", p->name);
					set_usbled_for_ejusb_btn(p, 1);
					p->state = 2;
				}
				break;
			case 2:
				/* nothing to do */
				break;
			case 3:
				/* nothing to do */
				break;
			default:
				_dprintf("%s: %s pressed, unknown state %d\n", __func__, p->state);
				p->state = 0;
				set_usbled_for_ejusb_btn(p, 0);
			}
		} else {
			/* button released */
			if (p->state)
				TRACE_PT("%s released\n", p->name);

			switch (p->state) {
			case 0:
				/* nothing to do */
				break;
			case 1:
				p->state = 0;
				break;
			case 2:
				/* Issue ejusb command. */
				p->nr_pids = 0;
				memset(&p->pids[0], 0, sizeof(p->pids));
				set_usbled_for_ejusb_btn(p, 2);
				if (p->port_mask) {
					for (m = p->port_mask, port = 1; m > 0; m >>= 1, port++) {
						if (!(m & 1))
							continue;
						sprintf(port_str, "%d", port);
						_eval(ejusb_argv, NULL, 0, &p->pids[p->nr_pids++]);
					}
				} else {
					strcpy(port_str, "-1");
					_eval(ejusb_argv, NULL, 0, &p->pids[p->nr_pids++]);
				}
				p->state = 3;
				break;
			case 3:
				for (i = 0, c = 0; p->nr_pids > 0 && i < p->nr_pids; ++i) {
					if (!p->pids[i])
						continue;
					if (!process_exists(p->pids[i]))
						p->pids[i] = 0;
					else
						c++;
				}
				if (!c) {
					p->state = 0;
					set_usbled_for_ejusb_btn(p, 0);
				}
				break;
			default:
				_dprintf("%s: %s released, unknown state %d\n", __func__, p->state);
				p->state = 0;
				set_usbled_for_ejusb_btn(p, 0);
			}
		}
	}
}
#else	/* !(RTCONFIG_EJUSB_BTN && RTCONFIG_BLINK_LED) */
static inline void handle_eject_usb_button(void) { }
#endif	/* RTCONFIG_EJUSB_BTN && RTCONFIG_BLINK_LED */

void btn_check(void)
{
#ifdef RTCONFIG_WIFI_SON
	pid_t pid;
	char *argv[]={"/sbin/delay_exec","4","rc rc_service restart_allnet",NULL};
#endif

	if (handle_btn_in_mfg())
		return;

#ifdef BTN_SETUP
	if (btn_pressed_setup == BTNSETUP_NONE)
	{
#endif
#ifndef RTCONFIG_N56U_SR2
		if (button_pressed(BTN_RESET))
#else
		if (0)
#endif
		{
			TRACE_PT("button RESET pressed\n");

	/*--------------- Add BTN_RST MFG test ------------------------*/
#ifndef RTCONFIG_WPS_RST_BTN
#ifdef RTCONFIG_DSL /* Paul add 2013/4/2 */
			if ((btn_count % 2) == 0)
				led_control(LED_POWER, LED_ON);
			else
				led_control(LED_POWER, LED_OFF);
#endif
#endif	/* ! RTCONFIG_WPS_RST_BTN */

			if (!btn_pressed)
			{
				btn_pressed = 1;
				btn_count = 0;
				alarmtimer(0, URGENT_PERIOD);
			}
			else
			{	/* Whenever it is pushed steady */
				if (++btn_count > RESET_WAIT_COUNT)
				{
					dbg("You can release RESET button now!\n");
#ifdef BLUECAVE
					if(btn_pressed == 1) {
						nvram_set("bc_ledbh", "reset");
						kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
					}
					run_app_script(NULL, "stop");
					system("ejusb -1");
#endif
#if (defined(PLN12) || defined(PLAC56))
					if (btn_pressed == 1)
						set_wifiled(5);
#elif defined(MAPAC1750)
					if (btn_pressed == 1)
						set_rgbled(RGBLED_YELLOW_SBLINK);
#elif defined(RTCONFIG_REALTEK)
					if (btn_pressed == 1)
						set_led(LED_BLINK_SLOW, LED_BLINK_SLOW);
#endif
					btn_pressed = 2;
				}
				if (btn_pressed == 2)
				{
#ifdef RTCONFIG_DSL /* Paul add 2013/4/2 */
					led_control(LED_POWER, LED_OFF);
					alarmtimer(0, 0);
					nvram_set("restore_defaults", "1");
					if (notify_rc_after_wait("resetdefault")) {
						/* Send resetdefault rc_service failed. */
						alarmtimer(NORMAL_PERIOD, 0);
					}
#elif defined(RTCONFIG_LP5523)
					if (btn_count == RESET_WAIT_COUNT+1)
						lp55xx_leds_proc(LP55XX_ORANGE_LEDS, LP55XX_RESET_SUCCESS);
#elif defined(MAPAC1750)
#else
				/* 0123456789 */
				/* 0011100111 */
					if ((btn_count % 10) < 2 || ((btn_count % 10) > 4 && (btn_count % 10) < 7))
#if defined(RTN11P) || defined(RTN300)
					{
						led_control(LED_LAN, LED_OFF);
						led_control(LED_WAN, LED_OFF);
						led_control(LED_WPS, LED_OFF);
					}
					else
					{
						led_control(LED_LAN, LED_ON);
						led_control(LED_WAN, LED_ON);
						led_control(LED_WPS, LED_ON);
					}
#else	/* ! (RTN11P || RTN300) */
						led_control(LED_POWER, LED_OFF);
					else
						led_control(LED_POWER, LED_ON);
#endif	/* ! (RTN11P || RTN300) */
#endif
				}
			}
		}
#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_DSL)
		else if (button_pressed(BTN_WIFI_SW))
		{
		//TRACE_PT("button BTN_WIFI_SW pressed\n");
			if (wlan_sw_init == 0)
			{
				wlan_sw_init = 1;
/*
				eval("iwpriv", "ra0", "set", "RadioOn=1");
				eval("iwpriv", "rai0", "set", "RadioOn=1");
				TRACE_PT("Radio On\n");
				nvram_set("wl0_radio", "1");
				nvram_set("wl1_radio", "1");
				nvram_commit();
*/
			}
			else
			{
				// if wlan switch on , btn reset routine goes here
				if (btn_pressed == 2)
				{
					// IT MUST BE SAME AS BELOW CODE
					led_control(LED_POWER, LED_OFF);
					alarmtimer(0, 0);
					nvram_set("restore_defaults", "1");
					if (notify_rc_after_wait("resetdefault")) {
						/* Send resetdefault rc_service failed. */
						alarmtimer(NORMAL_PERIOD, 0);
					}
				}

				if (nvram_match("wl0_HW_switch", "0") || nvram_match("wl1_HW_switch", "0")) {
					//Ever apply the Wireless-Professional Web GU.
					//Not affect the status of WiFi interface, so do nothing
				}
				else{	//trun on WiFi by HW slash, make sure both WiFi interface enable.
					if (nvram_match("wl0_radio", "0") || nvram_match("wl1_radio", "0")) {
						eval("iwpriv", "ra0", "set", "RadioOn=1");
						eval("iwpriv", "rai0", "set", "RadioOn=1");
						TRACE_PT("Radio On\n");
						nvram_set("wl0_radio", "1");
						nvram_set("wl1_radio", "1");

						nvram_set("wl0_HW_switch", "0");
						nvram_set("wl1_HW_switch", "0");
						nvram_commit();
					}
				}
			}
		}
#endif	/* RTCONFIG_WIRELESS_SWITCH && RTCONFIG_DSL */
		else
		{
#ifdef RTCONFIG_WPS_RST_BTN
			if (btn_pressed == 0)
				;
			else if (btn_count < WPS_RST_DO_RESTORE_COUNT)
			{
				if (btn_count < WPS_RST_DO_RESTORE_COUNT && btn_count >= WPS_RST_DO_WPS_COUNT && nvram_match("btn_ez_radiotoggle", "1"))
				{
					radio_switch(0);
				}
#ifdef RTCONFIG_WPS_ALLLED_BTN
				else if (btn_count < WPS_RST_DO_RESTORE_COUNT && btn_count >= WPS_RST_DO_WPS_COUNT && nvram_match("btn_ez_mode", "1"))
				{
					LED_status_on = nvram_get_int("AllLED");


					if (LED_status_on)
						nvram_set_int("AllLED", 0);
					else
						nvram_set_int("AllLED", 1);
					LED_status_on = !LED_status_on;

					if (LED_status_on) {
						TRACE_PT("LED turn to normal\n");
						led_control(LED_POWER, LED_ON);
#if defined(RTN11P_B1)
						system("reg s 0xB0000000; reg w 0x64 0x30015014");
						system("reg s 0xB0000600; reg w 0x04 0x1C20; reg w 0x24 0x69CB");
						led_control(LED_WAN, LED_ON);
						led_control(LED_LAN, LED_ON);
						led_control(LED_2G, LED_ON);
#endif
					}
					else {
						TRACE_PT("LED turn off\n");
						setAllLedOff();
					}

					/* check LED_WAN status */
					kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
				}
#endif
				if (btn_count < WPS_RST_DO_RESTORE_COUNT && btn_count >= WPS_RST_DO_WPS_COUNT
				   && !no_need_to_start_wps()
				   && !wps_band_radio_off(get_radio_band(0))
				   && !wps_band_ssid_broadcast_off(get_radio_band(0))
				   && nvram_match("btn_ez_radiotoggle", "0")
#ifdef RTCONFIG_WPS_ALLLED_BTN
				   && nvram_match("btn_ez_mode", "0")
#endif
				   && !nvram_match("wps_ign_btn", "1")
				)
				{
					btn_pressed_setup = BTNSETUP_DETECT;
					btn_count_setup = WPS_WAIT_COUNT;	//to trigger WPS
					alarmtimer(0, RUSHURGENT_PERIOD);
				}
				else
				{
					btn_pressed_setup = BTNSETUP_NONE;
					btn_count_setup = 0;
					alarmtimer(NORMAL_PERIOD, 0);
				}

#ifdef RTCONFIG_WPS_ALLLED_BTN
				if (!(btn_count < WPS_RST_DO_RESTORE_COUNT && btn_count >= WPS_RST_DO_WPS_COUNT && nvram_match("btn_ez_mode", "1")))
#endif
				led_control(LED_POWER, LED_ON);

				btn_count = 0;
				btn_pressed = 0;
			}
			else if (btn_count >= WPS_RST_DO_RESTORE_COUNT)	// to do restore
#else	/* ! RTCONFIG_WPS_RST_BTN */
			if (btn_pressed == 1)
			{
				btn_count = 0;
				btn_pressed = 0;
#if defined(RTCONFIG_LP5523)
				lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_PREVIOUS_STATE);
#elif defined(MAPAC1750)
#else
				led_control(LED_POWER, LED_ON);
#endif
				alarmtimer(NORMAL_PERIOD, 0);
			}
			else if (btn_pressed == 2)
#endif	/* ! RTCONFIG_WPS_RST_BTN */
			{
#if defined(RTN11P_B1)
				stop_wanduck();
				//Set WLED_N(GPIO44) to GPIO Mode, and turn on.
				system("reg s 0xB0000000; reg w 0x64 0x30015015");
				system("reg s 0xB0000600; reg w 0x04 0x1C20; reg w 0x24 0x69CB");
				led_control(LED_POWER, LED_OFF);
				led_control(LED_WAN, LED_OFF);
				led_control(LED_LAN, LED_OFF);
				led_control(LED_2G, LED_OFF);
				sleep(1);
				led_control(LED_WAN, LED_ON);
				led_control(LED_LAN, LED_ON);
				led_control(LED_2G, LED_ON);
				sleep(1);
				led_control(LED_WAN, LED_OFF);
				led_control(LED_LAN, LED_OFF);
				led_control(LED_2G, LED_OFF);
				sleep(1);
				led_control(LED_POWER, LED_ON);
				sleep(1);
#elif defined(RTCONFIG_LP5523)
				lp55xx_leds_proc(LP55XX_ORANGE_LEDS, LP55XX_ACT_BREATH_DOWN_011);
#elif defined(PLN12) || defined(PLAC56)
				set_wifiled(2);
#elif defined(MAPAC1750)
				set_rgbled(RGBLED_YELLOW);
#else
				led_control(LED_POWER, LED_OFF);
#endif
				alarmtimer(0, 0);
				nvram_set("restore_defaults", "1");
				if(notify_rc_after_wait("resetdefault_erase"))
				{
					/* Send resetdefault rc_service failed. */
					alarmtimer(NORMAL_PERIOD, 0);
				}
			}
#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_DSL)
			else
			{
				// no button is pressed or released
				if (wlan_sw_init == 0)
				{
					wlan_sw_init = 1;
					eval("iwpriv", "ra0", "set", "RadioOn=0");
					eval("iwpriv", "rai0", "set", "RadioOn=0");
					TRACE_PT("Radio Off\n");
					nvram_set("wl0_radio", "0");
					nvram_set("wl1_radio", "0");

					nvram_set("wl0_HW_switch", "1");
					nvram_set("wl1_HW_switch", "1");

					nvram_commit();
				}
				else
				{
					if (nvram_match("wl0_radio", "1") || nvram_match("wl1_radio", "1")) {
						eval("iwpriv", "ra0", "set", "RadioOn=0");
						eval("iwpriv", "rai0", "set", "RadioOn=0");
						TRACE_PT("Radio Off\n");
						nvram_set("wl0_radio", "0");
						nvram_set("wl1_radio", "0");

						nvram_set("wl0_timesched", "0");
						nvram_set("wl1_timesched", "0");
					}

					//indicate use switch HW slash manually.
					if (nvram_match("wl0_HW_switch", "0") || nvram_match("wl1_HW_switch", "0")) {
						nvram_set("wl0_HW_switch", "1");
						nvram_set("wl1_HW_switch", "1");
					}
				}
			}
#endif	/* RTCONFIG_WIRELESS_SWITCH && RTCONFIG_DSL */
		}

#ifdef BTN_SETUP
	}

	if (btn_pressed != 0) return;

#if defined(RTCONFIG_EJUSB_BTN) && defined(RTCONFIG_BLINK_LED)
	handle_eject_usb_button();
#endif

#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_QCA)
	// wait until wl is ready
	if (!nvram_get_int("wlready")) return;
#endif

#if defined(RTCONFIG_WIRELESS_SWITCH) && defined(RTCONFIG_QCA)
	if (wifi_sw_old != button_pressed(BTN_WIFI_SW))
	{
		wifi_sw_old = button_pressed(BTN_WIFI_SW);
		if (wifi_sw_old != 0 && (!nvram_match("wl0_HW_switch", "0") || !nvram_match("wl1_HW_switch", "0")))
		{
			TRACE_PT("button BTN_WIFI_SW pressed: ON\n");
			nvram_set("wl0_radio", "1");
			nvram_set("wl1_radio", "1");
			nvram_set("wl0_HW_switch", "0");	// 0 to be ON
			nvram_set("wl1_HW_switch", "0");	// 0 to be ON
			nvram_commit();
			eval("radio", "on");			// ON
		}
		else if (wifi_sw_old == 0 && (!nvram_match("wl0_HW_switch", "1") || !nvram_match("wl1_HW_switch", "1")))
		{
			TRACE_PT("button BTN_WIFI_SW released: OFF\n");
			eval("radio", "off");			// OFF
			nvram_set("wl0_radio", "0");
			nvram_set("wl1_radio", "0");
			nvram_set("wl0_HW_switch", "1");	// 1 to be OFF
			nvram_set("wl1_HW_switch", "1");	// 1 to be OFF
			nvram_commit();
		}
	}
#endif	/* RTCONFIG_WIRELESS_SWITCH && RTCONFIG_QCA */


#ifndef RTCONFIG_WPS_RST_BTN
	// independent wifi-toggle btn or Added WPS button radio toggle option
#if defined(RTCONFIG_WIFI_TOG_BTN)
	if (button_pressed(BTN_WIFI_TOG))
#else
	if (button_pressed(BTN_WPS) && nvram_match("btn_ez_radiotoggle", "1")
		&& (sw_mode() != SW_MODE_REPEATER)) // repeater mode not support HW radio
#endif
	{
		TRACE_PT("button WIFI_TOG pressed\n");
		if (btn_pressed_toggle_radio == 0) {
			radio_switch(0);
			btn_pressed_toggle_radio = 1;
			return;
		}
	}
	else{
		btn_pressed_toggle_radio = 0;
	}

#if defined(RTCONFIG_WPS_ALLLED_BTN)
	//use wps button to control all led on/off
	if (nvram_match("btn_ez_mode", "1"))
	{
		LED_status_old = LED_status;
		LED_status = button_pressed(BTN_WPS);

		if (LED_status) {
			TRACE_PT("button LED pressed\n");
			++BTN_pressed_count;
		}
		else{
			BTN_pressed_count = 0;
			LED_status_changed = 0;
		}

		if (BTN_pressed_count > WPS_LED_WAIT_COUNT && LED_status_changed == 0) {
			LED_status_changed = 1;
			LED_status_on = nvram_get_int("AllLED");

			if (LED_status_on)
				nvram_set_int("AllLED", 0);
			else
				nvram_set_int("AllLED", 1);
			LED_status_on = !LED_status_on;

			if (LED_status_on) {
				TRACE_PT("LED turn to normal\n");
				led_control(LED_POWER, LED_ON);
#if defined(RTAC65U) || defined(RTAC85U) || defined(RTN800HP)
			if (nvram_match("wl0_radio", "1")) {
				led_control(LED_2G, LED_ON);
			}
#ifdef RTCONFIG_HAS_5G
			if (nvram_match("wl1_radio", "1")) {
				led_control(LED_5G, LED_ON);
			}
#endif
#endif
#if defined(RTAC51UP) || defined(RTAC53)
				eval("rtkswitch", "100", "0x20000"); //lan/wan ethernet/giga led
				led_table_ctrl(LED_ON);
#endif
#if DSL_AC68U
				char *dslledcmd_argv[] = {"adslate", "led", "normal", NULL};
				_eval(dslledcmd_argv, NULL, 5, NULL);
				eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
				eval("et", "robowr", "0", "0x1a", "0x01ff");
				if (wlonunit == -1 || wlonunit == 0) {
					eval("wl", "ledbh", "10", "7");
				}
				if (wlonunit == -1 || wlonunit == 1) {
					eval("wl", "-i", "eth2", "ledbh", "10", "7");
					if (nvram_match("wl1_radio", "1")) {
						nvram_set("led_5g", "1");
						led_control(LED_5G, LED_ON);
					}
				}

				kill_pidfile_s("/var/run/usbled.pid", SIGTSTP); // inform usbled to reset status
#endif
#if defined(RTAC1200G) || defined(RTAC1200GP)
				eval("et", "robowr", "0", "0x18", "0x01ff");	// lan/wan ethernet/giga led
				eval("et", "robowr", "0", "0x1a", "0x01ff");
				eval("wl", "-i", "eth1", "ledbh", "3", "7");
				eval("wl", "-i", "eth2", "ledbh", "11", "7");
				kill_pidfile_s("/var/run/usbled.pid", SIGTSTP); // inform usbled to reset status
#endif
#ifdef RTCONFIG_QCA
				led_control(LED_2G, LED_ON);
#if defined(RTCONFIG_HAS_5G)
				led_control(LED_5G, LED_ON);
#if defined(RTCONFIG_HAS_5G_2)
				led_control(LED_5G2, LED_ON);
#endif	/* RTCONFIG_HAS_5G_2 */
#endif	/* RTCONFIG_HAS_5G */
#endif	/* RTCONFIG_QCA */
				wigig_led_control(LED_ON);
#ifdef RTCONFIG_LAN4WAN_LED
				LanWanLedCtrl();
#endif
			}
			else {
				TRACE_PT("LED turn off\n");
				setAllLedOff();
			}

			/* check LED_WAN status */
			kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
			return;
		}
	}
#endif
#endif	/* RTCONFIG_WPS_RST_BTN */

#ifdef RTCONFIG_LED_BTN
	LED_status_old = LED_status;
	LED_status = button_pressed(BTN_LED);

#if defined(RTAC68U) || defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
#if defined(RTAC68U)
	if (is_ac66u_v2_series())
		;
	else if (nvram_match("cpurev", "c0") && nvram_get_int("PA") != 5023) {
		if (!LED_status &&
		    (LED_status != LED_status_old))
		{
			LED_status_changed = 1;
			if (LED_status_first)
			{
				LED_status_first = 0;
				LED_status_on = 0;
			}
			else
				LED_status_on = 1 - LED_status_on;
		}
		else
			LED_status_changed = 0;
	} else {
		LED_status_changed = 0;
		if (LED_status != LED_status_old)
		{
			if (LED_status_first)
			{
				LED_status_first = 0;
				LED_status_on = LED_status;
			}
			else
				LED_status_changed = 1;
		}
	}
#elif defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
	if (!LED_status &&
	    (LED_status != LED_status_old))
	{
		LED_status_changed = 1;
		if (LED_status_first)
		{
			LED_status_first = 0;
			LED_status_on = 0;
		}
		else
			LED_status_on = 1 - LED_status_on;
	}
	else
		LED_status_changed = 0;
#endif

	if (LED_status_changed)
	{
		TRACE_PT("button BTN_LED pressed\n");
#if defined(RTAC68U)
		if (((!nvram_match("cpurev", "c0") || nvram_get_int("PA") == 5023) && LED_status == LED_status_on) ||
		      (nvram_match("cpurev", "c0") && nvram_get_int("PA") != 5023 && LED_status_on))
#elif defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
		if (LED_status_on)
#endif
			nvram_set_int("AllLED", 1);
		else
			nvram_set_int("AllLED", 0);
#if defined(RTAC68U)
		if (((!nvram_match("cpurev", "c0") || nvram_get_int("PA") == 5023) && LED_status == LED_status_on) ||
		      (nvram_match("cpurev", "c0") && nvram_get_int("PA") != 5023 && LED_status_on))
#elif defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
		if (LED_status_on)
#endif
		{
			led_control(LED_POWER, LED_ON);

#if defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
			kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
#if defined(HND_ROUTER) && defined(RTCONFIG_LAN4WAN_LED)
			setLANLedOn();
#endif
#else
#ifdef RTAC68U
			if (is_ac66u_v2_series())
				kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
			else
#endif
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01ff");
#endif

			if (wlonunit == -1 || wlonunit == 0) {
#ifdef RTAC68U
				eval("wl", "ledbh", "10", "7");
#elif defined(RTAC3200)
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
#elif defined(GTAC5300)
				eval("wl", "-i", "eth6", "ledbh", "9", "7");
#elif defined(RTCONFIG_BCM_7114) || defined(RTAC86U) || defined(AC2900)
				eval("wl", "ledbh", "9", "7");
#endif
			}
			if (wlonunit == -1 || wlonunit == 1) {
#ifdef RTAC68U
				eval("wl", "-i", "eth2", "ledbh", "10", "7");
#elif defined(RTAC3200)
				eval("wl", "ledbh", "10", "7");
#elif defined(GTAC5300)
				eval("wl", "-i", "eth7", "ledbh", "9", "7");
#elif defined(RTAC86U) || defined(AC2900)
				eval("wl", "-i", "eth6", "ledbh", "9", "7");
#elif defined(RTCONFIG_BCM_7114)
				eval("wl", "-i", "eth2", "ledbh", "9", "7");
#endif
			}
#if defined(RTAC3200) || defined(RTAC5300) || defined(GTAC5300)
			if (wlonunit == -1 || wlonunit == 2) {
#if defined(RTAC3200)
				eval("wl", "-i", "eth3", "ledbh", "10", "7");
#elif defined(GTAC5300)
				eval("wl", "-i", "eth8", "ledbh", "9", "7");
#elif defined(RTAC5300)
				eval("wl", "-i", "eth3", "ledbh", "9", "7");
#endif
			}
#endif
#ifdef RTCONFIG_LOGO_LED
			led_control(LED_LOGO, LED_ON);
#endif
			kill_pidfile_s("/var/run/usbled.pid", SIGTSTP); // inform usbled to reset status
		}
		else
			setAllLedOff();
	}
#elif defined(RTAC87U)
	if (LED_status)
		++BTN_pressed_count;
	else{
		BTN_pressed_count = 0;
		LED_status_changed = 0;
	}

	if (BTN_pressed_count >= LED_switch_count && LED_status_changed == 0) {
		LED_status_changed = 1;
		LED_status_on = nvram_get_int("AllLED");

		if (LED_status_on)
			nvram_set_int("AllLED", 0);
		else
			nvram_set_int("AllLED", 1);
		LED_status_on = !LED_status_on;

		if (LED_status_on) {
			led_control(LED_POWER, LED_ON);
#if defined(RTAC3200) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
#ifndef HND_ROUTER
			eval("et", "-i", "eth0", "robowr", "0", "0x18", "0x01ff");
			eval("et", "-i", "eth0", "robowr", "0", "0x1a", "0x01ff");
#else
			led_control(LED_WAN_NORMAL, LED_ON);
#endif
#else
			eval("et", "robowr", "0", "0x18", "0x01ff");
			eval("et", "robowr", "0", "0x1a", "0x01ff");
#endif
			qcsapi_wifi_run_script("router_command.sh", "lan4_led_ctrl on");

			if (nvram_match("wl0_radio", "1"))
				eval("wl", "ledbh", "10", "7");
			if (nvram_match("wl1_radio", "1")) {
				qcsapi_wifi_run_script("router_command.sh", "wifi_led_on");
				qcsapi_led_set(1, 1);
			}

#ifdef RTCONFIG_EXT_LED_WPS
			led_control(LED_WPS, LED_OFF);
#else
			led_control(LED_WPS, LED_ON);
#endif

			kill_pidfile_s("/var/run/usbled.pid", SIGTSTP); // inform usbled to reset status
		}
		else
			setAllLedOff();

		/* check LED_WAN status */
		kill_pidfile_s("/var/run/wanduck.pid", SIGUSR2);
	}
#endif
#endif	/* RTCONFIG_LED_BTN */

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	if ((psta_exist() || psr_exist())
		&& !dpsr_mode()
#ifdef RTCONFIG_DPSTA
		&& !dpsta_mode()
#endif
	)
		return;
#endif

	if (btn_pressed_setup < BTNSETUP_START)
	{
#ifdef RTCONFIG_WPS_RST_BTN
		if (btn_pressed_setup == BTNSETUP_DETECT)
#else
		if (button_pressed(BTN_WPS) &&
		    !no_need_to_start_wps() &&
		    !wps_band_radio_off(get_radio_band(0)) &&
		    !wps_band_ssid_broadcast_off(get_radio_band(0)) &&
#ifndef RTCONFIG_WIFI_TOG_BTN
		    nvram_match("btn_ez_radiotoggle", "0") &&
#endif
#ifdef RTCONFIG_WPS_ALLLED_BTN
		    nvram_match("btn_ez_mode", "0") &&
#endif
		    !nvram_match("wps_ign_btn", "1"))
#endif	/* ! RTCONFIG_WPS_RST_BTN */
		{
			if (nvram_match("wps_enable", "1")) {
				TRACE_PT("button WPS pressed\n");
#ifdef BLUECAVE
				bc_wps_led = 0;
#endif
				if (btn_pressed_setup == BTNSETUP_NONE)
				{
					btn_pressed_setup = BTNSETUP_DETECT;
					btn_count_setup = 0;
					alarmtimer(0, RUSHURGENT_PERIOD);
				}
				else
				{	/* Whenever it is pushed steady */
					if (++btn_count_setup > WPS_WAIT_COUNT)
					{
						btn_pressed_setup = BTNSETUP_START;
						btn_count_setup = 0;
						btn_count_setup_second = 0;
						nvram_set("wps_ign_btn", "1");
#ifdef RTCONFIG_WIFI_CLONE
#ifdef RTCONFIG_WIFI_SON
						if(((sw_mode() == SW_MODE_AP) && !nvram_match("cfg_master", "1") && nvram_get_int("x_Setting")) ||
						   ((sw_mode() == SW_MODE_ROUTER ) && !nvram_get_int("x_Setting"))) { //Range extender
							doSystem("killall wifimon_check");
							doSystem("killall wpa_supplicant");

							if(nvram_get_int("dfs_check_period"))
								doSystem("iwpriv wifi1 staDFSEn 1");

#ifdef RTCONFIG_DUAL_BACKHAUL
							doSystem("ifconfig %s up",get_staifname(0));
#endif
							doSystem("ifconfig %s up",get_staifname(1));

#else
						if (sw_mode() == SW_MODE_ROUTER
							|| sw_mode() == SW_MODE_AP) {
#endif
							nvram_set("wps_enrollee", "1");
							nvram_set("wps_e_success", "0");
						}
#ifdef RTCONFIG_WIFI_SON
						else
							nvram_set("wps_enrollee", "0");
#endif
#if (defined(PLN12) || defined(PLAC56))
						set_wifiled(3);
#elif defined(MAPAC1750)
						set_rgbled(RGBLED_BLUE_3ON1OFF);
#endif
#endif // RTCONFIG_WIFI_CLONE

#if 0
						start_wps_pbc(0);	// always 2.4G
#else
#ifdef RTCONFIG_REALTEK
						if (sw_mode() == SW_MODE_AP && nvram_get_int("wlc_psta") == 0) { // AP mode
							if (nvram_get_int("wps_band_x") != 0) { // always 2.4G
								nvram_set_int("wps_restart", 1);
								stop_wps();
								nvram_set_int("wps_band_x", 0);
								start_wps();
							}
						}
#endif
						kill_pidfile_s("/var/run/wpsaide.pid", SIGTSTP);
#endif
						wsc_timeout = WPS_TIMEOUT_COUNT;
#if defined(RTCONFIG_CONCURRENTREPEATER) && defined(RTCONFIG_MTK_REP)
						if(sw_mode() == SW_MODE_AP)
								nvram_set_int("led_status", LED_AP_WPS_START);
#endif
					}
				}
			} else {
				TRACE_PT("button WPS pressed, skip\n");
			}
		}
		else if (btn_pressed_setup == BTNSETUP_DETECT)
		{
			btn_pressed_setup = BTNSETUP_NONE;
#ifndef RTCONFIG_WPS_LED
#ifdef BLUECAVE
#else
			wps_led_control(LED_ON);
#endif
#endif
			btn_count_setup = 0;
			alarmtimer(NORMAL_PERIOD, 0);
		}
#ifdef RTCONFIG_WPS_LED
		else
		{
			if (nvram_match("wps_success", "1") && ++btn_count_setup_second > WPS_SUCCESS_COUNT) {
				btn_count_setup_second = 0;
				nvram_set("wps_success", "0");
				__wps_led_control(LED_OFF);
			}
		}
#endif
	}
	else
	{
		if (!nvram_match("wps_ign_btn", "1")) {
#ifndef RTCONFIG_WPS_RST_BTN
			if (button_pressed(BTN_WPS) &&
			    !no_need_to_start_wps() &&
			    !wps_band_radio_off(get_radio_band(0)) &&
			    !wps_band_ssid_broadcast_off(get_radio_band(0)))
			{
				/* Whenever it is pushed steady, again... */
				if (++btn_count_setup_second > WPS_WAIT_COUNT)
				{
					btn_pressed_setup = BTNSETUP_START;
					btn_count_setup_second = 0;
					nvram_set("wps_ign_btn", "1");
#if 0
					start_wps_pbc(0);	// always 2.4G
#else
#ifdef RTCONFIG_REALTEK
					if (sw_mode() == SW_MODE_AP && nvram_get_int("wlc_psta") == 0) { // AP mode
						if (nvram_get_int("wps_band_x") != 0) { // always 2.4G
							nvram_set_int("wps_restart", 1);
							stop_wps();
							nvram_set_int("wps_band_x", 0);
							start_wps();
						}
					}
#endif
					kill_pidfile_s("/var/run/wpsaide.pid", SIGTSTP);
#endif
					wsc_timeout = WPS_TIMEOUT_COUNT;
				}
			}
#endif	/* ! RTCONFIG_WPS_RST_BTN */

			if (is_wps_stopped() || --wsc_timeout == 0)
			{
#if defined(HND_ROUTER) && defined(RTCONFIG_PROXYSTA)
				if (!nvram_get_int("wps_band_x") && (is_dpsr(nvram_get_int("wps_band_x"))
#ifdef RTCONFIG_DPSTA
					|| is_dpsta(nvram_get_int("wps_band_x"))
#endif
				))
				eval("wl", "spatial_policy", "1");
#endif
				wsc_timeout = 0;

				btn_pressed_setup = BTNSETUP_NONE;
				btn_count_setup = 0;
				btn_count_setup_second = 0;
#ifdef BLUECAVE
				nvram_set("bc_ledbh", "wps_done");
#endif

#if defined(RTCONFIG_LP5523)
//				lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_PREVIOUS_STATE);
#else
				led_control_normal();
#endif // RTCONFIG_LP5523

				alarmtimer(NORMAL_PERIOD, 0);
#if defined(RTCONFIG_CONCURRENTREPEATER)
				nvram_set_int("led_status", LED_WPS_FAIL);
#endif

#if defined(RTCONFIG_QCA)
				if (nvram_match("wps_enrollee", "1"))
					stop_wps_method();
#endif
#ifdef RTCONFIG_WIFI_CLONE
				if (nvram_match("wps_e_success", "1")) {
#if (defined(PLN12) || defined(PLAC56))
					set_wifiled(2);
#endif

#ifdef RTCONFIG_WIFI_SON
					if (sw_mode() == SW_MODE_ROUTER) //default
					{
						_dprintf("=> switch router to RE mode.\n");
						nvram_set("lan_proto", "dhcp");
						nvram_set("lan_dnsenable_x", "1");
						nvram_set("w_Setting", "1");
						nvram_set("x_Setting", "1");
						nvram_set_int("sw_mode", SW_MODE_AP);
						nvram_set("qis_Setting", "1");
						nvram_unset("cfg_master");
						nvram_commit();
						_eval(argv, NULL, 0, &pid);
						if (pids("bluetoothd"))
							notify_rc_and_wait("stop_bluetooth_service");

						uptime_wait(22); //RE estimate time
					}
					else
					{
						_dprintf("=> run RE process\n");
						nvram_set("qis_Setting", "1");
						nvram_commit();
						start_re(0);
					}

#else
					notify_rc("restart_wireless");
#endif
				}
#if (defined(PLN12) || defined(PLAC56))
				else
					set_wifiled(1);
#elif defined(RTCONFIG_WIFI_SON)
				else
				{
					if((sw_mode() == SW_MODE_AP) && nvram_get_int("x_Setting")) //Range extender
					{
						pid_t pid;
						_dprintf("=>wifimon is back\n");
						char *wifimon[]={"wifimon_check",NULL};
						if (!pids(wifimon[0]))
						_eval(wifimon, NULL, 0, &pid);
					}
				}
#endif
#endif // RTCONFIG_WIFI_CLONE

#ifdef RTCONFIG_WIFI_SON
#ifdef RTCONFIG_WPS_ENROLLEE
				if (nvram_match("wps_enrollee", "0")) //CAP
#endif
					uptime_wait(30); //CAP estimate time
#if defined(RTCONFIG_LP5523)
				lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_PREVIOUS_STATE);
#elif defined(MAPAC1750)
				if (pids("bluetoothd") && wsc_timeout == 0)
					set_rgbled(RGBLED_WHITE);
				else
					nvram_set("prelink_pap_status", "-1");
#endif
#endif

#ifdef RTCONFIG_CFGSYNC
				send_event_to_cfgmnt(EID_RC_WPS_STOP);
#endif
				return;
			}
		}

		++btn_count_setup;
		btn_count_setup = (btn_count_setup % 20);

		/* 0123456789 */
		/* 1010101010 */
#if defined(RTCONFIG_LP5523)
		if (btn_count_setup == 1)
			lp55xx_leds_proc(LP55XX_GREENERY_LEDS, LP55XX_WPS_PARAM_SYNC);
#elif defined(MAPAC1750)
#elif defined(BLUECAVE)
		if (!bc_wps_led) {
			bc_wps_led = 1;
			nvram_set("bc_ledbh", "wps");
			kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
		}
#else
		if ((btn_count_setup % 2) == 0 && (btn_count_setup > 10))
			wps_led_control(LED_ON);
		else
			wps_led_control(LED_OFF);
#endif
#if defined(RTCONFIG_CONCURRENTREPEATER) && defined(RTCONFIG_MTK_REP)
		if(sw_mode() == SW_MODE_AP)
			nvram_set_int("led_status", LED_AP_WPS_START);
#endif
	}
#endif	/* BTN_SETUP */
}

#define DAYSTART (0)
#define DAYEND (60*60*23 + 60*59 + 60) // 86400
static int in_sched(int now_mins, int now_dow, int (*enableTime)[24])
{
	int currentTime;
	int x=0,y=0;
	int tableTimeStart, tableTimeEnd;
	// 8*60*60 = AM0.00~AM8.00
	currentTime = (now_mins*60) + now_dow*DAYEND;

	for(;x<7;x++)
	{
		y=0;
		for(;y<24;y++)
		{
			if (enableTime[x][y] == 1)
			{
				tableTimeStart = x*DAYEND + y*60*60;
				tableTimeEnd = x*DAYEND + (y+1)*60*60;

				if (y == 23)
				tableTimeEnd = tableTimeEnd - 1; //sunday = 0 ~ 86399
				WL_SCHED_DBG("tableTimeStart=%d, tableTimeEnd=%d, currentTime=%d\n",tableTimeStart,tableTimeEnd,currentTime);

				if (tableTimeStart<=currentTime && currentTime < tableTimeEnd)
				{
					return 1;
				}
			}
		}
	}
	return 0;
}

int timecheck_item(char *activeTime)
{
	int current, active;
	int now_dow;
	time_t now;
	struct tm *tm;
	char Date[] = "XX";
	char startTime[] = "XX";
	char endTime[] = "XX";
	int tableAllOn = 0;	// 0: wifi time all off 1: wifi time all on 2: check&calculate wifi open slot
	int schedTable[7][24];
	int x=0, y=0, z=0;	//for loop usage

	/* current router time */
	setenv("TZ", nvram_safe_get("time_zone_x"), 1);
	time(&now);
	tm = localtime(&now);
	now_dow = tm->tm_wday;
	current = tm->tm_hour * 60 + tm->tm_min; //minutes

	/* initial time table */
	x=0;
	for(;x<7;x++)
	{
		y=0;
		for(;y<24;y++)
		{
			schedTable[x][y]=0;
		}
	 }

	active = 0;
	if (activeTime[0] == '0' && activeTime[1] == '0' && activeTime[2] == '0' && activeTime[3] == '0'
		&& activeTime[4] == '0' && activeTime[5] == '0')
	{
		tableAllOn = 1; // all open
	}
	else if (!strcmp(activeTime, ""))
	{
		tableAllOn = 0; // all close
	}
	else
	{
		tableAllOn = 2;
		/* Counting variables quantity*/
		x=0;
		int schedCount = 1; // how many variables in activeTime	111014<222024<331214 count will be 3
		for(;x<strlen(activeTime);x++)
		{
			if (activeTime[x] == '<')
			schedCount++;
		}

		/* analyze for sched Date, startTime, endTime*/
		x=0;
		int loopCount=0;
		for(;x<schedCount;x++) // start for loop (schedCount)
		{
			do
			{
				if (loopCount < (2 + (7*x)))
				{
					Date[loopCount-7*x] = activeTime[loopCount];
				}
				else if (loopCount < (4 + (7*x)))
				{
					startTime[loopCount - (2+7*x)] = activeTime[loopCount];
				}
				else
				{
					endTime[loopCount - (4+7*x)] = activeTime[loopCount];
					if ((loopCount - (4+7*x)) == 1&& atoi(endTime) == 0)
					{
						endTime[0] = '2';
						endTime[1] = '4';

						if (Date[1] == '0')
							Date[1] = '6';
						else
							Date[1]=Date[1]-1;
					}
				}
				loopCount++;
			} while(activeTime[loopCount] != '<' && loopCount < strlen(activeTime));

			loopCount++;

			/*Check which time will enable or disable wifi radio*/
			int offSet=0;
			if (Date[0] == Date[1])
			{
				z=0;
				for(;z<(atoi(endTime) - atoi(startTime));z++)
					schedTable[Date[0]-'0'][atoi(startTime)+z] = 1;
			}
			else
			{
				z=0;
				for(;z<((atoi(endTime) - atoi(startTime))+24*(Date[1] - Date[0]));z++)
				{
					schedTable[Date[0]-'0'+offSet][(atoi(startTime)+z)%24] = 1;
					if ((atoi(startTime)+z)%24 == 23)
					{
						offSet++;
					}
				}
			}
		}//end for loop (schedCount)
	}

	if (tableAllOn == 0)
		active = 0;
	else if (tableAllOn == 1)
		active = 1;
	else
		active = in_sched(current, now_dow, schedTable);

	WL_SCHED_DBG("[wifi-scheduler] active=%d\n", active);
	return active;
}


int timecheck_reboot(char *activeSchedule)
{
	int active, current_time, current_date, Time2Active, Date2Active;
	time_t now;
	struct tm *tm;
	int i;

	setenv("TZ", nvram_safe_get("time_zone_x"), 1);

	time(&now);
	tm = localtime(&now);
	current_time = tm->tm_hour * 60 + tm->tm_min;
	current_date = 1 << (6-tm->tm_wday);
	active = 0;
	Time2Active = 0;
	Date2Active = 0;

	Time2Active = ((activeSchedule[7]-'0')*10 + (activeSchedule[8]-'0'))*60 + ((activeSchedule[9]-'0')*10 + (activeSchedule[10]-'0'));

	for(i=0;i<=6;i++) {
		Date2Active += (activeSchedule[i]-'0') << (6-i);
	}

	if ((current_time == Time2Active) && (Date2Active & current_date))	active = 1;

	//dbG("[watchdog] current_time=%d, ActiveTime=%d, current_date=%d, ActiveDate=%d, active=%d\n",
	//	current_time, Time2Active, current_date, Date2Active, active);

	return active;
}


int svcStatus[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

/* Check for time-reated service 	*/
/* 1. Wireless Radio			*/
/* 2. Guess SSID expire time 		*/
/* 3. Reboot Schedule			*/
//int timecheck(int argc, char *argv[])
void timecheck(void)
{
	int activeNow;
	char schedTime[2048];
	char prefix[]="wlXXXXXX_", tmp[100], tmp2[100];
	char word[256], *next;
	int unit, item;
	char lan_ifname[16];
	char wl_vifs[256], nv[40];
	int expire, need_commit = 0;

#ifndef CONFIG_BCMWL5
#if defined(RTCONFIG_PROXYSTA)
	if (mediabridge_mode())
		return;
#endif
#endif
	WL_SCHED_DBG("[wifi-scheduler] start to timecheck()...\n");

	item = 0;
	unit = 0;

	if (nvram_match("reload_svc_radio", "1"))
	{
		nvram_set("reload_svc_radio", "0");

		foreach (word, nvram_safe_get("wl_ifnames"), next) {
			SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
			svcStatus[item] = -1;
			item++;
			unit++;
		}

		item = 0;
		unit = 0;
	}

	// radio on/off
	if (nvram_match("svc_ready", "1") && nvram_match("wlready", "1"))
	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(unit);
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		//dbG("[watchdog] timecheck unit=%s radio=%s, timesched=%s\n", prefix, nvram_safe_get(strcat_r(prefix, "radio", tmp)), nvram_safe_get(strcat_r(prefix, "timesched", tmp2))); // radio toggle test
		if (nvram_match(strcat_r(prefix, "radio", tmp), "0") ||
			nvram_match(strcat_r(prefix, "timesched", tmp2), "0")) {
			item++;
			unit++;
			continue;
		}

		/*transfer wl_sched NULL value to 000000 value, because
		of old version firmware with wrong default value*/
		if(!strcmp(nvram_safe_get("wl_sched"), "") || !strcmp(nvram_safe_get(strcat_r(prefix, "sched", tmp)), ""))
		{
			nvram_set(strcat_r(prefix, "sched", tmp),"000000");
			nvram_set("wl_sched", "000000");
		}

		/*transfer wl_sched NULL value to 000000 value, because
		of old version firmware with wrong default value*/
		if (!strcmp(nvram_safe_get(strcat_r(prefix, "sched", tmp)), ""))
		{
			nvram_set(strcat_r(prefix, "sched", tmp),"000000");
			//nvram_set("wl_sched", "000000");
		}

		snprintf(schedTime, sizeof(schedTime), "%s", nvram_safe_get(strcat_r(prefix, "sched", tmp)));

		activeNow = timecheck_item(schedTime);
		snprintf(tmp, sizeof(tmp), "%d", unit);

		WL_SCHED_DBG("[wifi-scheduler] unit=%d, activeNow=%d\n", unit, activeNow);

		if (svcStatus[item] != activeNow) {
#ifdef RTCONFIG_QCA
			if (match_radio_status(unit, activeNow)) {
				svcStatus[item] = activeNow;
				item++;
				unit++;
				continue;
			}
#else
			svcStatus[item] = activeNow;
#endif
			if (activeNow == 1) eval("radio", "on", tmp);
			else eval("radio", "off", tmp);
			WL_SCHED_DBG("[wifi-scheduler] change radio ...\n");
		}
		item++;
		unit++;

	}

	// guest ssid expire check
	if ((sw_mode() != SW_MODE_REPEATER) &&
		(strlen(nvram_safe_get("wl0_vifs")) || strlen(nvram_safe_get("wl1_vifs")) ||
		 strlen(nvram_safe_get("wl2_vifs"))))
	{
		snprintf(lan_ifname, sizeof(lan_ifname), "%s", nvram_safe_get("lan_ifname"));
		sprintf(wl_vifs, "%s %s %s", nvram_safe_get("wl0_vifs"), nvram_safe_get("wl1_vifs"), nvram_safe_get("wl2_vifs"));

		foreach (word, wl_vifs, next) {
			snprintf(nv, sizeof(nv) - 1, "%s_expire_tmp", wif_to_vif (word));
			expire = nvram_get_int(nv);

			if (expire)
			{
				if (expire <= 30)
				{
					nvram_set(nv, "0");
					snprintf(nv, sizeof(nv) - 1, "%s_bss_enabled", wif_to_vif (word));
					nvram_set(nv, "0");
					if (!need_commit) need_commit = 1;
#ifdef CONFIG_BCMWL5
					eval("wl", "-i", word, "closed", "1");
					eval("wl", "-i", word, "bss_maxassoc", "1");
					eval("wl", "-i", word, "bss", "down");
#endif
					ifconfig(word, 0, NULL, NULL);
					eval("brctl", "delif", lan_ifname, word);
#ifdef RTCONFIG_WIFI_SON
					eval("brctl", "delif", BR_GUEST, word);
					char *tmp_str = strdup(nvram_safe_get("lan_ifnames"));
					if (tmp_str) {
						if (remove_word(tmp_str, word)) {
							trim_space(tmp_str);
							nvram_set("lan_ifnames", tmp_str);
						}
						free(tmp_str);
					}
					doSystem("wlanconfig %s destroy", word);
					/* restart hyd */
					eval("hive_hyd");
#endif
				}
				else
				{
					expire -= 30;
					sprintf(tmp, "%d", expire);
					nvram_set(nv, tmp);
				}
			}
		}

		if (need_commit)
		{
			need_commit = 0;
			nvram_commit();
		}
	}

#ifdef RTCONFIG_REBOOT_SCHEDULE
	/* Reboot Schedule */
	char reboot_schedule[PATH_MAX];
	if (nvram_match("ntp_ready", "1") && nvram_match("reboot_schedule_enable", "1"))
	{
		//SMTWTFSHHMM
		//XXXXXXXXXXX
		snprintf(reboot_schedule, sizeof(reboot_schedule), "%s", nvram_safe_get("reboot_schedule"));
		if (strlen(reboot_schedule) == 11 && atoi(reboot_schedule) > 2359)
		{
			if (timecheck_reboot(reboot_schedule))
			{
				if (shutdown_start("watchdog", __FUNCTION__, "reboot_schedule") == 0) {
				_dprintf("reboot plan alert...\n");
				sleep(1);
				eval("reboot");
				}
			}
		}
	}
#endif

#if defined(MAPAC1300) || defined(MAPAC2200) || defined(VZWAC1300)
#if defined(RTCONFIG_LP5523)
	// lp55xx led schedule
	int lp55xx_sch_enable = nvram_get_int("lp55xx_lp5523_sch_enable");
	if (lp55xx_sch_enable > 0 && nvram_match("x_Setting", "1"))
	{
		snprintf(schedTime, sizeof(schedTime), "%s", nvram_safe_get("lp55xx_lp5523_sch"));
		activeNow = timecheck_item(schedTime);

		if (activeNow == 1)
		{
			if (lp55xx_sch_enable==1)
			{
				nvram_set_int("lp55xx_lp5523_sch_enable", 2);
				lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_SCH_ENABLE);
			}
		}
		else
		{
			if (lp55xx_sch_enable!=1)
			{
				nvram_set_int("lp55xx_lp5523_sch_enable", 1);
				lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_PREVIOUS_STATE);
			}
		}
	}
#endif
#endif

	return;
}


static void chld_reap_local(int sig)
{
	chld_reap(sig);
}

#ifdef RTCONFIG_RALINK
int need_restart_wsc = 0;
#endif

static void catch_sig(int sig)
{
#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
	dbG("watchdog: skip catch_sig(), sig=[%d]\n", sig);
	return;
#endif

	if (sig == SIGUSR1)
	{
		dbG("[watchdog] Handle WPS LED for WPS Start\n");

		alarmtimer(NORMAL_PERIOD, 0);

		btn_pressed_setup = BTNSETUP_START;
		btn_count_setup = 0;
		btn_count_setup_second = 0;
#ifdef RTCONFIG_AMAS
		if (nvram_get_int("amesh_led"))
		wsc_timeout = AMESH_TIMEOUT_COUNT;
		else
#endif
		wsc_timeout = WPS_TIMEOUT_COUNT;
		alarmtimer(0, RUSHURGENT_PERIOD);

#if (defined(PLN12) || defined(PLAC56))
		set_wifiled(3);
#endif
	}
	else if (sig == SIGUSR2)
	{
		dbG("[watchdog] Handle WPS LED for WPS Stop\n");

		if (nvram_match("wps_ign_btn", "1")) {
			dbG("[watchdog] ignore SIGUSR2 for wps_ign_btn is set\n");
			return;
		}

		btn_pressed_setup = BTNSETUP_NONE;
		btn_count_setup = 0;
		btn_count_setup_second = 0;
		wsc_timeout = 1;
		alarmtimer(NORMAL_PERIOD, 0);
#if (defined(PLN12) || defined(PLAC56))
		set_wifiled(1);
#else
		led_control_normal();
#endif
	}
	else if (sig == SIGTSTP)
	{
		dbG("[watchdog] Reset WPS timeout count\n");

		wsc_timeout = WPS_TIMEOUT_COUNT;
	}
#if defined(RTAC1200G) || defined(RTAC1200GP)
	else if (sig == SIGHUP)
	{
		dbG("[watchdog] Reset alarm timer...\n");
		alarmtimer(NORMAL_PERIOD, 0);
	}
#endif
#ifdef RTCONFIG_RALINK
	else if (sig == SIGTTIN)
	{
		wsc_user_commit();
		need_restart_wsc = 1;
	}
#endif
}


/* simple traffic-led hints */
unsigned long get_devirq_count(char *irqs)
{
	FILE *f;
	char buf[256];
	char *irqname, *p;
	unsigned long counter1, counter2;

	if ((f = fopen("/proc/interrupts", "r")) == NULL) return -1;

	fgets(buf, sizeof(buf), f);

	counter1=counter2=0;

	while (fgets(buf, sizeof(buf), f)) {
		if ((p=strchr(buf, ':')) == NULL) continue;
		*p = 0;
		if ((irqname = strrchr(buf, ' ')) == NULL) irqname = buf;
		else ++irqname;

		if (strcmp(irqname, irqs)) continue;

#ifdef RTCONFIG_BCMSMP
		if (sscanf(p+1, "%lu%lu", &counter1, &counter2) != 2) continue;
#else
		if (sscanf(p+1, "%lu", &counter1) != 1) continue;
#endif

	}
	fclose(f);

	return counter1+counter2 > 2 ? counter1+counter2 : 0;
}

#if !defined(RTCONFIG_BLINK_LED)
void fake_dev_led(char *irqs, unsigned int LED_WHICH)
{
	static unsigned int blink_dev_check = 0;
	static unsigned int blink_dev = 0;
	static unsigned int data_dev = 0;
	unsigned long count_dev;
	int i;
	static int j;
	static int status = -1;
	static int status_old;

	if (!*irqs || !LED_WHICH)
		return;

	// check data per 10 count
	if ((blink_dev_check%10) == 0) {
		count_dev = get_devirq_count(irqs);
		if (count_dev && data_dev != count_dev) {
			blink_dev = 1;
			data_dev = count_dev;
		}
		else blink_dev = 0;
		led_control(LED_WHICH, LED_ON);
	}

	if (blink_dev) {
		j = rand_seed_by_time() % 3;
		for(i=0;i<10;i++) {
			usleep(33*1000);

			status_old = status;
			if (((i%2) == 0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_WHICH, LED_ON);
				else
					led_control(LED_WHICH, LED_OFF);
			}
		}
		led_control(LED_WHICH, LED_ON);
	}

	blink_dev_check++;
}
#endif

#ifdef RTCONFIG_FAKE_ETLAN_LED
unsigned long get_etlan_count()
{
	FILE *f;
	char buf[256];
	char *ifname, *p;
	unsigned long counter=0;
#ifdef GTAC5300
	unsigned long tmpcnt=0;
#endif

	if ((f = fopen("/proc/net/dev", "r")) == NULL) return -1;

	fgets(buf, sizeof(buf), f);
	fgets(buf, sizeof(buf), f);

	while (fgets(buf, sizeof(buf), f)) {
		if ((p=strchr(buf, ':')) == NULL) continue;
		*p = 0;
		if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
		else ++ifname;

#ifdef GTAC5300
		if (strcmp(ifname, "eth1") && strcmp(ifname, "eth2") && strcmp(ifname, "eth3") && strcmp(ifname, "eth4") && strcmp(ifname, "eth5")) continue;
		if (sscanf(p+1, "%lu", &tmpcnt) != 1) continue;
		counter += tmpcnt;
#else
		if (strcmp(ifname, "vlan1")) continue;
		if (sscanf(p+1, "%lu", &counter) != 1) continue;
#endif

	}
	fclose(f);

	return counter;
}

#ifdef GTAC5300
enum {
	AGGLED_ACT_ALLOFF,
	AGGLED_ACT_ALLON,
	AGGLED_ACT_EXTOFF
};

extern unsigned long _memaccess(int argc, char *argv[]);

void
aggled_control(int mode)
{
	//printf("aggled ctrl: change to mode %d\n", mode);
	switch(mode) {
	case AGGLED_ACT_ALLOFF:
		_memaccess(elm_of_strr(sw_aggled_alloff), (char **) sw_aggled_alloff);
		break;
	case AGGLED_ACT_ALLON:
		_memaccess(elm_of_strr(sw_aggled_allon), (char **) sw_aggled_allon);
		break;
	case AGGLED_ACT_EXTOFF:
		_memaccess(elm_of_strr(sw_aggled_extoff), (char **) sw_aggled_extoff);
		break;
	}
}
#endif

static int lstatus = 0;
static int allstatus = 0;
void fake_etlan_led(void)
{
#ifndef GTAC5300
	static unsigned int blink_etlan_check = 0;
	static unsigned int blink_etlan = 0;
	static unsigned int data_etlan = 0;
	unsigned long count_etlan;
	int i;
	static int j;
#endif
	static int status = -1;
	static int status_old;

#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	if (nvram_match("AllLED", "0")) {
		if (allstatus)
			led_control(LED_LAN, LED_OFF);
		allstatus = 0;
		return;
	}
	allstatus = 1;
#endif

	if (!GetPhyStatus(0)) {
		if (lstatus)
#ifdef GTAC5300
			aggled_control(AGGLED_ACT_ALLOFF);
#else
			led_control(LED_LAN, LED_OFF);
#endif
		lstatus = 0;
		status = -1;
		return;
	}
	lstatus = 1;

#ifdef GTAC5300
	status_old = status;
	status = GetPhyStatus(53134);
	if (status != status_old || status_old == -1) {
		if (status)
			aggled_control(AGGLED_ACT_ALLON);
		else
			aggled_control(AGGLED_ACT_EXTOFF);
	}
#else
	// check data per 10 count
	if ((blink_etlan_check%10) == 0) {
		count_etlan = get_etlan_count();
		if (count_etlan && data_etlan != count_etlan) {
			blink_etlan = 1;
			data_etlan = count_etlan;
		}
		else blink_etlan = 0;
		led_control(LED_LAN, LED_ON);
	}

	if (blink_etlan) {
		j = rand_seed_by_time() % 3;
		for(i=0;i<10;i++) {
			usleep(33*1000);

			status_old = status;
			if (((i%2) == 0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_LAN, LED_ON);
				else
					led_control(LED_LAN, LED_OFF);
			}
		}
		led_control(LED_LAN, LED_ON);
	}

	blink_etlan_check++;
#endif
}
#endif

#if defined(RTCONFIG_WLAN_LED) || defined(RTN18U)
unsigned long get_2g_count()
{
	FILE *f;
	char buf[256];
	char *ifname, *p;
	unsigned long counter1, counter2;

	if ((f = fopen("/proc/net/dev", "r")) == NULL) return -1;

	fgets(buf, sizeof(buf), f);
	fgets(buf, sizeof(buf), f);

	counter1=counter2=0;

	while (fgets(buf, sizeof(buf), f)) {
		if ((p=strchr(buf, ':')) == NULL) continue;
		*p = 0;
		if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
		else ++ifname;

		if (strcmp(ifname, "eth1")) continue;

		if (sscanf(p+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%lu", &counter1, &counter2) != 2) continue;

	}
	fclose(f);

	return counter1;
}

void fake_wl_led_2g(void)
{
	static unsigned int blink_2g_check = 0;
	static unsigned int blink_2g = 0;
	static unsigned int data_2g = 0;
	unsigned long count_2g;
	int i;
	static int j;
	static int status = -1;
	static int status_old;

	// check data per 10 count
	if ((blink_2g_check%10) == 0) {
		count_2g = get_2g_count();
		if (count_2g && data_2g != count_2g) {
			blink_2g = 1;
			data_2g = count_2g;
		}
		else blink_2g = 0;
		led_control(LED_2G, LED_ON);
	}

	if (blink_2g) {
		j = rand_seed_by_time() % 3;
		for(i=0;i<10;i++) {
			usleep(33*1000);

			status_old = status;
			if (((i%2) == 0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_2G, LED_ON);
				else
					led_control(LED_2G, LED_OFF);
			}
		}
		led_control(LED_2G, LED_ON);
	}

	blink_2g_check++;
}
#endif	/* RTCONFIG_WLAN_LED */

#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U) || defined(BCM4352)
unsigned long get_5g_count()
{
	FILE *f;
	char buf[256];
	char *ifname, *p;
	unsigned long counter1, counter2;

	if ((f = fopen("/proc/net/dev", "r")) == NULL) return -1;

	fgets(buf, sizeof(buf), f);
	fgets(buf, sizeof(buf), f);

	counter1=counter2=0;

	while (fgets(buf, sizeof(buf), f)) {
		if ((p=strchr(buf, ':')) == NULL) continue;
		*p = 0;
		if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
		else ++ifname;

		if (strcmp(ifname, "eth2")) continue;

		if (sscanf(p+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%lu", &counter1, &counter2) != 2) continue;

	}
	fclose(f);

	return counter1;
}

void fake_wl_led_5g(void)
{
	static unsigned int blink_5g_check = 0;
	static unsigned int blink_5g = 0;
	static unsigned int data_5g = 0;
	unsigned long count_5g;
	int i;
	static int j;
	static int status = -1;
	static int status_old;
	int debug = nvram_get_int("debug_fake_wl_led_5g");

	// check data per 10 count
	if ((blink_5g_check%10) == 0) {
		count_5g = get_5g_count();
		if (count_5g && data_5g != count_5g) {
			blink_5g = 1;
			data_5g = count_5g;
		}
		else blink_5g = 0;
		led_control(LED_5G, LED_ON);
	}

	if (blink_5g) {
#if defined(RTAC66U) || defined(BCM4352)
		j = rand_seed_by_time() % 3;
#endif
		for(i=0;i<10;i++) {
#if defined(RTAC66U) || defined(BCM4352)
			usleep(33*1000);

			status_old = status;
			if (((i%2) == 0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_5G, LED_ON);
				else
					led_control(LED_5G, LED_OFF);
			}
#else
			usleep(50*1000);
			if (i%2 == 0) {
				led_control(LED_5G, LED_OFF);
			}
			else {
				led_control(LED_5G, LED_ON);
			}
			if (debug)
			_dprintf("****** %s:%d ******\n", __func__, __LINE__);
#endif
		}
		led_control(LED_5G, LED_ON);
		if (debug)
		_dprintf("****** %s:%d ******\n", __func__, __LINE__);
	}

	blink_5g_check++;
}
#endif

static int led_confirmed = 0;
extern int led_gpio_table[LED_ID_MAX];

int confirm_led()
{
	if (
		1
#if defined(RTN53) || defined(RTN18U)
		&& led_gpio_table[LED_2G] != 0xff
		&& led_gpio_table[LED_2G] != -1
#endif
#ifdef RTCONFIG_FAKE_ETLAN_LED
		&& led_gpio_table[LED_LAN] != 0xff
		&& led_gpio_table[LED_LAN] != -1
#endif
#if defined(RTCONFIG_USB) && !defined(RTCONFIG_BLINK_LED)
#ifdef RTCONFIG_USB_XHCI
		&& led_gpio_table[LED_USB3] != 0xff
		&& led_gpio_table[LED_USB3] != -1
#endif
		&& led_gpio_table[LED_USB] != 0xff
		&& led_gpio_table[LED_USB] != -1
#endif
#ifdef RTCONFIG_MMC_LED
		&& led_gpio_table[LED_MMC] != 0xff
		&& led_gpio_table[LED_MMC] != -1
#endif
#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U) || defined(BCM4352)
		&& led_gpio_table[LED_5G] != 0xff
		&& led_gpio_table[LED_5G] != -1
#endif
#ifdef RTCONFIG_DSL
#ifndef RTCONFIG_DUALWAN
		&& led_gpio_table[LED_WAN] != 0xff
		&& led_gpio_table[LED_WAN] != -1
#endif
#endif
	)
		led_confirmed = 1;
	else
		led_confirmed = 0;

	return led_confirmed;
}

#ifdef SW_DEVLED
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
static int swled_alloff_counts = 0;
static int swled_alloff_x = 0;
#endif
#ifdef BLUECAVE
enum {
	CASE_NONE = 0,
	CASE_INDICATOR_INIT,
	CASE_INDICATOR_WPS,
	CASE_INDICATOR_WPS_DONE,
	CASE_INDICATOR_RESET,
};

static int bh_case = CASE_NONE;
static int indicator_rush_counts = -1;
static int central_rush_counts = 0;
static int led_alarm_rush = 0;
static int central_cycle = 0;
static int brightness_level = 0;
static int indicator_no_internet_red = -1;
static int indicator_no_internet_red_old = -1;
static int indicator_no_internet_cnt = 0;
static int central_bh[10] = {0,1,2,3,3,3,3,3,2,1};

void led_rush(int sig)
{
	if(nvram_match("bc_ledbh", "reset"))
		bh_case = CASE_INDICATOR_RESET;
	else if(nvram_match("bc_ledbh", "wps"))
		bh_case = CASE_INDICATOR_WPS;
	else if(nvram_match("bc_ledbh", "wps_done"))
		bh_case = CASE_INDICATOR_WPS_DONE;
	else if(!nvram_match("x_Setting", "1"))
		bh_case = CASE_INDICATOR_INIT;
	else
		bh_case = CASE_NONE;

	central_cycle = nvram_match("x_Setting", "0") ? 1 : 0;

	nvram_set("bc_ledbh", "");
	led_alarm_rush = 1;
	indicator_rush_counts = -1;
	alarmtimer(0, RUSHURGENT_PERIOD);
}

void led_stop(int sig)
{
	bh_case = CASE_NONE;
	indicator_rush_counts = -1;

	led_control(LED_INDICATOR_SIG1, LED_OFF); //turn off RED led
	if (!nvram_get_int("wave_ready"))
		led_control(LED_INDICATOR_SIG2, LED_ON); //turn on BLUE led
	else
		led_control(LED_INDICATOR_SIG2, LED_OFF); //turn off BLUE led


	if(!central_cycle) {
		led_alarm_rush = 0;
		alarmtimer(NORMAL_PERIOD, 0);
	}
}

void bluecave_ledbh_central()
{

	if(!nvram_get_int("wave_ready") || (!central_cycle && central_rush_counts == -1))
		return;

	/* cycle central led util QIS is completed */
	if(central_cycle) {
		central_rush_counts = (central_rush_counts + 1) % 6;
		if(central_rush_counts == 0)	//300 ms
			setCentralLedLv(central_bh[++brightness_level % 10]);
	}
	else {
		notify_rc("reset_led");
		central_rush_counts = -1;
	}
}

void bluecave_ledbh_indicator()
{
	int WpsLedBlink, WpsLedOff;

	if (!nvram_get_int("wave_ready") && bh_case != CASE_INDICATOR_RESET)
		return;


	/* indicator led control */
	switch(bh_case) {
		/* 0123456789 */
		/* 0011100111 */
		/* blink BLUE led util QIS is completed */
		case CASE_INDICATOR_INIT:

			if(indicator_rush_counts == -1) {
				led_control(LED_INDICATOR_SIG1, LED_OFF); //turn off RED led
				led_control(LED_INDICATOR_SIG2, LED_OFF); //turn off BLUE led
				indicator_rush_counts++;
			}

			if(nvram_match("x_Setting", "1"))
				kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
			break;

		case CASE_INDICATOR_WPS:
			led_control(LED_INDICATOR_SIG1, LED_OFF);
			indicator_rush_counts = (indicator_rush_counts + 1) % 20;
			if ((indicator_rush_counts % 2) == 0 && (indicator_rush_counts > 10))
				led_control(LED_INDICATOR_SIG2, LED_ON);
			else
				led_control(LED_INDICATOR_SIG2, LED_OFF);
			break;

		/* blink led for WPS complete/terminate */
		case CASE_INDICATOR_WPS_DONE:

			indicator_rush_counts++;
			if(nvram_get_int("wps_success")) {	// blink BLUE LED
				WpsLedBlink = LED_INDICATOR_SIG2;
				WpsLedOff = LED_INDICATOR_SIG1;
			}
			else {					// blink RED LED
				WpsLedBlink = LED_INDICATOR_SIG1;
				WpsLedOff = LED_INDICATOR_SIG2;
			}

			led_control(WpsLedOff, LED_OFF);
			if ((indicator_rush_counts % 10) < 2 || ((indicator_rush_counts % 10) > 4 && (indicator_rush_counts % 10) < 7))
				led_control(WpsLedBlink, LED_OFF);
			else
				led_control(WpsLedBlink, LED_ON);

			if(indicator_rush_counts >= 20 * 6) {	// blink 6s then turn off
				led_control(WpsLedBlink, LED_OFF);
				nvram_set("wps_success", "0");
				kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
			}
			break;

		case CASE_INDICATOR_RESET:
			indicator_rush_counts = (indicator_rush_counts + 1) % 10;
			if ((indicator_rush_counts % 10) < 2 || ((indicator_rush_counts % 10) > 4 && (indicator_rush_counts % 10) < 7)) {
					led_control(LED_INDICATOR_SIG1, LED_OFF);
					led_control(LED_INDICATOR_SIG2, LED_OFF);
			}
			else {
				led_control(LED_INDICATOR_SIG1, LED_ON);
				led_control(LED_INDICATOR_SIG2, LED_ON);
			}
			if (!button_pressed(BTN_RESET))
				kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
			break;

		case CASE_NONE:
		default:

			// regular checking WAN status
			if(!nvram_match("link_internet", "2")) {
				if(++indicator_no_internet_cnt >= 3)
					indicator_no_internet_red = 1;
			}
			else {
				indicator_no_internet_red = 0;
				indicator_no_internet_cnt = 0;
			}

			// Solid RED led if no internet ability
			if((indicator_no_internet_red != indicator_no_internet_red_old) ||
			   (indicator_no_internet_red && !get_gpio(nvram_get_int("led_idr_sig1_gpio")) ||
			   (!indicator_no_internet_red) && get_gpio(nvram_get_int("led_idr_sig2_gpio")))) // WAR for gpio was reset by mem xxx
			{
				indicator_no_internet_red == 1 ?
					led_control(LED_INDICATOR_SIG1, LED_ON) :
					led_control(LED_INDICATOR_SIG1, LED_OFF);
				led_control(LED_INDICATOR_SIG2, LED_OFF);
				indicator_no_internet_red_old = indicator_no_internet_red;
			}

			break;
	}
}
#endif

void led_check(int sig)
{
#ifdef BLUECAVE
	bluecave_ledbh_central();
	bluecave_ledbh_indicator();
	if(led_alarm_rush && !central_cycle && bh_case == CASE_NONE)
		kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR2);
#endif

#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
	int all_led;
	int turnoff_counts = swled_alloff_counts?:3;

	if ((all_led=nvram_get_int("AllLED")) == 0 && swled_alloff_x < turnoff_counts) {
		/* turn off again x times in case timing issues */
		led_table_ctrl(LED_OFF);
		swled_alloff_x++;
		_dprintf("force turnoff led table again!\n");
		return;
	}

	if (all_led)
		swled_alloff_x = 0;
	else
		return;
#endif

	if (!confirm_led()) {
		get_gpio_values_once(1);
	}

#ifdef RTCONFIG_WLAN_LED
	if (nvram_contains_word("rc_support", "led_2g"))
	{
#if defined(RTN53)
		if (nvram_get_int("wl0_radio") == 0)
			led_control(LED_2G, LED_OFF);
		else
#endif
		fake_wl_led_2g();
	}
#endif

#ifdef RTN18U
	if (nvram_match("bl_version", "1.0.0.0"))
		fake_wl_led_2g();
#endif

#ifdef RTCONFIG_FAKE_ETLAN_LED
	fake_etlan_led();
#endif

#if defined(RTCONFIG_USB) && (defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER))
	char p1_node[32], p2_node[32], ehci_ports[32], xhci_ports[32];

	/* led indicates which usb port */
	switch (get_model()) {
		case MODEL_RTAC5300:
		case MODEL_GTAC5300:
		case MODEL_RTAC88U:
		case MODEL_RTAC86U:
		case MODEL_RTAC3100:
		default:
			snprintf(p1_node, sizeof(p1_node), "%s", nvram_safe_get("usb_path1_node"));
			snprintf(p2_node, sizeof(p2_node), "%s", nvram_safe_get("usb_path2_node"));
			break;
	}

	snprintf(ehci_ports, sizeof(ehci_ports), "%s", nvram_safe_get("ehci_ports"));
	snprintf(xhci_ports, sizeof(xhci_ports), "%s", nvram_safe_get("xhci_ports"));

#ifdef RTCONFIG_USB_XHCI
	if(strlen(p1_node) > 0){
		if (strstr(xhci_ports, p1_node))
			fake_dev_led(nvram_safe_get("xhci_irq"), LED_USB3);
		else if (strstr(ehci_ports, p1_node))
			fake_dev_led(nvram_safe_get("ehci_irq"), LED_USB3);
	}
#endif
	if(strlen(p2_node) > 0)
		fake_dev_led(nvram_safe_get("ehci_irq"), LED_USB);
#endif

#ifdef RTCONFIG_MMC_LED
	if (*nvram_safe_get("usb_path3"))
		fake_dev_led(nvram_safe_get("mmc_irq"), LED_MMC);
#endif

#if defined(RTCONFIG_BRCM_USBAP) || defined(RTAC66U) || defined(BCM4352)
#if defined(RTAC66U) || defined(BCM4352)
	if (nvram_match("led_5g", "1") &&
	   (wlonunit == -1 || wlonunit == 1))
#endif
	{
#if defined(RTN53)
		if (nvram_get_int("wl1_radio") == 0)
			led_control(LED_5G, LED_OFF);
		else
#endif
		fake_wl_led_5g();
	}
#endif

// it is not really necessary, but if required, add internet led check here
// using wan_primary_ifunit() to get current working wan unit wan0 or wan1
// using wan0_state_t or wan1_state_t to get status of working wan,
//	WAN_STATE_CONNECTED means internet connected
//	else means internet disconnted

/* Paul add 2012/10/25 */
#ifdef RTCONFIG_DSL
#ifndef RTCONFIG_DUALWAN
	if (nvram_match("dsltmp_adslsyncsts","up") && is_wan_connect(0))
		led_DSLWAN();
#endif
#endif
}
#endif

#if defined (RTCONFIG_LED_BTN) || defined (RTCONFIG_WPS_ALLLED_BTN)
void led_table_ctrl(int on_off)
{
	int i;

	for(i=0; i < LED_ID_MAX; ++i) {
		if (led_gpio_table[i] != 0xff && led_gpio_table[i] != -1 && i != PWR_USB) {
			led_control(i, on_off);
		}
	}
}
#endif

/* Paul add 2012/10/25 */
#ifdef RTCONFIG_DSL
#ifndef RTCONFIG_DUALWAN
unsigned long get_dslwan_count()
{
	FILE *f;
	char buf[256];
	char *ifname, *p;
	unsigned long counter1, counter2;

	if ((f = fopen("/proc/net/dev", "r")) == NULL) return -1;

	fgets(buf, sizeof(buf), f);
	fgets(buf, sizeof(buf), f);

	counter1=counter2=0;

	while (fgets(buf, sizeof(buf), f)) {
		if ((p=strchr(buf, ':')) == NULL) continue;
		*p = 0;
		if ((ifname = strrchr(buf, ' ')) == NULL) ifname = buf;
		else ++ifname;

		if (nvram_match("dsl0_proto","pppoe") || nvram_match("dsl0_proto","pppoa"))
		{
			if (strcmp(ifname, "ppp0")) continue;
		}
		else //Mer, IPoA
		{
			if (strcmp(ifname, "eth2.1.1")) continue;
		}

		if (sscanf(p+1, "%lu%*u%*u%*u%*u%*u%*u%*u%*u%lu", &counter1, &counter2) != 2) continue;
	}
	fclose(f);

	return counter1;
}

void led_DSLWAN(void)
{
	static unsigned int blink_dslwan_check = 0;
	static unsigned int blink_dslwan = 0;
	static unsigned int data_dslwan = 0;
	unsigned long count_dslwan;
	int i;
	static int j;
	static int status = -1;
	static int status_old;

	// check data per 10 count
	if ((blink_dslwan_check%10) == 0) {
		count_dslwan = get_dslwan_count();
		if (count_dslwan && data_dslwan != count_dslwan) {
			blink_dslwan = 1;
			data_dslwan = count_dslwan;
}
		else blink_dslwan = 0;
		led_control(LED_WAN, LED_ON);
	}

	if (blink_dslwan) {
		j = rand_seed_by_time() % 3;
		for(i=0;i<10;i++) {
			usleep(33*1000);

			status_old = status;
			if (((i%2) == 0) && (i > (3 + 2*j)))
				status = 0;
			else
				status = 1;

			if (status != status_old)
			{
				if (status)
					led_control(LED_WAN, LED_ON);
				else
					led_control(LED_WAN, LED_OFF);
			}
		}
		led_control(LED_WAN, LED_ON);
	}

	blink_dslwan_check++;
}
#endif
#endif

/* signal handlers stuff */
void unblock_sigs()
{
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);
}

typedef void (*dog_func)(int sig);
dog_func dp=NULL;

void put_all_dogs()
{
	int sig;
        for (sig = 0; sig < (_NSIG - 1); sig++) {
		if(sigbones & 1<<sig  &&  (dp=fn_acts[sig])) {
			(*dp)(sig);
			sigbones &= ~(1<<sig);
		}
	}
}

void init_sig() 
{
	int sig;
        for (sig = 0; sig < (_NSIG - 1); sig++) {
		if(sig == SIGCHLD 
		|| sig == SIGUSR1
		|| sig == SIGUSR2
		|| sig == SIGTSTP
		|| sig == SIGALRM
#if defined(RTAC1200G) || defined(RTAC1200GP)
		|| sig == SIGHUP
#endif
#ifdef RTCONFIG_RALINK
		|| sig == SIGTTIN
#endif
		)
                signal(sig, watch_sig);
		fn_acts[sig] = NULL;
	}

	fn_acts[SIGCHLD] = chld_reap_local;
	fn_acts[SIGUSR1] = catch_sig;
	fn_acts[SIGUSR2] = catch_sig;
	fn_acts[SIGTSTP] = catch_sig;
	fn_acts[SIGALRM] = watchdog;
#if defined(RTAC1200G) || defined(RTAC1200GP)
	fn_acts[SIGHUP]  = catch_sig;
#endif
#ifdef RTCONFIG_RALINK
	fn_acts[SIGTTIN] = catch_sig;
#endif

	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGCHLD);
	sigaddset(&sigs_to_catch, SIGUSR1);
	sigaddset(&sigs_to_catch, SIGUSR2);
	sigaddset(&sigs_to_catch, SIGTSTP);
	sigaddset(&sigs_to_catch, SIGALRM);
#if defined(RTAC1200G) || defined(RTAC1200GP)
	sigaddset(&sigs_to_catch, SIGHUP);
#endif
#ifdef RTCONFIG_RALINK
	sigaddset(&sigs_to_catch, SIGTTIN);
#endif
}

#ifdef SW_DEVLED
void init_sig_swled() 
{
	int sig;
        for (sig = 0; sig < (_NSIG - 1); sig++) {
		if(sig == SIGALRM
#ifdef BLUECAVE
		|| sig == SIGUSR1
		|| sig == SIGUSR2
#endif
		)
                signal(sig, watch_sig);
		fn_acts[sig] = NULL;
	}

	fn_acts[SIGALRM] = led_check;
#ifdef BLUECAVE
	fn_acts[SIGUSR1] = led_rush;
	fn_acts[SIGUSR2] = led_stop;
#endif
}
#endif

#if defined(RTAC1200G) || defined(RTAC1200GP)
void wdg_heartbeat(int sig)
{
	if(factory_debug())
		return;

	if(sig == SIGUSR1) {
		wdg_timer_alive++;
	}
	else if(sig == SIGALRM) {
		if(wdg_timer_alive) {
			wdg_timer_alive = 0;
		}
		else {
			_dprintf("[%s] Watchdog's heartbeat is stop! Recover...\n", __FUNCTION__);
			kill_pidfile_s("/var/run/watchdog.pid", SIGHUP);
		}
	}
}

void init_sig_wmon() 
{
	int sig;
        for (sig = 0; sig < (_NSIG - 1); sig++) {
		if(sig == SIGALRM
		|| sig == SIGUSR1
		)
                signal(sig, watch_sig);
		fn_acts[sig] = NULL;
	}

	fn_acts[SIGALRM] = wdg_heartbeat;
	fn_acts[SIGUSR1] = wdg_heartbeat;
}
#endif

/* sw_mode */
#ifdef RTCONFIG_SWMODE_SWITCH
// copied from 2.x code
int pre_sw_mode=0, sw_mode=0;
int flag_sw_mode=0;
int tmp_sw_mode=0;
int count_stable=0;

void swmode_check()
{
#if defined(PLAC66U)
	int pre_sw_mode=sw_mode();

	if (!button_pressed(BTN_SWMODE_SW_ROUTER))
		sw_mode = SW_MODE_ROUTER;
	else    sw_mode = SW_MODE_AP;

	if ((sw_mode != pre_sw_mode) && !flag_sw_mode) {
		if( sw_mode == SW_MODE_ROUTER )
			_dprintf("[%s], switch to ROUTER Mode!\n", __FUNCTION__);
		else
			_dprintf("[%s], switch to AP Mode!\n", __FUNCTION__);

		flag_sw_mode=1;
		count_stable=0;
	}
	else if (flag_sw_mode == 1 && nvram_invmatch("asus_mfg", "1")) {
		if (sw_mode != pre_sw_mode) {
			if (++count_stable>4) { // stable for more than 5 second
				if (shutdown_start("watchdog", __FUNCTION__, "resetdefault") == 0) {
				dbg("Reboot to switch sw mode ..\n");
				flag_sw_mode=0;
				/* sw mode changed: restore defaults */
				led_control(LED_POWER, LED_OFF);
				alarmtimer(0, 0);
				nvram_set("restore_defaults", "1");
				if (notify_rc_after_wait("resetdefault")) {	/* Send resetdefault rc_service failed. */
					alarmtimer(NORMAL_PERIOD, 0);
				}
				}
			}
		}
		else flag_sw_mode = 0;
	}
#else
	if (!nvram_get_int("swmode_switch")) return;
	pre_sw_mode = sw_mode;

	if (button_pressed(BTN_SWMODE_SW_REPEATER))
		sw_mode=SW_MODE_REPEATER;
	else if (button_pressed(BTN_SWMODE_SW_AP))
		sw_mode=SW_MODE_AP;
	else sw_mode=SW_MODE_ROUTER;

	if (sw_mode != pre_sw_mode) {
		if (sw_mode() != sw_mode) {
			flag_sw_mode=1;
			count_stable=0;
			tmp_sw_mode=sw_mode;
		}
		else flag_sw_mode=0;
	}
	else if (flag_sw_mode == 1 && nvram_invmatch("asus_mfg", "1")) {
		if (tmp_sw_mode == sw_mode) {
			if (++count_stable>4) // stable for more than 5 second
			{
				if (shutdown_start("watchdog", __FUNCTION__, "reboot") == 0) {
				dbg("Reboot to switch sw mode ..\n");
				flag_sw_mode=0;
				sync();
				/* sw mode changed: restore defaults */
				nvram_set("nvramver", "0");
				nvram_commit();
				reboot(RB_AUTOBOOT);
				}
			}
		}
		else flag_sw_mode = 0;
	}
#endif	/* Model */
}
#endif	/* RTCONFIG_SWMODE_SWITCH */
#ifdef WEB_REDIRECT
void wanduck_check(void)
{
	if ((freeze_duck_count = nvram_get_int("freeze_duck")) > 0)
		nvram_set_int("freeze_duck", --freeze_duck_count);
}
#endif

#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
static int client_check_cnt = 0;
static int no_client_cnt = 0;
static int plc_wake = 1;

static void client_check(void)
{
	#define WIFI_STA_LIST_PATH "/tmp/wifi_sta_list"
	FILE *fp;
	char word[256], *next, ifnames[128];
	char buf[512];
	int i, n = 0;

	if (!nvram_match("plc_ready", "1"))
		return;

	/* every 2 seconds check */
	if (client_check_cnt++ > 0) {
		client_check_cnt = 0;
		return;
	}
	//dbg("%s:\n", __func__);

	/* check wireless client */
	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach(word, ifnames, next) {
		SKIP_ABSENT_BAND_AND_INC_UNIT(i);
		doSystem("wlanconfig %s list > %s", get_wifname(i), WIFI_STA_LIST_PATH);

		fp = fopen(WIFI_STA_LIST_PATH, "r");
		if (!fp) {
			dbg("%s: fopen fail\n", __func__);
			return;
		}
		while (fgets(buf, 512, fp) != NULL)
			n++;
		fclose(fp);
		unlink(WIFI_STA_LIST_PATH);

		++i;
	}

	/* check wire port */
	if (get_lanports_status())
		n++;

	if (n == 0)
		no_client_cnt++;
	else
		no_client_cnt = 0;

	if (plc_wake == 1 && no_client_cnt >= 5) {
		//dbg("%s: trigger Powerline to sleep...\n", __func__);
#if defined(PLN12)
		doSystem("swconfig dev %s port 1 set power 0", MII_IFNAME);
#elif defined(PLAC56)
		set_gpio((nvram_get_int("plc_wake_gpio") & 0xff), 1);
#else
#warning TBD: PL-AC66U...
#endif
		plc_wake = 0;
		nvram_set("plc_wake", "0");
	}
	else if (plc_wake == 0 && no_client_cnt == 0) {
		//dbg("%s: trigger Powerline to wake...\n", __func__);
#if defined(PLN12)
		doSystem("swconfig dev %s port 1 set power 1", MII_IFNAME);
#elif defined(PLAC56)
		set_gpio((nvram_get_int("plc_wake_gpio") & 0xff), 0);
#else
#warning TBD: PL-AC66U...
#endif
		plc_wake = 1;
		nvram_set("plc_wake", "1");
	}

	if (no_client_cnt >= 5)
		no_client_cnt = 0;
}
#endif

void regular_ddns_check(void)
{
#ifdef RPAC68U
/* The workaround solution avoiding watchdog segfault on RP-AC68U. */
	int r, wan_unit = rtk_wan_primary_ifunit(), last_unit = nvram_get_int("ddns_last_wan_unit");
#else
	int r, wan_unit = wan_primary_ifunit(), last_unit = nvram_get_int("ddns_last_wan_unit");
#endif
	char prefix[sizeof("wanX_YYY")];
	struct in_addr ip_addr;
	struct hostent *hostinfo;

	//_dprintf("regular_ddns_check...\n");

	hostinfo = gethostbyname(nvram_get("ddns_hostname_x"));
	ddns_check_count = 0;
	if (!hostinfo)
		return;

#if defined(RTCONFIG_DUALWAN)
	if (nvram_match("wans_mode", "lb")) {
		int ddns_wan_unit = nvram_get_int("ddns_wan_unit");

		if (ddns_wan_unit >= WAN_UNIT_FIRST && ddns_wan_unit < WAN_UNIT_MAX) {
			wan_unit = ddns_wan_unit;
		} else {
			int u = get_first_configured_connected_wan_unit();
			if (u < WAN_UNIT_FIRST || u >= WAN_UNIT_MAX)
				return;

			wan_unit = u;
		}
	}
#endif

	if (!nvram_match("wans_mode", "lb") && !is_wan_connect(wan_unit))
		return;

	snprintf(prefix, sizeof(prefix), "wan%d_", wan_unit);
	ip_addr.s_addr = *(unsigned long *)hostinfo -> h_addr_list[0];
	//_dprintf("%s ?= %s\n", nvram_pf_get(prefix, "ipaddr"), inet_ntoa(ip_addr));
	if (nvram_pf_match(prefix, "ipaddr", inet_ntoa(ip_addr)))
		return;

	//_dprintf("WAN IP change!\n");
	nvram_set("ddns_update_by_wdog", "1");
	if (wan_unit != last_unit)
		unlink("/tmp/ddns.cache");
	logmessage("watchdog", "Hostname/IP mapping error! Restart ddns.");
	if (last_unit != wan_unit)
		r = notify_rc("restart_ddns");
	else
		r = notify_rc("start_ddns");

	if (!r)
		nvram_set_int("ddns_last_wan_unit", wan_unit);


	return;
}

void ddns_check(void)
{
#ifdef RPAC68U
/* The workaround solution avoiding watchdog segfault on RP-AC68U. */
	int r, wan_unit = rtk_wan_primary_ifunit(), last_unit = nvram_get_int("ddns_last_wan_unit");
#else
	int r, wan_unit = wan_primary_ifunit(), last_unit = nvram_get_int("ddns_last_wan_unit");
#endif

	//_dprintf("ddns_check... %d\n", ddns_check_count);
	if (!nvram_match("ddns_enable_x", "1"))
		return;

#if defined(RTCONFIG_DUALWAN)
	if (nvram_match("wans_mode", "lb")) {
		int ddns_wan_unit = nvram_get_int("ddns_wan_unit");

		if (ddns_wan_unit >= WAN_UNIT_FIRST && ddns_wan_unit < WAN_UNIT_MAX) {
			wan_unit = ddns_wan_unit;
		} else {
			int u = get_first_configured_connected_wan_unit();
			if (u < WAN_UNIT_FIRST || u >= WAN_UNIT_MAX)
				return;

			wan_unit = u;
		}
	}
#endif

	if (!nvram_match("wans_mode", "lb") && !is_wan_connect(wan_unit))
		return;

	/* Check existence of ez-ipupdate/phddns
	 * if and only if last WAN unit is equal to new WAN unit.
	 */
	if (last_unit == wan_unit) {
		if (pids("ez-ipupdate"))	//ez-ipupdate is running!
			return;
		if (pids("phddns"))		//phddns is running!
			return;
	}

	if (nvram_match("ddns_regular_check", "1")&& !nvram_match("ddns_server_x", "WWW.ASUS.COM")) {
		int period = nvram_get_int("ddns_regular_period");
		if (period < 30) period = 60;
		if (ddns_check_count >= (period*2)) {
			regular_ddns_check();
			ddns_check_count = 0;
			return;
		}
		ddns_check_count++;
	}

	if (wan_unit == last_unit && nvram_match("ddns_updated", "1")) //already updated success
		return;

	if (wan_unit == last_unit) {
		if ( nvram_match("ddns_server_x", "WWW.ASUS.COM") ) {
			if ( !( !strcmp(nvram_safe_get("ddns_return_code_chk"),"Time-out") ||
				!strcmp(nvram_safe_get("ddns_return_code_chk"),"connect_fail") ||
				strstr(nvram_safe_get("ddns_return_code_chk"), "-1") ) )
				return;
		}
		else{ //non asusddns service
			if ( !strcmp(nvram_safe_get("ddns_return_code_chk"),"auth_fail") )
				return;
		}
	}

	if (nvram_get_int("ntp_ready") != 1)
		return;

	nvram_set("ddns_update_by_wdog", "1");
	if (wan_unit != last_unit)
		unlink("/tmp/ddns.cache");
	logmessage("watchdog", "start ddns.");
	if (last_unit != wan_unit)
		r = notify_rc("restart_ddns");
	else
		r = notify_rc("start_ddns");

	if (!r)
		nvram_set_int("ddns_last_wan_unit", wan_unit);

	return;
}

void networkmap_check()
{
#ifdef RTCONFIG_WIFI_SON
	if (sw_mode() == SW_MODE_AP && !nvram_match("cfg_master", "1"))
		return;
#endif
	if (!pids("networkmap"))
		start_networkmap(0);
}

void httpd_check()
{
#ifdef RTCONFIG_HTTPS
	int enable = nvram_get_int("http_enable");
	if ((enable != 1 && !pids("httpd")) ||
	    (enable != 0 && !pids("httpds")))
#else
	if (!pids("httpd"))
#endif
	{
#if defined(RTL_WTDOG)
		stop_rtl_watchdog();
#endif
		logmessage("watchdog", "restart httpd");
		stop_httpd();
		nvram_set("last_httpd_handle_request", nvram_safe_get("httpd_handle_request"));
		nvram_set("last_httpd_handle_request_fromapp", nvram_safe_get("httpd_handle_request_fromapp"));
		nvram_commit();
		start_httpd();
#if defined(RTL_WTDOG)
		start_rtl_watchdog();
#endif
	}
}

#ifdef RTCONFIG_LANTIQ
void wave_monitor_check()
{
	static int drop_caches_check = 0;

	if (!pids("wave_monitor")){
		nvram_set("wave_action", "3");
		nvram_set("wave_CFG", "1");
		logmessage("watchdog", "restart wave_monitor");
		killall_tk("wave_monitor");
		start_wave_monitor();
	}
	if(drop_caches_check < 4){
		f_write_string("/proc/sys/vm/drop_caches", "1", 0, 0);
		drop_caches_check++;
	}else{
		drop_caches_check = 0;
	}
}
#endif

void dnsmasq_check()
{
	if (!pids("dnsmasq")) {
		if (nvram_get_int("asus_mfg") == 1)
			return;

	if (!is_routing_enabled()
#ifdef RTCONFIG_WIRELESSREPEATER
		&& sw_mode() != SW_MODE_REPEATER
#endif
	)
		return;

#if defined(RTL_WTDOG)
		stop_rtl_watchdog();
#endif
		start_dnsmasq();
		TRACE_PT("watchdog: dnsmasq died. start dnsmasq...\n");

#if defined(RTL_WTDOG)
		start_rtl_watchdog();
#endif
	}
}

#ifdef RTCONFIG_NEW_USER_LOW_RSSI
void roamast_check()
{
	char prefix[]="wlXXXXXX_", tmp[64];
	char word[256], *next;
	int unit = 0, rast = 0;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		if (nvram_get_int(strcat_r(prefix, "user_rssi", tmp)) != 0) {
			rast = 1;
			break;
		}
		else {
			unit++;
			continue;
		}
	}

	if (rast && !pids("roamast"))
		start_roamast();
}
#endif

#if ! (defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
void watchdog_check()
{
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_BCM_7114)
	/* Do nothing. */
#else
	if (!pids("watchdog")) {
		if (nvram_get_int("upgrade_fw_status") == FW_INIT) {
			logmessage("watchdog02", "no wathdog, restarting");
			kill(1, SIGTERM);
		}
	}
	return;
#endif
}
#endif /* ! (RTCONFIG_QCA || RTCONFIG_RALINK) */

#if defined(RTCONFIG_QCA) && defined(RTCONFIG_WIGIG)
#define WIGIG_TEMP_PRINT_INTERVAL	(30)		/* seconds */
#define WIGIG_TEMP_CHECK_INTERVAL	(3 * 60)	/* seconds */
void wigig_temperatore_check(void)
{
	static double t_mac_avg = 0, t_radio_avg = 0;
	static long chk_t1 = 0, prn_t1 = 0;
	double t_mac = 0, t_radio = 0;
	char buf[48], *p;
	long t2;
	int r;

	t2 = uptime();
	if (chk_t1 != 0 && (t2 - chk_t1) < WIGIG_TEMP_CHECK_INTERVAL)
		return;

	/* Example: cat /sys/kernel/debug/ieee80211/phy0/wil6210/temp
	 * T_mac   = 73.089
	 * T_radio = 91.875
	 */
	chk_t1 = t2;
	r = f_read_string("/sys/kernel/debug/ieee80211/phy0/wil6210/temp", buf, sizeof(buf));
	if (r < 32)
		return;
	if (!(p = strstr(buf, "T_mac")) || (r = sscanf(p, "T_mac = %lf", &t_mac)) != 1)
		return;
	if (!(p = strstr(buf, "T_radio")) || (r = sscanf(p, "T_radio = %lf", &t_radio)) != 1)
		return;

	if (t_mac_avg != 0)
		t_mac_avg = (t_mac_avg + t_mac) / 2;
	else
		t_mac_avg = t_mac;

	if (t_radio_avg != 0)
		t_radio_avg = (t_radio_avg + t_radio) / 2;
	else
		t_radio_avg = t_radio;

	if (t_mac < 75 && t_mac_avg < 75 && t_radio < 75 && t_radio_avg < 75)
		return;
	if (prn_t1 != 0 && (t2 - prn_t1) < WIGIG_TEMP_PRINT_INTERVAL)
		return;
	dbg("WiGig temperature: MAC %.2lf (avg %.2lf), Radio %.2lf (avg %.2lf)\n",
		t_mac, t_mac_avg, t_radio, t_radio_avg);
	logmessage("WiGig temperature", "MAC %.2lf (avg %.2lf), Radio %.2lf (avg %.2lf)\n",
		t_mac, t_mac_avg, t_radio, t_radio_avg);
	prn_t1 = t2;
}
#endif	/* RTCONFIG_QCA && RTCONFIG_WIGIG */

#ifdef RTAC87U
void qtn_module_check(void)
{
	int ret;
	uint32_t channel;
	int channel_nvram;
	static int waiting = 0;
	static int failed = 0;
	char src_ip[16];
	char dst_ip[16];

	snprintf(src_ip, sizeof(src_ip), "%s", nvram_safe_get("QTN_RPC_CLIENT"));
	snprintf(dst_ip, sizeof(dst_ip), "%s", nvram_safe_get("QTN_RPC_SERVER"));

//	if (ATE_BRCM_FACTORY_MODE())
//		return;

	if (!nvram_get_int("qtn_ready"))
		return;

	if(nvram_get_int("wl1_chanspec") != 0){
		ret = rpc_qcsapi_get_channel(&channel);
		if ( ret == 0 ){
			channel_nvram =
				wf_chspec_ctlchan(wf_chspec_aton(nvram_safe_get("wl1_chanspec")));
			if(channel != channel_nvram){
				rpc_qcsapi_set_channel(nvram_safe_get("wl1_chanspec"));
				fprintf(stderr, "[qtn] current_channel=%d, next_channelActiveTime=%d\n", channel, channel_nvram);
			}
		}else{
				fprintf(stderr, "rpc_qcsapi_get_channel error:[%d][%d]\n", ret, channel);
		}
	}

	if(waiting < 2){
		waiting++;
		return;
	}

	if(icmp_check(src_ip, dst_ip) == 0 ){
		failed++;
		if(failed > 2){
			logmessage("QTN", "QTN connection lost[%s][%s]", src_ip, dst_ip);
			system("reboot &");
		}
	} else {
		failed = 0;
	}
	waiting = 0;

	return;
}

#endif

//#if defined(RTCONFIG_JFFS2LOG) && defined(RTCONFIG_JFFS2)
#if defined(RTCONFIG_JFFS2LOG) && (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
void syslog_commit_check(void)
{
	struct stat tmp_log_stat, jffs_log_stat;
	int tmp_stat, jffs_stat;

	tmp_stat = stat("/tmp/syslog.log", &tmp_log_stat);
	if (tmp_stat == -1)
		return;

	if (++log_commit_count >= LOG_COMMIT_PERIOD) {
		jffs_stat = stat("/jffs/syslog.log", &jffs_log_stat);
		if ( jffs_stat == -1) {
			eval("cp", "/tmp/syslog.log", "/tmp/syslog.log-1", "/jffs");
			return;
		}

		if ( tmp_log_stat.st_size > jffs_log_stat.st_size ||
		     difftime(tmp_log_stat.st_mtime, jffs_log_stat.st_mtime) > 0)
			eval("cp", "/tmp/syslog.log", "/tmp/syslog.log-1", "/jffs");

		log_commit_count = 0;
	}
	return;
}
#endif

#if defined(RTCONFIG_USB_MODEM)
void modem_log_check(void) {
#define MODEMLOG_FILE "/tmp/usb.log"
#define MAX_MODEMLOG_LINE 3
	FILE *fp;
	char cmd[64], var[16];
	int line = 0;

	if (++log_modem_count >= LOG_MODEM_PERIOD) {
		snprintf(cmd, 64, "cat %s |wc -l", MODEMLOG_FILE);
		if ((fp = popen("cat /tmp/usb.log |wc -l", "r")) != NULL) {
			var[0] = '\0';
			while(fgets(var, 16, fp)) {
				line = safe_atoi(var);
			}
			fclose(fp);

			if (line > MAX_MODEMLOG_LINE) {
				snprintf(cmd, 64, "cat %s |tail -n %d > %s-1", MODEMLOG_FILE, MAX_MODEMLOG_LINE, MODEMLOG_FILE);
				system(cmd);

				snprintf(cmd, 64, "mv %s-1 %s", MODEMLOG_FILE, MODEMLOG_FILE);
				system(cmd);

				snprintf(cmd, 64, "rm -f %s-1", MODEMLOG_FILE);
				system(cmd);
			}
		}

		log_modem_count = 0;
	}
	return;
}

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
void modem_flow_check(int modem_unit) {
	time_t now, start, diff_mon;
	struct tm tm_now, tm_start;
	char timebuf[32];
	int day_cycle;
	int reset;
	int unit;
	int data_save_sec = nvram_get_int("modem_bytes_data_save"), count;
	int debug = nvram_get_int("modem_bytes_data_cycle_debug");
	char tmp2[100], prefix2[32];
	char env_unit[32];

	usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

	unit = get_wan_unit(nvram_safe_get(strcat_r(prefix2, "act_dev", tmp2)));
	if (unit == -1)
		return;

	if (!is_wan_connect(unit))
		return;

	snprintf(env_unit, 32, "unit=%d", modem_unit);
	putenv(env_unit);

	if (data_save_sec == 0)
		data_save_sec = atoi(nvram_default_get("modem_bytes_data_save"));
	count = data_save_sec/30;
	modem_data_save = (modem_data_save+1)%count;
	if (!modem_data_save) {
		if (debug == 1)
			_dprintf("modem_flow_check: save the data usage.\n");
		eval("/usr/sbin/modem_status.sh", "bytes+");
	}

	if (++modem_flow_count >= MODEM_FLOW_PERIOD) {
		time(&now);
		memcpy(&tm_now, localtime(&now), sizeof(struct tm));
		if (debug == 1)
			_dprintf("modem_flow_check: now. year %4d, month %2d, day %2d, hour, %2d, minute %2d.\n", tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min);

		snprintf(timebuf, 32, "%s", nvram_safe_get("modem_bytes_data_start"));
		if (strlen(timebuf) <= 0 || !strcmp(timebuf, "0")) {
			snprintf(timebuf, 32, "%d", (int)now);
			nvram_set("modem_bytes_data_start", timebuf);
			eval("/usr/sbin/modem_status.sh", "bytes-");
		}
		start = strtol(nvram_safe_get("modem_bytes_data_start"), NULL, 10);
		memcpy(&tm_start, localtime(&start), sizeof(struct tm));
		if (debug == 1)
			_dprintf("modem_flow_check: start. year %4d, month %2d, day %2d, hour, %2d, minute %2d.\n", tm_start.tm_year+1900, tm_start.tm_mon+1, tm_start.tm_mday, tm_start.tm_hour, tm_start.tm_min);

		day_cycle = strtol(nvram_safe_get("modem_bytes_data_cycle"), NULL, 10);
		if (debug == 1)
			_dprintf("modem_flow_check: cycle=%d.\n", day_cycle);

		reset = 0;
		if (day_cycle >= 1 && day_cycle <= 31 && tm_now.tm_year+1900 >= 2014) {
			if (!start || tm_start.tm_year+1900 < 2014) {
				_dprintf("Start the cycle of the data count!\n");
				reset = 1;
			}
			else{
				if (tm_now.tm_mday == day_cycle) {
					// ex: day_cycle=20, start=2015/2/15 or 2014/4/30, now=2015/3/20. it should be reset by 2015/3/20, but in fact didn't yet.
					if (tm_now.tm_mday != tm_start.tm_mday || tm_now.tm_mon != tm_start.tm_mon || tm_now.tm_year != tm_start.tm_year)
						reset = 1;
				}
				else if (tm_now.tm_year != tm_start.tm_year) {
					reset = 1;
				}
				else if (tm_now.tm_mon < tm_start.tm_mon) {
					diff_mon = tm_now.tm_mon+12-tm_start.tm_mon;
					// ex: over 2 months. it should be reset, but in fact didn't yet.
					if (diff_mon > 1)
						reset = 1;
					else if (diff_mon == 1) {
						// ex: day_cycle=20, start=12/15, now=1/2. it should be reset by 12/20, but in fact didn't yet.
						// tm_start.tm_mday >= day_cycle: it had been reset.
						if (tm_start.tm_mday < day_cycle)
							reset = 1;
						// ex: day_cycle=20, start=12/25, now=1/21. it should be reset by 1/20, but in fact didn't yet.
						// tm_now.tm_mday < day_cycle: it didn't need to be reset.
						// tm_now.tm_mday == day_cycle: it had been reset by the above codes.
						else if (tm_now.tm_mday > day_cycle)
							reset = 1;
					}
				}
				else if (tm_now.tm_mon > tm_start.tm_mon) {
					diff_mon = tm_now.tm_mon-tm_start.tm_mon;
					// ex: over 2 months. it should be reset, but in fact didn't yet.
					if (diff_mon > 1)
						reset = 1;
					else if (diff_mon == 1) {
						// ex: day_cycle=31, start=2/28, now=3/1. it should be reset by 2/28(31?), but in fact didn't yet.
						// tm_start.tm_mday >= day_cycle: it had been reset.
						if (tm_start.tm_mday < day_cycle)
							reset = 1;
						// ex: day_cycle=20, start=2/25, now=3/21. it should be reset by 3/20, but in fact didn't yet.
						// tm_now.tm_mday < day_cycle: it didn't need to be reset.
						// tm_now.tm_mday == day_cycle: it had been reset by the above codes.
						else if (tm_now.tm_mday > day_cycle)
							reset = 1;
					}
				}
				else{ // tm_now.tm_mon == tm_start.tm_mon
					// ex: day_cycle=20, start=2/15, now=2/28. it should be reset by 2/20, but in fact didn't yet.
					// tm_now.tm_mday < day_cycle: it didn't need to be reset.
					// tm_now.tm_mday == day_cycle: it had been reset by the above codes.
					// tm_start.tm_mday >= day_cycle: it had been reset.
					if (tm_now.tm_mday > day_cycle && tm_start.tm_mday < day_cycle)
						reset = 1;
				}
			}
		}

		if (reset) {
			snprintf(timebuf, 32, "%d", (int)now);
			nvram_set("modem_bytes_data_start", timebuf);
			eval("/usr/sbin/modem_status.sh", "bytes-");
		}

		modem_flow_count = 0;
	}

	unsetenv("unit");

	return;
}
#endif
#endif

#ifdef RTCONFIG_NOTIFICATION_CENTER
void ntevent_intranet_usage_insight()
{
	time_t now;
	struct tm *tm;

	if (!nvram_get_int("ntp_ready")) return;

	char str[32];
	time(&now);
	tm = localtime(&now);

	/* send event at 9:00 each Monday */
	if (tm->tm_wday == 1 && tm->tm_hour == 9) {
		snprintf(str, 32, "0x%x", HINT_INTERNET_USAGE_INSIGHT_EVENT);
		eval("Notify_Event2NC", str, "");
	}
}
#endif

#if defined(RTCONFIG_USB) && defined(RTCONFIG_NOTIFICATION_CENTER) && defined(RTCONFIG_CLOUDSYNC)
static void ntevent_disk_usage_check(){
	char str[32];
	int ntevent_firstdisk = nvram_get_int("ntevent_firstdisk");
	int ntevent_nodiskuse_sec = nvram_get_int("ntevent_nodiskuse_sec");
	int ntevent_nodiskuse = nvram_get_int("ntevent_nodiskuse");
	int link_internet = nvram_get_int("link_internet");
	time_t now;
	unsigned long timestamp;

	if(ntevent_nodiskuse_sec <= 0){
		ntevent_nodiskuse_sec = 86400*3;
		nvram_set("ntevent_nodiskuse_sec", "259200");
		nvram_commit();
	}

	if(link_internet == 2 && !ntevent_nodiskuse && !ntevent_firstdisk){
		time(&now);
		snprintf(str, 32, "%s", nvram_safe_get("ntevent_nodiskuse_start"));
		if(strlen(str) > 0){
			timestamp = strtoll(str, (char **) NULL, 10);
			if(now-timestamp > ntevent_nodiskuse_sec){
				snprintf(str, 32, "0x%x", HINT_USB_CHECK_EVENT);
				eval("Notify_Event2NC", str, "");
				nvram_set("ntevent_nodiskuse", "1");
				nvram_commit();
			}
		}
		else{
			snprintf(str, 32, "%lu", now);
			nvram_set("ntevent_nodiskuse_start", str);
			nvram_commit();
		}
	}
}
#endif

static void auto_firmware_check()
{
#ifdef RTCONFIG_FORCE_AUTO_UPGRADE
	static int period = -1;
	static int bootup_check = 0;
#else
	static int period = 5757;
	static int bootup_check = 1;
#endif
	static int periodic_check = 0;
#ifndef RTCONFIG_FORCE_AUTO_UPGRADE
	int cycle_manual = nvram_get_int("fw_check_period");
	int cycle = (cycle_manual > 1) ? cycle_manual : 5760;
#endif
	time_t now;
	struct tm *tm;
#ifndef RTCONFIG_FORCE_AUTO_UPGRADE
	static int rand_hr, rand_min;
#endif
	int initial_state;

	if (!nvram_get_int("ntp_ready") || !nvram_get_int("firmware_check_enable"))
		return;

#ifdef RTCONFIG_FORCE_AUTO_UPGRADE
	setenv("TZ", nvram_safe_get("time_zone_x"), 1);
	time(&now);
	tm = localtime(&now);
	if ((tm->tm_hour >= 2) && (tm->tm_hour <= 6))	// 2 am to 6 am
		periodic_check = 1;
	else
		periodic_check = 0;
#else
	if (!bootup_check && !periodic_check)
	{
		time(&now);
		tm = localtime(&now);

		if ((tm->tm_hour == (2 + rand_hr)) &&	// every 48 hours at 2 am + random offset
		    (tm->tm_min == rand_min))
		{
			periodic_check = 1;
			period = -1;
		}
	}
#endif

#if 0
#if defined(RTAC68U)
	else if (After(get_blver(nvram_safe_get("bl_version")), get_blver("2.1.2.1")) && !nvram_get_int("PA") && !nvram_match("cpurev", "c0"))
	{
		periodic_check = 1;
		nvram_set_int("fw_check_period", 10);
	}
#endif
#endif

	if (bootup_check || periodic_check)
#ifdef RTCONFIG_FORCE_AUTO_UPGRADE
		period = (period + 1) % 20;
#else
		period = (period + 1) % cycle;
#endif
	else
		return;

	if (!period)
	{
		if (bootup_check)
		{
			bootup_check = 0;
#ifndef RTCONFIG_FORCE_AUTO_UPGRADE
			rand_hr = rand_seed_by_time() % 4;
			rand_min = rand_seed_by_time() % 60;
#endif
		}
		initial_state = nvram_get_int("webs_state_flag");

		if(!nvram_contains_word("rc_support", "noupdate")){
			eval("/usr/sbin/webs_update.sh");
		}
#ifdef RTCONFIG_DSL
		eval("/usr/sbin/notif_update.sh");
#endif

		if (nvram_get_int("webs_state_update") &&
		    !nvram_get_int("webs_state_error") &&
		    strlen(nvram_safe_get("webs_state_info")))
		{
			dbg("retrieve firmware information\n");

			if ((initial_state == 0) && (nvram_get_int("webs_state_flag") == 1))		// New update
			{
				char version[4], revision[3], build[16];

				memset(version, 0, sizeof(version));
				memset(revision, 0, sizeof(revision));
				memset(build, 0, sizeof(build));

				sscanf(nvram_safe_get("webs_state_info"), "%3[^_]_%2[^_]_%15s", version, revision, build);
				logmessage("watchdog", "New firmware version %s.%s_%s is available.", version, revision, build);
				run_custom_script("update-notification", NULL);
			}

#if defined(RTAC68U) || defined(RTCONFIG_FORCE_AUTO_UPGRADE)
#if defined(RTAC68U) && !defined(RTAC68A)
			if (!After(get_blver(nvram_safe_get("bl_version")), get_blver("2.1.2.1")) || nvram_get_int("PA") || nvram_match("cpurev", "c0"))
				return;
#endif

			if (nvram_get("login_ip") && !nvram_match("login_ip", ""))
				return;

			if (nvram_match("x_Setting", "0"))
				return;

			if (!nvram_get_int("webs_state_flag"))
			{
				dbg("no need to upgrade firmware\n");
				return;
			}

			if (shutdown_start("watchdog", __FUNCTION__, "auto_upgrade") != 0)
			{
				dbg("shutdown-start canceled the auto upgrade because it would reboot\n");
				return;
			}

			nvram_set_int("auto_upgrade", 1);

			eval("/usr/sbin/webs_upgrade.sh");

			if (nvram_get_int("webs_state_error"))
			{
				dbg("error execute upgrade script\n");
				goto ERROR;
			}

#ifndef RTCONFIG_FORCE_AUTO_UPGRADE
			nvram_set("restore_defaults", "1");
			ResetDefault();
#endif

#ifdef RTCONFIG_DUAL_TRX
			int count = 80;
#else
			int count = 50;
#endif
			while ((count-- > 0) && (nvram_get_int("webs_state_upgrade") == 1))
			{
				dbg("reboot count down: %d\n", count);
				sleep(1);
			}

			reboot(RB_AUTOBOOT);
#endif
		}
		else
			dbg("could not retrieve firmware information!\n");
#if defined(RTAC68U) || defined(RTCONFIG_FORCE_AUTO_UPGRADE)
ERROR:
		nvram_set_int("auto_upgrade", 0);
#endif
	}
}

#ifdef RTCONFIG_WIFI_SON
static void link_pap_status()
{
	int sw_mode = nvram_get_int("sw_mode");
	int count_point = 10;
	int prelink_pap_status = nvram_get_int("prelink_pap_status");
	int link_pap_status = 0;
	int alive = nvram_get_int("cfg_alive");

	if (!pids("bluetoothd")) {
		if (sw_mode == SW_MODE_ROUTER || (sw_mode == SW_MODE_AP && nvram_match("cfg_master", "1"))) {
			if (nvram_match("link_internet", "2"))
				link_pap_status = 1;
		}
		else if (sw_mode == SW_MODE_AP && !nvram_match("cfg_master", "1")) {
#ifdef RTCONFIG_ETHBACKHAUL
			if (nvram_get_int("eth_backl"))
				link_pap_status = 1;
			else {
#endif
			if (nvram_get_int("re_syncing")) //do not check wifi-status when CAP sync with RE
				return;

			if (!alive && (prelink_pap_status==-1 || prelink_pap_status>count_point))
				link_pap_status = 0;
			else
				link_pap_status = (int)getPapState(1)==2?1:((int)getPapState(0)==2?2:0);

#ifdef RTCONFIG_ETHBACKHAUL
			}
#endif
		}

		if (link_pap_status != prelink_pap_status)
		{
			if (sw_mode == SW_MODE_AP && !nvram_match("cfg_master", "1"))
				nvram_unset("lyra_re_dist");

			if (link_pap_status) {
#ifdef RTCONFIG_ETHBACKHAUL
				if (nvram_get_int("eth_backl"))
#if defined(RTCONFIG_LP5523)
					lp55xx_leds_proc(LP55XX_GREENERY_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
					set_rgbled(RGBLED_GREEN);
#endif
				else
#endif
#if defined(RTCONFIG_LP5523)
					lp55xx_leds_proc(LP55XX_LIGHT_CYAN_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
					set_rgbled(RGBLED_NIAGARA_BLUE);
#endif
			}
			else {
				if (prelink_pap_status < count_point) {
					if (sw_mode == SW_MODE_AP) {
						if (!alive && prelink_pap_status==-1) {
							link_pap_status = count_point + 180;
#if defined(RTCONFIG_LP5523)
							lp55xx_leds_proc(LP55XX_ALL_BREATH_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
#endif
						}
						else {
							if (nvram_match("wl1_country_code", "GB"))
								link_pap_status = count_point + 150;
							else
								link_pap_status = count_point + 15;

#if defined(RTCONFIG_LP5523)
							lp55xx_leds_proc(LP55XX_GREENERY_LEDS, LP55XX_ACT_3ON1OFF);
#elif defined(MAPAC1750)
							set_rgbled(RGBLED_BLUE_3ON1OFF);
#endif
						}
					}
					else {
						link_pap_status = count_point;
						if (sw_mode == SW_MODE_ROUTER || (sw_mode == SW_MODE_AP && nvram_match("cfg_master", "1")))
#if defined(RTCONFIG_LP5523)
							lp55xx_leds_proc(LP55XX_ORANGE_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
							set_rgbled(RGBLED_YELLOW);
#endif
						else
#if defined(RTCONFIG_LP5523)
							lp55xx_leds_proc(LP55XX_RED_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
							set_rgbled(RGBLED_RED);
#endif
					}
				}
				else if (prelink_pap_status > count_point) {
					link_pap_status=prelink_pap_status-1;

					if (link_pap_status == count_point) {
						if (!alive)
							nvram_set_int("cfg_alive", 99);
#if defined(RTCONFIG_LP5523)
						lp55xx_leds_proc(LP55XX_RED_LEDS, LP55XX_ACT_NONE);
#elif defined(MAPAC1750)
						set_rgbled(RGBLED_RED);
#endif
					}
				}
			}

			logmessage("WATCHDOG", "[%s] cfg alive:%d state:%d, pre state:%d", __func__, alive, link_pap_status, prelink_pap_status);
			nvram_set_int("prelink_pap_status", link_pap_status);
		}
	}
}
#endif

#ifdef RTCONFIG_BT_CONN
static void bt_turn_off_service()
{
	char buf[256];
	char *delim=";";
	char *tmp;

	memset(buf, '\0', sizeof(buf));
	strncpy(buf, nvram_safe_get("bt_turn_off_service"), sizeof(buf));

	if (strlen(buf)) {
		nvram_set("w_Setting", "1");
		tmp = strtok(buf, delim);
		while (tmp!=NULL) {
			int retry = 20;

			if (!strncmp(tmp, "ble_qis_done", strlen(tmp))) {
				nvram_set("x_Setting", "1");
				nvram_set("qis_Setting", "1");
				tmp = strtok(NULL, delim);
				continue;
			}

			while(retry-- > 0 && strlen(nvram_safe_get("rc_service"))) {
				_dprintf("\nwaiting for previous service");
				sleep(1);
			}

			notify_rc_and_wait(tmp);

			tmp = strtok(NULL, delim);
		}

		nvram_commit();
		stop_bluetooth_service();
		nvram_unset("bt_turn_off_service");
	}
}
#endif /* RTCONFIG_BT_CONN */

#ifdef RTCONFIG_AMAS
void amas_ctl_check()
{
	if (nvram_get_int("re_mode") == 1) {
		if (!pids("amas_bhctrl"))
			notify_rc("start_amas_bhctrl");
		if (!pids("amas_wlcconnect"))
			notify_rc("start_amas_wlcconnect");
		if (!pids("amas_lanctrl"))
			notify_rc("start_amas_lanctrl");
	}
}

void onboarding_check()
{
	static int onboarding_count = 0;

	if (!nvram_match("start_service_ready", "1"))
		return;

	if (!nvram_match("re_mode", "1"))
		return;

	if (strlen(nvram_safe_get("cfg_group")))
		return;

	onboarding_count++;

	if (onboarding_count > ONBOARDING_TIMEOUT) {
		_dprintf("### onboarding timeout, restore to default ###\n");
		notify_rc("resetdefault");
	}
}
#endif

#ifdef RTCONFIG_CFGSYNC
void cfgsync_check()
{
	if (nvram_match("x_Setting", "1") && 
		((!pids("cfg_client") && nvram_get_int("re_mode") == 1) || 
		(!pids("cfg_server") && nvram_get_int("cfg_master") == 1)))
	{
		_dprintf("start cfgsync\n");
		notify_rc("start_cfgsync");
	}
}
#endif /* RTCONFIG_CFGSYNC */

#ifdef RTCONFIG_TUNNEL
void mastiff_check()
{
	if (!pids("mastiff"))
		start_mastiff();
}
#endif /* RTCONFIG_TUNNEL */

#ifdef RTCONFIG_USER_LOW_RSSI
#define ETHER_ADDR_STR_LEN	18

typedef struct wl_low_rssi_count{
	char	wlif[32];
	int	lowc;
}wl_lowr_count_t;

#define		WLLC_SIZE	2
static wl_lowr_count_t wllc[WLLC_SIZE];

void init_wllc(void)
{
	char wlif[128], *next;
	int idx=0;

	memset(wllc, 0, sizeof(wllc));

	foreach (wlif, nvram_safe_get("wl_ifnames"), next)
	{
		SKIP_ABSENT_BAND_AND_INC_UNIT(idx);
		strncpy(wllc[idx].wlif, wlif, 32);
		wllc[idx].lowc = 0;

		idx++;
	}
}

#if defined(RTCONFIG_RALINK)
/* Defined in router/rc/sysdeps/ralink/ralink.c */
#elif defined(RTCONFIG_QCA)
/* Defined in router/rc/sysdeps/qca/qca.c */
#elif defined(RTCONFIG_ALPINE)
/* Defined in router/rc/sysdeps/alpine/alpine.c */
#elif defined(RTCONFIG_LANTIQ)
/* Defined in router/rc/sysdeps/lantiq/lantiq.c */
#else
#define	MAX_STA_COUNT	128
void rssi_check_unit(int unit)
{
	int lrsi = 0, lrc = 0;

	scb_val_t scb_val;
	char ea[ETHER_ADDR_STR_LEN];
	int i, ii;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char name[16];
	int val = 0;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	struct maclist *mac_list;
	int mac_list_size;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	lrc = nvram_get_int(strcat_r(prefix, "lrc", tmp));
	if (!lrc) lrc = 2;
	if (!(lrsi = nvram_get_int(strcat_r(prefix, "user_rssi", tmp))))
		return;

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	if (psta_exist_except(unit) || psr_exist_except(unit))
	{
		dbg("%s radio is disabled\n",
			nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz");
		return;
	}
	else if (is_psta(unit) || is_psr(unit))
	{
		dbg("skip interface %s under psta or psr mode\n", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
		return;
	}
#endif
	snprintf(name, sizeof(name), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
	wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
	val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;

	if (val)
	{
		dbg("%s radio is disabled\n",
			wl_nband_name(nvram_pf_get(prefix, "nband")));
		return;
	}

#ifdef RTCONFIG_WIRELESSREPEATER
	if ((sw_mode() == SW_MODE_REPEATER)
		&& (nvram_get_int("wlc_band") == unit))
	{
		sprintf(name_vif, "wl%d.%d", unit, 1);
		strlcpy(name, name_vif, sizeof(name));
	}
#endif

	/* buffers and length */
	mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);

	if (!mac_list)
		goto exit;

	memset(mac_list, 0, mac_list_size);

	/* query wl for authenticated sta list */
	strcpy((char*) mac_list, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, mac_list, mac_list_size))
		goto exit;

	for (i = 0; i < mac_list->count; i ++) {
		memcpy(&scb_val.ea, &mac_list->ea[i], ETHER_ADDR_LEN);

		if (wl_ioctl(name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
			continue;

		ether_etoa((void *)&mac_list->ea[i], ea);

		_dprintf("rssi chk.1. wlif (%s), chk ea=%s, rssi=%d(%d), lowr_cnt=%d, lrc=%d\n", name, ea, scb_val.val, lrsi, wllc[unit].lowc, lrc);

		if (scb_val.val < lrsi) {
			_dprintf("rssi chk.2. low rssi: ea=%s, lowc=%d(%d)\n", ea, wllc[unit].lowc, lrc);
			if (++wllc[unit].lowc > lrc) {
				_dprintf("rssi chk.3. deauth ea=%s\n", ea);

				scb_val.val = 8;	// reason code: Disassociated because sending STA is leaving BSS
				wllc[unit].lowc = 0;

				if (wl_ioctl(name, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val, sizeof(scb_val_t)))
					continue;
			}
		} else
			wllc[unit].lowc = 0;
	}

	for (i = 1; i < MAX_NO_MSSID; i++) {
#ifdef RTCONFIG_WIRELESSREPEATER
		if ((sw_mode() == SW_MODE_REPEATER)
			&& (unit == nvram_get_int("wlc_band")) && (i == 1))
			break;
#endif
		sprintf(prefix, "wl%d.%d_", unit, i);
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			sprintf(name_vif, "wl%d.%d", unit, i);

			memset(mac_list, 0, mac_list_size);

			/* query wl for authenticated sta list */
			strcpy((char*) mac_list, "authe_sta_list");
			if (wl_ioctl(name_vif, WLC_GET_VAR, mac_list, mac_list_size))
				goto exit;

			for (ii = 0; ii < mac_list->count; ii ++) {
				memcpy(&scb_val.ea, &mac_list->ea[ii], ETHER_ADDR_LEN);

				if (wl_ioctl(name_vif, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
					continue;

				ether_etoa((void *)&mac_list->ea[ii], ea);

				_dprintf("rssi chk.1. wlif (%s), chk ea=%s, rssi=%d(%d), lowr_cnt=%d, lrc=%d\n", name_vif, ea, scb_val.val, lrsi, wllc[unit].lowc, lrc);

				if (scb_val.val < lrsi) {
					_dprintf("rssi chk.2. low rssi: ea=%s, lowc=%d(%d)\n", ea, wllc[unit].lowc, lrc);
					if (++wllc[unit].lowc > lrc) {
						_dprintf("rssi chk.3. deauth ea=%s\n", ea);

						scb_val.val = 8;	// reason code: Disassociated because sending STA is leaving BSS
						wllc[unit].lowc = 0;

						if (wl_ioctl(name_vif, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val, sizeof(scb_val_t)))
							continue;
					}
				} else
					wllc[unit].lowc = 0;
			}
		}
	}

	/* error/exit */
exit:
	if (mac_list) free(mac_list);

	return;
}
#endif
void rssi_check()
{
	int ii = 0;
	char nv_param[NVRAM_MAX_PARAM_LEN];
	char temp[16];

	if (!nvram_get_int("wlready"))
		return;

	for (ii = 0; ii < DEV_NUMIFS; ii++) {
		sprintf(nv_param, "wl%d_unit", ii);
		snprintf(temp, sizeof(temp), "%s", nvram_safe_get(nv_param));

		if(strlen(temp) > 0)
			rssi_check_unit(ii);
	}
}
#endif

#if 0 //#ifdef RTCONFIG_TOR
#if (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
static void Tor_microdes_check() {

	FILE *f;
	char buf[256];
	char *p;
	struct stat tmp_db_stat, jffs_db_stat;
	int tmp_stat, jffs_stat;

	if (++tor_check_count >= TOR_CHECK_PERIOD) {
		tor_check_count = 0;

		jffs_stat = stat("/jffs/.tordb/cached-microdesc-consensus", &jffs_db_stat);
		if (jffs_stat != -1) {
			return;
		}

		tmp_stat = stat("/tmp/.tordb/cached-microdesc-consensus", &tmp_db_stat);
		if (tmp_stat == -1) {
			return;
		}

		if ((f = fopen("/tmp/torlog", "r")) == NULL) return;

		while (fgets(buf, sizeof(buf), f)) {
			if ((p=strstr(buf, "now have enough directory")) == NULL) continue;
			*p = 0;
			eval("cp", "-rfa", "/tmp/.tordb", "/jffs/.tordb");
			break;
		}
		fclose(f);
	}

	return;
}
#endif
#endif

#ifdef RTCONFIG_ERP_TEST
static void ErP_Test() {
	int i;
	int unit = 0;
	char nv_param[NVRAM_MAX_PARAM_LEN];
	char temp[16], name[16];
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	int val = 0;
	struct maclist *mac_list;
	int mac_list_size;
	char name_vif[] = "wlX.Y_XXXXXXXXXX";
	char ifname[32];
	char *next;
	int assoc_count = 0;

	if (!nvram_get_int("wlready"))
		return;

_dprintf("### ErP Check... ###\n");
	for (unit = 0; unit < DEV_NUMIFS; unit++) {
		snprintf(nv_param, sizeof(nv_param), "wl%d_unit", unit);
		snprintf(temp, sizeof(temp), "%s", nvram_safe_get(nv_param));

		if(strlen(temp) > 0){
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			snprintf(name, sizeof(name), "%s", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
			wl_ioctl(name, WLC_GET_RADIO, &val, sizeof(val));
			val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;

			if (val)
			{
				dbg("%s radio is disabled\n",
					wl_nband_name(nvram_pf_get(prefix, "nband")));
				continue;
			}

			/* buffers and length */
			mac_list_size = sizeof(mac_list->count) + 128 * sizeof(struct ether_addr);
			mac_list = malloc(mac_list_size);

			if (!mac_list)
				goto exit;

			memset(mac_list, 0, mac_list_size);

			/* query wl for authenticated sta list */
			strcpy((char*) mac_list, "authe_sta_list");
			if (wl_ioctl(name, WLC_GET_VAR, mac_list, mac_list_size))
				goto exit;

			if (mac_list->count) {
				assoc_count += mac_list->count;
				break;
			}

			for (i = 1; i < MAX_NO_MSSID; i++) {
				sprintf(prefix, "wl%d.%d_", unit, i);
				if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1")) {
					sprintf(name_vif, "wl%d.%d", unit, i);

					memset(mac_list, 0, mac_list_size);

					/* query wl for authenticated sta list */
					strcpy((char*) mac_list, "authe_sta_list");
					if (wl_ioctl(name_vif, WLC_GET_VAR, mac_list, mac_list_size))
						goto exit;

					if (mac_list->count) {
						assoc_count += mac_list->count;
						break;
					}
				}
			}
		}
	}//end unit for loop

	if (assoc_count) {
		//Back to Normal
		if (pwrsave_status == MODE_PWRSAVE) {
			_dprintf("ErP: Back to normal mode\n");
			no_assoc_check = 0;

			eval("wl", "-i", "eth2", "radio", "on"); // Turn on 5G-1 radio

			foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
				eval("wl", "-i", ifname, "txchain", "0xf");
				eval("wl", "-i", ifname, "rxchain", "0xf");
				eval("wl", "-i", ifname, "down");
				eval("wl", "-i", ifname, "up");
			}

			pwrsave_status = MODE_NORMAL;
		}
	}
	else {
		//No sta assoc. Enter PWESAVE Mode
		if (pwrsave_status == MODE_NORMAL) {
			no_assoc_check++;
			_dprintf("ErP: no_assoc_check = %d\n", no_assoc_check);

			if (no_assoc_check >= NO_ASSOC_CHECK) {
				_dprintf("ErP: Enter Power Save Mode\n");
				foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
					eval("wl", "-i", ifname, "txchain", "0x1");
					eval("wl", "-i", ifname, "rxchain", "0x1");
					eval("wl", "-i", ifname, "down");
					eval("wl", "-i", ifname, "up");
				}

				eval("wl", "-i", "eth2", "radio", "off"); // Turn off 5G-1 radio

				no_assoc_check = 0;
				pwrsave_status = MODE_PWRSAVE;
			}
		}
	}

	/* error/exit */
exit:
	if (mac_list) free(mac_list);

	return;
}
#endif

#ifdef RTCONFIG_CONCURRENTREPEATER
#define POWER_ON_TIME			40
#define CHECK_ASSOC_STATUS_TIME 60
#define OFFLINE_MAX_TIME		15
#define WLC_NUM 				2
typedef struct _wl_br_status{
	char wlcif[32];
	int rssi;
	int offline;
	int in_br;
}wl_br_status,*pwl_br_status;
wl_br_status wlbrs_list[WLC_NUM];

void dump_br_status(){
	pwl_br_status pstatus;
	int i;
	if(wlbrs_list[0].wlcif[0] == 0)
		return ;
	for(i = 0;i < WLC_NUM;i++){
		pstatus = &wlbrs_list[0]+ i*sizeof(wl_br_status);
		_dprintf("ifname=%s,in_br=%d,rssi=%d,offline = %d\n",pstatus->wlcif
			,pstatus->in_br,pstatus->rssi,pstatus->offline);
	}
}

void init_wl_br_status()
{
	pwl_br_status pstatus;
	int i = 0;
	for (i = 0; i < WLC_NUM; i++) {
		pstatus = &wlbrs_list[0]+ i*sizeof(wl_br_status);
		memset(pstatus->wlcif, 0, sizeof(pstatus->wlcif));
		pstatus->in_br = 0;
		pstatus->rssi = 0;
		pstatus->offline = 0;
	}
}

void update_wl_br_status(pwl_br_status list)
{
	int i;
	char buf[32];
	pwl_br_status pstatus;
	if(!list)
		return;
	if(list->wlcif[0]==0){
		if ((sw_mode() == SW_MODE_REPEATER && nvram_get_int("wlc_express") ==0) ||
			mediabridge_mode()){
			pstatus = list;
#if defined(RTCONFIG_QCA)
			strncpy(pstatus->wlcif, STA_2G,32);
#elif defined(RTCONFIG_RALINK)
			strncpy(pstatus->wlcif, APCLI_2G,32);
#elif defined(RTCONFIG_REALTEK)
			if (mediabridge_mode())
				strncpy(pstatus->wlcif, WIF_2G,32);
			else
				strncpy(pstatus->wlcif, VXD_2G,32);
#endif
			pstatus->offline = -1;
			if(WLC_NUM > 1){
				pstatus = list + sizeof(wl_br_status);
#if defined(RTCONFIG_QCA)
				strncpy(pstatus->wlcif, STA_5G,32);
#elif defined(RTCONFIG_RALINK)
				strncpy(pstatus->wlcif, APCLI_5G,32);
#elif defined(RTCONFIG_REALTEK)
			if (mediabridge_mode())
				strncpy(pstatus->wlcif, WIF_5G,32);
			else
				strncpy(pstatus->wlcif, VXD_5G,32);
#endif
				pstatus->offline = -1;
			}
		}
		else{
			return;
		}
	}

	for(i = 0;i < WLC_NUM;i++){
		pstatus = list + i*sizeof(wl_br_status);
		if(pstatus ->wlcif[0]){

			sprintf(buf, "wlc%d_state", i);
			if(nvram_get_int(buf) != WLC_STATE_CONNECTED){
				if(pstatus->offline >= 0)
					pstatus->offline++;
				continue;
			}

			pstatus->offline = 0;

			if (nvram_get_int(buf) == WLC_STATE_CONNECTED) {
				int rssi = get_conn_link_quality(i);
				if (rssi)
					pstatus->rssi = rssi;
				else
					pstatus->rssi = -1;
			}
			else
				pstatus->rssi = -1;
		}
	}
}
int is_wlc_br_online(pwl_br_status list){
	pwl_br_status pstatus;
	int i;
	if(!list || list->wlcif[0] == 0)
		return 0;
	for(i = 0;i < WLC_NUM;i++){
		pstatus = list + i*sizeof(wl_br_status);
		if(pstatus->in_br && pstatus->wlcif[0])
			return 1;
	}
	return 0;
}
int select_one_online(pwl_br_status list)
{
	pwl_br_status pstatus;
	int i;
	int max = -1,j = -1;
	char *lan_ifname;
	char ssidbuf[32];
	int connected = 0;
	static int skip_wait_flag = 0;
	static int wait_time = -99;

	if(!list || list->wlcif[0] == 0) {
		nvram_set_int("wlc_band", -1);
		nvram_set("wlc_ssid", "");
		return 0;
	}
	for(i = 0;i < WLC_NUM;i++){
		pstatus = list + i*sizeof(wl_br_status);
		if(pstatus->offline == 0 && pstatus->wlcif[0]) {
			if(max <= pstatus ->rssi){
				max = pstatus ->rssi;
				j = i;
			}
			if (pstatus->rssi >= 0)
				connected++;
		}
	}

	/* When DUT booted, wait for bands connected to select a the better quility band. */
	if (skip_wait_flag == 0 && (access_point_mode() || (repeater_mode() && nvram_get_int("wlc_express") != 0))) { // AP mode and express way mode
		skip_wait_flag = 1;
	}
	if (!skip_wait_flag) {
		if (((repeater_mode() && nvram_get_int("wlc_express") == 0) || mediabridge_mode()) && j >= 0) {
			if (WLC_NUM > 1) {
				if (j == 0 && wait_time == -99) { // 2G connected, wait 5g

		#if defined(RTCONFIG_WLMODULE_MT7615E_AP)
					wait_time = nvram_get_int("wl_time");
		#else
					wait_time = 10;
		#endif
				}
				else if (wait_time == -99){// 5G connected, wait 2g

		#if defined(RTCONFIG_WLMODULE_MT7615E_AP)
					wait_time = nvram_get_int("wl_time");
		#else
					wait_time = 10;
		#endif

				}

				if (wait_time > 0) {
					if (connected == WLC_NUM) { // all connected
						wait_time = 0;
					}
					else {
						wait_time--;
						return 0;
					}
				}
				else if (wait_time != -99)
					skip_wait_flag = 1;
			}
		}
	}
	if( j >=0 ){
		pstatus = list + j*sizeof(wl_br_status);
		pstatus ->in_br = 1;
		lan_ifname = strdup(nvram_safe_get("lan_ifname"));
		eval("brctl", "addif", lan_ifname, pstatus->wlcif);
		nvram_set_int("wlc_band", j);

		sprintf(ssidbuf, "wlc%d_ssid", j);
		nvram_set("wlc_ssid", nvram_safe_get(ssidbuf));

		free(lan_ifname);
		return 1;
	}

	nvram_set_int("wlc_band", -1);
	nvram_set("wlc_ssid", "");
	return 0;
}
void check_wlc_br_online(pwl_br_status list)
{
	pwl_br_status pstatus;
	int i;
	int j = -1;
	char *lan_ifname;
	if(!list || list->wlcif[0] == 0)
		return ;
	for(i = 0;i < WLC_NUM;i++){
		pstatus = list + i*sizeof(wl_br_status);
		if(pstatus->offline > OFFLINE_MAX_TIME && pstatus->wlcif[0] && pstatus->in_br){
			j = i;
			break;
		}
	}
	if( j >=0 ){
		pstatus = list + j*sizeof(wl_br_status);
		pstatus ->in_br = 0;
		lan_ifname = strdup(nvram_safe_get("lan_ifname"));
		eval("brctl", "delif", lan_ifname, pstatus->wlcif);
#if defined(RTCONFIG_QCA)
		/* special case for restart 2G/5G sta */
		ifconfig(pstatus->wlcif, 0, NULL, NULL);
		ifconfig(pstatus->wlcif, IFUP, NULL, NULL);
		if (!strcmp(pstatus->wlcif, STA_2G))
			eval("wpa_cli", "-p", "/var/run/wpa_supplicant-sta0", "-i", STA_2G, "reconnect");
		else if (!strcmp(pstatus->wlcif, STA_5G))
			eval("wpa_cli", "-p", "/var/run/wpa_supplicant-sta1", "-i", STA_5G, "reconnect");
#elif defined(RTCONFIG_RALINK)
		//TODO
#endif
		select_one_online(list);
		if (!nvram_match("wlc_band", "-1")) {
			/* renew IP */
			eval("killall", "-SIGUSR1", "udhcpc");
		}
		free(lan_ifname);
	}
}
void bridge_check()
{
	unsigned long uptime;
	struct sysinfo info;
	pwl_br_status plist = &wlbrs_list[0];
	sysinfo(&info);
	uptime = (unsigned long) info.uptime ;

	if (nvram_get_int("wps_cli_state") == 1 && sw_mode() == SW_MODE_REPEATER)
		return;
	if (uptime > POWER_ON_TIME) {
		update_wl_br_status(plist);
		if (!plist->wlcif[0])
			return;
		if (!is_wlc_br_online(plist)) {
			select_one_online(plist);
		}
		else {
			if (uptime > CHECK_ASSOC_STATUS_TIME) {
				check_wlc_br_online(plist);
			}
		}
	}
}
#endif /* RTCONFIG_CONCURRENTREPEATER */

#ifdef RTL_WTDOG
void watchdog_func()
{
	FILE *file;
	file = fopen("/proc/watchdog_kick","w+");
	if(file)
	{
		fputs("111", file);
		fclose(file);
	}
}
#endif

#ifdef RTCONFIG_ALPINE
#define RF_EXTERNAL_TEMPE	"temperature_rfic_external"
#define BB_INTERNAL_TEMPE	"temperature_bbic_internal"
static int preTempe = 0;
enum {
	//ascend degree
	ASCEND_DEGREE_1 = 30,
	ASCEND_DEGREE_2 = 45,
	ASCEND_DEGREE_3 = 50,
	ASCEND_DEGREE_4 = 55,
	ASCEND_DEGREE_5 = 65,
	ASCEND_DEGREE_6 = 80,
	ASCEND_DEGREE_7 = 85,
	ASCEND_DEGREE_8 = 90,
	//descend degree
	DESCEND_DEGREE_1 = 20,
	DESCEND_DEGREE_2 = 40,
	DESCEND_DEGREE_3 = 45,
	DESCEND_DEGREE_4 = 50,
	DESCEND_DEGREE_5 = 53,
	DESCEND_DEGREE_6 = 75,
	DESCEND_DEGREE_7 = 80,
	DESCEND_DEGREE_8 = 85,
};

void set_fan(int curTempe)
{
	//ascend path
	if(preTempe < curTempe)
	{
		if(curTempe >= ASCEND_DEGREE_8)
			system("i2cset -y 0 0x2e 0x22 0xff i");	//100% duty
		else if(curTempe >= ASCEND_DEGREE_7)
			system("i2cset -y 0 0x2e 0x22 0xe4 i");	//88% duty
		else if(curTempe >= ASCEND_DEGREE_6)
			system("i2cset -y 0 0x2e 0x22 0xd5 i");	//83% duty
		else if(curTempe >= ASCEND_DEGREE_5)
			system("i2cset -y 0 0x2e 0x22 0xc7 i");	//77% duty
		else if(curTempe >= ASCEND_DEGREE_4)
			system("i2cset -y 0 0x2e 0x22 0xab i");	//66% duty
		else if(curTempe >= ASCEND_DEGREE_3)
			system("i2cset -y 0 0x2e 0x22 0x9c i");	//61% duty
		else if(curTempe >= ASCEND_DEGREE_2)
			system("i2cset -y 0 0x2e 0x22 0x8e i");	//55% duty
		else if(curTempe >= ASCEND_DEGREE_1)
			system("i2cset -y 0 0x2e 0x22 0x72 i");	//44% duty
	}
	//descend path
	if(preTempe > curTempe)
	{
		if(curTempe <= DESCEND_DEGREE_2)
			system("i2cset -y 0 0x2e 0x22 0x72 i");	//44% duty
		else if(curTempe <= DESCEND_DEGREE_3)
			system("i2cset -y 0 0x2e 0x22 0x8e i");	//55% duty
		else if(curTempe <= DESCEND_DEGREE_4)
			system("i2cset -y 0 0x2e 0x22 0x9c i");	//61% duty
		else if(curTempe <= DESCEND_DEGREE_5)
			system("i2cset -y 0 0x2e 0x22 0xab i");	//66% duty
		else if(curTempe <= DESCEND_DEGREE_6)
			system("i2cset -y 0 0x2e 0x22 0xc7 i");	//77% duty
		else if(curTempe <= DESCEND_DEGREE_7)
			system("i2cset -y 0 0x2e 0x22 0xd5 i");	//83% duty
		else if(curTempe <= DESCEND_DEGREE_8)
			system("i2cset -y 0 0x2e 0x22 0xe4 i");	//88% duty
	}
}
void fan_check()
{	char *buffer;
	char *tmp, *p;
	int tTempe;
	system("qcsapi_pcie get_temperature > /tmp/qtn_tmpereture.txt");

	buffer = read_whole_file("/tmp/qtn_tmpereture.txt");
	if(buffer)
	{
		tmp = p = buffer;
		if((tmp = strstr(tmp, RF_EXTERNAL_TEMPE)))
		{
			if((tmp = strchr(tmp, '=')))
			{
				tmp++;
				p = tmp;
				tTempe = atoi(p);
				if(preTempe != tTempe)
					set_fan(tTempe);
			}
		}

		if((tmp = strstr(tmp, BB_INTERNAL_TEMPE)))
		{
			if((tmp = strchr(tmp, '=')))
			{
				tmp++;
				p = tmp;
				tTempe = atoi(p);
				if(preTempe != tTempe)
					set_fan(tTempe);
			}
		}
		free(buffer);
	}
	unlink("/tmp/qtn_tmpereture.txt");
}
#endif

/* wathchdog is runned in NORMAL_PERIOD, 1 seconds
 * check in each NORMAL_PERIOD
 *	1. button
 *
 * check in each NORAML_PERIOD*10
 *
 *	1. time-dependent service
 */

void watchdog(int sig)
{
	int period;

#ifdef RTL_WTDOG
	watchdog_func();
#endif
	/* handle button */
	btn_check();

	if (nvram_match("asus_mfg", "0")
#if defined(RTCONFIG_LED_BTN) || defined(RTCONFIG_WPS_ALLLED_BTN)
		&& nvram_get_int("AllLED")
#endif
	)
#if defined(RTCONFIG_CONCURRENTREPEATER)
#if defined(RTCONFIG_QCA)
	qca_wps_state_check();
#endif
#if !defined(RTCONFIG_AMAS)
	bridge_check();
#endif
#else
	service_check();
#endif

#if defined(RTCONFIG_QCA) && defined(RTCONFIG_WIGIG)
	wigig_temperatore_check();
#endif

	/* some io func will delay whole process in urgent mode, move this to sw_devled process */
	//led_check();

#ifdef RTCONFIG_CONCURRENTREPEATER
#ifdef RTCONFIG_QCA
	qca_wps_state_check();
#endif
#ifdef RTCONFIG_REALTEK
	rtk_wps_state_check();
	rtk_wl_led();
#endif
#endif

#ifdef RTCONFIG_RALINK
	if (need_restart_wsc) {
		char word[256], *next, ifnames[128];

		strcpy(ifnames, nvram_safe_get("wl_ifnames"));
		foreach (word, ifnames, next) {
			SKIP_ABSENT_FAKE_IFACE(word);
			eval("iwpriv", word, "set", "WscConfMode=0");
		}

		need_restart_wsc = 0;
		start_wsc_pin_enrollee();
	}
#endif
#ifdef RTCONFIG_SWMODE_SWITCH
	swmode_check();
#endif
#ifdef WEB_REDIRECT
	wanduck_check();
#endif

	/* if timer is set to less than 1 sec, then bypass the following */
	if (itv.it_value.tv_sec == 0)
		return;

#ifdef RTCONFIG_WIFI_SON
	if (nvram_match("x_Setting", "1")) {
		link_pap_status();
		if (f_exists("/tmp/hyd.conf"))
		{
			static int invalid_state=0;
			pid_t *pidList;
			pid_t *pl;
			int count;
			static int hyd_wake_cnt=0, hyd_reset_cnt=0;
			static long hyd_last_wake_time=0;
			long uptime_now;
#ifdef RTCONFIG_ETHBACKHAUL
			static int chaos_eth_state=0;
#endif

			uptime_now = uptime();
			if(uptime_now- atol(nvram_get("hyd_cfg_time"))>40)
			{
				pidList = find_pid_by_name("hyd");
				count=0;
				for (pl = pidList; *pl; pl++)
					count++;
				if (count!=1)
					invalid_state++;
				else
					invalid_state=0;
				if (invalid_state >= 2) {
					if (count==0) {
						if ((uptime_now-hyd_last_wake_time)>10) { /* reset */
							hyd_wake_cnt=0;
							hyd_reset_cnt=0;
						}
						hyd_last_wake_time = uptime_now;
						hyd_wake_cnt++;
						if (hyd_wake_cnt > 5) { /* something wrong, regen hyd config */
							hyd_wake_cnt=0;
							hyd_reset_cnt++;
							if (hyd_reset_cnt > 2) { /* serious situation... */
								logmessage("HYD", "hyd cannot startup, reboot!");
								eval("reboot");
							} else {
								char lbuf[40];
								_dprintf("[[[WATCHDOG]]] : reset hyd process!\n");
								/* log some information */
								logmessage("HYD", "==================================");
								system("ifconfig -a | logger -s");
								logmessage("HYD", "==================================");
								system("iwconfig | logger -s");
								logmessage("HYD", "==================================");
								system("cat /tmp/hyd.conf | logger -s");
								logmessage("HYD", "==================================");
								eval("hive_hyd");
								sprintf(lbuf, "%lu", uptime_now-40);
								nvram_set("hyd_cfg_time", lbuf);
							}
						} else {
							_dprintf("[[[WATCHDOG]]] : wakup hyd\n");
							eval("hyd","-C","/tmp/hyd.conf");
						}
					} else {
						int tmp_count;
						for (tmp_count=0; tmp_count<=(count-1); tmp_count++) {
							if (pidList[tmp_count]>1) {
								_dprintf("[[[WATCHDOG]]] : kill duplicated hyd[%d]\n", pidList[tmp_count]);
								kill(pidList[tmp_count], SIGKILL);
							}
						}
					}
				}
				free(pidList);
			}
#ifdef RTCONFIG_ETHBACKHAUL
			if (nvram_get_int("chaos_eth_daemon")) {
				if (chaos_eth_state++ > 10) {
					pidList = find_pid_by_name("eth_bh_mon");
					count=0;
					for (pl = pidList; *pl; pl++)
						count++;
					free(pidList);
					if (count==0) {
						pid_t eth_pid;
						char *ethmon[]={"eth_bh_mon", NULL};
						_dprintf("[[[WATCHDOG]]] : wakup eth_bh_mon\n");
						_eval(ethmon, NULL, 0, &eth_pid);
					} else
						nvram_unset("chaos_eth_daemon");
				}
			} else
				chaos_eth_state=0;
#endif
		}
	}
#if defined(RTCONFIG_BT_CONN)
	else if (nvram_match("x_Setting", "0")) {
/*		if (!pids("bluetoothd"))
			start_bluetooth_service();
*/
		bt_turn_off_service();
	}
#endif
#endif

#if (defined(PLN12) || defined(PLAC56) || defined(PLAC66U))
	client_check();
#endif

#ifdef RTCONFIG_AMAS
	onboarding_check();
#endif

	if (!nvram_match("asus_mfg", "0"))
		return;

	watchdog_period = (watchdog_period + 1) % 30;
#ifdef WATCHDOG_PERIOD2
	watchdog_period2 = (watchdog_period2 + 1) % 10;
#endif

#ifdef RTCONFIG_BCMARM
	if ((u3_chk_life < 20)
#ifdef RTAC68U
		&& hw_usb_cap()
#endif
	) {
		chkusb3_period = (chkusb3_period + 1) % u3_chk_life;
		if (!chkusb3_period && nvram_get_int("usb_usb3") &&
		    ((nvram_match("usb_path1_speed", "12") &&
		      !nvram_match("usb_path1", "printer") && !nvram_match("usb_path1", "modem")) ||
		     (nvram_match("usb_path2_speed", "12") &&
		      !nvram_match("usb_path2", "printer") && !nvram_match("usb_path2", "modem")))) {
			_dprintf("force reset usb pwr\n");
			stop_usb_program(1);
			sleep(1);
			set_pwr_usb(0);
			sleep(3);
			set_pwr_usb(1);
			u3_chk_life *= 2;
		}
	}
#endif

#ifdef BTN_SETUP
	if (btn_pressed_setup >= BTNSETUP_START)
		return;
#endif

#ifdef WATCHDOG_PERIOD2
	if (watchdog_period2) {
		if (!watchdog_period)
			goto wdp;
		return;
	}
#ifdef RTCONFIG_BONDING
	nvram_set_int("bondst", (bs = get_bonding_status()));

	if(bs!=bs_pre)
		logmessage("BONDING", bs_desc[bs]);
	bs_pre = bs;
#endif
#endif

	if (watchdog_period)
		return;

#ifdef WATCHDOG_PERIOD2
wdp:
#endif
#ifdef CONFIG_BCMWL5
#if !(defined(HND_ROUTER) && defined(RTCONFIG_HNDMFG))
	if (factory_debug())
#endif
#else
	if (IS_ATE_FACTORY_MODE())
#endif
	{
		return;
	}

#ifdef RTCONFIG_USER_LOW_RSSI
	rssi_check();
#endif

	/* check for time-related services */
	timecheck();
#ifdef RTCONFIG_BACKUP_LOG
#ifdef RTCONFIG_USB
	bk_center_main();
#endif
#endif

	/* Force a DDNS update every "x" days - default is 21 days */
	period = nvram_get_int("ddns_refresh_x");
	if ((period) && (++ddns_update_timer >= (DAY_PERIOD * period))) {
		ddns_update_timer = 0;
		nvram_set("ddns_updated", "0");
	}

	ddns_check();
	networkmap_check();
	httpd_check();
	dnsmasq_check();
#ifdef RTCONFIG_NEW_USER_LOW_RSSI
	roamast_check();
#endif
#ifdef RTAC87U
	qtn_module_check();
#endif
#ifdef RTCONFIG_ALPINE
	fan_check();
#endif
#ifdef RTCONFIG_LANTIQ
	wave_monitor_check();
#endif
//#if defined(RTCONFIG_JFFS2LOG) && defined(RTCONFIG_JFFS2)
#if defined(RTCONFIG_JFFS2LOG) && (defined(RTCONFIG_JFFS2)||defined(RTCONFIG_BRCM_NAND_JFFS2))
	syslog_commit_check();
#endif
#if defined(RTCONFIG_USB_MODEM)
	modem_log_check();
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	int modem_unit;

	for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit)
		modem_flow_check(modem_unit);
#endif
#endif
	auto_firmware_check();

#ifdef RTCONFIG_BWDPI
	auto_sig_check();		// libbwdpi.so
	web_history_save();		// libbwdpi.so
	AiProtectionMonitor_mail_log();	// libbwdpi.so
	tm_eula_check();		// libbwdpi.so
#endif

#ifdef RTCONFIG_NOTIFICATION_CENTER
	alert_mail_service();
	ntevent_intranet_usage_insight();
#endif

	check_hour_monitor_service();

#if 0 //#if defined(RTCONFIG_TOR) && (defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2))
	if (nvram_get_int("Tor_enable"))
		Tor_microdes_check();
#endif

#ifdef RTCONFIG_ERP_TEST
	ErP_Test();
#endif

#if defined(RTCONFIG_USB) && defined(RTCONFIG_NOTIFICATION_CENTER) && defined(RTCONFIG_CLOUDSYNC)
	ntevent_disk_usage_check();
#endif

#if defined(RTAC1200G) || defined(RTAC1200GP)
	if (pids("wdg_monitor"))
		kill_pidfile_s("/var/run/wdg_monitor.pid", SIGUSR1);
#endif

#ifdef RTCONFIG_AMAS
	amas_ctl_check();
#endif
#ifdef RTCONFIG_CFGSYNC
	cfgsync_check();
#endif
#ifdef RTCONFIG_TUNNEL
	mastiff_check();
#endif
}

#if ! (defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
void watchdog02(int sig)
{
	watchdog_check();
	return;
}
#endif /* ! (RTCONFIG_QCA || RTCONFIG_RALINK) */

int
watchdog_main(int argc, char *argv[])
{
	FILE *fp;
	const struct mfg_btn_s *p;

	/* write pid */
	if ((fp = fopen("/var/run/watchdog.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

#ifdef RTCONFIG_CONCURRENTREPEATER
	if (sw_mode() == SW_MODE_REPEATER) // Repeater mode
		nvram_set("wps_cli_state", "0");//init

	init_wl_br_status();
#endif

#ifdef RTCONFIG_SWMODE_SWITCH
	pre_sw_mode=sw_mode();
#endif

#ifdef RTCONFIG_BCMWL6
	if (mediabridge_mode())
		wlonunit = nvram_get_int("wlc_band");
#endif

#ifdef RTCONFIG_RALINK
	doSystem("iwpriv %s set WatchdogPid=%d", WIF_2G, getpid());
#if defined(RTCONFIG_HAS_5G)
	doSystem("iwpriv %s set WatchdogPid=%d", WIF_5G, getpid());
#endif	/* RTCONFIG_HAS_5G */
#endif	/* RTCONFIG_RALINK */

	init_sig();

#ifdef RTCONFIG_DSL //Paul add 2012/6/27
	nvram_set("dsltmp_syncloss", "0");
	nvram_set("dsltmp_syncloss_apply", "0");
#endif
	nvram_unset("wps_ign_btn");

	/* Set nvram variables which are used to record button state in mfg mode to "0".
	 * Because original code in ate.c rely on such behavior.
	 * If those variables are unset here, related ATE command print empty string
	 * instead of "0".
	 */
	for (p = &mfg_btn_table[0]; p->id < BTN_ID_MAX; ++p) {
		nvram_set(p->nv, "0");
	}

	if (!pids("ots"))
		start_ots();

	setenv("TZ", nvram_safe_get("time_zone_x"), 1);

	_dprintf("TZ watchdog\n");
	/* set timer */
	alarmtimer(NORMAL_PERIOD, 0);

	led_control_normal();
#ifdef RTL_WTDOG
	start_rtl_watchdog();
#endif

	/* Most of time it goes to sleep */
	int watchsig_dbg = nvram_get_int("watchsig");
	while (1)
	{
		while(sigbones)
			put_all_dogs();
		
		pause();

		if(watchsig_dbg) {
			sigset_t waitset;
			sigpending(&waitset);
			_dprintf("(now blocked sig:%s%s%s%s%s%s%s)\n", 
							sigismember(&waitset, SIGALRM)?"SIGALRM ":"",
							sigismember(&waitset, SIGCHLD)?"SIGCHLD ":"",
							sigismember(&waitset, SIGUSR1)?"SIGUSR1 ":"",
							sigismember(&waitset, SIGUSR2)?"SIGUSR2 ":"",
							sigismember(&waitset, SIGTSTP)?"SIGTSTP ":"",
							sigismember(&waitset, SIGHUP) ?"SIGHUP ":"",
							sigismember(&waitset, SIGTTIN)?"SIGTTIN ":""
			);
			if(nvram_match("reset_sig", "1"))
				unblock_sigs();
		}
	}

	return 0;
}

#if ! (defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
/* to check watchdog alive */
int watchdog02_main(int argc, char *argv[])
{
	FILE *fp;
	/* write pid */
	if ((fp = fopen("/var/run/watchdog02.pid", "w")) != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}
	/* set the signal handler */
	signal(SIGALRM, watchdog02);

	/* set timer */
	alarmtimer02(10, 0);
	/* Most of time it goes to sleep */
	while(1) {
		pause();
	}
	return 0;
}
#endif	/* ! (RTCONFIG_QCA || RTCONFIG_RALINK) */

#ifdef SW_DEVLED
/* to control misc dev led */
int sw_devled_main(int argc, char *argv[])
{
	FILE *fp;
	/* write pid */
	if ((fp = fopen("/var/run/sw_devled.pid", "w")) != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

#if defined (RTCONFIG_LED_BTN) || defined (RTCONFIG_WPS_ALLLED_BTN)
	swled_alloff_counts = nvram_get_int("offc");
#endif

	init_sig_swled();
	/* set timer */
	alarmtimer(NORMAL_PERIOD, 0);

#ifdef BLUECAVE
	kill_pidfile_s("/var/run/sw_devled.pid", SIGUSR1);
#endif

	/* Most of time it goes to sleep */
	while(1) {
		while(sigbones)
			put_all_dogs();
		pause();
	}
	return 0;
}
#endif

#if defined(RTAC1200G) || defined(RTAC1200GP)
/* workaroud of watchdog timer shutdown */
int wdg_monitor_main(int argc, char *argv[])
{
	FILE *fp;
	if ((fp = fopen("/var/run/wdg_monitor.pid", "w")) != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	init_sig_wmon();

	alarmtimer(WDG_MONITOR_PERIOD, 0);

	while(1) {
		while(sigbones)
			put_all_dogs();
		pause();
	}
	return 0;
}
#endif
