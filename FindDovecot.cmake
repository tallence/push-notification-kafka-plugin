# - Try to find Dovecot
# Once done this will define
#  DOVECOT_FOUND - System has Dovecot
#  DOVECOT_INCLUDE_DIRS - The Dovecot include directories
#  DOVECOT_DEFINITIONS - Compiler switches required for using Dovecot

find_package(PkgConfig)
pkg_check_modules(PC_DOVECOT QUIET dovecot)
set(DOVECOT_DEFINITIONS ${PC_DOVECOT_CFLAGS_OTHER})

find_path(DOVECOT_INCLUDE_DIR dovecot/config.h
        HINTS ${PC_DOVECOT_INCLUDEDIR} ${PC_DOVECOT_INCLUDE_DIRS}
        PATH_SUFFIXES dovecot)

find_path(DOVECOT_LIB_DIR doveadm
        HINTS /usr/lib/dovecot /usr/local/lib/dovecot
        PATH_SUFFIXES dovecot)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DOVECOT_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Dovecot DEFAULT_MSG DOVECOT_INCLUDE_DIR)

mark_as_advanced(DOVECOT_INCLUDE_DIR)

set(DOVECOT_INCLUDE_DIRS ${DOVECOT_INCLUDE_DIR} ${DOVECOT_INCLUDE_DIR}/dovecot)

find_library(LIBDOVECOT dovecot /usr/lib/dovecot /usr/local/lib/dovecot)
find_library(LIBDOVECOTSTORAGE dovecot-storage /usr/lib/dovecot /usr/local/lib/dovecot)
find_library(LIBDOVECOTNOTIFYPLUGIN 15_notify_plugin /usr/lib/dovecot /usr/local/lib/dovecot)
find_library(LIBDOVECOTPUSHNOTIFICATIONPLUGIN 20_push_notification_plugin /usr/lib/dovecot /usr/local/lib/dovecot)

