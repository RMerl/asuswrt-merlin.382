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
/* milli_httpd - pretty small HTTP server
** A combination of
** micro_httpd - really small HTTP server
** and
** mini_httpd - small HTTP server
**
** Copyright ?1999,2000 by Jef Poskanzer <jef@acme.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <ctype.h>

typedef unsigned int __u32;   // 1225 ham

#include <httpd.h>
//2008.08 magic{
#include <bcmnvram.h>	//2008.08 magic
#include <arpa/inet.h>	//2008.08 magic

#include <error.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <shared.h>
#include <shutils.h>

#define ETCPHYRD	14
#define SIOCGETCPHYRD   0x89FE
//#include "etioctl.h"

#include <sys/file.h>

#ifdef RTCONFIG_HTTPS
#include <syslog.h>
#include <mssl.h>
//#include <shutils.h>
#define SERVER_PORT_SSL	443
#endif
#include "bcmnvram_f.h"

/* A multi-family sockaddr. */
typedef union {
    struct sockaddr sa;
    struct sockaddr_in sa_in;
} usockaddr;

#include "queue.h"
#define MAX_CONN_ACCEPT 64
#define MAX_CONN_TIMEOUT 60

typedef struct conn_item {
	TAILQ_ENTRY(conn_item) entry;
	int fd;
	usockaddr usa;
} conn_item_t;

typedef struct conn_list {
	TAILQ_HEAD(, conn_item) head;
	int count;
} conn_list_t;

/* Globals. */
static FILE *conn_fp;
static char auth_userid[AUTH_MAX];
char auth_passwd[AUTH_MAX];
static char auth_realm[AUTH_MAX];
char host_name[64];
char referer_host[64];
char current_page_name[128];
char user_agent[1024];
char gen_token[32];
int do_ssl = 0; 	// use Global for HTTPS upgrade judgment in web.c
int ssl_stream_fd; 	// use Global for HTTPS stream fd in web.c

#ifdef TRANSLATE_ON_FLY
char Accept_Language[16];

struct language_table language_tables[] = {
	{"br", "BR"},
	{"pt-BR", "BR"},
	{"zh-cn", "CN"},
	{"zh-Hans-CN", "CN"},
	{"cs", "CZ"},
	{"cs-cz", "CZ"},
	{"da", "DA"},
	{"da-DK", "DA"},
	{"de", "DE"},
	{"de-at", "DE"},
	{"de-ch", "DE"},
	{"de-de", "DE"},
	{"de-li", "DE"},
	{"de-lu", "DE"},
	{"en", "EN"},
	{"en-us", "EN"},
	{"es", "ES"},
	{"es-ec", "ES"},
	{"es-py", "ES"},
	{"es-pa", "ES"},
	{"es-ni", "ES"},
	{"es-gt", "ES"},
	{"es-do", "ES"},
	{"es-es", "ES"},
	{"es-hn", "ES"},
	{"es-ve", "ES"},
	{"es-pr", "ES"},
	{"es-ar", "ES"},
	{"es-bo", "ES"},
	{"es-us", "ES"},
	{"es-co", "ES"},
	{"es-cr", "ES"},
	{"es-uy", "ES"},
	{"es-pe", "ES"},
	{"es-cl", "ES"},
	{"es-mx", "ES"},
	{"es-sv", "ES"},
	{"fi", "FI"},
	{"fi-FI", "FI"},
	{"fr", "FR"},
	{"fr-fr", "FR"},
	{"fr-BE", "FR"},
	{"fr-CA", "FR"},
	{"fr-CH", "FR"},
	{"fr-MC", "FR"},
	{"fr-LU", "FR"},
	{"hu-hu", "HU"},
	{"hu", "HU"},
	{"it", "IT"},
	{"it-it", "IT"},
	{"it-ch", "IT"},
	{"ja", "JP"},
	{"ja-JP", "JP"},
	{"ko", "KR"},
	{"ko-kr", "KR"},
	{"ms", "MS"},
	{"ms-MY", "MS"},
	{"ms-BN", "MS"},
	{"nl", "NL"},
	{"nl-NL", "NL"},
	{"no", "NO"},
	{"nb", "NO"},
	{"nn", "NO"},
	{"nb-NO", "NO"},
	{"nn-NO", "NO"},
	{"pl-pl", "PL"},
	{"pl", "PL"},
	{"ru", "RU"},
	{"ru-ru", "RU"},
	{"ro", "RO"},
	{"ro-ro", "RO"},
	{"sl", "SL"},
	{"sl-SI", "SL"},
	{"sv", "SV"},
	{"sv-FI", "SV"},
	{"sv-SE", "SV"},
	{"th", "TH"},
	{"th-TH", "TH"},
	{"th-TH-TH", "TH"},
	{"tr", "TR"},
	{"tr-TR", "TR"},
	{"zh", "TW"},
	{"zh-tw", "TW"},
	{"zh-Hant-TW", "TW"},
	{"zh-hk", "TW"},
	{"uk", "UK"},
	{NULL, NULL}
};

#endif //TRANSLATE_ON_FLY

/* Forwards. */
static int initialize_listen_socket(usockaddr* usa, const char *ifname);
int check_noauth_referrer(char* referer, int fromapp_flag);
char *get_referrer(char *referer, char *auth_referer, size_t length);
static void send_error( int status, char* title, char* extra_header, char* text );
//#ifdef RTCONFIG_CLOUDSYNC
void send_page( int status, char* title, char* extra_header, char* text , int fromapp);
//#endif
static void send_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp);
static void send_token_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp);
static void handle_request(void);
void send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time, int logintry);
void page_default_redirect(int fromapp_flag, char* url);
int check_user_agent(char* user_agent);
#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
void add_ifttt_flag(void);
#endif

int check_current_ip_is_lan_or_wan();

/* added by Joey */
//2008.08 magic{
//int redirect = 1;
int redirect = 0;
int change_passwd = 0;
int reget_passwd = 0;
int x_Setting = 0;
int skip_auth = 0;
char url[128];
int http_port = 0;
char *http_ifname = NULL;
time_t login_dt=0;
char login_url[128];
int login_error_status = 0;
char cloud_file[256];
int add_try = 0;


/* Added by Joey for handle one people at the same time */
unsigned int login_ip=0; // the logined ip
time_t login_timestamp=0; // the timestamp of the logined ip
time_t login_timestamp_tmp=0; // the timestamp of the current session.
time_t last_login_timestamp=0; // the timestamp of the current session.
unsigned int login_ip_tmp=0; // the ip of the current session.
unsigned int login_try=0;
unsigned int last_login_ip = 0;	// the last logined ip 2008.08 magic
//Add by Andy for handle the login block mechanism by LAN/WAN
time_t login_timestamp_tmp_wan=0; // the timestamp of the current session.
time_t last_login_timestamp_wan=0; // the timestamp of the current session.
time_t auth_check_dt=0;
unsigned int login_try_wan=0;
int cur_login_ip_type = -1;	//0:LAN, 1:WAN, -1:ERROR
unsigned int MAX_login;
int lock_flag = 0;

// 2008.08 magic {
time_t request_timestamp = 0;
time_t turn_off_auth_timestamp = 0;
int temp_turn_off_auth = 0;	// for QISxxx.htm pages

/* Const vars */
const int int_1 = 1;

void http_login(unsigned int ip, char *url);
void http_login_timeout(unsigned int ip, char *cookies, int fromapp_flag);
void http_logout(unsigned int ip, char *cookies, int fromapp_flag);
int http_login_check(void);

#if 0
static int check_if_inviteCode(const char *dirpath){
	return 1;
}
#endif
void sethost(char *host)
{
	char *cp;

	if(!host) return;

	memset(host_name, 0, sizeof(host_name));
	strlcpy(host_name, host, sizeof(host_name));

	cp = host_name;
	for ( cp = cp + 7; *cp && *cp != '\r' && *cp != '\n'; cp++ );
	*cp = '\0';
}

char *gethost(void)
{
	if(strlen(host_name)) {
		return host_name;
	}
	else return(nvram_safe_get("lan_ipaddr"));
}

#include <sys/sysinfo.h>
long uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);

	return info.uptime;
}

static int
initialize_listen_socket(usockaddr* usa, const char *ifname)
{
	struct ifreq ifr;
	int fd;

	memset(usa, 0, sizeof(usockaddr));
	usa->sa.sa_family = AF_INET;
	usa->sa_in.sin_addr.s_addr = htonl(INADDR_ANY);
	usa->sa_in.sin_port = htons(http_port);

	fd = socket(usa->sa.sa_family, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	if (ifname) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
			perror("ioctl");
			goto error;
		}
		usa->sa_in.sin_addr.s_addr = ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr;
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &int_1, sizeof(int_1)) < 0) {
		perror("setsockopt");
		goto error;
	}
	if (bind(fd, &usa->sa, sizeof(struct sockaddr_in)) < 0) {
		perror("bind");
		goto error;
	}
	if (listen(fd, 1024) < 0) {
		perror( "listen" );
		goto error;
	}

	return fd;

error:
	close(fd);
	return -1;
}

void 
page_default_redirect(int fromapp_flag, char* url)
{
	char inviteCode[256]={0};

	if(check_xss_blacklist(url, 1))
		strncpy(login_url, INDEXPAGE, sizeof(login_url));
	else
		strncpy(login_url, url, sizeof(login_url));

	if(fromapp_flag == 0){
		snprintf(inviteCode, sizeof(inviteCode), "<script>top.location.href='/page_default.cgi?url=%s';</script>", url);
	}
	send_page( 200, "OK", (char*) 0, inviteCode, fromapp_flag);
}

void
send_login_page(int fromapp_flag, int error_status, char* url, char* file, int lock_time, int logintry)
{
	HTTPD_DBG("error_status = %d\n", error_status);
	char inviteCode[256]={0};
	char buf[128] = {0};
	//char url_tmp[64]={0};
	char *cp, *file_var=NULL;

	if(logintry){
		if(!cur_login_ip_type)
		{
			++login_try;
			if(error_status != LOGINLOCK)
				last_login_timestamp = login_timestamp_tmp;
		}
		else
		{
			++login_try_wan;
			if(error_status != LOGINLOCK)
				last_login_timestamp_wan= login_timestamp_tmp_wan;
		}
	}

	if(url == NULL)
		strncpy(login_url, INDEXPAGE, sizeof(login_url));
	else
		strncpy(login_url, url, sizeof(login_url));

	login_dt = lock_time;

	login_error_status = error_status;

	if(fromapp_flag == 0){
		if(strncmp(login_url, "cloud_sync.asp", strlen(login_url))==0){
			if(file != NULL){
				cp = strstr(file,"flag=");
				if(cp != (char*) 0){
					file_var = &cp[5];
					memset(cloud_file, 0, sizeof(cloud_file));
					strncpy(cloud_file, file_var, sizeof(cloud_file));
				}
			}
		}
		else if(strncmp(login_url, "cfg_onboarding.cgi", strlen(login_url))==0){
			if(file != NULL){
				cp = strstr(file,"id=");
				if(cp != (char*) 0){
					file_var = &cp[3];
					if(!check_cmd_whitelist(file_var) && (strlen(file_var) == 12)){
						memset(cloud_file, 0, sizeof(cloud_file));
						strlcpy(cloud_file, file_var, sizeof(cloud_file));
					}
				}
			}
		}
		snprintf(inviteCode, sizeof(inviteCode), "<script>top.location.href='/Main_Login.asp';</script>");
	}else{
		snprintf(inviteCode, sizeof(inviteCode), "\"error_status\":\"%d\"", error_status);
		if(error_status == LOGINLOCK){
			snprintf(buf, sizeof(buf), ",\"remaining_lock_time\":\"%ld\"", LOCKTIME - login_dt);
			strcat(inviteCode, buf);
		}
	}
	send_page( 200, "OK", (char*) 0, inviteCode, fromapp_flag);
}

char
*get_referrer(char *referer, char *auth_referer, size_t length)
{
	char *cp1=NULL, *cp2=NULL, *location_cp=NULL, *location_cp1=NULL;

	if(strstr(referer,"\r") != (char*) 0)
		location_cp1 = strtok(referer, "\r");
	else
		location_cp1 = referer;

	location_cp = strstr(location_cp1,"//");
	if(location_cp != (char*) 0){
		cp1 = &location_cp[2];
		if(strstr(cp1,"/") != (char*) 0){
			cp2 = strtok(cp1, "/");
			strlcpy(auth_referer , cp2, length);
		}else
			strlcpy(auth_referer , cp1, length);
	}else
		strlcpy(auth_referer , location_cp1, length);

	return auth_referer;
}

static void
send_error( int status, char* title, char* extra_header, char* text )
{
	send_headers( status, title, extra_header, "text/html", 0);
	(void) fprintf( conn_fp, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\n<BODY BGCOLOR=\"#cc9999\"><H4>%d %s</H4>\n", status, title, status, title );
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "</BODY></HTML>\n" );
	(void) fflush( conn_fp );
}

//#ifdef RTCONFIG_CLOUDSYNC
void
send_page( int status, char* title, char* extra_header, char* text , int fromapp){
    if(fromapp == 0){
	send_headers( status, title, extra_header, "text/html", fromapp);
	(void) fprintf( conn_fp, "<HTML><HEAD>");
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "</HEAD></HTML>\n" );
    }else{
	send_headers( status, title, extra_header, "application/json;charset=UTF-8", fromapp );
	(void) fprintf( conn_fp, "{\n");
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "}\n" );	
    }
    (void) fflush( conn_fp );
}
//#endif

void
send_content_page( int status, char* title, char* extra_header, char* text , int fromapp){
	(void) fprintf( conn_fp, "<HTML><HEAD>");
	(void) fprintf( conn_fp, "%s\n", text );
	(void) fprintf( conn_fp, "</HEAD></HTML>\n" );
	(void) fflush( conn_fp );
}

static void
send_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp)
{
    time_t now;
    char timebuf[100];
    (void) fprintf( conn_fp, "%s %d %s\r\n", PROTOCOL, status, title );
    (void) fprintf( conn_fp, "Server: %s\r\n", SERVER_NAME );
    (void) fprintf( conn_fp, "x-frame-options: SAMEORIGIN\r\n");
    (void) fprintf( conn_fp, "x-xss-protection: 1; mode=block\r\n");

    if (fromapp != 0){
	(void) fprintf( conn_fp, "Cache-Control: no-store\r\n");	
	(void) fprintf( conn_fp, "Pragma: no-cache\r\n");
	if(fromapp == FROM_DUTUtil){
		(void) fprintf( conn_fp, "AiHOMEAPILevel: %d\r\n", EXTEND_AIHOME_API_LEVEL );
		(void) fprintf( conn_fp, "Httpd_AiHome_Ver: %d\r\n", EXTEND_HTTPD_AIHOME_VER );
		(void) fprintf( conn_fp, "Model_Name: %s\r\n", get_productid() );
	}else if(fromapp == FROM_ASSIA){
		(void) fprintf( conn_fp, "ASSIA_API_Level: %d\r\n", EXTEND_ASSIA_API_LEVEL );
	}
    }
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    (void) fprintf( conn_fp, "Date: %s\r\n", timebuf );
    if ( extra_header != (char*) 0 )
	(void) fprintf( conn_fp, "%s\r\n", extra_header );
    if ( mime_type != (char*) 0 ){
	if(fromapp != 0)
		(void) fprintf( conn_fp, "Content-Type: %s\r\n", "application/json;charset=UTF-8" );		
	else
		(void) fprintf( conn_fp, "Content-Type: %s\r\n", mime_type );
    }

    (void) fprintf( conn_fp, "Connection: close\r\n" );
    (void) fprintf( conn_fp, "\r\n" );
}

static void
send_token_headers( int status, char* title, char* extra_header, char* mime_type, int fromapp)
{
	time_t now;
	char timebuf[100];
	char asus_token[32]={0};
	memset(asus_token,0,sizeof(asus_token));

	if(nvram_match("x_Setting", "0") && strcmp( gen_token, "") != 0){
		strncpy(asus_token, gen_token, sizeof(asus_token));
	}else{
		generate_token(asus_token, sizeof(asus_token));
	}
	add_asus_token(asus_token);

    (void) fprintf( conn_fp, "%s %d %s\r\n", PROTOCOL, status, title );
    (void) fprintf( conn_fp, "Server: %s\r\n", SERVER_NAME );
    if(fromapp == FROM_DUTUtil){
	(void) fprintf( conn_fp, "AiHOMEAPILevel: %d\r\n", EXTEND_AIHOME_API_LEVEL );
	(void) fprintf( conn_fp, "Httpd_AiHome_Ver: %d\r\n", EXTEND_HTTPD_AIHOME_VER );
	(void) fprintf( conn_fp, "Model_Name: %s\r\n", get_productid() );
    }else if(fromapp == FROM_ASSIA){
	(void) fprintf( conn_fp, "ASSIA_API_Level: %d\r\n", EXTEND_ASSIA_API_LEVEL );
    }
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), RFC1123FMT, gmtime( &now ) );
    (void) fprintf( conn_fp, "Date: %s\r\n", timebuf );
    if ( extra_header != (char*) 0 )
	(void) fprintf( conn_fp, "%s\r\n", extra_header );
    if ( mime_type != (char*) 0 )
	(void) fprintf( conn_fp, "Content-Type: %s\r\n", mime_type );

	(void) fprintf( conn_fp, "Set-Cookie: asus_token=%s; HttpOnly;\r\n",asus_token );

    (void) fprintf( conn_fp, "Connection: close\r\n" );
    (void) fprintf( conn_fp, "\r\n" );
}

/* Simple shell-style filename matcher.  Only does ? * and **, and multiple
** patterns separated by |.  Returns 1 or 0.
*/
int
match( const char* pattern, const char* string )
    {
    const char* or;

    for (;;)
	{
	or = strchr( pattern, '|' );
	if ( or == (char*) 0 )
	    return match_one( pattern, strlen( pattern ), string );
	if ( match_one( pattern, or - pattern, string ) )
	    return 1;
	pattern = or + 1;
	}
    }


int
match_one( const char* pattern, int patternlen, const char* string )
    {
    const char* p;

    for ( p = pattern; p - pattern < patternlen; ++p, ++string )
	{
	if ( *p == '?' && *string != '\0' )
	    continue;
	if ( *p == '*' )
	    {
	    int i, pl;
	    ++p;
	    if ( *p == '*' )
		{
		/* Double-wildcard matches anything. */
		++p;
		i = strlen( string );
		}
	    else
		/* Single-wildcard matches anything but slash. */
		i = strcspn( string, "/" );
	    pl = patternlen - ( p - pattern );
	    for ( ; i >= 0; --i )
		if ( match_one( p, pl, &(string[i]) ) )
		    return 1;
	    return 0;
	    }
	if ( *p != *string )
	    return 0;
	}
    if ( *string == '\0' )
	return 1;
    return 0;
}

int web_write(const char *buffer, int len, FILE *stream)
{
	int n = len;
	int r = 0;

	while (n > 0) {
		r = fwrite(buffer, 1, n, stream);
		if (( r == 0) && (errno != EINTR)) return -1;
		buffer += r;
		n -= r;
	}
	return r;
}

int check_user_agent(char* user_agent){

	int fromapp = 0;

	if(user_agent != NULL){
		char *cp1=NULL, *app_router=NULL, *app_platform=NULL, *app_framework=NULL, *app_verison=NULL;
		cp1 = strdup(user_agent);

		vstrsep(cp1, "-", &app_router, &app_platform, &app_framework, &app_verison);

		if(app_router != NULL && app_framework != NULL && strcmp( app_router, "asusrouter") == 0){
				fromapp=FROM_ASUSROUTER;
			if(strcmp( app_framework, "DUTUtil") == 0)
				fromapp=FROM_DUTUtil;
			else if(strcmp( app_framework, "ASSIA") == 0)
				fromapp=FROM_ASSIA;
			else if(strcmp( app_framework, "IFTTT") == 0)
				fromapp=FROM_IFTTT;
			else if(strcmp( app_framework, "Alexa") == 0)
				fromapp=FROM_ALEXA;
			else
				fromapp=FROM_UNKNOWN;
		}
		if(cp1) free(cp1);
	}
	return fromapp;
}

#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
void add_ifttt_flag(void){

	memset(user_agent, 0, sizeof(user_agent));
	sprintf(user_agent, "%s",IFTTTUSERAGENT);
	return;
}
#endif

#if 0
void
do_file(char *path, FILE *stream)
{
	FILE *fp;
	char buf[1024];
	int nr;

	if (!(fp = fopen(path, "r")))
		return;
	while ((nr = fread(buf, 1, sizeof(buf), fp)) > 0) {
		web_write(buf, nr, stream);
	}
	fclose(fp);
}
#else
int do_fwrite(const char *buffer, int len, FILE *stream)
{
	int n = len;
	int r = 0;

	while (n > 0) {
		r = fwrite(buffer, 1, n, stream);
		if ((r == 0) && (errno != EINTR)) return -1;
		buffer += r;
		n -= r;
	}

	return r;
}

void do_file(char *path, FILE *stream)
{
	FILE *fp;
	char buf[1024];
	int nr;

	if ((fp = fopen(path, "r")) != NULL) {
		while ((nr = fread(buf, 1, sizeof(buf), fp)) > 0)
			do_fwrite(buf, nr, stream);
		fclose(fp);
	}
}

#endif

void set_referer_host(void)
{
	int port = 0;
	char *lan_ipaddr = nvram_safe_get("lan_ipaddr");
	const int d_len = strlen(DUT_DOMAIN_NAME);
	const int ip_len = strlen(lan_ipaddr);

	if(!strncmp(DUT_DOMAIN_NAME, host_name, d_len) && *(host_name + d_len) == ':' && (port = atoi(host_name + d_len + 1)) > 0 && port < 65536){	//transfer https domain to ip
		if(port == 80)
			strlcpy(referer_host, lan_ipaddr, sizeof(referer_host));
		else{
			memset(referer_host, 0, sizeof(referer_host));
			snprintf(referer_host,sizeof(referer_host),"%s:%d",lan_ipaddr, port);
		}
#ifdef RTAC68U
	} else if (is_dpsta_repeater() && nvram_get_int("re_mode") == 0
		&& !strncmp("repeater.asus.com", host_name, strlen("repeater.asus.com")) && *(host_name + strlen("repeater.asus.com")) == ':' && (port = atoi(host_name + strlen("repeater.asus.com") + 1)) > 0 && port < 65536){//transfer https domain to ip
		if(port == 80)
			strlcpy(referer_host, lan_ipaddr, sizeof(referer_host));
		else{
			memset(referer_host, 0, sizeof(referer_host));
			snprintf(referer_host,sizeof(referer_host),"%s:%d",lan_ipaddr, port);
		}
#endif
	}else if(!strcmp(DUT_DOMAIN_NAME, host_name))	//transfer http domain to ip
		strlcpy(referer_host, lan_ipaddr, sizeof(referer_host));
#ifdef RTAC68U
	else if (is_dpsta_repeater() && nvram_get_int("re_mode") == 0
		&& !strcmp("repeater.asus.com", host_name))   //transfer http domain to ip
		strlcpy(referer_host, lan_ipaddr, sizeof(referer_host));
#endif
	else if(!strncmp(lan_ipaddr, host_name, ip_len) && *(host_name + ip_len) == ':' && (port = atoi(host_name + ip_len + 1)) == 80)	//filter send hostip:80
		strlcpy(referer_host, lan_ipaddr, sizeof(referer_host));
	else if(nvram_match("x_Setting", "0"))
		strlcpy(referer_host, lan_ipaddr, sizeof(referer_host));
	else
		strlcpy(referer_host, host_name, sizeof(referer_host));
}

int is_firsttime(void);

time_t detect_timestamp, detect_timestamp_old, signal_timestamp;
char detect_timestampstr[32];


#define APPLYAPPSTR 	"applyapp.cgi"
#define GETAPPSTR 	"getapp"
#define APPGETCGI 	"appGet.cgi"

#ifdef RTCONFIG_ROG
#define APPLYROGSTR     "api.asp"
#endif


#ifdef RTCONFIG_LANTIQ
int wave_handle_flag(char *url)
{
	int ret = 0;

	if(strcmp(url, "qis/QIS_wireless.htm") == 0 ||
		strcmp(url, "QIS_wizard.htm") == 0){
		_dprintf("httpd_handle_request:[%s] from QIS\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_QIS);
	}else if(strcmp(url, "Advanced_Wireless_Content.asp") == 0){
		_dprintf("httpd_handle_request:[%s] from wireless\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_NORMAL);
	}else if(strcmp(url, "Advanced_WWPS_Content.asp") == 0){
		_dprintf("httpd_handle_request:[%s] for WPS\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_WPS);
	}else if(strcmp(url, "Advanced_WMode_Content.asp") == 0){
		_dprintf("httpd_handle_request:[%s] for WDS\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_WDS);
	}else if(strcmp(url, "Advanced_ACL_Content.asp") == 0){
		_dprintf("httpd_handle_request:[%s] for mac filter\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_ACL);
	}else if(strcmp(url, "Advanced_WAdvanced_Content.asp") == 0){
		_dprintf("httpd_handle_request:[%s] for advanced wireless\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_ADV);
	}else if(strcmp(url, "Guest_network.asp") == 0){
		_dprintf("httpd_handle_request:[%s] for guest network\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_VAP);
	}else if(strcmp(url, "device-map/router.asp") == 0){
		_dprintf("httpd_handle_request:[%s] for networkmap\n", url);
		nvram_set_int("wave_flag", WAVE_FLAG_NETWORKMAP);
	}
	return ret;
}
#endif

static void
handle_request(void)
{
	char line[10000], *cur;
	char *method, *path, *protocol, *authorization, *boundary, *alang, *cookies, *referer, *useragent;
	char *cp;
	char *file;
	int len;
	struct mime_handler *handler;
	struct except_mime_handler *exhandler;
	struct mime_referer *doreferer;
	int mime_exception, do_referer, login_state = -1;
	int fromapp=0;
	int cl = 0, flags;
	int auth_result = 1;
	int referer_result = 1;
#ifdef RTCONFIG_FINDASUS
	int i, isDeviceDiscovery=0;
	char id_local[32],prouduct_id[32];
#endif
	char inviteCode[512];

	/* Initialize the request variables. */
	authorization = boundary = cookies = referer = useragent = NULL;
	host_name[0] = 0;
	bzero( line, sizeof line );

	/* Parse the first line of the request. */
	if ( fgets( line, sizeof(line), conn_fp ) == (char*) 0 ) {
		send_error( 400, "Bad Request", (char*) 0, "No request found." );
		return;
	}

	method = path = line;
	strsep(&path, " ");
	//while (*path == ' ') path++;
	while (path && *path == ' ') path++;	// oleg patch
	protocol = path;
	strsep(&protocol, " ");
	//while (*protocol == ' ') protocol++;
	while (protocol && *protocol == ' ') protocol++;    // oleg pat
	cp = protocol;
	strsep(&cp, " ");
	if ( !method || !path || !protocol ) {
		send_error( 400, "Bad Request", (char*) 0, "Can't parse request." );
		return;
	}
	cur = protocol + strlen(protocol) + 1;

#ifdef TRANSLATE_ON_FLY
	memset(Accept_Language, 0, sizeof(Accept_Language));
#endif

	/* Parse the rest of the request headers. */
	while ( fgets( cur, line + sizeof(line) - cur, conn_fp ) != (char*) 0 )
	{
		//_dprintf("handle_request:cur = %s\n",cur);
		if ( strcmp( cur, "\n" ) == 0 || strcmp( cur, "\r\n" ) == 0 ) {
			break;
		}
#ifdef TRANSLATE_ON_FLY
		else if ( strncasecmp( cur, "Accept-Language:", 16) == 0 ) {
			if(change_preferred_lang(0)){
				char *p;
				struct language_table *pLang;
				char lang_buf[256];
				memset(lang_buf, 0, sizeof(lang_buf));
				alang = &cur[16];
				strncpy(lang_buf, alang, sizeof(lang_buf)-1);
				p = lang_buf;
				while (p != NULL && (p - lang_buf) < sizeof(lang_buf))
				{
					p = strtok (p, "\r\n ,;");
					if (p == NULL)  break;
					//2008.11 magic{
					int i, len=strlen(p);

					for (i=0;i<len;++i)
						if (isupper(p[i])) {
							p[i]=tolower(p[i]);
						}

					//2008.11 magic}
					for (pLang = language_tables; pLang->Lang != NULL; ++pLang)
					{
						if (strcasecmp(p, pLang->Lang)==0)
						{
							char dictname[32];
							_dprintf("handle_request: pLang->Lang = %s\n", pLang->Lang);
							if (!check_lang_support(pLang->Target_Lang))
								break;

							snprintf(dictname,sizeof(dictname),"%s.dict", pLang->Target_Lang);
							if(!check_if_file_exist(dictname))
							{
								break;
							}

							snprintf(Accept_Language,sizeof(Accept_Language),"%s",pLang->Target_Lang);
							break;
						}
					}

					if (Accept_Language[0] != 0) {
						break;
					}
					p+=strlen(p)+1;
				}

				if (Accept_Language[0] != 0) {
					nvram_set("preferred_lang", Accept_Language);
				}

				change_preferred_lang(1);
			}

			#ifdef RTCONFIG_DSL_TCLINUX
			if(is_firsttime()){
				if(nvram_match("preferred_lang", "CZ") || nvram_match("preferred_lang", "DE")) {
					int do_restart = 0;
					if( nvram_match("dslx_annex", "4")
						&& nvram_match("dsltmp_adslsyncsts", "down")
					){
						_dprintf("DSL: auto switch to annex b/j\n");
						nvram_set("dslx_annex", "6");
						do_restart = 1;
					}
					if(nvram_match("preferred_lang", "DE")
						&& nvram_match("dslx_vdsl_profile", "0")) {
						_dprintf("DSL: auto switch to 17a multi mode\n");
						nvram_set("dslx_vdsl_profile", "1");
						do_restart = 1;
					}
					if (do_restart)
						notify_rc("restart_dsl_setting");
				}
			}
			#endif
		}
#endif
		else if ( strncasecmp( cur, "Authorization:", 14 ) == 0 )
		{
			cp = &cur[14];
			cp += strspn( cp, " \t" );
			authorization = cp;
			cur = cp + strlen(cp) + 1;
		}
		else if ( strncasecmp( cur, "User-Agent:", 11 ) == 0 )
		{
			cp = &cur[11];
			cp += strspn( cp, " \t" );
			useragent = cp;
			cur = cp + strlen(cp) + 1;
			HTTPD_DBG("useragent: %s\n", useragent);
		}
		else if ( strncasecmp( cur, "Cookie:", 7 ) == 0 )
		{
			cp = &cur[7];
			cp += strspn( cp, " \t" );
			cookies = cp;
			cur = cp + strlen(cp) + 1;
		}
		else if ( strncasecmp( cur, "Referer:", 8 ) == 0 )
		{
			cp = &cur[8];
			cp += strspn( cp, " \t" );
			referer = cp;
			cur = cp + strlen(cp) + 1;
			//_dprintf("httpd referer = %s\n", referer);
		}
		else if ( strncasecmp( cur, "Host:", 5 ) == 0 )
		{
			cp = &cur[5];
			cp += strspn( cp, " \t" );
			sethost(cp);
			cur = cp + strlen(cp) + 1;
#ifdef RTCONFIG_FINDASUS
			sprintf(prouduct_id, "%s",get_productid());
			for(i = 0 ; i < strlen(prouduct_id) ; i++ ){
				prouduct_id[i] = tolower(prouduct_id[i]) ;
			}
			sprintf(id_local, "%s.local",prouduct_id);
			if(!strncmp(cp, "findasus", 8) || !strncmp(cp, id_local,strlen(id_local)))
				isDeviceDiscovery = 1;
			else
				isDeviceDiscovery = 0;
#endif
		}
		else if (strncasecmp( cur, "Content-Length:", 15 ) == 0) {
			cp = &cur[15];
			cp += strspn( cp, " \t" );
			cl = strtoul( cp, NULL, 0 );
		}
		else if ((cp = strstr( cur, "boundary=" ))) {
			boundary = &cp[9];
			for ( cp = cp + 9; *cp && *cp != '\r' && *cp != '\n'; cp++ );
			*cp = '\0';
			cur = ++cp;
		}
	}

	if ( strcasecmp( method, "get" ) != 0 && strcasecmp(method, "post") != 0 && strcasecmp(method, "head") != 0 ) {
		send_error( 501, "Not Implemented", (char*) 0, "That method is not implemented." );
		return;
	}

	if ( path[0] != '/' ) {
		send_error( 400, "Bad Request", (char*) 0, "Bad filename." );
		return;
	}
	file = &(path[1]);
	len = strlen( file );
	if ( file[0] == '/' || strcmp( file, ".." ) == 0 || strncmp( file, "../", 3 ) == 0 || strstr( file, "/../" ) != (char*) 0 || strcmp( &(file[len-3]), "/.." ) == 0 ) {
		send_error( 400, "Bad Request", (char*) 0, "Illegal filename." );
		return;
	}

//2008.08 magic{
	if (file[0] == '\0' || file[len-1] == '/'){
		if (is_firsttime()
#ifdef RTCONFIG_FINDASUS
		    && !isDeviceDiscovery
#endif
		   )
			//file = "QIS_wizard.htm";
			file = "QIS_default.cgi";
#ifdef RTCONFIG_FINDASUS
		else if(isDeviceDiscovery == 1)
			file = "find_device.asp";
#endif
		else
			file = INDEXPAGE;
	}

// 2007.11 James. {
	char *query;
	int file_len;

	memset(url, 0, sizeof(url));
	if ((query = index(file, '?')) != NULL) {
		file_len = strlen(file)-strlen(query);

		if(file_len > sizeof(url))
			file_len = sizeof(url);

		strncpy(url, file, file_len);
	}
	else
	{
		strncpy(url, file, sizeof(url)-1);
	}
// 2007.11 James. }

	if( (strstr(url, ".asp") || strstr(url, ".htm")) && !strstr(url, "update_networkmapd.asp") && !strstr(url, "update_clients.asp") && !strstr(url, "update_customList.asp") ) {
		memset(current_page_name, 0, sizeof(current_page_name));
		snprintf(current_page_name, sizeof(current_page_name), "%s", url);
	}

	if(strncmp(url, APPLYAPPSTR, strlen(APPLYAPPSTR))==0 
#ifdef RTCONFIG_ROG
		|| strncmp(url, APPLYROGSTR, strlen(APPLYROGSTR))==0
#endif
	)
		fromapp=1;
	else if(strncmp(url, GETAPPSTR, strlen(GETAPPSTR))==0)  {
		fromapp=1;
		strcpy(url, url+strlen(GETAPPSTR));
		file += strlen(GETAPPSTR);
	}

	memset(user_agent, 0, sizeof(user_agent));
	if(useragent != NULL)
		strncpy(user_agent, useragent, sizeof(user_agent)-1);
	else
		strcpy(user_agent, "");

	fromapp = check_user_agent(useragent);

#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
	ifttt_log(url, file);
#endif

#ifdef RTCONFIG_UIDEBUG
        char sysdepPath[128];
	snprintf(sysdepPath, sizeof(sysdepPath), "sysdep/%s/www/", nvram_safe_get("productid"));
	strcat(sysdepPath, url);
        if(check_if_file_exist(sysdepPath)){
// _dprintf("[httpd] ### GET ### sysdepPath: %s\n", sysdepPath);
		snprintf(sysdepPath, 128, "sysdep/%s/www/", nvram_safe_get("productid"));
		strcat(sysdepPath, file);
		file = sysdepPath;
// _dprintf("[httpd] file: %s\n", file);
        }
#endif
        HTTPD_DBG("file = %s\n", file);
	mime_exception = 0;
	do_referer = 0;

	if(!fromapp) {
		if(!cur_login_ip_type && (lock_flag & LOCK_LOGIN_LAN)){
			login_timestamp_tmp = uptime();
			login_dt = login_timestamp_tmp - last_login_timestamp;
			if(last_login_timestamp != 0 && login_dt > MAX_LOGIN_BLOCK_TIME){
				login_try = 0;
				last_login_timestamp = 0;
				lock_flag &= ~(LOCK_LOGIN_LAN);
				login_error_status = 0;
			}else{
				if((strncmp(file, "Main_Login.asp", 14)==0 && login_error_status == LOGINLOCK)|| strstr(url, ".png")){
				}else{
					send_login_page(fromapp, LOGINLOCK, url, NULL, login_dt, NOLOGINTRY);
					return;
				}
			}
		}
		else if(cur_login_ip_type && (lock_flag & LOCK_LOGIN_WAN)){
			login_timestamp_tmp_wan= uptime();
			login_dt = login_timestamp_tmp_wan - last_login_timestamp_wan;
			if(last_login_timestamp_wan!= 0 && login_dt > MAX_LOGIN_BLOCK_TIME){
				login_try_wan= 0;
				last_login_timestamp_wan= 0;
				lock_flag &= ~(LOCK_LOGIN_WAN);
				login_error_status = 0;
			}else{
				if((strncmp(file, "Main_Login.asp", 14)==0 && login_error_status == LOGINLOCK)|| strstr(url, ".png")){
				}else{
					send_login_page(fromapp, LOGINLOCK, url, NULL, login_dt, NOLOGINTRY);
					return;
				}
			}
		}
		http_login_timeout(login_ip_tmp, cookies, fromapp);	// 2008.07 James.
		login_state = http_login_check();
		// for each page, mime_exception is defined to do exception handler

		// check exception first
		for (exhandler = &except_mime_handlers[0]; exhandler->pattern; exhandler++) {
			if(match(exhandler->pattern, url))
			{
				mime_exception = exhandler->flag;
				break;
			}
		}

		// check doreferer first
		for (doreferer = &mime_referers[0]; doreferer->pattern; doreferer++) {
			if(match(doreferer->pattern, url))
			{
				do_referer = doreferer->flag;
				break;
			}
		}
	}
	x_Setting = nvram_get_int("x_Setting");

	for (handler = &mime_handlers[0]; handler->pattern; handler++) {
#ifdef RTCONFIG_HTTPS
		if (do_ssl && !strcmp(url, "offline.htm"))
			continue;
#endif
		if (match(handler->pattern, url))
		{
#ifdef RTCONFIG_LANTIQ
			wave_handle_flag(url);
#endif
			nvram_set("httpd_handle_request", url);
			nvram_set_int("httpd_handle_request_fromapp", fromapp);
			if(login_state==3 && !fromapp) { // few pages can be shown even someone else login
				if(!(mime_exception&MIME_EXCEPTION_MAINPAGE || (strncmp(file, "Main_Login.asp", 14)==0 && login_error_status == 9) || ((!handler->auth) && strncmp(file, "Main_Login.asp", 14) != 0))) {
					if(strcasecmp(method, "post") == 0 && handler->input)	//response post request
						while (cl--) (void)fgetc(conn_fp);

					send_login_page(fromapp, NOLOGIN, NULL, NULL, 0, NOLOGINTRY);
					return;
				}
			}
			if (handler->auth) {
				if ((mime_exception&MIME_EXCEPTION_NOAUTH_FIRST)&&!x_Setting) {
					//skip_auth=1;
				}
#ifdef RTCONFIG_WIFI_SON
				else if(!fromapp && !nvram_match("sw_mode", "1") && (nvram_match("sw_mode", "3") && !nvram_match("cfg_master", "1")) && strcmp(nvram_safe_get("hive_ui"), "") == 0){
					snprintf(inviteCode, sizeof(inviteCode), "<meta http-equiv=\"refresh\" content=\"0; url=message.htm\">\r\n");
					send_page( 200, "OK", (char*) 0, inviteCode, 0);
				}
#endif
#if defined(VZWAC1300)
				else if(!fromapp){
					snprintf(inviteCode, sizeof(inviteCode), "<script>top.location.href='/message.htm';</script>");
					send_page( 200, "OK", (char*) 0, inviteCode, 0);
				}
#endif
#ifdef RTCONFIG_AMAS
				//RD can do firmware upgrade, if re_upgrade set to 1.
				else if(!fromapp && nvram_match("re_mode", "1") && nvram_get_int("re_upgrade") == 0 && !check_AiMesh_whitelist(file)){
					snprintf(inviteCode, sizeof(inviteCode), "<meta http-equiv=\"refresh\" content=\"0; url=message.htm\">\r\n");
					send_page( 200, "OK", (char*) 0, inviteCode, 0);
					return;
				}
#endif
				else if((mime_exception&MIME_EXCEPTION_NOAUTH_ALL)) {
				}
				else {
					if(do_referer&CHECK_REFERER){
						referer_result = referer_check(referer, fromapp);
						HTTPD_DBG("referer_result = %d\n", referer_result);
						if(referer_result != 0){
							if(strcasecmp(method, "post") == 0 && handler->input)	//response post request
								while (cl--) (void)fgetc(conn_fp);

							send_login_page(fromapp, referer_result, NULL, NULL, 0, NOLOGINTRY);
							//if(!fromapp) http_logout(login_ip_tmp, cookies);
							return;
						}
					}
					handler->auth(auth_userid, auth_passwd, auth_realm);
					auth_result = auth_check(auth_realm, authorization, url, file, cookies, fromapp);
					HTTPD_DBG("auth_result = %d\n", auth_result);
					if (auth_result != 0)
					{
						if(strcasecmp(method, "post") == 0 && handler->input)	//response post request
							while (cl--) (void)fgetc(conn_fp);

						send_login_page(fromapp, auth_result, url, file, auth_check_dt, add_try);
						return;
					}
				}

				if(!fromapp) {
					if (	!strstr(url, "QIS_")
							&& !strstr(url, ".js")
							&& !strstr(url, ".css")
							&& !strstr(url, ".gif")
							&& !strstr(url, ".png")) {
#if defined(RTCONFIG_WIRELESSREPEATER) || defined(RTCONFIG_CONCURRENTREPEATER)					 
						if (nvram_match("x_Setting", "0") && (strstr(url, "start_apply2.htm") || strstr(url, "apscan.asp") || strstr(url, "data:image/")))
							;//TODO
						else
#endif
						http_login(login_ip_tmp, url);
					}
				}
			}else{
				if(do_referer&CHECK_REFERER){
					referer_result = check_noauth_referrer(referer, fromapp);
					if(referer_result != 0){
						if(strcasecmp(method, "post") == 0 && handler->input)	//response post request
							while (cl--) (void)fgetc(conn_fp);

						send_login_page(fromapp, referer_result, NULL, NULL, 0, NOLOGINTRY);
						//if(!fromapp) http_logout(login_ip_tmp, cookies);
						return;
					}
				}
			}

			if(!strcmp(file, "Logout.asp")){
				http_logout(login_ip_tmp, cookies, fromapp);
				send_login_page(fromapp, ISLOGOUT, NULL, NULL, 0, NOLOGINTRY);
				return;
			}
			if (strcasecmp(method, "post") == 0 && !handler->input) {
				send_error(501, "Not Implemented", NULL, "That method is not implemented.");
				return;
			}
// 2007.11 James. for QISxxx.htm pages }
			if (handler->input) {
				handler->input(file, conn_fp, cl, boundary);
#if defined(linux)
				if ((flags = fcntl(fileno(conn_fp), F_GETFL)) != -1 &&
						fcntl(fileno(conn_fp), F_SETFL, flags | O_NONBLOCK) != -1) {
					/* Read up to two more characters */
					if (fgetc(conn_fp) != EOF)
						(void)fgetc(conn_fp);

					fcntl(fileno(conn_fp), F_SETFL, flags);
				}
#elif defined(vxworks)
				flags = 1;
				if (ioctl(fileno(conn_fp), FIONBIO, (int) &flags) != -1) {
					/* Read up to two more characters */
					if (fgetc(conn_fp) != EOF)
						(void)fgetc(conn_fp);
					flags = 0;
					ioctl(fileno(conn_fp), FIONBIO, (int) &flags);
				}
#endif
			}
			if(!strstr(file, ".cgi") && !strstr(file, "syslog.txt") && !(strstr(file,"uploadIconFile.tar")) && !(strstr(file,"backup_jffs.tar")) && !(strstr(file,"networkmap.tar")) && !(strstr(file,".CFG")) && !(strstr(file,".log")) && !check_if_file_exist(file)
#ifdef RTCONFIG_USB_MODEM
					&& !strstr(file, "modemlog.txt")
#endif
#ifdef RTCONFIG_DSL_TCLINUX
					&& !strstr(file, "TCC.log")
#endif
#ifdef RTCONFIG_IPSEC
					&& !strstr(file, "ipsec.log")
#endif
#if defined(RTCONFIG_IFTTT) || defined(RTCONFIG_ALEXA)
					&& !strstr(file, "asustitle.png")
#endif
					&& !strstr(file,"cert_key.tar")
#ifdef RTCONFIG_OPENVPN
					&& !strstr(file, "server_ovpn.cert")
#endif
					){
				send_error( 404, "Not Found", (char*) 0, "File not found." );
				return;
			}
			if(nvram_match("x_Setting", "0") && (strcmp(url, "QIS_default.cgi")==0 || strcmp(url, "page_default.cgi")==0 || !strcmp(websGetVar(file, "x_Setting", ""), "1"))){
				if(!fromapp) set_referer_host();
				send_token_headers( 200, "Ok", handler->extra_header, handler->mime_type, fromapp);

			}else if(strncmp(url, "login.cgi", strlen(url))!=0){
				send_headers( 200, "Ok", handler->extra_header, handler->mime_type, fromapp);
			}
			if (strcasecmp(method, "head") != 0 && handler->output) {
				handler->output(file, conn_fp);
			}
			break;
		}
	}

	if (!handler->pattern){
		if(strlen(file) > 50 && !(strstr(file, "findasus")) && !(strstr(file, "acme-challenge"))){
			memset(cloud_file, 0, sizeof(cloud_file));
			if(!check_xss_blacklist(file, 0))
				strlcpy(cloud_file, file, sizeof(cloud_file));

			snprintf(inviteCode, sizeof(inviteCode), "<meta http-equiv=\"refresh\" content=\"0; url=cloud_sync.asp?flag=%s\">\r\n", cloud_file);
			send_page( 200, "OK", (char*) 0, inviteCode, 0);
		}
		else
			send_error( 404, "Not Found", (char*) 0, "File not found." );
	}
	nvram_unset("httpd_handle_request");
	nvram_unset("httpd_handle_request_fromapp");
}

//2008 magic{
void http_login_cache(usockaddr *u) {
	login_ip_tmp = (unsigned int)(u->sa_in.sin_addr.s_addr);
	cur_login_ip_type = check_current_ip_is_lan_or_wan();
	if(cur_login_ip_type == -1)
		_dprintf("[%s, %d]ERROR! Can not check the remote ip!\n", __FUNCTION__, __LINE__);
}

void http_login(unsigned int ip, char *url) {
	if(strncmp(url, "Main_Login.asp", strlen(url))==0)
		return;
	struct in_addr login_ip_addr;
	char *login_ip_str;
	char login_ipstr[32], login_timestampstr[32];

	if ((http_port != SERVER_PORT
	  && http_port != nvram_get_int("http_lanport")
#ifdef RTCONFIG_HTTPS
	  && http_port != SERVER_PORT_SSL
	  && http_port != nvram_get_int("https_lanport")
#endif
	    ) || ip == 0x100007f)
		return;

	login_ip = ip;
	last_login_ip = 0;

	login_ip_addr.s_addr = login_ip;
	login_ip_str = inet_ntoa(login_ip_addr);
	nvram_set("login_ip_str", login_ip_str);

	login_timestamp = uptime();

	memset(login_ipstr, 0, 32);
	sprintf(login_ipstr, "%u", login_ip);
	nvram_set("login_ip", login_ipstr);

	memset(login_timestampstr, 0, 32);
	sprintf(login_timestampstr, "%lu", login_timestamp);
	nvram_set("login_timestamp", login_timestampstr);
}

// 0: can not login, 1: can login, 2: loginer, 3: not loginer
int http_login_check(void)
{
	if ((http_port != SERVER_PORT
	  && http_port != nvram_get_int("http_lanport")
#ifdef RTCONFIG_HTTPS
	  && http_port != SERVER_PORT_SSL
	  && http_port != nvram_get_int("https_lanport")
#endif
	    ) || login_ip_tmp == 0x100007f)
		//return 1;
		return 0;	// 2008.01 James.

	if (login_ip == 0)
		return 1;
	else if (login_ip == login_ip_tmp)
		return 2;

	return 3;
}

void http_login_timeout(unsigned int ip, char *cookies, int fromapp_flag)
{
	time_t now, login_ts;

//	time(&now);
	now = uptime();
	login_ts = atol(nvram_safe_get("login_timestamp"));

// 2007.10 James. for really logout. {
	//if (login_ip!=ip && (unsigned long)(now-login_timestamp) > 60) //one minitues
	if ((login_ip != 0 && login_ip != ip) && ((unsigned long)(now-login_ts) > 60)) //one minitues
// 2007.10 James }
	{
		http_logout(login_ip, cookies, fromapp_flag);
	}
}

void http_logout(unsigned int ip, char *cookies, int fromapp_flag)
{
	if ((ip == login_ip || ip == 0 ) && fromapp_flag == 0) {
		last_login_ip = login_ip;
		login_ip = 0;
		login_timestamp = 0;

		nvram_set("login_ip", "");
		nvram_set("login_timestamp", "");
		memset(referer_host, 0, sizeof(referer_host));
		delete_logout_from_list(cookies);
// 2008.03 James. {
		if (change_passwd == 1) {
			change_passwd = 0;
			reget_passwd = 1;
		}
// 2008.03 James. }
	}else if(fromapp_flag != 0){
		delete_logout_from_list(cookies);
}
}
//2008 magic}
//

int is_auth(void)
{
	if (http_port == SERVER_PORT ||
	    http_port == nvram_get_int("http_lanport") ||
#ifdef RTCONFIG_HTTPS
	    http_port == SERVER_PORT_SSL ||
	    http_port == nvram_get_int("https_lanport") ||
#endif
	    strcmp(nvram_get_x("PrinterStatus", "usb_webhttpcheck_x"), "1") == 0)
		return 1;
	else
		return 0;
}

int is_firsttime(void)
{
#if defined(RTAC58U)
	if (!strncmp(nvram_safe_get("territory_code"), "CX", 2))
		return 0;
	else
#endif
	if (strcmp(nvram_get("x_Setting"), "1")==0)
		return 0;
	else
		return 1;
}

/* str_replace
* @param {char*} source
* @param {char*} find
* @param {char*} rep
* */
char *config_model_name(char *source, char *find,  char *rep){
   int find_L=strlen(find);
   int rep_L=strlen(rep);
   int length=strlen(source)+1;
   int gap=0;

   char *result_t = NULL;
   char *result = (char*)malloc(sizeof(char) * length);
   if(result == NULL)
   	return NULL;
   else
   	strcpy(result, source);

   char *former=source;
   char *location= strstr(former, find);

	/* stop searching when there is no finding string */
   while(location!=NULL){
       gap+=(location - former);
       result[gap]='\0';

       length+=(rep_L-find_L);
       result_t = (char*)realloc(result, length * sizeof(char));
       if(result_t == NULL){
       	free(result);
       	return NULL;
       }else
       	result = result_t;
       strcat(result, rep);
       gap+=rep_L;

       former=location+find_L;
       strcat(result, former);

       location= strstr(former, find);
   }
   return result;
}

#ifdef TRANSLATE_ON_FLY
/* Whether a language support should be enabled or not.
 * @lang:
 * @return:
 * 	0:	lang should not be supported.
 *     <0:	invalid parameter.
 *     >0:	lang can be supported.
 */

#ifdef RTCONFIG_AUTODICT
int
load_dictionary (char *lang, pkw_t pkw)
{
	char dfn[16];
	char dummy_buf[16];
	int dict_item_idx;
	char* tmp_ptr;
	int dict_item; // number of dict item, now get from text file
	char *q;
	FILE *dfp = NULL;
	int remain_dict;
	int dict_size = 0;
//	struct timeval tv1, tv2;
	const char *eng_dict = "EN.dict";
#ifndef RELOAD_DICT
	static char loaded_dict[12] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
#endif  // RELOAD_DICT
#ifdef RTCONFIG_DYN_DICT_NAME
	char *dyn_dict_buf;
	char *dyn_dict_buf_new=NULL;
#endif

//printf ("lang=%s\n", lang);

//	gettimeofday (&tv1, NULL);
	if (lang == NULL || (lang != NULL && strlen (lang) == 0)) {
		// if "lang" is invalid, use English as default
		snprintf (dfn, sizeof (dfn), eng_dict);
	} else {
		snprintf (dfn, sizeof (dfn), "%s.dict", lang);
	}

#ifndef RELOAD_DICT
//	printf ("loaded_dict (%s) v.s. dfn (%s)\n", loaded_dict, dfn);
	if (strcmp (dfn, loaded_dict) == 0) {
		return 1;
	}
	release_dictionary (pkw);
#endif  // RELOAD_DICT

	do {
//		 printf("Open (%s) dictionary file.\n", dfn);
//
// Now DICT files all use UTF-8, it is no longer a text file
// it need to use open as binary
//
		dfp = fopen (dfn, "rb");
		if (dfp != NULL)	{
#ifndef RELOAD_DICT
			snprintf (loaded_dict, sizeof (loaded_dict), "%s", dfn);
#endif  // RELOAD_DICT
			break;
		}

//		printf ("Open (%s) failure. errno %d (%s)\n", dfn, errno, strerror (errno));
		if (dfp == NULL && strcmp (dfn, eng_dict) == 0) {
			return 0;
		} else {
			// If we can't open specified language file, use English as default
			snprintf (dfn, sizeof (dfn), eng_dict);
		}
	} while (1);

	memset (pkw, 0, sizeof (kw_t));
	fseek (dfp, 0L, SEEK_END);
	dict_size = ftell (dfp) + 128;
	// skip BOM header length
	dict_size -= 3;
	printf ("dict_size %d\n", dict_size);

#ifdef RTCONFIG_DYN_DICT_NAME
	dyn_dict_buf = (char *) malloc(dict_size);
	fseek (dfp, 0L, SEEK_SET);
	// skip BOM
	fread (dummy_buf, 1, 3, dfp);
	// read to dict string buffer
	memset(dyn_dict_buf, 0, dict_size);
	fread (dyn_dict_buf, 1, dict_size, dfp);
	dyn_dict_buf_new = config_model_name(dyn_dict_buf, "ZVDYNMODELVZ", nvram_safe_get("productid"));

	free(dyn_dict_buf);

	if(dyn_dict_buf_new){
		dict_size = sizeof(char) * strlen(dyn_dict_buf_new);
		pkw->buf = (unsigned char *) (q = malloc (dict_size));
		strcpy(pkw->buf, dyn_dict_buf_new);
		free(dyn_dict_buf_new);
	}

#else
	pkw->buf = (char *) (q = malloc (dict_size));

	fseek (dfp, 0L, SEEK_SET);
	// skip BOM
	fread (dummy_buf, 1, 3, dfp);
	// read to dict string buffer
	memset(pkw->buf, 0, dict_size);
	fread (pkw->buf, 1, dict_size, dfp);
#endif
	// calc how many dict item , dict_item
	remain_dict = dict_size;
	tmp_ptr = (char *) pkw->buf;
	dict_item = 0;
	while (remain_dict>0) {
		if (*tmp_ptr == 0x0a) {
			dict_item++;
			tmp_ptr++;
			remain_dict--;
		}
		else if (*tmp_ptr == 0) {
			break;
		}
		else {
			tmp_ptr++;
			remain_dict--;
		}
	}
	// allocate memory according dict_item
	pkw->idx = malloc (dict_item * sizeof(unsigned char*));

	printf ("dict_item %d\n", dict_item);

	// get all string start and put to pkw->idx
	remain_dict = dict_size;
	for (dict_item_idx = 0; dict_item_idx < dict_item; dict_item_idx++) {
		pkw->idx[dict_item_idx] = (char *) q;
		while (remain_dict>0) {
			if (*q == 0x0a) {
				*q=0;
				q++;
				remain_dict--;
				break;
			}
			if (*q == 0) {
				break;
			}
			q++;
			remain_dict--;
		}
	}

	pkw->len = dict_item;

	fclose (dfp);

	return 1;
}


void
release_dictionary (pkw_t pkw)
{
	if (pkw == NULL)	{
		return;
	}

	//pkw->len = pkw->tlen = 0;
	pkw->len = 0;

	if (pkw->idx != NULL)   {
		free (pkw->idx);
		pkw->idx = NULL;
	}

	if (pkw->buf != NULL)   {
		free (pkw->buf);
		pkw->buf = NULL;
	}
}

char*
search_desc (pkw_t pkw, char *name)
{
	int i;
	char *ret = NULL;
	int dict_idx;
	char name_buf[128];

/*
	printf("search_desc:");
	printf(name);
	printf("\n");
*/

	if (pkw == NULL || (pkw != NULL && pkw->len <= 0)) {
		return NULL;
	}

	// remove equal
	memset(name_buf,0,sizeof(name_buf));
	// minus one for reserver one char for string zero char
	for (i = 0; i<sizeof(name_buf)-1; i++)  {
		if (*name == 0 || *name == '=') {
			break;
		}
		name_buf[i]=*name++;
	}

/*
	for (i = 0; i < pkw->len; ++i)  {
		char *p;
		p = pkw->idx[i];
		if (strncmp (name, p, strlen (name)) == 0) {
			ret = p + strlen (name);
			break;
		}
	}
*/

/*
	printf("search_desc:");
	printf(name_buf);
	printf("\n");
*/

	dict_idx = atoi(name_buf);
//	printf("%d , %d\n",dict_idx,pkw->len);
	if (dict_idx < pkw->len) {
		ret = (char *) pkw->idx[dict_idx];
	}
	else {
		ret = (char *) pkw->idx[0];
	}

/*
	printf("ret:");
	printf(ret);
	printf("\n");
*/

	return ret;
}
#else
int
load_dictionary (char *lang, pkw_t pkw)
{
	char dfn[16];
	char *p, *q;
	FILE *dfp = NULL;
	int dict_size = 0;
//	struct timeval tv1, tv2;
	const char *eng_dict = "EN.dict";
#ifndef RELOAD_DICT
	static char loaded_dict[12] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
#endif  // RELOAD_DICT

//	gettimeofday (&tv1, NULL);
	if (lang == NULL || (lang != NULL && strlen (lang) == 0)) {
		// if "lang" is invalid, use English as default
		snprintf (dfn, sizeof (dfn), eng_dict);
	} else {
		snprintf (dfn, sizeof (dfn), "%s.dict", lang);
	}

#ifndef RELOAD_DICT
//	printf ("loaded_dict (%s) v.s. dfn (%s)\n", loaded_dict, dfn);
	if (strcmp (dfn, loaded_dict) == 0) {
		return 1;
	}
	release_dictionary (pkw);
#endif  // RELOAD_DICT

	do {
//		 printf("Open (%s) dictionary file.\n", dfn);
		dfp = fopen (dfn, "r");
		if (dfp != NULL)	{
#ifndef RELOAD_DICT
			snprintf (loaded_dict, sizeof (loaded_dict), "%s", dfn);
#endif  // RELOAD_DICT
			break;
		}

//		printf ("Open (%s) failure. errno %d (%s)\n", dfn, errno, strerror (errno));
		if (dfp == NULL && strcmp (dfn, eng_dict) == 0) {
			return 0;
		} else {
			// If we can't open specified language file, use English as default
			snprintf (dfn, sizeof (dfn), eng_dict);
		}
	} while (1);

	memset (pkw, 0, sizeof (kw_t));
	fseek (dfp, 0L, SEEK_END);
	dict_size = ftell (dfp) + 128;
//	printf ("dict_size %d\n", dict_size);
	REALLOC_VECTOR (pkw->idx, pkw->len, pkw->tlen, sizeof (unsigned char*));
	pkw->buf = q = malloc (dict_size);

	fseek (dfp, 0L, SEEK_SET);
#if 0
	while (!feof (dfp)) {
		// if pkw->idx is not enough, add 32 item to pkw->idx
		REALLOC_VECTOR (pkw->idx, pkw->len, pkw->tlen, sizeof (unsigned char*));

		fscanf (dfp, "%[^\n]%*c", q);
		if ((p = strchr (q, '=')) != NULL) {
			pkw->idx[pkw->len] = q;
			pkw->len++;
			q = p + strlen (p);
			*q = '\0';
			q++;
		}
	}
#else
	while ((fscanf(dfp, "%[^\n]", q)) != EOF) {
		fgetc(dfp);

		// if pkw->idx is not enough, add 32 item to pkw->idx
		REALLOC_VECTOR (pkw->idx, pkw->len, pkw->tlen, sizeof (unsigned char*));

		if ((p = strchr (q, '=')) != NULL) {
			pkw->idx[pkw->len] = q;
			pkw->len++;
			q = p + strlen (p);
			*q = '\0';
			q++;
		}
	}

#endif

	fclose (dfp);
//	gettimeofday (&tv2, NULL);
//	printf("Load %d keywords spent %ldms\n", pkw->len, ((tv2.tv_sec * 1000000 + tv2.tv_usec) - (tv1.tv_sec * 1000000 + tv1.tv_usec)) / 1000);

	return 1;
}


void
release_dictionary (pkw_t pkw)
{
	if (pkw == NULL)	{
		return;
	}

	pkw->len = pkw->tlen = 0;

	if (pkw->idx != NULL)   {
		free (pkw->idx);
		pkw->idx = NULL;
	}

	if (pkw->buf != NULL)   {
		free (pkw->buf);
		pkw->buf = NULL;
	}
}

char*
search_desc (pkw_t pkw, char *name)
{
	int i;
	char *p, *ret = NULL;

	if (pkw == NULL || (pkw != NULL && pkw->len <= 0)) {
		return NULL;
	}
	for (i = 0; i < pkw->len; ++i)  {
		p = pkw->idx[i];
		if (strncmp (name, p, strlen (name)) == 0) {
			ret = p + strlen (name);
			break;
		}
	}

	return ret;
}
#endif
#endif //TRANSLATE_ON_FLY

int main(int argc, char **argv)
{
	usockaddr usa;
	int listen_fd[2];
	socklen_t sz;
	char pidfile[32];
	fd_set active_rfds;
	conn_list_t pool;
	int i, c;
	//int do_ssl = 0;

	do_ssl = 0; // default
	char log_filename[128] = {0};

#if defined(RTCONFIG_UIDEBUG)
	eval("touch", HTTPD_DEBUG);
#endif

#if defined(RTCONFIG_SW_HW_AUTH)
	//if(!httpd_sw_hw_check()) return 0;
#endif
	// usage : httpd -s -p [port]
	while ((c = getopt(argc, argv, "sp:i:w:")) != -1) {
		switch (c) {
		case 's':
#ifdef RTCONFIG_HTTPS
			do_ssl = 1;
#endif
			break;
		case 'p':
			http_port = atoi(optarg);
			break;
		case 'i':
			http_ifname = optarg;
			break;
		case 'w':
			//Generate Wi-Fi log
			snprintf(log_filename, sizeof(log_filename), "%s", optarg);
			FILE *fp = fopen(log_filename, "w");
			ej_wl_status_2g(0, fp, 0, NULL);
			fclose(fp);
			return 0;
		default:
			fprintf(stderr, "ERROR: unknown option %c\n", c);
			break;
		}
	}

	/* -p might be specified before -s */
	if (http_port == 0) {
#ifdef RTCONFIG_HTTPS
		if (do_ssl) {
			http_port = SERVER_PORT_SSL;
		} else
#endif
		{
			http_port = SERVER_PORT;
		}
	}

	//websSetVer();
	//2008.08 magic
	nvram_unset("login_timestamp");
	nvram_unset("login_ip");
	nvram_unset("login_ip_str");
	MAX_login = nvram_get_int("login_max_num");
	if(MAX_login <= DEFAULT_LOGIN_MAX_NUM)
		MAX_login = DEFAULT_LOGIN_MAX_NUM;

	detect_timestamp_old = 0;
	detect_timestamp = 0;
	signal_timestamp = 0;

	/* Ignore broken pipes */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, chld_reap);

#ifdef RTCONFIG_HTTPS
	//if (do_ssl)
		start_ssl();
#endif

	/* Initialize listen socket */
	for (i = 0; i < ARRAY_SIZE(listen_fd); i++)
		listen_fd[i] = -1;
#ifdef RTCONFIG_AIHOME_TUNNEL
	if (nvram_get_int("http_enable") == 1 && http_port == SERVER_PORT){
		//httpd listen lo 80 port for tunnel but unused ifname in https only
	}else
#endif
	if ((listen_fd[0] = initialize_listen_socket(&usa, http_ifname)) < 2){
		fprintf(stderr, "can't bind to %s address\n", http_ifname ? : "any");
		exit(errno);
	}
	if ((http_ifname && strcmp(http_ifname, "lo") != 0) &&
	    (listen_fd[1] = initialize_listen_socket(&usa, "lo")) < 2) {
		fprintf(stderr, "can't bind to %s address\n", "loopback");
		/* allow fail if previous bind to interface was ok */
		/* exit(errno); */
	}

	FILE *pid_fp;
	if (http_port==SERVER_PORT)
		strcpy(pidfile, "/var/run/httpd.pid");
	else sprintf(pidfile, "/var/run/httpd-%d.pid", http_port);

	if (!(pid_fp = fopen(pidfile, "w"))) {
		perror(pidfile);
		return errno;
	}
	fprintf(pid_fp, "%d", getpid());
	fclose(pid_fp);

	/* Init connection pool */
	FD_ZERO(&active_rfds);
	TAILQ_INIT(&pool.head);
	pool.count = 0;

	/* Loop forever handling requests */
	for (;;) {
		int max_fd, count;
		struct timeval tv;
		fd_set rfds;
		conn_item_t *item, *next;

		memcpy(&rfds, &active_rfds, sizeof(rfds));
		max_fd = -1;
		if (pool.count < MAX_CONN_ACCEPT) {
			for (i = 0; i < ARRAY_SIZE(listen_fd); i++) {
				if (listen_fd[i] < 0)
					continue;
				FD_SET(listen_fd[i], &rfds);
				max_fd = max(listen_fd[i], max_fd);
			}
		}
		TAILQ_FOREACH(item, &pool.head, entry)
			max_fd = max(item->fd, max_fd);

		/* Wait for new connection or incoming request */
		tv.tv_sec = MAX_CONN_TIMEOUT;
		tv.tv_usec = 0;
		while ((count = select(max_fd + 1, &rfds, NULL, NULL, &tv)) < 0 && errno == EINTR)
			continue;
		if (count < 0) {
			perror("select");
			return errno;
		}

		/* Check and accept new connection */
		item = NULL;
		for (i = 0; count && i < ARRAY_SIZE(listen_fd); i++) {
			if (listen_fd[i] < 0 || !FD_ISSET(listen_fd[i], &rfds))
				continue;

			item = malloc(sizeof(*item));
			if (item == NULL) {
				perror("malloc");
				return errno;
			}
			sz = sizeof(item->usa);
			while ((item->fd = accept(listen_fd[i], &item->usa.sa, &sz)) < 0 && errno == EINTR)
				continue;
			if (item->fd < 0) {
				perror("accept");
				free(item);
				continue;
			}

			/* Set the KEEPALIVE option to cull dead connections */
			setsockopt(item->fd, SOL_SOCKET, SO_KEEPALIVE, &int_1, sizeof(int_1));

			/* Add to active connections */
			FD_SET(item->fd, &active_rfds);
			TAILQ_INSERT_TAIL(&pool.head, item, entry);
			pool.count++;
		}
		/* Continue waiting over again */
		if (count && item)
			continue;

		/* Check and process pending or expired requests */
		TAILQ_FOREACH_SAFE(item, &pool.head, entry, next) {
			if (count && !FD_ISSET(item->fd, &rfds))
				continue;

			/* Delete from active connections */
			FD_CLR(item->fd, &active_rfds);
			TAILQ_REMOVE(&pool.head, item, entry);
			pool.count--;

			/* Process request if any */
			if (count) {
#ifdef RTCONFIG_HTTPS
				if (do_ssl) {
					ssl_stream_fd = item->fd;
					if (!(conn_fp = ssl_server_fopen(item->fd))) {
						perror("fdopen(ssl)");
						goto skip;
					}
				} else
#endif
				if (!(conn_fp = fdopen(item->fd, "r+"))) {
					perror("fdopen");
					goto skip;
				}

				http_login_cache(&item->usa);
#if defined(RTCONFIG_UIDEBUG)
				struct in_addr req_ip;
				req_ip.s_addr = login_ip_tmp;
				HTTPD_DBG("Log ip address: %s\n", inet_ntoa(req_ip));
#endif
				handle_request();
				fflush(conn_fp);
#ifdef RTCONFIG_HTTPS
				if (!do_ssl)
#endif
				shutdown(item->fd, 2), item->fd = -1;
				fclose(conn_fp);

			skip:
				/* Skip the rest of */
				if (--count == 0)
					next = NULL;

			}

			/* Close timed out and/or still alive */
			if (item->fd >= 0) {
				shutdown(item->fd, 2);
				close(item->fd);
			}

			free(item);
		}
	}

	for (i = 0; i < ARRAY_SIZE(listen_fd); i++) {
		if (listen_fd[i] < 0)
			continue;
		shutdown(listen_fd[i], 2);
		close(listen_fd[i]);
	}

	return 0;
}

#ifdef RTCONFIG_HTTPS
void save_cert(void)
{
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	eval("cp", "-p", "/etc/cert.pem", "/etc/key.pem", "/jffs/.cert/");
	chmod("/jffs/.cert/key.pem", S_IRUSR|S_IWUSR);
#else
	if (eval("tar", "-C", "/", "-czf", "/tmp/cert.tgz", "etc/cert.pem", "etc/key.pem") == 0) {
		if (nvram_set_file("https_crt_file", "/tmp/cert.tgz", 8192)) {
			nvram_commit_x();
		}
	}
	unlink("/tmp/cert.tgz");
#endif

}

void erase_cert(void)
{
	unlink("/etc/cert.pem");
	unlink("/etc/key.pem");
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	unlink(UPLOAD_CERT);
	unlink(UPLOAD_KEY);
#else
	nvram_unset("https_crt_file");
#endif
	//nvram_unset("https_crt_gen");
	nvram_set("https_crt_gen", "0");
	nvram_commit();
}

void start_ssl(void)
{
	int lockfd;
	int retry;
	int i;
	unsigned long long sn;
	char t[32];

	lockfd = open("/var/lock/sslinit.lock", O_CREAT | O_RDWR, 0666);

	// Avoid collisions if another httpd instance is initializing SSL cert
	for ( i = 1; i < 5; i++ ) {
		if (flock(lockfd, LOCK_EX | LOCK_NB) < 0) {
			//logmessage("httpd", "Conflict, waiting %d", i);
			sleep(i*i);
		} else {
			i = 5;
		}
	}

	if (nvram_match("https_crt_gen", "1")) {
		erase_cert();
	}

	retry = 1;
	while (1) {
		if ((!f_exists("/etc/cert.pem")) || (!f_exists("/etc/key.pem"))) {
			erase_cert();
			logmessage("httpd", "Generating SSL certificate...");

			// browsers seems to like this when the ip address moves...     -- zzz
			f_read("/dev/urandom", &sn, sizeof(sn));

			sprintf(t, "%llu", sn & 0x7FFFFFFFFFFFFFFFULL);
			eval("gencert.sh", t);

#ifdef RTCONFIG_LETSENCRYPT
			if (nvram_match("le_enable", "2"))
#endif
			{
				save_cert();
			}
		}

		if (mssl_init("/etc/cert.pem", "/etc/key.pem")) {
			flock(lockfd, LOCK_UN);
			return;
		}

		logmessage("httpd", "Failed to initialize SSL, generating new key/cert.");
		erase_cert();

		if (!retry) {
			flock(lockfd, LOCK_UN);
			logmessage("httpd", "Unable to start in SSL mode, exiting!");
			exit(1);
		}
		retry = 0;
	}
}
#endif

//return value: 0: LAN,  1: WAN,  -1: ERROR
int _check_ip_is_lan_or_wan(const char *target_ip, const char *lan_ip, const char *submask)
{
	char tmp1[20], tmp2[20];

	if(!target_ip || !lan_ip || !submask)
		return -1;

	//Convert target ip and lan ip.
	//ex. target ip is 168.95.10.10, lan ip is 192.168.1.1, subnet mask is 255.255.255.0. 
	//convert them as 168.95.10.0 and 192.168.1.0. Them compare these 2 values.
	//If they are different, the target ip would be WAN.
	if(get_network_addr_by_ip_prefix(target_ip, submask, tmp1, sizeof(tmp1)) == -1)
		return -1;

	if(get_network_addr_by_ip_prefix(lan_ip, submask, tmp2, sizeof(tmp2)) == -1)
		return -1;

	return !strcmp(tmp1, tmp2)? 0: 1;
}

//return value: 0: LAN,  1: WAN,  -1: ERROR
int check_current_ip_is_lan_or_wan()
{
	char *target_ip;
	struct in_addr temp_ip_addr;

	if(!login_ip_tmp)
		return -1;

	temp_ip_addr.s_addr = login_ip_tmp;
	target_ip = inet_ntoa(temp_ip_addr);

	return _check_ip_is_lan_or_wan(target_ip, nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));
}

