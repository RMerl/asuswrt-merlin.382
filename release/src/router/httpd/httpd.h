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
 * milli_httpd - pretty small HTTP server
 *
 * Copyright (C) 2001 ASUSTeK Inc.
 *
 */

#ifndef _httpd_h_
#define _httpd_h_

#define _GNU_SOURCE

#include <arpa/inet.h>
#if defined(DEBUG) && defined(DMALLOC)
#include <dmalloc.h>
#endif
#include <rtconfig.h>
#include <unlock.h>

/* DEBUG DEFINE */
#define HTTPD_DEBUG             "/tmp/HTTPD_DEBUG"

/* DEBUG FUNCTION */

#define HTTPD_DBG(fmt,args...) \
        if(f_exists(HTTPD_DEBUG) > 0) { \
                char info[1024]; \
                snprintf(info, sizeof(info), "echo \"[HTTPD][%s:(%d)]"fmt"\" >> /tmp/HTTPD_DEBUG.log", __FUNCTION__, __LINE__, ##args); \
                system(info); \
        }

/* Basic authorization userid and passwd limit */
#define AUTH_MAX 64

#define DEFAULT_LOGIN_MAX_NUM	5

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define IPV6_CLIENT_LIST        "/tmp/ipv6_client_list"

/* Generic MIME type handler */
struct mime_handler {
	char *pattern;
	char *mime_type;
	char *extra_header;
	void (*input)(char *path, FILE *stream, int len, char *boundary);
	void (*output)(char *path, FILE *stream);
	void (*auth)(char *userid, char *passwd, char *realm);
};

extern struct mime_handler mime_handlers[];

struct useful_redirect_list {
	char *pattern;
	char *mime_type;
};

extern struct useful_redirect_list useful_redirect_lists[];

#ifdef RTCONFIG_AMAS
struct AiMesh_whitelist {
	char *pattern;
	char *mime_type;
};
extern struct AiMesh_whitelist AiMesh_whitelists[];
#endif

struct stb_port {
        char *value;
        char *name;
};

struct model_stb_port {
        int model;
        char *odmpid;
        struct stb_port port_list[8];
};

struct iptv_profile {
        char *profile_name;

        /* for layout*/
        char *iptv_port;
        char *voip_port;
        char *bridge_port;
        char *iptv_config;
        char *voip_config;

        /* vlan settings */
        char *switch_wantag;
        char *switch_stb_x;
        char *switch_wan0tagid;
        char *switch_wan0prio;
        char *switch_wan1tagid;
        char *switch_wan1prio;
        char *switch_wan2tagid;
        char *switch_wan2prio;

        /* special applications */
        char *mr_enable_x;
        char *emf_enable;
        char *wan_vpndhcp;
        char *quagga_enable;
        char *mr_altnet_x;
        char *ttl_inc_enable;
};

#ifdef RTCONFIG_ODMPID
struct REPLACE_ODMPID_S {
        char *org_name;
        char *replace_name;
};
#endif

#define MIME_EXCEPTION_NOAUTH_ALL 	1<<0
#define MIME_EXCEPTION_NOAUTH_FIRST	1<<1
#define MIME_EXCEPTION_NORESETTIME	1<<2
#define MIME_EXCEPTION_MAINPAGE 	1<<3
#define CHECK_REFERER	1

#define SERVER_NAME "httpd/2.0"
#define SERVER_PORT 80
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

//asus token status for APP
#define NOTOKEN		1
#define AUTHFAIL	2
#define ACCOUNTFAIL	3
#define NOREFERER	4
#define WEB_NOREFERER	5
#define REFERERFAIL	6
#define LOGINLOCK	7
#define ISLOGOUT	8
#define NOLOGIN		9

#define LOCKTIME 60*5

/* image path for app */
#define IMAGE_MODEL_PRODUCT	"/Model_product.png"
#define IMAGE_WANUNPLUG		"/WANunplug.png"
#define IMAGE_ROUTER_MODE	                              "/rt.jpg"
#define IMAGE_REPEATER_MODE	"/re.jpg"
#define IMAGE_AP_MODE		"/ap.jpg"
#define IMAGE_MEDIA_BRIDGE_MODE	"/mb.jpg"

//Add Login Try
#define NOLOGINTRY   0
#define LOGINTRY   1

#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
#define IFTTTUSERAGENT  "asusrouter-Windows-IFTTT-1.0"
#define GETIFTTTCGI     "get_IFTTTPincode.cgi"
#define GETIFTTTOKEN "get_IFTTTtoken.cgi"
#endif

/* Exception MIME handler */
struct except_mime_handler {
	char *pattern;
	int flag;
};

extern struct except_mime_handler except_mime_handlers[];

/* MIME referer */
struct mime_referer {
	char *pattern;
	int flag;
};

extern struct mime_referer mime_referers[];

typedef struct asus_token_table asus_token_t;
struct asus_token_table{
	char useragent[1024];
	char token[32];
	char ipaddr[16];
	char login_timestampstr[32];
	char host[64];
	asus_token_t *next;
};

#define INC_ITEM        128
#define REALLOC_VECTOR(p, len, size, item_size) {                               \
        assert ((len) >= 0 && (len) <= (size));                                         \
        if (len == size)        {                                                                               \
                int new_size;                                                                                   \
                void *np;                                                                                               \
                /* out of vector, reallocate */                                                 \
                new_size = size + INC_ITEM;                                                             \
                np = malloc (new_size * (item_size));                                   \
                assert (np != NULL);                                                                    \
                bzero (np, new_size * (item_size));                                             \
                memcpy (np, p, len * (item_size));                                              \
                free (p);                                                                                               \
                p = np;                                                                                                 \
                size = new_size;                                                                                \
        }    \
}


/* CGI helper functions */
extern void init_cgi(char *query);
extern char * get_cgi(char *name);
extern char * webcgi_get(const char *name);  //Viz add 2010.08
#if 0
typedef struct kw_s     {
        int len, tlen;                                          // actually / total
        unsigned char **idx;
        unsigned char *buf;
} kw_t, *pkw_t;

extern int load_dictionary (char *lang, pkw_t pkw);
extern void release_dictionary (pkw_t pkw);
extern char* search_desc (pkw_t pkw, char *name);
#endif
#ifdef TRANSLATE_ON_FLY
//2008.10 magic{
struct language_table{
	char *Lang;
	char *Target_Lang;
};

extern struct language_table language_tables[];

//2008.10 magic}
typedef struct kw_s     {
        int len, tlen;                                          // actually / total
        char **idx;
        char *buf;
} kw_t, *pkw_t;

#define INC_ITEM        128
#define REALLOC_VECTOR(p, len, size, item_size) {                               \
        assert ((len) >= 0 && (len) <= (size));                                         \
        if (len == size)        {                                                                               \
                int new_size;                                                                                   \
                void *np;                                                                                               \
                /* out of vector, reallocate */                                                 \
                new_size = size + INC_ITEM;                                                             \
                np = malloc (new_size * (item_size));                                   \
                assert (np != NULL);                                                                    \
                bzero (np, new_size * (item_size));                                             \
                memcpy (np, p, len * (item_size));                                              \
                free (p);                                                                                               \
                p = np;                                                                                                 \
                size = new_size;                                                                                \
        }    \
}
#endif  // defined TRANSLATE_ON_FLY


/* Regular file handler */
extern void do_file(char *path, FILE *stream);

/* GoAhead 2.1 compatibility */
typedef FILE * webs_t;
typedef char char_t;
#define T(s) (s)
#define __TMPVAR(x) tmpvar ## x
#define _TMPVAR(x) __TMPVAR(x)
#define TMPVAR _TMPVAR(__LINE__)
#define websWrite(wp, fmt, args...) ({ int TMPVAR = fprintf(wp, fmt, ## args); fflush(wp); TMPVAR; })
#define websError(wp, code, msg, args...) fprintf(wp, msg, ## args)
#define websHeader(wp) fputs("<html lang=\"en\">", wp)
#define websFooter(wp) fputs("</html>", wp)
#define websDone(wp, code) fflush(wp)
#define websGetVar(wp, var, default) (get_cgi(var) ? : default)
#define websDefaultHandler(wp, urlPrefix, webDir, arg, url, path, query) ({ do_ej(path, wp); fflush(wp); 1; })
#define websWriteData(wp, buf, nChars) ({ int TMPVAR = fwrite(buf, 1, nChars, wp); fflush(wp); TMPVAR; })
#define websWriteDataNonBlock websWriteData

extern int ejArgs(int argc, char_t **argv, char_t *fmt, ...);

/* GoAhead 2.1 Embedded JavaScript compatibility */
extern void do_ej(char *path, FILE *stream);
struct ej_handler {
	char *pattern;
	int (*output)(int eid, webs_t wp, int argc, char_t **argv);
};
extern struct ej_handler ej_handlers[];

#define MAX_LOGIN_BLOCK_TIME	300
#define LOCK_LOGIN_LAN 	0x01
#define LOCK_LOGIN_WAN 	0x02

#ifdef vxworks
#define fopen(path, mode)	tar_fopen((path), (mode))
#define fclose(fp)		tar_fclose((fp))
#undef getc
#define getc(fp)		tar_fgetc((fp))
extern FILE * tar_fopen(const char *path, const char *mode);
extern void tar_fclose(FILE *fp);
extern int tar_fgetc(FILE *fp);
#endif
#ifdef TRANSLATE_ON_FLY

extern int check_lang_support(char *lang);
extern int load_dictionary (char *lang, pkw_t pkw);
extern void release_dictionary (pkw_t pkw);
extern char* search_desc (pkw_t pkw, char *name);
extern int change_preferred_lang(int finish);
extern int get_lang_num();
//extern char Accept_Language[16];
#else
static inline int check_lang_support(char *lang) { return 1; }
#endif //defined TRANSLATE_ON_FLY

extern int http_port;

/* api-*.c */
extern int check_imageheader(char *buf, long *filelen);
extern int check_imagefile(char *fname);
extern unsigned int get_radio_status(char *ifname);

/* aspbw.c */
extern void do_f(char *path, webs_t wp);

/* cgi.c */
extern int web_read(void *buffer, int len);
extern void set_cgi(char *name, char *value);

/* httpd.c */
extern void start_ssl(void);
extern char *gethost(void);
extern void http_logout(unsigned int ip, char *cookies, int fromapp_flag);
extern int is_auth(void);
extern int is_firsttime(void);
extern char *generate_token(char *token_buf, size_t length);
extern int match( const char* pattern, const char* string );
extern int match_one( const char* pattern, int patternlen, const char* string );
extern void send_page( int status, char* title, char* extra_header, char* text , int fromapp);
extern void send_content_page( int status, char* title, char* extra_header, char* text , int fromapp);
extern char *get_referrer(char *referer, char *auth_referer, size_t length);

/* web.c */
extern int ej_lan_leases(int eid, webs_t wp, int argc, char_t **argv);
extern int get_nat_vserver_table(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_route_table(int eid, webs_t wp, int argc, char_t **argv);
extern void copy_index_to_unindex(char *prefix, int unit, int subunit);
extern void logmessage(char *logheader, char *fmt, ...);
extern int is_private_subnet(const char *ip);
extern char* INET6_rresolve(struct sockaddr_in6 *sin6, int numeric);
extern char *trim_r(char *str);
extern void write_encoded_crt(char *name, char *value);
extern int is_wlif_up(const char *ifname);
extern void add_asus_token(char *token);
extern int check_token_timeout_in_list(void);
extern asus_token_t* search_timeout_in_list(asus_token_t **prev, int fromapp_flag);
extern asus_token_t* create_list(char *token);
extern void get_ipv6_client_info(void);
extern void get_ipv6_client_list(void);
extern int inet_raddr6_pton(const char *src, void *dst, void *buf);
extern int delete_logout_from_list(char *cookies);
extern void set_referer_host(void);
extern int check_xss_blacklist(char* para, int check_www);
extern int check_cmd_whitelist(char* para);
extern int useful_redirect_page(char *next_page);
extern char* reverse_str( char *str );
#ifdef RTCONFIG_AMAS
extern int check_AiMesh_whitelist(char *page);
#endif
#ifdef RTCONFIG_DNSPRIVACY
extern int ej_get_dnsprivacy_presets(int eid, webs_t wp, int argc, char_t **argv);
#endif

/* web-*.c */
extern int ej_wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit);
extern int ej_wl_status_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wps_info_2g(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wps_info(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_wl_status_array(int eid, webs_t wp, int argc, char_t **argv, int unit);
extern int ej_wl_status_2g_array(int eid, webs_t wp, int argc, char_t **argv);

/* web.c/web-*.c */
extern char referer_host[64];
extern char host_name[64];
extern char user_agent[1024];
extern char gen_token[32];
extern unsigned int login_ip_tmp;
extern int check_user_agent(char* user_agent);
#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
extern void add_ifttt_flag(void);
#endif

#ifdef RTCONFIG_HTTPS
extern int gen_ddns_hostname(char *ddns_hostname);
extern int check_model_name(void);
extern char *pwenc(char *input, char *output, int len);
#endif

#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
extern char ifttt_stoken[128];
extern char ifttt_query_string[2048];
extern time_t ifttt_timestamp;
extern char *gen_IFTTTPincode(char *pincode);
extern int gen_IFTTTtoken(char* stoken, char* token);
extern char* gen_IFTTT_inviteCode(char* inviteCode, char *asus_flag);
extern int check_ifttt_token(char* asus_token);
extern void ifttt_log(char* url, char* file);
extern int alexa_block_internet(int block);
#endif

extern unsigned int MAX_login;
extern int cur_login_ip_type;
extern time_t login_timestamp_tmp; // the timestamp of the current session.
extern time_t last_login_timestamp; // the timestamp of the current session.
extern time_t login_timestamp_tmp_wan; // the timestamp of the current session.
extern time_t last_login_timestamp_wan; // the timestamp of the current session.
extern unsigned int login_try;
extern unsigned int login_try_wan;
extern time_t auth_check_dt;
extern int lock_flag;
extern int add_try;
extern char auth_passwd[AUTH_MAX];
extern char* ipisdomain(char* hostname, char* str);
#ifdef RTCONFIG_AMAS
extern char* iscap(char* str);
#endif
extern int referer_check(char* referer, int fromapp_flag);
extern int auth_check( char* dirname, char* authorization, char* url, char* file, char* cookies, int fromapp_flag);
extern int check_noauth_referrer(char* referer, int fromapp_flag);
extern char current_page_name[128];
extern int gen_guestnetwork_pass(char *key, size_t size);
extern int alexa_pause_internet(int pause);
extern int httpd_sw_hw_check(void);
extern int ej_get_ui_support(int eid, webs_t wp, int argc, char **argv);
extern void page_default_redirect(int fromapp_flag, char* url);
#ifdef RTCONFIG_LANTIQ
extern int wave_app_flag;
extern int wave_handle_app_flag(char *name, int wave_app_flag);
#endif
#endif /* _httpd_h_ */
