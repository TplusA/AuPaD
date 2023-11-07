#! /bin/sh
#
# Run Cppcheck on sources and build directory.
#
# This script is primarily meant to be executed in a CI environment, but it is
# also useful for direct use by developers:
# - To avoid XML output, set environment variable XML_OUTPUT to a value unequal
#   to "yes".
# - Specify the location of the build directory in environment variable
#   BUILD_PATH because its default value "build" is most certainly wrong.
# - The Cppcheck binary can be specified by pointing environment variable
#   CPPCHECK to it (if it is left unspecified, the script tries to find a
#   specific version of Cppcheck, otherwise it defaults to plain "cppcheck").
#
# From a build directory, run the script as follows:
# $ XML_OUTPUT=no BUILD_PATH=$(pwd) /PATH/TO/SOURCE/aupad/cppcheck.sh
#

set -eu

XML_OUTPUT="${XML_OUTPUT:-yes}"

BUILD_PATH="${BUILD_PATH:-build}"
if test ! -d "${BUILD_PATH}"
then
    echo "Build path \"${BUILD_PATH}\" does not exist."
    exit 1
fi

CPPCHECK="${CPPCHECK:-}"
if test "x${CPPCHECK}" = 'x'
then
    STRBO_BASE_PATH="${STRBO_BASE_PATH:-${HOME}/StrBo}"
    if test -x "${STRBO_BASE_PATH}/cppcheck-2.12.0/bin/cppcheck"
    then
        CPPCHECK="${STRBO_BASE_PATH}/cppcheck-2.12.0/bin/cppcheck"
    else
        CPPCHECK='cppcheck'
    fi
fi

SOURCE_PATH="$(dirname "$0")"

if test "${XML_OUTPUT}" = 'yes'
then
    XML_OPTIONS='--xml --xml-version=2'
    XML_OUTFILE='cppcheck.xml'
else
    XML_OPTIONS=
    XML_OUTFILE='/dev/stdout'
fi

if test -f "${BUILD_PATH}/src/config.h"
then
    CONFIG_H="${BUILD_PATH}/src/config.h"
else
    CONFIG_H="${BUILD_PATH}/config.h"
fi

cd "${SOURCE_PATH}"

${CPPCHECK} \
  --quiet ${XML_OPTIONS} \
  --std=c++17 --platform=unix64 --library=std,gnu,gtk \
  --enable=all --inconclusive \
  --suppress=missingIncludeSystem \
  -j "$(nproc)" \
  -I. \
  -Isrc \
  -Idbus_interfaces \
  -I"${BUILD_PATH}" \
  -I"${BUILD_PATH}/src" \
  -DHAVE_CONFIG_H \
  -DNDEBUG \
  --include="${CONFIG_H}" \
  --include=strbo_common/cppcheck_17.hh \
  --config-exclude=/usr \
  --suppressions-list=cppcheck_suppressed.txt \
  --inline-suppr \
  "$@" \
  $(find src -regex '.*\.cc?$') 2>"${XML_OUTFILE}"
