#
# Copyright (C) 2020, 2021  T+A elektroakustik GmbH & Co. KG
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

compiler = meson.get_compiler('cpp')

if not compiler.has_header('doctest.h')
    subdir_done()
endif

testrunner_lib = static_library('testrunner', 'testrunner.cc',
    cpp_args: '-DDOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING',
    include_directories: '../src'
)

configure_file(input: '../documentation/models.json',
               output: 'test_models.json', copy: true)

configure_file(input: 'player_and_amplifier.json',
               output: 'test_player_and_amplifier.json', copy: true)

test('Configuration Store',
    executable('test_configstore',
        ['test_configstore.cc',
         'mock_os.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
        include_directories: '../src',
        link_with: [testrunner_lib, configstore_lib, sigpath_lib],
        build_by_default: false),
    workdir: meson.current_build_dir(),
    args: ['--reporters=strboxml', '--out=test_configstore.junit.xml'],
)

test('Configuration Store Roon Plug-In',
    executable('test_configstore_roon',
        ['test_configstore_roon.cc',
         'mock_os.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
        include_directories: '../src',
        link_with: [testrunner_lib, configstore_lib, configstore_roon_lib, sigpath_lib],
        build_by_default: false),
    workdir: meson.current_build_dir(),
    args: ['--reporters=strboxml', '--out=test_configstore_roon.junit.xml'],
)

test('Signal Paths',
    executable('test_signal_paths',
        ['test_signal_paths.cc',
         'mock_os.cc', 'mock_messages.cc', 'mock_backtrace.cc'],
        include_directories: '../src',
        link_with: [testrunner_lib, sigpath_lib],
        build_by_default: false),
    workdir: meson.current_build_dir(),
    args: ['--reporters=strboxml', '--out=test_signal_paths.junit.xml'],
)
