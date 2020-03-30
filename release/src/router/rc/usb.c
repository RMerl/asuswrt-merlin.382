/*

	USB Support

*/

#include <rc.h>

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
#include <PMS_DBAPIs.h>
#endif

#ifdef RTCONFIG_USB
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <proto/ethernet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/mount.h>
#include <mntent.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/swap.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <rc_event.h>
#include <shared.h>

#include <disk_io_tools.h>
#include <disk_share.h>
#include <disk_initial.h>

char *usb_dev_file = "/proc/bus/usb/devices";

#define ERR_DISK_FS_RDONLY "1"
#define ERR_DISK_SCSI_KILL "10"

#define USB_CLS_PER_INTERFACE	0	/* for DeviceClass */
#define USB_CLS_AUDIO		1
#define USB_CLS_COMM		2
#define USB_CLS_HID		3
#define USB_CLS_PHYSICAL	5
#define USB_CLS_STILL_IMAGE	6
#define USB_CLS_PRINTER		7
#define USB_CLS_MASS_STORAGE	8
#define USB_CLS_HUB		9
#define USB_CLS_CDC_DATA	0x0a
#define USB_CLS_CSCID		0x0b	/* chip+ smart card */
#define USB_CLS_CONTENT_SEC	0x0d	/* content security */
#define USB_CLS_VIDEO		0x0e
#define USB_CLS_WIRELESS_CONTROLLER	0xe0
#define USB_CLS_MISC		0xef
#define USB_CLS_APP_SPEC	0xfe
#define USB_CLS_VENDOR_SPEC	0xff
#define USB_CLS_3GDEV		0x35

#define OP_MOUNT		1
#define OP_UMOUNT		2
#define OP_SETNVRAM		3

char *find_sddev_by_mountpoint(char *mountpoint);

/* Adjust bdflush parameters.
 * Do this here, because Tomato doesn't have the sysctl command.
 * With these values, a disk block should be written to disk within 2 seconds.
 */
#ifdef LINUX26
void tune_bdflush(void)
{
	f_write_string("/proc/sys/vm/dirty_writeback_centisecs", "200", 0, 0);

#ifndef RTCONFIG_BCMARM
	f_write_string("/proc/sys/vm/dirty_expire_centisecs", "200", 0, 0);
#else
	printf("no tune_bdflush\n");
#endif
}
#else
#include <sys/kdaemon.h>
#define SET_PARM(n) (n * 2 | 1)
void tune_bdflush(void)
{
	bdflush(SET_PARM(5), 100);
	bdflush(SET_PARM(6), 100);
	bdflush(SET_PARM(8), 0);
}
#endif // LINUX26

#define NFS_EXPORT     "/etc/exports"

#ifdef RTCONFIG_USB_PRINTER
void
start_lpd()
{
	pid_t pid;
	char *lpd_argv[] = { "lpd", NULL, NULL };

	if(getpid()!=1) {
		notify_rc("start_lpd");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	if (!pids("lpd"))
	{
		unlink("/var/run/lpdparent.pid");

		if (is_routing_enabled())
			lpd_argv[1] = nvram_safe_get("lan_ifname");
		//return xstart("lpd");
		_eval(lpd_argv, NULL, 0, &pid);
	}
}

void
stop_lpd()
{
	if(getpid()!=1) {
		notify_rc("stop_lpd");
		return;
	}

	if (pids("lpd"))
	{
		killall_tk("lpd");
		unlink("/var/run/lpdparent.pid");
	}
}

void
start_u2ec()
{
	pid_t pid;
	char *u2ec_argv[] = { "u2ec", NULL };

	if(getpid()!=1) {
		notify_rc("start_u2ec");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	if (!pids("u2ec"))
	{
		unlink("/var/run/u2ec.pid");
		_eval(u2ec_argv, NULL, 0, &pid);

		nvram_set("apps_u2ec_ex", "1");
	}
}

void
stop_u2ec()
{
#if defined(RTCONFIG_QCA)
	nvram_commit();
#endif
	if(getpid()!=1) {
		notify_rc("stop_u2ec");
		return;
	}

	if (pids("u2ec")){
		killall_tk("u2ec");
		unlink("/var/run/u2ec.pid");
	}
}
#endif

FILE* fopen_or_warn(const char *path, const char *mode)
{
	FILE *fp = fopen(path, mode);

	if (!fp)
	{
		dbg("hotplug USB: No such file or directory: %s\n", path);
		errno = 0;
		return NULL;
	}

	return fp;
}

#ifdef DLM

int
test_user(char *target, char *pattern)
{
	char s[384];
	char p[32];
	char *start;
	char *pp;
	strcpy(s, target);
	strcpy(p, pattern);
	start = s;
	while ((pp=strchr(start, ';')) != NULL)
	{
		*pp='\0';
		if (! strcmp(start, p))
			return 1;
		start=pp+1;
	}
	return 0;
}

int
fill_smbpasswd_input_file(const char *passwd)
{
	FILE *fp;

	unlink("/tmp/smbpasswd");
	fp = fopen("/tmp/smbpasswd", "w");

	if (fp && passwd)
	{
		fprintf(fp,"%s\n", passwd);
		fprintf(fp,"%s\n", passwd);
		fclose(fp);

		return 1;
	}
	else
		return 0;
}
#endif

void add_usb_host_module(void)
{
#if defined(HND_ROUTER)
	tweak_usb_affinity(1);
#endif
#if defined(RTCONFIG_USB_XHCI)
#if defined(RTN65U) || defined(RTCONFIG_QCA) || defined(RTAC85U)
	char *u3_param = "u3intf=0";
#endif
#endif
	char param[32];
	int i;

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(3,2,0)
	modprobe(USBCOMMON_MOD);
#endif
	modprobe(USBCORE_MOD);

#if defined(RTCONFIG_USB_XHCI)
#if defined(RTN65U) || defined(RTCONFIG_QCA) || defined(RTAC85U)
	if (nvram_get_int("usb_usb3") == 1)
		u3_param = "u3intf=1";
#if !defined(RTCONFIG_SOC_IPQ40XX)
	modprobe(USB30_MOD, u3_param);
#endif
#if defined(RTCONFIG_SOC_IPQ8064)
	modprobe("udc-core");
	modprobe("dwc3-ipq");
#endif

#elif defined(RTCONFIG_XHCIMODE)
	modprobe(USB30_MOD);
#elif defined(RTCONFIG_LANTIQ)
	modprobe(USB30_MOD);
#elif defined(RTCONFIG_ALPINE)
	modprobe(USB30_MOD);
#else
	if (nvram_get_int("usb_usb3") == 1) {
		modprobe(USB30_MOD);
#ifdef HND_ROUTER
		modprobe("xhci-plat-hcd");
#endif
	}
#endif
#endif // RTCONFIG_USB_XHCI

	/* if enabled, force USB2 before USB1.1 */
	if (nvram_get_int("usb_usb2") == 1) {
#if defined(RTN56UB1) || defined(RTN56UB2) || defined(RTAC1200GA1) || defined(RTAC1200GU)
		modprobe(USB20_MOD);
#else
		i = nvram_get_int("usb_irq_thresh");
		if ((i < 0) || (i > 6))
			i = 0;
		sprintf(param, "log2_irq_thresh=%d", i);
		modprobe(USB20_MOD, param);
#ifdef HND_ROUTER
		modprobe("ehci-platform");
#endif
#endif
	}

#ifndef RTCONFIG_LANTIQ
	// BlueCave: ehci will handle USB 2.0 & 1.x.
	if (nvram_get_int("usb_uhci") == 1) {
		modprobe(USBUHCI_MOD);
	}
	if (nvram_get_int("usb_ohci") == 1) {
		modprobe(USBOHCI_MOD);
#ifdef HND_ROUTER
		modprobe("ohci-platform");
#endif
	}
#endif

#if defined(RTCONFIG_SOC_IPQ40XX)
	modprobe(USB_PHY1);
	modprobe(USB_PHY2);
	modprobe(USB_DWC3_IPQ);
	modprobe(USB_DWC3);
#if defined(RTCONFIG_USB_XHCI)
	modprobe(USB30_MOD, u3_param);

	/* workaround for some USB dongle */
	modprobe_r(USB_DWC3_IPQ);
	modprobe(USB_DWC3_IPQ);
#endif
#endif

#ifdef HND_ROUTER
	if(!nvram_get_int("usb_usb3")){
		modprobe(USB30_MOD);
		modprobe("xhci-plat-hcd");
	}

	modprobe("bcm_usb");

	if(!nvram_get_int("usb_usb3")){
		modprobe_r("xhci-plat-hcd");
		modprobe_r(USB30_MOD);
	}
#endif
}

#ifdef RTCONFIG_USB_MODEM
static int usb_modem_modules_loaded = 0;

void add_usb_modem_modules(void)
{
	if (usb_modem_modules_loaded)
		return;
	usb_modem_modules_loaded = 1;

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(4,1,0)
	modprobe("mii"); // for usbnet.
#endif
	modprobe("usbnet");
#ifdef RTCONFIG_INTERNAL_GOBI
	if(nvram_get_int("usb_gobi"))
		modprobe("gobi");
#endif
	modprobe("cdc-acm");
#if !defined(RTCONFIG_INTERNAL_GOBI) || defined(RTCONFIG_USB_MULTIMODEM)
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(4,1,0)
	modprobe("libphy");
#endif
	modprobe("asix");
	modprobe("ax88179_178a");
	modprobe("cdc_ether");
	modprobe("rndis_host");
	modprobe("cdc_ncm");
	modprobe("cdc_wdm");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(4,1,0)
	modprobe("huawei_cdc_ncm"); // depend on cdc_ncm, cdc_wdm.
#endif
	if(nvram_get_int("usb_qmi"))
		modprobe("qmi_wwan");
	modprobe("cdc_mbim");
#endif
}

void remove_usb_modem_modules(void)
{
#ifdef RTCONFIG_USB_BECEEM
	modprobe_r("drxvi314");
#endif
#ifdef RTCONFIG_INTERNAL_GOBI
	killall_tk("gobi");
	modprobe_r("gobi");
#endif
#if !defined(RTCONFIG_INTERNAL_GOBI) || defined(RTCONFIG_USB_MULTIMODEM)
	modprobe_r("cdc_mbim");
	modprobe_r("qmi_wwan");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(4,1,0)
	modprobe_r("huawei_cdc_ncm");
#endif
	modprobe_r("cdc_wdm");
	modprobe_r("cdc_ncm");
	modprobe_r("rndis_host");
	modprobe_r("cdc_ether");
	modprobe_r("ax88179_178a");
	modprobe_r("asix");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(4,1,0)
	modprobe_r("libphy");
#endif
	modprobe_r("option");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
	modprobe_r("usb_wwan");
#endif
	modprobe_r("usbserial");
#endif
	modprobe_r("cdc-acm");
	modprobe_r("usbnet");
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(4,1,0)
	modprobe_r("mii"); // for usbnet.
#endif
	modprobe_r("sr_mod");
	modprobe_r("cdrom");

	usb_modem_modules_loaded = 0;
}

#ifdef RTCONFIG_INTERNAL_GOBI
#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
int modem_data_update(int sim_order){
	int modem_unit, wan_unit;
	char tmp[100], prefix[32];
	char *modem_dev;
	unsigned long long rx_old, tx_old;
	unsigned long long rx_reset, tx_reset;
	unsigned long long rx_new, tx_new;
	unsigned long long rx_now, tx_now;
	char path[256];

	printf("Updating %dth net traffic:\n", sim_order);

	for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
		usb_modem_prefix(modem_unit, prefix, sizeof(prefix));
		modem_dev = nvram_safe_get(strcat_r(prefix, "act_dev", tmp));
		wan_unit = get_wan_unit(modem_dev);
		if(wan_unit == -1)
			continue;

		rx_old = strtoull(nvram_safe_get("modem_bytes_rx"), NULL, 10);
		tx_old = strtoull(nvram_safe_get("modem_bytes_tx"), NULL, 10);

		printf("rx_old=%llu\n"
				"tx_old=%llu\n",
				rx_old, tx_old);

		rx_reset = strtoull(nvram_safe_get("modem_bytes_rx_reset"), NULL, 10);
		if(rx_reset < 0)
			rx_reset = 0;
		tx_reset = strtoull(nvram_safe_get("modem_bytes_tx_reset"), NULL, 10);
		if(tx_reset < 0)
			tx_reset = 0;

		printf("rx_reset=%llu\n"
				"tx_reset=%llu\n",
				rx_reset, tx_reset);

		snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", modem_dev);
		f_read_string(path, tmp, sizeof(tmp));
		if(strlen(tmp) > 0)
			tmp[strlen(tmp)-1] = '\0';
		rx_new = strtoull(tmp, NULL, 10);
		snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", modem_dev);
		f_read_string(path, tmp, sizeof(tmp));
		if(strlen(tmp) > 0)
			tmp[strlen(tmp)-1] = '\0';
		tx_new = strtoull(tmp, NULL, 10);

		printf("rx_new=%llu\n"
				"tx_new=%llu\n",
				rx_new, tx_new);

		rx_now = rx_new+rx_old;
		if(rx_now >= rx_reset)
			rx_now -= rx_reset;

		tx_now = tx_new+tx_old;
		if(tx_now >= tx_reset)
			tx_now -= tx_reset;

		printf("rx_now=%llu\n"
				"tx_now=%llu\n",
				rx_now, tx_now);

		snprintf(tmp, sizeof(tmp), "%llu", rx_now);
		nvram_set("modem_bytes_rx", tmp);
		snprintf(tmp, sizeof(tmp), "%llu", tx_now);
		nvram_set("modem_bytes_tx", tmp);

		snprintf(tmp, sizeof(tmp), "%llu", rx_new);
		nvram_set("modem_bytes_rx_reset", tmp);
		snprintf(tmp, sizeof(tmp), "%llu", tx_new);
		nvram_set("modem_bytes_tx_reset", tmp);
	}

	return 0;
}

int modem_data_reset(int sim_order){
	int modem_unit, wan_unit;
	char tmp[100], prefix[32];
	char *modem_dev;
	char rx_new[32], tx_new[32];
	char path[256];
	time_t now;
	char timebuf[32];

	printf("Reseting %dth net traffic:\n", sim_order);

	for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
		usb_modem_prefix(modem_unit, prefix, sizeof(prefix));
		modem_dev = nvram_safe_get(strcat_r(prefix, "act_dev", tmp));
		wan_unit = get_wan_unit(modem_dev);
		if(wan_unit == -1)
			continue;

		time(&now);
		snprintf(timebuf, sizeof(timebuf), "%d", (int)now);

		snprintf(path, sizeof(path), "/jffs/sim/%d/modem_bytes_data_start", sim_order);
		f_write_string(path, timebuf, 0, 0);
		nvram_set("modem_bytes_data_start", timebuf);

		snprintf(path, sizeof(path), "/jffs/sim/%d/modem_bytes_rx", sim_order);
		f_write_string(path, "0", 0, 0);
		nvram_set("modem_bytes_rx", "0");
		snprintf(path, sizeof(path), "/jffs/sim/%d/modem_bytes_tx", sim_order);
		f_write_string(path, "0", 0, 0);
		nvram_set("modem_bytes_tx", "0");

		snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/rx_bytes", modem_dev);
		f_read_string(path, rx_new, sizeof(rx_new));
		if(strlen(rx_new) > 0)
			rx_new[strlen(rx_new)-1] = '\0';
		snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/tx_bytes", modem_dev);
		f_read_string(path, tx_new, sizeof(tx_new));
		if(strlen(rx_new) > 0)
			tx_new[strlen(tx_new)-1] = '\0';

		printf("rx_new=%s\n"
				"tx_new=%s\n",
				rx_new, tx_new);

		nvram_set("modem_bytes_rx_reset", rx_new);
		nvram_set("modem_bytes_tx_reset", tx_new);
	}

	return 0;
}

int modem_data_save(int sim_order){
	char *rx_now = nvram_safe_get("modem_bytes_rx");
	char *tx_now = nvram_safe_get("modem_bytes_tx");
	char path[256];

	printf("Saving %dth net traffic:\n"
			"rx_now=%s\n"
			"tx_now=%s\n",
			sim_order, rx_now, tx_now);

	snprintf(path, sizeof(path), "/jffs/sim/%d/modem_bytes_rx", sim_order);
	f_write_string(path, rx_now, 0, 0);
	snprintf(path, sizeof(path), "/jffs/sim/%d/modem_bytes_tx", sim_order);
	f_write_string(path, tx_now, 0, 0);

	return 0;
}

int modem_data_usage(){
	printf("\nUsage: modem_data [OPTION]...\n\n"
			"Options:\n"
			"\t\t -o \t Set the SIM's order. 1 ~ 9\n"
			"\t\t -u \t Update the net traffic\n"
			"\t\t -r \t Reset the net traffic\n"
			"\t\t -s \t Save the net traffic\n"
			"\t\t -h \t Display this help\n");

	return 0;
}

int modem_data_main(int argc, char *argv[]){
	int c;
	int sim_order = 1;

	if(argc == 1){
		modem_data_usage();
		return 0;
	}

	while((c = getopt(argc, argv, "o:ursh")) != -1){
		switch(c){
			case 'o':
				sim_order = strtod(optarg, NULL);
				if(sim_order < 1 || sim_order > 9)
					sim_order = 1;

				printf("modem data: sim_order=%d.\n", sim_order);
				break;
			case 'u':
				printf("modem data: Updating the %dth net traffic...\n", sim_order);
				modem_data_update(sim_order);
				break;
			case 'r':
				printf("modem data: Reseting the %dth net traffic...\n", sim_order);
				modem_data_reset(sim_order);
				break;
			case 's':
				printf("modem data: Saving the %dth net traffic...\n", sim_order);
				modem_data_save(sim_order);
				break;
			case 'h':
				modem_data_usage();
				break;
			default:
				printf("Invalid parameters: '%c'", c);
				modem_data_usage();
				break;
		}
	}

	return 0;
}
#endif
#endif // RTCONFIG_INTERNAL_GOBI
#endif // RTCONFIG_USB_MODEM

void start_usb(int orig)
{
	char param[32];
	int i;

	_dprintf("%s\n", __func__);

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

#if defined(RTCONFIG_SOC_IPQ40XX)
	_dprintf("insmod dakota usb module....\n");
	modprobe(USB_PHY1);
	modprobe(USB_PHY2);
	modprobe(USB_DWC3_IPQ);
	modprobe(USB_DWC3);
#endif

	tune_bdflush();

	if (nvram_get_int("usb_enable")) {
		if(orig)
			add_usb_host_module();

#ifdef RTCONFIG_USB_MODEM
		if(orig)
			add_usb_modem_modules();
#endif

		/* mount usb device filesystem */
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(3,2,0)
		mount("debugfs", "/sys/kernel/debug", "debugfs", MS_MGC_VAL, NULL);
#if defined(RTCONFIG_RALINK) && defined(LINUX30)
		mount(USBFS, "/proc/bus/usb", USBFS, MS_MGC_VAL, NULL);
#endif		
#else
		mount(USBFS, "/proc/bus/usb", USBFS, MS_MGC_VAL, NULL);
#endif

#ifdef LINUX26
		if ((i = nvram_get_int("led_usb_gpio")) != 255) {
			modprobe("ledtrig-usbdev");
			modprobe("leds-usb");
			sprintf(param, "%d", i);
			f_write_string("/sys/class/leds/usb-led/gpio_pin", param, 0, 0);
			f_write_string("/sys/class/leds/usb-led/device_name", "1-1", 0, 0);
		}

		/* big enough for minidlna to minitor all media files? */
		f_write_string("/proc/sys/fs/inotify/max_user_watches", "100000", 0, 0);
#endif

#if defined(RTCONFIG_SAMBASRV) || defined(RTCONFIG_FTP)
		if (nvram_get_int("usb_storage")) {
			/* insert scsi and storage modules before usb drivers */
			modprobe(SCSI_MOD);
#ifdef LINUX26
			modprobe(SCSI_WAIT_MOD);
#endif
			modprobe(SD_MOD);
			modprobe(SG_MOD);
			modprobe(USBSTORAGE_MOD);

			if (nvram_get_int("usb_fs_ext3")) {
#ifdef LINUX26
				modprobe("mbcache");	// used by ext2/ext3
#endif
				/* insert ext3 first so that lazy mount tries ext4 before ext2 */
				modprobe("jbd");
				modprobe("ext3");
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_EXT4FS)
				modprobe("ext4");
#endif
				modprobe("ext2");
			}

			if (nvram_get_int("usb_fs_fat")) {
#ifdef RTCONFIG_OPENPLUS_TFAT
				if(nvram_match("usb_fatfs_mod", "tuxera"))
					modprobe("tfat");
				else{
					modprobe("fat");
					modprobe("vfat");
				}
#elif defined(RTCONFIG_TFAT)
				modprobe("tfat");
#else
				modprobe("fat");
				modprobe("vfat");
#endif
			}
#ifdef RTCONFIG_NTFS
			if(nvram_get_int("usb_fs_ntfs")){
#ifdef RTCONFIG_TUXERA_NTFS
#if defined(RTCONFIG_OPENPLUSTUXERA_NTFS)
				if(nvram_match("usb_ntfs_mod", "tuxera"))
#endif
				modprobe("tntfs");
#elif defined(RTCONFIG_PARAGON_NTFS)
#if defined(RTCONFIG_OPENPLUSPARAGON_NTFS)
				if(nvram_match("usb_ntfs_mod", "paragon"))
#endif
#ifdef RTCONFIG_UFSD_DEBUG
				modprobe("ufsd_debug");
#else
				modprobe("ufsd");
#endif
#endif
			}
#endif
#ifdef RTCONFIG_HFS
			if(nvram_get_int("usb_fs_hfs")){
#ifdef RTCONFIG_TUXERA_HFS
#if defined(RTCONFIG_OPENPLUSTUXERA_HFS)
				if(nvram_invmatch("usb_hfs_mod", "tuxera")){
					modprobe("hfs");
					modprobe("hfsplus");
				}
				else
#endif
				modprobe("thfsplus");
#elif defined(RTCONFIG_PARAGON_HFS)
#if defined(RTCONFIG_OPENPLUSPARAGON_HFS)
				if(nvram_invmatch("usb_hfs_mod", "paragon")){
					modprobe("hfs");
					modprobe("hfsplus");
				}
				else
#endif
#ifdef RTCONFIG_UFSD_DEBUG
				modprobe("ufsd_debug");
#else
				modprobe("ufsd");
#endif
#else
				modprobe("hfs");
				modprobe("hfsplus");
#endif
			}
#endif
		}
#endif

#ifdef RTCONFIG_USB_PRINTER
		if (nvram_get_int("usb_printer")) {
			symlink("/dev/usb", "/dev/printers");
			modprobe(USBPRINTER_MOD);
		}
#endif
#if defined(RTCONFIG_BT_CONN)
		modprobe("btusb");
		modprobe("ath3k");
#if defined(MAPAC1300) || defined(MAPAC2200) || defined(VZWAC1300)
		if (nvram_match("x_Setting", "0"))
			system("/usr/bin/btchk.sh &"); /* workaround script */
#endif
#endif	/* RTCONFIG_BT_CONN */
#ifdef HND_ROUTER
		modprobe("btusbdrv");
#endif
	}
}

#ifdef RTCONFIG_SOC_IPQ40XX
void remove_dakota_usb_modules(void)
{
	modprobe_r(USB_DWC3);
	modprobe_r(USB_DWC3_IPQ);
	modprobe_r(USB_PHY2);
	modprobe_r(USB_PHY1);
}
#endif

#ifdef RTCONFIG_USB_PRINTER
void remove_usb_prn_module(void)
{
	modprobe_r(USBPRINTER_MOD);
}
#endif

void remove_usb_storage_module(void)
{
#if defined(RTCONFIG_SAMBASRV) || defined(RTCONFIG_FTP)
	modprobe_r("ext2");
	modprobe_r("ext3");
	modprobe_r("jbd");
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_EXT4FS)
	modprobe_r("ext4");
	modprobe_r("jbd2");
#endif
#ifdef LINUX26
	modprobe_r("mbcache");
#endif
#ifdef RTCONFIG_OPENPLUS_TFAT
	if(nvram_match("usb_fatfs_mod", "tuxera"))
		modprobe_r("tfat");
	else{
		modprobe_r("vfat");
		modprobe_r("fat");
	}
#elif defined(RTCONFIG_TFAT)
	modprobe_r("tfat");
#else
	modprobe_r("vfat");
	modprobe_r("fat");
#endif
#ifdef RTCONFIG_NTFS
#ifdef RTCONFIG_TUXERA_NTFS
#if defined(RTCONFIG_OPENPLUSTUXERA_NTFS)
	if(nvram_match("usb_ntfs_mod", "tuxera"))
#endif
	modprobe_r("tntfs");
#elif defined(RTCONFIG_PARAGON_NTFS)
#if defined(RTCONFIG_OPENPLUSPARAGON_NTFS)
	if(nvram_match("usb_ntfs_mod", "paragon"))
#endif
	{
#ifdef RTCONFIG_UFSD_DEBUG
	modprobe_r("ufsd_debug");
	modprobe_r("jnl_debug");
#else
	modprobe_r("ufsd");
	modprobe_r("jnl");
#endif
	}
#endif
#endif
#ifdef RTCONFIG_HFS
#ifdef RTCONFIG_TUXERA_HFS
#if defined(RTCONFIG_OPENPLUSTUXERA_HFS)
	if(nvram_match("usb_hfs_mod", "tuxera"))
#endif
	modprobe_r("thfsplus");
#elif defined(RTCONFIG_PARAGON_HFS)
#if defined(RTCONFIG_OPENPLUSPARAGON_HFS)
	if(nvram_match("usb_hfs_mod", "paragon"))
#endif
	{
#ifdef RTCONFIG_UFSD_DEBUG
	modprobe_r("ufsd_debug");
	modprobe_r("jnl_debug");
#else
	modprobe_r("ufsd");
	modprobe_r("jnl");
#endif
	}
#endif
#endif
	modprobe_r("fuse");
	sleep(1);
#ifdef RTCONFIG_SAMBASRV
	modprobe_r("nls_cp437");
	modprobe_r("nls_cp850");
	modprobe_r("nls_cp852");
	modprobe_r("nls_cp866");
#ifdef LINUX26
	modprobe_r("nls_cp932");
	modprobe_r("nls_cp936");
	modprobe_r("nls_cp949");
	modprobe_r("nls_cp950");
#endif
#endif
	modprobe_r(USBSTORAGE_MOD);
	modprobe_r(SG_MOD);
	modprobe_r(SD_MOD);
#ifdef LINUX26
	modprobe_r(SCSI_WAIT_MOD);
#endif
	modprobe_r(SCSI_MOD);
#endif
}

void remove_usb_led_module(void)
{
#ifdef LINUX26
	modprobe_r("leds-usb");
	modprobe_r("ledtrig-usbdev");
#endif
}

void remove_usb_host_module(void)
{
#ifndef HND_ROUTER
	modprobe_r(USBOHCI_MOD);
	modprobe_r(USBUHCI_MOD);
	modprobe_r(USB20_MOD);
#if defined(RTCONFIG_BT_CONN)
	modprobe_r("ath3k");
	modprobe_r("btusb");
#endif
#if defined(RTCONFIG_USB_XHCI)
#if defined(RTCONFIG_SOC_IPQ8064)
	modprobe_r("dwc3-ipq");
	modprobe_r("udc-core");
#endif
	modprobe_r(USB30_MOD);
#endif
#else  // HND_ROUTER
	tweak_usb_affinity(0);
#endif

#if defined(RTCONFIG_BLINK_LED)
	/* If both bled and USB Bus traffic statistics are enabled,
	 * don't remove USB core and USB common kernel module.
	 */
	if (!((nvram_get_int("led_usb_gpio") | nvram_get_int("led_usb3_gpio")) & GPIO_BLINK_LED))
#endif
	{
		modprobe_r(USBCORE_MOD);
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(3,2,0)
		modprobe_r(USBCOMMON_MOD);
#endif
	}
}

void remove_usb_module(void)
{
#ifdef RTCONFIG_USB_MODEM
	remove_usb_modem_modules();
#endif
#ifdef RTCONFIG_USB_PRINTER
	remove_usb_prn_module();
#endif
#if defined(RTCONFIG_SAMBASRV) || defined(RTCONFIG_FTP)
	remove_usb_storage_module();
#endif
	remove_usb_led_module();
	remove_usb_host_module();

#ifdef RTCONFIG_SOC_IPQ40XX
	remove_dakota_usb_modules();
#endif
}

// mode 0: stop all USB programs, mode 1: stop the programs from USB hotplug.
void stop_usb_program(int mode)
{
#ifdef RTCONFIG_USB_MODEM
#ifdef RTCONFIG_USB_BECEEM
	killall("wimaxd", SIGTERM);
	killall("wimaxd", SIGUSR1);
	if(!g_reboot)
		sleep(1);
#endif
#endif

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
#if defined(RTCONFIG_APP_PREINSTALLED) && defined(RTCONFIG_CLOUDSYNC)
	if(pids("inotify") || pids("asuswebstorage") || pids("webdav_client") || pids("dropbox_client") || pids("ftpclient") || pids("sambaclient") || pids("usbclient") || pids("google_client")){
		_dprintf("%s: stop_cloudsync.\n", __func__);
		stop_cloudsync(-1);
	}
#endif

	stop_app();
#endif

	stop_nas_services(0);

	if(mode)
		return;

#ifdef RTCONFIG_WEBDAV
	stop_webdav();
#else
	if(f_exists("/opt/etc/init.d/S50aicloud"))
		system("sh /opt/etc/init.d/S50aicloud scan");
#endif

#ifdef RTCONFIG_USB_PRINTER
	stop_lpd();
	stop_u2ec();
#endif
}

#ifndef RTCONFIG_ERPTEST
void stop_usb()
{
	int f_force = 0;
#else
void stop_usb(int f_force)
{
#endif
	int disabled = !nvram_get_int("usb_enable");

	_dprintf("%s: stopping the USB features...\n", __func__);

#ifdef RTCONFIG_USB_MODEM
	int modem_unit;
	char prefix2[32];
	char env_unit[32];

	for(modem_unit = MODEM_UNIT_FIRST; modem_unit < MODEM_UNIT_MAX; ++modem_unit){
		usb_modem_prefix(modem_unit, prefix2, sizeof(prefix2));

		snprintf(env_unit, 32, "unit=%d", modem_unit);
		putenv(env_unit);

#if defined(RTCONFIG_JFFS2) || defined(RTCONFIG_BRCM_NAND_JFFS2) || defined(RTCONFIG_UBIFS)
		_dprintf("stop_usb: save the modem(%d) data.\n", modem_unit);
		eval("/usr/sbin/modem_status.sh", "bytes+");
#endif

		unsetenv("unit");
	}
#endif

	stop_usb_program(0);

#ifdef RTCONFIG_USB_MODEM
	remove_usb_modem_modules();
#endif
#ifdef RTCONFIG_USB_PRINTER
	remove_usb_prn_module();
#endif

#if defined(RTCONFIG_SAMBASRV) || defined(RTCONFIG_FTP)
	// only stop storage services if disabled
	if (disabled || !nvram_get_int("usb_storage") || f_force) {
		// Unmount all partitions
		remove_storage_main(0);

		// Stop storage services
		remove_usb_storage_module();
	}
#endif

	remove_usb_led_module();

#ifndef HND_ROUTER
	if (disabled || nvram_get_int("usb_ohci") != 1 || f_force) modprobe_r(USBOHCI_MOD);
	if (disabled || nvram_get_int("usb_uhci") != 1 || f_force) modprobe_r(USBUHCI_MOD);
	if (disabled || nvram_get_int("usb_usb2") != 1 || f_force) modprobe_r(USB20_MOD);

#if defined(RTN56UB1) ||  defined(RTN56UB2) || defined(RTAC1200GA1) || defined(RTAC1200GU)
	modprobe_r(USB20_MOD);
#endif

#if defined(RTCONFIG_USB_XHCI)
#if defined(RTN65U) || defined(RTCONFIG_QCA) || defined(RTAC85U)
	if (disabled) {
#if defined(RTCONFIG_SOC_IPQ8064)
		modprobe_r("dwc3-ipq");
		modprobe_r("udc-core");
#endif
		modprobe_r(USB30_MOD);
	}
#elif 0	//defined(RTCONFIG_XHCIMODE)
	modprobe_r(USB30_MOD);
#else
	if (disabled || nvram_get_int("usb_usb3") != 1 || f_force) modprobe_r(USB30_MOD);
#endif
#endif

	// only unload core modules if usb is disabled
	if (disabled || f_force) {
		umount("/proc/bus/usb"); // unmount usb device filesystem
		modprobe_r(USBOHCI_MOD);
		modprobe_r(USBUHCI_MOD);
		modprobe_r(USB20_MOD);
#if defined(RTCONFIG_BT_CONN)
		modprobe_r("ath3k");
		modprobe_r("btusb");
#endif	/* RTCONFIG_BT_CONN */
#if defined(RTCONFIG_USB_XHCI) && !defined(HND_ROUTER)
#if defined(RTCONFIG_SOC_IPQ8064)
		modprobe_r("dwc3-ipq");
		modprobe_r("udc-core");
#endif
		modprobe_r(USB30_MOD);
#endif
#if defined(RTCONFIG_BLINK_LED)
		/* If both bled and USB Bus traffic statistics are enabled,
		 * don't remove USB core and USB common kernel module.
		 */
		if (!((nvram_get_int("led_usb_gpio") | nvram_get_int("led_usb3_gpio")) & GPIO_BLINK_LED) || f_force)
#endif
		{
			modprobe_r(USBCORE_MOD);
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(3,2,0)
			modprobe_r(USBCOMMON_MOD);
#endif
		}
	}

#ifdef RTCONFIG_SOC_IPQ40XX
	if(disabled)remove_dakota_usb_modules();
#endif
#endif // HND_ROUTER
}

#ifdef RTCONFIG_ERPTEST
void restart_usb(int stopit)
{
	stop_usb(1);

	if(stopit)
		return;

	sleep(2);
	start_usb(1);
}
#endif

#ifdef RTCONFIG_USB_PRINTER
void start_usblpsrv(void)
{
#if !(defined(HND_ROUTER) && defined(RTCONFIG_HNDMFG))
	if (nvram_get_int("asus_mfg"))
#endif
	return;

	nvram_set("u2ec_device", "");
	nvram_set("u2ec_busyip", "");
	nvram_set("MFP_busy", "0");

	start_u2ec();
	start_lpd();
}
#endif

#define MOUNT_VAL_FAIL 	0
#define MOUNT_VAL_RONLY	1
#define MOUNT_VAL_RW 	2
#define MOUNT_VAL_EXIST	3

int mount_r(char *mnt_dev, char *mnt_dir, char *_type)
{
	struct mntent *mnt;
	int ret;
	char options[140];
	char flagfn[128];
	int dir_made;
	char type[16];

	if(_type != NULL)
		strlcpy(type, _type, sizeof(type));
	else
		type[0] = '\0';

	if(strlen(type) > 0 && !strcmp(type, "apple_efi")){
		TRACE_PT("Don't mount the EFI partition(%s) of APPLE!\n", mnt_dev, type);
		return MOUNT_VAL_EXIST;
	}

	if((mnt = findmntents(mnt_dev, 0, NULL, 0))){
		syslog(LOG_INFO, "USB partition at %s already mounted on %s",
			mnt_dev, mnt->mnt_dir);
		return MOUNT_VAL_EXIST;
	}

	options[0] = 0;

	if(strlen(type) > 0){
#ifndef RTCONFIG_BCMARM
		unsigned long flags = MS_NOATIME | MS_NODEV;
#else
		unsigned long flags = MS_NODEV;
#endif

		if (strcmp(type, "swap") == 0 || strcmp(type, "mbr") == 0) {
			/* not a mountable partition */
			flags = 0;
		}
		else if (!strcmp(type, "unknown")) {
			/* Usually should be EFI, and not a mountable partition */
			flags = 0;
		}
		else if(!strncmp(type, "ext", 3)){
			sprintf(options, "user_xattr");

			if (nvram_invmatch("usb_ext_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_ext_opt"));
		}
		else if (strcmp(type, "vfat") == 0) {
			sprintf(options, "umask=0000");

#ifdef RTCONFIG_BCMARM
			sprintf(options + strlen(options), ",allow_utime=0022" + (options[0] ? 0 : 1));
#endif

			if (nvram_invmatch("smbd_cset", ""))
				sprintf(options + strlen(options), ",iocharset=%s%s",
						isdigit(nvram_get("smbd_cset")[0]) ? "cp" : "",
						nvram_get("smbd_cset"));

			if (nvram_invmatch("smbd_cpage", "")) {
				char cp[16];

				snprintf(cp, sizeof(cp), "%s", nvram_safe_get("smbd_cpage"));
				sprintf(options + strlen(options), ",codepage=%s" + (options[0] ? 0 : 1), cp);
				snprintf(flagfn, sizeof(flagfn), "nls_cp%s", cp);
				TRACE_PT("USB %s(%s) is setting the code page to %s!\n", mnt_dev, type, flagfn);

				snprintf(cp, sizeof(cp), "%s", nvram_safe_get("smbd_nlsmod"));
				if(strlen(cp) > 0 && (strcmp(cp, flagfn) != 0))
					modprobe_r(cp);

				modprobe(flagfn);
				nvram_set("smbd_nlsmod", flagfn);
			}

			sprintf(options + strlen(options), ",shortname=winnt" + (options[0] ? 0 : 1));
#ifdef RTCONFIG_OPENPLUS_TFAT
			if(nvram_match("usb_fatfs_mod", "tuxera")){
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_QCA)
				if(nvram_get_int("stop_iostreaming"))
					sprintf(options + strlen(options), ",nodev" + (options[0] ? 0 : 1));
				else
					sprintf(options + strlen(options), ",nodev,iostreaming" + (options[0] ? 0 : 1));
#else
				sprintf(options + strlen(options), ",noatime" + (options[0] ? 0 : 1));
#endif
			}
#ifdef LINUX26
			else
				sprintf(options + strlen(options), ",flush" + (options[0] ? 0 : 1));
#endif
#elif defined(RTCONFIG_TFAT)
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_QCA)
			if(nvram_get_int("stop_iostreaming"))
				sprintf(options + strlen(options), ",nodev" + (options[0] ? 0 : 1));
			else
				sprintf(options + strlen(options), ",nodev,iostreaming" + (options[0] ? 0 : 1));
#else
			sprintf(options + strlen(options), ",noatime" + (options[0] ? 0 : 1));
#endif
#else
#ifdef LINUX26
			sprintf(options + strlen(options), ",flush" + (options[0] ? 0 : 1));
#endif
#endif

			if (nvram_invmatch("usb_fat_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_fat_opt"));
		}
		else if (strncmp(type, "ntfs", 4) == 0) {
			sprintf(options, "umask=0000,nodev");

			if (nvram_invmatch("smbd_cset", ""))
				sprintf(options + strlen(options), "%snls=%s%s", options[0] ? "," : "",
						isdigit(nvram_get("smbd_cset")[0]) ? "cp" : "",
						nvram_get("smbd_cset"));
#if defined(RTCONFIG_REALTEK) && defined(RTCONFIG_TUXERA_NTFS)
			/* TUXERA module not support codepage options. */
#else			
			if (nvram_invmatch("smbd_cpage", "")) {
				char cp[16];

				snprintf(cp, sizeof(cp), "%s", nvram_safe_get("smbd_cpage"));
				sprintf(options + strlen(options), ",codepage=%s" + (options[0] ? 0 : 1), cp);
				snprintf(flagfn, sizeof(flagfn), "nls_cp%s", cp);
				TRACE_PT("USB %s(%s) is setting the code page to %s!\n", mnt_dev, type, flagfn);

				snprintf(cp, sizeof(cp), "%s", nvram_safe_get("smbd_nlsmod"));
				if(strlen(cp) > 0 && (strcmp(cp, flagfn) != 0))
					modprobe_r(cp);

				modprobe(flagfn);
				nvram_set("smbd_nlsmod", flagfn);
			}
#endif
#ifndef RTCONFIG_BCMARM
			sprintf(options + strlen(options), ",noatime" + (options[0] ? 0 : 1));
#endif

			if (nvram_invmatch("usb_ntfs_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_ntfs_opt"));
		}
		else if(!strncmp(type, "hfs", 3)){
			sprintf(options, "umask=0000,nodev,force");

#ifndef RTCONFIG_BCMARM
			sprintf(options + strlen(options), ",noatime" + (options[0] ? 0 : 1));
#endif
#ifdef RTCONFIG_KERNEL_HFSPLUS
			sprintf(options + strlen(options), ",force" + (options[0] ? 0 : 1));
#endif

			if(nvram_invmatch("usb_hfs_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_hfs_opt"));
		}


		if (flags) {
			if ((dir_made = mkdir_if_none(mnt_dir))) {
				/* Create the flag file for remove the directory on dismount. */
				snprintf(flagfn, sizeof(flagfn), "%s/.autocreated-dir", mnt_dir);
				f_write(flagfn, NULL, 0, 0, 0);
			}

			if(!strncmp(type, "vfat", 4)){
#ifdef RTCONFIG_OPENPLUS_TFAT
				if(nvram_match("usb_fatfs_mod", "tuxera"))
					ret = eval("mount", "-t", "tfat", "-o", options, mnt_dev, mnt_dir);
				else
					ret = eval("mount", "-t", "vfat", "-o", options, mnt_dev, mnt_dir);
#elif defined(RTCONFIG_TFAT)
				ret = eval("mount", "-t", "tfat", "-o", options, mnt_dev, mnt_dir);
#else
				ret = eval("mount", "-t", "vfat", "-o", options, mnt_dev, mnt_dir);
#endif
				if(ret != 0){
					syslog(LOG_INFO, "USB %s(%s) failed to mount at the first try!" , mnt_dev, type);
					TRACE_PT("USB %s(%s) failed to mount at the first try!\n", mnt_dev, type);
				}
			}
			else
			{
				ret = mount(mnt_dev, mnt_dir, type, flags, options[0] ? options : "");
				if(ret != 0){
					if(strncmp(type, "ext", 3)){
						syslog(LOG_INFO, "USB %s(%s) failed to mount at the first try!", mnt_dev, type);
						TRACE_PT("USB %s(%s) failed to mount At the first try!\n", mnt_dev, type);
						logmessage("usb", "USB %s(%s) failed to mount At the first try!\n", mnt_dev, type);
					}
					else{
						if(!strcmp(type, "ext4")){
							snprintf(type, 16, "ext3");
							ret = mount(mnt_dev, mnt_dir, type, flags, options[0] ? options : "");
						}

						if(ret != 0 && !strcmp(type, "ext3")){
							snprintf(type, sizeof(type), "ext2");
							ret = mount(mnt_dev, mnt_dir, type, flags, options[0] ? options : "");
						}

						if(ret != 0 && !strcmp(type, "ext2")){
							snprintf(type, sizeof(type), "ext");
							ret = mount(mnt_dev, mnt_dir, type, flags, options[0] ? options : "");
						}

						if(ret != 0){
							syslog(LOG_INFO, "USB %s(ext) failed to mount at the first try!", mnt_dev);
							TRACE_PT("USB %s(ext) failed to mount at the first try!\n", mnt_dev);
							logmessage("usb", "USB %s(ext) failed to mount at the first try!\n", mnt_dev);
						}
					}
				}
			}

#ifdef RTCONFIG_NTFS
			/* try ntfs-3g in case it's installed */
			if (ret != 0 && strncmp(type, "ntfs", 4) == 0) {
				if (nvram_get_int("usb_fs_ntfs")) {
#ifdef RTCONFIG_OPEN_NTFS3G
#if defined(RTCONFIG_OPENPLUSPARAGON_NTFS) || defined(RTCONFIG_OPENPLUSTUXERA_NTFS)
					if(nvram_match("usb_ntfs_mod", "open"))
#endif
					ret = eval("ntfs-3g", "-o", options, mnt_dev, mnt_dir);
#endif
#if defined(RTCONFIG_TUXERA_NTFS)
#if defined(RTCONFIG_OPENPLUSTUXERA_NTFS)
					else
#endif
					ret = eval("mount", "-t", "tntfs", "-o", options, mnt_dev, mnt_dir);
#endif
#if defined(RTCONFIG_PARAGON_NTFS)
#if defined(RTCONFIG_OPENPLUSPARAGON_NTFS)
					else
#endif
					{
					if(nvram_get_int("usb_fs_ntfs_sparse"))
						ret = eval("mount", "-t", "ufsd", "-o", options, "-o", "force", "-o", "sparse", mnt_dev, mnt_dir);
					else
						ret = eval("mount", "-t", "ufsd", "-o", options, "-o", "force", mnt_dev, mnt_dir);
					}
#endif
				}
			}
#endif

#ifdef RTCONFIG_HFS
			/* try HFS in case it's installed */
			if(ret != 0 && !strncmp(type, "hfs", 3)){
				if (nvram_get_int("usb_fs_hfs")) {
#ifdef RTCONFIG_KERNEL_HFSPLUS
#if defined(RTCONFIG_OPENPLUSPARAGON_HFS) || defined(RTCONFIG_OPENPLUSTUXERA_HFS)
					if(nvram_match("usb_hfs_mod", "open"))
#endif
					{
					eval("fsck.hfsplus", "-f", mnt_dev);//Scan
					ret = eval("mount", "-t", "hfsplus", "-o", options, mnt_dev, mnt_dir);
					}
#endif
#if defined(RTCONFIG_TUXERA_HFS)
#if defined(RTCONFIG_OPENPLUSTUXERA_HFS)
					else
#endif
					ret = eval("mount", "-t", "thfsplus", "-o", options, mnt_dev, mnt_dir);
#endif
#if defined(RTCONFIG_PARAGON_HFS)
#if defined(RTCONFIG_OPENPLUSPARAGON_HFS)
					else
#endif
					ret = eval("mount", "-t", "ufsd", "-o", options, mnt_dev, mnt_dir);
#endif
				}
			}
#endif

			if (ret != 0){ /* give it another try - guess fs */
				TRACE_PT("give it another try - guess fs.\n");
#ifndef RTCONFIG_BCMARM
				ret = eval("mount", "-o", "noatime,nodev", mnt_dev, mnt_dir);
#else
				ret = eval("mount", "-o", "nodev", mnt_dev, mnt_dir);
#endif
			}

			if (ret == 0) {
				syslog(LOG_INFO, "USB %s%s fs at %s mounted on %s",
						type, (flags & MS_RDONLY) ? " (ro)" : "", mnt_dev, mnt_dir);
				TRACE_PT("USB %s%s fs at %s mounted on %s.\n",
						type, (flags & MS_RDONLY) ? " (ro)" : "", mnt_dev, mnt_dir);
				logmessage("usb", "USB %s%s fs at %s mounted on %s.\n",
						type, (flags & MS_RDONLY) ? " (ro)" : "", mnt_dev, mnt_dir);
				return (flags & MS_RDONLY) ? MOUNT_VAL_RONLY : MOUNT_VAL_RW;
			}

			if (dir_made) {
				unlink(flagfn);
				rmdir(mnt_dir);
			}
		}
	}
	return MOUNT_VAL_FAIL;
}


struct mntent *mount_fstab(char *dev_name, char *type, char *label, char *uuid)
{
	struct mntent *mnt = NULL;
#if 0
	if (eval("mount", "-a") == 0)
		mnt = findmntents(dev_name, 0, NULL, 0);
#else
	char spec[PATH_MAX+1];

	if (label && *label) {
		sprintf(spec, "LABEL=%s", label);
		if (eval("mount", spec) == 0)
			mnt = findmntents(dev_name, 0, NULL, 0);
	}

	if (!mnt && uuid && *uuid) {
		sprintf(spec, "UUID=%s", uuid);
		if (eval("mount", spec) == 0)
			mnt = findmntents(dev_name, 0, NULL, 0);
	}

	if (!mnt) {
		if (eval("mount", dev_name) == 0)
			mnt = findmntents(dev_name, 0, NULL, 0);
	}

	if (!mnt) {
		/* Still did not find what we are looking for, try absolute path */
		if (realpath(dev_name, spec)) {
			if (eval("mount", spec) == 0)
				mnt = findmntents(dev_name, 0, NULL, 0);
		}
	}
#endif

	if (mnt)
		syslog(LOG_INFO, "USB %s fs at %s mounted on %s", type, dev_name, mnt->mnt_dir);
	return (mnt);
}


/* Check if the UFD is still connected because the links created in /dev/discs
 * are not removed when the UFD is  unplugged.
 * The file to read is: /proc/scsi/usb-storage-#/#, where # is the host no.
 * We are looking for "Attached: Yes".
 */
static int usb_ufd_connected(int host_no)
{
	char proc_file[128];
#ifndef LINUX26
	char line[256];
#endif
	FILE *fp;

	snprintf(proc_file, sizeof(proc_file), "%s/%s-%d/%d", PROC_SCSI_ROOT, USB_STORAGE, host_no, host_no);
	fp = fopen(proc_file, "r");

	if (!fp) {
		/* try the way it's implemented in newer kernels: /proc/scsi/usb-storage/[host] */
		snprintf(proc_file, sizeof(proc_file), "%s/%s/%d", PROC_SCSI_ROOT, USB_STORAGE, host_no);
		fp = fopen(proc_file, "r");
	}

	if (fp) {
#ifdef LINUX26
		fclose(fp);
		return 1;
#else
		while (fgets(line, sizeof(line), fp) != NULL) {
			if (strstr(line, "Attached: Yes")) {
				fclose(fp);
				return 1;
			}
		}
		fclose(fp);
#endif
	}

	return 0;
}


#ifndef MNT_DETACH
#define MNT_DETACH	0x00000002	/* from linux/fs.h - just detach from the tree */
#endif
int umount_mountpoint(struct mntent *mnt, uint flags);
int uswap_mountpoint(struct mntent *mnt, uint flags);

/* Unmount this partition from all its mountpoints.  Note that it may
 * actually be mounted several times, either with different names or
 * with "-o bind" flag.
 * If the special flagfile is now revealed, delete it and [attempt to] delete
 * the directory.
 */
int umount_partition(char *dev_name, int host_num, char *dsc_name, char *pt_name, uint flags)
{
	sync();	/* This won't matter if the device is unplugged, though. */

	if (flags & EFH_HUNKNOWN) {
		/* EFH_HUNKNOWN flag is passed if the host was unknown.
		 * Only unmount disconnected drives in this case.
		 */
		if (usb_ufd_connected(host_num))
			return 0;
	}

	/* Find all the active swaps that are on this device and stop them. */
	findmntents(dev_name, 1, uswap_mountpoint, flags);

	/* Find all the mountpoints that are for this device and unmount them. */
	findmntents(dev_name, 0, umount_mountpoint, flags);

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	usb_notify();
#endif

	return 0;
}

int uswap_mountpoint(struct mntent *mnt, uint flags)
{
	swapoff(mnt->mnt_fsname);
	return 0;
}

int umount_mountpoint(struct mntent *mnt, uint flags)
{
	int ret = 1, count;
	char flagfn[128];

	snprintf(flagfn, sizeof(flagfn), "%s/.autocreated-dir", mnt->mnt_dir);

	/* Run user pre-unmount scripts if any. It might be too late if
	 * the drive has been disconnected, but we'll try it anyway.
 	 */
	if (nvram_get_int("usb_automount"))
		run_nvscript("script_usbumount", mnt->mnt_dir, 3);
	/* Run *.autostop scripts located in the root of the partition being unmounted if any. */
	//run_userfile(mnt->mnt_dir, ".autostop", mnt->mnt_dir, 5);
	//run_nvscript("script_autostop", mnt->mnt_dir, 5);
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
#if defined(RTCONFIG_APP_PREINSTALLED) && defined(RTCONFIG_CLOUDSYNC)
	char word[PATH_MAX], *next_word;
	char *b, *nvp, *nv;
	int type = 0, enable = 0;
	char sync_dir[PATH_MAX];

	nv = nvp = strdup(nvram_safe_get("cloud_sync"));
	if(nv){
		while((b = strsep(&nvp, "<")) != NULL){
			count = 0;
			foreach_62(word, b, next_word){
				switch(count){
					case 0: // type
						type = atoi(word);
						break;
				}
				++count;
			}

			if(type == 1){
				continue;
			}
			else if(type == 0){
				char *b_bak, *ptr_b_bak;
				ptr_b_bak = b_bak = strdup(b);
				for(count = 0, next_word = strsep(&b_bak, ">"); next_word != NULL; ++count, next_word = strsep(&b_bak, ">")){
					switch(count){
						case 5: // dir
							memset(sync_dir, 0, PATH_MAX);
							strncpy(sync_dir, next_word, PATH_MAX);
							break;
						case 6: // enable
							enable = atoi(next_word);
							break;
					}
				}
				free(ptr_b_bak);

				if(!enable)
					continue;
_dprintf("cloudsync: dir=%s.\n", sync_dir);

				char mounted_path[PATH_MAX], *ptr, *other_path;
				ptr = sync_dir+strlen(POOL_MOUNT_ROOT)+1;
_dprintf("cloudsync: ptr=%s.\n", ptr);
				if((other_path = strchr(ptr, '/')) != NULL){
					ptr = other_path;
					++other_path;
				}
				else
					ptr = "";
_dprintf("cloudsync: other_path=%s.\n", other_path);

				memset(mounted_path, 0, PATH_MAX);
				strncpy(mounted_path, sync_dir, (strlen(sync_dir)-strlen(ptr)));
_dprintf("cloudsync: mounted_path=%s.\n", mounted_path);

				if(!strcmp(mounted_path, mnt->mnt_dir)){
_dprintf("%s: stop_cloudsync.\n", __func__);
					stop_cloudsync(type);
				}
			}
		}
		free(nv);
	}
#endif

#ifdef RTCONFIG_DSL_TCLINUX
	if(nvram_match("dslx_diag_enable", "1") && nvram_match("dslx_diag_state", "1")) {
		eval("req_dsl_drv", "runtcc");
		eval("req_dsl_drv", "dumptcc");
	}
	nvram_set("dsltmp_diag_log_path", "");
#endif

#ifdef RTCONFIG_PUSH_EMAIL
#ifdef RTCONFIG_DBLOG
	if(nvram_match("dblog_enable", "1")) {
		eval("dblogcmd", "exit"); // to stop dblog daemon
	}
	nvram_set("dblog_usb_path", "");
	#endif /* RTCONFIG_DBLOG */
#endif /* RTCONFIG_PUSH_EMAIL */

	if(!g_reboot && nvram_match("apps_mounted_path", mnt->mnt_dir))
		stop_app();
#endif
	
#ifdef RTCONFIG_USB_SWAP	
		stop_usb_swap(mnt->mnt_dir);
#endif	

	run_custom_script("unmount", 120, mnt->mnt_dir, NULL);

	sync();
	sleep(1);       // Give some time for buffers to be physically written to disk

	for (count = 0; count < 35; count++) {
		sync();
		ret = umount(mnt->mnt_dir);
		if (!ret)
			break;

		_dprintf("%s: umount %s count %d, return %d (errno %d %s)\n",
			__func__, mnt->mnt_dir, count, ret, errno, strerror(errno));
		syslog(LOG_NOTICE, "USB partition unmounted from %s fail. (return %d, %s)",
			mnt->mnt_dir, ret, strerror(errno));
		/* If we could not unmount the drive on the 1st try,
		 * kill all NAS applications so they are not keeping the device busy -
		 * unless it's an unmount request from the Web GUI.
		 */
		if ((count == 0) && ((flags & EFH_USER) == 0)){
//_dprintf("restart_nas_services(%d): test 1.\n", getpid());
			restart_nas_services(1, 0);
		}
		/* If we could not unmount the driver ten times,
		 * it is likely caused by downloadmaster/mediaserver/asus_lighttpd.
		 * Remove them again per ten retry.
		 */
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		if ((count && !(count % 9)) && ((flags & EFH_USER) == 0)){
			if (nvram_match("apps_mounted_path", mnt->mnt_dir))
				stop_app();
		}
#endif
		sleep(1);
	}

	if (ret == 0) {
		_dprintf("USB partition unmounted from %s\n", mnt->mnt_dir);
		syslog(LOG_NOTICE, "USB partition unmounted from %s", mnt->mnt_dir);
	}

	if (ret && ((flags & EFH_SHUTDN) != 0)) {
		/* If system is stopping (not restarting), and we couldn't unmount the
		 * partition, try to remount it as read-only. Ignore the return code -
		 * we can still try to do a lazy unmount.
		 */
		eval("mount", "-o", "remount,ro", mnt->mnt_dir);
	}

	if (ret && ((flags & EFH_USER) == 0)) {
		/* Make one more try to do a lazy unmount unless it's an unmount
		 * request from the Web GUI.
		 * MNT_DETACH will expose the underlying mountpoint directory to all
		 * except whatever has cd'ed to the mountpoint (thereby making it busy).
		 * So the unmount can't actually fail. It disappears from the ken of
		 * everyone else immediately, and from the ken of whomever is keeping it
		 * busy when they move away from it. And then it disappears for real.
		 */
		ret = umount2(mnt->mnt_dir, MNT_DETACH);
		_dprintf("USB partition busy - will unmount ASAP from %s\n", mnt->mnt_dir);
		syslog(LOG_NOTICE, "USB partition busy - will unmount ASAP from %s", mnt->mnt_dir);
	}

	if (ret == 0) {
		if ((unlink(flagfn) == 0)) {
			// Only delete the directory if it was auto-created
			rmdir(mnt->mnt_dir);
		}
	}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	if(nvram_match("apps_mounted_path", mnt->mnt_dir)){
		nvram_set("apps_dev", "");
		nvram_set("apps_mounted_path", "");
	}
#endif

	return (ret == 0);
}

static int diskmon_status(int status)
{
	static int run_status = DISKMON_IDLE;
	int old_status = run_status;
	char *message;

	switch (status) {
	case DISKMON_IDLE:
		message = "be idle";
		break;
	case DISKMON_START:
		message = "start...";
		break;
	case DISKMON_UMOUNT:
		message = "unmount partition";
		break;
	case DISKMON_SCAN:
		message = "scan partition";
		break;
	case DISKMON_REMOUNT:
		message = "re-mount partition";
		break;
	case DISKMON_FINISH:
		message = "done";
		break;
	case DISKMON_FORCE_STOP:
		message = "forcely stop";
		break;
	case DISKMON_FORMAT:
		message = "format partition";
		break;
	default:
		/* Just return previous status */
		return old_status;
	}

	/* Set new status */
	run_status = status;
	nvram_set_int("diskmon_status", status);
	logmessage("disk monitor", message);
	return old_status;
}

#if defined(RTCONFIG_DISK_MONITOR)
/**
 * Remove disk monitor log file of a disk partition.
 * @device:	device name, e.g., sda1, sdc5, etc
 */
static void remove_disk_log(char *device)
{
	int i;
	char fsck_log_name[64];

	if (!device || *device == '\0')
		return;

	if (!strncmp(device, "/dev/", 5))
		device += 5;

	if (strlen(device) > 6 || (strncmp(device, "sd", 2) && strncmp(device, "hd", 2)))
		return;

	snprintf(fsck_log_name, sizeof(fsck_log_name), "/tmp/fsck_ret/%s.log", device);
	unlink(fsck_log_name);
	for (i = 0; i < 10; ++i) {
		snprintf(fsck_log_name, sizeof(fsck_log_name), "/tmp/fsck_ret/%s.%d", device, i);
		unlink(fsck_log_name);
	}
}
#else
static inline void remove_disk_log(char *device) { }
#endif

/* Mount this partition on this disc.
 * If the device is already mounted on any mountpoint, don't mount it again.
 * If this is a swap partition, try swapon -a.
 * If this is a regular partition, try mount -a.
 *
 * Before we mount any partitions:
 *	If the type is swap and /etc/fstab exists, do "swapon -a"
 *	If /etc/fstab exists, try mounting using fstab.
 *  We delay invoking mount because mount will probe all the partitions
 *	to read the labels, and we don't want it to do that early on.
 *  We don't invoke swapon until we actually find a swap partition.
 *
 * If the mount succeeds, execute the *.asusrouter scripts in the top
 * directory of the newly mounted partition.
 * Returns NZ for success, 0 if we did not mount anything.
 */
int mount_partition(char *dev_name, int host_num, char *dsc_name, char *pt_name, uint flags)
{
	char the_label[128], mountpoint[128], uuid[40];
	int ret;
	char *type, *ptr;
	static char *swp_argv[] = { "swapon", "-a", NULL };
	struct mntent *mnt;
#if defined(RTCONFIG_USB_MODEM) || defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	char command[PATH_MAX];
#endif

	if ((type = detect_fs_type(dev_name)) == NULL)
		return 0;

	find_label_or_uuid(dev_name, the_label, uuid);

	run_custom_script("pre-mount", 120, dev_name, type);

	if (f_exists("/etc/fstab")) {
		if (strcmp(type, "swap") == 0) {
			_eval(swp_argv, NULL, 0, NULL);
			return 0;
		}
		if (mount_r(dev_name, NULL, NULL) == MOUNT_VAL_EXIST)
			return 0;
		if ((mnt = mount_fstab(dev_name, type, the_label, uuid))) {
			strlcpy(mountpoint, mnt->mnt_dir, sizeof(mountpoint));
			ret = MOUNT_VAL_RW;
			goto done;
		}
	}

	if (*the_label != 0) {
		for (ptr = the_label; *ptr; ptr++) {
			if (!isalnum(*ptr) && !strchr("+-&.@()", *ptr))
				*ptr = '_';
		}
		snprintf(mountpoint, sizeof(mountpoint), "%s/%s", POOL_MOUNT_ROOT, the_label);

		int the_same_name = 0;
		while(check_if_file_exist(mountpoint) || check_if_dir_exist(mountpoint)){
			++the_same_name;
			snprintf(mountpoint, sizeof(mountpoint), "%s/%s(%d)", POOL_MOUNT_ROOT, the_label, the_same_name);
		}

		if ((ret = mount_r(dev_name, mountpoint, type)))
			goto done;
	}

	/* Can't mount to /mnt/LABEL, so try mounting to /mnt/discDN_PN */
	snprintf(mountpoint, sizeof(mountpoint), "%s/%s", POOL_MOUNT_ROOT, pt_name);
	ret = mount_r(dev_name, mountpoint, type);

done:
	if (ret == MOUNT_VAL_RONLY || ret == MOUNT_VAL_RW)
	{
		chmod(mountpoint, 0777);

		char usb_node[32], port_path[8];
		char prefix[] = "usb_pathXXXXXXXXXXXXXXXXX_", tmp[100];

		ptr = dev_name+5;

		// Get USB node.
		if(get_usb_node_by_device(ptr, usb_node, 32) != NULL){
			if(get_path_by_node(usb_node, port_path, 8) != NULL){
				snprintf(prefix, sizeof(prefix), "usb_path%s", port_path);
				// for ATE.
				if(strlen(nvram_safe_get(strcat_r(prefix, "_fs_path0", tmp))) <= 0){
					nvram_set(tmp, ptr);
_dprintf("usb_path: 3. set %s=%s.\n", tmp, ptr);
				}
				else
_dprintf("usb_path: 4. don't set %s.\n", tmp);

#ifdef RTCONFIG_USB_MODEM
				unsigned int vid, pid;

				vid = strtoul(nvram_safe_get(strcat_r(prefix, "_vid", tmp)), NULL, 16);
				pid = strtoul(nvram_safe_get(strcat_r(prefix, "_pid", tmp)), NULL, 16);

				if(is_create_file_dongle(vid, pid)){
					if(strcmp(nvram_safe_get("stop_sg_remove"), "1")){
						memset(command, 0, PATH_MAX);
						snprintf(command, PATH_MAX, "touch %s/wcdma.cfg", mountpoint);
						system(command);
					}

					return 0; // skip to restart_nasapps.
				}
#endif
			}
		}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		if(!strcmp(nvram_safe_get("apps_mounted_path"), "")){
			char apps_folder[32], buff1[64], buff2[64];

			memset(apps_folder, 0, 32);
			strncpy(apps_folder, nvram_safe_get("apps_install_folder"), 32);

			memset(command, 0, PATH_MAX);
			snprintf(command, PATH_MAX, "/usr/sbin/app_check_folder.sh %s", mountpoint);
			system(command);
			//sleep(1);

			memset(buff1, 0, 64);
			snprintf(buff1, sizeof(buff1), "%s/%s/.asusrouter", mountpoint, apps_folder);
			memset(buff2, 0, 64);
			snprintf(buff2, sizeof(buff2), "%s/%s/.asusrouter.disabled", mountpoint, apps_folder);

			if(check_if_file_exist(buff1) && !check_if_file_exist(buff2)){
				// fsck the partition.
				if(strcmp(nvram_safe_get("stop_fsck"), "1") && host_num != -3
						// there's some problem with fsck.ext4.
						&& strcmp(type, "ext4")
						){
					cprintf("%s: umount partition %s...\n", apps_folder, dev_name);
					logmessage("asusware", "umount partition %s...\n", dev_name);
					diskmon_status(DISKMON_UMOUNT);

					findmntents(dev_name, 0, umount_mountpoint, EFH_HP_REMOVE);

					cprintf("%s: Automatically scan partition %s...\n", apps_folder, dev_name);
					logmessage("asusware", "Automatically scan partition %s...\n", dev_name);
					diskmon_status(DISKMON_SCAN);

					memset(command, 0, PATH_MAX);
					snprintf(command, PATH_MAX, "/usr/sbin/app_fsck.sh %s %s", type, dev_name);
					system(command);

					cprintf("%s: re-mount partition %s...\n", apps_folder, dev_name);
					logmessage("asusware", "re-mount partition %s...\n", dev_name);
					diskmon_status(DISKMON_REMOUNT);

					ret = mount_r(dev_name, mountpoint, type);

					cprintf("%s: done.\n", apps_folder);
					logmessage("asusware", "done.\n");
					diskmon_status(DISKMON_FINISH);
				}

				system("rm -rf /tmp/opt");

				memset(command, 0, PATH_MAX);
				snprintf(command, PATH_MAX, "ln -sf %s/%s /tmp/opt", mountpoint, apps_folder);
				system(command);

				if(strcmp(nvram_safe_get("stop_runapp"), "1")){
					memset(buff1, 0, 64);
					snprintf(buff1, sizeof(buff1), "APPS_DEV=%s", dev_name+5);
					putenv(buff1);
					memset(buff2, 0, 64);
					snprintf(buff2, sizeof(buff2), "APPS_MOUNTED_PATH=%s", mountpoint);
					putenv(buff2);
					/* Run user *.asusrouter and post-mount scripts if any. */
					memset(command, 0, PATH_MAX);
					snprintf(command, PATH_MAX, "%s/.asusrouter", nvram_safe_get("apps_local_space"));
					system(command);
					unsetenv("APPS_DEV");
					unsetenv("APPS_MOUNTED_PATH");
				}
			}
		}

		usb_notify();
#endif
#ifdef RTCONFIG_USB_SWAP
			start_usb_swap(mountpoint);
#endif
#ifdef RTCONFIG_DSL_TCLINUX
		if(ret == MOUNT_VAL_RW) {
			if(nvram_match("dsltmp_diag_log_path", "")) {
				nvram_set("dsltmp_diag_log_path", mountpoint);
				if(nvram_match("dslx_diag_enable", "1") && nvram_match("dslx_diag_state", "1"))
					start_dsl_diag();
			}
		}
#endif

#ifdef RTCONFIG_PUSH_EMAIL
#ifdef RTCONFIG_DBLOG
		if(ret == MOUNT_VAL_RW) {
			if(nvram_match("dblog_usb_path", "")) {
				nvram_set("dblog_usb_path", mountpoint);
				//(enable=1) && (state=run || state=reboot)
				if(nvram_match("dblog_enable", "1") && (nvram_match("dblog_state", "1")||nvram_match("dblog_state", "2"))) {
					start_dblog(0);
				}
			}
		}
#endif /* RTCONFIG_DBLOG */
#endif /* RTCONFIG_PUSH_EMAIL */

		// check the permission files.
		if(ret == MOUNT_VAL_RW)
			test_of_var_files(mountpoint);

		if (nvram_get_int("usb_automount"))
			run_nvscript("script_usbmount", mountpoint, 3);

		run_custom_script("post-mount", 120, mountpoint, NULL);

#if defined(RTCONFIG_APP_PREINSTALLED) && defined(RTCONFIG_CLOUDSYNC)
		char word[PATH_MAX], *next_word;
		char cloud_setting[2048], *b, *nvp, *nv;
		int type = 0, rule = 0, enable = 0;
		char username[64], password[64], url[PATH_MAX], sync_dir[PATH_MAX];
		int count;
		char cloud_token[PATH_MAX];
		char cloud_setting_buf[PATH_MAX];
		char cloud_setting_buf2[PATH_MAX];

		snprintf(cloud_setting, sizeof(cloud_setting), "%s", nvram_safe_get("cloud_sync"));

		memset(cloud_setting_buf, 0, PATH_MAX);

		if(!nvram_get_int("enable_cloudsync") || strlen(cloud_setting) <= 0)
			return (ret == MOUNT_VAL_RONLY || ret == MOUNT_VAL_RW);

		if(pids("asuswebstorage") && pids("webdav_client") && pids("dropbox_client") && pids("ftpclient") && pids("sambaclient") && pids("usbclient"))
			return (ret == MOUNT_VAL_RONLY || ret == MOUNT_VAL_RW);

		nv = nvp = strdup(cloud_setting);
		if(nv){
			while((b = strsep(&nvp, "<")) != NULL){
				count = 0;
				foreach_62(word, b, next_word){
					switch(count){
						case 0: // type
							type = atoi(word);
							break;
					}
					++count;
				}

				if(type == 1 || type == 2 || type == 3 || type == 4 || type == 5|| type == 6){ //20150619 magic add type 4 sambclient and type 5 usbclient
					if(strlen(cloud_setting_buf) > 0) {
						sprintf(cloud_setting_buf2, "%s<%s", cloud_setting_buf, b);
						strlcpy(cloud_setting_buf, cloud_setting_buf2, sizeof(cloud_setting_buf));
					} else
						strcpy(cloud_setting_buf, b);
				}
				else if(type == 0){
					char *b_bak, *ptr_b_bak;
					ptr_b_bak = b_bak = strdup(b);
					for(count = 0, next_word = strsep(&b_bak, ">"); next_word != NULL; ++count, next_word = strsep(&b_bak, ">")){
						switch(count){
							case 0: // type
								type = atoi(next_word);
								break;
							case 1: // username
								memset(username, 0, 64);
								strncpy(username, next_word, 64);
								break;
							case 2: // password
								memset(password, 0, 64);
								strncpy(password, next_word, 64);
								break;
							case 3: // url
								memset(url, 0, PATH_MAX);
								strncpy(url, next_word, PATH_MAX);
								break;
							case 4: // rule
								rule = atoi(next_word);
								break;
							case 5: // dir
								memset(sync_dir, 0, PATH_MAX);
								strncpy(sync_dir, next_word, PATH_MAX);
								break;
							case 6: // enable
								enable = atoi(next_word);
								break;
						}
					}
					free(ptr_b_bak);
_dprintf("cloudsync: enable=%d, type=%d, user=%s, dir=%s.\n", enable, type, username, sync_dir);

					if(!enable)
						continue;

					memset(cloud_token, 0, PATH_MAX);
					snprintf(cloud_token, PATH_MAX, "%s/.__cloudsync_%d_%s.txt", mountpoint, type, username);
_dprintf("cloudsync: cloud_token=%s.\n", cloud_token);

					if(check_if_file_exist(cloud_token)){
						char mounted_path[PATH_MAX], *other_path;
						char true_cloud_setting[PATH_MAX];

						ptr = sync_dir+strlen(POOL_MOUNT_ROOT)+1;
						if((other_path = strchr(ptr, '/')) != NULL){
							ptr = other_path;
							++other_path;
						}
						else
							ptr = "";

						memset(mounted_path, 0, PATH_MAX);
						strncpy(mounted_path, sync_dir, (strlen(sync_dir)-strlen(ptr)));
_dprintf("cloudsync:   mountpoint=%s.\n", mountpoint);
_dprintf("cloudsync: mounted_path=%s.\n", mounted_path);

						if(strcmp(mounted_path, mountpoint)){
							memset(true_cloud_setting, 0, PATH_MAX);
							snprintf(true_cloud_setting, PATH_MAX, "%d>%s>%s>%s>%d>%s%s%s>%d", type, username, password, url, rule, mountpoint, (other_path != NULL)?"/":"", (other_path != NULL)?other_path:"", enable);
_dprintf("cloudsync: true_cloud_setting=%s.\n", true_cloud_setting);

							if(strlen(cloud_setting_buf) > 0) {
								sprintf(cloud_setting_buf2, "%s<%s", cloud_setting_buf, true_cloud_setting);
								strlcpy(cloud_setting_buf, cloud_setting_buf2, sizeof(cloud_setting_buf));
							} else
								strcpy(cloud_setting_buf, true_cloud_setting);
						}
						else{
							if(strlen(cloud_setting_buf) > 0) {
								sprintf(cloud_setting_buf2, "%s<%s", cloud_setting_buf, b);
								strlcpy(cloud_setting_buf, cloud_setting_buf2, sizeof(cloud_setting_buf));
							} else
								strcpy(cloud_setting_buf, b);
						}
					}
					else{
						if(strlen(cloud_setting_buf) > 0) {
							sprintf(cloud_setting_buf2, "%s<%s", cloud_setting_buf, b);
							strlcpy(cloud_setting_buf, cloud_setting_buf2, sizeof(cloud_setting_buf));
						} else
							strcpy(cloud_setting_buf, b);
					}
				}
			}
			free(nv);

_dprintf("cloudsync: set nvram....\n");
			nvram_set("cloud_sync", cloud_setting_buf);
_dprintf("cloudsync: wait a second...\n");
			sleep(1); // wait the nvram be ready.
_dprintf("%s: start_cloudsync.\n", __func__);
			start_cloudsync(0);
		}
#endif
	}
#ifdef RTCONFIG_USBRESET
	else if(strcmp(type, "unknown")){ // Can't mount the the known partition.
		char usbreset_active[16];

		snprintf(usbreset_active, sizeof(usbreset_active), "%s", nvram_safe_get("usbreset_active"));

_dprintf("test a. %s: usbreset_active=%s.\n", dev_name, usbreset_active);
		if(strlen(usbreset_active) <= 0 || !strcmp(usbreset_active, "0")){
			nvram_set("usbreset_active", "1");

			notify_rc("start_usbreset");

_dprintf("test b. USB RESET finish.\n");
		}
	}
#endif
	return (ret == MOUNT_VAL_RONLY || ret == MOUNT_VAL_RW);
}

/* Mount or unmount all partitions on this controller.
 * Parameter: action_add:
 * 0  = unmount
 * >0 = mount only if automount config option is enabled.
 * <0 = mount regardless of config option.
 */
void hotplug_usb_storage_device(int host_no, int action_add, uint flags)
{
	if (!nvram_get_int("usb_enable"))
		return;

	_dprintf("%s: host %d action: %d\n", __func__, host_no, action_add);

	if (action_add) {
		if (nvram_get_int("usb_storage") && (nvram_get_int("usb_automount") || action_add < 0)) {
			/* Do not probe the device here. It's either initiated by user,
			 * or hotplug_usb() already did.
			 */
			if (exec_for_host(host_no, 0x00, flags, mount_partition)) {
//_dprintf("restart_nas_services(%d): test 2.\n", getpid());
				restart_nas_services(1, 1); // restart all NAS applications
			}
		}
	}
	else {
		if (nvram_get_int("usb_storage") || ((flags & EFH_USER) == 0)) {
			/* When unplugged, unmount the device even if
			 * usb storage is disabled in the GUI.
			 */
			exec_for_host(host_no, (flags & EFH_USER) ? 0x00 : 0x02, flags, umount_partition);
			/* Restart NAS applications (they could be killed by umount_mountpoint),
			 * or just re-read the configuration.
			 */
//_dprintf("restart_nas_services(%d): test 3.\n", getpid());
			restart_nas_services(1, 1);
		}
	}
}


/* This gets called at reboot or upgrade.  The system is stopping. */
void remove_storage_main(int shutdn)
{
	if (shutdn){
//_dprintf("restart_nas_services(%d): test 4.\n", getpid());
		restart_nas_services(1, 0);
	}
	/* Unmount all partitions */
	exec_for_host(-1, 0x02, shutdn ? EFH_SHUTDN : 0, umount_partition);
}

#ifdef RTCONFIG_REALTEK
static const char *path_to_name(const char *path) {
	const char *s = path, *tmp;
	//_dprintf("%s(1)\n", __func__);

	if (path == NULL) return 0;

	while ((tmp = strchr(s, '/'))!= 0) {
		//_dprintf("%s(2 %s)\n", __func__, tmp);
		s = &tmp[1];
	}

	//_dprintf("%s(3 %s)\n", __func__, s);
	if (strlen(s))
		return s;

	return 0;
}
#endif /* RTCONFIG_REALTEK */

/*******
 * All the complex locking & checking code was removed when the kernel USB-storage
 * bugs were fixed.
 * The crash bug was with overlapped I/O to different USB drives, not specifically
 * with mount processing.
 *
 * And for USB devices that are slow to come up.  The kernel now waits until the
 * USB drive has settled, and it correctly reads the partition table before calling
 * the hotplug agent.
 *
 * The kernel patch was cleaning up data structures on an unplug.  It
 * needs to wait until the disk is unmounted.  We have 20 seconds to do
 * the unmounts.
 *******/


/* Plugging or removing usb device
 *
 * On an occurrance, multiple hotplug events may be fired off.
 * For example, if a hub is plugged or unplugged, an event
 * will be generated for everything downstream of it, plus one for
 * the hub itself.  These are fired off simultaneously, not serially.
 * This means that many many hotplug processes will be running at
 * the same time.
 *
 * The hotplug event generated by the kernel gives us several pieces
 * of information:
 * PRODUCT is vendorid/productid/rev#.
 * DEVICE is /proc/bus/usb/bus#/dev#
 * ACTION is add or remove
 * SCSI_HOST is the host (controller) number (this relies on the custom kernel patch)
 *
 * Note that when we get a hotplug add event, the USB susbsystem may
 * or may not have yet tried to read the partition table of the
 * device.  For a new controller that has never been seen before,
 * generally yes.  For a re-plug of a controller that has been seen
 * before, generally no.
 *
 * On a remove, the partition info has not yet been expunged.  The
 * partitions show up as /dev/discs/disc#/part#, and /proc/partitions.
 * It appears that doing a "stat" for a non-existant partition will
 * causes the kernel to re-validate the device and update the
 * partition table info.  However, it won't re-validate if the disc is
 * mounted--you'll get a "Device busy for revalidation (usage=%d)" in
 * syslog.
 *
 * The $INTERFACE is "class/subclass/protocol"
 * Some interesting classes:
 *	8 = mass storage
 *	7 = printer
 *	3 = HID.   3/1/2 = mouse.
 *	6 = still image (6/1/1 = Digital camera Camera)
 *	9 = Hub
 *	255 = scanner (255/255/255)
 *
 * Observed:
 *	Hub seems to have no INTERFACE (null), and TYPE of "9/0/0"
 *	Flash disk seems to have INTERFACE of "8/6/80", and TYPE of "0/0/0"
 *
 * When a hub is unplugged, a hotplug event is generated for it and everything
 * downstream from it.  You cannot depend on getting these events in any
 * particular order, since there will be many hotplug programs all fired off
 * at almost the same time.
 * On a remove, don't try to access the downstream devices right away, give the
 * kernel time to finish cleaning up all the data structures, which will be
 * in the process of being torn down.
 *
 * On the initial plugin, the first time the kernel usb-storage subsystem sees
 * the host (identified by GUID), it automatically reads the partition table.
 * On subsequent plugins, it does not.
 *
 * Special values for Web Administration to unmount or remount
 * all partitions of the host:
 *	INTERFACE=TOMATO/...
 *	ACTION=add/remove
 *	SCSI_HOST=<host_no>
 * If host_no is negative, we unmount all partions of *all* hosts.
 */
void hotplug_usb(void)
{
	int add;
	int host = -1;
	char *interface = getenv("INTERFACE");
	char *action = getenv("ACTION");
	char *product = getenv("PRODUCT");
#ifdef LINUX26
	char *device = getenv("DEVICENAME");
	char *subsystem = getenv("SUBSYSTEM");
	int is_block = strcmp(subsystem ? : "", "block") == 0;
#else
	char *device = getenv("DEVICE");
#endif
	char *scsi_host = getenv("SCSI_HOST");
	char *usbport = getenv("USBPORT");

#ifdef RTCONFIG_REALTEK
	char *devpath = getenv("DEVPATH");
	//_dprintf("devpath: %s\n", devpath);
	device = path_to_name(devpath);
	if( !device || strncmp(device, "sd", 2)!=0 || strlen(device)!=4) { // must be sda1,sdb2 etc.
		//_dprintf("no device\n");
		return;
	}
	//_dprintf("device: %s\n", device);
#endif /* RTCONFIG_REALTEK */

	_dprintf("%s hotplug INTERFACE=%s ACTION=%s USBPORT=%s HOST=%s DEVICE=%s\n",
		subsystem ? : "USB", interface, action, usbport, scsi_host, device);

	if (!nvram_get_int("usb_enable")) return;
#ifdef LINUX26
	if (!action || ((!interface || !product) && !is_block))
#else
	if (!interface || !action || !product)	/* Hubs bail out here. */
#endif
		return;

	if (scsi_host)
		host = safe_atoi(scsi_host);

	if (!wait_action_idle(10)) return;

	add = (strcmp(action, "add") == 0);
	if (add && (strncmp(interface ? : "", "TOMATO/", 7) != 0)) {
#ifdef LINUX26
		if (!is_block && device)
#endif
		syslog(LOG_DEBUG, "Attached USB device %s [INTERFACE=%s PRODUCT=%s]",
			device, interface, product);

#ifndef LINUX26
		/* To allow automount to be blocked on startup.
		 * In kernel 2.6 we still need to serialize mount/umount calls -
		 * so the lock is down below in the "block" hotplug processing.
		 */
		file_unlock(file_lock("usb"));
#endif
	}

	if (strncmp(interface ? : "", "TOMATO/", 7) == 0) {	/* web admin */
		if (scsi_host == NULL)
			host = safe_atoi(product);	// for backward compatibility
		/* If host is negative, unmount all partitions of *all* hosts.
		 * If host == -1, execute "soft" unmount (do not kill NAS apps, no "lazy" umount).
		 * If host == -2, run "hard" unmount, as if the drive is unplugged.
		 * This feature can be used in custom scripts as following:
		 *
		 * # INTERFACE=TOMATO/1 ACTION=remove PRODUCT=-1 SCSI_HOST=-1 hotplug usb
		 *
		 * PRODUCT is required to pass the env variables verification.
		 */
		/* Unmount or remount all partitions of the host. */
		hotplug_usb_storage_device(host < 0 ? -1 : host, add ? -1 : 0,
			host == -2 ? 0 : EFH_USER);
	}
#ifdef LINUX26
	//else if (is_block && strcmp(getenv("MAJOR") ? : "", "8") == 0
	else if (is_block && safe_atoi(getenv("MAJOR") ? : "0") == USB_DISK_MAJOR
#if (!defined(LINUX30) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
		&& strcmp(getenv("PHYSDEVBUS") ? : "", "scsi") == 0
#endif
		)
	{
		/* scsi partition */
		char devname[64];
		int lock, c;
		char *d, dev[32], nv_name[32];

		/* strip trail digits */
		strlcpy(dev, device, sizeof(dev));
		d = dev + strlen(dev) - 1;
		while (d > dev && isdigit(*d))
			*d-- = '\0';
		sprintf(nv_name, "ignore_nas_service_%s", dev);

		snprintf(devname, sizeof(devname), "/dev/%s", device);
		lock = file_lock("usb");
		remove_disk_log(device);
		if (add) {
			if (nvram_get(nv_name) != NULL)
				nvram_unset(nv_name);
			if (nvram_get_int("usb_storage") && nvram_get_int("usb_automount")) {
				int minor = safe_atoi(getenv("MINOR") ? : "0");
				if ((minor % 16) == 0 && !is_no_partition(device)) {
					/* This is a disc, and not a "no-partition" device,
					 * like APPLE iPOD shuffle. We can't mount it.
					 */
					file_unlock(lock);
					return;
				}
				TRACE_PT(" mount to dev: %s\n", devname);
				if (mount_partition(devname, host, NULL, device, EFH_HP_ADD)) {
_dprintf("restart_nas_services(%d): test 5.\n", getpid());
					//restart_nas_services(1, 1); // restart all NAS applications
					notify_rc_and_wait("restart_nasapps");
				}
				TRACE_PT(" end of mount\n");
			}
		}
		else {
			/* When unplugged, unmount the device even if usb storage is disabled in the GUI */
			umount_partition(devname, host, NULL, device, EFH_HP_REMOVE);
			/* Restart NAS applications (they could be killed by umount_mountpoint),
			 * or just re-read the configuration.
			 */

			if ((c = nvram_get_int(nv_name)) >= 1) {
				if (c > 1)
					nvram_set_int(nv_name, --c);
				else
					nvram_unset(nv_name);
			} else {
_dprintf("restart_nas_services(%d): test 6.\n", getpid());
				//restart_nas_services(1, 1);
				notify_rc_after_wait("restart_nasapps");
			}
		}
		file_unlock(lock);
	}
#endif
	else if (strncmp(interface ? : "", "8/", 2) == 0) {	/* usb storage */
		run_nvscript("script_usbhotplug", NULL, 2);
#ifndef LINUX26
		hotplug_usb_storage_device(host, add, (add ? EFH_HP_ADD : EFH_HP_REMOVE) | (host < 0 ? EFH_HUNKNOWN : 0));
#endif
	}
	else {	/* It's some other type of USB device, not storage. */
#ifdef LINUX26
		if (is_block) return;
#endif
		/* Do nothing.  The user's hotplug script must do it all. */
		run_nvscript("script_usbhotplug", NULL, 2);
	}
}



// -----------------------------------------------------------------------------

// !!TB - FTP Server

#ifdef RTCONFIG_FTP
/* VSFTPD code mostly stolen from Oleg's ASUS Custom Firmware GPL sources */

void write_ftpd_conf()
{
	FILE *fp;
	char maxuser[16];
	int passive_port;
#ifdef RTCONFIG_HTTPS
	unsigned long long sn;
	char t[32];
#endif

	/* write /etc/vsftpd.conf */
	fp=fopen("/etc/vsftpd.conf", "w");
	if (fp==NULL) return;

	if (nvram_match("st_ftp_mode", "2")
			|| (nvram_match("st_ftp_mode", "1") && !strcmp(nvram_safe_get("st_ftp_force_mode"), ""))
			)
		fprintf(fp, "anonymous_enable=NO\n");
	else{
		fprintf(fp, "anonymous_enable=YES\n");
		fprintf(fp, "anon_upload_enable=YES\n");
		fprintf(fp, "anon_mkdir_write_enable=YES\n");
		fprintf(fp, "anon_other_write_enable=YES\n");
	}

	fprintf(fp, "nopriv_user=root\n");
	fprintf(fp, "write_enable=YES\n");
	fprintf(fp, "local_enable=YES\n");
	fprintf(fp, "chroot_local_user=YES\n");
	fprintf(fp, "local_umask=000\n");
	fprintf(fp, "dirmessage_enable=NO\n");
	fprintf(fp, "xferlog_enable=NO\n");
	fprintf(fp, "syslog_enable=NO\n");
	fprintf(fp, "connect_from_port_20=YES\n");
	fprintf(fp, "use_localtime=YES\n");
#if (!defined(LINUX30) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36))
	fprintf(fp, "use_sendfile=NO\n");
#endif
#ifndef RTCONFIG_BCMARM
	fprintf(fp, "isolate=NO\n");	// 3.x: Broken for MIPS
#endif

#ifdef RTCONFIG_IPV6
	if (ipv6_enabled()) {
		fprintf(fp, "listen_ipv6=YES\n");
		// vsftpd 3.x can't use both listen at same time.  We don't specify an interface, so
		// the listen_ipv6 directive will also make vsftpd listen to ipv4 IPs
		fprintf(fp, "listen=NO\n");
	} else {
		fprintf(fp, "listen=YES\n");
	}

#else
	fprintf(fp, "listen=YES\n");
#endif
	fprintf(fp, "pasv_enable=YES\n");
	if (nvram_get_int("ftp_wanac")) {
		passive_port = nvram_get_int("ftp_pasvport");
		if (passive_port > 0) {
			if (passive_port > 65505)
				nvram_set_int("ftp_pasvport", 65505);
			fprintf(fp, "pasv_min_port=%d\n", passive_port);
			fprintf(fp, "pasv_max_port=%d\n", passive_port + 30);
		}
	}
	fprintf(fp, "tcp_wrappers=NO\n");
	strcpy(maxuser, nvram_safe_get("st_max_user"));
	if ((atoi(maxuser)) > 0)
		fprintf(fp, "max_clients=%s\n", maxuser);
	else
		fprintf(fp, "max_clients=%s\n", "10");
	fprintf(fp, "ftp_username=anonymous\n");
	fprintf(fp, "ftpd_banner=Welcome to ASUS %s FTP service.\n", get_productid());

	// update codepage
	modprobe_r("nls_cp936");
	modprobe_r("nls_cp950");

	if (strcmp(nvram_safe_get("ftp_lang"), "EN") != 0)
	{
		fprintf(fp, "enable_iconv=YES\n");
		if (nvram_match("ftp_lang", "TW")) {
			fprintf(fp, "remote_charset=cp950\n");
			modprobe("nls_cp950");
		}
		else if (nvram_match("ftp_lang", "CN")) {
			fprintf(fp, "remote_charset=cp936\n");
			modprobe("nls_cp936");
		}
	}

	if(!strcmp(nvram_safe_get("enable_ftp_log"), "1")){
		fprintf(fp, "xferlog_enable=YES\n");
		fprintf(fp, "xferlog_file=/var/log/vsftpd.log\n");
	}

#ifdef RTCONFIG_HTTPS
	if(nvram_get_int("ftp_tls")){
		fprintf(fp, "ssl_enable=YES\n");
		fprintf(fp, "rsa_cert_file=%s\n", HTTPD_CERT);
		fprintf(fp, "rsa_private_key_file=%s\n", HTTPD_KEY);

		if(!f_exists(HTTPD_CERT) || !f_exists(HTTPD_KEY)
#ifdef RTCONFIG_LETSENCRYPT
			|| !cert_key_match(HTTPD_CERT, HTTPD_KEY)
#endif
			) {
#ifdef RTCONFIG_LETSENCRYPT
			if(nvram_match("le_enable", "1")) {
				cp_le_cert(LE_FULLCHAIN, HTTPD_CERT);
				cp_le_cert(LE_KEY, HTTPD_KEY);
			}
			else if(nvram_match("le_enable", "2")) {
				unlink(HTTPD_CERT);
				unlink(HTTPD_KEY);
				if(f_exists(UPLOAD_CERT) && f_exists(UPLOAD_KEY)) {
					eval("cp", UPLOAD_CERT, HTTPD_CERT);
					eval("cp", UPLOAD_KEY, HTTPD_KEY);
				}
			}
#else
			if(f_exists(UPLOAD_CERT) && f_exists(UPLOAD_KEY)){
				eval("cp", UPLOAD_CERT, HTTPD_CERT);
				eval("cp", UPLOAD_KEY, HTTPD_KEY);
			}
#endif
		}

		// Is it valid now?  Otherwise, generate one.
		if(!f_exists(HTTPD_CERT) || !f_exists(HTTPD_KEY)
#ifdef RTCONFIG_LETSENCRYPT
                        || !cert_key_match(HTTPD_CERT, HTTPD_KEY)
#endif
		) {
			f_read("/dev/urandom", &sn, sizeof(sn));
			sprintf(t, "%llu", sn & 0x7FFFFFFFFFFFFFFFULL);
			eval("gencert.sh", t);
		}
	} else {
		fprintf(fp, "ssl_enable=NO\n");
	}
#else
	fprintf(fp, "ssl_enable=NO\n");
#endif	// HTTPS

	append_custom_config("vsftpd.conf", fp);
	fclose(fp);

	use_custom_config("vsftpd.conf", "/etc/vsftpd.conf");
	run_postconf("vsftpd", "/etc/vsftpd.conf");
}

/*
 * st_ftp_modex: 0:no-ftp, 1:anonymous, 2:account
 */

void
start_ftpd(void)
{
	pid_t pid;
	char *vsftpd_argv[] = { "vsftpd", "/etc/vsftpd.conf", NULL };

	if (getpid() != 1) {
		notify_rc_after_wait("start_ftpd");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	if (!nvram_get_int("enable_ftp"))
		return;

	if (!sd_partition_num() && !nvram_match("usb_debug", "1"))
		return;

	write_ftpd_conf();

	killall("vsftpd", SIGHUP);

//#ifdef RTCONFIG_LANTIQ
#if 0
	if (!pids("vsftpd"))
		cpu_eval(NULL, "2", "vsftpd", "/etc/vsftpd.conf");
#else
	if (!pids("vsftpd")) {
#ifdef RTCONFIG_LANTIQ
		_eval_retry(vsftpd_argv, NULL, 0, &pid, "vsftpd");
#else
		_eval(vsftpd_argv, NULL, 0, &pid);
#endif
	}
#endif

	if (pids("vsftpd"))
		logmessage("FTP server", "daemon is started");
}

void stop_ftpd(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("stop_ftpd");
		return;
	}

	killall_tk("vsftpd");
	unlink("/tmp/vsftpd.conf");
	logmessage("FTP Server", "daemon is stopped");
}
#endif	// RTCONFIG_FTP

#if defined(RTCONFIG_TFTP_SERVER)
#define MIN_TFTPD_SPACE	(10 * 1024 * 1024)	/* unit: kilobytes */
void start_tftpd(void)
{
	pid_t pid;
	char lan_ip[sizeof("111.222.333.444XXX")];
	char tftpdir[PATH_MAX] = "";
	char *tftpd_argv[] = { "in.tftpd", "-4lcvs", "-u", "nobody", "--address", lan_ip, tftpdir, NULL };
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;

	if (getpid() != 1) {
		notify_rc_after_wait("start_ftpd");
		return;
	}

	disk_list = read_disk_data();
	if (disk_list == NULL){
		usb_dbg("Can't get any disk's information.\n");
		return;
	}
	strlcpy(lan_ip, nvram_get("lan_ipaddr"), sizeof(lan_ip));
	if (*lan_ip == '\0')
		strlcpy(lan_ip, "192.168.1.1", sizeof(lan_ip));

	/* Find first partition/disk that has tftpboot directory or 10GB available space if tftpdir directory is not available.  */
	for (disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		for (partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next, *tftpdir = '\0'){
			if (!partition_info->mount_point)
				continue;

			snprintf(tftpdir, sizeof(tftpdir), "%s/tftpboot", partition_info->mount_point);
			if (f_exists(tftpdir)) {
				usb_dbg("Skip device %s due to non-directory /tftpboot exist.\n", partition_info->device);
				continue;
			}

			if (!d_exists(tftpdir) && (partition_info->size_in_kilobytes - partition_info->used_kilobytes) < MIN_TFTPD_SPACE) {
				usb_dbg("Skip small device %s\n", partition_info->device);
				continue;
			}
			mkdir_if_none(tftpdir);
			if (!d_exists(tftpdir)) {
				usb_dbg("Skip device %s due to mkdir /tftpboot fail.\n", partition_info->device);
				continue;
			}
			break;
		}
		if (*tftpdir != '\0')
			break;
	}
	free_disk_data(&disk_list);

	if (*tftpdir == '\0') {
		usb_dbg("No suitable device for tftpd\n");
		return;
	}

	killall_tk("in.tftpd");
	if (!pids("in.tftpd"))
		_eval(tftpd_argv, NULL, 0, &pid);

	if (pids("in.tftpd"))
		logmessage("TFTP-HPA server", "daemon is started on %s", tftpdir);
}

void stop_tftpd(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("stop_tftpd");
		return;
	}

	killall_tk("in.tftpd");
	logmessage("TFTP-HPA Server", "daemon is stopped");
}
#endif	/* RTCONFIG_TFTP_SERVER */

// -----------------------------------------------------------------------------

// !!TB - Samba

#ifdef RTCONFIG_SAMBASRV
#ifdef RTCONFIG_PERMISSION_MANAGEMENT
#define PMS_GRP_DNAME     "default"       // group default name
#define PMS_GRP_DGID      "500"           // group default gid
void PMS_Encryption_PW(char *input, char *output, int size)
{
	char *p;
	char salt[32];
	char s[512];

	if (input == NULL || *input == '\0') {
		strlcpy(output, "", size);
		return;
	}

	strncpy(salt, "$1$", 3);
	f_read("/dev/urandom", s, 6);
	base64_encode((unsigned char *) s, salt + 3, 6);
	salt[3 + 8] = 0;
	p = salt;
	while (*p) {
		if (*p == '+') *p = '.';
		++p;
	}

	strlcpy(output, crypt(input, salt), size);
}

void create_custom_passwd(void)
{
	char output[35];
	int uid = 1000;
	int gid = 501;
	int default_set = 0;
	int is_first = 1;
	int acc_num, group_num;
	FILE *fps, *fpp;
	PMS_ACCOUNT_INFO_T *account_list, *follow_account;
	PMS_ACCOUNT_GROUP_INFO_T *group_list, *follow_group;
	char char_user[64];

	/* Get account / group list */
	if (PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0) {
		printf("%s: Can't read account / group list\n", __FUNCTION__);
		return;
	}

	/* write /etc/passwd.custom & /etc/shadow.custom */
	fps = fopen("/etc/shadow.custom", "w+");
	fpp = fopen("/etc/passwd.custom", "w+");
	if (fpp && fps) {
		is_first = 1;
		for (follow_account = account_list; follow_account != NULL; follow_account = follow_account->next) {
			if (is_first) {
				is_first = 0;
				continue;
			}

			if (follow_account->active) {
				PMS_Encryption_PW(follow_account->passwd, output, sizeof(output));

				memset(char_user, 0, sizeof(char_user));
				ascii_to_char_safe(char_user, follow_account->name, sizeof(char_user));

				fprintf(fps, "%s:%s:0:0:99999:7:0:0:\n", char_user, output);
				fprintf(fpp, "%s:x:%d:%s::/dev/null:/dev/null\n", char_user, uid, PMS_GRP_DGID);
				uid++;
			}
		}
	}
	if (fpp)
		fclose(fpp);
	if (fps)
		fclose(fps);

	chmod("/etc/shadow.custom", 0600);
	chmod("/etc/passwd.custom", 0644);

	/* write /etc/group.custom & /etc/ghsadow.custom */
	fps = fopen("/etc/gshadow.custom", "w+");
	fpp = fopen("/etc/group.custom", "w+");
	if (fpp && fps) {
		for (follow_group = group_list; follow_group != NULL; follow_group = follow_group->next) {
			memset(char_user, 0, sizeof(char_user));
			ascii_to_char_safe(char_user, follow_group->name, sizeof(char_user));

			if (!strcmp(char_user, PMS_GRP_DNAME) && default_set == 0) {
				fprintf(fps, "%s:*:%s:\n", PMS_GRP_DNAME, PMS_GRP_DGID);
				fprintf(fpp, "%s:x:%s:\n", PMS_GRP_DNAME, PMS_GRP_DGID);
				default_set = 1;
			}
			else if (!strcmp(char_user, nvram_safe_get("http_username"))) {
				continue;
			}
			else {
				PMS_OWNED_INFO_T *owned_account = follow_group->owned_account;

				fprintf(fps, "%s:*:%d:", char_user, gid);
				fprintf(fpp, "%s:x:%d:", char_user, gid);

				is_first = 1;
				while (owned_account != NULL) {
					PMS_ACCOUNT_GROUP_INFO_T *Account_owned = (PMS_ACCOUNT_GROUP_INFO_T *) owned_account->member;

					memset(char_user, 0, sizeof(char_user));
					ascii_to_char_safe(char_user, Account_owned->name, sizeof(char_user));

					fprintf(fps, is_first ? "%s" : ",%s", char_user);
					fprintf(fpp, is_first ? "%s" : ",%s", char_user);

					is_first = 0;
					owned_account = owned_account->next;
				}
				fprintf(fps, "\n");
				fprintf(fpp, "\n");
				gid++;
			}
		}
	}
	if (fps)
		fclose(fps);
	if (fpp)
		fclose(fpp);

	chmod("/etc/gshadow.custom", 0600);
	chmod("/etc/group.custom", 0644);

	/* free list */
	PMS_FreeAccInfo(&account_list, &group_list);
}
#else
void create_custom_passwd(void)
{
	FILE *fp;
	int result, n=0, i;
	int acc_num;
	char **account_list;
	int is_first = 1;

	/* write /etc/passwd.custom */
	fp = fopen("/etc/passwd.custom", "w+");
	if (!fp) {
		usb_dbg("Can't open /etc/passwd.custom\n");
		return;
	}
	result = get_account_list(&acc_num, &account_list);
	if (result < 0) {
		usb_dbg("Can't get the account list.\n");
		free_2_dimension_list(&acc_num, &account_list);
		return;
	}
	is_first = 1;
	for (i=0, n=500; i<acc_num; i++, n++)
	{
		if(is_first){
			is_first = 0;
			continue;
		}

		fprintf(fp, "%s:x:%d:%d::/dev/null:/dev/null\n", account_list[i], n, n);
	}
	fclose(fp);

	chmod("/etc/passwd.custom", 0644);

	/* write /etc/group.custom  */
	fp = fopen("/etc/group.custom", "w+");
	is_first = 1;
	for (i=0, n=500; i<acc_num; i++, n++)
	{
		if(is_first){
			is_first = 0;
			continue;
		}

		fprintf(fp, "%s:x:%d:\n", account_list[i], n);
	}
	fclose(fp);

	chmod("/etc/group.custom", 0644);

	free_2_dimension_list(&acc_num, &account_list);
}
#endif

static void kill_samba(int sig)
{
	if (sig == SIGTERM) {
		killall_tk("smbd");
		killall_tk("nmbd");
	}
	else {
		killall("smbd", sig);
		killall("nmbd", sig);
	}
}

#if defined(RTCONFIG_GROCTRL) && !defined(HND_ROUTER)
void enable_gro(int interval)
{
	char *argv[3] = {"echo", "", NULL};
	char lan_ifname[128], lan_ifnames[32], *next;
	char path[64] = {0};
	char parm[32] = {0};

	if(nvram_get_int("gro_disable"))
		return;

	/* enabled gso on vlan interface */
	snprintf(lan_ifnames, sizeof(lan_ifnames), "%s", nvram_safe_get("lan_ifnames"));
	foreach(lan_ifname, lan_ifnames, next) {
		if (!strncmp(lan_ifname, "vlan", 4)) {
			snprintf(path, sizeof(path), ">>/proc/net/vlan/%s", lan_ifname);
#if LINUX_KERNEL_VERSION == KERNEL_VERSION(2,6,36)
			snprintf(parm, sizeof(parm), "-gro %d", interval);
#else
			/* 131072 define the max length gro skb can chained */
			snprintf(parm, sizeof(parm), "-gro %d %d", interval, 131072);
#endif
			argv[1] = parm;
			 _eval(argv, path, 0, NULL);
		}
	}
}
#endif

#if defined(RTCONFIG_LANTIQ)
void tune_tso(void)
{
	f_write_string("/proc/sys/net/ipv4/tcp_min_tso_segs", "44", 0, 0);
}
#endif

#if 0
int suit_double_quote(const char *output, const char *input, int outsize){
	char *src = (char *)input;
	char *dst = (char *)output;
	char *end = (char *)output + outsize - 1;

	if(src == NULL || dst == NULL || outsize <= 0)
		return 0;

	for(; *src && dst < end; ++src){
		if(*src =='"'){
			if(dst+2 > end)
				break;

			*dst++ = '\\';
			*dst++ = *src;
		}
		else
			*dst++ = *src;
	}

	if(dst <= end)
		*dst = '\0';

	return dst-output;
}
#endif

#if 0
#ifdef RTCONFIG_BCMARM
extern void del_samba_rules(void);
extern void add_samba_rules(void);
#endif
#endif

void
start_samba(void)
{
	int acc_num;
	char cmd[256];
#if defined(SMP)
#if defined(GTAC5300)
	char *cpu_list = "3";
#else
	char *cpu_list = "1";
#endif
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_SOC_IPQ8064)
	int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
	int taskset_ret = -1;
#endif
#endif
	char smbd_cmd[32];

	if (getpid() != 1) {
		notify_rc_after_wait("start_samba");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	if (nvram_match("enable_samba", "0")) return;

	if ((!sd_partition_num() && !nvram_match("usb_debug", "1")) &&
	    (nvram_match("smbd_master", "0")) &&
	    (nvram_match("smbd_wins", "0"))) {
		return;
	}

#if defined(RTCONFIG_GROCTRL) && !defined(HND_ROUTER)
#if LINUX_KERNEL_VERSION >= KERNEL_VERSION(2,6,36)
	enable_gro(2);
#else
	enable_gro(1);
#endif
#endif
#if defined(RTCONFIG_LANTIQ)
	tune_tso();
#endif

#if 0
#ifdef RTCONFIG_BCMARM
	add_samba_rules();
#endif
#endif

#if defined(RTCONFIG_BCMARM) && !defined(HND_ROUTER)
	tweak_smp_affinity(1);
#endif

	mkdir_if_none("/var/run/samba");
	mkdir_if_none("/etc/samba");

	unlink("/etc/smb.conf");
	unlink("/etc/samba/smbpasswd");

	/* write samba configure file*/
	system("/sbin/write_smb_conf");

	/* write smbpasswd  */
	system("smbpasswd nobody \"\"");

#ifdef RTCONFIG_PERMISSION_MANAGEMENT
	PMS_ACCOUNT_INFO_T *account_list, *follow_account;
	PMS_ACCOUNT_GROUP_INFO_T *group_list;
	int group_num;

	if(PMS_GetAccountInfo(PMS_ACTION_GET_FULL, &account_list, &group_list, &acc_num, &group_num) < 0){
		printf("%s(PMS): Can't read account / group list\n", __FUNCTION__);
		PMS_FreeAccInfo(&account_list, &group_list);
		return;
	}

	for(follow_account = account_list; follow_account != NULL; follow_account = follow_account->next){
		if(!(follow_account->active))
			continue;

		char char_user[64], char_passwd[64], suit_user[64], suit_passwd[64];

		memset(char_user, 0, 64);
		ascii_to_char_safe(char_user, follow_account->name, 64);
		memset(suit_user, 0, 64);
		str_escape_quotes(suit_user, char_user, 64);

		memset(char_passwd, 0, 64);
		ascii_to_char_safe(char_passwd, follow_account->passwd, 64);
		memset(suit_passwd, 0, 64);
		str_escape_quotes(suit_passwd, char_passwd, 64);

		sprintf(cmd, "smbpasswd \"%s\" \"%s\"", suit_user, suit_passwd);

		system(cmd);
	}

	PMS_FreeAccInfo(&account_list, &group_list);
#else
	char *nv, *nvp, *b;
	char char_user[64], char_passwd[64], suit_user[64], suit_passwd[64];
	char *tmp_ascii_user, *tmp_ascii_passwd;
	int i;

	acc_num = nvram_get_int("acc_num");
	if(acc_num < 0)
		acc_num = 0;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
			if(vstrsep(b, ">", &tmp_ascii_user, &tmp_ascii_passwd) != 2)
				continue;

			memset(char_user, 0, 64);
			ascii_to_char_safe(char_user, tmp_ascii_user, 64);
			memset(suit_user, 0, 64);
			str_escape_quotes(suit_user, char_user, 64);
#ifdef RTCONFIG_NVRAM_ENCRYPT
			char dec_passwd[64];
			memset(dec_passwd, 0, sizeof(dec_passwd));
			pw_dec(tmp_ascii_passwd, dec_passwd);
			tmp_ascii_passwd = dec_passwd;
#endif
			memset(char_passwd, 0, 64);
			ascii_to_char_safe(char_passwd, tmp_ascii_passwd, 64);

			memset(suit_passwd, 0, 64);
			str_escape_quotes(suit_passwd, char_passwd, 64);
			sprintf(cmd, "smbpasswd \"%s\" \"%s\"", suit_user, suit_passwd);
			system(cmd);

			if(++i >= acc_num)
				break;
		}
	}
	if(nv)
		free(nv);
#endif

	xstart("nmbd", "-D", "-s", "/etc/smb.conf");

#if defined(RTCONFIG_TFAT) || defined(RTCONFIG_TUXERA_NTFS) || defined(RTCONFIG_TUXERA_HFS)
	if(nvram_get_int("enable_samba_tuxera") == 1)
		snprintf(smbd_cmd, 32, "%s/smbd", "/usr/bin");
	else
		snprintf(smbd_cmd, 32, "%s/smbd", "/usr/sbin");
#else
	snprintf(smbd_cmd, 32, "%s/smbd", "/usr/sbin");
#endif

#if defined(SMP) && !defined(HND_ROUTER)
#if defined(RTCONFIG_BCMARM) || defined(RTCONFIG_SOC_IPQ8064)
#if 0
	if(cpu_num > 1)
		taskset_ret = cpu_eval(NULL, "1", "ionice", "-c1", "-n0", smbd_cmd, "-D", "-s", "/etc/smb.conf");
	else
		taskset_ret = eval("ionice", "-c1", "-n0", smbd_cmd, "-D", "-s", "/etc/smb.conf");
#else
	if(!nvram_match("stop_taskset", "1")){
		if(cpu_num > 1)
			taskset_ret = cpu_eval(NULL, cpu_list, smbd_cmd, "-D", "-s", "/etc/smb.conf");
		else
			taskset_ret = eval(smbd_cmd, "-D", "-s", "/etc/smb.conf");
	}
#endif

	if(taskset_ret != 0)
#endif
#endif
#ifdef RTCONFIG_ALPINE
		cpu_eval(NULL, "3", smbd_cmd, "-D", "-s", "/etc/smb.conf");
#elif defined(RTCONFIG_LANTIQ)
		cpu_eval(NULL, "1", smbd_cmd, "-D", "-s", "/etc/smb.conf");
#else
		xstart(smbd_cmd, "-D", "-s", "/etc/smb.conf");
#endif

	start_wsdd();

	logmessage("Samba Server", "daemon is started");

	return;
}

void stop_samba(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("stop_samba");
		return;
	}

	stop_wsdd();
	kill_samba(SIGTERM);
	/* clean up */
	unlink("/var/log/smb");
	unlink("/var/log/nmb");

	eval("rm", "-rf", "/var/run/samba");

	logmessage("Samba Server", "smb daemon is stopped");

#if defined(RTCONFIG_BCMARM) && !defined(HND_ROUTER)
	tweak_smp_affinity(0);
#endif

#if 0
#ifdef RTCONFIG_BCMARM
	del_samba_rules();
#endif
#endif

#if defined(RTCONFIG_GROCTRL) && !defined(HND_ROUTER)
	enable_gro(0);
#endif
}
#endif	// RTCONFIG_SAMBASRV

#ifdef RTCONFIG_MEDIA_SERVER
#define MEDIA_SERVER_APP	"minidlna"

/*
 * 1. if (dms_dbdir) exist and files.db there, use it
 * 2. find the first and the largest write-able directory in /tmp/mnt
 * 3. /var/cache/minidlna
 */
int find_dms_dbdir_candidate(char *dbdir)
{
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info, *picked;
	u64 max_size = 256*1024;
	int found;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		cprintf("Can't get any disk's information.\n");
		return 0;
	}

	found = 0;
	picked = NULL;

	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next) {
		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			if(partition_info->mount_point == NULL){
				cprintf("Skip if it can't be mounted.\n");
				continue;
			}
			if(strncmp(partition_info->permission, "rw", 2)!=0) {
				cprintf("Skip if permission is not rw\n");
				continue;
			}

			if(partition_info->size_in_kilobytes > max_size && (partition_info->size_in_kilobytes - partition_info->used_kilobytes)>128*1024) {
				max_size = partition_info->size_in_kilobytes;
				picked = partition_info;
			}
		}
	}
	if(picked && picked->mount_point) {
		strcpy(dbdir, picked->mount_point);
		found = 1;
	}

	free_disk_data(&disk_list);

	return found;
}

void find_dms_dbdir(char *dbdir)
{
	char dbdir_t[128], dbfile[128];
	int found=0;

  	strcpy(dbdir_t, nvram_safe_get("dms_dbdir"));

	/* if previous dms_dbdir there, use it */
	if(!strcmp(dbdir_t, nvram_default_get("dms_dbdir"))) {
		sprintf(dbfile, "%s/files.db", dbdir_t);
		if (check_if_file_exist(dbfile)) {
			strcpy(dbdir, dbdir_t);
			found = 1;
		}
	}

	/* find the first write-able directory */
	if(!found && find_dms_dbdir_candidate(dbdir_t)) {
		sprintf(dbdir, "%s/.minidlna", dbdir_t);
		found = 1;
	}

 	/* use default dir */
	if(!found)
		strcpy(dbdir, nvram_default_get("dms_dbdir"));

	nvram_set("dms_dbdir", dbdir);

	return;
}

#define TYPE_AUDIO	0x01
#define TYPE_VIDEO	0x02
#define TYPE_IMAGES	0x04
#define ALL_MEDIA	0x07

void start_dms(void)
{
	FILE *f;
	int port, pid;
	char dbdir[100];
	char *argv[] = { MEDIA_SERVER_APP, "-f", "/etc/"MEDIA_SERVER_APP".conf", "-R", NULL, NULL, NULL };
	static int once = 1;
	unsigned char ea[ETHER_ADDR_LEN];
	char serial[18], uuid[37];
	char *nv, *nvp, *b, *c;
	char *nv2, *nvp2;
	unsigned char type = 0;
	char types[5];
	int j, index = 4;

	if (getpid() != 1) {
		notify_rc("start_dms");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

#if !(defined(HND_ROUTER) && defined(RTCONFIG_HNDMFG))
	if (!sd_partition_num() && !nvram_match("usb_debug", "1"))
#endif
		return;

	if (!is_routing_enabled() && !is_lan_connected())
		set_invoke_later(INVOKELATER_DMS);

	if (nvram_get_int("dms_sas") == 0)
		once = 0;

	if (nvram_get_int("dms_enable") != 0) {
#if 1
		if ((nvram_get_int("dms_rebuild") == 0)) {
			argv[index - 1] = "-r";
		}
#else
		if ((!once) && (nvram_get_int("dms_rescan") == 0)) {
			// no forced rescan
			argv[--index] = NULL;
		}
#endif

		if ((f = fopen(argv[2], "w")) != NULL) {
			port = nvram_get_int("dms_port");
			// dir rule:
			// default: dmsdir=/tmp/mnt, dbdir=/var/cache/minidlna
			// after setting dmsdir, dbdir="dmsdir"/minidlna

			if (!nvram_get_int("dms_dir_manual")) {
				nvram_set("dms_dir_x", "");
				nvram_set("dms_dir_type_x", "");
			}

			nv = nvp = nvram_get_int("dms_dir_manual") ?
				strdup(nvram_safe_get("dms_dir_x")) :
				strdup(nvram_default_get("dms_dir_x"));
			nv2 = nvp2 = nvram_get_int("dms_dir_manual") ?
				strdup(nvram_safe_get("dms_dir_type_x")) :
				strdup(nvram_default_get("dms_dir_type_x"));

			memset(dbdir, 0, sizeof(dbdir));
			find_dms_dbdir(dbdir);

			if (strlen(dbdir))
				mkdir_if_none(dbdir);
			if (!check_if_dir_exist(dbdir)) {
				strcpy(dbdir, nvram_default_get("dms_dbdir"));
				mkdir_if_none(dbdir);
			}

			nvram_set("dms_dbcwd", dbdir);

			if (!ether_atoe(get_lan_hwaddr(), ea))
				f_read("/dev/urandom", ea, sizeof(ea));
			snprintf(serial, sizeof(serial), "%02x:%02x:%02x:%02x:%02x:%02x",
				 ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);
			snprintf(uuid, sizeof(uuid), "4d696e69-444c-164e-9d41-%02x%02x%02x%02x%02x%02x",
				 ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);

			fprintf(f,
				"network_interface=%s\n"
				"port=%d\n"
				"friendly_name=%s\n"
				"db_dir=%s\n"
				"enable_tivo=%s\n"
				"strict_dlna=%s\n"
				"inotify=yes\n"
				"notify_interval=600\n"
				"album_art_names=Cover.jpg/cover.jpg/Thumb.jpg/thumb.jpg\n",
				nvram_safe_get("lan_ifname"),
				(port < 0) || (port >= 0xffff) ? 0 : port,
				is_valid_hostname(nvram_get("dms_friendly_name")) ? nvram_get("dms_friendly_name") : get_productid(),
				dbdir,
				nvram_get_int("dms_tivo") ? "yes" : "no",
				nvram_get_int("dms_stdlna") ? "yes" : "no");

			fprintf(f, "presentation_url=");
#ifdef RTCONFIG_HTTPS
			if (nvram_get_int("http_enable") == 1) {
				fprintf(f, "%s://%s:%d/\n", "https", nvram_safe_get("lan_ipaddr"), nvram_get_int("https_lanport") ? : 443);
			} else
#endif
			{
				fprintf(f, "%s://%s:%d/\n", "http", nvram_safe_get("lan_ipaddr"), nvram_get_int("http_lanport") ? : 80);
			}

			if (!nvram_get_int("dms_dir_manual"))
				fprintf(f, "media_dir=%s\n", nvram_default_get("dms_dir"));
			else
			while ((b = strsep(&nvp, "<")) != NULL && (c = strsep(&nvp2, "<")) != NULL) {
				if (!strlen(b)) continue;
				if (!strlen(c)) continue;

				type = 0;
				while (*c) {
					if (*c == ',')
						break;

					if (*c == 'A' || *c == 'a')
						type |= TYPE_AUDIO;
					else if (*c == 'V' || *c == 'v')
						type |= TYPE_VIDEO;
					else if (*c == 'P' || *c == 'p')
						type |= TYPE_IMAGES;
					else
						type = ALL_MEDIA;

					c++;
				}

				if (type == ALL_MEDIA)
					types[0] = 0;
				else {
					j = 0;
					if (type & TYPE_AUDIO)
						types[j++] = 'A';
					if (type & TYPE_VIDEO)
						types[j++] = 'V';
					if (type & TYPE_IMAGES)
						types[j++] = 'P';

					types[j++] =  ',';
					types[j] =  0;
				}

				if ((strlen(b) <= PATH_MAX) && check_if_dir_exist(b))
					fprintf(f, "media_dir=%s%s\n", types, b);
			}

			if (nv) free(nv);
			if (nv2) free(nv2);

			fprintf(f,
				"serial=%s\n"
				"uuid=%s\n"
				"model_number=%s\n",
				serial, uuid,
				rt_serialno);

			nv = nvram_safe_get("dms_sort");
			if (!*nv || isdigit(*nv))
				nv = (!*nv || atoi(nv)) ? "+upnp:class,+upnp:originalTrackNumber,+dc:title" : NULL;
			if (nv)
				fprintf(f, "force_sort_criteria=%s\n", nv);

			append_custom_config(MEDIA_SERVER_APP".conf",f);

			fclose(f);
		}

		if (nvram_get_int("dms_dbg"))
			argv[index++] = "-v";

		if (nvram_get_int("dms_web"))
			argv[index++] = "-W";

		use_custom_config(MEDIA_SERVER_APP".conf","/etc/"MEDIA_SERVER_APP".conf");
		run_postconf(MEDIA_SERVER_APP, "/etc/"MEDIA_SERVER_APP".conf");

		/* start media server if it's not already running */
		if (pidof(MEDIA_SERVER_APP) <= 0) {
			if ((_eval(argv, NULL, 0, &pid) == 0) && (once)) {
				/* If we started the media server successfully, wait 1 sec
				 * to let it die if it can't open the database file.
				 * If it's still alive after that, assume it's running and
				 * disable forced once-after-reboot rescan.
				 */
				sleep(1);
				if (pidof(MEDIA_SERVER_APP) > 0)
					once = 0;
			}
		}
	}
	else killall_tk(MEDIA_SERVER_APP);
}

void stop_dms(void)
{
	if (getpid() != 1) {
		notify_rc("stop_dms");
		return;
	}

	// keep dms always run except for disabling it
	// killall_tk(MEDIA_SERVER_APP);
}

// it is called by rc only
void force_stop_dms(void)
{
	killall_tk(MEDIA_SERVER_APP);
#if 0
	eval("rm", "-rf", nvram_safe_get("dms_dbcwd"));
#endif
}

void
write_mt_daapd_conf(char *servername)
{
	FILE *fp;
	char dmsdir[PATH_MAX], *ptr;
	char dbdir[128], dbdir_t[128];

	if (check_if_file_exist("/etc/mt-daapd.conf"))
		return;

	fp = fopen("/etc/mt-daapd.conf", "w");

	if (fp == NULL)
		return;

	snprintf(dmsdir, sizeof(dmsdir), "%s", nvram_safe_get("dms_dir"));
	if (!check_if_dir_exist(dmsdir))
		snprintf(dmsdir, sizeof(dmsdir), "%s", nvram_default_get("dms_dir"));

#if 1
	char *http_passwd = nvram_safe_get("http_passwd");
#ifdef RTCONFIG_NVRAM_ENCRYPT
	int declen = pw_dec_len(http_passwd);
	char dec_passwd[declen];
	memset(dec_passwd, 0, sizeof(dec_passwd));
	pw_dec(http_passwd, dec_passwd);
	http_passwd = dec_passwd;
#endif
	memset(dbdir, 0, sizeof(dbdir));
	if (find_dms_dbdir_candidate(dbdir_t))
		sprintf(dbdir, "%s/.mt-daapd", dbdir_t);

	ptr = strlen(dbdir) ? dbdir : "/var/cache/mt-daapd";
	nvram_set("daapd_dbdir", ptr);

	fprintf(fp, "web_root /etc/web\n");
	fprintf(fp, "port 3689\n");
	fprintf(fp, "admin_pw %s\n", http_passwd);
	fprintf(fp, "db_dir %s\n", ptr);
	fprintf(fp, "mp3_dir %s\n", dmsdir);
	fprintf(fp, "servername %s\n", servername);
	fprintf(fp, "runas %s\n", nvram_safe_get("http_username"));
	fprintf(fp, "extensions .mp3,.m4a,.m4p,.aac,.ogg\n");
	fprintf(fp, "rescan_interval 300\n");
	fprintf(fp, "always_scan 1\n");
	fprintf(fp, "compress 1\n");
#else
	fprintf(fp, "[general]\n");
	fprintf(fp, "web_root = /rom/mt-daapd/admin-root\n");
	fprintf(fp, "port = 3689\n");
	fprintf(fp, "admin_pw = %s\n", nvram_safe_get("http_passwd"));
	fprintf(fp, "db_type = sqlite3\n");
	fprintf(fp, "db_parms = /var/cache/mt-daapd\n");
	fprintf(fp, "mp3_dir = %s\n", "/mnt");
	fprintf(fp, "servername = %s\n", servername);
	fprintf(fp, "runas = %s\n", nvram_safe_get("http_username"));
	fprintf(fp, "extensions = .mp3,.m4a,.m4p\n");
	fprintf(fp, "scan_type = 2\n");
	fprintf(fp, "[plugins]\n");
	fprintf(fp, "plugin_dir = /rom/mt-daapd/plugins\n");
	fprintf(fp, "[scanning]\n");
	fprintf(fp, "process_playlists = 1\n");
	fprintf(fp, "process_itunes = 1\n");
	fprintf(fp, "process_m3u = 1\n");
#endif
	fclose(fp);
}

void
start_mt_daapd()
{
	char servername[32];

	if (getpid() != 1) {
		notify_rc("start_mt_daapd");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	if (nvram_invmatch("daapd_enable", "1"))
		return;

	if (!sd_partition_num() && !nvram_match("usb_debug", "1"))
		return;

	if (is_valid_hostname(nvram_safe_get("daapd_friendly_name")))
		strncpy(servername, nvram_safe_get("daapd_friendly_name"), sizeof(servername));
	else
		servername[0] = '\0';
	if (strlen(servername)==0) strncpy(servername, get_productid(), sizeof(servername));
		write_mt_daapd_conf(servername);

	if (pids("mt-daapd")) {
		killall_tk("mt-daapd");

		if (check_if_dir_exist(nvram_safe_get("daapd_dbdir")))
			eval("rm", "-rf", nvram_safe_get("daapd_dbdir"));
	}

#if !(defined(RTCONFIG_TIMEMACHINE) || defined(RTCONFIG_MDNS))
	if (is_routing_enabled())
#endif
	{
		eval("mt-daapd", "-m");
#if defined(RTCONFIG_TIMEMACHINE) || defined(RTCONFIG_MDNS)
		restart_mdns();
#else
		doSystem("mDNSResponder %s thehost %s _daap._tcp. 3689 &", nvram_safe_get("lan_ipaddr"), servername);
#endif
	}
#if !(defined(RTCONFIG_TIMEMACHINE) || defined(RTCONFIG_MDNS))
	else
		eval("mt-daapd");
#endif

	if (pids("mt-daapd"))
	{
		logmessage("iTunes Server", "daemon is started");

		return;
	}

	return;
}

void
stop_mt_daapd()
{
	if(getpid()!=1) {
		notify_rc("stop_mt_daapd");
		return;
	}

	if(!pids("mt-daapd") &&
#if defined(RTCONFIG_TIMEMACHINE) || defined(RTCONFIG_MDNS)
			!pids("avahi-daemon")
#else
			!pids("mDNSResponder")
#endif
			)
		return;

#if defined(RTCONFIG_TIMEMACHINE) || defined(RTCONFIG_MDNS)
	if (pids("avahi-daemon"))
		restart_mdns();
#else
	if (pids("mDNSResponder"))
		killall_tk("mDNSResponder");
#endif

	if (pids("mt-daapd"))
		killall_tk("mt-daapd");

	unlink("/etc/mt-daapd.conf");

	if (check_if_dir_exist(nvram_safe_get("daapd_dbdir")))
		eval("rm", "-rf", nvram_safe_get("daapd_dbdir"));

	logmessage("iTunes", "daemon is stopped");
}
#endif
#endif	// RTCONFIG_MEDIA_SERVER

// -------------

// !!TB - webdav

//#ifdef RTCONFIG_WEBDAV
#if 0
void write_webdav_permissions()
{
	FILE *fp;
	int acc_num = 0, i;
	char *nv, *nvp, *b;
	char *tmp_ascii_user, *tmp_ascii_passwd;

	/* write /tmp/lighttpd/permissions */
	fp = fopen("/tmp/lighttpd/permissions", "w");
	if (fp==NULL) return;

	acc_num = nvram_get_int("acc_num");
	if(acc_num < 0)
		acc_num = 0;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
			if(vstrsep(b, ">", &tmp_ascii_user, &tmp_ascii_passwd) != 2)
				continue;

			char char_user[64], char_passwd[64];

			memset(char_user, 0, 64);
			ascii_to_char_safe(char_user, tmp_ascii_user, 64);
			memset(char_passwd, 0, 64);
#ifdef RTCONFIG_NVRAM_ENCRYPT
			int declen = pw_dec_len(tmp_ascii_passwd);
			char dec_passwd[declen];
			memset(dec_passwd, 0, sizeof(dec_passwd));
			pw_dec(tmp_ascii_passwd, dec_passwd);
			tmp_ascii_passwd = dec_passwd;
#endif
			ascii_to_char_safe(char_passwd, tmp_ascii_passwd, 64);

			fprintf(fp, "%s:%s\n", char_user, char_passwd);

			if(++i >= acc_num)
				break;
		}
	}
	if(nv)
		free(nv);

	fclose(fp);
}

void write_webdav_server_pem()
{
	unsigned long long sn;
	char t[32];

	if(!f_exists("/etc/server.pem")){
		f_read("/dev/urandom", &sn, sizeof(sn));
		sprintf(t, "%llu", sn & 0x7FFFFFFFFFFFFFFFULL);
		eval("gencert.sh", t);
	}
}
#endif

void start_webdav(void)	// added by Vanic
{
#ifdef RTCONFIG_WEBDAV
	pid_t pid, pid1;
	char *lighttpd_argv[] = { "/usr/sbin/lighttpd", "-f", "/tmp/lighttpd.conf", "-D", NULL };
	char *lighttpd_monitor_argv[] = { "/usr/sbin/lighttpd-monitor", NULL };
#endif

	if(getpid()!=1) {
		notify_rc("start_webdav");
		return;
	}
/*
#ifndef RTCONFIG_WEBDAV
	system("sh /opt/etc/init.d/S50aicloud scan");
#else
*/
	if (nvram_get_int("webdav_aidisk") || nvram_get_int("webdav_proxy"))
		nvram_set("enable_webdav", "1");
	else if (!nvram_get_int("webdav_aidisk") && !nvram_get_int("webdav_proxy") && nvram_get_int("enable_webdav"))
	{
		// enable both when enable_webdav by other apps
		nvram_set("webdav_aidisk", "1");
		nvram_set("webdav_proxy", "1");
	}

	if (nvram_match("enable_webdav", "0")) return;

/* Independent from AiCloud
ifdef RTCONFIG_TUNNEL
	//- start tunnel
	start_mastiff();
#endif*/

#ifndef RTCONFIG_WEBDAV
	if(f_exists("/opt/etc/init.d/S50aicloud"))
		system("sh /opt/etc/init.d/S50aicloud scan");
#else
	/* WebDav directory */
	mkdir_if_none("/tmp/lighttpd");
	mkdir_if_none("/tmp/lighttpd/uploads");
	chmod("/tmp/lighttpd/uploads", 0777);
	mkdir_if_none("/tmp/lighttpd/www");
	chmod("/tmp/lighttpd/www", 0777);

	/* tmp/lighttpd/permissions */
	//write_webdav_permissions();

	/* WebDav SSL support */
	//write_webdav_server_pem();
	if(f_size(LIGHTTPD_CERTKEY) != f_size(HTTPD_KEY) + f_size(HTTPD_CERT))
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "cat %s %s > %s", HTTPD_KEY, HTTPD_CERT, LIGHTTPD_CERTKEY);
		system(buf);
	}

	/* write WebDav configure file*/
	system("/sbin/write_webdav_conf");

	if (!f_exists("/tmp/lighttpd.conf")) return;

	if (!pids("lighttpd")){
		_eval(lighttpd_argv, NULL, 0, &pid);
	}
	if (!pids("lighttpd-monitor")){
		_eval(lighttpd_monitor_argv, NULL, 0, &pid1);
	}

	if (pids("lighttpd"))
		logmessage("WEBDAV server", "daemon is started");
#endif
}

void stop_webdav(void)
{
	if (getpid() != 1) {
		notify_rc("stop_webdav");
		return;
	}

#ifndef RTCONFIG_WEBDAV
	if(f_exists("/opt/etc/init.d/S50aicloud"))
		system("sh /opt/etc/init.d/S50aicloud scan");
#else
	if (pids("lighttpd-monitor")){
		kill_pidfile_tk("/tmp/lighttpd/lighttpd-monitor.pid");
		unlink("/tmp/lighttpd/lighttpd-monitor.pid");
	}

	if (pids("lighttpd")){
		kill_pidfile_tk("/tmp/lighttpd/lighttpd.pid");
		// charles: unlink lighttpd.conf will cause lighttpd error
		//	we should re-write lighttpd.conf
		system("/sbin/write_webdav_conf");

		unlink("/tmp/lighttpd/lighttpd.pid");
	}

	if (pids("lighttpd-arpping")){
		kill_pidfile_tk("/tmp/lighttpd/lighttpd-arpping.pid");
		unlink("/tmp/lighttpd/lighttpd-arpping.pid");
	}

	logmessage("WEBDAV Server", "daemon is stopped");
#endif

/* Independent from AiCloud
#ifdef RTCONFIG_TUNNEL
        //- stop tunnel
        stop_mastiff();
#endif*/

}
//#endif	// RTCONFIG_WEBDAV

#ifdef RTCONFIG_WEBDAV
void stop_all_webdav(void)
{
	if (getpid() != 1) {
		notify_rc("stop_webdav");
		return;
	}

	stop_webdav();

	if (pids("lighttpd-arpping")){
		kill_pidfile_tk("/tmp/lighttpd/lighttpd-arpping.pid");
		unlink("/tmp/lighttpd/lighttpd-arpping.pid");
	}
	logmessage("WEBDAV Server", "arpping daemon is stopped");
}
#endif

//#ifdef RTCONFIG_CLOUDSYNC
void start_cloudsync(int fromUI)
{
	char word[PATH_MAX], *next_word;
	char *b, *nvp, *nv;
	int type = 0, enable = 0;
	char username[64], sync_dir[PATH_MAX];
	int count;
	char cloud_token[PATH_MAX];
	char mounted_path[PATH_MAX], *ptr, *other_path;
	int pid, s = 3;
	char *cmd1_argv[] = { "nice", "-n", "10", "inotify", NULL };
	char *cmd2_argv[] = { "nice", "-n", "10", "asuswebstorage", NULL };
	char *cmd3_argv[] = { "touch", cloud_token, NULL };
	char *cmd4_argv[] = { "nice", "-n", "10", "webdav_client", NULL };
	char *cmd5_argv[] = { "nice", "-n", "10", "dropbox_client", NULL };
	char *cmd6_argv[] = { "nice", "-n", "10", "ftpclient", NULL};
	char *cmd7_argv[] = { "nice", "-n", "10", "sambaclient", NULL};
	char *cmd8_argv[] = { "nice", "-n", "10", "usbclient", NULL};
    char *cmd9_argv[] = { "nice", "-n", "10", "google_client", NULL };
	char buf[32];

	memset(buf, 0, 32);
	sprintf(buf, "start_cloudsync %d", fromUI);

	if(getpid()!=1) {
		notify_rc(buf);
		return;
	}

	if(nvram_match("enable_cloudsync", "0")){
		logmessage("Cloudsync client", "manually disabled all rules");
		return;
	}

	/* If total memory size < 200MB, reduce priority of inotify, asuswebstorage, webdavclient, etc. */
	if (get_meminfo_item("MemTotal") < 200*1024)
		s = 0;

	nv = nvp = strdup(nvram_safe_get("cloud_sync"));
	if(nv){
		while((b = strsep(&nvp, "<")) != NULL){
			count = 0;
			foreach_62(word, b, next_word){
				switch(count){
					case 0: // type
						type = atoi(word);
						break;
				}
				++count;
			}

			if(type == 1){
				if(!pids("inotify"))
					_eval(&cmd1_argv[s], NULL, 0, &pid);

				if(!pids("webdav_client")){
					_eval(&cmd4_argv[s], NULL, 0, &pid);
					sleep(2); // wait webdav_client.
				}

				if(pids("inotify") && pids("webdav_client"))
					logmessage("Webdav client", "daemon is started");
			}
			else if(type == 3){
				if(!pids("inotify"))
					_eval(cmd1_argv, NULL, 0, &pid);

				if(!pids("dropbox_client")){
					_eval(cmd5_argv, NULL, 0, &pid);
					sleep(2); // wait dropbox_client.
				}

				if(pids("inotify") && pids("dropbox_client"))
					logmessage("dropbox client", "daemon is started");
			}
			else if(type == 2){
				if(!pids("inotify"))
					_eval(cmd1_argv, NULL, 0, &pid);

				if(!pids("ftpclient")){
					_eval(cmd6_argv, NULL, 0, &pid);
					sleep(2); // wait ftpclient.
				}

				if(pids("inotify") && pids("ftpclient"))
					logmessage("ftp client", "daemon is started");
			}
			else if(type == 4){
				if(!pids("inotify"))
					_eval(cmd1_argv, NULL, 0, &pid);

				if(!pids("sambaclient")){
					_eval(cmd7_argv, NULL, 0, &pid);
					sleep(2); // wait sambaclient.
				}

				if(pids("inotify") && pids("sambaclient"))
					logmessage("sambaclient", "daemon is started");
			}
			else if(type == 5){
				if(!pids("inotify"))
					_eval(cmd1_argv, NULL, 0, &pid);

				if(!pids("usbclient")){
					_eval(cmd8_argv, NULL, 0, &pid);
					sleep(2); // wait usbclient.
				}

				if(pids("inotify") && pids("usbclient"))
					logmessage("usbclient", "daemon is started");
			}
	          else if(type == 6){
                if(!pids("inotify"))
                    _eval(cmd1_argv, NULL, 0, &pid);

                if(!pids("google_client")){
                    _eval(cmd9_argv, NULL, 0, &pid);
                    sleep(2); // wait google_client.
                }
                if(pids("inotify") && pids("google_client"))
                {
                    logmessage("google client", "daemon is started");
                }
            }
			else if(type == 0){
				char *b_bak, *ptr_b_bak;
				ptr_b_bak = b_bak = strdup(b);
				for(count = 0, next_word = strsep(&b_bak, ">"); next_word != NULL; ++count, next_word = strsep(&b_bak, ">")){
					switch(count){
						case 0: // type
							type = atoi(next_word);
							break;
						case 1: // username
							memset(username, 0, 64);
							strncpy(username, next_word, 64);
							break;
						case 5: // dir
							memset(sync_dir, 0, PATH_MAX);
							strncpy(sync_dir, next_word, PATH_MAX);
							break;
						case 6: // enable
							enable = atoi(next_word);
							break;
					}
				}
				free(ptr_b_bak);

				if(!enable){
					logmessage("Cloudsync client", "manually disabled");
					continue;
				}

				ptr = sync_dir+strlen(POOL_MOUNT_ROOT)+1;
				if((other_path = strchr(ptr, '/')) != NULL){
					ptr = other_path;
					++other_path;
				}
				else
					ptr = "";

				memset(mounted_path, 0, PATH_MAX);
				strncpy(mounted_path, sync_dir, (strlen(sync_dir)-strlen(ptr)));

				FILE *fp;
				char check_target[PATH_MAX], line[PATH_MAX];
				int got_mount = 0;

				memset(check_target, 0, PATH_MAX);
				sprintf(check_target, " %s ", mounted_path);

				if((fp = fopen(MOUNT_FILE, "r")) == NULL){
					logmessage("Cloudsync client", "Could read the disk's data");
					return;
				}

				while(fgets(line, sizeof(line), fp) != NULL){
					if(strstr(line, check_target)){
						got_mount = 1;
						break;
					}
				}
				fclose(fp);

				if(!got_mount){
					logmessage("Cloudsync client", "The specific disk isn't existed");
					continue;
				}

				if(strlen(sync_dir))
					mkdir_if_none(sync_dir);

				memset(cloud_token, 0, PATH_MAX);
				snprintf(cloud_token, PATH_MAX, "%s/.__cloudsync_%d_%s.txt", mounted_path, type, username);
				if(!fromUI && !check_if_file_exist(cloud_token)){
_dprintf("start_cloudsync: No token file.\n");
					continue;
				}

				_eval(cmd3_argv, NULL, 0, NULL);

				if(!pids("inotify"))
					_eval(&cmd1_argv[s], NULL, 0, &pid);

				if(!pids("asuswebstorage")){
					_eval(&cmd2_argv[s], NULL, 0, &pid);
					sleep(2); // wait asuswebstorage.
				}

				if(pids("inotify") && pids("asuswebstorage"))
					logmessage("Cloudsync client", "daemon is started");
			}
		}
		free(nv);
	}
}

void stop_cloudsync(int type)
{
	char buf[32];

	memset(buf, 0, 32);
	sprintf(buf, "stop_cloudsync %d", type);

	if(getpid()!=1) {
		notify_rc(buf);
		return;
	}

	if(type == 1){
		if(pids("inotify") && !pids("asuswebstorage") && !pids("dropbox_client") && !pids("ftpclient") && !pids("sambaclient") && !pids("usbclient")&& !pids("google_client"))
			killall_tk("inotify");

		if(pids("webdav_client"))
			killall_tk("webdav_client");

		logmessage("Webdav_client", "daemon is stopped");
	}
	else if(type == 2){
		if(pids("inotify") && !pids("asuswebstorage") && !pids("dropbox_client") && !pids("webdav_client") && !pids("sambaclient") && !pids("usbclient")&& !pids("google_client"))
			killall_tk("inotify");

		if(pids("ftpclient"))
			killall_tk("ftpclient");

		logmessage("ftp client", "daemon is stopped");
	}
	else if(type == 3){
		if(pids("inotify") && !pids("asuswebstorage") && !pids("webdav_client") && !pids("ftpclient") && !pids("sambaclient") && !pids("usbclient")&& !pids("google_client"))
			killall_tk("inotify");

		if(pids("dropbox_client"))
			killall_tk("dropbox_client");

		logmessage("dropbox_client", "daemon is stopped");
	}
	else if(type == 4){
		if(pids("inotify") && !pids("asuswebstorage") && !pids("webdav_client") && !pids("ftpclient") && !pids("dropbox_client") && !pids("usbclient")&& !pids("google_client"))
			killall_tk("inotify");

		if(pids("sambaclient"))
			killall_tk("sambaclient");

		logmessage("sambaclient", "daemon is stopped");
	}
	else if(type == 5){
		if(pids("inotify") && !pids("asuswebstorage") && !pids("webdav_client") && !pids("ftpclient") && !pids("dropbox_client") && !pids("sambaclient")&& !pids("google_client"))
			killall_tk("inotify");

		if(pids("usbclient"))
			killall_tk("usbclient");

		logmessage("usbclient", "daemon is stopped");
	}
	else if(type == 6){

        if(pids("inotify") && !pids("asuswebstorage") && !pids("webdav_client") && !pids("ftpclient") && !pids("sambaclient") && !pids("dropbox_client")&&!pids("usbclient"))
            killall_tk("inotify");

        if(pids("google_client"))
        {
            killall_tk("google_client");
        }

        logmessage("google_client", "daemon is stopped");

    }
	else if(type == 0){
		if(pids("inotify") && !pids("webdav_client") && !pids("dropbox_client") && !pids("ftpclient") && !pids("sambaclient")  && !pids("usbclient")&& !pids("google_client"))
			killall_tk("inotify");

		if(pids("asuswebstorage"))
			killall_tk("asuswebstorage");
		logmessage("Cloudsync client", "daemon is stoped");
	}
	else{
	if(pids("inotify"))
			killall_tk("inotify");

	if(pids("webdav_client"))
			killall_tk("webdav_client");

		if(pids("asuswebstorage"))
			killall_tk("asuswebstorage");

	if(pids("dropbox_client"))
			killall_tk("dropbox_client");

	if(pids("ftpclient"))
			killall_tk("ftpclient");

	if(pids("sambaclient"))
			killall_tk("sambaclient");

	if(pids("usbclient"))
			killall_tk("usbclient");

	if(pids("google_client"))
    {           
            killall_tk("google_client");
              //system("killall google_client");
    }

		logmessage("Cloudsync client and Webdav_client and dropbox_client ftp_client sambaclient usb_client google_client", "daemon is stopped");
	}
}
//#endif

#ifdef RTCONFIG_USB
int sd_partition_num()
{
	FILE *procpt;
	char line[128], ptname[32];
	int ma, mi, sz;
	int count = 0;

	if ((procpt = fopen("/proc/partitions", "r"))) {
		while (fgets(line, sizeof(line), procpt)) {
			if (sscanf(line, " %d %d %d %[^\n ]", &ma, &mi, &sz, ptname) != 4)
				continue;

			if (!strncmp(ptname, "sd", 2))
				count++;
		}
	}

	return count;
}

void start_nas_services(int force)
{
	if(!force && getpid() != 1){
		notify_rc_after_wait("start_nasapps");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

#ifdef RTCONFIG_MODEM_BRIDGE
	if(sw_mode() == SW_MODE_AP && nvram_get_int("modem_bridge"))
		return;
#endif

	if(!check_if_dir_empty("/mnt"))
	{
#ifdef RTCONFIG_WEBDAV
		// webdav still needed if no disk is mounted
		start_webdav();
#else
		if(f_exists("/opt/etc/init.d/S50aicloud"))
			system("sh /opt/etc/init.d/S50aicloud scan");
#endif
#ifdef RTCONFIG_SAMBASRV
		if (nvram_get_int("smbd_master") || nvram_get_int("smbd_wins")) {
			start_samba();
		}
#endif
		return;
	}

	setup_passwd();
#ifdef RTCONFIG_SAMBASRV
	start_samba();
#endif

#if defined(RTCONFIG_TFTP_SERVER)
	start_tftpd();
#endif
#ifdef RTCONFIG_NFS
	start_nfsd();
#endif
if (nvram_match("asus_mfg", "0")) {
#ifdef RTCONFIG_FTP
	start_ftpd();
#endif
#ifdef RTCONFIG_MEDIA_SERVER
	start_dms();
	start_mt_daapd();
#endif
#ifdef RTCONFIG_WEBDAV
	//start_webdav();
#endif
}
#ifdef RTCONFIG_TIMEMACHINE
	start_timemachine();
#endif
	char *usbuipath=nvram_safe_get("usbUIpath");
	if(strlen(usbuipath) > 0){
	   if(d_exists(usbuipath) || f_exists(usbuipath)){
	      remove("/tmp/lighttpd/www/USB");
	      unlink("/tmp/lighttpd/www/USB");
	      symlink(usbuipath, "/tmp/lighttpd/www/USB");
	   }
	}
}

void stop_nas_services(int force)
{
	if(!force && getpid() != 1){
		notify_rc_after_wait("stop_nasapps");
		return;
	}

#ifdef RTCONFIG_MEDIA_SERVER
	force_stop_dms();
	stop_mt_daapd();
#endif
#ifdef RTCONFIG_FTP
	stop_ftpd();
#endif
#if defined(RTCONFIG_TFTP_SERVER)
	stop_tftpd();
#endif

#ifdef RTCONFIG_SAMBASRV
	stop_samba();
#endif
#ifdef RTCONFIG_NFS
	stop_nfsd();
#endif
#ifdef RTCONFIG_WEBDAV
	//stop_webdav();
#endif
#ifdef RTCONFIG_TIMEMACHINE
	stop_timemachine();
#endif
}

void restart_nas_services(int stop, int start)
{
	int fd = file_lock("usbnas");

	/* restart all NAS applications */
	if (stop)
		stop_nas_services(0);
	if (start)
		start_nas_services(0);
	file_unlock(fd);
}


void restart_sambaftp(int stop, int start)
{
	int fd = file_lock("sambaftp");

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	/* restart all NAS applications */
	if (stop) {
#ifdef RTCONFIG_SAMBASRV
		stop_samba();
#endif
#ifdef RTCONFIG_NFS
		stop_nfsd();
#endif
#ifdef RTCONFIG_FTP
		stop_ftpd();
#endif
#ifdef RTCONFIG_WEBDAV
		stop_webdav();
#else
		if(f_exists("/opt/etc/init.d/S50aicloud"))
			system("sh /opt/etc/init.d/S50aicloud scan");
#endif
	}

	if (start) {
#ifdef RTCONFIG_SAMBA_SRV
		setup_passwd();
		start_samba();
#endif
#ifdef RTCONFIG_NFS
		start_nfsd();
#endif
#ifdef RTCONFIG_FTP
		start_ftpd();
#endif
#ifdef RTCONFIG_WEBDAV
		start_webdav();
#else
		if(f_exists("/opt/etc/init.d/S50aicloud"))
			system("sh /opt/etc/init.d/S50aicloud scan");
#endif
	}
	file_unlock(fd);
}

#define USB_PORT_0	0x01
#define USB_PORT_1	0x02
#define USB_PORT_2	0x04

static void ejusb_usage(void)
{
	printf(	"Usage: ejusb [-1|1|2|1.*|2.*] [0|1*] [-u 0*|1]\n"
		"First parameter means disk_port.\n"
		"\t-1: All ports\n"
		"\t 1: disk port 1\n"
		"\t 2: disk port 2\n"
//		"\t 3: disk port 3\n"
		"Second parameter means whether ejusb restart NAS applications or not.\n"
		"Another parameters.\n"
		"\t-u: hold/unplug disk after unmount jobs done.\n"
		"\tDefault value is 1.\n");
}


/* Write 1 to /sys/bus/usb/devices/USB_NODE/remove to simulate unplug event.
 * @disk_port:	USB path, e.g., 2-1, 3-1.4.4
 * @return:
 * 	-1:	invalid parameter
 * 	-2:	not all partition of the disk are unmounted.
 * 	0:	success
 */
int remove_usb_disk(char *disk_port)
{
	int ret = 0, count = 1;
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char nv_name[32], path_name[128], node[32] = "";

	if (!disk_port || *disk_port == '\0')
		return -1;

	disk_list = read_disk_data();
	for (disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next) {
		if (strcmp(disk_info->port, disk_port))
			continue;

		sprintf(nv_name, "usb_path_%s", disk_info->device);
		strlcpy(node, nvram_safe_get(nv_name), sizeof(node));
		for (partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next) {
			count++;
			if (*node == '\0') {
				sprintf(nv_name, "usb_path_%s", partition_info->device);
				strlcpy(node, nvram_safe_get(nv_name), sizeof(node));
			}
			if (!partition_info->mount_point)
				continue;

			ret = -2;
			break;
		}

		if (ret)
			break;
		sprintf(nv_name, "ignore_nas_service_%s", disk_info->device);
		nvram_set_int(nv_name, count);
		if (is_m2ssd_port(node)) {
			snprintf(path_name, sizeof(path_name), "/sys/block/%s/device/delete", disk_info->device);
		} else {
			snprintf(path_name, sizeof(path_name), "/sys/bus/usb/devices/%s/remove", node);
		}
		f_write_string(path_name, "1", 0, 0);
	}
	free_disk_data(&disk_list);

	return ret;
}

/* @port_path:
 *     >=  0:	Remove all disk on specified USB root hub port.
 *     == -1:	Remove all disk on all USB root hub port.
 *     == X.Y:	Remove a disk on USB root hub port X and USB hub port Y.
 * @unplug:
 * 		If true, unplug USB disk after unmount jobs done.
 * @return:
 * 	 0:	success
 * 	-1:	invalid parameter
 * 	-2:	read disk data fail
 * 	-3:	device not found
 */
int __ejusb_main(const char *port_path, int unplug)
{
	int all_disk;
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char nvram_name[32], devpath[16], d_port[16], *d_dot;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		printf("Can't get any disk's information.\n");
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

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%s_removed", disk_info->port);
		nvram_set(nvram_name, "1");

		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			if (!partition_info->mount_point)
				continue;

			memset(devpath, 0, 16);
			sprintf(devpath, "/dev/%s", partition_info->device);
			umount_partition(devpath, 0, NULL, NULL, EFH_HP_REMOVE);
		}

		if (unplug)
			remove_usb_disk(disk_info->port);
	}
	free_disk_data(&disk_list);

	return 0;
}

int ejusb_main(int argc, char *argv[])
{
	int opt;
	int ports, restart_nasapps = 1, unplug = 0;
	char *ports_str;

	if (argc < 2) {
		ejusb_usage();
		return -1;
	}

	ports_str = argv[1];
	ports = atoi(argv[1]);
	if(ports != -1 && (ports < 1 || ports > 3)) {
		ejusb_usage();
		return -1;
	}
	if (argc == 3 && isdigit(*argv[2])) {
		restart_nasapps = atoi(argv[2]);
		argc -= 2;
		argv += 2;
	} else {
		argc--;
		argv++;
	}

	while ((opt = getopt(argc, argv, "u:")) != -1) {
		switch (opt) {
		case 'u':
			unplug = !!atoi(optarg);
			break;
		}
	}

	__ejusb_main(ports_str, unplug);

	if (restart_nasapps) {
		_dprintf("restart_nas_services(%d): test 7.\n", getpid());
		//restart_nas_services(1, 1);
		notify_rc_after_wait("restart_nasapps");
	} else {
		_dprintf("restart_nas_services(%d) is skipped: test 7.\n", getpid());
	}

	return 0;
}

#ifdef RTCONFIG_DISK_MONITOR
static int stop_diskscan()
{
	return nvram_get_int("diskmon_force_stop");
}

static unsigned int mount_counter = 0;

static int __inc_mount_counter(struct mntent *mnt, uint flags)
{
	mount_counter++;
	dbg("%s: mnt->mnt_dir [%s] mount_counter [%d]\n",
		__func__, mnt->mnt_dir, mount_counter);
	return 0;
}

/**
 * Format specified block device.
 * @port_path:	Path of USB port.
 * 		If it is -1, diskmon_usbport nvarm variable is used instead.
 * @return:
 * 	N/A
 *
 * Format steps:
 * 1. Unmount all partitions of the device and check.
 * 2. Clear partition table of the device via destroying first 1MB of the
 *    device.
 * 3. Let kernel forget old partitions of the device via re-read partition table,
 *    BLKRRPART ioctl, and triggers hotplug remove events.
 * 4. Make sure all partitions of the device gone via checking whether any
 *    partition of the device still exist in /proc/partitions.
 * 5. Format the device.
 * 6. Remount the device or original partitions of the device based whether
 *    it is formatted or not.
 */
static void start_diskformat(char *port_path)
{
	int fd, len, nr_format = 0;
	char **cmd = NULL;
	char write_file_name[32], line[128], usbport[16];
	char disk_system[32], disk_label[32], devpath[16], ofdev[16 + 3], *ptr_path;
	FILE *fp;
	time_t t1, t2;
	disk_info_t *disk_list, *disk_info, *di = NULL;
	partition_info_t *partition_info;

	if (!port_path)
		return;

	memset(write_file_name, 0, 32);
	nvram_set("disk_format_flag", "0"); //0: initial, 1: unmount, 2: format, 3: re-mount, 4: finish, -1: error

	strlcpy(disk_system, nvram_safe_get("diskformat_file_system"), sizeof(disk_system));
	strlcpy(disk_label, nvram_safe_get("diskformat_label"), sizeof(disk_label));
	strlcpy(usbport, nvram_safe_get("diskmon_usbport"), sizeof(usbport));

	if(atoi(port_path) == -1)
		ptr_path = usbport;
	else
		ptr_path = port_path;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		dbg("Can't get any disk's information.\n");
		return;
	}

	mount_counter = 0;
	for (disk_info = disk_list; !di && disk_info != NULL; disk_info = disk_info->next){
		if (strcmp(disk_info->port, ptr_path))
			continue;

		di = disk_info;
		stop_nas_services(1);
		for (partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			// umount partition and stop USB apps.
			dbg("disk_format: umount partition %s...\n", partition_info->device);
			diskmon_status(DISKMON_UMOUNT);
			nvram_set("disk_format_flag", "1");
			snprintf(devpath, sizeof(devpath), "/dev/%s", partition_info->device);
			umount_partition(devpath, 0, NULL, NULL, EFH_HP_REMOVE | EFH_USER);
			findmntents(devpath, 0, __inc_mount_counter, 0);
		}
	}

	if (!di) {
		dbg("disk_format: Can't find device via ptr_path [%s]\n", ptr_path);
		logmessage("disk_format", "Can't find device via ptr_path [%s]", ptr_path);
	} else if (mount_counter) {
		dbg("disk_format: Can't unmount %d partition(s) of device %s\n", di->device, mount_counter);
		logmessage("disk_format", "Can't unmount %d partition(s) of device %s", di->device, mount_counter);
	} else {
#if defined(RTCONFIG_TUXERA_NTFS) || defined(RTCONFIG_TUXERA_HFS)
		char *tfat_cmd[] = { "mkfatfs", "-l", disk_label, "-v", devpath, NULL };
		char *tntfs_cmd[] = {"mkntfs", "-F", "-L", disk_label, "-v", devpath, NULL };
		char *thfsplus_cmd[] = { "newfs_hfs", "-v", disk_label, devpath, NULL };
#endif
		// format partition.
		snprintf(devpath, sizeof(devpath), "/dev/%s", di->device);
		dbg("disk_format: prepare device %s ...\n", di->device);
		diskmon_status(DISKMON_FORMAT);
		nvram_set("disk_format_flag", "2");
		sprintf(write_file_name, "/tmp/disk_format/%s.log", di->device);

		//Check folder exist or not
		if (!check_if_dir_exist("/tmp/disk_format/"))
			mkdir("/tmp/disk_format/", 0755);
		if (check_if_file_exist(write_file_name))
			unlink(write_file_name);

		cmd = NULL;
#if defined(RTCONFIG_TUXERA_NTFS) || defined(RTCONFIG_TUXERA_HFS)
		if (!strcmp(disk_system, "tfat") && nvram_match("usb_fatfs_mod", "tuxera")) {
			cmd = tfat_cmd;
		}
		else if(!strcmp(disk_system, "tntfs") && nvram_match("usb_ntfs_mod", "tuxera")) {
			cmd = tntfs_cmd;
		}
		else if(!strcmp(disk_system, "thfsplus") && nvram_match("usb_hfs_mod", "tuxera")) {
			cmd = thfsplus_cmd;
		}
		else
#endif
		{
			dbg("disk_format: Can't format [%s] as filesystem [%s] w/o tool.\n", devpath, disk_system);
			logmessage("disk_format", "Can't format %s as filesystem %s w/o tool.", devpath, disk_system);
		}

		if (cmd != NULL) {
			nr_format++;
			snprintf(ofdev, sizeof(ofdev), "of=%s", devpath);
			eval("dd", "if=/dev/zero", ofdev, "bs=1048576", "count=1");
			sync();

			/* Let kernel forgot old partition(s) of the device and make sure it happen. Max 10 seconds.*/
			if ((fd = open(devpath, O_RDONLY)) >= 0) {
				if (!ioctl(fd, BLKRRPART, NULL)) {
					dbg("disk_format: Re-read partition table successful.\n");
				} else {
					dbg("disk_format: Re-read partition table failed. errno %d (%s)\n", errno, strerror(errno));
					logmessage("disk_format", "Re-read partition table failed. errno %d (%s)\n", errno, strerror(errno));
				}
			} else {
				dbg("disk_format: Can't open device %s. errno %d (%s)\n", devpath, errno, strerror(errno));
			}
			if (fd >= 0)
				close(fd);

			/* Wait until all partition of the device gone. */
			t1 = t2 = uptime();
			len = strlen(di->device);
			fp = fopen("/proc/partitions", "r");
			while (fp && ((t2 - t1) <= 10)) {
				int part_cnt = 0;
				char dev[64];

				dbg("disk_format: check partitions of %s\n", di->device);
				fseek(fp, SEEK_SET, 0);
				while (fgets(line, sizeof(line), fp)) {
					if (sscanf(line, " %*d %*d %*d %[^\n ]", dev) != 1)
						continue;
					if (strncmp(di->device, dev, len))
						continue;
					if (strlen(dev) <= len || !isdigit(*(dev + len)))
						continue;

					part_cnt++;
				}

				if (!part_cnt)
					break;

				dbg("%d partitions of %s still exist.\n", part_cnt, di->device);
				sleep(1);
				t2 = uptime();
			}
			if (!fp)
				dbg("disk_format: Open /proc/partitions for %s failed. errno %d (%s)\n", di->device, errno, strerror(errno));
			else if (t2 - t1 > 10)
				dbg("disk_format: One or more partitions of %s can't be removed.\n", di->device);
			if (fp != NULL)
				fclose(fp);

			dbg("disk_format: Format %s ...\n", devpath);
			logmessage("disk_format", "Format %s ...", devpath);
			_eval(cmd, write_file_name, 0, NULL);
		}
	}

	if (nr_format) {
		// re-mount formatted device.
		dbg("disk_format: re-mount device %s...\n", di->device);
		diskmon_status(DISKMON_REMOUNT);
		nvram_set("disk_format_flag", "3");
		mount_partition(devpath, -3, NULL, di->device, EFH_HP_ADD);
	} else {
		// re-mount original partition.
		for (partition_info = di->partitions; partition_info != NULL; partition_info = partition_info->next){
			dbg("disk_format: re-mount original partition %s...\n", partition_info->device);
			diskmon_status(DISKMON_REMOUNT);
			nvram_set("disk_format_flag", "3");
			mount_partition(devpath, -3, NULL, partition_info->device, EFH_HP_ADD);
		}
	}

	start_nas_services(1);

	free_disk_data(&disk_list);

	// finish & restart USB apps.
	dbg("disk_format: done.\n");
	diskmon_status(DISKMON_FINISH);
	nvram_set("disk_format_flag", "4");
	return;
}

// -1: manully scan by diskmon_usbport, 1: scan the USB port 1,  2: scan the USB port 2.
static void start_diskscan(char *port_path)
{
	disk_info_t *disk_list, *disk_info;
	partition_info_t *partition_info;
	char policy[16], monpart[128], usbport[16], devpath[16], *ptr_path;

	if (!port_path || stop_diskscan())
		return;

	snprintf(policy, sizeof(policy), "%s", nvram_safe_get("diskmon_policy"));
	snprintf(monpart, sizeof(monpart), "%s", nvram_safe_get("diskmon_part"));
	snprintf(usbport, sizeof(usbport), "%s", nvram_safe_get("diskmon_usbport"));

	if(atoi(port_path) == -1)
		ptr_path = usbport;
	else
		ptr_path = port_path;

	disk_list = read_disk_data();
	if(disk_list == NULL){
		cprintf("Can't get any disk's information.\n");
		return;
	}

	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next){
		/* If hub port number is not specified in port_path,
		 * don't compare it with hub port number in disk_info->port.
		 */
		if (!strcmp(policy, "disk") && strcmp(disk_info->port, ptr_path))
			continue;

		for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
			if(partition_info->mount_point == NULL){
				cprintf("Skip to scan %s: It can't be mounted.\n");
				continue;
			}

			if(!strcmp(policy, "part") && strcmp(monpart, partition_info->device))
				continue;

			// there's some problem with fsck.ext4.
			if(!strcmp(partition_info->file_system, "ext4"))
				continue;

			if(stop_diskscan())
				goto stop_scan;

			// umount partition and stop USB apps.
			cprintf("disk_monitor: umount partition %s...\n", partition_info->device);
			diskmon_status(DISKMON_UMOUNT);
			sprintf(devpath, "/dev/%s", partition_info->device);
			umount_partition(devpath, 0, NULL, NULL, EFH_HP_REMOVE);

			if(stop_diskscan())
				goto stop_scan;

			// scan partition.
			eval("mount"); /* what for ??? */
			cprintf("disk_monitor: scan partition %s...\n", partition_info->device);
			diskmon_status(DISKMON_SCAN);
			eval("/usr/sbin/app_fsck.sh", partition_info->file_system, devpath);

			if(stop_diskscan())
				goto stop_scan;

			// re-mount partition.
			cprintf("disk_monitor: re-mount partition %s...\n", partition_info->device);
			diskmon_status(DISKMON_REMOUNT);
			mount_partition(devpath, -3, NULL, partition_info->device, EFH_HP_ADD);

			start_nas_services(1);
		}
	}

	free_disk_data(&disk_list);
	// finish & restart USB apps.
	cprintf("disk_monitor: done.\n");
	diskmon_status(DISKMON_FINISH);
	return;

stop_scan:
	free_disk_data(&disk_list);
	diskmon_status(DISKMON_FORCE_STOP);
}

#define NO_SIG -1

static int diskmon_signal = NO_SIG;

static void diskmon_sighandler(int sig)
{
	switch(sig) {
		case SIGTERM:
			cprintf("disk_monitor: Finish!\n");
			logmessage("disk_monitor", "Finish");
			unlink("/var/run/disk_monitor.pid");
			diskmon_signal = sig;
			exit(0);
		case SIGUSR1:
			logmessage("disk_monitor", "Format manually...");
			cprintf("disk_monitor: Format manually...\n");
			diskmon_status(DISKMON_START);
			start_diskformat("-1");
			sleep(1);
			diskmon_status(DISKMON_IDLE);
			diskmon_signal = sig;
			break;
		case SIGUSR2:
			logmessage("disk_monitor", "Scan manually...");
			cprintf("disk_monitor: Scan manually...\n");
			diskmon_status(DISKMON_START);
			start_diskscan("-1");
			sleep(10);
			diskmon_status(DISKMON_IDLE);
			diskmon_signal = sig;
			break;
		case SIGALRM:
			//logmessage("disk_monitor", "Got SIGALRM...");
			cprintf("disk_monitor: Got SIGALRM...\n");
			diskmon_signal = sig;
			break;
	}
}

void start_diskmon(void)
{
	char *diskmon_argv[] = { "disk_monitor", NULL };
	pid_t pid;

	if (getpid() != 1) {
		notify_rc("start_diskmon");
		return;
	}

#ifdef RTAC68U
	if (!hw_usb_cap())
		return;
#endif

	_eval(diskmon_argv, NULL, 0, &pid);
}

void stop_diskmon(void)
{
	if (getpid() != 1) {
		notify_rc("stop_diskmon");
		return;
	}

	killall_tk("disk_monitor");
}

int first_alarm = 1;

int diskmon_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t mask;
	int diskmon_freq = DISKMON_FREQ_DISABLE;
	time_t now;
	struct tm local;
	char *nv, *nvp;
	char *set_day, *set_week, *set_hour;
#if defined(RTCONFIG_M2_SSD)
	int val_day[3] = {0, }, val_hour[3] = {0, }, wait_second[3] = {0, };
#else
	int val_day[2] = {0, 0}, val_hour[2] = {0, 0};
	int wait_second[2] = {0, 0};
#endif
	int diskmon_alarm_sec = 0, wait_hour = 0;
	char port_path[16];
	int port_num;
	char word[PATH_MAX], *next;
	char nvram_name[32];

	fp = fopen("/var/run/disk_monitor.pid", "w");
	if(fp != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	cprintf("disk_monitor: starting...\n");
	diskmon_status(DISKMON_IDLE);

	nvram_set_int("diskmon_force_stop", 0);

	signal(SIGTERM, diskmon_sighandler);
	signal(SIGUSR1, diskmon_sighandler);
	signal(SIGUSR2, diskmon_sighandler);
	signal(SIGALRM, diskmon_sighandler);

	sigfillset(&mask);
	sigdelset(&mask, SIGTERM);
	sigdelset(&mask, SIGUSR1);
	sigdelset(&mask, SIGUSR2);
	sigdelset(&mask, SIGALRM);

	port_num = 0;

	foreach(word, nvram_safe_get("ehci_ports"), next){
		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_diskmon_freq", (port_num+1));

		diskmon_freq = nvram_get_int(nvram_name);
		if(diskmon_freq == DISKMON_FREQ_DISABLE){
			++port_num;
			continue;
		}

		memset(nvram_name, 0, 32);
		sprintf(nvram_name, "usb_path%d_diskmon_freq_time", (port_num+1));
		nv = nvp = strdup(nvram_safe_get(nvram_name));
		if(!nv || strlen(nv) <= 0){
			cprintf("disk_monitor: Without setting the running time at the port %d!\n", (port_num+1));
			++port_num;
			continue;
		}

		if((vstrsep(nvp, ">", &set_day, &set_week, &set_hour) != 3)){
			cprintf("disk_monitor: Without the correct running time at the port %d!\n", (port_num+1));
			++port_num;
			continue;
		}

		val_hour[port_num] = atoi(set_hour);
		if(diskmon_freq == DISKMON_FREQ_MONTH)
			val_day[port_num] = atoi(set_day);
		else if(diskmon_freq == DISKMON_FREQ_WEEK)
			val_day[port_num] = atoi(set_week);
		else if(diskmon_freq == DISKMON_FREQ_DAY)
			val_day[port_num] = -1;
cprintf("disk_monitor: Port %d: val_day=%d, val_hour=%d.\n", port_num, val_day[port_num], val_hour[port_num]);

		++port_num;
	}

	while(1){
		time(&now);
		localtime_r(&now, &local);
cprintf("disk_monitor: day=%d, week=%d, time=%d:%d.\n", local.tm_mday, local.tm_wday, local.tm_hour, local.tm_min);

		if(diskmon_signal == SIGUSR2){
cprintf("disk_monitor: wait more %d seconds and avoid to scan too often.\n", DISKMON_SAFE_RANGE*60);
			diskmon_alarm_sec = DISKMON_SAFE_RANGE*60;
		}
		else if(first_alarm || diskmon_signal == SIGALRM){
cprintf("disk_monitor: decide if scan the target...\n");
			diskmon_alarm_sec = 0;
			port_num = 0;
			foreach(word, nvram_safe_get("ehci_ports"), next){
				if(local.tm_min <= DISKMON_SAFE_RANGE){
					if(val_hour[port_num] == local.tm_hour){
						if((diskmon_freq == DISKMON_FREQ_MONTH && val_day[port_num] == local.tm_mday)
								|| (diskmon_freq == DISKMON_FREQ_WEEK && val_day[port_num] == local.tm_wday)
								|| (diskmon_freq == DISKMON_FREQ_DAY)){
cprintf("disk_monitor: Running...\n");
							// Running!!
							diskmon_status(DISKMON_START);
							sprintf(port_path, "%d", port_num + 1);
							start_diskscan(port_path);
							sleep(10);
							diskmon_status(DISKMON_IDLE);
						}

						wait_hour = DISKMON_DAY_HOUR;
					}
					else if(val_hour[port_num] > local.tm_hour)
						wait_hour = val_hour[port_num]-local.tm_hour;
					else // val_hour < local.tm_hour
						wait_hour = 23-local.tm_hour+val_hour[port_num];

					wait_second[port_num] = wait_hour*DISKMON_HOUR_SEC;
				}
				else
					wait_second[port_num] = (60-local.tm_min)*60;
cprintf("disk_monitor: %d: wait_second=%d...\n", port_num, wait_second[port_num]);

				if(diskmon_alarm_sec == 0 || diskmon_alarm_sec > wait_second[port_num])
					diskmon_alarm_sec = wait_second[port_num];

				++port_num;
			}
		}

		if(first_alarm || diskmon_signal == SIGUSR2 || diskmon_signal == SIGALRM){
			if(first_alarm)
				first_alarm = 0;

cprintf("disk_monitor: wait_second=%d...\n", diskmon_alarm_sec);
			alarm(diskmon_alarm_sec);
		}

cprintf("disk_monitor: Pause...\n\n");
		diskmon_signal = NO_SIG;
		sigsuspend(&mask);
	}

	unlink("/var/run/disk_monitor.pid");

	return 0;
}

void record_pool_error(const char *device, const char *flag){
	char word[PATH_MAX], *next;
	char tmp[100], prefix[] = "usb_pathXXXXXXXXXX_";
	char pool_act[16];
	int port, len;
	int orig_val;

	port = 1;
	foreach(word, nvram_safe_get("ehci_ports"), next){
		snprintf(prefix, sizeof(prefix), "usb_path%d_", port);

		snprintf(pool_act, sizeof(pool_act), "%s", nvram_safe_get(strcat_r(prefix, "act", tmp)));
		len = strlen(pool_act);
		if(len > 0 && !strncmp(device, pool_act, len)){
			orig_val = strtol(nvram_safe_get(strcat_r(prefix, "pool_error", tmp)), NULL, 0);
			if(orig_val == 0)
				nvram_set(tmp, flag);

			break;
		}

		++port;
	}
}

void remove_scsi_device(int host, int channel, int id, int lun){
	char buf[128];

	if(nvram_match("diskremove_bad_device", "0")){
_dprintf("diskremove: don't remove the bad device: %d:%d:%d:%d.\n", host, channel, id, lun);
		return;
	}

	memset(buf, 0, 128);
	sprintf(buf, "echo \"scsi remove-single-device %d %d %d %d\" > /proc/scsi/scsi", host, channel, id, lun);

_dprintf("diskremove: removing the device: %d:%d:%d:%d.\n", host, channel, id, lun);
	system(buf);

	sleep(1);
}

void remove_pool_error(const char *device, const char *flag){
	char word[PATH_MAX], *next;
	char tmp[100], prefix[] = "usb_pathXXXXXXXXXX_";
	char pool_act[16];
	int port, len;
	int host, channel, id, lun;
_dprintf("diskremove: device=%s, flag=%s.\n", device, flag);

	if(flag == NULL || !strcmp(flag, "0"))
		return;

	port = 1;
	foreach(word, nvram_safe_get("ehci_ports"), next){
		snprintf(prefix, sizeof(prefix), "usb_path%d_", port);

		snprintf(pool_act, sizeof(pool_act), "%s", nvram_safe_get(strcat_r(prefix, "act", tmp)));
_dprintf("diskremove: pool_act=%s.\n", pool_act);
		len = strlen(pool_act);
		if(len > 0 && !strncmp(device, pool_act, len)){
			host = channel = id = lun = -2;
			if(find_disk_host_info(pool_act, &host, &channel, &id, &lun) == -1){
				_dprintf("diskremove: Didn't get the correct info of the device.\n");
				return;
			}

			if(strcmp(flag, ERR_DISK_FS_RDONLY)){
_dprintf("diskremove: host=%d, channel=%d, id=%d, lun=%d.\n", host, channel, id, lun);
				remove_scsi_device(host, channel, id, lun);
			}
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
			else{
_dprintf("diskremove: stop_app.\n");
				stop_app();
			}
#endif

			break;
		}

		++port;
	}
}

int diskremove_main(int argc, char *argv[]){
	char *subsystem = getenv("SUBSYSTEM");
	char *device = getenv("DEVICE");
	char *flag = getenv("FLAG");
	int host, channel, id, lun;

	if(!subsystem || strlen(subsystem) <= 0
			|| !device || strlen(device) <= 0
			|| !flag || strlen(flag) <= 0)
		return -1;
_dprintf("diskremove: SUBSYSTEM=%s, DEVICE=%s, FLAG=%s.\n", subsystem, device, flag);

	return 0;

	record_pool_error(device, flag);

	if(!strcmp(subsystem, "filesystem")){
		remove_pool_error(device, flag);
	}
	else if(!strcmp(subsystem, "scsi")){
		host = channel = id = lun = -2;
		if(find_str_host_info(device, &host, &channel, &id, &lun) == -1){
_dprintf("diskremove: Didn't get the correct info of the SCSI device.\n");
			return -1;
		}

		if(!strcmp(flag, ERR_DISK_SCSI_KILL)){
_dprintf("diskremove: host=%d, channel=%d, id=%d, lun=%d.\n", host, channel, id, lun);
			remove_scsi_device(host, channel, id, lun);
		}
	}

	return 0;
}
#else
int diskremove_main(int argc, char *argv[]){
	return 0;
}
#endif

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
int start_app(void)
{
	char cmd[PATH_MAX];
	char apps_dev[16];
	char apps_mounted_path[PATH_MAX];

	snprintf(apps_dev, sizeof(apps_dev), "%s", nvram_safe_get("apps_dev"));
	snprintf(apps_mounted_path, sizeof(apps_mounted_path), "%s", nvram_safe_get("apps_mounted_path"));

	if(strlen(apps_dev) <= 0 || strlen(apps_mounted_path) <= 0)
		return -1;

	memset(cmd, 0, PATH_MAX);
	sprintf(cmd, "%s/.asusrouter %s %s", nvram_safe_get("apps_local_space"), apps_dev, apps_mounted_path);
	system(cmd);

	return 0;
}

int stop_app(void)
{
	if(strlen(nvram_safe_get("apps_dev")) <= 0 || strlen(nvram_safe_get("apps_mounted_path")) <= 0)
		return -1;

	system("/usr/sbin/app_stop.sh");
	sync();

	return 0;
}

void usb_notify(){
	char target_dir[128], target[128], buf[16];
	DIR *dp;
	struct dirent *entry;

	memset(target_dir, 0, 128);
	sprintf(target_dir, "%s/%s", NOTIFY_DIR, NOTIFY_TYPE_USB);
	if(!check_if_dir_exist(target_dir))
		return;

	if(!(dp = opendir(target_dir)))
		return;

	while((entry = readdir(dp)) != NULL){
		if(entry->d_name[0] == '.')
			continue;

		memset(target, 0, 128);
		sprintf(target, "%s/%s", target_dir, entry->d_name);

		if(!pids(entry->d_name)){
			unlink(target);
			continue;
		}

		f_read_string(target, buf, 16);

		killall(entry->d_name, atoi(buf));
	}
	closedir(dp);
}
#endif
#endif // RTCONFIG_USB

//#ifdef RTCONFIG_WEBDAV
#define DEFAULT_WEBDAVPROXY_RIGHT 0

int find_webdav_right(char *account)
{
	char *nv, *nvp, *b;
	char *acc, *right;
	int ret;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));
	ret = DEFAULT_WEBDAVPROXY_RIGHT;

	if(nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)==0) {
				ret = atoi(right);
				break;
			}
		}
		free(nv);
	}

	return ret;
}

void webdav_account_default(void)
{
	char *nv, *nvp, *b;
	char *accname, *accpasswd;
	int right;
	char new[256];
	int i;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	strcpy(new, "");

	if(nv) {
		i=0;
		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &accname, &accpasswd) != 2)) continue;

			right = find_webdav_right(accname);

			if(i==0) sprintf(new, "%s>%d", accname, right);
			else sprintf(new, "%s<%s>%d", new, accname, right);
			i++;
		}
		free(nv);
		nvram_set("acc_webdavproxy", new);
	}
}
//#endif


#ifdef RTCONFIG_NFS
void start_nfsd(void)
{
	struct stat	st_buf;
	FILE 		*fp;
        char *nv, *nvp, *b, *c;
	char *dir, *access, *options;

	if (nvram_match("nfsd_enable", "0")) return;

#ifdef HND_ROUTER
	modprobe("exportfs");
	modprobe("sunrpc");
	modprobe("grace");
	modprobe("lockd");
	modprobe("nfsd");
#endif

	/* create directories/files */
	mkdir_if_none("/var/lib");
	mkdir_if_none("/var/lib/nfs");
#ifdef LINUX26
	mkdir_if_none("/var/lib/nfs/v4recovery");
	mount("nfsd", "/proc/fs/nfsd", "nfsd", MS_MGC_VAL, NULL);
#endif
	unlink("/var/lib/nfs/etab");
	unlink("/var/lib/nfs/xtab");
	unlink("/var/lib/nfs/rmtab");
	close(creat("/var/lib/nfs/etab", 0644));
	close(creat("/var/lib/nfs/xtab", 0644));
	close(creat("/var/lib/nfs/rmtab", 0644));

	/* (re-)create /etc/exports */
	if (stat(NFS_EXPORT, &st_buf) == 0)	{
		unlink(NFS_EXPORT);
	}

	if ((fp = fopen(NFS_EXPORT, "w")) == NULL) {
		perror(NFS_EXPORT);
		return;
	}

	nv = nvp = strdup(nvram_safe_get("nfsd_exportlist"));
	if (nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if ((vstrsep(b, ">", &dir, &access, &options) != 3))
				continue;

			fputs(dir, fp);

			while ((c = strsep(&access, " ")) != NULL) {
				fprintf(fp, " %s(no_root_squash%s%s)", c, ((strlen(options) > 0) ? "," : ""), options);
			}
			fputs("\n", fp);
		}
		free(nv);
	}

	append_custom_config("exports", fp);
	fclose(fp);
	run_postconf("exports", NFS_EXPORT);
	if (!pids("portmap"))
		eval("/usr/sbin/portmap");
	eval("/usr/sbin/statd");

	if (nvram_match("nfsd_enable_v2", "1")) {
		eval("/usr/sbin/nfsd", "-V 2");
		eval("/usr/sbin/mountd", "-V 2");
	} else {
		eval("/usr/sbin/nfsd", "-N 2");
		eval("/usr/sbin/mountd", "-N 2");
	}

	sleep(1);
	eval("/usr/sbin/exportfs", "-a");

	return;
}

void restart_nfsd(void)
{
	//eval("/usr/sbin/exportfs", "-au");
	//eval("/usr/sbin/exportfs", "-a");
	eval("/usr/sbin/exportfs", "-r");

	return;
}

void stop_nfsd(void)
{
	eval("/usr/sbin/exportfs", "-au");
	killall_tk("mountd");
	killall("nfsd", SIGKILL);
	killall_tk("statd");
//	killall_tk("portmap");

#ifdef LINUX26
	umount("/proc/fs/nfsd");
#endif

#ifdef HND_ROUTER
	modprobe_r("nfsd");
	modprobe_r("lockd");
	modprobe_r("grace");
	modprobe_r("sunrpc");
	modprobe_r("exportfs");
#endif

	return;
}

#endif


void start_wsdd()
{
	unsigned char ea[ETHER_ADDR_LEN];
	char serial[18];
	pid_t pid;
	char bootparms[64];
	char *wsdd_argv[] = { "/usr/sbin/wsdd2",
				"-d",
				"-w",
				"-i",
				nvram_safe_get("lan_ifname"),
				"-b",
				NULL,	// boot parameters
				NULL };
	stop_wsdd();

	if (!ether_atoe(get_lan_hwaddr(), ea))
		f_read("/dev/urandom", ea, sizeof(ea));

	snprintf(serial, sizeof(serial), "%02x%02x%02x%02x%02x%02x",
		ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);

	snprintf(bootparms, sizeof(bootparms), "sku:%s,serial:%s", get_productid(), serial);
	wsdd_argv[6] = bootparms;

#if 0
	if(!f_exists("/etc/machine-id"))
		system("echo $(nvram get lan_hwaddr) | md5sum | cut -b -32 > /etc/machine-id");
#endif

	_eval(wsdd_argv, NULL, 0, &pid);
}

void stop_wsdd() {
	if (pids("wsdd2"))
		killall_tk("wsdd2");
}

