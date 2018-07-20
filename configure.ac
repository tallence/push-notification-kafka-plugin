#
# Copyright (c) 2018 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

AC_PREREQ([2.59])

AC_INIT([push-notification-kafka], [0.0.1])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar subdir-objects])
LT_INIT([shared disable-static pic-only])

AM_MAINTAINER_MODE

AC_DEFINE_UNQUOTED(PUSH_NOTIFICATION_KAFKA_NAME, "$PACKAGE_NAME", 
	[Define to the full name of push-notification-kafka for Dovecot.])
AC_DEFINE_UNQUOTED(PUSH_NOTIFICATION_KAFKA_VERSION, "$PACKAGE_VERSION", 
	[Define to the version of push-notification-kafka for Dovecot.])

AC_CHECK_HEADERS([librdkafka/rdkafka.h], [], [AC_MSG_ERROR([cannot build without Kafka support: librdkafka/rdkafka.h not found])], [])

AC_SEARCH_LIBS([rd_kafka_new], [rdkafka], [], [AC_MSG_ERROR([cannot build without Kafka support: librdkafka not found])], [])

AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_INSTALL

AC_ENABLE_SHARED
AC_DISABLE_STATIC

AC_ISC_POSIX

# Couple with Dovecot

DC_DOVECOT
DC_DOVECOT_MODULEDIR

# LIBDOVECOT_INCLUDE="$LIBDOVECOT_INCLUDE $LIBDOVECOT_STORAGE_INCLUDE" 
CFLAGS="$DOVECOT_CFLAGS"
LIBS="$DOVECOT_LIBS"
AC_SUBST(LIBDOVECOT_INCLUDE)

# Valgrind

AC_ARG_ENABLE(valgrind,
[AC_HELP_STRING([--enable-valgrind], [Enable Valgrind memory leak checks in testsuite [default=no]])],
    if test x$enableval = xno || test x$enableval = xauto; then
        want_valgrind=$enableval
    else
        want_valgrind=yes
    fi,
    want_valgrind=no)
AM_CONDITIONAL(TESTSUITE_VALGRIND, test "$want_valgrind" = "yes")

# Makefiles

AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_OUTPUT

echo
AC_MSG_NOTICE([Install prefix ................ : $prefix])
AC_MSG_NOTICE([Dovecot directory ............. : $dovecotdir])
