#
# Copyright (c) 2018 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

module_LTLIBRARIES = lib90_push_notification_kafka_plugin.la

AM_CPPFLAGS = $(LIBDOVECOT_INCLUDE)

lib90_push_notification_kafka_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
lib90_push_notification_kafka_plugin_la_LDFLAGS = -module -avoid-version
lib90_push_notification_kafka_plugin_la_LIBADD = $(LIBDOVECOT) -lrdkafka

lib90_push_notification_kafka_plugin_la_SOURCES = \
	push-notification-kafka-plugin.c \
	push-notification-kafka-event.c \
	push-notification-kafka-driver.c

noinst_HEADERS = \
	push-notification-kafka-event.h \
	push-notification-kafka-plugin.h 
	