include ../common.mak

CFLAGS += -Os -Wall $(EXTRACFLAGS)
CFLAGS += $(PRIVATE_EXTRACFLAGS)
CFLAGS += -I$(SRCBASE) -I.
ifeq ($(RTCONFIG_BCM7),y)
CFLAGS += -I$(SRCBASE)/../../dhd/src/wl/exe/
endif
ifeq ($(RTCONFIG_BCM_7114),y)
CFLAGS += -I$(SRCBASE)/../dhd/src/include
CFLAGS += -I$(SRCBASE)/../dhd/src/shared/bcmwifi/include
endif
ifeq ($(RTCONFIG_BCM9),y)
CFLAGS += -I$(SRCBASE)/../sys/src/include/
endif
ifeq ($(HND_ROUTER),y)
CFLAGS += -I$(SRCBASE)/../../dhd/src/include/
CFLAGS += -I$(SRCBASE)/../../dhd/src/shared/bcmwifi/include/
CFLAGS += -I$(SRCBASE)/../../dhd/components/shared/devctrl_if/
endif
CFLAGS += -I$(SRCBASE)/include -I$(TOP)/shared -I$(TOP)/zlib -I$(TOP)/networkmap
ifneq ($(or $(RTCONFIG_BCM7),$(RTCONFIG_BCM_7114),$(RTCONFIG_BCM9),$(HND_ROUTER)),y)
ifeq ($(RTCONFIG_BCMWL6),y)
CFLAGS += -I$(SRCBASE)/wl/exe/
endif
endif
CFLAGS += -I$(TOP)/push_log
CFLAGS += -DASUS_DDNS -DTRANSLATE_ON_FLY -DFLASH_EMULATOR -DLinux -DWSC 

ifeq ($(RTCONFIG_NETOOL),y)
CFLAGS += -I$(TOP)/rc
endif

ifeq ($(RTCONFIG_VPN_FUSION),y)
CFLAGS += -I$(TOP)/rc
endif

ifeq ($(RTCONFIG_BCM_7114),y)
CFLAGS += -I$(SRCBASE)/../dhd/components/shared/
endif

ifeq ($(RTCONFIG_RALINK),y)
CFLAGS += -DRalink_WPS
CFLAGS += -I$(TOP)/shared/sysdeps/ralink -I$(TOP)/wireless_tools
endif

ifeq ($(RTCONFIG_QCA),y)
CFLAGS += -DQCA_WPS
CFLAGS += -I$(TOP)/shared/sysdeps/qca -I$(TOP)/wireless_tools
endif

ifeq ($(CONFIG_REALTEK),y)
CFLAGS += -I$(TOP)/shared/sysdeps/realtek
endif

ifeq ($(RTCONFIG_ALPINE),y)
CFLAGS += -DALPINE_WPS
CFLAGS += -I$(TOP)/httpd/sysdeps -I$(TOP)/shared/sysdeps/alpine -I$(TOP)/wireless_tools
endif
ifeq ($(RTCONFIG_LANTIQ),y)
CFLAGS += -DLANTIQ_WPS
CFLAGS += -I$(TOP)/shared/sysdeps/lantiq -I$(TOP)/wireless_tools
endif

include $(SRCBASE)/.config

ifeq ($(RTN11P)$(RTN300),y)
CFLAGS += -DBAND_2G_ONLY
endif

ifeq ($(RTN14U),y)
CFLAGS += -DBAND_2G_ONLY
endif

ifeq ($(RTN14U)$(RTAC52U)$(RTAC51U)$(RTN11P)$(RTN300)$(RTN54U)$(RTAC1200HP)$(RTN56UB1)$(RTN56UB2)$(RTAC54U)$(RTAC1200)$(RTAC1200GA1)$(RTAC1200GU),y)
CFLAGS += -DRA_ESW
endif

ifeq ($(RTCONFIG_DNSMASQ),y)
CFLAGS += -DDNSMASQ
endif

ifeq ($(RTCONFIG_BWDPI),y)
CFLAGS += -I$(TOP)/sqlite
CFLAGS += -I$(TOP)/bwdpi_source/src/tdts_udb_sample
CFLAGS += -I$(TOP)/bwdpi_source/src/tdts_udb_sample/linux
CFLAGS += -I$(TOP)/bwdpi_source/include
CFLAGS += -I$(TOP)/bwdpi_source/asus_include
endif

ifeq ($(RTCONFIG_TRAFFIC_LIMITER),y)
CFLAGS += -I$(TOP)/traffic_limiter
CFLAGS += -I$(TOP)/sqlite
endif

ifeq ($(RTCONFIG_SW_HW_AUTH), y)
CFLAGS += -I$(TOP)/sw-hw-auth
endif

ifeq ($(or $(RTAC68A),$(RT4GAC68U)),y)
CFLAGS += -DRTAC68U
endif

OBJS = $(if $(wildcard httpd.c), httpd.o, prebuild/httpd.o)
OBJS += cgi.o ej.o 
OBJS += web.o common.o nvram_f.o 
OBJS += aspbw.o initial_web_hook.o
OBJS += apps.o
OBJS += sysinfo.o data_arrays.o
OBJS += unlock.o

ifeq ($(CONFIG_REALTEK),y)
OBJS += web-realtek.o
else ifeq ($(CONFIG_RALINK),y)
OBJS += web-ralink.o
else ifeq ($(CONFIG_QCA),y)
OBJS += web-qca.o
else ifeq ($(CONFIG_ALPINE),y)
OBJS += web-alpine.o
else ifeq ($(CONFIG_LANTIQ),y)
OBJS += $(if $(wildcard sysdeps/web-lantiq.c),web-lantiq.o,prebuild/web-lantiq.o)
else
OBJS += web-broadcom.o
ifeq ($(RTCONFIG_QTN),y)
OBJS += find_host_addr.o
endif
endif

ifeq ($(RTCONFIG_QSR10G),y)
OBJS += web-qsr10g.o
endif

ifeq ($(RTCONFIG_DSL),y)
OBJS += web-dsl.o
ifeq ($(RTCONFIG_RALINK),y)
OBJS += web-dsl-upg.o
endif
endif

ifeq ($(RTCONFIG_GEOIP),y)
OBJS += geoiplookup.o
endif

ifeq ($(and $(HND_ROUTER),$(RTCONFIG_VISUALIZATION)),y)
OBJS += vis_gui.o
endif

ifeq ($(RTCONFIG_HTTPS),y)
OBJS += pwenc.o
endif

OBJS += $(if $(wildcard web_hook.c), web_hook.o, prebuild/web_hook.o)

LIBS = -L$(TOP)/nvram${BCMEX}$(EX7) -lnvram -L$(TOP)/shared -lshared
ifeq ($(HND_ROUTER),y)
LIBS += -L$(TOP)/wlcsm -lwlcsm
endif

# for ceil()
LIBS += -lm

ifeq ($(RTCONFIG_PUSH_EMAIL),y)
ifeq ($(RTCONFIG_OPENSSL11),y)
CFLAGS += -I$(TOP)/openssl-1.1/include
LIBS += -L$(TOP)/openssl-1.1 -lssl -lcrypto -ldl
else
CFLAGS += -I$(TOP)/openssl/include
LIBS += -L$(TOP)/openssl -lssl -lcrypto -ldl
endif
endif

ifeq ($(RTCONFIG_BCMARM),y)
CFLAGS += -I$(SRCBASE)/shared/bcmwifi/include -DTYPEDEF_FLOAT_T
CFLAGS += -I$(SRCBASE)/common/include
LIBS += -lgcc_s
ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client -I$(TOP)/shared/sysdeps/qtn
LIBS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif
endif

ifeq ($(RTCONFIG_AMAS),y)
LDFLAGS += -L$(TOP)/json-c/.libs -ljson-c
endif

ifeq ($(RTCONFIG_USB),y)
CFLAGS += -I$(TOP)/libdisk	
LIBS += -L$(TOP)/libdisk -ldisk
ifeq ($(RTCONFIG_USB_SMS_MODEM),y)
CFLAGS += -I$(TOP)/smspdu
LIBS += -L$(TOP)/smspdu -lsmspdu
endif
endif

ifeq ($(RTCONFIG_HTTPS),y)
CFLAGS += -I$(TOP)/mssl
LIBS += -L$(TOP)/mssl -lmssl
ifeq ($(RTCONFIG_OPENSSL11),y)
CFLAGS += -I$(TOP)/openssl-1.1/include
LIBS += -L$(TOP)/openssl-1.1 -lssl -lcrypto -ldl
else
CFLAGS += -I$(TOP)/openssl/include
LIBS += -L$(TOP)/openssl -lssl -lcrypto -ldl
endif
endif

ifeq ($(RTCONFIG_OPENVPN),y)
OBJS += openvpn_options.o
CFLAGS += -I$(TOP)/libvpn
LIBS += -L$(TOP)/libvpn -lvpn
ifeq ($(RTCONFIG_OPENSSL11),y)
LIBS += -L$(TOP)/openssl-1.1 -lssl -lcrypto -ldl
else
LIBS += -L$(TOP)/openssl -lssl -lcrypto -ldl
endif
endif

CFLAGS += -I$(TOP)/json-c
LIBS += -L$(TOP)/json-c/.libs -ljson-c

ifeq ($(RTCONFIG_BWDPI),y)
LIBS += -L$(TOP)/bwdpi_source/asus -lbwdpi
LIBS += -L$(TOP)/bwdpi_source/asus_sql -lbwdpi_sql
LIBS += -L$(TOP)/sqlite/.libs -lsqlite3
LIBS += -lpthread
endif

ifeq ($(RTCONFIG_TRAFFIC_LIMITER),y)
LIBS += -L$(TOP)/traffic_limiter -ltraffic_limiter
LIBS += -L$(TOP)/sqlite/.libs -lsqlite3
endif

ifeq ($(RTCONFIG_GEOIP),y)
CFLAGS += -I$(TOP)/GeoIP-1.6.2/libGeoIP
LIBS += -L$(TOP)/GeoIP-1.6.2/libGeoIP/.libs -lGeoIP
endif

ifeq ($(RTCONFIG_EXT_RTL8365MB),y)
CFLAGS += -I$(LINUXDIR)/drivers/char/rtl8365mb
endif

ifeq ($(RTCONFIG_QCA),y)
LIBS += -lpthread -ldl
endif

export CFLAGS
ifeq ($(RTCONFIG_QTN),y)
vpath %.c sysdeps $(TOP)/libqcsapi_client/qcsapi_rpc_common/client $(TOP)/shared/sysdeps/qtn
else
vpath %.c sysdeps
endif

ifeq ($(RTCONFIG_NOTIFICATION_CENTER),y)
CFLAGS  += -I$(TOP)/sqlite
CFLAGS  += -I$(TOP)/nt_center/lib
CFLAGS  += -DASUSWRT_SDK
LIBS += -L$(TOP)/nt_center/lib/ -lnt
LIBS += -L$(TOP)/sqlite/.libs -lsqlite3
LIBS += -lpthread #Depend on sqlite
endif

ifeq ($(RTCONFIG_AIHOME_TUNNEL),y)
CFLAGS  += -I$(TOP)/aaews
endif

ifeq ($(RTCONFIG_LETSENCRYPT),y)
CFLAGS  += -I$(TOP)/libletsencrypt
ifneq ($(RTCONFIG_OPENSSL11),y) # Kludge, library still uses 1.0.2
LIBS += -L$(TOP)/libletsencrypt -lletsencrypt
endif
endif

ifeq ($(RTCONFIG_PERMISSION_MANAGEMENT),y)
CFLAGS  += -I$(TOP)/sqlCipher
CFLAGS  += -I$(TOP)/PMS_DBapis
LIBS += -L$(TOP)/PMS_DBapis -lpms_sql
LIBS += -L $(TOP)/sqlCipher/.libs -lsqlcipher
endif

ifeq ($(RTCONFIG_CFGSYNC),y)
CFLAGS += -I$(TOP)/cfg_mnt
LIBS += -L$(TOP)/cfg_mnt -lcfgmnt
endif

ifeq ($(RTCONFIG_QSR10G),y)
CFLAGS += -I$(TOP)/libqcsapi_client_10g
LIBS += -L$(TOP)/libqcsapi_client_10g -lqcsapi_client
endif

CFLAGS += -I$(TOP)/libpasswd
LIBS += -L$(TOP)/libpasswd -lpasswd

ifeq ($(RTCONFIG_LANTIQ),y)
LIBS += -L$(TOP)/fapi_wlan_common-05.04.00.131 -lfapiwlancommon -ldl -L$(TOP)/libhelper-1.4.0.2 -lhelper -L$(TOP)/libfapi-0.1 -lfapi -L$(TOP)/wireless_tools/ -liw
endif

all: httpd #test_apps

#test_apps: apps.c
#	$(CC) -DAPPS $(CFLAGS) $(LIBS) $^ -o $@

httpd: $(OBJS)
	@echo " [httpd] CC $@"
	@$(CC) -o $@ $(OBJS) $(LIBS) $(EXTRALDFLAGS)

	$(SIZECHECK)
	$(CPTMP)


install: all
	@echo " [httpd] Installing to $(INSTALLDIR)"
	@install -D httpd $(INSTALLDIR)/usr/sbin/httpd
	@$(STRIP) $(INSTALLDIR)/usr/sbin/httpd
	@cd $(INSTALLDIR)/usr/sbin/; ln -sf httpd httpds
	@chmod 0500 $(INSTALLDIR)/usr/sbin/httpd
	#@install -m 0755 test_apps $(INSTALLDIR)/usr/sbin
	#@$(STRIP) $(INSTALLDIR)/usr/sbin/test_apps

clean:
	rm -f httpd *.o .*.depend test_apps

size: httpd
	mipsel-uclibc-nm --print-size --size-sort httpd

# handle prebuilt object here
ifneq ($(wildcard ./prebuild/),)
ifeq ($(RTCONFIG_HTTPS),y)
pwenc.o:
	@-cp -f ./prebuild/pwenc.o pwenc.o
endif
web_hook.o:
	@-cp -f ./prebuild/web_hook.o web_hook.o
endif

%.o: %.c .%.depend
	@echo " [httpd] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
