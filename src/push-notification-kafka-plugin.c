// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include <push-notification-kafka-plugin.h>

#include "lib.h"
#include "push-notification-drivers.h"

const char *push_notification_kafka_plugin_version = DOVECOT_ABI_VERSION;

extern struct push_notification_driver push_notification_driver_kafka;

void push_notification_kafka_plugin_init(struct module *module ATTR_UNUSED) {
  push_notification_driver_register(&push_notification_driver_kafka);
}

void push_notification_kafka_plugin_deinit(void) {
  push_notification_driver_unregister(&push_notification_driver_kafka);
}
