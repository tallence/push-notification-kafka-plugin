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
#include "lib.h"
#include "str.h"
#include "push-notification-kafka-event.h"
#include "push-notification-kafka-driver.h"
#include "test-common.h"

static void test_init_driver(void) {
  test_begin("test_init_driver");

  struct push_notification_driver_config config;
  struct mail_user user;
  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  struct push_notification_driver_kafka_context context;
  context.topic = "test";
  context.rkt = NULL;
  const char error[255];

  kafka_global = init_kafka_global();
  kafka_global->rk = NULL;
  kafka_global->flush_time_in_ms = 100;
  kafka_global->brokers = "kafka:9092";
  push_notification_driver_kafka_init_global();

  push_notification_driver_kafka_init_topic(&context);
  char* username = "abcdf";
  string_t* test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxRename\",\"oldMailbox\":\"OLD_BOX\"}");
  for (int i = 0; i < 10; i++) {
    push_notification_driver_kafka_send_to_kafka(&context, test, username);
  }
  push_notification_driver_kafka_deinit_topic(&context);
  push_notification_driver_kafka_deinit_global();
  i_free(kafka_global);
  pool_unref(&pool);
  test_assert(1 == 1);
  test_end();
}
int main(void) {
  static void (*test_functions[])(void) = {test_init_driver, NULL};
  return test_run(test_functions);
}
