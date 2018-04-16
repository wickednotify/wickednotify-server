#pragma once

#include <stdbool.h>
#include <stdio.h>
#include "db_framework.h"
#include "apns.h"
#include <getopt.h>
#include <pwd.h>
#include "util_canonical.h"
#include "util_darray.h"
#include "util_error.h"
#include "util_ini.h"

#define SUN_PROGRAM_LOWER_NAME "wickednotify"
#define SUN_PROGRAM_WORD_NAME "WickedNotify"
#define SUN_PROGRAM_UPPER_NAME "WICKEDNOTIFY"

extern char *str_global_config_file;
extern char *str_global_port;

extern char *str_global_device_table;
extern char *str_global_notification_table;
extern char *str_global_account_table;
extern char *str_global_account_table_pk_column;
extern char *str_global_account_table_guid_column;
extern char *str_global_listen_channel;

extern char *str_global_query_validate_guid;
extern char *str_global_query_delete_device;
extern char *str_global_query_insert_device;
extern char *str_global_query_get_notification;
extern char *str_global_query_get_unsent_notifications;
extern char *str_global_query_get_profile_id_from_notification;

extern char *str_global_pg_user;
extern char *str_global_pg_password;
extern char *str_global_pg_host;
extern char *str_global_pg_port;
extern char *str_global_pg_database;
extern char *str_global_pg_sslmode;
extern char *str_global_pg_conn_string;

extern char cwd[1024];

/*
This function reads the options from the command line and the config file
*/
bool parse_options(int argc, char *const *argv);

/*
This function prints the usage and exits
*/
void usage();
