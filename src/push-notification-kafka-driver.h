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

#ifndef SRC_LIB_TEST_PUSH_NOTIFICATION_KAFKA_DRIVER_H_
#define SRC_LIB_TEST_PUSH_NOTIFICATION_KAFKA_DRIVER_H_

#include <librdkafka/rdkafka.h>
#include "lib.h"
#include "push-notification-events.h"

extern struct push_notification_event push_notification_event_messagenew;
extern struct push_notification_event push_notification_event_messageappend;

#define LOG_LABEL "Kafka Push Notification: "

#define DEFAULT_TOPIC "dovecot"
#define DEFAULT_SERVERS "localhost:9092"
#define DEFAULT_PREFIX "$"
#define DEFAULT_EVENTS                                                \
  "FlagsClear,FlagsSet,MailboxCreate,MailboxDelete,MailboxRename,"    \
  "MailboxSubscribe,MailboxUnsubscribe,MessageAppend,MessageExpunge," \
  "MessageNew,MessageRead,MessageTrash"
#define DEFAULT_DEBUG ""

/* This is data that is shared by all plugin users. */
struct push_notification_driver_kafka_global {
  int refcount;

  char *brokers;
  char *debug;

  /* if send message to topic fails, make a some retries */
  int max_retries;
  int retry_poll_time_in_ms;

  /* shutdown timeouts */
  int flush_time_in_ms;
  int topic_close_time_in_ms;
  int destroy_time_in_ms;

  void (*error_cb)(rd_kafka_t *rk, int err, const char *reason, void *opaque);
  void (*dr_msg_cb)(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque);
  rd_kafka_conf_t *rkc; /* Kafka configuration object */
  rd_kafka_t *rk;       /* Producer instance handle */
};
static struct push_notification_driver_kafka_global *kafka_global = NULL;

struct push_notification_driver_kafka_context {
  pool_t pool;

  char *topic;
  char **events;
  bool enabled;

  struct push_notification_driver_kafka_render_context render_ctx;

  rd_kafka_topic_t *rkt;
};

// required for testability: "inject" global configuration
extern struct push_notification_driver_kafka_global *init_kafka_global(void);

extern void push_notification_driver_kafka_send_to_kafka(struct push_notification_driver_kafka_context *ctx,
                                                         string_t *str, const char *username);

extern void push_notification_driver_kafka_init_topic(struct push_notification_driver_kafka_context *ctx);

extern void push_notification_driver_kafka_deinit_topic(struct push_notification_driver_kafka_context *ctx);

extern rd_kafka_t *push_notification_driver_kafka_init_global(void);

extern void push_notification_driver_kafka_deinit_global(void);

#endif /* SRC_LIB_TEST_PUSH_NOTIFICATION_KAFKA_DRIVER_H_ */
