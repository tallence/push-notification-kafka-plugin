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
static char* brokers = "kafka:9092";

static void listen_kafka(const char* topic, int num_msg) {
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
    test_assert(FALSE);
    return;
  }

  /* Consumer groups always use broker based offset storage */
  if (rd_kafka_topic_conf_set(topic_conf, "offset.store.method", "broker", errstr, sizeof(errstr)) !=
      RD_KAFKA_CONF_OK) {
    fprintf(stderr, "%% %s\n", errstr);
    test_assert(FALSE);
    return;
  }

  /* Set default topic config for pattern-matched topics. */
  rd_kafka_conf_set_default_topic_conf(conf, topic_conf);

  rd_kafka_topic_partition_list_t* topics;
  /* Create Kafka handle */
  if (!(rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr)))) {
    fprintf(stderr, "%% Failed to create new consumer: %s\n", errstr);
    test_assert(FALSE);
    return;
  }

  /* Add brokers */
  if (rd_kafka_brokers_add(rk, brokers) == 0) {
    fprintf(stderr, "%% No valid brokers specified\n");
    test_assert(FALSE);
    return;
  }

  /* Redirect rd_kafka_poll() to consumer_poll() */
  rd_kafka_poll_set_consumer(rk);
  topics = rd_kafka_topic_partition_list_new(2);

  rd_kafka_topic_partition_t* t = rd_kafka_topic_partition_list_add(topics, topic, 0);
  t->offset = 0;

  fprintf(stderr, "%% Assigning %d partitions\n", topics->cnt);

  if ((err = rd_kafka_assign(rk, topics))) {
    fprintf(stderr, "%% Failed to assign partitions: %s\n", rd_kafka_err2str(err));
    test_assert(FALSE);
  }
  int i = 0;
  int msg_count = 0;
  while (TRUE) {
    rd_kafka_message_t* rkmessage;
    rkmessage = rd_kafka_consumer_poll(rk, 10);
    if (rkmessage) {
      if (rkmessage->key_len) {
        printf("Key: %.*s\n", (int)rkmessage->key_len, (char*)rkmessage->key);
      }
      printf("%.*s\n", (int)rkmessage->len, (char*)rkmessage->payload);
      rd_kafka_message_destroy(rkmessage);
      msg_count++;
    }

    i++;
    if (i > num_msg + 10) {
      break;
    }
  }

  err = rd_kafka_consumer_close(rk);
  rd_kafka_topic_partition_list_destroy(topics);
  /* Destroy handle */
  rd_kafka_destroy(rk);
  fprintf(stdout, "found messages in queue = %d , expected %d\n", msg_count, num_msg);
  test_assert(num_msg == msg_count);
}

static void test_init_driver(void) {
  test_begin("test_init_driver");

  struct push_notification_driver_config config;
  struct mail_user user;
  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  struct push_notification_driver_kafka_context context;
  char* topic = "test08";
  int num_messages = 10;
  context.topic = topic;  // will be set to 0 by deinit
  context.rkt = NULL;
  const char error[255];

  kafka_global = init_kafka_global();
  kafka_global->rk = NULL;
  kafka_global->flush_time_in_ms = 100;
  kafka_global->brokers = brokers;
  kafka_global->topic_close_time_in_ms = -1;  // wait for all callbacks to finish
  push_notification_driver_kafka_init_global();

  push_notification_driver_kafka_init_topic(&context);

  char* username = "abcdf";
  string_t* test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxRename\",\"oldMailbox\":\"OLD_BOX\"}");
  for (int i = 0; i < num_messages; i++) {
    push_notification_driver_kafka_send_to_kafka(&context, test, username);
  }

  push_notification_driver_kafka_deinit_topic(&context);
  push_notification_driver_kafka_deinit_global();
  i_free(kafka_global);
  pool_unref(&pool);

  listen_kafka(topic, num_messages);
  test_end();
}

int main(void) {
  static void (*test_functions[])(void) = {test_init_driver, NULL};
  return test_run(test_functions);
}
