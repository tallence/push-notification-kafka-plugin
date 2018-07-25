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

#ifndef SRC_PUSH_NOTIFICATION_KAFKA_EVENT_H_
#define SRC_PUSH_NOTIFICATION_KAFKA_EVENT_H_

#include "lib.h"

struct push_notification_driver_kafka_render_context {
  bool send_flags;
  char *keyword_prefix;
};

extern string_t *push_notification_driver_kafka_render_mbox(
    struct push_notification_driver_txn *dtxn, struct push_notification_driver_kafka_render_context *render_ctx,
    struct push_notification_txn_mbox *mbox, struct push_notification_txn_event *const *event);

extern string_t *push_notification_driver_kafka_render_msg(
    struct push_notification_driver_txn *dtxn, struct push_notification_driver_kafka_render_context *render_ctx,
    struct push_notification_txn_msg *msg, struct push_notification_txn_event *const *event);

#endif  // SRC_PUSH_NOTIFICATION_KAFKA_EVENT_H_
