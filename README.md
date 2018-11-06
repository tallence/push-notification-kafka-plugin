# Kafka Driver for Dovecot's Push Notification Framework

This project provides a driver for [Dovecot's Push Notification Framework](https://wiki.dovecot.org/Plugins/PushNotification) for sending push notifications events to a Kafka broker. The events will be rendered as JSON data structures.

## Disclaimer

This project is under active development and not in any kind of release state. Be aware it is possible and very likely that APIs, interfaces and or the data format change at any time before a first release.

The code is in a developer tested state, but is NOT production ready. Although the code is still flagged as experimental, we encourage users to try it out for non-production clusters and non-critical data sets and report their experience, findings and issues.

## push-notification-kafka

The provided driver for [Dovecot's Push Notification Framework](https://wiki.dovecot.org/Plugins/PushNotification) is a thin wrapper around the framework events. They are rendered as JSON data structures and published to a Kafka topic. [librdkafka](https://github.com/edenhill/librdkafka) is used for accessing the Kafka brokers as a producer.

### Compile and install the Plugins

To compile the plugin you need a configured or installed Dovecot >= 2.2.21.

#### Checking out the source

You can clone from github with

    git clone https://gitlab1.tallence.com/Telekom/push-notification-kafka-plugin.git

The build requires that you have the following software/packages installed:

    librdkafka-devel version >= 0.11.4
    dovecot-devel

#### Dovecot installation in /usr/local

    ./autogen.sh
    ./configure
    make
    sudo make install

#### Dovecot installation in ~/dovecot

    ./autogen.sh
    ./configure --prefix=/home/<user>/dovecot
    make install

#### Configured source tree in ~/workspace/core

    ./autogen.sh
    ./configure --with-dovecot=/home/<user>/workspace/core
    make install

### Configuration

#### mail_plugins

Follow the instructions in [Dovecot's Push Notification Framework](https://wiki.dovecot.org/Plugins/PushNotification) description. Add the driver `push_notification_kafka` to the list of mail_plugins:

    mail_plugins = $mail_plugins notify push_notification push_notification_kafka

#### Plugin Configuration

 To active the Kafka driver an entry `push_notification_driver` has to be added to the Dovecot plugin configuration, specifying the Kafka driver URL.  

    push_notification_driver = kafka:topic=dovecot
    push_notification_driver2 = kafka:topic=expunge events=FlagsClear,FlagsSet,MessageExpunge

The Kafka driver is able to produce to *one* Cluster of Kafka brokers. You can start multiple driver instances publishing to different topics with different configurations.

The following options can be configured as part of the driver URL:

* **topic**: The name of the Kafka topic to publish to. The default value is _dovecot_.
* **events**: The list of push notification events that will be published. The default is all of them:
  * FlagsClear
  * FlagsSet
  * MailboxCreate
  * MailboxDelete
  * MailboxRename
  * MailboxSubscribe
  * MailboxUnsubscribe
  * MessageAppend
  * MessageExpunge
  * MessageNew
  * MessageRead
  * MessageTrash
* **keyword_prefix**: If _FlagsClear_ or _FlagsSet_ are active, `keyword_prefix` controls which keywords will be published. The default is $.
* **send_flags**: If _FlagsClear_ or _FlagsSet_ are active, `send_flags` controls whether changes of IMAP flags like /Seen will be published. The default value is `on`
  * `on`: publish flag changes
  * `off`: do not publish flag changes
* **feature**: The name of an `user_db` field that enables or disables Kafka events on a per user base. The value must be `on` to allow Kafka events. If `feature` is not configured, all users are enabled by default.

 Some general communications properties are configurable in the plugin section:

* **kafka.notification.kafka_brokers**: Bootstrap servers for the Kafka cluster. The default value is `localhost:9092`.
* **kafka.notification.kafka_debug**: Debug option for librdkafka. See [Configuation](https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md) for details.
* **kafka.notification.kafka_max_retries**: If the internal queue is full, wait for messages to be delivered and then retry. The internal queue represents both messages to be sent_kafka_max_retries_ is 0, which is the default value, no retries will be done.
* **kafka.notification.kafka_retry_poll_time_in_ms**: The time to wait between retries.
* **kafka.notification.kafka_flush_time_in_ms**: Time in ms to wait while flushing internal queues on close.
* **kafka.notification.kafka_destroy_time_in_ms**: Time in ms to wait for the Kafka producer instance to be destroyed.

* **kafka.notification.settings.**: The prefix can be used to set the librdkafka specific configuration settings as described [here](https://docs.confluent.io/2.0.0/clients/librdkafka/CONFIGURATION_8md.html).
```
plugin {
  push_notification_driver = kafka:topic=test
  kafka.notification.kafka_brokers = localhost:9092
  kafka.notification.kafka_debug =
  kafka.notification.kafka_max_retries = 0
  kafka.notification.kafka_retry_poll_time_in_ms = 500
  kafka.notification.kafka_flush_time_in_ms = 1000
  kafka.notification.kafka_destroy_time_in_ms = 1000
  
  #ssl configuration
  kafka.notification.settings.security.protocol=ssl
  kafka.notification.settings.ssl.key.location=client_host.key  
  kafka.notification.settings.ssl.key.password=PLAINTEXT pwd
  kafka.notification.settings.ssl.certificate.location=client_host.pem
  kafka.notification.settings.ssl.ca.location=ca-cert
}
```

### Event Serialization

The events are serialized as JSON data structures. The invariant prefix is
* user
* mailbox
* event

A mail is identified by
* uidvalidity
* uid

Depending of the actual event type additional fields like keyword or flags are send.

```
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsClear","uidvalidity":1531299564,"uid":13,"keywords":["$label2","$label1"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsClear","uidvalidity":1531299564,"uid":13,"flags":["\\Seen"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsSet","uidvalidity":1531299564,"uid":13,"keywords":["$label1"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsSet","uidvalidity":1531299564,"uid":13,"flags":["\\Seen"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"MessageRead","uidvalidity":1531299564,"uid":13}
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsSet","uidvalidity":1531299564,"uid":13,"keywords":["$label2"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsClear","uidvalidity":1531299564,"uid":13,"keywords":["$label2","$label1"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"FlagsSet","uidvalidity":1531299564,"uid":13,"flags":["\\Deleted"]}
{"user":"520000004149-0001","mailbox":"INBOX","event":"MessageTrash","uidvalidity":1531299564,"uid":13}
{"user":"520000004149-0001","mailbox":"INBOX","event":"MessageExpunge","uidvalidity":1531299564,"uid":13}
```

**Any known field has to be ignored by the consumer.**

## Known Bugs

* The mailbox events are not working.

## Thanks

<table border="0">
  <tr>
    <td><img src="https://upload.wikimedia.org/wikipedia/commons/2/2e/Telekom_Logo_2013.svg"</td>
    <td>The development of this software is sponsored by Deutsche Telekom. We would like to take this opportunity to thank Deutsche Telekom.</td>
  </tr>
  <tr>
    <td><img src="https://www.tallence.com/fileadmin/user_upload/content/Mailing/tallence_logo-email.png"</td>
    <td><a href="https://www.tallence.com/">Tallence</a> carried out the initial development.</td>
  </tr>
  <tr>
    <td><img src="https://upload.wikimedia.org/wikipedia/commons/3/37/Dovecot-logo.png"</td>
    <td>This plugin borrows heavily from <a href="https://github.com/dovecot/core/">Dovecot</a> itself particularly for the automatic detection of dovecont-config (see m4/dovecot.m4). The lib-dict and lib-storage were also used as reference material for understanding the Dovecot dictionary and storage API.</td>
  </tr>
</table>
