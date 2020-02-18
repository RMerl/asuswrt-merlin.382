# Copyright (c) 2001-2004, Roger Dingledine
# Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson
# Copyright (c) 2007-2019, The Tor Project, Inc.
# See LICENSE for licensing information

ACLOCAL_AMFLAGS = -I m4

noinst_LIBRARIES=
EXTRA_DIST=
noinst_HEADERS=
bin_PROGRAMS=
EXTRA_PROGRAMS=
CLEANFILES=
TESTS=
noinst_PROGRAMS=
DISTCLEANFILES=
bin_SCRIPTS=
AM_CPPFLAGS=\
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/ext \
	-I$(top_srcdir)/src/ext/trunnel \
	-I$(top_srcdir)/src/trunnel

AM_CFLAGS=@TOR_SYSTEMD_CFLAGS@ @CFLAGS_BUGTRAP@ @TOR_LZMA_CFLAGS@ @TOR_ZSTD_CFLAGS@
SHELL=@SHELL@

if COVERAGE_ENABLED
TESTING_TOR_BINARY=$(top_builddir)/src/app/tor-cov$(EXEEXT)
else
TESTING_TOR_BINARY=$(top_builddir)/src/app/tor$(EXEEXT)
endif

if USE_RUST
## this MUST be $(), otherwise am__DEPENDENCIES will not track it
rust_ldadd=$(top_builddir)/$(TOR_RUST_LIB_PATH) \
	$(TOR_RUST_EXTRA_LIBS)
else
rust_ldadd=
endif

# "Common" libraries used to link tor's utility code.
TOR_UTIL_LIBS = \
        src/lib/libtor-geoip.a \
	src/lib/libtor-process.a \
	src/lib/libtor-time.a \
	src/lib/libtor-fs.a \
	src/lib/libtor-encoding.a \
	src/lib/libtor-sandbox.a \
	src/lib/libtor-container.a \
	src/lib/libtor-net.a \
	src/lib/libtor-thread.a \
	src/lib/libtor-memarea.a \
	src/lib/libtor-math.a \
	src/lib/libtor-meminfo.a \
	src/lib/libtor-osinfo.a \
	src/lib/libtor-log.a \
	src/lib/libtor-lock.a \
	src/lib/libtor-fdio.a \
	src/lib/libtor-string.a \
	src/lib/libtor-term.a \
	src/lib/libtor-smartlist-core.a \
	src/lib/libtor-malloc.a \
	src/lib/libtor-wallclock.a \
	src/lib/libtor-err.a \
	src/lib/libtor-intmath.a \
	src/lib/libtor-ctime.a

# Variants of the above for linking the testing variant of tor (for coverage
# and tests)
if UNITTESTS_ENABLED
TOR_UTIL_TESTING_LIBS = \
        src/lib/libtor-geoip-testing.a \
	src/lib/libtor-process-testing.a \
	src/lib/libtor-time-testing.a \
	src/lib/libtor-fs-testing.a \
	src/lib/libtor-encoding-testing.a \
	src/lib/libtor-sandbox-testing.a \
	src/lib/libtor-container-testing.a \
	src/lib/libtor-net-testing.a \
	src/lib/libtor-thread-testing.a \
	src/lib/libtor-memarea-testing.a \
	src/lib/libtor-math-testing.a \
	src/lib/libtor-meminfo-testing.a \
	src/lib/libtor-osinfo-testing.a \
	src/lib/libtor-term-testing.a \
	src/lib/libtor-log-testing.a \
	src/lib/libtor-lock-testing.a \
	src/lib/libtor-fdio-testing.a \
	src/lib/libtor-string-testing.a \
	src/lib/libtor-smartlist-core-testing.a \
	src/lib/libtor-malloc-testing.a \
	src/lib/libtor-wallclock-testing.a \
	src/lib/libtor-err-testing.a \
	src/lib/libtor-intmath.a \
	src/lib/libtor-ctime-testing.a
endif

# Internal crypto libraries used in Tor
TOR_CRYPTO_LIBS = \
	src/lib/libtor-tls.a \
	src/lib/libtor-crypt-ops.a \
	$(LIBKECCAK_TINY) \
	$(LIBDONNA)

# Variants of the above for linking the testing variant of tor (for coverage
# and tests)
if UNITTESTS_ENABLED
TOR_CRYPTO_TESTING_LIBS = \
	src/lib/libtor-tls-testing.a \
	src/lib/libtor-crypt-ops-testing.a \
	$(LIBKECCAK_TINY) \
	$(LIBDONNA)
endif

# All static libraries used to link tor.
TOR_INTERNAL_LIBS = \
	src/core/libtor-app.a \
	src/lib/libtor-compress.a \
	src/lib/libtor-evloop.a \
	$(TOR_CRYPTO_LIBS) \
	$(TOR_UTIL_LIBS) \
	src/trunnel/libor-trunnel.a \
	src/lib/libtor-trace.a

# Variants of the above for linking the testing variant of tor (for coverage
# and tests)
if UNITTESTS_ENABLED
TOR_INTERNAL_TESTING_LIBS = \
	src/core/libtor-app-testing.a \
	src/lib/libtor-compress-testing.a \
	src/lib/libtor-evloop-testing.a \
	$(TOR_CRYPTO_TESTING_LIBS) \
	$(TOR_UTIL_TESTING_LIBS) \
	src/trunnel/libor-trunnel-testing.a \
	src/lib/libtor-trace.a
endif

TOR_LDFLAGS_CRYPTLIB=@TOR_LDFLAGS_openssl@
TOR_LIBS_CRYPTLIB=@TOR_OPENSSL_LIBS@
TOR_CFLAGS_CRYPTLIB=
if USE_NSS
TOR_CFLAGS_CRYPTLIB+=@NSS_CFLAGS@
TOR_LIBS_CRYPTLIB+=@NSS_LIBS@
endif

# All libraries used to link tor-cov

include src/include.am
include doc/include.am
include contrib/include.am

EXTRA_DIST+= \
	ChangeLog					\
	CONTRIBUTING					\
	CODE_OF_CONDUCT                                 \
	INSTALL						\
	LICENSE						\
	Makefile.nmake					\
	README						\
	ReleaseNotes					\
	scripts/maint/checkIncludes.py                  \
	scripts/maint/checkSpace.pl

## This tells etags how to find mockable function definitions.
AM_ETAGSFLAGS=--regex='{c}/MOCK_IMPL([^,]+,\W*\([a-zA-Z0-9_]+\)\W*,/\1/s'

if COVERAGE_ENABLED
TEST_CFLAGS=-fno-inline -fprofile-arcs -ftest-coverage
if DISABLE_ASSERTS_IN_UNIT_TESTS
TEST_CPPFLAGS=-DTOR_UNIT_TESTS -DTOR_COVERAGE -DDISABLE_ASSERTS_IN_UNIT_TESTS @TOR_MODULES_ALL_ENABLED@
else
TEST_CPPFLAGS=-DTOR_UNIT_TESTS -DTOR_COVERAGE @TOR_MODULES_ALL_ENABLED@
endif
TEST_NETWORK_FLAGS=--coverage --hs-multi-client 1
else
TEST_CFLAGS=
TEST_CPPFLAGS=-DTOR_UNIT_TESTS @TOR_MODULES_ALL_ENABLED@
TEST_NETWORK_FLAGS=--hs-multi-client 1
endif
TEST_NETWORK_WARNING_FLAGS=--quiet --only-warnings

if LIBFUZZER_ENABLED
TEST_CFLAGS += -fsanitize-coverage=trace-pc-guard,trace-cmp,trace-div
# not "edge"
endif

TEST_NETWORK_ALL_LOG_DIR=$(top_builddir)/test_network_log
TEST_NETWORK_ALL_DRIVER_FLAGS=--color-tests yes

#install-data-local:
#	$(INSTALL) -m 755 -d $(LOCALSTATEDIR)/lib/tor

# Allows to override rpmbuild with rpmbuild-md5 from fedora-packager so that
# building for EL5 won't fail on https://bugzilla.redhat.com/show_bug.cgi?id=490613
RPMBUILD ?= rpmbuild

# Use automake's dist-gzip target to build the tarball
dist-rpm: dist-gzip
	TIMESTAMP=$$(date +"%Y-%m-%d_%H.%M.%S"); 				\
	RPM_BUILD_DIR=$$(mktemp -d "/tmp/tor-rpm-build-$$TIMESTAMP-XXXX");	\
	mkdir -p "$$RPM_BUILD_DIR"/{BUILD,RPMS,SOURCES/"tor-$(VERSION)",SPECS,SRPMS}; \
	cp -fa "$(distdir).tar.gz" "$$RPM_BUILD_DIR"/SOURCES/;			\
	LIBS=-lrt $(RPMBUILD) -ba --define "_topdir $$RPM_BUILD_DIR" tor.spec; 	\
	cp -fa "$$RPM_BUILD_DIR"/SRPMS/* .;                           		\
	cp -fa "$$RPM_BUILD_DIR"/RPMS/* .;                            		\
	rm -rf "$$RPM_BUILD_DIR";						\
	echo "RPM build finished";						\
	#end of dist-rpm

doxygen:
	doxygen && cd doc/doxygen/latex && make

test: all
	$(top_builddir)/src/test/test

check-local: check-spaces check-changes check-includes

need-chutney-path:
	@if test ! -d "$$CHUTNEY_PATH"; then \
		echo '$$CHUTNEY_PATH was not set.'; \
		if test -d $(top_srcdir)/../chutney -a -x $(top_srcdir)/../chutney/chutney; then \
			echo "Assuming test-network.sh will find" $(top_srcdir)/../chutney; \
		else \
			echo; \
			echo "To run these tests, git clone https://git.torproject.org/chutney.git ; export CHUTNEY_PATH=\`pwd\`/chutney"; \
			exit 1; \
		fi \
	fi

# Note that test-network requires a copy of Chutney in $CHUTNEY_PATH.
# Chutney can be cloned from https://git.torproject.org/chutney.git .
test-network: need-chutney-path $(TESTING_TOR_BINARY) src/tools/tor-gencert
	$(top_srcdir)/src/test/test-network.sh $(TEST_NETWORK_FLAGS)

# Run all available tests using automake's test-driver
# only run IPv6 tests if we can ping6 ::1 (localhost)
# only run IPv6 tests if we can ping ::1 (localhost)
# some IPv6 tests will fail without an IPv6 DNS server (see #16971 and #17011)
# only run mixed tests if we have a tor-stable binary
# Try the syntax for BSD ping6, Linux ping6, and Linux ping -6,
# because they're incompatible
test-network-all: need-chutney-path test-driver $(TESTING_TOR_BINARY) src/tools/tor-gencert
	mkdir -p $(TEST_NETWORK_ALL_LOG_DIR)
	rm -f $(TEST_NETWORK_ALL_LOG_DIR)/*.log $(TEST_NETWORK_ALL_LOG_DIR)/*.trs
	@flavors="$(TEST_CHUTNEY_FLAVORS)"; \
	if ping6 -q -c 1 -o ::1 >/dev/null 2>&1 || ping6 -q -c 1 -W 1 ::1 >/dev/null 2>&1 || ping -6 -c 1 -W 1 ::1 >/dev/null 2>&1; then \
		echo "ping6 ::1 or ping ::1 succeeded, running IPv6 flavors: $(TEST_CHUTNEY_FLAVORS_IPV6)."; \
		flavors="$$flavors $(TEST_CHUTNEY_FLAVORS_IPV6)"; \
	else \
		echo "ping6 ::1 and ping ::1 failed, skipping IPv6 flavors: $(TEST_CHUTNEY_FLAVORS_IPV6)."; \
		skip_flavors="$$skip_flavors $(TEST_CHUTNEY_FLAVORS_IPV6)"; \
	fi; \
	if command -v tor-stable >/dev/null 2>&1; then \
		echo "tor-stable found, running mixed flavors: $(TEST_CHUTNEY_FLAVORS_MIXED)."; \
		flavors="$$flavors $(TEST_CHUTNEY_FLAVORS_MIXED)"; \
	else \
		echo "tor-stable not found, skipping mixed flavors: $(TEST_CHUTNEY_FLAVORS_MIXED)."; \
		skip_flavors="$$skip_flavors $(TEST_CHUTNEY_FLAVORS_MIXED)"; \
	fi; \
	for f in $$skip_flavors; do \
		echo "SKIP: $$f"; \
	done; \
	for f in $$flavors; do \
		$(SHELL) $(top_srcdir)/test-driver --test-name $$f --log-file $(TEST_NETWORK_ALL_LOG_DIR)/$$f.log --trs-file $(TEST_NETWORK_ALL_LOG_DIR)/$$f.trs $(TEST_NETWORK_ALL_DRIVER_FLAGS) $(top_srcdir)/src/test/test-network.sh --flavor $$f $(TEST_NETWORK_FLAGS); \
		$(top_srcdir)/src/test/test-network.sh $(TEST_NETWORK_WARNING_FLAGS); \
	done; \
	echo "Log and result files are available in $(TEST_NETWORK_ALL_LOG_DIR)."; \
	! grep -q FAIL $(TEST_NETWORK_ALL_LOG_DIR)/*.trs

need-stem-path:
	@if test ! -d "$$STEM_SOURCE_DIR"; then \
		echo '$$STEM_SOURCE_DIR was not set.'; echo; \
		echo "To run these tests, git clone https://git.torproject.org/stem.git/ ; export STEM_SOURCE_DIR=\`pwd\`/stem"; \
		exit 1; \
	fi

test-stem: need-stem-path $(TESTING_TOR_BINARY)
	@$(PYTHON) "$$STEM_SOURCE_DIR"/run_tests.py --tor "$(TESTING_TOR_BINARY)" --integ --log notice --target RUN_ALL;

test-stem-full: need-stem-path $(TESTING_TOR_BINARY)
	@$(PYTHON) "$$STEM_SOURCE_DIR"/run_tests.py --tor "$(TESTING_TOR_BINARY)" --all --log notice --target RUN_ALL,ONLINE -v;

test-full: need-stem-path need-chutney-path check test-network test-stem

test-full-online: need-stem-path need-chutney-path check test-network test-stem-full

reset-gcov:
	rm -f $(top_builddir)/src/*/*.gcda $(top_builddir)/src/*/*/*.gcda

HTML_COVER_DIR=$(top_builddir)/coverage_html
coverage-html: all
if COVERAGE_ENABLED
	test -e "`which lcov`" || (echo "lcov must be installed. See <http://ltp.sourceforge.net/coverage/lcov.php>." && false)
	test -d "$(HTML_COVER_DIR)" || $(MKDIR_P) "$(HTML_COVER_DIR)"
	lcov --rc lcov_branch_coverage=1 --directory $(top_builddir)/src --zerocounters
	$(MAKE) reset-gcov
	$(MAKE) check
	lcov --capture --rc lcov_branch_coverage=1 --no-external --directory $(top_builddir) --base-directory $(top_srcdir) --output-file "$(HTML_COVER_DIR)/lcov.tmp"
	lcov --remove "$(HTML_COVER_DIR)/lcov.tmp" --rc lcov_branch_coverage=1 'test/*' 'ext/tinytest*' '/usr/*' --output-file "$(HTML_COVER_DIR)/lcov.info"
	genhtml --branch-coverage -o "$(HTML_COVER_DIR)" "$(HTML_COVER_DIR)/lcov.info"
else
	@printf "Not configured with --enable-coverage, run ./configure --enable-coverage\n"
endif

coverage-html-full: all
	test -e "`which lcov`" || (echo "lcov must be installed. See <http://ltp.sourceforge.net/coverage/lcov.php>." && false)
	test -d "$(HTML_COVER_DIR)" || mkdir -p "$(HTML_COVER_DIR)"
	lcov --rc lcov_branch_coverage=1 --directory ./src --zerocounters
	$(MAKE) reset-gcov
	$(MAKE) check
	$(MAKE) test-stem-full
	CHUTNEY_TOR=tor-cov CHUTNEY_TOR_GENCERT=tor-cov-gencert $(top_srcdir)/src/test/test-network.sh
	CHUTNEY_TOR=tor-cov CHUTNEY_TOR_GENCERT=tor-cov-gencert $(top_srcdir)/src/test/test-network.sh --flavor hs
	lcov --capture --rc lcov_branch_coverage=1 --no-external --directory . --output-file "$(HTML_COVER_DIR)/lcov.tmp"
	lcov --remove "$(HTML_COVER_DIR)/lcov.tmp" --rc lcov_branch_coverage=1 'test/*' 'ext/tinytest*' '/usr/*' --output-file "$(HTML_COVER_DIR)/lcov.info"
	genhtml --branch-coverage -o "$(HTML_COVER_DIR)" "$(HTML_COVER_DIR)/lcov.info"

# Avoid strlcpy.c, strlcat.c, aes.c, OpenBSD_malloc_Linux.c, sha256.c,
# tinytest*.[ch]
check-spaces:
if USE_PERL
	$(PERL) $(top_srcdir)/scripts/maint/checkSpace.pl -C \
		$(top_srcdir)/src/lib/*/*.[ch] \
		$(top_srcdir)/src/core/*/*.[ch] \
		$(top_srcdir)/src/feature/*/*.[ch] \
		$(top_srcdir)/src/app/*/*.[ch] \
		$(top_srcdir)/src/test/*.[ch] \
		$(top_srcdir)/src/test/*/*.[ch] \
		$(top_srcdir)/src/tools/*.[ch]
endif

check-includes:
if USEPYTHON
	$(PYTHON) $(top_srcdir)/scripts/maint/checkIncludes.py
endif

check-docs: all
	$(PERL) $(top_builddir)/scripts/maint/checkOptionDocs.pl

check-logs:
	$(top_srcdir)/scripts/maint/checkLogs.pl \
		$(top_srcdir)/src/*/*.[ch] | sort -n

.PHONY: check-typos
check-typos:
	@if test -x "`which misspell 2>&1;true`"; then \
		echo "Checking for Typos ..."; \
		(misspell \
			$(top_srcdir)/src/[^e]*/*.[ch] \
			$(top_srcdir)/doc \
			$(top_srcdir)/contrib \
			$(top_srcdir)/scripts \
			$(top_srcdir)/README \
			$(top_srcdir)/ChangeLog \
			$(top_srcdir)/INSTALL \
			$(top_srcdir)/ReleaseNotes \
			$(top_srcdir)/LICENSE); \
	else \
		echo "Tor can use misspell to check for typos."; \
		echo "It seems that you don't have misspell installed."; \
		echo "You can install the latest version of misspell here: https://github.com/client9/misspell#install"; \
	fi

.PHONY: rustfmt
rustfmt:
if USE_RUST
	@if test -x "`which cargo-fmt 2>&1;true`"; then \
		echo "Formatting Rust code ..."; \
		(cd "$(top_srcdir)/src/rust" && cargo fmt --all --); \
	else \
		echo "Tor uses rustfmt (via cargo-fmt) to format Rust code."; \
		echo "However, it seems that you don't have rustfmt installed."; \
		printf "You can install rustfmt by following the directions here:"; \
		echo " https://github.com/rust-lang-nursery/rustfmt"; \
	fi
endif

.PHONY: check-rustfmt
check-rustfmt:
if USE_RUST
	@if test -x "`which cargo-fmt 2>&1;true`"; then \
		printf "Running rustfmt..."; \
		(cd "$(top_srcdir)/src/rust" && cargo fmt --all -- --check && echo "done.") || \
		(echo "**************** check-rustfmt failed. ****************"; \
		 echo "   Run \`make rustfmt\` to apply the above changes."; \
		 exit 1); \
	else \
		echo "Tor uses rustfmt (via cargo-fmt) to format Rust code."; \
		echo "However, it seems that you don't have rustfmt installed."; \
		printf "You can install rustfmt by following the directions here:"; \
		echo " https://github.com/rust-lang-nursery/rustfmt"; \
	fi
endif

.PHONY: clippy
clippy:
if USE_RUST
	@if test -x "`which cargo-clippy 2>&1;true`"; then \
		echo "Running cargo clippy ..."; \
		echo "Prepare yourself for the onslaught of suggestions ..."; \
		(cd "$(top_srcdir)/src/rust" && cargo clippy); \
	else \
		echo "Tor can use clippy to lint Rust code."; \
		echo "However, it seems that you don't have clippy installed."; \
		echo "You can install the latest version of clippy by following the directions here: https://github.com/rust-lang-nursery/rust-clippy"; \
	fi
endif

.PHONY: check-changes
check-changes:
if USEPYTHON
	@if test -d "$(top_srcdir)/changes"; then \
		$(PYTHON) $(top_srcdir)/scripts/maint/lintChanges.py $(top_srcdir)/changes; \
		fi
endif

.PHONY: update-versions
update-versions:
	$(PERL) $(top_builddir)/scripts/maint/updateVersions.pl

.PHONY: callgraph
callgraph:
	$(top_builddir)/scripts/maint/run_calltool.sh

version:
	@echo "Tor @VERSION@"
	@if test -d "$(top_srcdir)/.git" && test -x "`which git 2>&1;true`"; then \
	   echo -n "git: " ;\
	   (cd "$(top_srcdir)" && git rev-parse --short=16 HEAD); \
	fi

mostlyclean-local:
	rm -f $(top_builddir)/src/*/*.gc{da,no} $(top_builddir)/src/*/*/*.gc{da,no}
	rm -rf $(HTML_COVER_DIR)
	rm -rf $(top_builddir)/doc/doxygen
	rm -rf $(TEST_NETWORK_ALL_LOG_DIR)

clean-local:
	rm -rf $(top_builddir)/src/rust/target
	rm -rf $(top_builddir)/src/rust/.cargo/registry

if USE_RUST
distclean-local: distclean-rust
endif

# This relies on some internal details of how automake implements
# distcheck.  We check two directories because automake-1.15 changed
# from $(distdir)/_build to $(distdir)/_build/sub.
show-distdir-testlog:
	@if test -d "$(distdir)/_build/sub"; then \
	  cat $(distdir)/_build/sub/$(TEST_SUITE_LOG); \
	else \
	  cat $(distdir)/_build/$(TEST_SUITE_LOG); fi

# Similarly, this relies on automake internals to run file on an
# intermittent core file whose provenance is not known to us.  See
# ticket 26787.
show-distdir-core:
	@if test -d "$(distdir)/_build/sub"; then \
	  file $(distdir)/_build/sub/core ; \
	else \
	  file $(distdir)/_build/core; fi

show-libs:
	@echo $(TOR_INTERNAL_LIBS)

show-testing-libs:
	@echo $(TOR_INTERNAL_TESTING_LIBS)
