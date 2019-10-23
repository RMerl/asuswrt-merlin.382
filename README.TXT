The GPL source is created for ASUS wireless router related products. Please visit the ASUS support site (http://support.asus.com) to get the latest GPL tarball.It has a lot in common with many wireless router open source projects, including Oleg/Tomato/DD-WRT/OpenWRT. Thanks the developers of those projects for making the source code available.

Set Up Environment(Tested in Fedora 8/9 and Ubuntu)

    1. prepare environment

	a. Ubuntu
		Install these packages (I used synaptic: "sudo synaptic")

		libncurses5
		libncurses5-dev
		m4
		bison
		gawk
		flex
		libstdc++6-4.4-dev
		g++-4.4
		g++
		gengetopt
		git (or git-core)
		gitk
		zlib1g-dev
		autoconf
		autopoint (or gettext)
		libtool
		shtool
		autogen
		mtd-utils
		intltool
		sharutils
		docbook-xsl-*
		libstdc++5
		texinfo
		dos2unix (or tofrodos)
		xsltproc
		uboot-mkimage
		device-tree-compiler
		python 2.7 or above.
		qemu
		gperf
		liblzo2-dev
		uuid-dev
		build-essential
		lzma-dev
		liblzma-dev
		lzma
		binutils-dev
		patch
		cmake
		intltool
		libglib2.0-dev
		gtk-doc-tools

		If you are using Ubuntu amd64, You may need to install 32-bit packages listed below.
			libc6-i386
			lib32stdc++6
			lib32z1
			libelf1:i386
			libncurses5:i386
			ia32-libs
			libc6-dev-i386

	b. Fedora
		python 2.7 or above.
		sudo yum groupinstall "Development Tools"
		sudo yum install gettext-devel libxml2-devel mtd-utils-ubi ncurses-devel \
			zlib-devel intltool sharutils docbook-dtds docbook-style-xsl \
			uboot-tools dtc gengetopt qemu gperf cmake glib2-devel gtk-doc

		If you are using Fedora 19 or above, you need to install below packages.
		sudo yum groupinstall "C Development Tools and Libraries"

		If you are using Fedora x86_64, You may need to install 32-bit packages listed below.
			elfutils-libelf.i686
			glibc.i686
			libstdc++.i686
			zlib.i686

		If you are using Fedora 24+, You may need to install 32-bit packages listed below.
			ncurses-compat-libs.i686

   2. prepare source to, ex, $HOME/asuswrt

	cd $HOME
	tar xvfz [tar file]

   3. setup development system

	Broadcom SoC models
	===================

		To install the tools:
		    * copy the tools/brcm/ directory to /opt
		    * add /opt/brcm/hndtools-mipsel-linux/bin to your path
		    * add /opt/brcm/hndtools-mipsel-uclibc/bin to your path

	Broadcom HND SoC models
	=======================

		Update your environment variables as following:
		    - LD_LIBRARY_PATH=/opt/toolchains/crosstools-arm-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25/usr/lib
		    - TOOLCHAIN_BASE=/opt/toolchains
		    - PATH=/opt/toolchains/crosstools-arm-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25/usr/bin:/opt/toolchains/crosstools-aarch64-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25/usr/bin:/projects/hnd/tools/linux/hndtools-armeabi-2011.09/bin:$PATH

	Mediatek/Ralink SoC models
	==========================

		To install the tools:
	    	    - copy the tools/brcm/ directory to /opt
		    - add /opt/brcm/hndtools-mipsel-linux/bin to your path
		    - add /opt/brcm/hndtools-mipsel-uclibc/bin to your path
		    If it is MT7621 or MT7628 chip:
	    	    - extract tools/buildroot-gcc463_32bits.tar.bz2 to /opt
		    - add /opt/buildroot-gcc463/bin to your path
		    otherwise :
	    	    - extract tools/buildroot-gcc342.tar.bz2 to /opt
		    - add /opt/buildroot-gcc342/bin to your path

		For MT7621 Uboot:
	    	    - extract mips-2012.03.tar.bz2 directory to /opt
		    - add /opt/mips-2012.03/bin to your uboot path

	Qualcomm QCA9557/QCA953x/QCA956x MIPS SoC models
	================================

		To install the tools:
		    Mesh Router:
		    - extract tools/openwrt-gcc463.mips.mesh.tar.bz2 directory to /opt
		    - add /opt/openwrt-gcc463.mips.mesh/bin to your path
		    - If you want to build small utilities out of asuswrt box,
		      add STAGING_DIR environment variable as below:

		      export STAGING_DIR=/opt/openwrt-gcc463.mips.mesh

		    Others: (For example, RT-AC55U, 4G-AC55U.)
	    	    - extract tools/openwrt-gcc463.mips.tar.bz2 directory to /opt
		    - add /opt/openwrt-gcc463.mips/bin to your path
		    - If you want to build small utilities out of asuswrt box,
		      add STAGING_DIR environment variable as below:

		      export STAGING_DIR=/opt/openwrt-gcc463.mips

	Qualcomm IPQ806x/IPQ40xx ARM SoC models
	===============================

		For example, BRT-AC828.

		To install the tools:
		    - extract tools/openwrt-gcc463.arm.tar.bz2 directory to /opt
		    - add /opt/openwrt-gcc463.arm/bin to your path
		    - If you want to build small utilities out of asuswrt box,
		      add STAGING_DIR environment variable as below:
	
		      export STAGING_DIR=/opt/openwrt-gcc463.arm

	Note: Broadcom/Ralink(except 4708 series) platform use the same toolchain for user space program, so please set PATH to the same directory as above

   4. build firmware.

	1. rt-n16
		cd release/src-rt
		make rt-n16

	2. rt-n56u
		cd release/src-ra
		make rt-n56u

	3. rt-n65u
		cd release/src-ra-3.0
		make rt-n65u

	4. rt-n14u (/ rt-ac52u / rt-ac51u / rt-n11p / rt-n54u)
		cd release/src-ra-mt7620
		make rt-n14u
		( make rt-ac52u  )
		( make rt-ac51u  )
		( make rt-n11p   )
		( make rt-n54u   )

	5. rt-ac56u (/ rt-ac68u / rt-n18uhp)
		cd release/src-rt-6.x.4708
		make rt-ac56u
		( make rt-ac68u  )
		( make rt-n18uhp )

	6. rt-ac55u (/ rt-ac55uhp )
		cd release/src-qca
		make rt-ac55u
		( make rt-ac55uhp )

	7. brt-ac828 (/ rt-ac88q / rt-ad7200 )
		cd release/src-qca-ipq8064
		make brt-ac828
		( make rt-ad7200 )

	8. rt-ac58u (/ rt-ac82u / map-ac1300 / map-ac2200 / vrz-ac1300 )
		cd release/src-qca-dakota
		make rt-ac58u
		( make rt-ac82u )
		( make map-ac1300 )
		( make map-ac2200 )
		( make vrz-ac1300 )

	9. rt-ac85u (/ rt-ac85u / rt-ac65u / rp-ac87 )
		cd release/src-ra-5010
		make rt-ac85u
		( make rt-ac65u )
		( make rp-ac87 )

	10. rt-ac1200 (/ rt-n11p_b1 / rt-n10p_v3 / rt-ac1200gu / rt-ac51u+ / rt-ac53 )
		cd release/src-ra-4300
		make rt-ac1200
		( make rt-n11p_b1 )
		( make rt-n10p_v3 )
		( make rt-ac1200gu )
		( make rt-ac51u+ )
		( make rt-ac53 )

	11. rp-ac68u (/ rp-ac53 / rp-ac55 )
		cd release/src-rtk-819x
		make rp-ac68u
		( make rp-ac53 )
		( make rp-ac55 )

