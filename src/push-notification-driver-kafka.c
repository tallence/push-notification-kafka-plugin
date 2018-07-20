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

#include <librdkafka/rdkafka.h>

#include "lib.h"
#include "str.h"
#include "array.h"
#include "hash.h"
#include "json-parser.h"

#include "push-notification-drivers.h"
#include "push-notification-events.h"
#include "push-notification-txn-mbox.h"
#include "push-notification-txn-msg.h"
#include "push-notification-event-mailboxcreate.h"
#include "push-notification-event-mailboxrename.h"
#include "push-notification-event-flagsset.h"
#include "push-notification-event-flagsclear.h"
#include "push-notification-event-messageexpunge.h"

extern struct push_notification_event push_notification_event_mailboxcreate;
extern struct push_notification_event push_notification_event_mailboxrename;
extern struct push_notification_event push_notification_event_flagsclear;
extern struct push_notification_event push_notification_event_flagsset;

#define LOG_LABEL "Kafka Push Notification: "

#define DEFAULT_TOPIC "dovecot"
#define DEFAULT_SERVERS "localhost"
#define DEFAULT_PREFIX "$"
#define DEFAULT_EVENTS "FlagsClear,FlagsSet,MailboxCreate,MailboxDelete,MailboxRename,MailboxSubscribe,MailboxUnsubscribe,MessageAppend,MessageExpunge,MessageNew,MessageRead,MessageTrash"
#define DEFAULT_FEATURE_NAME "push_notification_kafka"
#define DEFAULT_DEBUG ""

/* This is data that is shared by all plugin users. */
struct push_notification_driver_kafka_global
{
    int refcount;

    char *brokers;
    char *debug;

    /* if send message to topic fails, make a some retries */
    int max_retries;
    int retry_poll_time_in_ms;

    /* shutdown timeouts */
    int flush_time_in_ms;
    int destroy_time_in_ms;

    rd_kafka_conf_t *rkc; /* Kafka configuration object */
    rd_kafka_t *rk; /* Producer instance handle */
};
static struct push_notification_driver_kafka_global *kafka_global = NULL;

struct push_notification_driver_kafka_context
{
    pool_t pool;

    char *topic;
    bool send_flags;
    char *keyword_prefix;
    char **events;
    bool enabled;

    rd_kafka_topic_t* rkt;
};

static bool
str_starts_with (const char * str, const char * prefix);

/* Kafka stuff */

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
void
push_notification_driver_kafka_msg_cb (rd_kafka_t *rk ATTR_UNUSED, const rd_kafka_message_t *rkmessage, void *opaque ATTR_UNUSED) {
    i_debug ("%smsg_cb: called", LOG_LABEL);

    if (rkmessage->err) {
        i_error ("%smsg_cb: message delivery failed: %s", LOG_LABEL, rd_kafka_err2str (rkmessage->err));}
    else {
        i_debug ("%smsg_cb: message delivered (%zd bytes, partition %"PRId32")", LOG_LABEL, rkmessage->len, rkmessage->partition);
    }

    if (rkmessage->err)
    fprintf(stderr, "%% Message delivery failed: %s\n",
                    rd_kafka_err2str(rkmessage->err));
    else
    fprintf(stderr,
                    "%% Message delivered (%zd bytes, "
                    "partition %"PRId32")\n",
                    rkmessage->len, rkmessage->partition);

    /* The rkmessage is destroyed automatically by librdkafka */
}

void
push_notification_driver_kafka_err_cb (rd_kafka_t *rk, int err, const char *reason, void *opaque) {
    i_error ("%serr_cb: %s: %s: %s", LOG_LABEL, rd_kafka_name (rk), rd_kafka_err2str (err), reason);
}

static rd_kafka_t *
push_notification_driver_kafka_init_global () {
    if (kafka_global->rk == NULL) {
        char errstr[512]; /* librdkafka API error reporting buffer */

        i_debug ("%sinit_global - initialize brokers=%s", LOG_LABEL, kafka_global->brokers);
        /*
         * Create Kafka client configuration place-holder
         */
        kafka_global->rkc = rd_kafka_conf_new ();
        rd_kafka_conf_set_error_cb (kafka_global->rkc, push_notification_driver_kafka_err_cb);
        rd_kafka_conf_set_dr_msg_cb (kafka_global->rkc, push_notification_driver_kafka_msg_cb);

        /* Set bootstrap broker(s) as a comma-separated list of
         * host or host:port (default port 9092).
         * librdkafka will use the bootstrap brokers to acquire the full
         * set of brokers from the cluster. */
        if (rd_kafka_conf_set (kafka_global->rkc, "bootstrap.servers", kafka_global->brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            i_error ("%sinit_global - rd_kafka_conf_set(bootstrap.servers) failed with %s", LOG_LABEL, errstr);
            return NULL;
        }
        if (rd_kafka_conf_set (kafka_global->rkc, "debug", "all", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            i_error ("%sinit_global - rd_kafka_conf_set(debug) failed with %s", LOG_LABEL, errstr);
            return NULL;
        }

        /*
         * Create producer instance.
         *
         * NOTE: rd_kafka_new() takes ownership of the conf object
         *       and the application must not reference it again after
         *       this call.
         */
        kafka_global->rk = rd_kafka_new (RD_KAFKA_PRODUCER, kafka_global->rkc, errstr, sizeof(errstr));
        if (kafka_global->rk == NULL) {
            i_error ("%sinit_global - rd_kafka_new() failed to create new producer with %s", LOG_LABEL, errstr);

            rd_kafka_conf_destroy (kafka_global->rkc);
            kafka_global->rkc = NULL;
        }
    }

    return kafka_global->rk;
}

static void
push_notification_driver_kafka_deinit_global () {
    if (kafka_global->rk != NULL) {
        /* Shutdown Kafka */

        /* Wait for final messages to be delivered or fail.
         * rd_kafka_flush() is an abstraction over rd_kafka_poll() which
         * waits for all messages to be delivered. */
        i_debug ("%sdeinit_global - flushing Kafka messages...", LOG_LABEL);
        rd_kafka_flush (kafka_global->rk, kafka_global->flush_time_in_ms);

        i_debug ("%sdeinit_global - rd_kafka_destroy...", LOG_LABEL);
        /* Destroy the Kafka producer instance */
        rd_kafka_destroy (kafka_global->rk);

        rd_kafka_wait_destroyed (kafka_global->destroy_time_in_ms);

        kafka_global->rk = NULL;
    }
}

static void
push_notification_driver_kafka_init_topic (struct push_notification_driver_kafka_context *ctx) {
    if (push_notification_driver_kafka_init_global () != NULL) {
        i_debug ("%sinit_topic - initialize topic=%s", LOG_LABEL, ctx->topic);
        if (ctx->rkt == NULL) {

            /* Create topic object that will be reused for each message
             * produced.
             *
             * Both the producer instance (rd_kafka_t) and topic objects (topic_t)
             * are long-lived objects that should be reused as much as possible.
             */
            ctx->rkt = rd_kafka_topic_new (kafka_global->rk, ctx->topic, NULL);
            if (ctx->rkt == NULL) {
                i_error ("%sinit_topic - rd_kafka_topic_new() failed to create topic %s object with %s", LOG_LABEL, ctx->topic, rd_kafka_err2str (rd_kafka_last_error ()));
            }
        }
    } else {
        i_error ("%sinit_topic - Kafka not initialized", LOG_LABEL);
    }
}

static void
push_notification_driver_kafka_deinit_topic (struct push_notification_driver_kafka_context *ctx) {
    if (kafka_global->rk != NULL) {
        rd_kafka_poll (kafka_global->rk, 0/*non-blocking*/);
    }

    if (ctx->rkt != NULL) {
        i_debug ("%sdeinit_topic: destroy Kafka topic object '%s'", LOG_LABEL, ctx->topic);
        rd_kafka_topic_destroy (ctx->rkt);
        ctx->rkt = NULL;
    }
}

static void
push_notification_driver_kafka_send_to_kafka (struct push_notification_driver_kafka_context* ctx, string_t* str) {
    push_notification_driver_kafka_init_topic (ctx);

    i_assert (str != NULL);

    if (ctx->rkt != NULL) {
        int retry_counter = 0;

        retry: if (rd_kafka_produce (ctx->rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, (void *) str_c (str), str_len (str), NULL, 0, NULL) == -1) {
            /* Failed to *enqueue* message for producing. */
            i_error ("%ssend_to_kafka - failed to produce to topic=%s: %s", LOG_LABEL, ctx->topic, rd_kafka_err2str (rd_kafka_last_error ()));

            /* Poll to handle delivery reports */
            if (rd_kafka_last_error () == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
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
                rd_kafka_poll (kafka_global->rk, kafka_global->retry_poll_time_in_ms);
                if (retry_counter++ < kafka_global->max_retries) {
                    goto retry;
                }
            }
        }

        i_debug ("%ssend_to_kafka - send %zu bytes to topic=%s", LOG_LABEL, str_len (str), ctx->topic);

        /* Keep Kafka happy. */
        rd_kafka_poll (kafka_global->rk, 0/*non-blocking*/);
    } else {
        i_error ("%ssend_to_kafka - topic=%s not initialized", LOG_LABEL, ctx->topic);
    }
}

/* The push notification driver itself */

static int
push_notification_driver_kafka_init (struct push_notification_driver_config *config, struct mail_user *user, pool_t pool, void **context, const char **error_r ATTR_UNUSED) {
    const char *tmp;
    struct push_notification_driver_kafka_context *ctx = p_new(pool, struct push_notification_driver_kafka_context, 1);

    ctx->pool = pool;

    /* Process driver configuration. */

    tmp = hash_table_lookup(config->config, (const char * ) "topic");
    if (tmp == NULL) {
        ctx->topic = i_strdup (DEFAULT_TOPIC);
    } else {
        ctx->topic = i_strdup (tmp);
    }

    tmp = hash_table_lookup(config->config, (const char * ) "keyword_prefix");
    if (tmp == NULL) {
        ctx->keyword_prefix = i_strdup (DEFAULT_PREFIX);
    } else {
        ctx->keyword_prefix = i_strdup (tmp);
    }

    const char *feature = hash_table_lookup(config->config, (const char * ) "feature");
    if (tmp == NULL) {
        feature = DEFAULT_FEATURE_NAME;
    }
    tmp = mail_user_plugin_getenv (user, feature);
    ctx->enabled = (tmp != NULL && strcasecmp (tmp, "on") == 0);

    const char *events = hash_table_lookup(config->config, (const char * ) "events");
    if (events == NULL) {
        events = DEFAULT_EVENTS;
    }
    if (ctx->events != NULL) {
        p_strsplit_free (pool, ctx->events);
    }
    ctx->events = p_strsplit (pool, events, ",");

    tmp = hash_table_lookup(config->config, (const char * ) "send_flags");
    ctx->send_flags = (tmp == NULL || strcasecmp (tmp, "on") == 0);

    /* Initialize global Kafka context. */

    if (kafka_global == NULL) {
        kafka_global = i_new(struct push_notification_driver_kafka_global, 1);
        kafka_global->rk = NULL;
        kafka_global->refcount = 0;

        tmp = mail_user_plugin_getenv (user, "kafka_brokers");
        if (tmp == NULL) {
            kafka_global->brokers = i_strdup (DEFAULT_SERVERS);
        } else {
            kafka_global->brokers = i_strdup (tmp);
        }

        tmp = mail_user_plugin_getenv (user, "kafka_debug");
        if (tmp == NULL) {
            kafka_global->debug = i_strdup (DEFAULT_DEBUG);
        } else {
            kafka_global->debug = i_strdup (tmp);
        }

        tmp = mail_user_plugin_getenv(user, "kafka_max_retries");
        if (tmp == NULL) {
            kafka_global->max_retries = 0;
        } else {if (str_to_uint(tmp, &kafka_global->max_retries) < 0 ||
                            kafka_global->max_retries < 0 ) {
                i_error("%sinit - max_retries Level must be positive");
                kafka_global->max_retries = 0;
            }
        }

        kafka_global->max_retries = 0;
        kafka_global->retry_poll_time_in_ms = 500;
        kafka_global->flush_time_in_ms = 1000;
        kafka_global->destroy_time_in_ms = 1000;
    }

    ++kafka_global->refcount;
    *context = ctx;

    push_notification_driver_debug (LOG_LABEL, user, "init - topic=%s, brokers=%s, keyword-prefix=%s, send_flags=%d, enabled=%d, events=[%s]", ctx->topic, kafka_global->brokers, ctx->keyword_prefix, ctx->send_flags, ctx->enabled, events);

    return 0;
}

static bool
push_notification_driver_kafka_begin_txn (struct push_notification_driver_txn *dtxn) {
    struct push_notification_driver_kafka_context *ctx = (struct push_notification_driver_kafka_context *) dtxn->duser->context;
    struct mail_user *user = dtxn->ptxn->muser;

    if (ctx->enabled) {
        /* if enabled, subscribe configured events. */

        push_notification_driver_debug (LOG_LABEL, user, "begin_txn - user=%s", user->username);

        char * const *event;
        for (event = ctx->events; *event != NULL; event++) {
            push_notification_event_init (dtxn, *event, NULL);
        }

        return TRUE;
    }

    push_notification_driver_debug (LOG_LABEL, user, "begin_txn - user=%s, skipped because disabled", user->username);
    return FALSE;
}

static void
push_notification_driver_kafka_process_mbox (struct push_notification_driver_txn *dtxn, struct push_notification_txn_mbox *mbox) {
    struct push_notification_driver_kafka_context *ctx = (struct push_notification_driver_kafka_context *) dtxn->duser->context;
    struct mail_user *user = dtxn->ptxn->muser;
    struct push_notification_txn_event * const *event;

    if (array_is_created (&mbox->eventdata)) {
        push_notification_driver_debug (LOG_LABEL, user, "process_mbox - user=%s, mailbox=%s", user->username, mbox->mailbox);

        array_foreach (&mbox->eventdata, event)
        {
            const char * event_name = (*event)->event->event->name;

            string_t *str = str_new (dtxn->ptxn->pool, 256);
            str_append (str, "{\"user\":\"");
            json_append_escaped (str, user->username);
            str_append (str, "\",\"mailbox\":\"");
            json_append_escaped (str, mbox->mailbox);
            str_printfa (str, "\",\"event\":\"%s\"", event_name);

            if (strcmp (push_notification_event_mailboxcreate.name, event_name) == 0) {
                struct push_notification_event_mailboxcreate_data *data = (*event)->data;
                str_printfa (str, "\",\"uidvalidity\":%u", data->uid_validity);
            } else if (strcmp (push_notification_event_mailboxrename.name, event_name) == 0) {
                struct push_notification_event_mailboxrename_data *data = (*event)->data;
                str_append (str, "\",\"oldMailbox\":\"");
                json_append_escaped (str, data->old_mbox);
                str_printfa (str, "\"");
            }
            str_append (str, "}");

            push_notification_driver_debug (LOG_LABEL, user, "process_mbox - sending notification to Kafka: %s", str_c (str));

            push_notification_driver_kafka_send_to_kafka (ctx, str);

            str_free (&str);
        }
    } else {
        push_notification_driver_debug (LOG_LABEL, user, "process_mbox - user=%s, mailbox=%s, no eventdata", user->username, mbox->mailbox);
    }
}

static string_t *
build_flags_event (struct push_notification_driver_txn *dtxn, const char *event_name, struct push_notification_txn_msg *msg, enum mail_flags flags, ARRAY_TYPE(keywords) *keywords) {
    struct push_notification_driver_kafka_context *ctx = (struct push_notification_driver_kafka_context *) dtxn->duser->context;
    struct mail_user *user = dtxn->ptxn->muser;
    string_t *str = str_new (dtxn->ptxn->pool, 512);

    str_append (str, "{\"user\":\"");
    json_append_escaped (str, user->username);
    str_append (str, "\",\"mailbox\":\"");
    json_append_escaped (str, msg->mailbox);
    str_printfa (str, "\",\"event\":\"%s\",\"uidvalidity\":%u,\"uid\":%u", event_name, msg->uid_validity, msg->uid);

    bool flag_written = FALSE;
    if (ctx->send_flags) {
        str_append (str, ",\"flags\":[");

        if ((flags & MAIL_ANSWERED) != 0) {
            str_append (str, "\"\\\\Answered\"");
            flag_written = TRUE;
        }
        if ((flags & MAIL_FLAGGED) != 0) {
            if (flag_written) {
                str_append (str, ",");
            }
            str_append (str, "\"\\\\Flagged\"");
            flag_written = TRUE;
        }
        if ((flags & MAIL_DELETED) != 0) {
            if (flag_written) {
                str_append (str, ",");
            }
            str_append (str, "\"\\\\Deleted\"");
            flag_written = TRUE;
        }
        if ((flags & MAIL_SEEN) != 0) {
            if (flag_written) {
                str_append (str, ",");
            }
            str_append (str, "\"\\\\Seen\"");
            flag_written = TRUE;
        }
        if ((flags & MAIL_DRAFT) != 0) {
            if (flag_written) {
                str_append (str, ",");
            }
            str_append (str, "\"\\\\Draft\"");
            flag_written = TRUE;
        }

        str_append (str, "]");
    }

    str_append (str, ",\"keywords\":[");

    bool keyword_written = FALSE;
    const char * const *keyword;
    int i = 0;
    array_foreach(keywords, keyword)
    {
        if (str_starts_with (*keyword, ctx->keyword_prefix)) {
            if (i > 0) {
                str_append (str, ",\"");
            } else {
                str_append (str, "\"");
            }
            json_append_escaped (str, *keyword);
            str_append (str, "\"");
            i++;
            keyword_written |= TRUE;
        }
    }
    str_append (str, "]");

    if (flag_written || keyword_written) {
        // nothing written, send no event
        return str;
    }

    return NULL;
}

static void
push_notification_driver_kafka_process_msg (struct push_notification_driver_txn *dtxn, struct push_notification_txn_msg *msg) {
    struct push_notification_driver_kafka_context *ctx = (struct push_notification_driver_kafka_context *) dtxn->duser->context;
    struct mail_user *user = dtxn->ptxn->muser;

    if (array_is_created (&msg->eventdata)) {
        struct push_notification_txn_event * const *event;
        push_notification_driver_debug (LOG_LABEL, user, "process_msg - user=%s, mailbox=%s, uid=%u", user->username, msg->mailbox, msg->uid);

        array_foreach (&msg->eventdata, event)
        {
            const char * event_name = (*event)->event->event->name;
            push_notification_driver_debug (LOG_LABEL, user, "process_msg - user=%s, mailbox=%s, uid=%u, event=%s", user->username, msg->mailbox, msg->uid, event_name);

            string_t *str = NULL;

            if (strcmp (push_notification_event_flagsset.name, (*event)->event->event->name) == 0) {
                struct push_notification_event_flagsset_data *data = (*event)->data;
                str = build_flags_event (dtxn, event_name, msg, data->flags_set, &data->keywords_set);
            } else if (strcmp (push_notification_event_flagsclear.name, (*event)->event->event->name) == 0) {
                struct push_notification_event_flagsclear_data *data = (*event)->data;
                str = build_flags_event (dtxn, event_name, msg, data->flags_clear, &data->keywords_clear);
            } else {
                str = str_new (dtxn->ptxn->pool, 512);
                str_append (str, "{\"user\":\"");
                json_append_escaped (str, user->username);
                str_append (str, "\",\"mailbox\":\"");
                json_append_escaped (str, msg->mailbox);
                str_printfa (str, "\",\"event\":\"%s\",\"uidvalidity\":%u,\"uid\":%u}", event_name, msg->uid_validity, msg->uid);
            }

            if (str != NULL) {
                push_notification_driver_debug (LOG_LABEL, user, "process_msg - sending notification to Kafka: %s", str_c (str));

                push_notification_driver_kafka_send_to_kafka (ctx, str);

                str_free (&str);
            }
        }
    } else {
        push_notification_driver_debug (LOG_LABEL, user, "process_msg - user=%s, mailbox=%s, uid=%u, no eventdata", user->username, msg->mailbox, msg->uid);
    }
}

static void
push_notification_driver_kafka_deinit (struct push_notification_driver_user *duser ATTR_UNUSED) {
    struct push_notification_driver_kafka_context *ctx = duser->context;

    push_notification_driver_kafka_deinit_topic (ctx);

    if (kafka_global != NULL) {
        i_assert(kafka_global->refcount > 0);
        --kafka_global->refcount;
    }

    i_free(ctx->topic);
    i_free(ctx->keyword_prefix);

    if (ctx->events != NULL) {
        p_strsplit_free (ctx->pool, ctx->events);
        ctx->events = NULL;
    }
}

static void
push_notification_driver_kafka_cleanup (void) {
    if ((kafka_global != NULL) && (kafka_global->refcount <= 0)) {
        push_notification_driver_kafka_deinit_global ();

        i_free (kafka_global->brokers);
        i_free (kafka_global->debug);
        i_free_and_null (kafka_global);
    }
}

/* Utilities */

static bool
str_starts_with (const char * str, const char * prefix) {
    if (str == NULL || prefix == NULL)
        return FALSE;

    for (;; str++, prefix++) {
        if (!*prefix)
            return TRUE;
        else if (*str != *prefix)
            return FALSE;
    }

    return FALSE;
}

/* Driver definition */

extern struct push_notification_driver push_notification_driver_kafka;
struct push_notification_driver push_notification_driver_kafka =
    { .name = "kafka", .v =
        { .init = push_notification_driver_kafka_init, //
            .begin_txn = push_notification_driver_kafka_begin_txn, //
            .process_mbox = push_notification_driver_kafka_process_mbox, //
            .process_msg = push_notification_driver_kafka_process_msg, //
            .deinit = push_notification_driver_kafka_deinit, //
            .cleanup = push_notification_driver_kafka_cleanup } //
    };
