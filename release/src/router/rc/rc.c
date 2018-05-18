/*

	Tomato Firmware
	Copyright (C) 2006-2009 Jonathan Zarate

*/
#include "rc.h"
#include "interface.h"
#include <sys/time.h>
#ifdef HND_ROUTER
#include <limits.h>
#include <time.h>
#endif

#ifdef RTCONFIG_RALINK
#include <ralink.h>
#endif

#ifdef RTCONFIG_QCA
#include <qca.h>
#endif

#if defined(RTCONFIG_LP5523)
#include <lp5523led.h>
#endif

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof(a[0]))
#endif /* ARRAYSIZE */

#ifdef  __CONFIG_WBD__
static void
wbd_restore_defaults(void)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXXXXXXXXXXXXXXXX_mssid_";
	int i, j, iter_param;

	/* List of WBD NVRAMs */
	char* wbd_nvrams[] = {
		/* NVRAMs general to WBD application */
		"wbd_ifnames",
		"wbd_mode",
		"wbd_msglevel",
		"wbd_ignr_maclst",
		"wbd_fixed_ifnames",
		"wbd_no_dedicated_backhaul",
		/* NVRAMs for Target BSS Identification Configuration */
		"wbd_tbss_wght_idx",
		"wbd_tbss_wght",
		"wbd_tbss_thld_idx",
		"wbd_tbss_thld",
		"wbd_tbss_algo",
		"wbd_adv_thld",
		"wbd_tbss_stacnt_thld",
		/* NVRAMs for Weak Client Identification Configuration */
		"wbd_wc_algo",
		"wbd_wc_thld_idx",
		"wbd_wc_thld",
		"wbd_weak_sta_cfg",
		"wbd_weak_sta_policy",
		/* NVRAMs for Timeouts */
		"wbd_tm_join",
		"wbd_tm_keepalive",
		"wbd_tm_wd_probe",
		"wbd_tm_slv_inactv",
		"wbd_tm_subscribe",
		"wbd_tm_sta_reports",
		"wbd_tm_blk_sta",
		"wbd_tm_wd_weakclient",
		"wbd_tm_evt_grace",
		"wbd_tm_wd_tbss",
		"wbd_tm_config",
		"wbd_tm_actframe",
		"wbd_bounce_detect",
		"wbd_tm_dfs_list",
		"bcm_stamon_get_interval"
	};

	/* Traverse through all WBD NVRAMs */
	for (iter_param = 0; iter_param < ARRAYSIZE(wbd_nvrams); iter_param++) {

		/* Clear NVRAMs without Prefix */
		nvram_unset(wbd_nvrams[iter_param]);

		/* Traverse through all Primary Prefix */
		for (i = 0; i < MAX_NVPARSE; i++) {
			sprintf(prefix, "wl%d_", i);

			/* Clear NVRAMs with Primary Prefix */
			nvram_unset(strcat_r(prefix, wbd_nvrams[iter_param], tmp));

			/* Traverse through all Virtual Prefix */
			for (j = 0; j < MAX_NVPARSE; j++) {
				sprintf(prefix, "wl%d.%d_", i, j);

				/* Clear NVRAMs with Virtual Prefix */
				nvram_unset(strcat_r(prefix, wbd_nvrams[iter_param], tmp));
			}
		}
	}
}
#endif /* __CONFIG_WBD__ */

#ifdef WLHOSTFBT

/* Clear FBT_APs NVRAMS based on prefix */
void
fbt_aps_restore_defaults(char *prefix)
{
	char tmp_prefix[] = "wlXXXXXXXXXXXXXXXXXXXXXXXXXXXX_mssid_";
	char *fbt_aps, *next;
	char tmp[100], fbt_ap[100], tmp_fbt_ap[100];
	int iter_param, i, j;

	/* List of FBT_AP NVRAMs */
	char* fbt_ap_nvrams[] = {
		"addr",
		"r1kh_id",
		"r0kh_id",
		"r0kh_id_len",
		"br_addr",
		"r0kh_key",
		"r1kh_key",
	};

	/* Get fbt_all_gen_aps NVRAM */
	fbt_aps = nvram_safe_get(strcat_r(prefix, "fbt_all_gen_aps", tmp));
	/* If no values, no need to restore those */
	if (strlen(fbt_aps) <= 0) {
		goto fbt_all_aps;
	}
	/* For each fbt_all_gen_aps, clear the fbt_ap_nvrams */
	foreach(fbt_ap, fbt_aps, next) {
		/* Traverse through all FBT_AP NVRAMs */
		for (iter_param = 0; iter_param < ARRAYSIZE(fbt_ap_nvrams); iter_param++) {
			snprintf(tmp_fbt_ap, sizeof(tmp_fbt_ap), "%s_%s", fbt_ap,
				fbt_ap_nvrams[iter_param]);
			nvram_unset(tmp_fbt_ap);
		}
	}

fbt_all_aps:
	/* Get fbt_all_aps NVRAM to clear the fbt_bssid NVRAM*/
	fbt_aps = nvram_safe_get(strcat_r(prefix, "fbt_all_aps", tmp));
	/* If no values, no need to restore those */
	if (strlen(fbt_aps) <= 0) {
		return;
	}

	/* For each fbt_aps, clear the fbt_bssid nvrams */
	foreach(fbt_ap, fbt_aps, next) {
		/* Traverse through all Primary Prefix to restore fbt_aps NVRAMs */
		for (i = 0; i < MAX_NVPARSE; i++) {
			sprintf(tmp_prefix, "wl%d_", i);
			snprintf(tmp_fbt_ap, sizeof(tmp_fbt_ap), "%s%s_fbt_bssid",
				tmp_prefix, fbt_ap);
			nvram_unset(tmp_fbt_ap);

			/* Traverse through all Virtual Prefix */
			for (j = 0; j < MAX_NVPARSE; j++) {
				sprintf(tmp_prefix, "wl%d.%d_", i, j);
				snprintf(tmp_fbt_ap, sizeof(tmp_fbt_ap), "%s%s_fbt_bssid",
					tmp_prefix, fbt_ap);
				nvram_unset(tmp_fbt_ap);
			}
		}
	}
}

/* Clear all the FBT NVRAMs */
void
fbt_restore_defaults(void)
{
	char tmp[100], prefix[] = "wlXXXXXXXXXXXXXXXXXXXXXXXXXXXX_mssid_";
	int i, j, iter_param;

	/* List of FBT NVRAMs */
	char* fbt_nvrams[] = {
		"fbt",
		"fbt_mdid",
		"fbtoverds",
		"fbt_reassoc_time",
		"fbt_ap",
		"r0kh_id",
		"r1kh_id",
		"r0kh_key",
		"fbt_aps",
		"fbt_all_aps",
		"fbt_all_gen_aps"
	};

	/* Traverse through all Primary Prefix to restore fbt_aps NVRAMs */
	for (i = 0; i < MAX_NVPARSE; i++) {
		sprintf(prefix, "wl%d_", i);
		fbt_aps_restore_defaults(prefix);

		/* Traverse through all Virtual Prefix */
		for (j = 0; j < MAX_NVPARSE; j++) {
			sprintf(prefix, "wl%d.%d_", i, j);
			fbt_aps_restore_defaults(prefix);
		}
	}

	/* Traverse through all FBT NVRAMs */
	for (iter_param = 0; iter_param < ARRAYSIZE(fbt_nvrams); iter_param++) {
		/* Traverse through all Primary Prefix */
		for (i = 0; i < MAX_NVPARSE; i++) {
			sprintf(prefix, "wl%d_", i);

			/* Clear NVRAMs with Primary Prefix */
			nvram_unset(strcat_r(prefix, fbt_nvrams[iter_param], tmp));

			/* Traverse through all Virtual Prefix */
			for (j = 0; j < MAX_NVPARSE; j++) {
				sprintf(prefix, "wl%d.%d_", i, j);

				/* Clear NVRAMs with Virtual Prefix */
				nvram_unset(strcat_r(prefix, fbt_nvrams[iter_param], tmp));
			}
		}
	}
}
#endif /* WLHOSTFBT */

#ifdef LINUX_2_6_36
static int
coma_uevent(void)
{
	char *modalias = NULL;
	char lan_ifname[32], *lan_ifnames, *next;

	modalias = getenv("MODALIAS");
	if (!strcmp(modalias, "platform:coma_dev")) {

		/* down WiFi adapter */
		lan_ifnames = nvram_safe_get("lan_ifnames");
		foreach(lan_ifname, lan_ifnames, next) {
			if (!strncmp(lan_ifname, "eth", 3)) {
				eval("wl", "-i", lan_ifname, "down");
			}
		}

		system("echo \"2\" > /proc/bcm947xx/coma");
	}
	return 0;
}
#endif /* LINUX_2_6_36 */

#ifdef DEBUG_RCTEST
int test_mknode(int id);
// used for various testing
static int rctest_main(int argc, char *argv[])
{
	int on;

	if (argc < 2) {
		_dprintf("test what?\n");
	}
	else if (strcmp(argv[1], "rc_service")==0) {
		notify_rc(argv[2]);
	}
#ifdef RTCONFIG_BCM_7114
	else if (strcmp(argv[1], "spect")==0) {
		start_dfs();
	}
#endif
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	else if (strcmp(argv[1], "permission") == 0) {
		setup_passwd();
	}
#endif
	else if (strcmp(argv[1], "GetPhyStatus")==0) {
		printf("Get Phy status:%d\n", GetPhyStatus(0));
	}
	else if (strcmp(argv[1], "GetExtPhyStatus")==0) {
		printf("Get Ext Phy status:%d\n", GetPhyStatus(atoi(argv[2])));
	}
#ifdef HND_ROTUER
	else if (strcmp(argv[1], "memdw")==0) {
		const char *dws[]={"dw", argv[2]};
		_memaccess(2, dws);
	}
#endif
	else if (strcmp(argv[1], "get_phy_status")==0) {
		int mask;
		mask = atoi(argv[2]);
		TRACE_PT("debug for phy_status %x\n", get_phy_status(mask));
	}
	else if (strcmp(argv[1], "get_phy_speed")==0) {
		int mask;
		mask = atoi(argv[2]);
		TRACE_PT("debug for phy_speed %x\n", get_phy_speed(mask));
	}
#ifdef HND_ROUTER
	else if (strcmp(argv[1], "regr")==0) {
		unsigned int reg;
		sscanf(argv[2], "%x", &reg);
		_dprintf("regaccess rd 0x%x\n", reg);
		hnd_ethswctl(REGACCESS, reg, 2, 0, 0);
	}
	else if (strcmp(argv[1], "regw")==0) {
		unsigned int reg, data;
		sscanf(argv[2], "%x", &reg);
		sscanf(argv[3], "%x", &data);
		_dprintf("regaccess wr 0x%x, 0x%x\n", reg, data);
		hnd_ethswctl(REGACCESS, reg, 2, 1, data);
	}
	else if (strcmp(argv[1], "pregr")==0) {
		unsigned int reg;
		sscanf(argv[2], "%x", &reg);
		_dprintf("pregaccess rd 0x%x\n", reg);
		hnd_ethswctl(PMDIOACCESS, reg, 2, 0, 0);
	}
	else if (strcmp(argv[1], "pregw")==0) {
		unsigned int reg, data;
		sscanf(argv[2], "%x", &reg);
		sscanf(argv[3], "%x", &data);
		_dprintf("pregaccess wr 0x%x, 0x%x\n", reg, data);
		hnd_ethswctl(PMDIOACCESS, reg, 2, 1, data);
	}
	else if (strcmp(argv[1], "set_phy_ctrl")==0) {
		unsigned int mask, ctrl;
		sscanf(argv[2], "%x", &mask);
		sscanf(argv[3], "%x", &ctrl);
		_dprintf("phy_ctrl 0x%x/%x (%d)\n", mask, ctrl, set_phy_ctrl(mask, ctrl));
	}
	else if (strcmp(argv[1], "set_ex53134_ctrl")==0) {
		unsigned int mask, ctrl;
		sscanf(argv[2], "%x", &mask);
		sscanf(argv[3], "%x", &ctrl);
		_dprintf("ex53134 phy_ctrl 0x%x/%x (%d)\n", mask, ctrl, set_ex53134_ctrl(mask, ctrl));
	}
#endif
	else if (strcmp(argv[1], "lanports_ctrl")==0) {
		int val;
		val = atoi(argv[2]);
		_dprintf("lan ctrl %d\n", lanport_ctrl(val));
	}
	else if (strcmp(argv[1], "handle_notifications")==0) {
		handle_notifications();
	}
	else if (strcmp(argv[1], "check_action")==0) {
		_dprintf("check: %d\n", check_action());
	}
	else if (strcmp(argv[1], "nvramhex")==0) {
		int i;
		char *nv;

		nv = nvram_safe_get(argv[2]);

		_dprintf("nvram %s(%d): ", nv, strlen(nv));
		for(i=0;i<strlen(nv);i++) {
			_dprintf(" %x", (unsigned char)*(nv+i));
		}
		_dprintf("\n");
	}
	else {
		on = atoi(argv[2]);
		_dprintf("%s %d\n", argv[1], on);

		if (strcmp(argv[1], "vlan") == 0)
		{
			if (on) start_vlan();
			else stop_vlan();
		}
		else if (strcmp(argv[1], "lan") == 0) {
			if (on) start_lan();
			else stop_lan();
		}
		else if (strcmp(argv[1], "wl") == 0) {
			if (on)
			{
				start_wl();
				lanaccess_wl();
			}
		}
		else if (strcmp(argv[1], "wan") == 0) {
			if (on) start_wan();
			else stop_wan();
		}
		else if (strcmp(argv[1], "wan_port") == 0) {
			if (on) start_wan_port();
			else stop_wan_port();
		}
		else if (strcmp(argv[1], "firewall") == 0) {
			//if (on) start_firewall();
			//else stop_firewall();
		}
		else if (strcmp(argv[1], "watchdog") == 0) {
			if (on) start_watchdog();
			else stop_watchdog();
		}
#ifdef RTAC87U
		else if (strcmp(argv[1], "watchdog02") == 0) {
			if (on) start_watchdog02();
			else stop_watchdog02();
		}
#endif
#ifdef SW_DEVLED
		else if (strcmp(argv[1], "sw_devled") == 0) {
			if (on) start_sw_devled();
			else stop_sw_devled();
		}
#endif
#if defined(RTAC1200G) || defined(RTAC1200GP)
		else if (strcmp(argv[1], "wdg_monitor") == 0) {
			if (on) start_wdg_monitor();
			else stop_wdg_monitor();
		}
#endif
#ifdef RTCONFIG_FANCTRL
		else if (strcmp(argv[1], "phy_tempsense") == 0) {
			if (on) start_phy_tempsense();
			else stop_phy_tempsense();
		}
#endif
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
		else if (strcmp(argv[1], "psta_monitor") == 0) {
			if (on) start_psta_monitor();
			else stop_psta_monitor();
		}
#endif
#if defined(RTCONFIG_AMAS) && (defined(RTCONFIG_BCMWL6) || defined(RTCONFIG_LANTIQ))
		else if (strcmp(argv[1], "obd") == 0) {
			if (on) start_obd();
			else stop_obd();
		}
#endif
#ifdef RTCONFIG_IPERF
		else if (strcmp(argv[1], "monitor") == 0) {
			if (on) start_monitor();
			else stop_monitor();
		}
#endif
		else if (strcmp(argv[1], "qos") == 0) {//qos test
			if (on) {
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
					sleep(1);
#if 0
					f_write_string("/proc/sys/net/ipv4/conf/default/force_igmp_version", "0", 0, 0);
					f_write_string("/proc/sys/net/ipv4/conf/all/force_igmp_version", "0", 0, 0);
#endif
				}
#endif
			add_iQosRules(get_wan_ifname(wan_primary_ifunit()));
#ifdef RTCONFIG_BWDPI
				if (nvram_get_int("qos_type") == 1) {
					start_dpi_engine_service();
				}

				else
#endif
				start_iQos();
			}
			else
			{
#ifdef RTCONFIG_RALINK
				if (nvram_get_int("hwnat") &&
					/* TODO: consider RTCONFIG_DUALWAN case */
//					!nvram_match("wan0_proto", "l2tp") &&
//					!nvram_match("wan0_proto", "pptp") &&
//					!(nvram_get_int("fw_pt_l2tp") || nvram_get_int("fw_pt_ipsec") &&
//					(nvram_match("wl0_radio", "0") || nvram_get_int("wl0_mrate_x")) &&
//					(nvram_match("wl1_radio", "0") || nvram_get_int("wl1_mrate_x")) &&
					!module_loaded("hw_nat"))
				{
#if 0
					f_write_string("/proc/sys/net/ipv4/conf/default/force_igmp_version", "2", 0, 0);
					f_write_string("/proc/sys/net/ipv4/conf/all/force_igmp_version", "2", 0, 0);
#endif

#if defined(RTN14U) || defined(RTAC52U) || defined(RTAC51U) || defined(RTN11P) || defined(RTN300) || defined(RTN54U) || defined(RTAC1200HP) || defined(RTN56UB1) || defined(RTAC54U) || defined(RTN56UB2) || defined(RTAC1200GA1) || defined (RTAC1200GU) || defined(RTAC85U) || defined(RTAC51UP) || defined(RTAC53) || defined(RTN800HP)
					if (!(!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")))
#endif
					{
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
						sleep(1);
					}
				}
#endif
#ifdef RTCONFIG_BWDPI
				if (nvram_get_int("qos_type") == 1) {
					stop_dpi_engine_service(1);
				}
				else
#endif
				stop_iQos();
				del_iQosRules();
			}
		}
#ifdef RTCONFIG_WEBDAV
		else if (strcmp(argv[1], "webdav") == 0) {
			if (on)
				start_webdav();
		}
#endif
#ifdef RTCONFIG_TUNNEL
		else if (strcmp(argv[1], "mastiff") == 0) {
			if (on)
				start_mastiff();
		}
#endif
		else if (strcmp(argv[1], "gpiow") == 0) {
			if (argc>=4) set_gpio(atoi(argv[2]), atoi(argv[3]));
		}
		else if (strcmp(argv[1], "gpior") == 0) {
			printf("%d\n", get_gpio(atoi(argv[2])));
		}
#ifndef HND_ROUTER
		else if (strcmp(argv[1], "gpiod") == 0) {
			if (argc>=4) gpio_dir(atoi(argv[2]), atoi(argv[3]));
		}
#endif
		else if (strcmp(argv[1], "init_switch") == 0) {
			init_switch();
		}
		else if (strcmp(argv[1], "set_action") == 0) {
			set_action(on);
		}
		else if (strcmp(argv[1], "pwr_usb") == 0) {
			set_pwr_usb(atoi(argv[2]));
			_dprintf("done.\n");
		}
#ifdef RT4GAC68U
		else if (strcmp(argv[1], "pwr_modem") == 0) {
			set_pwr_modem(atoi(argv[2]));
			_dprintf("done.\n");
		}
#endif
		else if (strcmp(argv[1], "enc_chk") == 0) {
			unsigned char enc_buf[ENC_WORDS_LEN];
			unsigned char dec_buf[DATA_WORDS_LEN + 1];

			_dprintf("get enc str:[%s]\n", enc_str(argv[2], (char *) enc_buf));
			_dprintf("get dec str:[%s]\n", dec_str((char *) enc_buf, (char *) dec_buf));

			_dprintf("done(%d)\n", strcmp(argv[2], (const char *) dec_buf));
		}
#ifdef RTCONFIG_BCMFA
		else if (strcmp(argv[1], "fa_rev") == 0) {
			_dprintf("(%d) done.\n", get_fa_rev());
		}
		else if (strcmp(argv[1], "fa_dump") == 0) {
			_dprintf("(%d) done.\n", get_fa_dump());
		}
#endif
		else {
			printf("what?\n");
		}
	}
	return 0;
}
#endif

#if defined(RTCONFIG_SOC_IPQ8064) || defined(RTCONFIG_SOC_IPQ40XX) || defined(RTCONFIG_LANTIQ) || defined(RPAC51) || defined(MAPAC1750)
/* download firmware */
#ifndef FIRMWARE_DIR
#if defined(RTCONFIG_SOC_IPQ8064) || defined(RTCONFIG_SOC_IPQ40XX) || defined(RPAC51) || defined(MAPAC1750)
#define FIRMWARE_DIR	"/lib/firmware"
#else
#define FIRMWARE_DIR	"/tmp"
#endif
#endif
#ifndef FW_BUF_SIZE
#define FW_BUF_SIZE	4096
#endif
static int hotplug_firmware(void)
{
	int ret = EINVAL;
	FILE *f_fw = NULL, *f_loading = NULL, *f_data = NULL;
	char sysfs_path[PATH_MAX], fw_path[PATH_MAX];
	unsigned char buf[FW_BUF_SIZE] __attribute__((aligned(4)));
	char *action, *devpath, *fw_name;
	char *fw_root = FIRMWARE_DIR;
	char *sysfs_root = "/sys";
	const void *hook_data;
	size_t hook_size = 0, len, tlen = 0;

	action = getenv("ACTION");
	devpath = getenv("DEVPATH");
	fw_name = getenv("FIRMWARE");
	if (!action || !devpath || !fw_name) {
		_dprintf("ACTION (%s), DEVPATH (%s), or FIRMWARE (%s) are NULL!\n",
			(!action)?"<NULL>":action, (!devpath)?"<NULL>":devpath, (!fw_name)?"<NULL>":fw_name);
		return EINVAL;
	}
	if (strcmp(action, "add"))	/* Only "add" action is required to support downloade firmware */
		return 0;

	// Generate filename that are required.
	sysfs_path[0] = fw_path[0] = '\0';
	sprintf(sysfs_path, "%s/%s/loading", sysfs_root, devpath);
	f_loading = fopen(sysfs_path, "w");
	if (!f_loading) {
		_dprintf("Open %s/%s/loading fail, errno %d (%s)\n",
			sysfs_root, devpath, errno, strerror(errno));
		goto err_exit1;
	}
	sprintf(sysfs_path, "%s/%s/data", sysfs_root, devpath);
	f_data = fopen(sysfs_path, "wb");

	hook_data = req_fw_hook(fw_name, &hook_size);
	if (!hook_data || !hook_size) {
		sprintf(fw_path, "%s/%s", fw_root, fw_name);
		f_fw = fopen(fw_path, "rb");
	}
	// If open firmware successful, notify kernel we are going to download firmware.
	// If open firmware failure, notify kernel we cannot get firmware
	if ((hook_data && hook_size) || (f_fw && f_data)) {
		fputs ("1", f_loading);
	} else {
		fputs ("-1", f_loading);
		_dprintf("Open data (%s,%p) or firmware (%s,%p) failure\n", sysfs_path, f_data, fw_path, f_fw);
		goto err_exit2;
	}
	fflush (f_loading);

	/* Download firmware */
	if (hook_data && hook_size) {
		tlen = len = hook_size;
		while (tlen > 0) {
			len = fwrite(hook_data + (hook_size - tlen), 1, len, f_data);
			tlen -= len;
		}
	} else {
		while (!feof(f_fw)) {
			len = fread(buf, 1, FW_BUF_SIZE, f_fw);
			len = fwrite(buf, 1, len, f_data);
			tlen += len;
		}
	}
	fflush (f_data);

	/* Notify kernel the downloading process had finished */
	fputs ("0", f_loading);
	fflush (f_loading);

	ret = 0;

err_exit2:
	if (f_loading)
		fclose(f_loading);
	if (f_data)
		fclose(f_data);
	if (f_fw)
		fclose(f_fw);
err_exit1:
	return ret;
}
#endif

static int hotplug_main(int argc, char *argv[])
{
	if (argc >= 2) {
		if (strcmp(argv[1], "net") == 0) {
			hotplug_net();
		}
#ifdef RTCONFIG_USB
		// !!TB - USB Support
		else if (strcmp(argv[1], "usb") == 0) {
			hotplug_usb();
		}
#ifdef LINUX26
		else if (strcmp(argv[1], "block") == 0) {
			hotplug_usb();
		}
#endif
#if defined(LINUX_2_6_36)
		else if (!strcmp(argv[1], "platform"))
			return coma_uevent();
#endif /* LINUX_2_6_36 */
#endif
#if defined(RTCONFIG_SOC_IPQ8064) || defined(RTCONFIG_SOC_IPQ40XX) || defined(RTCONFIG_LANTIQ) || defined(RPAC51) || defined(MAPAC1750)
		else if(!strcmp(argv[1], "firmware")) {
			hotplug_firmware();
		}
#endif
	}
	return 0;
}

typedef struct {
	const char *name;
	int (*main)(int argc, char *argv[]);
} applets_t;

static const applets_t applets[] = {
	{ "preinit",			init_main				},
	{ "init",			init_main				},
	{ "console",			console_main				},
#ifdef DEBUG_RCTEST
	{ "rc",				rctest_main				},
#endif
	{ "ip-up",			ipup_main				},
	{ "ip-down",			ipdown_main				},
	{ "ip-pre-up",			ippreup_main				},
#ifdef RTCONFIG_IPV6
	{ "ipv6-up",			ip6up_main				},
	{ "ipv6-down",			ip6down_main				},
#endif
	{ "auth-fail",			authfail_main				},
#ifdef RTCONFIG_VPNC
	{ "vpnc-ip-up",			vpnc_ipup_main				},
	{ "vpnc-ip-down",		vpnc_ipdown_main			},
	{ "vpnc-ip-pre-up",		vpnc_ippreup_main			},
	{ "vpnc-auth-fail",		vpnc_authfail_main			},
#ifdef RTCONFIG_VPN_FUSION
	{ "ovpnc-up",			vpnc_ovpn_up_main				},
	{ "ovpnc-down",		vpnc_ovpn_down_main			},
	{ "ovpnc-route-up",	vpnc_ovpn_route_up_main			},
#endif
#endif
#ifdef RTCONFIG_EAPOL
	{ "wpa_cli",			wpacli_main			},
#endif
	{ "hotplug",			hotplug_main			},
#ifndef HND_ROUTER
#ifdef RTCONFIG_BCMARM
	{ "mtd-write",			mtd_write_main_old		},
	{ "mtd-erase",			mtd_unlock_erase_main_old	},
	{ "mtd-unlock",			mtd_unlock_erase_main_old	},
#else
	{ "mtd-write",			mtd_write_main			},
	{ "mtd-erase",			mtd_unlock_erase_main		},
	{ "mtd-unlock",			mtd_unlock_erase_main		},
#endif
#endif
	{ "watchdog",			watchdog_main			},
#if ! (defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK))
	{ "watchdog02",			watchdog02_main			},
#endif  /* ! (RTCONFIG_QCA || RTCONFIG_RALINK) */
#ifdef SW_DEVLED
	{ "sw_devled",			sw_devled_main			},
#endif
#if defined(RTAC1200G) || defined(RTAC1200GP)
	{ "wdg_monitor",		wdg_monitor_main		},
#endif
#ifdef RTCONFIG_FANCTRL
	{ "phy_tempsense",		phy_tempsense_main		},
#endif
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	{ "psta_monitor",		psta_monitor_main		},
#endif
#if defined(RTCONFIG_AMAS) && (defined(RTCONFIG_BCMWL6) || defined(RTCONFIG_LANTIQ)) && !defined(RTCONFIG_DISABLE_REPEATER_UI)
	{ "obd",			obd_main			},
#endif
#ifdef RTCONFIG_IPERF
	{ "monitor",			monitor_main			},
#endif
#ifdef RTCONFIG_QTN
	{ "qtn_monitor",		qtn_monitor_main		},
#endif
#ifdef RTCONFIG_LANTIQ
	{ "wave_monitor",		wave_monitor_main		},
#endif
#if defined(RTCONFIG_USB) && !defined(RTCONFIG_NO_USBPORT)
	{ "usbled",			usbled_main			},
#endif
	{ "ddns_updated", 		ddns_updated_main		},
	{ "ddns_custom_updated",	ddns_custom_updated_main	},
	{ "radio",			radio_main			},
	{ "ots",			ots_main			},
	{ "udhcpc",			udhcpc_wan			},
	{ "udhcpc_lan",			udhcpc_lan			},
	{ "zcip",			zcip_wan			},
#ifdef RTCONFIG_IPV6
	{ "dhcp6c",			dhcp6c_wan			},
#endif
#if defined(RTCONFIG_CONCURRENTREPEATER)
#if !defined(RPAC68U)
	{ "led_monitor",		led_monitor_main		},
#endif
#if defined(RTCONFIG_RALINK)
	{ "re_wpsc",			re_wpsc_main			},
	{ "air_monitor",		air_monitor_main		},
#endif
#endif
#ifdef RTCONFIG_WPS
	{ "wpsaide",			wpsaide_main			},
#ifdef RTCONFIG_QCA
	{ "get_wps_er",			get_wps_er_main			},
#endif
#endif
	{ "halt",			reboothalt_main			},
	{ "reboot",			reboothalt_main			},
	{ "ntp", 			ntp_main			},
#ifdef RTCONFIG_NETOOL
	{ "netool", 			netool_main			},
#endif
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_EXT_RTL8365MB) || defined(RTCONFIG_EXT_RTL8370MB)
	{ "rtkswitch",			config_rtkswitch		},
#if defined(RTAC53) || defined(RTAC51UP)
	{ "mtkswitch",			config_mtkswitch		},
#endif
#elif defined(RTCONFIG_QCA)
	{ "rtkswitch",			config_rtkswitch		},
#endif
	{ "delay_exec",			delay_main			},

	{ "wanduck",			wanduck_main			},
	{ "conn_diag",			conn_diag_main			},
#if defined(CONFIG_BCMWL5) && !defined(HND_ROUTER) && defined(RTCONFIG_DUALWAN)
	{ "dualwan",			dualwan_control			},
#endif
	{ "tcpcheck",			tcpcheck_main			},
	{ "autodet", 			autodet_main			},
#ifdef RTCONFIG_DETWAN
	{ "detwan", 			detwan_main			},
#endif
#ifdef RTCONFIG_QCA_PLC_UTILS
	{ "autodet_plc", 		autodet_plc_main		},
#endif
#ifdef RTCONFIG_CIFS
	{ "mount-cifs",			mount_cifs_main			},
#endif
#ifdef RTCONFIG_USB
	{ "ejusb",			ejusb_main			},
#ifdef RTCONFIG_DISK_MONITOR
	{ "disk_monitor",		diskmon_main			},
#endif
	{ "disk_remove",		diskremove_main			},
#endif
	{ "firmware_check",		firmware_check_main             },
#ifdef RTAC68U
	{ "firmware_enc_crc",		firmware_enc_crc_main		},
	{ "fw_check",			fw_check_main			},
#endif
#ifdef BUILD_READMEM
	{ "readmem",			readmem_main			},
#endif
#ifdef RTCONFIG_HTTPS
	{ "rsasign_check",		rsasign_check_main		},
	{ "rsarootca_check",		rsarootca_check_main		},
#endif
	{ "service",			service_main			},
#ifdef RTCONFIG_SPEEDTEST
	{ "speedtest",			speedtest_main			},
#endif
#if defined(RTCONFIG_BWDPI)
	{ "bwdpi",			bwdpi_main			},
	{ "bwdpi_check",		bwdpi_check_main		},
	{ "bwdpi_wred_alive",		bwdpi_wred_alive_main		},
	{ "bwdpi_db_10",		bwdpi_db_10_main		},
	{ "rsasign_sig_check",		rsasign_sig_check_main		},
#endif
	{ "hour_monitor",		hour_monitor_main		},
#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_INTERNAL_GOBI
	{ "lteled",			lteled_main			},
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	{ "modem_data",			modem_data_main			},
#endif
#endif
#endif
#ifdef RTCONFIG_TR069
	{ "dhcpc_lease",		dhcpc_lease_main		},
#endif
#ifdef RTCONFIG_NEW_USER_LOW_RSSI
	{ "roamast",			roam_assistant_main		},
#endif
#ifdef RTCONFIG_DHCP_OVERRIDE
	{ "detectWAN_arp",		detectWAN_arp_main		},
#endif
#if defined(RTCONFIG_KEY_GUARD)
	{ "keyguard",			keyguard_main			},
#endif
#ifdef RTCONFIG_LETSENCRYPT
	{ "le_acme",				le_acme_main			},
#endif
#if !(defined(RTCONFIG_QCA) || defined(RTCONFIG_RALINK) || defined(RTCONFIG_REALTEK))
	{ "erp_monitor",		erp_monitor_main		},
#endif
#if defined(MAPAC2200)
	{ "dpdt_ant",			dpdt_ant_main		},
#endif
#if defined(MAPAC1300) || defined(VZWAC1300)
	{ "thermal_txpwr",		thermal_txpwr_main		},
#endif
#ifdef RTCONFIG_ADTBW
	{ "adtbw",			adtbw_main		},
#endif
	{NULL, NULL}
};

extern void gen_spcmd(char *);
int main(int argc, char **argv)
{
	char *base;
	int f;

	/*
		Make sure std* are valid since several functions attempt to close these
		handles. If nvram_*() runs first, nvram=0, nvram gets closed. - zzz
	*/

	if ((f = open("/dev/null", O_RDWR)) < 0) {
	}
	else if (f < 3) {
		dup(f);
		dup(f);
	}
	else {
		close(f);
	}

	base = strrchr(argv[0], '/');
	base = base ? base + 1 : argv[0];

#if 0
	if (strcmp(base, "rc") == 0) {
		if (argc < 2) return 1;
		if (strcmp(argv[1], "start") == 0) return kill(1, SIGUSR2);
		if (strcmp(argv[1], "stop") == 0) return kill(1, SIGINT);
		if (strcmp(argv[1], "restart") == 0) return kill(1, SIGHUP);
		++argv;
		--argc;
		base = argv[0];
	}
#endif

#if !defined(CONFIG_BCMWL5)
    if (getpid() != 1)
    {
#endif

#if defined(DEBUG_NOISY)
	if (nvram_match("debug_logrc", "1")) {
		int i;

		cprintf("[rc %d] ", getpid());
		for (i = 0; i < argc; ++i) {
			cprintf("%s ", argv[i]);
		}
		cprintf("\n");

	}
#endif

#if defined(DEBUG_NOISY)
	if (nvram_match("debug_ovrc", "1")) {
		char tmp[256];
		char *a[32];

		realpath(argv[0], tmp);
		if ((strncmp(tmp, "/tmp/", 5) != 0) && (argc < 32)) {
			snprintf(tmp, sizeof(tmp),"%s%s", "/tmp/", base);
			if (f_exists(tmp)) {
				cprintf("[rc] override: %s\n", tmp);
				memcpy(a, argv, argc * sizeof(a[0]));
				a[argc] = 0;
				a[0] = tmp;
				execvp(tmp, a);
				exit(0);
			}
		}
	}
#endif
#if !defined(CONFIG_BCMWL5)
    }
#endif

	if (strcmp(base, "reboot") == 0 || strcmp(base, "halt") == 0) {
		int result = shutdown_start("rc", "main", base);

		if (result != 0)
			return result;
	}

	const applets_t *a;
	for (a = applets; a->name; ++a) {
		if (strcmp(base, a->name) == 0) {
			openlog(base, LOG_PID, LOG_USER);
			return a->main(argc, argv);
		}
	}


#ifdef RTCONFIG_WIFI_SON
        if(!strcmp(base, "hive_cap")){
                if(nvram_get_int("sw_mode")==SW_MODE_ROUTER || nvram_match("cfg_master", "1")) {
                        printf("start central ap...\n");
                        if (argv[1] && (!strcmp(argv[1], "start")))
                                start_cap(0);
                        else if (argv[1] && (!strcmp(argv[1], "config_change")))
                                start_cap(1);
                        else if (argv[1] && (!strcmp(argv[1], "restart")))
                                start_cap(2);
                        else
                                printf("error command.\n");
                }
     		else
                        printf("Error. CAP should run in router mode.\n");
                return 0;
        }

        if(!strcmp(base, "hive_re")){
                if (nvram_get_int("sw_mode")==SW_MODE_AP && !nvram_match("cfg_master", "1")) {
                        printf("start range extender ...\n");
                        if (argv[1] && (!strcmp(argv[1], "start")))
                                start_re(0);
                        else if (argv[1] && (!strcmp(argv[1], "reset")))
                                start_re(1);
                        else if (argv[1] && (!strcmp(argv[1], "restart")))
                                start_re(2);
                        else if (argv[1] && (!strcmp(argv[1], "waitimeout")))
                                start_re(3);
                        else
                                printf("error command.\n");
                }
                else
        		printf("Error. RE should run in AP mode.\n");
                return 0;
       }
        if(!strcmp(base, "hive_cmd")){
                if(nvram_get_int("sw_mode")==SW_MODE_ROUTER || nvram_match("cfg_master", "1")) {
                        printf("start central ap...\n");
			if(argv[1] && strlen(argv[1])) {
				if (!strcmp(argv[1], "reboot"))
					gen_spcmd("xx");
                                start_cmd(argv[1]);
			}
                        else
                                printf("error command.\n");
                }
     		else
                        printf("Error. CAP should run in router mode.\n");
                return 0;
        }
	if(!strcmp(base, "hive_wsplcd")){
                start_wsplcd();
                return 0;
       }
	if(!strcmp(base, "hive_hyd")){
                start_hyd();
                return 0;
       }
#ifdef RTCONFIG_ETHBACKHAUL
        if(!strcmp(base, "hive_eth")){
                if (nvram_get_int("sw_mode")==SW_MODE_AP && !nvram_match("cfg_master", "1")) {
			nvram_set("eth_detect_proc","1");
                        if (argv[1] && (!strcmp(argv[1], "1")))
                                start_eth(1);
                        else if (argv[1] && (!strcmp(argv[1], "0")))
			{
				_dprintf("run when upstream plugout ...RE\n");
                                start_eth(0);
			}
                        else if (argv[1] && (!strcmp(argv[1], "2")))
			{
				_dprintf("run when downstream plugout ...RE\n");
                                start_eth(2);
			}
                        else if (argv[1] && (!strcmp(argv[1], "3")))
                                start_eth(3);
			
			nvram_set("eth_detect_proc","0");
			nvram_set_int("prelink_pap_status", -1); // trigger LED to change
                }
		else if (nvram_get_int("sw_mode")==SW_MODE_ROUTER || nvram_match("cfg_master", "1")) {
                        if (argv[1] && (!strcmp(argv[1], "1")))
			{
				start_eth(1); /* move to repacd */
			}
                        else if (argv[1] && (!strcmp(argv[1], "0")))
			{
				nvram_set("eth_detect_proc","1");
				_dprintf("run when lan plugout...CAP\n");
				start_eth(0); /* move to repacd */
				nvram_set("eth_detect_proc","0");
			}
			nvram_set_int("prelink_pap_status", -1); // trigger LED to change
		}	
                else
        		printf("Error command.\n");
                return 0;
       }

       if(!strcmp(base, "eth_bh_mon")){
		start_eth_bh_mon();
                return 0;
       }

       if(!strcmp(base, "ethbh_peer_detect")){
		if(argc >= 4)
			ethbh_peer_detect(argv[1], argv[2], argv[3]);
                return 0;
       }
#endif
       if(!strcmp(base, "wifimon_check")){	
		int default_sec=0;
                if (nvram_get_int("sw_mode")==SW_MODE_AP && !nvram_match("cfg_master", "1")) {
			if(argv[1] && strlen(argv[1]))
			{
				if (safe_atoi(argv[1])>0 && safe_atoi(argv[1])<240)
				{
					start_wifimon_check(safe_atoi(argv[1]));
					return 0;
				}
			}
			start_wifimon_check(default_sec);
		}
		else
			printf("Error. wifimon_check should run in RE.\n");
		return 0;
	}
#if defined(RTCONFIG_LP5523)
/*
 * Manual setting LP5523 leds
 * usage: lp55xx_set_led [behavior_mode] [Blue_vol] [Green_vol] [Red_vol]
 *
 * Behavior:	300~xxx(xxx please refer to lp55xx_leds_mode in shared/lp5523led.h)
 * XXX_vol:	0~255
 *
 * */
	if (!strcmp(base, "lp55xx_set_led")) {
		if (argv[1] && argv[2] && argv[3] && argv[4])
		{
			char tmp[32];
			int ptb_mode=0;
			int i=0;

			memset(tmp, '\0', 32);
			while (argv[i])
			{
				if (i>5)
				{
					printf("Error. Enter too many parameters.\n");
					return 0;
				}

				switch (i) {
				case 1:
					if (safe_atoi(argv[i])>LP55XX_END_BLINK || safe_atoi(argv[i])<LP55XX_ACT_NONE)
					{
						printf("Error. Set LED behavior failed. [%d - %d]\n", LP55XX_ACT_NONE, LP55XX_END_BLINK);
						return 0;
					}

					ptb_mode = safe_atoi(argv[i]);
					break;
				case 2:
				case 3:
				case 4:
					if (safe_atoi(argv[i])>255 || safe_atoi(argv[i])<0)
					{
						printf("Error. Set LED(%d) power failed. [%d - %d]\n", i-1, 0, 255);
						return 0;
					}

					if (i==2)
						sprintf(tmp, "%s%s%x", tmp, safe_atoi(argv[i])<16?"0":"", safe_atoi(argv[i]));
					else
						sprintf(tmp, "%s_%s%x", tmp, safe_atoi(argv[i])<16?"0":"", safe_atoi(argv[i]));

					break;
				}

				i++;
			}

			nvram_set("lp55xx_lp5523_manual", tmp);
			lp55xx_leds_proc(LP55XX_MANUAL_COL, ptb_mode);
		}
		else
			printf("Error. Enter parameter failed.\n\n"
				"Usage: lp55xx_set_led [behavior_mode] [Blue_vol] [Green_vol] [Red_vol]\n\n"
				"Behavior: 300~xxx(xxx please refer to lp55xx_leds_mode in shared/lp5523led.h)\n"
				"XXX_vol:  0~255\n");

		return 0;
	}
/*
 * Manual setting LP5523 leds
 * usage: lp55xx_user_set [enable] [color] [behavior] [brightness] [start] [end]
 *
 * enable:	0:OFF, 1:ON
 * color:	100~xxx(xxx please refer to lp55xx_leds_mode in shared/lp5523led.h)
 * behavior:	300~xxx(xxx please refer to lp55xx_leds_mode in shared/lp5523led.h)
 * brightness:	10~100(%)
 * start:	0~24(schedule)/ 99(user defined led)
 * end:		0~24(schedule)/ 99(user defined led)
 *
 * */
	if (!strcmp(base, "lp55xx_user_set")) {
		if (argv[1] && argv[2] && argv[3] && argv[4] && argv[5] && argv[6])
		{
			int i=0, prestate=0;
			int ptc_mode=0, ptb_mode=0, brightness=0;
			int enable=0, sch_start=0, sch_end=0;

			while (argv[i])
			{
				if (i>7)
				{
					printf("Error. Enter too many parameters.\n");
					return 0;
				}

				switch (i) {
				case 1:
					enable = safe_atoi(argv[i]);

					if (enable>2 || enable<0)
					{
						printf("Error. Set LED enable failed. [%d - %d]\n", 0 , 1);
						return 0;
					}
					break;
				case 2:
					ptc_mode = safe_atoi(argv[i]);

					if (ptc_mode>LP55XX_END_COLOR || ptc_mode<LP55XX_ALL_BREATH_LEDS)
					{
						printf("Error. Set LED color failed. [%d - %d]\n", LP55XX_ALL_BREATH_LEDS, LP55XX_END_COLOR);
						return 0;
					}
					break;
				case 3:
					ptb_mode = safe_atoi(argv[i]);

					if (ptb_mode>LP55XX_END_BLINK || ptb_mode<LP55XX_ACT_NONE)
					{
						printf("Error. Set LED behavior failed. [%d - %d]\n", LP55XX_ACT_NONE, LP55XX_END_BLINK);
						return 0;
					}
					break;
				case 4:
					brightness = safe_atoi(argv[i]);

					if (brightness>100 || brightness<10)
					{
						printf("Error. Set LED brightness failed. [%d - %d]\n", 10 , 100);
						return 0;
					}
					break;
				case 5:
					sch_start = safe_atoi(argv[i]);
				case 6:
					sch_end = safe_atoi(argv[i]);

					if ( sch_end>24 || sch_end<0)
					{
						if (sch_end!=99)
						{
							printf("Error. Set LED %s time failed. [%d - %d] or %d to set user defininaton\n", i==5?"start":"end", 0, 24, 99);
							return 0;
						}
					}
					break;
				}

				i++;
			}

			if (sch_start==99 || sch_end==99)
			{
				if (enable)
				{
					nvram_set_int("lp55xx_lp5523_user_col", ptc_mode);
					nvram_set_int("lp55xx_lp5523_user_beh", ptb_mode);
					nvram_set_int("lp55xx_lp5523_user_brightness", brightness);
				}

				prestate = 1;
				nvram_set_int("lp55xx_lp5523_user_enable", enable);
			}
			else
			{
				if (enable)
				{
					nvram_set_int("lp55xx_lp5523_sch_col", ptc_mode);
					nvram_set_int("lp55xx_lp5523_sch_beh", ptb_mode);
					nvram_set_int("lp55xx_lp5523_sch_brightness", brightness);
					lp55xx_leds_sch(sch_start, sch_end);
				}
				else if (nvram_get_int("lp55xx_lp5523_sch_enable"))
					prestate = 1;

				nvram_set_int("lp55xx_lp5523_sch_enable", enable);

				if (enable==2)
					lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_SCH_ENABLE);
			}

			if (prestate)
				lp55xx_leds_proc(LP55XX_ALL_LEDS_OFF, LP55XX_PREVIOUS_STATE);


			nvram_commit();
		}
		else
			printf("Error. Enter parameter failed.\n\n"
				"Usage: lp55xx_user_set [enable] [color_mode] [behavior_mode] [brightness] [start time] [end time]\n\n"
				"enable:  	0:OFF, 1:ON\n"
				"color:  	100~xxx(xxx please refer to lp55xx_leds_mode in shared/lp5523led.h)\n"
				"behavior:  	300~xxx(xxx please refer to lp55xx_leds_mode in shared/lp5523led.h)\n"
				"brightness:  	10~100\n"
				"start:  	0~24(schedule)/ 99(user defined led)\n"
				"end:  		0~24(schedule)/ 99(user defined led)\n");

		return 0;
	}
#endif
#endif /* RTCONFIG_WIFI_SON */

	if (!strcmp(base, "restart_wireless")) {
		printf("restart wireless...\n");
		restart_wireless();
		return 0;
	}
#ifdef RTCONFIG_BCM_7114
	else if (!strcmp(base, "stop_wl")) {
		stop_wl_bcm();
		return 0;
	}
#endif
#ifdef RTCONFIG_USB
#ifdef RTCONFIG_ERPTEST
	else if (!strcmp(base, "restart_usb")) {
		int f_stop = 0;
		if (argc == 2)
			f_stop = atoi(argv[1]);
		printf("%s usb...\n", f_stop ? "stop" : "restart");
		restart_usb(f_stop);
		return 0;
	}
#endif
	else if (!strcmp(base, "get_apps_name")) {
		if (argc != 2) {
			printf("Usage: get_apps_name [File name]\n");
			return 0;
		}

		return get_apps_name(argv[1]);
	}
#ifdef BCM_MMC
	else if (!strcmp(base, "asus_mmc")) {
		if (argc != 3) {
			printf("Usage: asus_mmc [device_name] [action]\n");
			return 0;
		}

		return asus_mmc(argv[1], argv[2]);
	}
#endif
	else if (!strcmp(base, "asus_sd")) {
		if (argc != 3) {
			printf("Usage: asus_sd [device_name] [action]\n");
			return 0;
		}

		return asus_sd(argv[1], argv[2]);
	}
	else if (!strcmp(base, "asus_lp")) {
		if (argc != 3) {
			printf("Usage: asus_lp [device_name] [action]\n");
			return 0;
		}

		return asus_lp(argv[1], argv[2]);
	}
	else if (!strcmp(base, "asus_sg")) {
		if (argc != 3) {
			printf("Usage: asus_sg [device_name] [action]\n");
			return 0;
		}

		return asus_sg(argv[1], argv[2]);
	}
	else if (!strcmp(base, "asus_sr")) {
		if (argc != 3) {
			printf("Usage: asus_sr [device_name] [action]\n");
			return 0;
		}

		return asus_sr(argv[1], argv[2]);
	}
	else if (!strcmp(base, "asus_tty")) {
		if (argc != 3) {
			printf("Usage: asus_tty [device_name] [action]\n");
			return 0;
		}

		return asus_tty(argv[1], argv[2]);
	}
	else if (!strcmp(base, "asus_usbbcm")) {
		if (argc != 3) {
			printf("Usage: asus_usbbcm [device_name] [action]\n");
			return 0;
		}

		return asus_usbbcm(argv[1], argv[2]);
	}
	else if (!strcmp(base, "asus_usb_interface")) {
		if (argc != 3) {
			printf("Usage: asus_usb_interface [device_name] [action]\n");
			return 0;
		}

		return asus_usb_interface(argv[1], argv[2]);
	}
	else if (!strcmp(base, "usb_notify")) {
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		usb_notify();
#endif

		return 0;
	}
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	else if (!strcmp(base, "run_app_script")) {
		if (argc != 3) {
			printf("Usage: run_app_script [Package name | allpkg] [APP action]\n");
			return 0;
		}

		if (!strcmp(argv[1], "allpkg"))
			return run_app_script(NULL, argv[2]);
		else
			return run_app_script(argv[1], argv[2]);
	}
	else if (!strcmp(base, "chk_app_state")) {
#define PID_FILE "/var/run/chk_app_state.pid"
		FILE *fp;
		char chk_value[4];

		if (f_read_string(PID_FILE, chk_value, 4) > 0
				&& atoi(chk_value) != getpid()) {
			_dprintf("Already running!\n");
			return 0;
		}

		if ((fp = fopen(PID_FILE, "w")) == NULL) {
			_dprintf("Can't open the pid file!\n");
			return 0;
		}

		fprintf(fp, "%d", getpid());
		fclose(fp);

		strlcpy(chk_value, nvram_safe_get("apps_state_switch"), sizeof(chk_value));
		if (strcmp(chk_value, "")) {
			if (atoi(chk_value) != APPS_SWITCH_FINISHED && !pids("app_switch.sh")) {
				_dprintf("Don't have the switch script.\n");
				nvram_set("apps_state_switch", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		strlcpy(chk_value, nvram_safe_get("apps_state_install"), sizeof(chk_value));
		if (strcmp(chk_value, "")) {
			if (atoi(chk_value) != APPS_INSTALL_FINISHED && !pids("app_install.sh")) {
				_dprintf("Don't have the install script.\n");
				nvram_set("apps_state_install", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		strlcpy(chk_value, nvram_safe_get("apps_state_upgrade"), sizeof(chk_value));
		if (strcmp(chk_value, "")) {
			if (atoi(chk_value) != APPS_UPGRADE_FINISHED && !pids("app_upgrade.sh")) {
				_dprintf("Don't have the upgrade script.\n");
				nvram_set("apps_state_upgrade", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		strlcpy(chk_value, nvram_safe_get("apps_state_enable"), sizeof(chk_value));
		if (strcmp(chk_value, "")) {
			if (atoi(chk_value) != APPS_ENABLE_FINISHED && !pids("app_set_enabled.sh")) {
				_dprintf("Don't have the enable script.\n");
				nvram_set("apps_state_enable", "");
			}

			unlink(PID_FILE);
			return 0;
		}

		unlink(PID_FILE);
		return 0;
	}
#endif
#endif
#ifdef RTCONFIG_LANTIQ
	else if(!strcmp(base, "update_client")) {
		_dprintf("update_client\n");
		if(argc == 4)
			update_client_event(argv[1], argv[2], atoi(argv[3]));
		return 0;
	}
	else if(!strcmp(base, "restart_bluetoothd")) {
		system("killall bluetoothd");
		system("hciconfig hci0 down");
		system("hciconfig hci0 reset");
		system("hciconfig hci0 up");
		system("hciconfig hci0 leadv 0");
		system("bluetoothd -n &");
		return 0;
	}
	else if(!strcmp(base, "set_usb3_to_usb2")) {
		set_usb3_to_usb2();
		puts("1");
		return 0;
	}
	else if(!strcmp(base, "set_usb2_to_usb3")) {
		set_usb2_to_usb3();
		puts("1");
		return 0;
	}
	else if (!strcmp(base, "start_repeater")) {
		start_repeater();
		return 0;
	}
#endif
	else if (!strcmp(base, "ATE")) {
		if ( argc == 2 || argc == 3 || argc == 4) {
			asus_ate_command(argv[1], argv[2], argv[3]);
		}
		else
			printf("ATE_ERROR\n");
		return 0;
	}
#if defined(RTCONFIG_DSL)
	else if (!strcmp(base, "asustest")) {
		if ( argc == 2 || argc == 3) {
			asustest_command(argv[1], argv[2]);
		}
		else
			printf("asustest:parameter error\n");
		return 0;
	}
#endif

#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_ALPINE)
	else if (!strcmp(base, "FWRITE")) {
		if (argc == 3)
			return FWRITE(argv[1], argv[2]);
		else
		return 0;
	}
	else if (!strcmp(base, "FREAD")) {
		if (argc == 3)
		{
			unsigned int addr;
			int len;
			addr = strtoul(argv[1], NULL, 16);
			if (argv[2][0] == '0' && argv[2][1] == 'x')
				len  = (int) strtoul(argv[2], NULL, 16);
			else
				len  = (int) strtoul(argv[2], NULL, 10);

			if (len > 0)
				return FREAD(addr, len);
		}
		printf("ATE_ERROR\n");
		return 0;
	}
#endif	/* RTCONFIG_RALINK || RTCONFIG_QCA */
#if defined(RTCONFIG_RALINK)
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(base, "asuscfe_5g")) {
		if (argc == 2)
			return asuscfe(argv[1], WIF_5G);
		else
			return EINVAL;
	}
#endif	/* RTCONFIG_HAS_5G */
	else if (!strcmp(base, "asuscfe_2g")) {
		if (argc == 2)
			return asuscfe(argv[1], WIF_2G);
		else
			return EINVAL;
	}
	else if (!strcmp(base, "stainfo_2g")) {
		return stainfo(0);
	}
#if defined(RTCONFIG_HAS_5G)
	else if (!strcmp(base, "stainfo_5g")) {
		return stainfo(1);
	}
#endif	/* RTCONFIG_HAS_5G */
#ifdef RTCONFIG_DSL
	else if (!strcmp(base, "gen_ralink_config")) {
		if (argc != 3) {
			printf("Usage: gen_ralink_config [band] [is_iNIC]\n");
			return 0;
		}
		return gen_ralink_config(atoi(argv[1]), atoi(argv[2]));
	}
#endif
#endif

#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK)
	else if (!strcmp(base, "set_factory_mode")) {
		set_factory_mode();
		return 0;
	}
#endif
#if defined(RTCONFIG_OPENPLUS_TFAT) || defined(RTCONFIG_OPENPLUSPARAGON_NTFS) || defined(RTCONFIG_OPENPLUSTUXERA_NTFS) || defined(RTCONFIG_OPENPLUSPARAGON_HFS) || defined(RTCONFIG_OPENPLUSTUXERA_HFS)
	else if(!strcmp(base, "set_fs_coexist")){
		set_fs_coexist();
		puts("1");
		return 0;
	}
#endif
	else if (!strcmp(base, "run_telnetd")) {
		_start_telnetd(1);
		return 0;
	}
#ifdef RTCONFIG_SSH
	else if (!strcmp(base, "run_sshd")) {
		start_sshd();
		return 0;
	}
#endif
#ifdef RTCONFIG_WTFAST
	else if (!strcmp(base, "run_wtfast")) {
		start_wtfast();
		return 0;
	}
#endif
#if defined(RTCONFIG_PPTPD) || defined(RTCONFIG_ACCEL_PPTPD)
	else if (!strcmp(base, "run_pptpd")) {
		start_pptpd();
		return 0;
	}
#endif
#ifdef RTCONFIG_PARENTALCTRL
	else if (!strcmp(base, "pc")) {
		pc_main(argc, argv);
		return 0;
	}
	else if (!strcmp(base, "pc_block")) {
		pc_block_main(argc, argv);
		return 0;
	}
	else if (!strcmp(base, "pc_tmp")) {
		pc_tmp_main(argc, argv);
		return 0;
	}
#endif
#if defined(CONFIG_BCMWL5) \
		|| (defined(RTCONFIG_RALINK) && defined(RTCONFIG_WIRELESSREPEATER)) \
		|| defined(RTCONFIG_QCA) || defined(RTCONFIG_REALTEK) \
		|| defined(RTCONFIG_QSR10G) || defined(RTCONFIG_LANTIQ)
	else if (!strcmp(base, "wlcscan")) {
		return wlcscan_main();
	}
#if defined(RTCONFIG_WLCSCAN_RSSI)
	else if (!strcmp(base, "wlcscan_ssid_rssi")) {
		return wlcscan_ssid_rssi(atoi(argv[1]), argv[2]);
	}
#endif
#ifdef RTCONFIG_QTN
	else if (!strcmp(base, "start_psta_qtn")) {
		return start_psta_qtn();
	}
	else if (!strcmp(base, "start_ap_qtn")) {
		return start_ap_qtn();
	}
	else if (!strcmp(base, "start_channel_scan_qtn")) {
		return start_channel_scan_qtn(1);
	}
	else if (!strcmp(base, "start_qtn_stateless")) {
		return gen_stateless_conf();
	}
	else if (!strcmp(base, "restart_qtn")) {
		return reset_qtn(0);
	}
#endif
#endif
#if defined(CONFIG_BCMWL5) && !defined(HND_ROUTER) && defined(RTCONFIG_DUALWAN)
	else if (!strcmp(base, "dualwan")) {
		dualwan_control(argc, argv);
	}
#endif
#ifdef RTCONFIG_WIRELESSREPEATER
	else if (!strcmp(base, "wlcconnect")) {
		return wlcconnect_main();
	}
#endif
#ifdef RTCONFIG_AMAS
#if !defined(RTCONFIG_DISABLE_REPEATER_UI)
	else if (!strcmp(base, "amas_wlcconnect")) {
		return amas_wlcconnect_main();
	}
	else if (!strcmp(base, "amas_bhctrl")) {
		return amas_bhctrl_main();
	}
#endif	
	else if (!strcmp(base, "amas_lanctrl")) {
		return amas_lanctrl_main();
	}
#endif	
#ifdef CONFIG_BCMWL5
	else if (!strcmp(base, "setup_dnsmq")) {
		if (argc != 2)
			return 0;

		return setup_dnsmq(atoi(argv[1]));
	}
#endif
	else if (!strcmp(base, "add_multi_routes")) {
		return add_multi_routes();
	}
	else if (!strcmp(base, "led_ctrl")) {
		if (argc != 3)
			return 0;

		return(led_control_atomic(atoi(argv[1]), atoi(argv[2])));
	}
#ifdef HND_ROUTER
	else if (!strcmp(base, "hnd-erase")) {
		if (argv[1] && (!strcmp(argv[1], "nvram"))) {
			nvram_set("restore_defaults", "1");
			nvram_commit();
			nvram_set(ASUS_STOP_COMMIT, "1");
			return hnd_nvram_erase();
		}
	}
	else if (!strcmp(base, "hnd-write")) {
		if (argc >= 2) {
			return bca_sys_upgrade(argv[1]);
		} else {
			_dprintf("%s@%d *** Error argc=%d\n", __FUNCTION__, __LINE__, argc);
			return EINVAL;
		}
	}
	else if (!strcmp(base, "mtd_erase_image_update")) {
		mtd_erase_image_update();
		return 0;
	}
#else
	else if (!strcmp(base, "nvram_erase")) {
		nvram_set(ASUS_STOP_COMMIT, "1");
		erase_nvram();
		return 0;
	}
#ifdef RTCONFIG_BCMARM
#if defined(RTAC1200G) || defined(RTAC1200GP)
	/* mtd-erase2 [device] */
	else if (!strcmp(base, "mtd-erase2")) {
		if (argv[1] && ((!strcmp(argv[1], "boot")) ||
				(!strcmp(argv[1], "linux")) ||
				(!strcmp(argv[1], "rootfs")) ||
				(!strcmp(argv[1], "nvram")))) {
			return mtd_erase(argv[1]);
		} else {
			fprintf(stderr, "usage: mtd-erase2 [device]\n");
			return EINVAL;
		}
	}
	/* mtd-write2 [path] [device] */
	else if (!strcmp(base, "mtd-write2")) {
		if (argc >= 3)
			return mtd_write(argv[1], argv[2]);
		else {
			fprintf(stderr, "usage: mtd-write2 [path] [device]\n");
			return EINVAL;
		}
	}
#else
	/* mtd-erase2 [device] */
	else if (!strcmp(base, "mtd-erase2")) {
		if (argv[1] && ((!strcmp(argv[1], "boot")) ||
				(!strcmp(argv[1], "linux")) ||
				(!strcmp(argv[1], "linux2")) ||
				(!strcmp(argv[1], "rootfs")) ||
				(!strcmp(argv[1], "rootfs2")) ||
				(!strcmp(argv[1], "brcmnand")) ||
				(!strcmp(argv[1], "nvram")))) {
			return mtd_erase(argv[1]);
		} else {
			fprintf(stderr, "usage: mtd-erase2 [device]\n");
			return EINVAL;
		}
	}
	/* mtd-write2 [path] [device] */
	else if (!strcmp(base, "mtd-write2")) {
		if (argc >= 3)
			return mtd_write(argv[1], argv[2]);
		else {
			fprintf(stderr, "usage: mtd-write2 [path] [device]\n");
			return EINVAL;
		}
	}
#endif
#endif
#endif
	else if (!strcmp(base, "test_endian")) {
		int num = 0x04030201;
		char c = *(char *)(&num);

		if (c == 0x04 || c == 0x01) {
			if (c == 0x04)
				printf("Big.\n");
			else
				printf("Little.\n");
		}
		else
			printf("test error!\n");

		return 0;
	}
	else if (!strcmp(base, "free_caches")) {
		int c;
		unsigned int test_num;
		char *set_value = NULL;
		int clean_time = 1;
		int threshold = 0;

		if (argc) {
			while((c = getopt(argc, argv, "c:w:t:")) != -1) {
				switch(c) {
					case 'c': // set the clean-cache mode: 0~3.
						test_num = strtol(optarg, NULL, 10);
			if (test_num == LONG_MIN || test_num == LONG_MAX) {
				_dprintf("ERROR: unknown value %s...\n", optarg);
							return 0;
						}

						if (test_num < 0 || test_num > 3) {
							_dprintf("ERROR: the value %s was over the range...\n", optarg);
							return 0;
						}

						set_value = optarg;

						break;
					case 'w': // set the waited time for cleaning.
						test_num = strtol(optarg, NULL, 10);
			if (test_num < 0 || test_num == LONG_MIN || test_num == LONG_MAX) {
				_dprintf("ERROR: unknown value %s...\n", optarg);
							return 0;
						}

						clean_time = test_num;

						break;
					case 't': // set the waited time for cleaning.
						test_num = strtol(optarg, NULL, 10);
			if (test_num < 0 || test_num == LONG_MIN || test_num == LONG_MAX) {
				_dprintf("ERROR: unknown value %s...\n", optarg);
							return 0;
						}

						threshold = test_num;

						break;
					default:
						fprintf(stderr, "Usage: free_caches [ -c clean_mode ] [ -w clean_time ] [ -t threshold ]\n");
						break;
				}
			}
		}

		if (!set_value)
			set_value = FREE_MEM_PAGE;

		free_caches(set_value, clean_time, threshold);

		return 0;
	}
#ifdef RTCONFIG_USB_MODEM
	else if (!strcmp(base, "write_3g_ppp_conf")) {
		int modem_unit;

		if (argc != 1 && argc != 2) {
			printf("Usage: %s <modem_unit>\n", argv[0]);
			return 0;
		}

		if (argc == 2)
			modem_unit = atoi(argv[1]);
		else
			modem_unit = 0;

		return write_3g_ppp_conf(modem_unit);
	}
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
	else if (!strcmp(base, "lplus")) {
		if (argc != 3) {
			printf("Usage: %s <integer1> <integer2>.\n", argv[0]);
			return 0;
		}

		unsigned long long int1 = strtoull(argv[1], NULL, 10);
		unsigned long long int2 = strtoull(argv[2], NULL, 10);
		unsigned long long total = int1+int2;

		printf("%llu", total);

		return 0;
	}
	else if (!strcmp(base, "lminus")) {
		if (argc != 3) {
			printf("Usage: %s <integer1> <integer2>.\n", argv[0]);
			return 0;
		}

		unsigned long long int1 = strtoull(argv[1], NULL, 10);
		unsigned long long int2 = strtoull(argv[2], NULL, 10);
		unsigned long long total = int1-int2;

		printf("%llu", total);

		return 0;
	}
#endif
#endif
#ifdef RTCONFIG_TOR
	else if (!strcmp(base, "start_tor")) {
		start_Tor_proxy();
		return 0;
	}
#endif
#ifdef RTCONFIG_NEW_USER_LOW_RSSI
	else if (!strcmp(base, "start_roamast")) {
		start_roamast();
		return 0;
	}
#endif
#if defined(RTCONFIG_KEY_GUARD)
	else if (!strcmp(base, "start_keyguard")) {
		start_keyguard();
		return 0;
	}
#endif
#if defined(RTCONFIG_BT_CONN)
	else if (!strcmp(base, "start_bluetooth_service")) {
		start_bluetooth_service();
		return 0;
	}
#endif	/* RTCONFIG_BT_CONN */
#if defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA)
	else if (!strcmp(base, "dump_powertable")) {
		if (!IS_ATE_FACTORY_MODE())
			return 0;
		dump_powertable();
		return 0;
	}
#endif
#if defined(RTCONFIG_RALINK)
	else if (!strcmp(base, "dump_txbftable")) {
		if (!IS_ATE_FACTORY_MODE())
			return 0;
		dump_txbftable();
		return 0;
	}
#endif
	printf("Unknown applet: %s\n", base);
	return 0;
}
