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
 * Copyright 2019 Eric Sauvageau.
 *
 */

#include <string.h>
#include <rc.h>
#include <bcmnvram.h>
#include <shutils.h>

#define NTPD_PIDFILE "/var/run/ntpd.pid"

int start_ntpd(void)
{
	char *ntpd_argv[] = { "/usr/sbin/ntp",
		"-t",
		"-S", "/sbin/ntpd_synced",
		"-p", "pool.ntp.org",
		NULL, NULL,		/* -p second_server */
		NULL, NULL, NULL,	/* -l, -I, ifname */
		NULL };
	int ret, index = 6;
	pid_t pid;

	if (getpid() != 1) {
		notify_rc("start_ntpd");
		return 0;
	}

	stop_ntpd();

	if (!nvram_match("ntp_server0", ""))
		ntpd_argv[index - 1] = nvram_safe_get("ntp_server0");

	if (!nvram_match("ntp_server1", "")) {
		ntpd_argv[index++] = "-p";
		ntpd_argv[index++] = nvram_safe_get("ntp_server1");
	}

	if (nvram_get_int("ntpd_enable")) {
		ntpd_argv[index++] = "-l";
		ntpd_argv[index++] = "-I";
		ntpd_argv[index++] = nvram_safe_get("lan_ifname");
	}

	ret = _eval(ntpd_argv, NULL, 0, &pid);
	if (ret == 0)
		logmessage("ntpd", "Started ntpd");

	return ret;
}

void stop_ntpd(void)
{
	if (getpid() != 1) {
		notify_rc("stop_ntpd");
		return;
	}

	if (pids("ntp")) {
		killall_tk("ntp");
		logmessage("ntpd", "Stopped ntpd");
	}
}

int ntpd_synced_main(int argc, char *argv[])
{
#if 0
	if (argc == 2 && !strcmp(argv[1], "unsync"))
		logmessage("ntpd", "Unable to reach ntp server so far, keep trying");
#endif

	if (!nvram_match("ntp_ready", "1") && (argc == 2 && !strcmp(argv[1], "step"))) {
		nvram_set("ntp_ready", "1");
		logmessage("ntpd", "Initial clock set");
/* Code from ntpclient */
#if !defined(RPAC56) && !defined(MAPAC1300) && !defined(MAPAC2200) && !defined(VZWAC1300)
		if(nvram_contains_word("rc_support", "defpsk"))
			nvram_set("x_Setting", "1");
#endif
#ifdef RTCONFIG_CFGSYNC
		if (pidof("cfg_server") >= 0)
			kill_pidfile_s("/var/run/cfg_server.pid", SIGUSR1);
		if (pidof("cfg_client") >= 0)
			kill_pidfile_s("/var/run/cfg_client.pid", SIGUSR1);
#endif

/* Code from ntp */
		nvram_set("reload_svc_radio", "1");
		nvram_set("svc_ready", "1");

		setup_timezone();

#ifdef RTCONFIG_DNSPRIVACY
		if (nvram_get_int("dnspriv_enable"))
			notify_rc("restart_stubby");
#endif
#ifdef RTCONFIG_DNSSEC
		if (nvram_get_int("dnssec_enable"))
			kill_pidfile_s("/var/run/dnsmasq.pid", SIGINT);
#endif
#ifdef RTCONFIG_DISK_MONITOR
		notify_rc("restart_diskmon");
#endif

		stop_ddns();
		start_ddns();
#ifdef RTCONFIG_OPENVPN
		start_ovpn_eas();
#endif

	}

	return 0;
}
