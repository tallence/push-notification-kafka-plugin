cmake_minimum_required(VERSION 3.5)
project(push-notification-kafka-plugin)

set(CMAKE_C_STANDARD 99)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}")

FIND_PACKAGE(Dovecot 2.3 REQUIRED)
INCLUDE_DIRECTORIES(${DOVECOT_INCLUDE_DIRS} src)

include(CheckFunctionExists)
check_function_exists(snprintf HAVE_SNPRINTF)

set(SOURCE_FILES src/push-notification-kafka-plugin.c src/push-notification-kafka-driver.c src/push-notification-kafka-event.c)
add_library(push-notification-kafka-plugin SHARED ${SOURCE_FILES})

target_link_libraries(push-notification-kafka-plugin debug ${LIBDOVECOTSTORAGE} ${LIBDOVECOT})

include(GNUInstallDirs)
install(TARGETS push-notification-kafka-plugin
        LIBRARY DESTINATION ${DOVECOT_LIB_DIR}/doveadm)


add_executable(push-notification-kafka-driver-test src/lib-test/push-notification-kafka-driver-test.c src/lib-test/test-common.c ${SOURCE_FILES})
target_link_libraries(push-notification-kafka-driver-test debug ${LIBDOVECOTNOTIFYPLUGIN} ${LIBDOVECOTPUSHNOTIFICATIONPLUGIN} ${LIBDOVECOTSTORAGE} ${LIBDOVECOT} rdkafka)

add_executable(push-notification-kafka-event-test src/lib-test/push-notification-kafka-event-test.c src/lib-test/test-common.c ${SOURCE_FILES})
target_link_libraries(push-notification-kafka-event-test debug ${LIBDOVECOTNOTIFYPLUGIN} ${LIBDOVECOTPUSHNOTIFICATIONPLUGIN} ${LIBDOVECOTSTORAGE} ${LIBDOVECOT} rdkafka)
