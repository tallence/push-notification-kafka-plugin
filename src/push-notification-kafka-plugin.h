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

#ifndef PUSH_NOTIFICATION_KAFKA_PLUGIN_H
#define PUSH_NOTIFICATION_KAFKA_PLUGIN_H

struct module;

void
push_notification_kafka_plugin_init (struct module *module);
void
push_notification_kafka_plugin_deinit (void);

#endif /* PUSH_NOTIFICATION_KAFKA_PLUGIN_H */
