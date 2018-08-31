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
	
test_programs = \
	test-kafka-event 

noinst_PROGRAMS = $(test_programs)

#test_libs = \
#	lib-test/libtest.la 	
test_kafka_event_CPPFLAGS = $(LIBDOVECOT_INCLUDE)
test_kafka_event_SOURCES = lib-test/push-notification-kafka-event-test.c lib-test/test-common.c lib-test/test-common.h 
test_kafka_event_LDFLAGS = -module -avoid-version
test_kafka_event_LDADD = push-notification-kafka-event.lo $(test_libs) $(LIBDOVECOT) -l15_notify_plugin -l20_push_notification_plugin -ldovecot-storage	
test_kafka_event_DEPENDENCIES = $(noinst_LTLIBRARIES) $(test_libs) $(LIBDOVECOT_DEPS)

check: check-am check-test
check-test: all-am
	for bin in $(test_programs); do \
	  if ! $(RUN_TEST) ./$$bin; then exit 1; fi; \
	done
