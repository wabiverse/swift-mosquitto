/*
 Copyright (c) 2009-2020 Roger Light <roger@atchoo.org>
 
 All rights reserved. This program and the accompanying materials
 are made available under the terms of the Eclipse Public License 2.0
 and Eclipse Distribution License v1.0 which accompany this distribution.
 
 The Eclipse Public License is available at
 https://www.eclipse.org/legal/epl-2.0/
 and the Eclipse Distribution License is available at
 http://www.eclipse.org/org/documents/edl-v10.php.
 
 SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 
 Contributors:
 Roger Light - initial implementation and documentation.
 */

#include "config.h"

#ifndef WIN32
/* For initgroups() */
#  include <unistd.h>
#  include <grp.h>
#  include <assert.h>
#endif

#ifndef WIN32
#include <pwd.h>
#else
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#ifndef WIN32
#  include <sys/time.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WITH_SYSTEMD
#  include <systemd/sd-daemon.h>
#endif
#ifdef WITH_WRAP
#include <tcpd.h>
#endif
#ifdef WITH_WEBSOCKETS
#  include <libwebsockets.h>
#endif

#include "mosquitto.h"
#include "mosquitto_broker.h"

typedef struct mosquitto_db_shim {
  uint64_t last_db_id;
  struct mosquitto__subhier *subs;
  struct mosquitto__retainhier *retains;
  struct mosquitto *contexts_by_id;
  struct mosquitto *contexts_by_sock;
  struct mosquitto *contexts_for_free;
#ifdef WITH_BRIDGE
  struct mosquitto **bridges;
#endif
  struct clientid__index_hash *clientid_index_hash;
  struct mosquitto_msg_store *msg_store;
  struct mosquitto_msg_store_load *msg_store_load;
  time_t now_s; /* Monotonic clock, where possible */
  time_t now_real_s; /* Read clock, for measuring session/message expiry */
#ifdef WITH_BRIDGE
  int bridge_count;
#endif
  int msg_store_count;
  unsigned long msg_store_bytes;
  char *config_file;
  struct mosquitto__config *config;
  int auth_plugin_count;
  bool verbose;
#ifdef WITH_SYS_TREE
  int subscription_count;
  int shared_subscription_count;
  int retained_count;
#endif
  int persistence_changes;
  struct mosquitto *ll_for_free;
#ifdef WITH_EPOLL
  int epollfd;
#endif
  struct mosquitto_message_v5 *plugin_msgs;
} mosquitto_db;

enum mosquitto_protocol {
  mp_mqtt,
  mp_mqttsn,
  mp_websockets
};

typedef struct plugin__callbacks_shim {
  struct mosquitto__callback *tick;
  struct mosquitto__callback *acl_check;
  struct mosquitto__callback *basic_auth;
  struct mosquitto__callback *control;
  struct mosquitto__callback *disconnect;
  struct mosquitto__callback *ext_auth_continue;
  struct mosquitto__callback *ext_auth_start;
  struct mosquitto__callback *message;
  struct mosquitto__callback *psk_key;
  struct mosquitto__callback *reload;
} plugin__callbacks;

typedef struct mosquitto_plugin_id_t_shim {
  struct mosquitto__listener *listener;
} mosquitto_plugin_id_t;

typedef struct mosquitto__security_options_shim {
  /* Any options that get added here also need considering
   * in config__read() with regards whether allow_anonymous
   * should be disabled when these options are set.
   */
  struct mosquitto__unpwd *unpwd;
  struct mosquitto__unpwd *psk_id;
  struct mosquitto__acl_user *acl_list;
  struct mosquitto__acl *acl_patterns;
  char *password_file;
  char *psk_file;
  char *acl_file;
  struct mosquitto__auth_plugin_config *auth_plugin_configs;
  int auth_plugin_config_count;
  int8_t allow_anonymous;
  bool allow_zero_length_clientid;
  char *auto_id_prefix;
  uint16_t auto_id_prefix_len;
  plugin__callbacks plugin_callbacks;
  mosquitto_plugin_id_t *pid; /* For registering as a "plugin" */
} mosquitto__security_options;

typedef struct mosquitto__listener_shim {
  uint16_t port;
  char *host;
  char *bind_interface;
  int max_connections;
  char *mount_point;
  int *socks;
  int sock_count;
  int client_count;
  enum mosquitto_protocol protocol;
  int socket_domain;
  bool use_username_as_clientid;
  uint8_t max_qos;
  uint16_t max_topic_alias;
#ifdef WITH_TLS
  char *cafile;
  char *capath;
  char *certfile;
  char *keyfile;
  char *tls_engine;
  char *tls_engine_kpass_sha1;
  char *ciphers;
  char *ciphers_tls13;
  char *psk_hint;
  SSL_CTX *ssl_ctx;
  char *crlfile;
  char *tls_version;
  char *dhparamfile;
  bool use_identity_as_username;
  bool use_subject_as_username;
  bool require_certificate;
  enum mosquitto__keyform tls_keyform;
#endif
#ifdef WITH_WEBSOCKETS
  struct lws_context *ws_context;
  bool ws_in_init;
  char *http_dir;
  struct lws_protocols *ws_protocol;
#endif
  mosquitto__security_options security_options;
#ifdef WITH_UNIX_SOCKETS
  char *unix_socket_path;
#endif
} mosquitto__listener;

mosquitto_db db;

static struct mosquitto__listener_sock *listensock = NULL;
static int listensock_count = 0;
static int listensock_index = 0;

bool flag_reload = false;
#ifdef WITH_PERSISTENCE
bool flag_db_backup = false;
#endif
bool flag_tree_print = false;
int run;
#ifdef WITH_WRAP
#include <syslog.h>
int allow_severity = LOG_INFO;
int deny_severity = LOG_INFO;
#endif

/* mosquitto shouldn't run as root.
 * This function will attempt to change to an unprivileged user and group if
 * running as root. The user is given in config->user.
 * Returns 1 on failure (unknown user, setuid/setgid failure)
 * Returns 0 on success.
 * Note that setting config->user to "root" does not produce an error, but it
 * strongly discouraged.
 */
int drop_privileges(struct mosquitto__config *config)
{
  return MOSQ_ERR_SUCCESS;
}

static void mosquitto__daemonise(void)
{
#ifndef WIN32
  char *err;
  pid_t pid;
  
  pid = fork();
  if(pid < 0){
    err = strerror(errno);
    printf("Error in fork: %s\n", err);
    exit(1);
  }
  if(pid > 0){
    exit(0);
  }
  if(setsid() < 0){
    err = strerror(errno);
    printf("Error in setsid: %s\n", err);
    exit(1);
  }
  
  if(!freopen("/dev/null", "r", stdin)){
    printf("Error whilst daemonising (%s): %s\n", "stdin", strerror(errno));
    exit(1);
  }
  if(!freopen("/dev/null", "w", stdout)){
    printf("Error whilst daemonising (%s): %s\n", "stdout", strerror(errno));
    exit(1);
  }
  if(!freopen("/dev/null", "w", stderr)){
    printf("Error whilst daemonising (%s): %s\n", "stderr", strerror(errno));
    exit(1);
  }
#else
  printf("Warning: Can't start in daemon mode in Windows.\n");
#endif
}


void listener__set_defaults(struct mosquitto__listener *listener)
{}


void listeners__reload_all_certificates(void)
{}


static int listeners__start_single_mqtt(struct mosquitto__listener *listener)
{
  return MOSQ_ERR_SUCCESS;
}


#ifdef WITH_WEBSOCKETS
void listeners__add_websockets(struct lws_context *ws_context, mosq_sock_t fd)
{}
#endif

static int listeners__add_local(const char *host, uint16_t port)
{
  return MOSQ_ERR_SUCCESS;
}

static int listeners__start_local_only(void)
{
  return MOSQ_ERR_SUCCESS;
}


static int listeners__start(void)
{
  return MOSQ_ERR_SUCCESS;
}


static void listeners__stop(void)
{}


static void signal__setup(void)
{}


static int pid__write(void)
{
  return MOSQ_ERR_SUCCESS;
}


int mosquitto_main(int argc, char *argv[])
{
  return MOSQ_ERR_SUCCESS;
}
