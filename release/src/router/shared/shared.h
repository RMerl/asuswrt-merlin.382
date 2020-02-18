#ifndef __SHARED_H__
#define __SHARED_H__

#include <rtconfig.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <endian.h>
#ifndef _LINUX_IF_H
#include <net/if.h>
#endif
#include <rtstate.h>
#include <stdarg.h>

#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_BCMWL6
#include "bcmwifi_channels.h"
#else
#include "bcmwifi.h"
#endif
#endif

#ifdef RTCONFIG_REALTEK
#include "realtek_common.h"
#include <time.h>
#endif
#if defined(RTCONFIG_USB) || defined(RPAC51)
#include <mntent.h>	// !!TB
#endif

#ifdef RTCONFIG_TRAFFIC_LIMITER
#include <tld_utils.h>
#endif

#ifdef HND_ROUTER
#include "ethswctl.h"
#endif

#ifdef RTCONFIG_LP5523
#include "lp5523led.h"
#endif /* RTCONFIG_LP5523 */

#include <sys/stat.h>
#include <ftw.h>
#include "network_utility.h"

/* Endian conversion functions. */
#define __bswap16(x) (uint16_t)	( \
				(((uint16_t)(x) & 0x00ffu) << 8) | \
				(((uint16_t)(x) & 0xff00u) >> 8))

#define __bswap32(x) (uint32_t)	( \
				(((uint32_t)(x) & 0xff000000u) >> 24) | \
				(((uint32_t)(x) & 0x00ff0000u) >>  8) | \
				(((uint32_t)(x) & 0x0000ff00u) <<  8) | \
				(((uint32_t)(x) & 0x000000ffu) << 24))

#define __bswap64(x) (uint64_t)	( \
				(((uint64_t)(x) & 0xff00000000000000ull) >> 56) | \
				(((uint64_t)(x) & 0x00ff000000000000ull) >> 40) | \
				(((uint64_t)(x) & 0x0000ff0000000000ull) >> 24) | \
				(((uint64_t)(x) & 0x000000ff00000000ull) >>  8) | \
				(((uint64_t)(x) & 0x00000000ff000000ull) <<  8) | \
				(((uint64_t)(x) & 0x0000000000ff0000ull) << 24) | \
				(((uint64_t)(x) & 0x000000000000ff00ull) << 40) | \
				(((uint64_t)(x) & 0x00000000000000ffull) << 56))

#if __BYTE_ORDER == __BIG_ENDIAN
#ifndef le16_to_cpu
#define le16_to_cpu(x) __bswap16(x)
#endif
#ifndef le32_to_cpu
#define le32_to_cpu(x) __bswap32(x)
#endif
#ifndef le64_to_cpu
#define le64_to_cpu(x) __bswap64(x)
#endif
#ifndef be16_to_cpu
#define be16_to_cpu(x) (x)
#endif
#ifndef be32_to_cpu
#define be32_to_cpu(x) (x)
#endif
#ifndef cpu_to_le16
#define cpu_to_le16(x) __bswap16(x)
#endif
#ifndef cpu_to_le32
#define cpu_to_le32(x) __bswap32(x)
#endif
#ifndef cpu_to_be32
#define cpu_to_be32(x) (x)
#endif
#else	/* __BYTE_ORDER != __BIG_ENDIAN */
#ifndef le16_to_cpu
#define le16_to_cpu(x) (x)
#endif
#ifndef le32_to_cpu
#define le32_to_cpu(x) (x)
#endif
#ifndef le64_to_cpu
#define le64_to_cpu(x) (x)
#endif
#ifndef be16_to_cpu
#define be16_to_cpu(x) __bswap16(x)
#endif
#ifndef be32_to_cpu
#define be32_to_cpu(x) __bswap32(x)
#endif
#ifndef cpu_to_le16
#define cpu_to_le16(x) (x)
#endif
#ifndef cpu_to_le32
#define cpu_to_le32(x) (x)
#endif
#ifndef cpu_to_be32
#define cpu_to_be32(x) __bswap32(x)
#endif
#endif	/* __BYTE_ORDER == __BIG_ENDIAN */

/* btn_XXX_gpio, led_XXX_gpio */
#define GPIO_ACTIVE_LOW 0x1000
#define GPIO_BLINK_LED	0x2000
#define GPIO_PIN_MASK	0x00FF
/* bit8:  USB port 1
 * bit9:  USB port 2
 * bit10: USB port 3
 * bit11: USB port 4
 * If bit8~bit11 is equal to zero, all USB ports.
 */
#define GPIO_EJUSB_MASK	0x0F00

#define GPIO_EJUSB_SHIFT	(8)
#define GPIO_EJUSB_PORT(p)	(1 << (GPIO_EJUSB_SHIFT + p - 1))	/* p = 1 ~ 4 */
#define GPIO_EJUSB_ALL_PORT	(0 << GPIO_EJUSB_SHIFT)

#define GPIO_DIR_IN	0
#define GPIO_DIR_OUT	1

#define PROC_IRQ		"/proc/irq"
#define SYS_CLASS_NET		"/sys/class/net"

/**
 * skb->mark usage
 * 1.	bit 28~31:	Load-balance rule, IPTABLES_MARK_LB_*
 * 2.	bit  6~7 :	Facebook Wi-Fi, FBWIFI_MARK_*
 * 3.	bit  0~5 :	QoS (T.QoS: bit 0~2, BWLIT: bit 0~5)
 * 4.   bit 31~8 :	TrendMicro adaptive QoS needs bit 8~31.
 */
#define IPTABLES_MARK_LB_SET(x)	((((x)&0xFU)|0x8)<<28)			/* mark for load-balance, bit 28~31, bit31 is always 1. */
#define IPTABLES_MARK_LB_MASK	IPTABLES_MARK_LB_SET(0xf)
#define FBWIFI_MARK_SET(x)	(((x)&0x3U)<<6)				/* Facebook Wi-Fi: bit 6~7 */
#define FBWIFI_MARK_MASK	FBWIFI_MARK_SET(0x3)
#define FBWIFI_MARK_INV_MASK	(~(FBWIFI_MARK_SET(0x3)))
#define QOS_MASK		(0x3F)

#ifdef RTCONFIG_INTERNAL_GOBI
#define DEF_SECOND_WANIF	"usb"
#elif defined(RTCONFIG_WANPORT2) && !defined(RTAD7200)
#define DEF_SECOND_WANIF	"wan2"
#else
#define DEF_SECOND_WANIF	"none"
#endif

#define ACTION_LOCK		"/var/lock/action"

#ifdef RTCONFIG_PUSH_EMAIL
//#define logmessage logmessage_push
#define logmessage logmessage_normal	//tmp
#else
#define logmessage logmessage_normal
#endif

#ifdef RTCONFIG_DSL
#define SYNC_LOG_FILE "/tmp/adsl/sync_status_log.txt"
#define LOG_RECORD_FILE "/tmp/adsl/log_record.txt"
#endif

#define Y2K			946684800UL		// seconds since 1970

#define ASIZE(array)	(sizeof(array) / sizeof(array[0]))

#ifdef LINUX26
#define	MTD_DEV(arg)	"/dev/mtd"#arg
#define	MTD_BLKDEV(arg)	"/dev/mtdblock"#arg
#define	DEV_GPIO(arg)	"/dev/gpio"#arg
#else
#define	MTD_DEV(arg)	"/dev/mtd/"#arg
#define	MTD_BLKDEV(arg)	"/dev/mtdblock/"#arg
#define	DEV_GPIO(arg)	"/dev/gpio/"#arg
#endif

#if defined(RTCONFIG_DEFAULT_AP_MODE)
#define DUT_DOMAIN_NAME "ap.asus.com"
#elif defined(RTCONFIG_CONCURRENTREPEATER)
#define DUT_DOMAIN_NAME "repeater.asus.com"
#else
#define DUT_DOMAIN_NAME "router.asus.com"
#endif
#define OLD_DUT_DOMAIN_NAME1 "www.asusnetwork.net"
#define OLD_DUT_DOMAIN_NAME2 "www.asusrouter.com"

/* index page defined for httpd and wanduck */
#if defined(GTAC5300)
#define INDEXPAGE "GameDashboard.asp"
#else
#define INDEXPAGE "index.asp"
#endif

#define NETWORKMAP_PAGE "index.asp"

#if defined(RTCONFIG_SAVEJFFS)
#define JFFS_CFGS_HDR	"JCFG"
#define JFFS_CFGS	"/tmp/jffs_cfgs"
#endif

#ifndef AC2900
#if defined(VZWAC1300)
#define SSID_PREFIX	"VZW"
#else
#define SSID_PREFIX	"ASUS"
#endif
#else
#include "version.h"
#define SSID_PREFIX	RT_BUILD_NAME
#endif

//version.c
extern const char *rt_version;
extern const char *rt_serialno;
extern const char *rt_rcno;
extern const char *rt_extendno;
extern const char *rt_buildname;
extern const char *rt_buildinfo;
extern const char *rt_swpjverno;

extern void set_basic_fw_name(void);

#ifdef DEBUG_NOISY
#define _dprintf		cprintf
#define csprintf		cprintf
#else
#define _dprintf(args...)	do { } while(0)
#define csprintf(args...)	do { } while(0)
#endif

#define ASUS_STOP_COMMIT	"asus_stop_commit"
#define LED_CTRL_HIPRIO 	"LED_CTRL_HIPRIO"

#ifdef RTCONFIG_IPV6
/* NOTE: Do not insert new entries in the middle of this enum,
 * always add them to the end! */
enum {
	IPV6_DISABLED = 0,
	IPV6_NATIVE_DHCP,
	IPV6_6TO4,
	IPV6_6IN4,
	IPV6_6RD,
	IPV6_MANUAL,
	IPV6_PASSTHROUGH
};

#ifndef RTF_UP
/* Keep this in sync with /usr/src/linux/include/linux/route.h */
#define RTF_UP		0x0001  /* route usable			*/
#define RTF_GATEWAY     0x0002  /* destination is a gateway	*/
#define RTF_HOST	0x0004  /* host entry (net otherwise)	*/
#define RTF_REINSTATE   0x0008  /* reinstate route after tmout	*/
#define RTF_DYNAMIC     0x0010  /* created dyn. (by redirect)	*/
#define RTF_MODIFIED    0x0020  /* modified dyn. (by redirect)	*/
#define RTF_REJECT      0x0200  /* Reject route			*/
#endif
#ifndef RTF_DEFAULT
#define	RTF_DEFAULT	0x00010000	/* default - learned via ND	*/
#define	RTF_ADDRCONF	0x00040000	/* addrconf route - RA		*/
#define	RTF_CACHE	0x01000000	/* cache entry			*/
#endif
#define IPV6_MASK (RTF_GATEWAY|RTF_HOST|RTF_DEFAULT|RTF_ADDRCONF|RTF_CACHE)
#endif

#ifdef RTCONFIG_LANTIQ
enum {
	WAVE_FLAG_NORMAL=0,
	WAVE_FLAG_QIS,
	WAVE_FLAG_ACL,
	WAVE_FLAG_WPS,
	WAVE_FLAG_WDS,
	WAVE_FLAG_ADV,
	WAVE_FLAG_VAP,
	WAVE_FLAG_NETWORKMAP,
	WAVE_FLAG_APP_NORMAL,
	WAVE_FLAG_APP_ADV,
	WAVE_FLAG_APP_NORMAL_ADV,
	WAVE_FLAG_APP_WPS,
	WAVE_FLAG_APP_VAP,
	WAVE_FLAG_APP_NETWORKMAP
};

enum {
	WAVE_ACTION_IDLE=0,
	WAVE_ACTION_INIT=1,
	WAVE_ACTION_WEB=3,
	WAVE_ACTION_RE_AP2G_ON=4,
	WAVE_ACTION_RE_AP2G_OFF=5,
	WAVE_ACTION_RE_AP5G_ON=6,
	WAVE_ACTION_RE_AP5G_OFF=7,
	WAVE_ACTION_SET_CHANNEL_2G=8,
	WAVE_ACTION_SET_CHANNEL_5G=9,
	WAVE_ACTION_OPENACL_FOR_OBD=10,
	WAVE_ACTION_RECOVERACL_FOR_OBD=11,
	WAVE_ACTION_SETALLOWACL_2G=12,
	WAVE_ACTION_SETALLOWACL_5G=13,
	WAVE_ACTION_CLIENT2G_ON=14,
	WAVE_ACTION_CLIENT2G_OFF=15,
	WAVE_ACTION_CLIENT5G_ON=16,
	WAVE_ACTION_CLIENT5G_OFF=17,
	WAVE_ACTION_SET_WPS2G_CONFIGURED=18,
	WAVE_ACTION_SET_WPS5G_CONFIGURED=19,
};
#endif

#define GIF_LINKLOCAL  0x0001  /* return link-local addr */
#define GIF_PREFIXLEN  0x0002  /* return prefix length */
#define GIF_PREFIX     0x0004  /* return prefix, not addr */

#define EXTEND_AIHOME_API_LEVEL		17

#define EXTEND_HTTPD_AIHOME_VER		0

#define EXTEND_ASSIA_API_LEVEL		1

#define S_53134		53134

#ifdef RTCONFIG_CFGSYNC
#define CFG_PREFIX      "CFG"

#ifdef RTCONFIG_ADV_RAST
enum romaingEvent {
        EID_RM_STA_MON = 1,
        EID_RM_STA_MON_REPORT,
        EID_RM_STA_CANDIDATE,
        EID_RM_STA_ACL,
	EID_RM_STA_FILTER,
	EID_RM_MAX
};
#define RAST_IPC_MAX_CONNECTION		5
#define RAST_IPC_SOCKET_PATH		"/etc/rast_ipc_socket"
#define RAST_PREFIX     "RAST"
/* key name of json from rast */
#define RAST_EVENT_ID   "EID"
#define RAST_STA        "STA"
#define RAST_STA_2G	"STA_2G"
#define RAST_STA_5G	"STA_5G"
#define RAST_AP         "AP"
#define RAST_RSSI       "RSSI"
#define RAST_BAND       "BAND"
#define RAST_PEERIP     "PIP"
#define RAST_STATUS     "STATUS"
#define RAST_CANDIDATE_AP "CANDIDATE"
#define RAST_STA_RSSI	"STA_RSSI"
#define RAST_CANDIDATE_AP_RSSI	"AP_RSSI"
#define RAST_CANDIDATE_AP_RSSI_CRITERIA  "AP_RSSI_CRITERIA"

#define RAST_JVALUE_BAND_2G "2"
#define RAST_JVALUE_BAND_5G "1"

#endif	//END RTCONFIG_ADV_RAST
#endif	//END RTCONFIG_CFGSYNC

#ifdef RTCONFIG_AMAS
#define OUI_ASUS      "\xF8\x32\xE4"
#endif

enum {
	FROM_BROWSER,
	FROM_ASUSROUTER,
	FROM_DUTUtil,
	FROM_ASSIA,
	FROM_IFTTT,
	FROM_ALEXA,
	FROM_UNKNOWN
};

enum {
	ACT_IDLE,
	ACT_TFTP_UPGRADE_UNUSED,
	ACT_WEB_UPGRADE,
	ACT_WEBS_UPGRADE_UNUSED,
	ACT_SW_RESTORE,
	ACT_HW_RESTORE,
	ACT_ERASE_NVRAM,
	ACT_NVRAM_COMMIT,
	ACT_REBOOT,
	ACT_UNKNOWN
};

typedef struct {
	int count;
	struct {
		struct in_addr addr;
		unsigned short port;
	} dns[3];
} dns_list_t;

typedef struct {
	int count;
	struct {
		char name[IFNAMSIZ + 1];
		char ip[sizeof("xxx.xxx.xxx.xxx") + 1];
	} iface[2];
} wanface_list_t;

#define IP2UINT(a,b,c,d)	(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | ((uint32_t)d))
struct ip_mask_s {
	uint32_t	ip;	/* host-endian IPv4 address */
	uint32_t	cidr;	/* CIDR netmask */
};

#if defined(RTCONFIG_TAGGED_BASED_VLAN)
/*  First VLAN br interface is br3  */
#define VLAN_START_IFNAMES_INDEX	5
#define VLAN_MAX_NUM			8
#elif defined(RTCONFIG_PORT_BASED_VLAN)
#define VLAN_START_IFNAMES_INDEX	1
#define VLAN_MAX_NUM			8	/* FIXME */
#endif

#define DHCP_STATICLIST_EXAMPLE		"<00:03:7f:00:00:02>192.168.100.200"
#define SUBNET_RULE_EXAMPLE		"<192.168.100.254>255.255.255.128>1>192.168.100.102>192.168.100.253>864000>12345678901234567890123456789012>192.168.100.100>192.168.100.099>1>"
#define SUBNET_STATICLIST_EXAMPLE	"00:03:7f:20:00:01 192.168.120.101;"
#define STATIC_MAC_IP_BINDING_PER_LAN	64
#define STATIC_MAC_IP_BINDING_PER_VLAN	8

#define EXCLUDE_NET_USB_MODEM		(1U << 0)
#define EXCLUDE_NET_LAN			(1U << 1)
#define EXCLUDE_NET_WAN			(1U << 2)
#define EXCLUDE_NET_PPTP_SERVER		(1U << 3)
#define EXCLUDE_NET_OPENVPN_SERVER	(1U << 4)
#define EXCLUDE_NET_IPSEC_SERVER	(1U << 5)
#define EXCLUDE_NET_IPSEC_CLIENT	(1U << 6)
#define EXCLUDE_NET_VLAN		(1U << 7)
#define EXCLUDE_NET_FREE_WIFI		(1U << 8)
#define EXCLUDE_NET_CAPTIVE_PORTAL	(1U << 9)

/* Combination of above EXCLUDE_NET_XXX */
#define EXCLUDE_NET_ALL_EXCEPT_LAN_VLAN	(~0U & ~(EXCLUDE_NET_LAN | EXCLUDE_NET_VLAN))
#define EXCLUDE_NET_ALL_EXCEPT_COOVACHILLI	(~0U & ~(EXCLUDE_NET_FREE_WIFI | EXCLUDE_NET_CAPTIVE_PORTAL))

#if defined(RTCONFIG_TAGGED_BASED_VLAN) || defined(RTCONFIG_PORT_BASED_VLAN)
struct vlan_rule_s {
	int	enable;
	char	br_if[IFNAMSIZ];	/* maybe empty string if this rule is not enabled. */
#if defined(RTCONFIG_TAGGED_BASED_VLAN)
/* vlan_rulelist example
 * 	<1>1>0>0000>00010001>000F>000F>default>1>0
 * 	<1>2>0>0000>00020002>0000>0000>192.168.5.88/24>1>0
 * 	<1>3>0>0000>00040004>0000>0000>192.168.6.88/24>1>0
 * 	<1>4>0>0000>00080008>0000>0000>192.168.7.88/24>1>0
 * 	<1>5>0>0000>00100010>0000>0000>192.168.8.88/24>1>0
 * 	<1>6>0>0000>00200020>0000>0000>192.168.9.88/24>1>0
 * 	<1>7>0>0000>00400040>0000>0000>192.168.11.88/24>1>0
 * 	<1>8>0>0000>00800080>0000>0000>192.168.12.88/24>1>0
 */
	char	vid[8];
	char	prio[4];
	char	wanportset[6];
	char	lanportset[10];
	char	wl2gset[6];
	char	wl5gset[6];
	char	subnet_name[32];
	char	internet[4];
	char	public_vlan[4];
#elif defined(RTCONFIG_PORT_BASED_VLAN)
	char	desc[32];
	char	portset[32];
	char	wlmap[16];
	char	subnet_name[32];
	char	intranet[16];
#endif
};

struct vlan_rules_s {
	unsigned int		nr_rules;
	struct vlan_rule_s	rules[VLAN_MAX_NUM];
};
#endif

extern char *read_whole_file(const char *target);
extern char *get_line_from_buffer(const char *buf, char *line, const int line_size);
extern char *get_upper_str(const char *const str, char **target);
extern int upper_strcmp(const char *const str1, const char *const str2);
extern int upper_strncmp(const char *const str1, const char *const str2, int count);
extern char *upper_strstr(const char *const str, const char *const target);
#if defined(HND_ROUTER)
// defined (__GLIBC__) && !defined(__UCLIBC__)
size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);
#endif

/*
* Concatenate two strings together into a caller supplied buffer
* @param      s1    first string
* @param      s2    second string
* @param      buf  buffer large enough to hold both strings
* @param       buf_len the length of buf
* @return      buf
*/
static inline char * strlcat_r(const char *s1, const char *s2, char *buf, const size_t buf_len)
{
        strlcpy(buf, s1, buf_len);
        strlcat(buf, s2, buf_len);
        return buf;
}       

extern in_addr_t inet_addr_(const char *addr);
extern int inet_equal(const char *addr1, const char *mask1, const char *addr2, const char *mask2);
extern int inet_intersect(const char *addr1, const char *mask1, const char *addr2, const char *mask2);
extern int inet_deconflict(const char *addr1, const char *mask1, const char *addr2, const char *mask2, struct in_addr *result);

extern void chld_reap(int sig);
extern int get_wan_proto(void);
#ifdef RTCONFIG_IPV6
extern char *ipv6_nvname(const char *name);
extern char *ipv6_nvname_by_unit(const char *name, int unit);
extern int get_ipv6_service(void);
extern int get_ipv6_service_by_unit(int unit);
#define ipv6_enabled()	(get_ipv6_service() != IPV6_DISABLED)
extern const char *ipv6_router_address(struct in6_addr *in6addr);
extern const char *ipv6_gateway_address(void);
#else
#define ipv6_enabled()	(0)
#endif
extern void notice_set(const char *path, const char *format, ...);
#if defined(RTAC1200HP)
extern void led_5g_onoff(void);
#endif
extern void set_action(int a);
extern int check_action(void);
extern int wait_action_idle(int n);
extern int wl_client(int unit, int subunit);
extern const char *_getifaddr(const char *ifname, int family, int flags, char *buf, int size);
extern const char *getifaddr(const char *ifname, int family, int flags);
extern long uptime(void);
extern float uptime2(void);
extern char *wl_nvprefix(char *prefix, int prefix_size, int unit, int subunit);
extern char *wl_nvname(const char *nv, int unit, int subunit);
extern char *wl_nband_name(const char *nband);
extern int wl_wave_unit(int wps_band);
extern char *wl_vifname_wave(int unit, int subunit);
#ifdef RTCONFIG_LANTIQ
extern int wl_client_unit(int unit);
extern char *get_staifname(int unit);
extern int get_wifname_band(char *name);
extern char *get_wififname(int band);
#endif
extern unsigned int get_radio_status(char *ifname);
extern int get_radio(int unit, int subunit);
extern void set_radio(int on, int unit, int subunit);

extern char *nvram_get_r(const char *name, char *buf, size_t buflen);
#define nvram_safe_get_r(name, buf, bufsize) (nvram_get_r(name, buf, bufsize) ? : "")
extern char *nvram_pf_get(char *prefix, const char *name);
#define nvram_pf_safe_get(prefix, name) (nvram_pf_get(prefix, name) ? : "")
extern int nvram_pf_set(char *prefix, const char *name, const char *value);
#define nvram_pf_unset(prefix, name) nvram_pf_set(prefix, name, NULL)
extern int nvram_get_int(const char *key);
extern int nvram_pf_get_int(char *prefix, const char *key);
extern int nvram_set_int(const char *key, int value);
extern int nvram_pf_set_int(char *prefix, const char *key, int value);
extern int nvram_pf_match(char *prefix, char *name, char *match);
extern int nvram_pf_invmatch(char *prefix, char *name, char *invmatch);
extern double nvram_get_double(const char *key);
extern int nvram_set_double(const char *key, double value);
#ifdef HND_ROUTER
extern char *nvram_split_get(const char *key, char *buffer, int maxlen, int maxinst);
extern int nvram_split_set(const char *key, char *value, int size, int maxinst);
#endif

//	extern long nvram_xget_long(const char *name, long min, long max, long def);
extern int nvram_contains_word(const char *key, const char *word);
extern int nvram_is_empty(const char *key);
extern void nvram_commit_x(void);
extern int connect_timeout(int fd, const struct sockaddr *addr, socklen_t len, int timeout);
extern int mtd_getinfo(const char *mtdname, int *part, int *size);
#if defined(RTCONFIG_UBIFS)
extern int ubi_getinfo(const char *ubiname, int *dev, int *part, int *size);
#endif
extern int foreach_wif(int include_vifs, void *param,
	int (*func)(int idx, int unit, int subunit, void *param));

//shutils.c
extern void dbgprintf (const char * format, ...); //Ren
extern void cprintf(const char *format, ...);
extern int _eval(char *const argv[], const char *path, int timeout, int *ppid);
extern char *enc_str(char *str, char *enc_buf);
extern char *dec_str(char *ec_str, char *dec_buf);
extern int num_of_wl_if();

// usb.c
#ifdef RTCONFIG_USB
extern char *detect_fs_type(char *device);
extern struct mntent *findmntents(char *file, int swp,
	int (*func)(struct mntent *mnt, uint flags), uint flags);
extern int find_label_or_uuid(char *dev_name, char *label, char *uuid);
extern void add_remove_usbhost(char *host, int add);

#define DEV_DISCS_ROOT	"/dev/discs"

/* Flags used in exec_for_host calls
 */
#define EFH_1ST_HOST	0x00000001	/* func is called for the 1st time for this host */
#define EFH_1ST_DISC	0x00000002	/* func is called for the 1st time for this disc */
#define EFH_HUNKNOWN	0x00000004	/* host is unknown */
#define EFH_USER	0x00000008	/* process is user-initiated - either via Web GUI or a script */
#define EFH_SHUTDN	0x00000010	/* exec_for_host is called at shutdown - system is stopping */
#define EFH_HP_ADD	0x00000020	/* exec_for_host is called from "add" hotplug event */
#define EFH_HP_REMOVE	0x00000040	/* exec_for_host is called from "remove" hotplug event */
#define EFH_PRINT	0x00000080	/* output partition list to the web response */

typedef int (*host_exec)(char *dev_name, int host_num, char *dsc_name, char *pt_name, uint flags);
extern int exec_for_host(int host, int obsolete, uint flags, host_exec func);
extern int is_no_partition(const char *discname);
#endif //RTCONFIG_USB

#define MODEL_UNUSED(x)	__MDL_UNUSED(x)
#define __MDL_UNUSED(s)	MODEL_unused ## s

/* NOTE: Do not insert new entries in the middle of this enum,
 * always add them to the end! */
enum {
	MODEL_GENERIC = -1,
	MODEL_UNKNOWN = 0,
	MODEL_DSLN55U,
	MODEL_DSLAC68U,
	MODEL_EAN66,
	MODEL_RTN11P,
	MODEL_RTN300,
	MODEL_RTN13U,
	MODEL_RTN14U,
	MODEL_RTAC52U,
	MODEL_RTAC51U,
	MODEL_RTN54U,
	MODEL_RTAC54U,
	MODEL_RTN56UB1,
	MODEL_RTN56UB2,
	MODEL_RTAC1200HP,
	MODEL_RTAC55U,
	MODEL_RTAC55UHP,
	MODEL_RT4GAC55U,
	MODEL_PLN12,
	MODEL_PLAC56,
	MODEL_PLAC66U,
	MODEL_RTAC58U,
	MODEL_RT4GAC53U,
	MODEL_RTAC82U,
	MODEL_MAPAC1300,
	MODEL_MAPAC2200,
	MODEL_VZWAC1300,
	MODEL_MAPAC1750,
	MODEL_MAPAC3000,
	MODEL_RTN36U3,
	MODEL_RTN56U,
	MODEL_RTN65U,
	MODEL_RTN67U,
	MODEL_RTN12,
	MODEL_RTN12B1,
	MODEL_RTN12C1,
	MODEL_RTN12D1,
	MODEL_RTN12VP,
	MODEL_RTN12HP,
	MODEL_RTN12HP_B1,
	MODEL_APN12,
	MODEL_APN12HP,
	MODEL_RTN16,
	MODEL_RTN18U,
	MODEL_RTN15U,
	MODEL_RTN53,
	MODEL_RTN66U,
	MODEL_RTAC66U,
	MODEL_RTAC68U,
	MODEL_UNUSED(__LINE__),
	MODEL_RTAC87U,
	MODEL_RTAC56S,
	MODEL_RTAC56U,
	MODEL_RTAC53U,
	MODEL_RTAC3200,
	MODEL_RTAC88U,
	MODEL_RTAC3100,
	MODEL_RTAC5300,
	MODEL_GTAC5300,
	MODEL_RTN14UHP,
	MODEL_RTN10U,
	MODEL_RTN10P,
	MODEL_RTN10D1,
	MODEL_RTN10PV2,
	MODEL_RTAC1200,
	MODEL_RTAC1200G,
	MODEL_RTAC1200GP,
	MODEL_RTAC1200GA1,
	MODEL_RTAC1200GU,
	MODEL_RPAC66,
	MODEL_RPAC51,
	MODEL_RTAC51UP,
	MODEL_RTAC53,
	MODEL_RTN11P_B1,
	MODEL_RPAC87,
	MODEL_RTAC85U,
	MODEL_RTN800HP,
	MODEL_RTAC88N,
	MODEL_BRTAC828,
	MODEL_RTAC88S,
	MODEL_RPAC53,
	MODEL_RPAC68U,
	MODEL_RPAC55,
	MODEL_RTAC86U,
	MODEL_GTAC9600,
	MODEL_BLUECAVE,
	MODEL_RTAD7200,
	MODEL_GTAX6000,
	MODEL_GTAX6000N,
	MODEL_GTAX6000S,
};

/* NOTE: Do not insert new entries in the middle of this enum,
 * always add them to the end! */
enum {
	SWITCH_GENERIC = -1,
	SWITCH_UNKNOWN = 0,
	SWITCH_BCM5325,
	SWITCH_BCM53115,
	SWITCH_BCM53125,
	SWITCH_BCM5301x,
	SWITCH_BCM5301x_EX,
	SWITCH_BCM53134,
	SWITCH_RT305x,
	SWITCH_RT3350,
	SWITCH_RT3352,
	SWITCH_RT5350,
	SWITCH_MT7620,
	SWITCH_MT7621,
	SWITCH_MT7628,
	SWITCH_RTL8365MB,
	SWITCH_RTL8367MB,
	SWITCH_RTL8367RB,
	SWITCH_RTL8370M,
};

#define RTCONFIG_NVRAM_VER "1"

/* NOTE: Do not insert new entries in the middle of this enum,
 * always add them to the end! The numeric Hardware ID value is
 * stored in the configuration file, and is used to determine
 * whether or not this config file can be restored on the router.
 */
enum {
	HW_BCM4702,
	HW_BCM4712,
	HW_BCM5325E,
	HW_BCM4704_BCM5325F,
	HW_BCM5352E,
	HW_BCM5354G,
	HW_BCM4712_BCM5325E,
	HW_BCM4704_BCM5325F_EWC,
	HW_BCM4705L_BCM5325E_EWC,
	HW_BCM5350,
	HW_BCM5356,
	HW_BCM4716,
	HW_BCM4718,
	HW_BCM4717,
	HW_BCM5365,
	HW_BCM4785,
	HW_UNKNOWN
};

#define SUP_SES			(1 << 0)
#define SUP_BRAU		(1 << 1)
#define SUP_AOSS_LED		(1 << 2)
#define SUP_WHAM_LED		(1 << 3)
#define SUP_HPAMP		(1 << 4)
#define SUP_NONVE		(1 << 5)
#define SUP_80211N		(1 << 6)
#define SUP_1000ET		(1 << 7)

extern int check_hw_type(void);
//	extern int get_hardware(void) __attribute__ ((weak, alias ("check_hw_type")));
extern int get_model(void);
extern char *get_modelid(int model);
extern int get_switch(void);
extern int supports(unsigned long attr);

// pids.c
extern int pids(char *appname);
extern pid_t* find_pid_by_name(const char *);

// process.c
extern char *psname(int pid, char *buffer, int maxlen);
extern int pidof(const char *name);
extern int killall(const char *name, int sig);
extern int process_exists(pid_t pid);
extern int module_loaded(const char *module);
extern int ppid(int pid);

// files.c
extern int check_if_dir_empty(const char *dirpath);
extern int _file_lock(const char *dir, const char *tag);
extern int _file_unlock(int lockfd);
extern int file_lock(const char *tag);
extern void file_unlock(int lockfd);

#define FW_CREATE	0
#define FW_APPEND	1
#define FW_NEWLINE	2
#define FW_SILENT	4	/* Don't print error message even write file fail. */

#define ACTION_LOCK_FILE "/var/lock/a_w_l" // action write lock

extern unsigned long f_size(const char *path);
extern int f_exists(const char *file);
extern int d_exists(const char *path);
extern int f_read_excl(const char *path, void *buffer, int max);
extern int f_read(const char *file, void *buffer, int max);												// returns bytes read
extern int f_write_excl(const char *path, const void *buffer, int len, unsigned flags, unsigned cmode);
extern int f_write(const char *file, const void *buffer, int len, unsigned flags, unsigned cmode);		//
extern int f_read_string(const char *file, char *buffer, int max);										// returns bytes read, not including term; max includes term
extern int f_write_string(const char *file, const char *buffer, unsigned flags, unsigned cmode);		//
extern int f_read_alloc(const char *path, char **buffer, int max);
extern int f_read_alloc_string(const char *path, char **buffer, int max);
extern int f_wait_exists(const char *name, int max);
extern int f_wait_notexists(const char *name, int max);


// button & led
enum btn_id {
	BTN_RESET = 0,
	BTN_WPS,
	BTN_FAN,
	BTN_HAVE_FAN,
#ifdef RTCONFIG_SWMODE_SWITCH
	BTN_SWMODE_SW_ROUTER,
	BTN_SWMODE_SW_REPEATER,
	BTN_SWMODE_SW_AP,
#endif
#ifdef RTCONFIG_WIRELESS_SWITCH
	BTN_WIFI_SW,
#endif
#ifdef RTCONFIG_WIFI_TOG_BTN
	BTN_WIFI_TOG,
#endif
#ifdef RTCONFIG_LED_BTN
	BTN_LED,
#endif
#ifdef RT4GAC55U
	BTN_LTE,
#endif
#ifdef RTCONFIG_EJUSB_BTN
	BTN_EJUSB1,
	BTN_EJUSB2,	/* If two USB LED and two EJECT USB button are true, map USB3 port to this button. */
#endif

	BTN_ID_MAX,	/* last item */
};

#if defined(RTCONFIG_CONCURRENTREPEATER)
enum led_color {
	LED_NONE = 0,
	LED_RED,
	LED_GREEN,
	LED_BLUE,
	LED_ORANGE,
	LED_GREEN2,
	LED_GREEN3,
	LED_GREEN4,
	ALL_LED,
	LED_SL4,
	LED_SL3,
	LED_SL2,
	LED_SL1	
};
#endif

enum led_id {
	LED_POWER = 0,
#if defined(RTCONFIG_PWRRED_LED)
	LED_POWER_RED,
#endif
	LED_USB,
	LED_WPS,
	FAN,
	HAVE_FAN,
	LED_LAN,
	LED_WAN,
#if defined(RTCONFIG_WANPORT2)
	LED_WAN2,
#endif
#ifdef HND_ROUTER
	LED_WAN_NORMAL,
#endif
	LED_2G,
	LED_5G,
	LED_5G2,
	LED_60G,
	LED_USB3,
#ifdef RTCONFIG_LAN4WAN_LED
	LED_LAN1,
	LED_LAN2,
	LED_LAN3,
	LED_LAN4,
#endif
#ifdef RTCONFIG_LOGO_LED
	LED_LOGO,
#endif
	LED_WAN_RED,
#if defined(RTCONFIG_WANPORT2) && defined(RTCONFIG_WANRED_LED)
	LED_WAN2_RED,
#endif
#if defined(RTCONFIG_FAILOVER_LED)
	LED_FAILOVER,
#endif
	LED_SATA,
#ifdef RTCONFIG_QTN
	BTN_QTN_RESET,	/* QTN platform uses led_control() to control this gpio. */
#endif
#ifdef RTCONFIG_LED_ALL
	LED_ALL,
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
#if defined(RT4GAC53U)
	LED_LTE_OFF,
	LED_POWER_RED,
#else
	LED_3G,
	LED_LTE,
#endif
	LED_SIG1,
	LED_SIG2,
	LED_SIG3,
#if defined(RT4GAC53U)
	LED_SIG4,
#endif
#endif
#if (defined(PLN12) || defined(PLAC56))
	PLC_WAKE,
	LED_POWER_RED,
	LED_2G_GREEN,
	LED_2G_ORANGE,
	LED_2G_RED,
	LED_5G_GREEN,
	LED_5G_ORANGE,
	LED_5G_RED,
#elif defined(MAPAC1750)
	LED_BLUE,
	LED_GREEN,
	LED_RED,
#endif
#ifdef RPAC53
	LED_POWER_RED ,
	LED_2G_ORANGE,
	LED_2G_GREEN,
	LED_2G_RED,
	LED_5G_ORANGE,
	LED_5G_GREEN,
	LED_5G_RED,
#endif
#if defined(RPAC66)
	LED_ORANGE_POWER ,
	LED_2G_BLUE,
	LED_2G_GREEN,
	LED_2G_RED,
	LED_5G_BLUE,
	LED_5G_GREEN,
	LED_5G_RED,
#endif
#if defined(RPAC51)
	LED_RED_POWER ,
	LED_SINGLE,
	LED_FAR,
	LED_NEAR,
#endif
#if defined(RPAC87)
	LED_2G_GREEN1 ,
	LED_2G_GREEN2,
	LED_2G_GREEN3,
	LED_2G_GREEN4,
	LED_5G_GREEN1,
	LED_5G_GREEN2,
	LED_5G_GREEN3,
	LED_5G_GREEN4,
#endif
#ifdef RPAC55
	LED_POWER_RED,
	LED_WIFI,
	LED_SIG1,
	LED_SIG2,
#endif
#ifdef BLUECAVE
	LED_CENTRAL_SIG1,
	LED_CENTRAL_SIG2,
	LED_CENTRAL_SIG3,
	LED_INDICATOR_SIG1,
	LED_INDICATOR_SIG2,
#endif	
#ifdef RTCONFIG_MMC_LED
	LED_MMC,
#endif
#ifdef RTCONFIG_RESET_SWITCH
	LED_RESET_SWITCH,
#endif
#if defined(RTAC5300) || defined(GTAC5300)
	RPM_FAN,	/* use to control FAN RPM (Hi/Lo) */
#endif
#if defined(RTCONFIG_USB) || defined (RTCONFIG_LED_BTN) || defined (RTCONFIG_WPS_ALLLED_BTN)
	PWR_USB,
#endif

	LED_ID_MAX,	/* last item */
};

// Outside of enum to avoid conflicting with Asus's code
enum led_merlin_id {
	LED_SWITCH = LED_ID_MAX + 1,
	LED_5G_FORCED,
};


enum led_fan_mode_id {
	LED_OFF = 0,
	LED_ON,
	FAN_OFF,
	FAN_ON,
	HAVE_FAN_OFF,
	HAVE_FAN_ON,

	LED_FAN_MODE_MAX,	/* last item */
};

static inline int have_usb3_led(int model)
{
	/* Return true if a model has USB LED and USB3 LED both. */
	switch (model) {
		case MODEL_RTN18U:
		case MODEL_RTAC56U:
		case MODEL_RTAC56S:
		case MODEL_RTAC68U:
#ifndef RTCONFIG_ETRON_XHCI_USB3_LED
		case MODEL_RTAC58U:
		case MODEL_RTAC82U:
		case MODEL_RTAC55U:
		case MODEL_RTAC55UHP:
		case MODEL_MAPAC3000:
#endif
		case MODEL_DSLAC68U:
		case MODEL_RTAC3200:
		case MODEL_BRTAC828:
		case MODEL_RTAD7200:
		case MODEL_RTAC88U:
		case MODEL_RTAC86U:
		case MODEL_RTAC3100:
		case MODEL_RTAC5300:
		case MODEL_GTAC5300:
			return 1;
	}
	return 0;
}

#if defined(RTCONFIG_M2_SSD)
static inline int have_sata_led(int model)
{
	/* Return true if a model has SATA LED */
	switch (model) {
		case MODEL_BRTAC828:
			return 1;
	}
	return 0;
}
#else
static inline int have_sata_led(__attribute__ ((unused)) int model) { return 0; }
#endif

#define MAX_NO_BRIDGE 	2
#if defined(BRTAC828)
#define MAX_NO_MSSID	8
#else
#ifdef RTCONFIG_PSR_GUEST
#define MAX_NO_MSSID	5
#else
#define MAX_NO_MSSID	4
#endif
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ary) (sizeof(ary) / sizeof((ary)[0]))
#endif

#if defined(RTCONFIG_WIGIG)
/* 2-nd 5G may not exist! Remember to check existence of wl2_nband in firmware
 * or 5G-2 rc_support in UI for that band.  At compile-time, RTCONFIG_HAS_5G_2
 * can be used to test.  But, non-QCA platform haven't support RTCONFIG_HAS_5G_2.
 */
#define MAX_NR_WL_IF			4
#elif defined(RTCONFIG_HAS_5G_2)
#define MAX_NR_WL_IF			3
#elif defined(MAPAC2200)
#define MAX_NR_WL_IF			3
#elif defined(RTCONFIG_HAS_5G)
#define MAX_NR_WL_IF			2
#else	/* ! RTCONFIG_HAS_5G */
#define MAX_NR_WL_IF			1	/* Single 2G */
#endif	/* ! RTCONFIG_HAS_5G */

enum wl_band_id {
	WL_2G_BAND = 0,
	WL_5G_BAND = 1,
	WL_5G_2_BAND = 2,
	WL_60G_BAND = 3,

	WL_NR_BANDS				/* Maximum number of Wireless bands of all models. */
};

#define SKIP_ABSENT_FAKE_IFACE(iface)		if (!strncmp(iface, "FAKE", 4)) { continue; }
#define SKIP_ABSENT_BAND(u)			if (!nvram_get(wl_nvname("nband", u, 0))) { continue; }
#define SKIP_ABSENT_BAND_AND_INC_UNIT(u)	if (!nvram_get(wl_nvname("nband", u, 0))) { ++u; continue; }

#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_LANTIQ)
static inline int __access_point_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_AP
#if defined(RTCONFIG_AMAS)
		&& !nvram_get_int("re_mode")
#endif
#if defined(RTCONFIG_PROXYSTA) && defined(RTCONFIG_LANTIQ)
		&& !nvram_get_int("wlc_psta")
#endif
		);
}

static inline int access_point_mode(void)
{
	return __access_point_mode(sw_mode());
}

#if defined(RTCONFIG_WIRELESSREPEATER)
static inline int __repeater_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_REPEATER
#if defined(RTCONFIG_PROXYSTA)
		&& nvram_get_int("wlc_psta") != 1
#endif
		);
}
static inline int repeater_mode(void)
{
	return __repeater_mode(sw_mode());
}
#else
static inline int __repeater_mode(int __attribute__((__unused__)) sw_mode) { return 0; }
static inline int repeater_mode(void) { return 0; }
#endif

#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
static inline int __mediabridge_mode(int sw_mode)
{
#ifdef RTCONFIG_LANTIQ
	return (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1);
#else
	return (sw_mode == SW_MODE_REPEATER && nvram_get_int("wlc_psta") == 1);
#endif
}
static inline int mediabridge_mode(void)
{
	return __mediabridge_mode(sw_mode());
}
#else
static inline int __mediabridge_mode(int __attribute__((__unused__)) sw_mode) { return 0; }
static inline int mediabridge_mode(void) { return 0; }
#endif
#else
/* Broadcom platform and others */
static inline int __access_point_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_AP
#if defined(RTCONFIG_PROXYSTA)
		&& !nvram_get_int("wlc_psta")
#endif
		);
}

static inline int access_point_mode(void)
{
	return __access_point_mode(sw_mode());
}

#if defined(RTCONFIG_WIRELESSREPEATER)
static inline int __repeater_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_REPEATER);
}
static inline int repeater_mode(void)
{
	return __repeater_mode(sw_mode());
}
#else
static inline int __repeater_mode(int sw_mode) { return 0; }
static inline int repeater_mode(void) { return 0; }
#endif

#ifdef RTCONFIG_PROXYSTA
static inline int __mediabridge_mode(int sw_mode)
{
	return (sw_mode == SW_MODE_AP && nvram_get_int("wlc_psta") == 1);
}
static inline int mediabridge_mode(void)
{
	return __mediabridge_mode(sw_mode());
}
#else
static inline int __mediabridge_mode(__attribute__ ((unused)) int sw_mode) { return 0; }
static inline int mediabridge_mode(void) { return 0; }
#endif
#endif	/* RTCONFIG_RALINK || RTCONFIG_QCA */

static inline int client_mode()
{
	if  (repeater_mode() || mediabridge_mode()
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
		|| ((sw_mode() == SW_MODE_AP) && nvram_get_int("wlc_psta"))
#endif
	)
		return 1;

	return 0;
}

#ifdef RTCONFIG_DPSTA
static inline int dpsta_mode()
{
	return ((sw_mode() == SW_MODE_AP) && (nvram_get_int("wlc_psta") == 2) && (nvram_get_int("wlc_dpsta") == 1));
}
#endif

static inline int dpsr_mode()
{
	return ((sw_mode() == SW_MODE_AP) && (nvram_get_int("wlc_psta") == 2) && (nvram_get_int("wlc_dpsta") == 2));
}

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
static inline int psr_mode()
{
        return (sw_mode() == SW_MODE_AP && nvram_get_int("wlc_psta") == 2 && !nvram_get_int("wlc_dpsta"));
}
#endif

static inline int get_wps_multiband(void)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	return nvram_get_int("wps_multiband");
#else
	return 0;
#endif
}

static inline int get_radio_band(int band)
{
#if defined(RTCONFIG_WPSMULTIBAND)
	(void) band;
	return -1;
#else
	return band;
#endif
}

#ifdef RTCONFIG_DUALWAN
static inline int dualwan_unit__usbif(int unit)
{
	int type = get_dualwan_by_unit(unit);
	return (type == WANS_DUALWAN_IF_USB || type == WANS_DUALWAN_IF_USB2
			);
}

static inline int dualwan_unit__nonusbif(int unit)
{
	int type = get_dualwan_by_unit(unit);
	return (type == WANS_DUALWAN_IF_WAN || type == WANS_DUALWAN_IF_DSL || type == WANS_DUALWAN_IF_LAN || type == WANS_DUALWAN_IF_WAN2
#ifdef RTCONFIG_MULTICAST_IPTV
		|| type == WAN_UNIT_IPTV || type == WAN_UNIT_VOIP
#endif
			);
}
extern int get_usbif_dualwan_unit(void);
extern int get_primaryif_dualwan_unit(void);
#else // RTCONFIG_DUALWAN
static inline int dualwan_unit__usbif(int unit)
{
#ifdef RTCONFIG_USB_MODEM
	return (unit == WAN_UNIT_SECOND);
#else
	return 0;
#endif
}

static inline int dualwan_unit__nonusbif(int unit)
{
#ifdef RTCONFIG_MULTICAST_IPTV
	return (unit == WAN_UNIT_FIRST || unit == WAN_UNIT_IPTV || unit == WAN_UNIT_VOIP);
#else
	return (unit == WAN_UNIT_FIRST);
#endif
}
static inline int get_usbif_dualwan_unit(void)
{
	return get_wanunit_by_type(WANS_DUALWAN_IF_USB);
}

static inline int get_primaryif_dualwan_unit(void)
{
	return wan_primary_ifunit();
}
#endif // RTCONFIG_DUALWAN

#ifdef CONFIG_BCMWL5
static inline int guest_wlif(char *ifname)
{
	return strncmp(ifname, "wl", 2) == 0 && strchr(ifname, '.');
}
#elif defined RTCONFIG_RALINK
static inline int guest_wlif(char *ifname)
{
	return strncmp(ifname, "ra", 2) == 0 && !strchr(ifname, '0');
}
#elif defined(RTCONFIG_QCA)
/**
 * This function doesn't check wlX_vifnames nvram variable.
 * If required, check it in caller.
 * @return:
 * 	0:	@ifname is not any guest network interface name.
 *  otherwise:	@ifname is one of gueset network interface name.
 */
static inline int guest_wlif(char *ifname)
{
	int r, r1, v;
	char *p = ifname + 4;

	r = !strncmp(ifname, "ath0", 4) || !strncmp(ifname, "ath1", 4) || !strncmp(ifname, "ath2", 4);
	if (r) {
		v = atoi(p);
		if (*p == '\0' || v <= 0 || v > 16)
			r = 0;
	}

	r1 = !strncmp(ifname, "wlan", 4);
	if (r1) {
		/* Multiple SSID haven't been supported by kernel v3.4 Wigig 802.11ad driver.
		 * Guest network interface is not possible yet.
		 */
		return 0;
	}

	return r;
}
#elif defined RTCONFIG_REALTEK
static inline int guest_wlif(char *ifname)
{
	return strncmp(ifname, "wl", 2) == 0 && !strchr(ifname, '0');
}
#elif defined RTCONFIG_LANTIQ
static inline int guest_wlif(char *ifname)
{
	char *p = ifname + 5;
	int v;

	if (strncmp(ifname, "wlan", 4) || (*p == '\0'))
		return 0;

	v = atoi(p+1);
	if (v < 0 || v > 4)
		return 0;

	return 1;
}
#elif defined RTCONFIG_ALPINE
#define GUEST_IF_NUM 6
static inline int guest_wlif(char *ifname)
{
	// char *p = ifname + 5;
	int i;

	char *guest_wlif_name[GUEST_IF_NUM] = {
		"wifi2_1", "wifi2_2", "wifi2_3",
		"wifi0_1", "wifi0_2", "wifi0_3"};

	for( i = 0 ; i < GUEST_IF_NUM; ++i){
		if(strcmp(ifname, guest_wlif_name[i]) == 0) return 1;
	}

	return 0;
}
#endif

extern int init_gpio(void);
extern int set_pwr_usb(int boolOn);
#ifdef RT4GAC68U
extern int set_pwr_modem(int boolOn);
#endif
extern int button_pressed(int which);
extern int led_control(int which, int mode);
extern int led_control_atomic(int which, int mode);

/* api-*.c */
extern uint32_t gpio_dir(uint32_t gpio, int dir);
extern uint32_t set_gpio(uint32_t gpio, uint32_t value);
extern uint32_t get_gpio(uint32_t gpio);
extern int get_switch_model(void);
#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
extern uint32_t get_phy_status(int wan_unit);
extern uint32_t get_phy_speed(int wan_unit);
extern uint32_t set_phy_ctrl(int wan_unit, int ctrl);
#else
extern uint32_t get_phy_status(uint32_t portmask);
extern uint32_t get_phy_speed(uint32_t portmask);
extern uint32_t set_phy_ctrl(uint32_t portmask, int ctrl);
#endif
extern char *get_lan_mac_name(void);
extern char *get_wan_mac_name(void);
extern char *get_2g_hwaddr(void);
extern char *get_lan_hwaddr(void);
extern char *get_wan_hwaddr(void);
extern char *get_label_mac(void);
#if defined(RTCONFIG_QCA)
extern char *__get_wlifname(int band, int subunit, char *buf);
extern int get_wlsubnet(int band, const char *ifname);
extern char *get_wififname(int band);
extern char *get_staifname(int band);
extern char *get_vphyifname(int band);
#ifdef IWLIB_H
extern int get_ap_mac(const char *ifname, struct iwreq *pwrq);
#endif
extern int chk_assoc(const char *ifname);
extern int match_radio_status(int unit, int status);
extern int get_ch(int freq);
extern int get_channel(const char *ifname);
#endif
extern char *get_wlifname(int unit, int subunit, int subunit_x, char *buf);
extern char *get_wlxy_ifname(int x, int y, char *buf);
#if defined(RTCONFIG_RALINK_MT7620)
extern int get_mt7620_wan_unit_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#elif defined(RTCONFIG_RALINK_MT7621)
extern int get_mt7621_wan_unit_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#endif
#ifdef RTCONFIG_AMAS
extern int aimesh_re_mode(void);
extern void add_beacon_vsie(char *hexdata);
extern void del_beacon_vsie(char *hexdata);
extern int get_port_status(int unit);
extern int is_wlsta_exist(int unit, int vidx);
#if defined(RTCONFIG_BCMWL6)
extern int get_wl_sta_list(void);
extern int get_maxassoc(char *ifname);
extern int wl_add_ie(int unit, uint32 pktflag, int ielen, uchar *oui, uchar *data);
extern void wl_del_ie_with_oui(int unit, uchar *oui);
#endif
#if defined(RTCONFIG_LANTIQ)
extern int get_wl_sta_list(void);
//extern int get_maxassoc(char *ifname);
extern int get_psta_status(int unit);
#endif
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
extern int get_psta_status(int unit);
#endif

#define WLSTA_JSON_FILE 				"/tmp/wl_sta_list.json"
#define MAX_STA_COUNT 128
#define MAX_SUBIF_NUM 4
#if defined(RTCONFIG_LANTIQ)
#define MACF    "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERP_TO_MACF(ea)      ((struct ether_addr *) (ea))->ether_addr_octet[0], \
                                ((struct ether_addr *) (ea))->ether_addr_octet[1], \
                                ((struct ether_addr *) (ea))->ether_addr_octet[2], \
                                ((struct ether_addr *) (ea))->ether_addr_octet[3], \
                                ((struct ether_addr *) (ea))->ether_addr_octet[4], \
                                ((struct ether_addr *) (ea))->ether_addr_octet[5]
#define ETHER_TO_MACF(ea)       (ea).ether_addr_octet[0], \
                                (ea).ether_addr_octet[1], \
                                (ea).ether_addr_octet[2], \
                                (ea).ether_addr_octet[3], \
                                (ea).ether_addr_octet[4], \
                                (ea).ether_addr_octet[5]
#elif defined(RTCONFIG_QCA)
#define MACF	"%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERP_TO_MACF(ea)	((struct ether_addr *) (ea))->ether_addr_octet[0], \
				((struct ether_addr *) (ea))->ether_addr_octet[1], \
				((struct ether_addr *) (ea))->ether_addr_octet[2], \
				((struct ether_addr *) (ea))->ether_addr_octet[3], \
				((struct ether_addr *) (ea))->ether_addr_octet[4], \
				((struct ether_addr *) (ea))->ether_addr_octet[5]
#define ETHER_TO_MACF(ea)	(ea).ether_addr_octet[0], \
				(ea).ether_addr_octet[1], \
				(ea).ether_addr_octet[2], \
				(ea).ether_addr_octet[3], \
				(ea).ether_addr_octet[4], \
				(ea).ether_addr_octet[5]
#else
#define MACF	"%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERP_TO_MACF(ea)	((struct ether_addr *) (ea))->octet[0], \
				((struct ether_addr *) (ea))->octet[1], \
				((struct ether_addr *) (ea))->octet[2], \
				((struct ether_addr *) (ea))->octet[3], \
				((struct ether_addr *) (ea))->octet[4], \
				((struct ether_addr *) (ea))->octet[5]
#define ETHER_TO_MACF(ea)	(ea).octet[0], \
				(ea).octet[1], \
				(ea).octet[2], \
				(ea).octet[3], \
				(ea).octet[4], \
				(ea).octet[5]
#endif

#endif	/* RTCONFIG_AMAS */

/* sysdeps/ralink/ *.c */
#if defined(RTCONFIG_RALINK)
extern int rtkswitch_ioctl(int val, int val2);
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern int rtkswitch_WanPort_linkUp(void);
extern int rtkswitch_WanPort_linkDown(void);
extern int rtkswitch_LanPort_linkUp(void);
extern int rtkswitch_LanPort_linkDown(void);
extern int rtkswitch_AllPort_linkUp(void);
extern int rtkswitch_AllPort_linkDown(void);
extern int rtkswitch_Reset_Storm_Control(void);
extern int ralink_gpio_read_bit(int idx);
extern int ralink_gpio_write_bit(int idx, int value);
extern char *wif_to_vif(char *wif);
extern int ralink_gpio_init(unsigned int idx, int dir);
extern int config_rtkswitch(int argc, char *argv[]);
extern int config_mtkswitch(int argc, char *argv[]);
extern int get_channel_list_via_driver(int unit, char *buffer, int len);
extern int get_channel_list_via_country(int unit, const char *country_code, char *buffer, int len);
#if defined(RTCONFIG_RALINK_MT7620)
extern int __mt7620_wan_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#elif defined(RTCONFIG_RALINK_MT7620)
extern int __mt7621_wan_bytecount(int unit, unsigned long *tx, unsigned long *rx);
#endif

#elif defined(RTCONFIG_QCA)
extern int rtkswitch_ioctl(int val, int *val2);
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern int rtkswitch_WanPort_linkUp(void);
extern int rtkswitch_WanPort_linkDown(void);
extern int rtkswitch_LanPort_linkUp(void);
extern int rtkswitch_LanPort_linkDown(void);
extern int rtkswitch_AllPort_linkUp(void);
extern int rtkswitch_AllPort_linkDown(void);
extern int rtkswitch_Reset_Storm_Control(void);
extern char *wif_to_vif(char *wif);
extern int config_rtkswitch(int argc, char *argv[]);
extern int get_qca8337_PHY_power(int port);
#ifdef RTCONFIG_LAN4WAN_LED
extern int led_ctrl(void);
#endif
#if defined(RTCONFIG_SOC_IPQ40XX)
extern unsigned int rtkswitch_Port_phyStatus(unsigned int port_mask);
extern int get_channel_list_via_driver(int unit, char *buffer, int len);
extern int get_channel_list_via_country(int unit, const char *country_code, char *buffer, int len);
#endif
extern unsigned int __rtkswitch_WanPort_phySpeed(int wan_unit);
extern void ATE_port_status(void);
#elif defined(RTCONFIG_ALPINE)
extern char *wl_vifname_qtn(int unit, int subunit);
extern char *wif_to_vif(char *wif);
extern int config_rtkswitch(int argc, char *argv[]);
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern void ATE_qca8337_port_status(void);
#elif defined(RTCONFIG_LANTIQ)
extern char *wif_to_vif(char *wif);
extern int config_rtkswitch(int argc, char *argv[]);
extern unsigned int rtkswitch_wanPort_phyStatus(int wan_unit);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
extern unsigned int rtkswitch_WanPort_phySpeed(void);
extern void ATE_qca8337_port_status(void);
#else
#define wif_to_vif(wif) (wif)
extern int config_rtkswitch(int argc, char *argv[]);
extern unsigned int rtkswitch_lanPorts_phyStatus(void);
#endif

#if defined(RTCONFIG_POWER_SAVE)
extern void set_power_save_mode(void);
#else
static inline void set_power_save_mode(void) { }
#endif

/* sysdeps/broadcom/ *.c */
#ifdef CONFIG_BCMWL5
#ifdef RTCONFIG_BCMFA
extern int get_fa_rev(void);
extern int get_fa_dump(void);
#endif
#ifdef HND_ROUTER
extern uint32_t hnd_get_phy_status(int port, int offs, unsigned int regv, unsigned int pmdv);
extern uint32_t hnd_get_phy_speed(int port, int offs, unsigned int regv, unsigned int pmdv);
extern int hnd_ethswctl(ecmd_t act, unsigned int val, int len, int wr, unsigned long long regdata);
extern uint32_t set_ex53134_ctrl(uint32_t portmask, int ctrl);
#endif
extern int fw_check(void);
extern int with_non_dfs_chspec(char *wif);
extern chanspec_t select_band1_chspec_with_same_bw(char *wif, chanspec_t chanspec);
extern chanspec_t select_band4_chspec_with_same_bw(char *wif, chanspec_t chanspec);
extern chanspec_t select_chspec_with_band_bw(char *wif, int band, int bw, chanspec_t chanspec);
extern void wl_reset_ssid(char *wif);
#endif
#ifdef RTCONFIG_AMAS
//extern char *get_pap_bssid(int unit, char bssid_str[]);
#ifndef RTCONFIG_LANTIQ
extern int get_wlan_service_status(int bssidx, int vifidx);
extern void set_wlan_service_status(int bssidx, int vifidx, int enabled);
#endif
#endif
#ifdef RTCONFIG_LACP
extern uint32_t traffic_trunk(int port_num, uint32_t *rx, uint32_t *tx);
#endif
#ifdef RTCONFIG_JFFS_NVRAM
extern char * jffs_nvram_get(const char *name);
extern int jffs_nvram_set(const char *name, const char *value);
extern int jffs_nvram_unset(const char *name);
extern int large_nvram(const char *name);
extern void jffs_nvram_init();
extern int jffs_nvram_getall(int len_nvram, char *buf, int count);
#endif

// base64.c
extern int base64_encode(const unsigned char *in, char *out, int inlen);		// returns amount of out buffer used
extern int base64_decode(const char *in, unsigned char *out, int inlen);	// returns amount of out buffer used
extern int base64_encoded_len(int len);
extern int base64_decoded_len(int len);										// maximum possible, not actual

/* boardapi.c */
extern int extract_gpio_pin(const char *gpio);
extern int lanport_status(void);
extern void get_gpio_values_once(int force);
#ifdef HND_ROUTER
extern int _gpio_active_low(int gpionr);
extern void dump_ledtable();
#endif

/* discover.c */
extern int discover_interface(const char *current_wan_ifname, int dhcp_det);
extern int discover_all(int wan_unit);

// strings.c
extern int char_to_ascii_safe(const char *output, const char *input, int outsize);
extern void char_to_ascii(const char *output, const char *input);
#if defined(RTCONFIG_UTF8_SSID)
extern int char_to_ascii_safe_with_utf8(const char *output, const char *input, int outsize);
extern void char_to_ascii_with_utf8(const char *output, const char *input);
#endif
extern int ascii_to_char_safe(const char *output, const char *input, int outsize);
extern void ascii_to_char(const char *output, const char *input);
extern const char *find_word(const char *buffer, const char *word);
extern int remove_word(char *buffer, const char *word);
extern void trim_space(char *str);
extern int replace_char(char *str, const char from, const char to);
extern int str_escape_quotes(const char *output, const char *input, int outsize);
extern void toLowerCase(char *str);
extern void toUpperCase(char *str);

// file.c
extern int check_if_file_exist(const char *file);
extern int check_if_dir_exist(const char *file);
extern int check_if_dir_writable(const char *dir);
extern unsigned int __dir_size__;
extern int sum(const char *fpath, const struct stat *sb, int typeflag);
extern int d_size(const char *dirPath);

#if defined(RTCONFIG_SAVEJFFS)
/* jffs_cfgs.c */
extern int append_jffs_cfgs(FILE *fp_jc, char *nv_fn);
extern int restore_jffs_cfgs(char *nv_fn);
#endif

/* mdio.c */
#if defined(RTCONFIG_SWITCH_RTL8370M_PHY_QCA8033_X2) || \
    defined(RTCONFIG_SWITCH_RTL8370MB_PHY_QCA8033_X2)
extern int mdio_read(char *ifname, int location);
extern int mdio_write(char *ifname, int location, int value);
extern int mdio_phy_speed(char *ifname);
#endif

/* misc.c */
extern char *get_productid(void);
extern void logmessage_normal(char *logheader, char *fmt, ...);
extern char *get_logfile_path(void);
extern char *get_syslog_fname(unsigned int idx);
#ifdef RTCONFIG_USB_MODEM
extern char *get_modemlog_fname(void);
#endif
#if defined(RTCONFIG_HTTPS)
extern int nvram_get_file(const char *key, const char *fname, int max);
extern int nvram_set_file(const char *key, const char *fname, int max);
#endif
extern int free_caches(const char *clean_mode, const int clean_time, const unsigned int threshold);
extern int update_6rd_info(void);
extern int is_private_subnet(const char *ip);
extern const char *get_wanface(void);
extern const char *get_wanip(void);
extern int is_intf_up(const char* ifname);
extern uint32_t crc_calc(uint32_t crc, const char *buf, int len);
extern int illegal_ipv4_address(char *addr);
extern int illegal_ipv4_netmask(char *netmask);
extern int test_and_get_free_uint_network(int t_class, uint32_t *exp_ip, uint32_t exp_cidr, uint32_t excl);
extern int test_and_get_free_char_network(int t_class, char *ip_cidr_str, uint32_t excl);
extern enum wan_unit_e get_first_connected_public_wan_unit(void);
#ifdef RTCONFIG_IPV6
extern const char *get_wan6face(void);
extern const char *ipv6_address(const char *ipaddr6);
extern const char *ipv6_prefix(struct in6_addr *in6addr);
#if 0 /* unused */
extern const char *ipv6_prefix(const char *ifname);
extern int ipv6_prefix_len(const char *ifname);
#endif
extern void reset_ipv6_linklocal_addr(const char *ifname, int flush);
extern int with_ipv6_linklocal_addr(const char *ifname);
#if 1 /* temporary till httpd route table redo */
extern void ipv6_set_flags(char *flagstr, int flags);
extern char* INET6_rresolve(struct sockaddr_in6 *sin6, int numeric);
#endif
#endif
#if defined(RTCONFIG_OPENVPN) || defined(RTCONFIG_IPSEC)
#define OVPN_FS_PATH	"/jffs/openvpn"
extern int set_crt_parsed(const char *name, char *file_path);
#endif
extern int get_wifi_unit(char *wif);
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
#ifdef RTCONFIG_DPSTA
extern int is_dpsta(int unit);
#endif
extern int is_dpsr(int unit);
extern int is_psta(int unit);
extern int is_psr(int unit);
extern int psta_exist(void);
extern int psta_exist_except(int unit);
extern int psr_exist(void);
extern int psr_exist_except(int unit);
#endif

struct ifino_s {
	char ifname[IFNAMSIZ];
	ino_t inode;
	unsigned long long last_rx, last_tx;
	unsigned long long shift_rx, shift_tx;
};

struct ifname_ino_tbl {
	unsigned nr_items;
	struct ifino_s items[50];
};

extern struct ifino_s *ifname_ino_ptr(struct ifname_ino_tbl *ifinotbl, const char *ifname);
extern ino_t get_iface_inode(const char *ifname);
extern unsigned int netdev_calc(char *ifname, char *ifname_desc, unsigned long *rx, unsigned long *tx, char *ifname_desc2, unsigned long *rx2, unsigned long *tx2, char *nv_lan_ifname, char *nv_lan_ifnames);
extern void disable_dpi_engine_setting(void);
extern int get_iface_hwaddr(char *name, unsigned char *hwaddr);
#if defined(RTCONFIG_SOC_IPQ8064)
extern int __set_iface_ps(const char *ifname, int nr_rx_mask, const unsigned int *rx_mask, int nr_tx_mask, const unsigned int *tx_mask);
static inline int set_iface_ps2(const char *ifname, unsigned int rx_mask, unsigned tx_mask)
{
	const unsigned int rx_mask_ary[] = { rx_mask }, tx_mask_ary[] = { tx_mask };
	return __set_iface_ps(ifname, 0, rx_mask_ary, 0, tx_mask_ary);
}
static inline int set_iface_ps(const char *ifname, unsigned int rx_mask)
{
	const unsigned int rx_mask_ary[] = { rx_mask };
	return __set_iface_ps(ifname, 0, rx_mask_ary, -1, NULL);
}
extern int ctrl_gro(char *iface, int onoff);
extern int ctrl_wan_gro(int wan_unit, int onoff);
extern int ctrl_lan_gro(int onoff);
#else
static inline int __set_iface_ps(__attribute__ ((unused)) const char *ifname, __attribute__ ((unused)) int nr_rx_mask,\
	__attribute__ ((unused)) const unsigned int *rx_mask, __attribute__ ((unused)) int nr_tx_mask,\
	__attribute__ ((unused)) const unsigned int *tx_mask) { return 0; }
static inline int set_iface_ps2(__attribute__ ((unused)) const char *ifname, __attribute__ ((unused)) unsigned int rx_mask,\
	__attribute__ ((unused)) unsigned tx_mask) { return 0; }
static inline int set_iface_ps(__attribute__ ((unused)) const char *ifname, __attribute__ ((unused)) unsigned int rx_mask) { return 0; }
static inline int ctrl_gro(__attribute__ ((unused)) char *iface, __attribute__ ((unused)) int onoff) { return 0; }
static inline int ctrl_wan_gro(__attribute__ ((unused)) int wan_unit, __attribute__ ((unused)) int onoff) { return 0; }
static inline int ctrl_lan_gro(__attribute__ ((unused)) int onoff) { return 0; }
#endif
extern int set_irq_smp_affinity(unsigned int irq, unsigned int cpu_mask);
extern char *get_qos_prefix(int unit, char *buf);
extern int internet_ready(void);
extern void set_no_internet_ready(void);
extern unsigned int num_of_mssid_support(unsigned int unit);
extern enum led_id get_wl_led_id(int band);
extern char *get_wl_led_gpio_nv(int band);
#if defined(RTCONFIG_QCA)
extern char *get_wsup_drvname(int band);
#else
static inline char *get_wsup_drvname(__attribute__ ((unused)) int band) { return ""; }
#endif
#ifdef RTCONFIG_TRAFFIC_LIMITER
extern unsigned int traffic_limiter_read_bit(const char *type);
extern void traffic_limiter_set_bit(const char *type, int unit);
extern void traffic_limiter_clear_bit(const char *type, int unit);
extern double traffic_limiter_get_realtime(int unit);
#endif
#if defined(RTCONFIG_PORT_BASED_VLAN) || defined(RTCONFIG_TAGGED_BASED_VLAN)
extern struct vlan_rules_s *get_vlan_rules(void);
#endif
#if defined(HND_ROUTER) && defined(RTCONFIG_BONDING)
extern int get_bonding_status();
#endif
extern int isValidMacAddress(const char* mac);
extern int isValidEnableOption(const char* option, int range);

/* scripts.c */
extern void run_custom_script(char *name, int timeout, char *arg1, char *arg2);
extern void run_postconf(char *name, char *config);
extern void use_custom_config(char *config, char *target);
extern void append_custom_config(char *config, FILE *fp);

/* mt7620.c */
#if defined(RTCONFIG_RALINK_MT7620)
extern void ATE_mt7620_esw_port_status(void);
#elif defined(RTCONFIG_RALINK_MT7621)
extern void ATE_mt7621_esw_port_status(void);
#endif

/* mt7628.c */
extern void ATE_mt7628_esw_port_status(void);

/* notify_rc.c */
extern int notify_rc(const char *event_name);
extern int notify_rc_after_wait(const char *event_name);
extern int notify_rc_after_period_wait(const char *event_name, int wait);
extern int notify_rc_and_wait(const char *event_name);
extern int notify_rc_and_wait_1min(const char *event_name);
extern int notify_rc_and_wait_2min(const char *event_name);
extern int notify_rc_and_period_wait(const char *event_name, int wait);

/* models.c */
extern int get_blver(char *bls);

/* wl.c */
#ifdef CONFIG_BCMWL5
#ifdef __CONFIG_DHDAP__
extern int dhd_probe(char *name);
extern int dhd_ioctl(char *name, int cmd, void *buf, int len);
extern int dhd_iovar_setbuf(char *ifname, char *iovar, void *param, int paramlen, void *bufptr, int buflen);
extern int dhd_iovar_setint(char *ifname, char *iovar, int val);
#endif
#endif

/* rtstate.c */
extern char *get_wanx_ifname(int unit);
extern int get_lanports_status(void);
extern int set_wan_primary_ifunit(const int unit);
#ifdef RTCONFIG_DUALWAN
extern int get_nr_wan_unit(void);
#else
static inline int get_wans_dualwan(void) {
#ifdef RTCONFIG_USB_MODEM
	return WANSCAP_WAN | WANSCAP_USB;
#else
	return WANSCAP_WAN;
#endif
}
static inline int get_dualwan_by_unit(int unit) {
#ifdef RTCONFIG_MULTICAST_IPTV
	if(unit == WAN_UNIT_IPTV)
		return WAN_UNIT_IPTV;
	if(unit == WAN_UNIT_VOIP)
		return WAN_UNIT_VOIP;
#endif
#ifdef RTCONFIG_USB_MODEM
	return (unit == WAN_UNIT_FIRST) ? WANS_DUALWAN_IF_WAN : WANS_DUALWAN_IF_USB;
#else
	return (unit == WAN_UNIT_FIRST) ? WANS_DUALWAN_IF_WAN : WANS_DUALWAN_IF_NONE;
#endif
}
static inline int get_nr_wan_unit(void) { return 1; }
#endif
extern int get_nr_guest_network(int band);
extern int get_gate_num(void);
extern void set_lan_phy(char *phy);
extern void add_lan_phy(char *phy);
extern void set_wan_phy(char *phy);
extern void add_wan_phy(char *phy);

/* semaphore.c */
extern void init_spinlock(void);

#if defined(RTCONFIG_WPS_LED)
static inline int wps_led_control(int onoff)
{
	return led_control(LED_WPS, onoff);
}

static inline int __wps_led_control(int onoff)
{
	return led_control(LED_WPS, onoff);
}
#else
static inline int wps_led_control(int onoff)
{
	if (nvram_get_int("led_pwr_gpio") != nvram_get_int("led_wps_gpio"))
		return led_control(LED_WPS, onoff);
	else
		return led_control(LED_POWER, onoff);
}

/* If individual WPS LED absents, don't do anything. */
static inline int __wps_led_control(int __attribute__((__unused__)) onoff) { return 0; }
#endif

enum {
	YADNS_DISABLED = -1,
	YADNS_BASIC,
	YADNS_SAFE,
	YADNS_FAMILY,
	YADNS_COUNT,
	YADNS_FIRST = YADNS_DISABLED + 1,
};

#ifdef RTCONFIG_YANDEXDNS
#define YADNS_DNSPORT 1253
int get_yandex_dns(int family, int mode, char **server, int max_count);
#endif

#if defined(RTCONFIG_PWRRED_LED)
static inline int power_red_led_control(int onoff)
{
	return led_control(LED_POWER_RED, onoff);
}
#else
static inline int power_red_led_control(__attribute__ ((unused)) int onoff) { return 0; }
#endif

#if defined(RTCONFIG_FAILOVER_LED)
static inline int failover_led_control(int onoff)
{
	return led_control(LED_FAILOVER, onoff);
}
#else
static inline int failover_led_control(__attribute__ ((unused)) int onoff) { return 0; }
#endif

#if defined(RTCONFIG_WIGIG)
static inline int wigig_led_control(int onoff)
{
	return led_control(LED_60G, onoff);
}
#else
static inline int wigig_led_control(__attribute__ ((unused)) int onoff) { return 0; }
#endif

#if defined(RTCONFIG_M2_SSD)
static inline int sata_led_control(int onoff)
{
	return led_control(LED_SATA, onoff);
}
#else
static inline int sata_led_control(__attribute__ ((unused)) int onoff) { return 0; }
#endif

#if defined(RTCONFIG_WANRED_LED)
static inline int wan_red_led_control(int onoff)
{
	return led_control(LED_WAN_RED, onoff);
}
#else
static inline int wan_red_led_control(__attribute__ ((unused)) int onoff) { return 0; }
#endif	/* RTCONFIG_WANRED_LED */

#if defined(RTCONFIG_WANRED_LED) && defined(RTCONFIG_WANPORT2)
static inline int wan2_red_led_control(int onoff)
{
	return led_control(LED_WAN2_RED, onoff);
}
#else
static inline int wan2_red_led_control(__attribute__ ((unused)) int onoff) { return 0; }
#endif

/* bled.c */
#if defined(RTCONFIG_BLINK_LED)
extern int __config_netdev_bled(const char *led_gpio, const char *ifname, unsigned int min_blink_speed, unsigned int interval);
extern int config_netdev_bled(const char *led_gpio, const char *ifname);
extern int set_bled_udef_pattern(const char *led_gpio, unsigned int interval, const char *pattern);
extern int set_bled_udef_tigger(const char *main_led_gpio, const char *tigger);
extern int set_bled_normal_mode(const char *led_gpio);
extern int set_bled_udef_pattern_mode(const char *led_gpio);
extern int start_bled(unsigned int gpio_nr);
extern int stop_bled(unsigned int gpio_nr);
extern int del_bled(unsigned int gpio_nr);
extern int append_netdev_bled_if(const char *led_gpio, const char *ifname);
extern int remove_netdev_bled_if(const char *led_gpio, const char *ifname);
extern int __config_swports_bled(const char *led_gpio, unsigned int port_mask, unsigned int min_blink_speed, unsigned int interval, int sleep);
extern int update_swports_bled(const char *led_gpio, unsigned int port_mask);
extern int __config_usbbus_bled(const char *led_gpio, char *bus_list, unsigned int min_blink_speed, unsigned int interval);
extern int is_swports_bled(const char *led_gpio);
extern int __config_interrupt_bled(const char *led_gpio, char *interrupt_list, unsigned int min_blink_speed, unsigned int interval);
extern int config_interrupt_bled(const char *led_gpio, char *interrupt_list);
extern int add_gpio_to_bled(const char *main_led_gpio, const char *led_gpio);
#if (defined(PLN12) || defined(PLAC56))
extern void set_wifiled(int mode);
#elif defined(MAPAC1750)
/* color */
#define RGBLED_OFF			0x0
#define RGBLED_BLED			0x1
#define RGBLED_GLED			0x2
#define RGBLED_RLED			0x4
#define RGBLED_COLOR_MESK		RGBLED_BLED | RGBLED_GLED | RGBLED_RLED
#define RGBLED_BLUE			RGBLED_BLED
#define RGBLED_GREEN			RGBLED_GLED
#define RGBLED_RED			RGBLED_RLED
#define RGBLED_WHITE			RGBLED_COLOR_MESK
#define RGBLED_NIAGARA_BLUE		RGBLED_BLED | RGBLED_GLED
#define RGBLED_YELLOW			RGBLED_GLED | RGBLED_RLED
#define RGBLED_PURPLE			RGBLED_BLED | RGBLED_RLED
/* blink */
#define RGBLED_SBLINK			0x10
#define RGBLED_3ON1OFF			0x20
#define RGBLED_ATE_MODE			0x40
#define RGBLED_3ON3OFF			0x80
#define RGBLED_BLINK_MESK		RGBLED_SBLINK | RGBLED_3ON1OFF | RGBLED_ATE_MODE | RGBLED_3ON3OFF
/* color+blink */
#define RGBLED_BLUE_3ON1OFF		RGBLED_BLUE | RGBLED_3ON1OFF
#define RGBLED_BLUE_3ON3OFF		RGBLED_BLUE | RGBLED_3ON3OFF
#define RGBLED_PURPLE_3ON1OFF		RGBLED_PURPLE | RGBLED_3ON1OFF
#define RGBLED_WHITE_SBLINK		RGBLED_WHITE | RGBLED_SBLINK
#define RGBLED_YELLOW_SBLINK		RGBLED_YELLOW | RGBLED_SBLINK
extern void set_rgbled(unsigned int mode);
#endif

static inline void enable_wifi_bled(char *ifname)
{
	int unit;
	int v = LED_ON;

	if (!ifname || *ifname == '\0')
		return;
	unit = get_wifi_unit(ifname);
	if (unit < 0 || unit > MAX_NR_WL_IF) {
		return;
	}

	if (!guest_wlif(ifname)) {
#if defined(RTCONFIG_QCA)
		v = LED_OFF;	/* WiFi not ready. Don't turn on WiFi LED here. */		
#endif
#if defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTN56UB2) || defined(RTAC1200GA1) || defined(RTAC1200GU) || defined(RTAC85U)
		if(!get_radio(1, 0) && unit==1) //*5G WiFi not ready. Don't turn on WiFi GPIO LED . */
		 	v=LED_OFF;
#endif		
#if defined(RTN56UB1) || defined(RTN56UB2) || defined(RTAC1200GA1) || defined(RTAC1200GU) || defined(RTAC85U) || defined(RTN800HP)
		if(!get_radio(0, 0) && unit==0) //*2G WiFi not ready. Don't turn on WiFi GPIO LED . */
		 	v=LED_OFF;
#endif		
		led_control(get_wl_led_id(unit), v);
	} else {
		append_netdev_bled_if(get_wl_led_gpio_nv(unit), ifname);
	}
}

static inline void disable_wifi_bled(char *ifname)
{
	int unit;

	if (!ifname || *ifname == '\0')
		return;
	unit = get_wifi_unit(ifname);
	if (unit < 0 || unit >= MAX_NR_WL_IF)
		return;
#if !defined(RTCONFIG_HAS_5G_2)
	if (unit == 2)
		return;
#endif

	if (!guest_wlif(ifname)) {
		led_control(get_wl_led_id(unit), LED_OFF);
	} else {
		remove_netdev_bled_if(get_wl_led_gpio_nv(unit), ifname);
	}
}

static inline int config_swports_bled(const char *led_gpio, unsigned int port_mask)
{
	unsigned int min_blink_speed = 10;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_swports_bled(led_gpio, port_mask, min_blink_speed, interval, 0);
}

static inline int config_swports_bled_sleep(const char *led_gpio, unsigned int port_mask)
{
	unsigned int min_blink_speed = 10;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_swports_bled(led_gpio, port_mask, min_blink_speed, interval, 1);
}

static inline int config_usbbus_bled(const char *led_gpio, char *bus_list)
{
	unsigned int min_blink_speed = 50;	/* KB/s */
	unsigned int interval = 100;		/* ms */

	return __config_usbbus_bled(led_gpio, bus_list, min_blink_speed, interval);
}
#else	/* !RTCONFIG_BLINK_LED */
static inline int __config_netdev_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) const char *ifname, __attribute__ ((unused)) unsigned int min_blink_speed, __attribute__ ((unused)) unsigned int interval) { return 0; }
static inline int config_netdev_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) const char *ifname) { return 0; }
static inline int set_bled_udef_pattern(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) unsigned int interval, __attribute__ ((unused)) const char *pattern) { return 0; }
static inline int set_bled_udef_tigger(__attribute__ ((unused)) const char *main_led_gpio, __attribute__ ((unused)) const char *tigger) { return 0; }
static inline int set_bled_normal_mode(__attribute__ ((unused)) const char *led_gpio) { return 0; }
static inline int set_bled_udef_pattern_mode(__attribute__ ((unused)) const char *led_gpio) { return 0; }
static inline int start_bled(__attribute__ ((unused)) unsigned int gpio_nr) { return 0; }
static inline int stop_bled(__attribute__ ((unused)) unsigned int gpio_nr) { return 0; }
static inline int chg_bled_state(__attribute__ ((unused)) unsigned int gpio_nr) { return 0; }
static inline int del_bled(__attribute__ ((unused)) unsigned int gpio_nr) { return 0; }
static inline int append_netdev_bled_if(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) const char *ifname) { return 0; }
static inline int remove_netdev_bled_if(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) const char *ifname) { return 0; }
static inline int __config_swports_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) unsigned int port_mask, __attribute__ ((unused)) unsigned int min_blink_speed, __attribute__ ((unused)) unsigned int interval, __attribute__ ((unused)) int sleep) { return 0; }
static inline int update_swports_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) unsigned int port_mask) { return 0; }
static inline int __config_usbbus_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) char *bus_list, __attribute__ ((unused)) unsigned int min_blink_speed, __attribute__ ((unused)) unsigned int interval) { return 0; }
static inline int __config_interrupt_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) char *interrupt_list, __attribute__ ((unused)) unsigned int min_blink_speed, __attribute__ ((unused)) unsigned int interval) { return 0; }
static inline int config_interrupt_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) char *interrupt_list) { return 0; }

static inline void enable_wifi_bled(__attribute__ ((unused)) char *ifname) { }
static inline void disable_wifi_bled(__attribute__ ((unused)) char *ifname) { }
static inline int config_swports_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) unsigned int port_mask) { return 0; }
static inline int config_swports_bled_sleep(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) unsigned int port_mask) { return 0; }
static inline int config_usbbus_bled(__attribute__ ((unused)) const char *led_gpio, __attribute__ ((unused)) char *bus_list) { return 0; }
static inline int is_swports_bled(__attribute__ ((unused)) const char *led_gpio) { return 0; }
static inline int add_gpio_to_bled(__attribute__ ((unused)) const char *main_led_gpio, __attribute__ ((unused)) const char *led_gpio) { return 0; }

#endif	/* RTCONFIG_BLINK_LED */

/* bwdpi_utlis.c */
#if defined(RTCONFIG_BWDPI)
extern int check_wrs_switch(void);
extern int check_bwdpi_nvram_setting(void);
#endif
extern void StampToDate(unsigned long timestamp, char *date);
extern int check_filesize_over(char *path, long int size);
extern time_t get_last_month_timestamp(void);

/* mac_name_tab.c */
extern char *search_mnt(char *mac);

#if defined(RTCONFIG_USB)
static inline int is_usb3_port(char *usb_node)
{
	if (!usb_node)
		return 0;

#if defined(RTCONFIG_QCA)
	/* If your platform has two USB3 bus, first item of ehci_port must be checked. */
	if (strstr(usb_node, get_usb_xhci_port(0)) ||
	    strstr(usb_node, get_usb_ehci_port(0))
	   )
		return 1;

	return 0;
#else
	if (strstr(usb_node, get_usb_xhci_port(0)) || strstr(usb_node, get_usb_xhci_port(1)))
		return 1;

	return 0;
#endif
}
#endif

#if defined(RTCONFIG_M2_SSD)
static inline int is_m2ssd_port(char *usb_node)
{
	if (!usb_node)
		return 0;

	if (strstr(usb_node, get_usb_ehci_port(2)))
		return 1;

	return 0;
}
#else
static inline int is_m2ssd_port(__attribute__ ((unused)) char *usb_node) { return 0; }
#endif

#ifdef RTCONFIG_BCM5301X_TRAFFIC_MONITOR

#if defined(RTN18U) || defined(RTAC56U) || defined(RTAC56S) || defined(RTAC68U) || defined(RTAC3200) || defined(DSL_AC68U)
#define WAN0DEV "vlan2"
#endif

#if defined(RTAC5300) || defined(GTAC5300)
#define WAN0DEV "vlan2"
#endif

#if defined(RTAC88U) || defined(RTAC86U) || defined(AC2900) || defined(RTAC3100)
#ifdef RTCONFIG_EXT_RTL8365MB
#define WAN0DEV "vlan2"
#else
#define WAN0DEV "vlan2"
#endif
#endif

#ifdef RTAC87U
#define WAN0DEV "vlan2"
#endif
#endif	/* RTCONFIG_BCM5301X_TRAFFIC_MONITOR */

#ifdef RTCONFIG_CAPTIVE_PORTAL
extern void deauth_guest_sta(char *, char *);
extern int FindBrifByWlif(char *wl_ifname, char *brif_name, int size);
#endif

#ifdef RTCONFIG_HTTPS
#define HTTPD_CERT	"/etc/cert.pem"
#define HTTPD_KEY	"/etc/key.pem"
#define LIGHTTPD_CERTKEY	"/etc/server.pem"
#define UPLOAD_CERT_FOLDER	"/jffs/.cert"
#define UPLOAD_CERT	"/jffs/.cert/cert.pem"
#define UPLOAD_KEY	"/jffs/.cert/key.pem"
#ifdef RTCONFIG_LETSENCRYPT
#define ACME_CERTHOME	"/jffs/.le"
#endif
#endif

#ifdef RTAC68U
extern int is_ac66u_v2_series();
extern int is_n66u_v2();
extern int hw_usb_cap();
extern int is_ssid_rev3_series();
#ifdef RTCONFIG_TCODE
extern unsigned int hardware_flag();
#endif
extern int is_dpsta_repeater();
extern void ac68u_cofs();
#endif

/* rtstate.c */
extern char *get_default_ssid(int unit, int subunit);

extern int wanport_status(int wan_unit);
/* bwdpi_utils.c */
extern void erase_symbol(char *old, char *sym);

/* pwenc.c */
#ifdef RTCONFIG_NVRAM_ENCRYPT
#define NVRAM_ENC_LEN	1024
#define NVRAM_ENC_MAXLEN	4096
extern int pw_enc(const char *input, char *output);
extern int pw_dec(const char *input, char *output);
extern int pw_enc_blen(const char *input);
extern int pw_dec_len(const char *input);
extern int set_enc_nvram(char *name, char *input, char *output);
extern int enc_nvram(char *name, char *input, char *output);
extern int dec_nvram(char *name, char *input, char *output);
extern int start_enc_nvram(void);
extern int start_dec_nvram(void);
extern int init_enc_nvram(void);
#endif

/* amas_utils.c */
#ifdef RTCONFIG_AMAS
extern int is_wlsta_exist(int unit, int vidx);
extern int is_wlsta_connect(int unit, int vidx, char *macaddr);
extern void set_deauth_sta(int bssidx, int vifidx, char *mac_addr);
#endif

enum {
	CKN_STR_DEFAULT_ASUS = 0,
	CKN_STR1,
	CKN_STR2,
	CKN_STR3,
	CKN_STR4,
	CKN_STR5,
	CKN_STR6,
	CKN_STR7,
	CKN_STR8,
	CKN_STR10   = 10,
	CKN_STR12   = 12,
	CKN_STR15   = 15,
	CKN_STR16,
	CKN_STR17,
	CKN_STR20   = 20,
	CKN_STR32   = 32,
	CKN_STR39   = 39,
	CKN_STR63   = 63,
	CKN_STR64   = 64,
	CKN_STR65,
	CKN_STR100  = 100,
	CKN_STR128  = 128,
	CKN_STR255  = 255,
	CKN_STR256,
	CKN_STR512  = 512,
	CKN_STR1024 = 1024,
	CKN_STR2048 = 2048,
	CKN_STR2500 = 2500,
	CKN_STR2999 = 2999,
	CKN_STR3999 = 3999,
	CKN_STR4096 = 4096,
	CKN_STR5500 = 5500,
	CKN_STR7999 = 7999,
	CKN_STR8192 = 8192,
	CKN_STR_MAX = 65535
};

#define CKN_STR_DEFAULT 1024	// Otherwise a whole bunch of nvram can't be changed by webui

enum {
	CKN_TYPE_DEFAULT = 0
};

enum {
	CKN_ACC_LEVEL_DEFAULT = 0
};

enum {
	CKN_ENC_DEFAULT = 0,
	CKN_ENC_SVR
};

#ifdef RTCONFIG_COOVACHILLI
extern void deauth_guest_sta(char *, char *);
#endif

#ifdef RTCONFIG_CFGSYNC
#define	CFGSYNC_GROUPID_LEN	CKN_STR32
#define CLIENT_STALIST_JSON_PATH	"/tmp/stalist.json"
extern int is_valid_group_id(const char *);
extern char *if_nametoalias(char *name, char *alias, int alias_len);
extern int check_re_in_macfilter(int unit, char *mac);
extern void update_macfilter_relist();
#endif

#if defined(RTCONFIG_DETWAN) && (defined(RTCONFIG_SOC_IPQ40XX))
extern void vlan_accept_vid_via_switch(int accept, int wan, int lan);
extern int detwan_set_def_vid(const char *ifname, int vid, int needTagged, int avoidVid);
#endif

extern int IPTV_ports_cnt(void);

#ifdef RTCONFIG_BCMWL6
#define WL_5G_BAND_2	1 << (2 - 1)
#define WL_5G_BAND_3	1 << (3 - 1)
#define WL_5G_BAND_4	1 << (4 - 1)
#endif

enum {
	UI_SW_MODE_NONE=0,
	UI_SW_MODE_ROUTER,
	UI_SW_MODE_REPEATER,
	UI_SW_MODE_AP,
	UI_SW_MODE_MB,
	UI_SW_MODE_HOTSPOT,
	UI_SW_MODE_EXPRESS_2G,
	UI_SW_MODE_EXPRESS_5G
};

static inline int get_sw_mode(void)
{
	int wlc_psta = nvram_get_int("wlc_psta");
	int wlc_express = nvram_get_int("wlc_express");

	switch(sw_mode()){
		case 1:
			return UI_SW_MODE_ROUTER;
		case 2:
			if(wlc_psta==0){
				if(wlc_express==0)	/* BCM/RALINK/QCA/LANTIQ/REALTEK */
					return UI_SW_MODE_REPEATER;
				else if(wlc_express==1)	/* REALTEK RP-series */
					return UI_SW_MODE_EXPRESS_2G;
				else if(wlc_express==2)	/* REALTEK RP-series */
					return UI_SW_MODE_EXPRESS_5G;
			}else if(wlc_psta==1)	/* RALINK / QCA / LANTIQ */
				return UI_SW_MODE_MB;
			break;
		case 3:
			if(wlc_psta==0)	/* BCM/RALINK/QCA/LANTIQ/REALTEK */
				return UI_SW_MODE_AP;
			else if(wlc_psta==1)	/* BCM/REALTEK */
				return UI_SW_MODE_MB;
			else if(wlc_psta==2 || wlc_psta==3)	/* BCM */
				return UI_SW_MODE_REPEATER;
			break;
		case 4:
			return UI_SW_MODE_HOTSPOT;
	}

	return UI_SW_MODE_NONE;
}

#if defined(RTCONFIG_SW_HW_AUTH) && defined(RTCONFIG_AMAS)
/*
	AMAS define type bitmap
*/
#define AMAS_CAP 0x0001
#define AMAS_RE  0x0002

/*
	API define and bitmap define in sw-hw-auth 
	getAmasSupportMode() : global API for amas usage
	0 : not support AMAS
	1 : support CAP only
	2 : support RE only
	3 : CAP + RE
 */
extern int getAmasSupportMode();

#endif  /* defined(RTCONFIG_SW_HW_AUTH) && defined(RTCONFIG_AMAS) */

#endif	/* !__SHARED_H__ */
