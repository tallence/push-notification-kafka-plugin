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
#include "config.h"
#include "lib.h"
#include "test-common.h"
#include "mail-types.h"
#include "str.h"
#include "array.h"

#include "push-notification-kafka-event.h"
#include "push-notification-drivers.h"
#include "push-notification-txn-mbox.h"
#include "push-notification-txn-msg.h"
#include "push-notification-event-mailboxrename.h"
#include "push-notification-event-mailboxcreate.h"
#include "push-notification-event-messagenew.h"
#include "push-notification-events.h"
#include "push-notification-kafka-driver.h"
#include "push-notification-event-messageappend.h"


static void test_str_starts_with(void) {
  test_begin("unit_test_str_starts_with");
  test_assert(str_starts_with("HALLO WETL", "HAL") == TRUE);
  test_assert(str_starts_with("HALLO WETL", NULL) == FALSE);
  test_assert(str_starts_with(NULL, "HAL") == FALSE);
  test_assert(str_starts_with(NULL, NULL) == FALSE);
  test_end();
}

static void test_push_notification_driver_kafka_render_mbox_rename(void) {
  test_begin("unit_test_push_notification_driver_kafka_render_mbox_rename_msg");

  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  string_t *test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxRename\",\"oldMailbox\":\"OLD_BOX\"}");

  struct push_notification_driver_txn dtxn;
  struct push_notification_txn ptxn;
  struct mail_user user;
  user.username = "testuser";
  ptxn.muser = &user;
  ptxn.pool = pool;
  dtxn.ptxn = &ptxn;

  struct push_notification_txn_mbox mbox;
  mbox.mailbox = "INBOX";

  struct push_notification_txn_event event;
  struct push_notification_event_mailboxrename_data data;
  data.old_mbox = "OLD_BOX";
  event.data = &data;
  struct push_notification_event_config event_cfg;
  const struct push_notification_event notify_event = {.name = push_notification_event_mailboxrename.name};

  event_cfg.event = &notify_event;
  event.event = &event_cfg;
  struct push_notification_txn_event *const msg = &event;
  string_t *t = push_notification_driver_kafka_render_mbox(&dtxn, NULL, &mbox, &msg);
  // i_info("MSG: %s ", str_c(t));
  test_assert(str_equals(t, test));
  pool_unref(&pool);
  test_end();
}
static void test_push_notification_driver_kafka_render_mbox_create(void) {
  test_begin("unit_test_push_notification_driver_kafka_render_mbox_create_msg");

  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  string_t *test = str_new(pool, 256);
  str_append(test, "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxCreate\",\"uidvalidity\":1}");
  struct push_notification_driver_txn dtxn;
  struct push_notification_txn ptxn;
  struct mail_user user;
  user.username = "testuser";
  ptxn.muser = &user;
  ptxn.pool = pool;
  dtxn.ptxn = &ptxn;

  struct push_notification_txn_mbox mbox;
  mbox.mailbox = "INBOX";

  struct push_notification_txn_event event;
  struct push_notification_event_mailboxcreate_data data;
  data.uid_validity = 1;
  event.data = &data;
  struct push_notification_event_config event_cfg;
  const struct push_notification_event notify_event = {.name = push_notification_event_mailboxcreate.name};

  event_cfg.event = &notify_event;
  event.event = &event_cfg;
  struct push_notification_txn_event *const msg = &event;
  string_t *t = push_notification_driver_kafka_render_mbox(&dtxn, NULL, &mbox, &msg);
  // i_info("MSG: %s ", str_c(t));
  test_assert(str_equals(t, test));
  pool_unref(&pool);
  test_end();
}

static void test_write_flags(void) {
  test_begin("unit_test_write_flags");

  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  string_t *str = str_new(pool, 256);
  test_assert(write_flags(MAIL_ANSWERED, str) == TRUE);
  test_assert(strcmp(str_c(str), "\"\\\\Answered\"") == 0);
  str_free(&str);

  str = str_new(pool, 256);
  string_t *test = str_new(pool, 256);
  str_append(test, "\"\\\\Answered\"");
  str_append(test, ",");
  str_append(test, "\"\\\\Flagged\"");
  str_append(test, ",");
  str_append(test, "\"\\\\Deleted\"");
  str_append(test, ",");
  str_append(test, "\"\\\\Seen\"");
  str_append(test, ",");
  str_append(test, "\"\\\\Draft\"");

  enum mail_flags flags = MAIL_ANSWERED;
  flags |= MAIL_FLAGGED;
  flags |= MAIL_DELETED;
  flags |= MAIL_SEEN;
  flags |= MAIL_DRAFT;

  test_assert((flags & MAIL_ANSWERED) != 0);
  test_assert((flags & MAIL_FLAGGED) != 0);
  test_assert((flags & MAIL_DELETED) != 0);
  test_assert((flags & MAIL_SEEN) != 0);
  test_assert((flags & MAIL_DRAFT) != 0);
  test_assert(write_flags(flags, str) == TRUE);
  test_assert(str_equals(str, test));

  str_free(&str);
  str_free(&test);

  pool_unref(&pool);
  test_end();
}
static void write_msg_prefix_test(void) {
  test_begin("unit_test_write_msg_prefix");
  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 1024);
  string_t *test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxCreate\",\"uidvalidity\":2,\"uid\":1");
  struct push_notification_driver_txn dtxn;
  struct push_notification_driver_user duser;
  struct push_notification_txn ptxn;
  struct mail_user user;
  struct push_notification_driver_kafka_context kafka_context;

  user.username = "testuser";
  ptxn.muser = &user;
  ptxn.pool = pool;
  kafka_context.userdb_json = NULL;

  duser.context = &kafka_context;

  dtxn.ptxn = &ptxn;
  dtxn.duser = &duser;
  
  struct push_notification_txn_msg event;
  event.uid = 1;
  event.uid_validity = 2;
  event.mailbox = "INBOX";
#if ! DOVECOT_PREREQ(2, 3)
  event.seq = 1;
#endif

  string_t *t = write_msg_prefix(&dtxn, "MailboxCreate", &event);
  //  i_info("MSG: %s ", str_c(t));
  test_assert(str_equals(t, test));
  pool_unref(&pool);
  test_end();
}
static void write_flags_event_test(void) {
  test_begin("unit_test_write_flags_event");
  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  string_t *test = str_new(pool, 256);
  str_append(
      test,
      "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxCreate\",\"uidvalidity\":2,\"uid\":1,"
      "\"flags\":[\"\\\\Seen\"],\"oldFlags\":[\"\\\\Flagged\"],\"keywords\":[\"k:new_keyword\"],\"oldKeywords\":["
      "\"k:old_keyword\"]}");

  struct push_notification_driver_txn dtxn;
  struct push_notification_driver_user duser;
  struct push_notification_txn ptxn;
  struct mail_user user;
  struct push_notification_driver_kafka_context kafka_context;

  user.username = "testuser";
  ptxn.muser = &user;
  ptxn.pool = pool;
  kafka_context.userdb_json = NULL;

  duser.context = &kafka_context;

  dtxn.ptxn = &ptxn;
  dtxn.duser = &duser;

  struct push_notification_txn_msg event;
  event.uid = 1;
  event.uid_validity = 2;
  event.mailbox = "INBOX";
#if ! DOVECOT_PREREQ(2, 3)
  event.seq = 1;
#endif

  struct push_notification_driver_kafka_render_context ctx;
  ctx.send_flags = TRUE;
  ctx.keyword_prefix = "k";
  ARRAY_TYPE(keywords) keywords;
  t_array_init(&keywords, 1);
  const char *k = "k:new_keyword";
  const char *const *name = &k;
  array_append(&keywords, name, 1);

  enum mail_flags flags = MAIL_SEEN;
  ARRAY_TYPE(keywords) keywords_old;
  t_array_init(&keywords_old, 2);
  const char *k2 = "k:old_keyword";
  const char *const *name2 = &k2;
  array_append(&keywords_old, name2, 1);
  enum mail_flags flags_old = MAIL_FLAGGED;
  string_t *t = write_flags_event(&dtxn, &ctx, "MailboxCreate", &event, flags, &keywords, flags_old, &keywords_old);
  // i_info("MSG: %s ", str_c(t));
  test_assert(str_equals(t, test));

  /****
   * Test JSON string with several keywords
   */
  string_t *test2 = str_new(pool, 256);
  str_append(test2,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxCreate\",\"uidvalidity\":2,\"uid\":1,"
             "\"flags\":[\"\\\\Seen\"],\"oldFlags\":[\"\\\\Flagged\"],\"keywords\":[\"k:new_keyword\",\"k:new_"
             "keyword2\"],\"oldKeywords\":["
             "\"k:old_keyword\",\"k:old_keyword2\"]}");

  const char *k3 = "k:new_keyword2";
  const char *const *name3 = &k3;
  array_append(&keywords, name3, 1);

  const char *k4 = "k:old_keyword2";
  const char *const *name4 = &k4;
  array_append(&keywords_old, name4, 1);
  t = write_flags_event(&dtxn, &ctx, "MailboxCreate", &event, flags, &keywords, flags_old, &keywords_old);
  // i_info("MSG: %s ", str_c(t));
  test_assert(str_equals(t, test2));

  /***
   * Test More then one flag.
   */
  string_t *test3 = str_new(pool, 256);
  str_append(test3,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MailboxCreate\",\"uidvalidity\":2,\"uid\":1,"
             "\"flags\":[\"\\\\Flagged\",\"\\\\Seen\"],\"oldFlags\":[\"\\\\Answered\",\"\\\\Flagged\"],\"keywords\":["
             "\"k:new_keyword\","
             "\"k:new_"
             "keyword2\"],\"oldKeywords\":["
             "\"k:old_keyword\",\"k:old_keyword2\"]}");

  flags |= MAIL_FLAGGED;
  flags_old |= MAIL_ANSWERED;
  t = write_flags_event(&dtxn, &ctx, "MailboxCreate", &event, flags, &keywords, flags_old, &keywords_old);
  // i_info("MSG: %s ", str_c(t));

  test_assert(str_equals(t, test3));

  pool_unref(&pool);
  test_end();
}

static void write_event_messagenew_test(void) {
  test_begin("unit_write_event_messagenew");
  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  string_t *test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MessageNew\",\"uidvalidity\":2,\"uid\":1,"
             "\"date\":\"2018-08-30T14:02:03+00:00\",\"from\":\"from@test.com\",\"snippet\":\"short "
             "snippet.....\",\"subject\":\"test subject\",\"to\":\"to@test.com\"}");
  
  struct push_notification_driver_txn dtxn;
  struct push_notification_driver_user duser;
  struct push_notification_txn ptxn;
  struct mail_user user;
  struct push_notification_driver_kafka_context kafka_context;

  user.username = "testuser";
  ptxn.muser = &user;
  ptxn.pool = pool;
  kafka_context.userdb_json = NULL;

  duser.context = &kafka_context;

  dtxn.ptxn = &ptxn;
  dtxn.duser = &duser;

  struct push_notification_txn_msg msg;
  msg.uid = 1;
  msg.uid_validity = 2;
  msg.mailbox = "INBOX";
#if ! DOVECOT_PREREQ(2, 3)
  msg.seq = 1;
#endif

  struct push_notification_txn_event event;
  struct push_notification_event_messagenew_data data;
  data.date = 1535637723;
  data.date_tz = 0;
  data.from = "from@test.com";
  data.to = "to@test.com";
  data.subject = "test subject";
  data.snippet = "short snippet.....";

  event.data = &data;
  struct push_notification_event_config event_cfg;
  const struct push_notification_event notify_event = {.name = push_notification_event_messagenew.name};

  event_cfg.event = &notify_event;
  event.event = &event_cfg;
  struct push_notification_txn_event *const ev = &event;

  string_t *t = write_event_messagenew(&dtxn, &msg, &ev);
  // i_info("MSG: %s ", str_c(t));
  test_assert(str_equals(t, test));

  pool_unref(&pool);
  test_end();
}

static void write_event_messageappend_test(void) {
  test_begin("unit_write_event_messageappend");
  pool_t pool;
  pool = pool_alloconly_create("auth request handler", 4096);
  string_t *test = str_new(pool, 256);
  str_append(test,
             "{\"user\":\"testuser\",\"mailbox\":\"INBOX\",\"event\":\"MessageAppend\",\"uidvalidity\":2,\"uid\":1,"
             "\"from\":\"from@test.com\",\"snippet\":\"short "
             "snippet.....\",\"subject\":\"test subject\",\"to\":\"to@test.com\"}");
  struct push_notification_driver_txn dtxn;
  struct push_notification_driver_user duser;
  struct push_notification_txn ptxn;
  struct mail_user user;
  struct push_notification_driver_kafka_context kafka_context;

  user.username = "testuser";
  ptxn.muser = &user;
  ptxn.pool = pool;
  kafka_context.userdb_json = NULL;

  duser.context = &kafka_context;

  dtxn.ptxn = &ptxn;
  dtxn.duser = &duser;

  struct push_notification_txn_msg msg;
  msg.uid = 1;
  msg.uid_validity = 2;
  msg.mailbox = "INBOX";
#if ! DOVECOT_PREREQ(2, 3)
  msg.seq = 1;
#endif

  struct push_notification_txn_event event;
  struct push_notification_event_messageappend_data data;

  data.from = "from@test.com";
  data.to = "to@test.com";
  data.subject = "test subject";
  data.snippet = "short snippet.....";

  event.data = &data;
  struct push_notification_event_config event_cfg;
  const struct push_notification_event notify_event = {.name = push_notification_event_messageappend.name};

  event_cfg.event = &notify_event;
  event.event = &event_cfg;
  struct push_notification_txn_event *const ev = &event;

  string_t *t = write_event_messageappend(&dtxn, &msg, &ev);
  i_info("MSG: %s ", str_c(t));
  i_info("MSG: %s ", str_c(test));

  test_assert(str_equals(t, test));

  pool_unref(&pool);
  test_end();
}
int main(void) {
  static void (*test_functions[])(void) = {test_str_starts_with,
                                           test_push_notification_driver_kafka_render_mbox_rename,
                                           test_push_notification_driver_kafka_render_mbox_create,
                                           test_write_flags,
                                           write_msg_prefix_test,
                                           write_flags_event_test,
                                           write_event_messagenew_test,
                                           write_event_messageappend_test,
                                           NULL};
  return test_run(test_functions);
}
