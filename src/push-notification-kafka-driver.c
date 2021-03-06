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

#include <time.h>
#include <syslog.h>
#include <librdkafka/rdkafka.h>

#include "config.h"
#include "lib.h"
#include "str.h"
#include "array.h"
#include "hash.h"
#include "json-parser.h"
#include "iso8601-date.h"
#include "macros.h"

#include "push-notification-drivers.h"
#include "push-notification-events.h"
#include "push-notification-txn-mbox.h"
#include "push-notification-txn-msg.h"
#include "push-notification-event-messagenew.h"
#include "push-notification-event-messageappend.h"
#include "push-notification-event-flagsclear.h"
#include "push-notification-event-flagsset.h"

#include "push-notification-kafka-plugin.h"
#include "push-notification-kafka-event.h"
#include "push-notification-kafka-driver.h"

/* Kafka stuff */

struct push_notification_driver_kafka_global *kafka_global = NULL;

/**
 * @brief Message delivery report callback.
 *
 * This callback is called exactly once per message, indicating if
 * the message was succesfully delivered
 * (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) or permanently
 * failed delivery (rkmessage->err != RD_KAFKA_RESP_ERR_NO_ERROR).
 *
 * The callback is triggered from rd_kafka_poll() and executes on
 * the application's thread.
 */
static void push_notification_driver_kafka_msg_cb(rd_kafka_t *rk ATTR_UNUSED, const rd_kafka_message_t *rkmessage,
                                                  void *opaque ATTR_UNUSED) {
  if (rkmessage->err) {
    fprintf(stderr, "%smsg_cb: message delivery failed: %s\n", LOG_LABEL, rd_kafka_err2str(rkmessage->err));
  }
#ifdef DEBUG
  else {
    i_debug("%smsg_cb: message delivered (%zd bytes, partition %" PRId32 ")", LOG_LABEL, rkmessage->len,
            rkmessage->partition);
  }
#endif
  /* The rkmessage is destroyed automatically by librdkafka */
}
static void push_notification_driver_kafka_err_cb(rd_kafka_t *rk, int err, const char *reason,
                                                  void *opaque ATTR_UNUSED) {
  fprintf(stderr, "%serr_cb: %s: %s: %s\n", LOG_LABEL, rd_kafka_name(rk), rd_kafka_err2str(err), reason);
}

static void push_notification_driver_log_print(const rd_kafka_t *rk, int level, const char *fac, const char *buf) {
  if (level == LOG_ERR) {
    fprintf(stderr, "%sRDKAFKA-%d-%s: %s: %s\n", LOG_LABEL, level, fac, rk ? rd_kafka_name(rk) : "-", buf);
  }
}

static void read_plugin_kafka_settings(const char *prefix) {
  const char *const *envs;
  unsigned int i, count;
  char errstr[512];

  if (kafka_global->user == NULL || kafka_global->user->set == NULL) {
    return;
  }
  if (!array_is_created(&kafka_global->user->set->plugin_envs)) {
    return;
  }
  if (kafka_global->rkc == NULL) {
    return;
  }
  if (prefix == NULL) {
    return;
  }

  envs = array_get(&kafka_global->user->set->plugin_envs, &count);
  for (i = 0; i < count; i += 2) {
    if (strlen(envs[i]) > strlen(prefix)) {
      if (strncmp(envs[i], prefix, strlen(prefix)) == 0) {
        const char *s = envs[i] + strlen(prefix) + 1;
        if (rd_kafka_conf_set(kafka_global->rkc, s, envs[i + 1], errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
          i_warning("%s", errstr);
        }
      }
    }
  }
}

rd_kafka_t *push_notification_driver_kafka_init_global() {
  if (kafka_global->rk == NULL) {
    char errstr[512]; /* librdkafka API error reporting buffer */
#ifdef DEBUG
    i_debug("%sinit_global - initialize brokers=%s", LOG_LABEL, kafka_global->brokers);
#endif
    /*
     * Create Kafka client configuration place-holder
     */
    kafka_global->rkc = rd_kafka_conf_new();

    /* do not use dovecot logging (e.g. i_debug, i_info, i_err) in log callback. */
    rd_kafka_conf_set_log_cb(kafka_global->rkc, push_notification_driver_log_print);

    // check 90-plugin.conf for librbkafka settings.
    read_plugin_kafka_settings("kafka.notification.settings");

    rd_kafka_conf_set_error_cb(kafka_global->rkc, kafka_global->error_cb);
    rd_kafka_conf_set_dr_msg_cb(kafka_global->rkc, kafka_global->dr_msg_cb);

    /* Set bootstrap broker(s) as a comma-separated list of
     * host or host:port (default port 9092).
     * librdkafka will use the bootstrap brokers to acquire the full
     * set of brokers from the cluster. */
    if (rd_kafka_conf_set(kafka_global->rkc, "bootstrap.servers", kafka_global->brokers, errstr, sizeof(errstr)) !=
        RD_KAFKA_CONF_OK) {
      i_error("%sinit_global - rd_kafka_conf_set(bootstrap.servers) failed with %s", LOG_LABEL, errstr);
      return NULL;
    }
    if (rd_kafka_conf_set(kafka_global->rkc, "debug", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
      i_error("%sinit_global - rd_kafka_conf_set(debug) failed with %s", LOG_LABEL, errstr);
      return NULL;
    }

    /*
     * Create producer instance.
     *
     * NOTE: rd_kafka_new() takes ownership of the conf object
     *       and the application must not reference it again after
     *       this call.
     */
    kafka_global->rk = rd_kafka_new(RD_KAFKA_PRODUCER, kafka_global->rkc, errstr, sizeof(errstr));
    if (kafka_global->rk == NULL) {
      i_error(
          "%sinit_global - rd_kafka_new() failed to create new producer with "
          "%s",
          LOG_LABEL, errstr);

      rd_kafka_conf_destroy(kafka_global->rkc);
      kafka_global->rkc = NULL;
    }
    /* set log level to err (default is LOG_DEBUG)*/
    rd_kafka_set_log_level(kafka_global->rk, LOG_ERR);
  }

  return kafka_global->rk;
}

void push_notification_driver_kafka_deinit_global() {
  if (kafka_global->rk != NULL) {
/* Shutdown Kafka */

/* Wait for final messages to be delivered or fail.
 * rd_kafka_flush() is an abstraction over rd_kafka_poll() which
 * waits for all messages to be delivered. */
#ifdef DEBUG
    i_debug("%sdeinit_global - flushing Kafka messages...", LOG_LABEL);
#endif
    rd_kafka_flush(kafka_global->rk, kafka_global->flush_time_in_ms);
#ifdef DEBUG
    i_debug("%sdeinit_global - rd_kafka_destroy...", LOG_LABEL);
#endif
    /* Destroy the Kafka producer instance */
    rd_kafka_destroy(kafka_global->rk);

    rd_kafka_wait_destroyed(kafka_global->destroy_time_in_ms);

    kafka_global->rk = NULL;
  }
}

void push_notification_driver_kafka_init_topic(struct push_notification_driver_kafka_context *ctx) {
  if (push_notification_driver_kafka_init_global() != NULL) {
#ifdef DEBUG
    i_debug("%sinit_topic - initialize topic=%s", LOG_LABEL, ctx->topic);
#endif
    if (ctx->rkt == NULL) {
      /* Create topic object that will be reused for each message
       * produced.
       *
       * Both the producer instance (rd_kafka_t) and topic objects (topic_t)
       * are long-lived objects that should be reused as much as possible.
       */
      ctx->rkt = rd_kafka_topic_new(kafka_global->rk, ctx->topic, NULL);
      if (!ctx->rkt) {
        i_error(
            "%sinit_topic - rd_kafka_topic_new() failed to create topic %s "
            "object with %s",
            LOG_LABEL, ctx->topic, rd_kafka_err2str(rd_kafka_last_error()));
      }
    }
  } else {
    i_error("%sinit_topic - Kafka not initialized", LOG_LABEL);
  }
}

void push_notification_driver_kafka_deinit_topic(struct push_notification_driver_kafka_context *ctx) {
  if (kafka_global->rk != NULL) {
    rd_kafka_poll(kafka_global->rk, kafka_global->topic_close_time_in_ms /*non-blocking*/);
  }

  if (ctx->rkt != NULL) {
#ifdef DEBUG
    i_debug("%sdeinit_topic: destroy Kafka topic object '%s'", LOG_LABEL, ctx->topic);
#endif
    rd_kafka_topic_destroy(ctx->rkt);
    ctx->rkt = NULL;
  }
}

void push_notification_driver_kafka_send_to_kafka(struct push_notification_driver_kafka_context *ctx, string_t *str,
                                                  const char *username) {
  i_debug("%ssend_to_kafka", LOG_LABEL);
  push_notification_driver_kafka_init_topic(ctx);

  i_assert(str != NULL);

  if (ctx->rkt != NULL) {
    int retry_counter = 0;

  retry:
    if (rd_kafka_produce(ctx->rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, (void *)str_c(str), str_len(str),
                         username, strlen(username), NULL) == -1) {
      /* Failed to *enqueue* message for producing. */
      i_error("%ssend_to_kafka - failed to produce to topic=%s: %s", LOG_LABEL, ctx->topic,
              rd_kafka_err2str(rd_kafka_last_error()));

      /* Poll to handle delivery reports */
      if (rd_kafka_last_error() == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
        /* If the internal queue is full, wait for
         * messages to be delivered and then retry.
         * The internal queue represents both
         * messages to be sent and messages that have
         * been sent or failed, awaiting their
         * delivery report callback to be called.
         *
         * The internal queue is limited by the
         * configuration property
         * queue.buffering.max.messages */
        rd_kafka_poll(kafka_global->rk, kafka_global->retry_poll_time_in_ms);
        if (retry_counter++ < kafka_global->max_retries) {
          goto retry;
        }
      }
    }
#ifdef DEBUG
    i_debug("%ssend_to_kafka - send %zu bytes to topic=%s with key=%s", LOG_LABEL, str_len(str), ctx->topic, username);
#endif
    /* Keep Kafka happy. */
    rd_kafka_poll(kafka_global->rk, 0 /*non-blocking*/);
  } else {
    i_error("%ssend_to_kafka - topic=%s not initialized", LOG_LABEL, ctx->topic);
  }
}

struct push_notification_driver_kafka_global *init_kafka_global() {
  kafka_global = i_new(struct push_notification_driver_kafka_global, 1);
  kafka_global->error_cb = push_notification_driver_kafka_err_cb;
  kafka_global->dr_msg_cb = push_notification_driver_kafka_msg_cb;
  return kafka_global;
}

/* The push notification driver itself */

static int push_notification_driver_kafka_init(struct push_notification_driver_config *config, struct mail_user *user,
                                               pool_t pool, void **context, const char **error_r ATTR_UNUSED) {
  const char *tmp;
  struct push_notification_driver_kafka_context *ctx = p_new(pool, struct push_notification_driver_kafka_context, 1);

  ctx->pool = pool;

  /* Process driver configuration. */

  tmp = hash_table_lookup(config->config, (const char *)"topic");
  if (tmp == NULL) {
    ctx->topic = i_strdup(DEFAULT_TOPIC);
  } else {
    ctx->topic = i_strdup(tmp);
  }

  tmp = hash_table_lookup(config->config, (const char *)"keyword_prefix");
  if (tmp == NULL) {
    ctx->render_ctx.keyword_prefix = i_strdup(DEFAULT_PREFIX);
  } else {
    ctx->render_ctx.keyword_prefix = i_strdup(tmp);
  }

  const char *feature = hash_table_lookup(config->config, (const char *)"feature");
  ctx->enabled = TRUE;
  if (feature != NULL) {
    tmp = mail_user_plugin_getenv(user, feature);
    ctx->enabled = (tmp != NULL && strcasecmp(tmp, "on") == 0);
  }

  const char *events = hash_table_lookup(config->config, (const char *)"events");
  if (events == NULL) {
    events = DEFAULT_EVENTS;
  }
  if (ctx->events != NULL) {
    p_strsplit_free(pool, ctx->events);
  }
  ctx->events = p_strsplit(pool, events, ",");

  ctx->userdb_json = NULL;
  const char *userdb_fields_string = hash_table_lookup(config->config, (const char *)"userdb");
  if (userdb_fields_string != NULL) {
    char **userdb_fields = p_strsplit(pool, userdb_fields_string, ",");

    string_t *userdb_json = str_new(ctx->pool, 1024);
    char *const *userdb_field;
    int i = 0;
    str_append(userdb_json, "\"userdb\":{");
    for (userdb_field = userdb_fields; *userdb_field != NULL; userdb_field++) {
      const char *value = mail_user_plugin_getenv(user, *userdb_field);
      if (value != NULL) {
        if (i++)
          str_append(userdb_json, ",\"");
        else
          str_append(userdb_json, "\"");
        json_append_escaped(userdb_json, *userdb_field);
        str_append(userdb_json, "\":\"");
        json_append_escaped(userdb_json, value);
        str_append(userdb_json, "\"");
      }
    }
    str_append(userdb_json, "},");

    if (i > 0) {
      ctx->userdb_json = i_strdup(str_c(userdb_json));
    }

    p_strsplit_free(ctx->pool, userdb_fields);
    str_free(&userdb_json);
  }

  tmp = hash_table_lookup(config->config, (const char *)"send_flags");
  ctx->render_ctx.send_flags = (tmp == NULL || strcasecmp(tmp, "on") == 0);

  /* Initialize global Kafka context. */

  if (kafka_global == NULL) {
    kafka_global = init_kafka_global();
    kafka_global->rk = NULL;
    kafka_global->refcount = 0;

    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_brokers");
    if (tmp == NULL) {
      kafka_global->brokers = i_strdup(DEFAULT_SERVERS);
    } else {
      kafka_global->brokers = i_strdup(tmp);
    }

    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_debug");
    if (tmp == NULL) {
      kafka_global->debug = i_strdup(DEFAULT_DEBUG);
    } else {
      kafka_global->debug = i_strdup(tmp);
    }

    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_max_retries");
    if (tmp == NULL) {
      kafka_global->max_retries = 0;
    } else {
      if (str_to_int(tmp, &kafka_global->max_retries) < 0 || kafka_global->max_retries < 0) {
        i_error("%sinit - kafka_max_retries must be positive", LOG_LABEL);
        kafka_global->max_retries = 0;
      }
    }

    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_retry_poll_time_in_ms");
    if (tmp == NULL) {
      kafka_global->retry_poll_time_in_ms = 500;
    } else {
      if (str_to_int(tmp, &kafka_global->retry_poll_time_in_ms) < 0 || kafka_global->retry_poll_time_in_ms < 0) {
        i_error("%sinit - kafka_retry_poll_time_in_ms must be positive", LOG_LABEL);
        kafka_global->retry_poll_time_in_ms = 500;
      }
    }

    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_flush_time_in_ms");
    if (tmp == NULL) {
      kafka_global->flush_time_in_ms = 1000;
    } else {
      if (str_to_int(tmp, &kafka_global->flush_time_in_ms) < 0 || kafka_global->flush_time_in_ms < 0) {
        i_error("%sinit - kafka_flush_time_in_ms must be positive", LOG_LABEL);
        kafka_global->flush_time_in_ms = 1000;
      }
    }
    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_topic_close_time_in_ms");
    if (tmp == NULL) {
      kafka_global->topic_close_time_in_ms = 1000;
    } else {
      if (str_to_int(tmp, &kafka_global->topic_close_time_in_ms) < 0 || kafka_global->topic_close_time_in_ms < 0) {
        i_error("%sinit - topic_close_time_in_ms must be positive", LOG_LABEL);
        kafka_global->topic_close_time_in_ms = 1000;
      }
    }

    tmp = mail_user_plugin_getenv(user, "kafka.notification.kafka_destroy_time_in_ms");
    if (tmp == NULL) {
      kafka_global->destroy_time_in_ms = 1000;
    } else {
      if (str_to_int(tmp, &kafka_global->destroy_time_in_ms) < 0 || kafka_global->destroy_time_in_ms < 0) {
        i_error("%sinit - kafka_destroy_time_in_ms must be positive", LOG_LABEL);
        kafka_global->destroy_time_in_ms = 1000;
      }
    }
    kafka_global->user = user;
  }

  ++kafka_global->refcount;
  *context = ctx;

  push_notification_driver_debug(LOG_LABEL, user,
                                 "init - topic=%s, brokers=%s, keyword-prefix=%s, send_flags=%d, "
                                 "enabled=%d, events=[%s], userdb=[%s]",
                                 ctx->topic, kafka_global->brokers, ctx->render_ctx.keyword_prefix,
                                 ctx->render_ctx.send_flags, ctx->enabled, events, userdb_fields_string);

  return 0;
}

static bool push_notification_driver_kafka_begin_txn(struct push_notification_driver_txn *dtxn) {
  struct push_notification_driver_kafka_context *ctx =
      (struct push_notification_driver_kafka_context *)dtxn->duser->context;
  struct mail_user *user = dtxn->ptxn->muser;

  mail_user_ref(user);

  if (ctx->enabled) {
    /* if enabled, subscribe configured events. */

    push_notification_driver_debug(LOG_LABEL, user, "begin_txn - user=%s", user->username);

    char *const *event;
    for (event = ctx->events; *event != NULL; event++) {
      if (strcmp(*event, push_notification_event_messagenew.name) == 0) {
        struct push_notification_event_messagenew_config *config_ =
            p_new(dtxn->ptxn->pool, struct push_notification_event_messagenew_config, 1);
        config_->flags = PUSH_NOTIFICATION_MESSAGE_HDR_FROM | PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT |
                         PUSH_NOTIFICATION_MESSAGE_HDR_TO | PUSH_NOTIFICATION_MESSAGE_HDR_DATE |
                         PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET;
        push_notification_event_init(dtxn, *event, (void *)config_);
      } else if (strcmp(*event, push_notification_event_messageappend.name) == 0) {
        struct push_notification_event_messageappend_config *config_ =
            p_new(dtxn->ptxn->pool, struct push_notification_event_messageappend_config, 1);
        config_->flags = PUSH_NOTIFICATION_MESSAGE_HDR_FROM | PUSH_NOTIFICATION_MESSAGE_HDR_SUBJECT |
                         PUSH_NOTIFICATION_MESSAGE_HDR_TO | PUSH_NOTIFICATION_MESSAGE_HDR_DATE |
                         PUSH_NOTIFICATION_MESSAGE_BODY_SNIPPET;
        push_notification_event_init(dtxn, *event, (void *)config_);
      } else {
        push_notification_event_init(dtxn, *event, NULL);
      }
    }

    return TRUE;
  }

  push_notification_driver_debug(LOG_LABEL, user, "begin_txn - user=%s, skipped because disabled", user->username);
  return FALSE;
}

static void push_notification_driver_kafka_process_mbox(struct push_notification_driver_txn *dtxn,
                                                        struct push_notification_txn_mbox *mbox) {
  struct push_notification_driver_kafka_context *ctx =
      (struct push_notification_driver_kafka_context *)dtxn->duser->context;
  struct mail_user *user = dtxn->ptxn->muser;
  struct push_notification_txn_event *const *event;

  if (array_is_created(&mbox->eventdata)) {
    push_notification_driver_debug(LOG_LABEL, user, "process_mbox - user=%s, mailbox=%s", user->username,
                                   mbox->mailbox);
    array_foreach(&mbox->eventdata, event) {
      const char *event_name = (*event)->event->event->name;

      push_notification_driver_debug(LOG_LABEL, user, "process_mbox - user=%s, mailbox=%s, event=%s", user->username,
                                     mbox->mailbox, event_name);

      string_t *str = push_notification_driver_kafka_render_mbox(dtxn, &ctx->render_ctx, mbox, event);

      if (str != NULL) {
        push_notification_driver_debug(LOG_LABEL, user, "process_mbox - sending notification to Kafka: %s", str_c(str));
        push_notification_driver_kafka_send_to_kafka(ctx, str, user->username);
        str_free(&str);
      }
    }
  } else {
    push_notification_driver_debug(LOG_LABEL, user, "process_mbox - user=%s, mailbox=%s, no eventdata", user->username,
                                   mbox->mailbox);
  }
}

static void push_notification_driver_kafka_process_msg(struct push_notification_driver_txn *dtxn,
                                                       struct push_notification_txn_msg *msg) {
  struct push_notification_driver_kafka_context *ctx =
      (struct push_notification_driver_kafka_context *)dtxn->duser->context;
  struct mail_user *user = dtxn->ptxn->muser;

  if (array_is_created(&msg->eventdata)) {
    struct push_notification_txn_event *const *event;
    push_notification_driver_debug(LOG_LABEL, user, "process_msg - user=%s, mailbox=%s, uid=%u", user->username,
                                   msg->mailbox, msg->uid);

    array_foreach(&msg->eventdata, event) {
      const char *event_name = (*event)->event->event->name;
      push_notification_driver_debug(LOG_LABEL, user, "process_msg - user=%s, mailbox=%s, uid=%u, event=%s",
                                     user->username, msg->mailbox, msg->uid, event_name);

      string_t *str = push_notification_driver_kafka_render_msg(dtxn, &ctx->render_ctx, msg, event);

      if (str != NULL) {
        push_notification_driver_debug(LOG_LABEL, user, "process_msg - sending notification to Kafka: %s", str_c(str));
        push_notification_driver_kafka_send_to_kafka(ctx, str, user->username);
        str_free(&str);
      }
    }
    push_notification_driver_debug(LOG_LABEL, user, "process_msg - finished");
  } else {
    push_notification_driver_debug(LOG_LABEL, user, "process_msg - user=%s, mailbox=%s, uid=%u, no eventdata",
                                   user->username, msg->mailbox, msg->uid);
  }
}

static void push_notification_driver_kafka_end_txn(struct push_notification_driver_txn *dtxn,
                                                   bool success ATTR_UNUSED) {
  struct mail_user *user = dtxn->ptxn->muser;

  push_notification_driver_debug(LOG_LABEL, user, "end_txn - user=%s", user->username);

  mail_user_unref(&user);
}

static void push_notification_driver_kafka_deinit(struct push_notification_driver_user *duser) {
  struct push_notification_driver_kafka_context *ctx = duser->context;

#ifdef DEBUG
  i_debug("%sdeinit: called", LOG_LABEL);
#endif

  push_notification_driver_kafka_deinit_topic(ctx);

#ifdef DEBUG
  i_debug("%sdeinit: free memory", LOG_LABEL);
#endif

  if (kafka_global != NULL) {
    i_assert(kafka_global->refcount > 0);
    --kafka_global->refcount;
  }

  i_free(ctx->topic);
  i_free(ctx->userdb_json);

  i_free(ctx->render_ctx.keyword_prefix);

  if (ctx->events != NULL) {
    p_strsplit_free(ctx->pool, ctx->events);
    ctx->events = NULL;
  }
}

static void push_notification_driver_kafka_cleanup(void) {
#ifdef DEBUG
  i_debug("%scleanup: called", LOG_LABEL);
#endif

  if ((kafka_global != NULL) && (kafka_global->refcount <= 0)) {
    push_notification_driver_kafka_deinit_global();

    i_free(kafka_global->brokers);
    i_free(kafka_global->debug);
    i_free_and_null(kafka_global);
  }
}

/* Driver definition */

extern struct push_notification_driver push_notification_driver_kafka;
struct push_notification_driver push_notification_driver_kafka = {
    .name = "kafka",
    .v = {.init = push_notification_driver_kafka_init,                  //
          .begin_txn = push_notification_driver_kafka_begin_txn,        //
          .process_mbox = push_notification_driver_kafka_process_mbox,  //
          .process_msg = push_notification_driver_kafka_process_msg,    //
          .end_txn = push_notification_driver_kafka_end_txn,            //
          .deinit = push_notification_driver_kafka_deinit,              //
          .cleanup = push_notification_driver_kafka_cleanup}            //
};
