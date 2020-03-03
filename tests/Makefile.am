#
# Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
#
# This file is part of AuPaD.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
#

if WITH_DOCTEST
check_PROGRAMS = test_configstore test_configstore_roon test_signal_paths

TESTS = run_tests.sh

if WITH_VALGRIND
AM_TESTS_ENVIRONMENT = VALGRIND="$(VALGRIND)"
TESTS += valgrind.sh
endif

EXTRA_DIST = run_tests.sh valgrind.sh
CLEANFILES = *.junit.xml *.valgrind.xml

AM_CPPFLAGS = -DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING
AM_CPPFLAGS += -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CPPFLAGS += -I$(top_srcdir)/dbus_interfaces
AM_CFLAGS = $(CWARNINGS)
AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = libtestrunner.la

libtestrunner_la_SOURCES = testrunner.cc

test_configstore_SOURCES = \
    test_configstore.cc \
    mock_os.hh mock_os.cc \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_expectation.hh
test_configstore_LDADD = \
    libtestrunner.la \
    $(top_builddir)/src/libconfigstore.la \
    $(top_builddir)/src/libsigpath.la
test_configstore_CPPFLAGS = $(AM_CPPFLAGS)
test_configstore_CXXFLAGS = $(AM_CXXFLAGS)

test_configstore_roon_SOURCES = \
    test_configstore_roon.cc \
    mock_os.hh mock_os.cc \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_expectation.hh
test_configstore_roon_LDADD = \
    libtestrunner.la \
    $(top_builddir)/src/libconfigstore.la \
    $(top_builddir)/src/libconfigstore_roon.la \
    $(top_builddir)/src/libsigpath.la
test_configstore_roon_CPPFLAGS = $(AM_CPPFLAGS)
test_configstore_roon_CXXFLAGS = $(AM_CXXFLAGS)

test_signal_paths_SOURCES = \
    test_signal_paths.cc \
    mock_os.hh mock_os.cc \
    mock_messages.hh mock_messages.cc \
    mock_backtrace.hh mock_backtrace.cc \
    mock_expectation.hh
test_signal_paths_LDADD = \
    libtestrunner.la \
    $(top_builddir)/src/libsigpath.la
test_signal_paths_CPPFLAGS = $(AM_CPPFLAGS)
test_signal_paths_CXXFLAGS = $(AM_CXXFLAGS)

BUILT_SOURCES = test_models.json

CLEANFILES += $(BUILT_SOURCES)

test_models.json:
	$(LN_S) $(top_srcdir)/documentation/models.json test_models.json

doctest: $(check_PROGRAMS)
	for p in $(check_PROGRAMS); do \
	    if ./$$p $(DOCTEST_EXTRA_OPTIONS); then :; \
	    else \
	        echo "*****"; \
	        echo "***** Running $$p again in gdb due to failures"; \
	        echo "*****"; \
	        gdb -ex run --args ./$$p $(DOCTEST_EXTRA_OPTIONS); \
	    fi; \
	done

doctest-valgrind: $(check_PROGRAMS)
	for p in $(check_PROGRAMS); do $(VALGRIND) --leak-check=full --show-reachable=yes --error-limit=no ./$$p $(DOCTEST_EXTRA_OPTIONS); done
endif

