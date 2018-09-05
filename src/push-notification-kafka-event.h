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
#include "push-notification-drivers.h"
#include "push-notification-txn-mbox.h"
#include "push-notification-event-mailboxrename.h"

extern struct push_notification_event push_notification_event_mailboxcreate;
extern struct push_notification_event push_notification_event_mailboxrename;
extern struct push_notification_event push_notification_event_flagsclear;
extern struct push_notification_event push_notification_event_flagsset;
extern struct push_notification_event push_notification_event_messagenew;
extern struct push_notification_event push_notification_event_messageappend;

struct push_notification_driver_kafka_render_context {
  bool send_flags;
  char *keyword_prefix;
};
// T
extern bool str_starts_with(const char *str, const char *prefix);
extern bool write_flags(enum mail_flags flags, string_t *str);

extern string_t *push_notification_driver_kafka_render_mbox(
    struct push_notification_driver_txn *dtxn, struct push_notification_driver_kafka_render_context *render_ctx,
    struct push_notification_txn_mbox *mbox, struct push_notification_txn_event *const *event);

extern string_t *push_notification_driver_kafka_render_msg(
    struct push_notification_driver_txn *dtxn, struct push_notification_driver_kafka_render_context *render_ctx,
    struct push_notification_txn_msg *msg, struct push_notification_txn_event *const *event);

extern string_t *write_msg_prefix(struct push_notification_driver_txn *dtxn, const char *event_name,
                                  struct push_notification_txn_msg *msg);

extern string_t *write_flags_event(struct push_notification_driver_txn *dtxn,
                                   struct push_notification_driver_kafka_render_context *render_ctx,
                                   const char *event_name, struct push_notification_txn_msg *msg, enum mail_flags flags,
                                   ARRAY_TYPE(keywords) * keywords, enum mail_flags flags_old,
                                   ARRAY_TYPE(keywords) * keywords_old);

extern string_t *write_event_messagenew(struct push_notification_driver_txn *dtxn,
                                        struct push_notification_txn_msg *msg,
                                        struct push_notification_txn_event *const *event);
extern string_t *write_event_messageappend(struct push_notification_driver_txn *dtxn,
                                           struct push_notification_txn_msg *msg,
                                           struct push_notification_txn_event *const *event);
#endif  // SRC_PUSH_NOTIFICATION_KAFKA_EVENT_H_
