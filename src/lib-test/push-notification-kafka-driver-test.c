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
#include <unistd.h>
#include "lib.h"
#include "str.h"
#include "push-notification-kafka-event.h"
#include "push-notification-kafka-driver.h"
#include "test-common.h"
#include <syslog.h>
static char* brokers = "kafka:9092";

static int listen_kafka(const char* topic, int num_msg) {
  char errstr[512];
  rd_kafka_conf_t* conf;
  rd_kafka_topic_conf_t* topic_conf;
  rd_kafka_t* rk; /* Producer instance handle */
  rd_kafka_resp_err_t err;
  /* Kafka configuration */

  conf = rd_kafka_conf_new();
  /* Topic configuration */
  topic_conf = rd_kafka_topic_conf_new();

  /* Consumer groups require a group id */

  char* group = "rdkafka_consumer_example";
  if (rd_kafka_conf_set(conf, "group.id", group, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%% %s\n", errstr);
    return -1;
  }

  /* Consumer groups always use broker based offset storage */
  if (rd_kafka_topic_conf_set(topic_conf, "offset.store.method", "broker", errstr, sizeof(errstr)) !=
      RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%% %s\n", errstr);
    return -1;
  }

  /* Set default topic config for pattern-matched topics. */
  rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

  rd_kafka_topic_partition_list_t* topics;
  /* Create Kafka handle */
  if (!(rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr)))) {
    fprintf(stderr, "%% Failed to create new consumer: %s\n", errstr);
    return -1;
  }
  /* set log level to err (default is LOG_DEBUG)*/
  rd_kafka_set_log_level(rk, LOG_ERR);
  /* Add brokers */
  if (rd_kafka_brokers_add(rk, brokers) == 0) {
    fprintf(stderr, "%% No valid brokers specified\n");
    return -1;
  }

  /* Redirect rd_kafka_poll() to consumer_poll() */
  rd_kafka_poll_set_consumer(rk);
  topics = rd_kafka_topic_partition_list_new(2);

  rd_kafka_topic_partition_t* t = rd_kafka_topic_partition_list_add(topics, topic, 0);
  t->offset = 0;  // set queue offset for this client to start position!

  fprintf(stderr, "%% Assigning %d partitions\n", topics->cnt);

  if ((err = rd_kafka_assign(rk, topics))) {
    fprintf(stderr, "%% Failed to assign partitions: %s\n", rd_kafka_err2str(err));
    test_assert(FALSE);
  }
  int i = 0;
  int msg_count = 0;
  while (TRUE) {
    rd_kafka_message_t* rkmessage;
    rkmessage = rd_kafka_consumer_poll(rk, 1000);
    if (rkmessage) {
      if (rkmessage->key_len) {
        printf("Key: %.*s\n", (int)rkmessage->key_len, (char*)rkmessage->key);
        printf("%.*s\n", (int)rkmessage->len, (char*)rkmessage->payload);
        msg_count++;
      }
      rd_kafka_message_destroy(rkmessage);
    }

    // avoid rd_kafka_consumer_poll for ever!
    if (i >= num_msg + 30) {
      break;
    }
    i++;
  }

  err = rd_kafka_consumer_close(rk);
  rd_kafka_topic_partition_list_destroy(topics);
  /* Destroy handle */
  rd_kafka_destroy(rk);
  fprintf(stdout, "found messages in queue = %d , expected %d\n", msg_count, num_msg);
  test_assert(num_msg == msg_count);
  return 0;
}

static void test_init_send_deinit_driver(void) {
  test_begin("test_init_send_deinit_driver");

  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  struct push_notification_driver_kafka_context context;
  char* topic = "test08";
  int num_messages = 10;
  context.topic = topic;  // will be set to 0 by deinit
  context.rkt = NULL;
  context.userdb_json = NULL;

  kafka_global = init_kafka_global();
  kafka_global->rk = NULL;
  kafka_global->flush_time_in_ms = 1000;
  kafka_global->brokers = brokers;
  kafka_global->topic_close_time_in_ms = 10000;  // wait for all callbacks to finish in ms
  test_assert(push_notification_driver_kafka_init_global() != NULL);

  push_notification_driver_kafka_init_topic(&context);
  test_assert(context.rkt != NULL);

  char* username = "abcdf";
  string_t* test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxRename\",\"oldMailbox\":\"OLD_BOX\"}");
  for (int i = 0; i < num_messages; i++) {
    push_notification_driver_kafka_send_to_kafka(&context, test, username);
  }

  push_notification_driver_kafka_deinit_topic(&context);
  test_assert(context.rkt == NULL);
  push_notification_driver_kafka_deinit_global();
  test_assert(kafka_global->rk == NULL);
  i_free(kafka_global);
  pool_unref(&pool);

  int ret = listen_kafka(topic, 10);
  test_assert(ret == 0);
  test_end();
}

static bool err_cb_called = FALSE;
static void test_push_notification_driver_kafka_err_cb(rd_kafka_t* rk, int err, const char* reason,
                                                       void* opaque ATTR_UNUSED) {
  i_info("%serr_cb_test: %s: %s: %s", LOG_LABEL, rd_kafka_name(rk), rd_kafka_err2str(err), reason);
  err_cb_called = TRUE;
}

static void test_init_kafka_not_reachable(void) {
  test_begin("test_init_kafka_not_reachable");

  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  struct push_notification_driver_kafka_context context;
  char* topic = "test09";
  context.topic = topic;  // will be set to 0 by deinit
  context.rkt = NULL;
  context.userdb_json = NULL;

  kafka_global = init_kafka_global();
  kafka_global->rk = NULL;
  kafka_global->flush_time_in_ms = 1000;
  kafka_global->brokers = "kafka_2:9092";       // invalid port! kafka not available.
  kafka_global->topic_close_time_in_ms = 2000;  // wait for all callbacks to finish in ms
  kafka_global->error_cb = test_push_notification_driver_kafka_err_cb;

  test_assert(push_notification_driver_kafka_init_global() != NULL);
  test_assert(kafka_global->rk != NULL);
  test_assert(kafka_global->rkc != NULL);
  push_notification_driver_kafka_init_topic(&context);

  // deinit will call rd_kafka_poll to wait for all callbacks,
  push_notification_driver_kafka_deinit_topic(&context);
  rd_kafka_destroy(kafka_global->rk);
  i_free(kafka_global);
  pool_unref(&pool);
  test_assert(err_cb_called);
  test_end();
}

int main(void) {
  static void (*test_functions[])(void) = {test_init_send_deinit_driver, test_init_kafka_not_reachable, NULL};
  return test_run(test_functions);
}
