#include "common_config.h"

char *str_global_config_file = NULL;
char *str_global_port = NULL;

char *str_global_device_table = NULL;
char *str_global_notification_table = NULL;
char *str_global_account_table = NULL;
char *str_global_account_table_pk_column = NULL;
char *str_global_account_table_guid_column = NULL;
char *str_global_listen_channel = NULL;

char *str_global_query_validate_guid = NULL;
char *str_global_query_delete_device = NULL;
char *str_global_query_insert_device = NULL;
char *str_global_query_get_notification = NULL;
char *str_global_query_get_unsent_notifications = NULL;
char *str_global_query_get_profile_id_from_notification = NULL;

char *str_global_pg_user = NULL;
char *str_global_pg_password = NULL;
char *str_global_pg_host = NULL;
char *str_global_pg_port = NULL;
char *str_global_pg_database = NULL;
char *str_global_pg_sslmode = NULL;
char *str_global_pg_conn_string = NULL;

char cwd[1024];

// clang-format off
// VAR									CONFIG NAME						COMMAND-LINE SHORT NAME		COMMAND-LINE FULL NAME
// str_global_config_file				NULL							c							config-file
// str_global_port						port							p							port
// str_global_log_level					log_level						l							log-level
// str_global_device_table				device_table					d							device-table
// str_global_notification_table		notification_table				n							notification-table
// str_global_account_table				account_table					a							account-table
// str_global_account_table_pk_column	account_table_pk_column			k							account-table-pk-column
// str_global_account_table_guid_column	account_table_guid_column		g							account-table-guid-column
// str_global_listen_channel			listen_channel					N							listen-channel
// str_global_pg_user					pg_user							U							pg-user
// str_global_pg_password				pg_password						W							pg-password
// str_global_pg_host					pg_host							H							pg-host
// str_global_pg_port					pg_port							P							pg-port
// str_global_pg_database				pg_database						D							pg-database
// str_global_pg_sslmode				pg_sslmode						S							pg-sslmode
// int_global_apns_mode					apns_mode						M							apns-mode
// str_global_apns_cer					apns_cert						C							apns-cert
// clang-format on

/*
This function is called for each directive in the ini file
*/
static int handler(void *str_user, const char *str_section, const char *str_name, const char *str_value) {
	if (str_user != NULL) {
	} // get rid of unused variable warning

	size_t int_len = 0;

#define SMATCH(s, n) strcmp(str_section, s) == 0 && strcmp(str_name, n) == 0
	if (SMATCH("", "port")) {
		SFREE(str_global_port);
		SERROR_SNCAT(str_global_port, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "log_level")) {
		SFREE(str_global_log_level);
		SERROR_SNCAT(str_global_log_level, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "device_table")) {
		SFREE(str_global_device_table);
		SERROR_SNCAT(str_global_device_table, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "notification_table")) {
		SFREE(str_global_notification_table);
		SERROR_SNCAT(str_global_notification_table, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "account_table")) {
		SFREE(str_global_account_table);
		SERROR_SNCAT(str_global_account_table, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "account_table_pk_column")) {
		SFREE(str_global_account_table_pk_column);
		SERROR_SNCAT(str_global_account_table_pk_column, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "account_table_column")) {
		SFREE(str_global_account_table_guid_column);
		SERROR_SNCAT(str_global_account_table_guid_column, &int_len, str_value, strlen(str_value));

	} else if (SMATCH("", "listen_channel")) {
		SFREE(str_global_listen_channel);
		SERROR_SNCAT(str_global_listen_channel, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "pg_user")) {
		SFREE(str_global_pg_user);
		SERROR_SNCAT(str_global_pg_user, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "pg_password")) {
		SFREE(str_global_pg_password);
		SERROR_SNCAT(str_global_pg_password, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "pg_host")) {
		SFREE(str_global_pg_host);
		SERROR_SNCAT(str_global_pg_host, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "pg_port")) {
		SFREE(str_global_pg_port);
		SERROR_SNCAT(str_global_pg_port, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "pg_database")) {
		SFREE(str_global_pg_database);
		SERROR_SNCAT(str_global_pg_database, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "pg_sslmode")) {
		SFREE(str_global_pg_sslmode);
		SERROR_SNCAT(str_global_pg_sslmode, &int_len, str_value, strlen(str_value));
	
	} else if (SMATCH("", "apns_mode")) {
		int_global_apns_mode = str_value[0] == 'p' || str_value[0] == 'P' ? APNS_MODE_PRODUCTION : APNS_MODE_DEVELOPMENT;
	
	} else if (SMATCH("", "apns_cert")) {
		SFREE(str_global_apns_cert);
		SERROR_SNCAT(str_global_apns_cert, &int_len, str_value, strlen(str_value));
	
	} else {
		SERROR("Unknown config section/name: %s %s", str_section, str_name);
	}
	bol_error_state = false;
	return 1;
error:
	bol_error_state = false;
	return 0;
}

bool parse_options(int argc, char *const *argv) {
	size_t int_global_len = 0;
	size_t int_prefix_len = 0;
	char *str_temp_conninfo_value = NULL;

	SERROR_SNCAT(str_global_log_level, &int_global_len,
		"error", (size_t)5);

	int ch;
	int_prefix_len = strlen(PREFIX);

	SERROR_SNCAT(
		str_global_config_file, &int_global_len,
		PREFIX, int_prefix_len,
		"/etc/" SUN_PROGRAM_LOWER_NAME "/" SUN_PROGRAM_LOWER_NAME ".conf",
			strlen("/etc/" SUN_PROGRAM_LOWER_NAME "/" SUN_PROGRAM_LOWER_NAME ".conf"));

	SERROR_SNCAT(str_global_port, &int_global_len, "8989", (size_t)4);

	SERROR_SNCAT(str_global_device_table				, &int_global_len, "wkd.rdevice", (size_t)11);
	SERROR_SNCAT(str_global_notification_table			, &int_global_len, "wkd.rnotification", (size_t)17);
	SERROR_SNCAT(str_global_account_table				, &int_global_len, "wkd.rprofile", (size_t)12);
	SERROR_SNCAT(str_global_account_table_pk_column		, &int_global_len, "id", (size_t)2);
	SERROR_SNCAT(str_global_account_table_guid_column	, &int_global_len, "guid", (size_t)4);
	SERROR_SNCAT(str_global_listen_channel				, &int_global_len, "wk_notify", (size_t)9);

	SERROR_SNCAT(str_global_pg_user		, &int_global_len, "wkd_notify_user", (size_t)14);
	SERROR_SNCAT(str_global_pg_password	, &int_global_len, "wkd_notify_password", (size_t)18);
	SERROR_SNCAT(str_global_pg_host		, &int_global_len, "localhost", (size_t)9);
	SERROR_SNCAT(str_global_pg_port		, &int_global_len, "5432", (size_t)4);
	SERROR_SNCAT(str_global_pg_database	, &int_global_len, "postgres", (size_t)8);
	SERROR_SNCAT(str_global_pg_sslmode	, &int_global_len, "require", (size_t)7);
	
	// options descriptor
	// clang-format off
	static struct option longopts[22] = {
		{"help",							no_argument,			NULL,	'h'},
		{"version",							no_argument,			NULL,	'v'},
		{"config-file",						required_argument,		NULL,	'c'},
		{"port",							required_argument,		NULL,	'p'},
		{"log-level",						required_argument,		NULL,	'l'},
		{"device-table", 					required_argument, 		NULL, 	'd'},
		{"notification-table", 				required_argument, 		NULL, 	'n'},
		{"account-table", 					required_argument, 		NULL, 	'a'},
		{"account-table-pk-column", 		required_argument, 		NULL, 	'k'},
		{"account-table-guid-column", 		required_argument, 		NULL, 	'g'},
		{"listen-channel", 					required_argument, 		NULL, 	'N'},
		{"pg-user", 						required_argument, 		NULL, 	'U'},
		{"pg-password", 					required_argument, 		NULL, 	'W'},
		{"pg-host", 						required_argument, 		NULL, 	'H'},
		{"pg-port", 						required_argument, 		NULL, 	'P'},
		{"pg-database", 					required_argument, 		NULL, 	'D'},
		{"pg-sslmode", 						required_argument, 		NULL, 	'S'},
		{"apns-mode", 						required_argument, 		NULL, 	'M'},
		{"apns-cert", 						required_argument, 		NULL, 	'C'},
		{NULL,								0,						NULL,	0}
	};
	// clang-format on

	while ((ch = getopt_long(argc, argv, "hvc:p:l:d:n:a:k:g:N:U:W:H:P:D:S:M:C:", longopts, NULL)) != -1) {
		if (ch == '?') {
			// getopt_long prints an error in this case
			goto error;

		} else if (ch == 'h') {
			usage();
			goto error;
		} else if (ch == 'v') {
			printf(SUN_PROGRAM_WORD_NAME " %s\n", VERSION);
			goto error;
		} else if (ch == 'c') {
			SFREE(str_global_config_file);
			SERROR_SNCAT(str_global_config_file, &int_global_len,
				optarg, strlen(optarg));
		}
	}

	opterr = 0;
	optind = 1;

	char *str_config_empty = "";
	ini_parse(str_global_config_file, handler, &str_config_empty);

	while ((ch = getopt_long(argc, argv, "hvc:p:l:d:n:a:k:g:N:U:W:H:P:D:S:M:C:", longopts, NULL)) != -1) {
		if (ch == '?') {
			// getopt_long prints an error in this case
			goto error;

		} else if (ch == 'h') {
		} else if (ch == 'v') {
		} else if (ch == 'c') {

		} else if (ch == 'l') {
			SFREE(str_global_log_level);
			SERROR_SNCAT(str_global_log_level, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'd') {
			SFREE(str_global_device_table);
			SERROR_SNCAT(str_global_device_table, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'n') {
			SFREE(str_global_notification_table);
			SERROR_SNCAT(str_global_notification_table, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'a') {
			SFREE(str_global_account_table);
			SERROR_SNCAT(str_global_account_table, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'k') {
			SFREE(str_global_account_table_pk_column);
			SERROR_SNCAT(str_global_account_table_pk_column, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'g') {
			SFREE(str_global_account_table_guid_column);
			SERROR_SNCAT(str_global_account_table_guid_column, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'N') {
			SFREE(str_global_listen_channel);
			SERROR_SNCAT(str_global_listen_channel, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'U') {
			SFREE(str_global_pg_user);
			SERROR_SNCAT(str_global_pg_user, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'W') {
			SFREE(str_global_pg_password);
			SERROR_SNCAT(str_global_pg_password, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'H') {
			SFREE(str_global_pg_host);
			SERROR_SNCAT(str_global_pg_host, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'P') {
			SFREE(str_global_pg_port);
			SERROR_SNCAT(str_global_pg_port, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'D') {
			SFREE(str_global_pg_database);
			SERROR_SNCAT(str_global_pg_database, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'S') {
			SFREE(str_global_pg_sslmode);
			SERROR_SNCAT(str_global_pg_sslmode, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 'M') {
			int_global_apns_mode = optarg[0] == 'p' || optarg[0] == 'P' ? APNS_MODE_PRODUCTION : APNS_MODE_DEVELOPMENT;

		} else if (ch == 'C') {
			SFREE(str_global_apns_cert);
			SERROR_SNCAT(str_global_apns_cert, &int_global_len, optarg, strlen(optarg));

		} else if (ch == 0) {
			fprintf(stderr, "no options");
			goto error;
		} else {
			usage();
			goto error;
		}
	}

	SINFO(int_global_apns_mode == APNS_MODE_PRODUCTION ? "APNS_MODE_PRODUCTION" : "APNS_MODE_DEVELOPMENT");
	
	SERROR_CHECK(str_global_apns_cert != NULL, "APNS certificate is required");

	char *str_query1 = "SELECT ( "
		"CASE WHEN (SELECT count(*) "
		"	FROM {{account_table}} "
		"	WHERE {{account_table_guid_column}} = {{GUID}}) = 1 "
		"THEN "
		"		(SELECT id "
		"			FROM {{account_table}} "
		"			WHERE {{account_table_guid_column}} = {{GUID}})::text "
		"ELSE "
		"	'NO MATCH' "
		"END);";

	SERROR_SNCAT(str_global_query_validate_guid, &int_global_len, str_query1, strlen(str_query1));
	SERROR_BREPLACE(str_global_query_validate_guid, &int_global_len, "{{account_table}}", str_global_account_table, "g");
	SERROR_BREPLACE(str_global_query_validate_guid, &int_global_len, "{{account_table_guid_column}}", str_global_account_table_guid_column, "g");
	
	char *str_query2 = "DELETE FROM {{device_table}} WHERE apns_device_id = {{APNS}};";

	SERROR_SNCAT(str_global_query_delete_device, &int_global_len, str_query2, strlen(str_query2));
	SERROR_BREPLACE(str_global_query_delete_device, &int_global_len, "{{device_table}}", str_global_device_table, "g");

	char *str_query3 = "INSERT INTO {{device_table}} (profile_id, apns_device_id) VALUES ({{PROFILEID}}, {{APNS}});";

	SERROR_SNCAT(str_global_query_insert_device, &int_global_len, str_query3, strlen(str_query3));
	SERROR_BREPLACE(str_global_query_insert_device, &int_global_len, "{{device_table}}", str_global_device_table, "g");

	char *str_query4 = "SELECT title, body, recipient_id, apns_device_id, {{notification_table}}.create_stamp, extra "
		"FROM {{notification_table}} "
		"LEFT JOIN {{device_table}} ON {{device_table}}.profile_id = {{notification_table}}.recipient_id "
		"WHERE {{notification_table}}.id = {{NOTIFYID}};";

	SERROR_SNCAT(str_global_query_get_notification, &int_global_len, str_query4, strlen(str_query4));
	SERROR_BREPLACE(str_global_query_get_notification, &int_global_len, "{{device_table}}", str_global_device_table, "g");
	SERROR_BREPLACE(str_global_query_get_notification, &int_global_len, "{{notification_table}}", str_global_notification_table, "g");

	char *str_query5 = "SELECT title, body, id, create_stamp, extra "
		"FROM {{notification_table}} "
		"WHERE {{notification_table}}.id > {{NOTIFYID}} AND {{notification_table}}.recipient_id = {{PROFILEID}} AND {{notification_table}}.rread IS NOT NULL;";

	SERROR_SNCAT(str_global_query_get_unsent_notifications, &int_global_len, str_query5, strlen(str_query5));
	SERROR_BREPLACE(str_global_query_get_unsent_notifications, &int_global_len, "{{device_table}}", str_global_device_table, "g");
	SERROR_BREPLACE(str_global_query_get_unsent_notifications, &int_global_len, "{{notification_table}}", str_global_notification_table, "g");

	char *str_query6 = "SELECT recipient_id "
	"FROM {{notification_table}} "
	"WHERE {{notification_table}}.id = {{NOTIFYID}};";

	SERROR_SNCAT(str_global_query_get_profile_id_from_notification, &int_global_len, str_query6, strlen(str_query6));
	SERROR_BREPLACE(str_global_query_get_profile_id_from_notification, &int_global_len, "{{notification_table}}", str_global_notification_table, "g");

str_temp_conninfo_value = str_global_pg_host;
	int_global_len = strlen(str_temp_conninfo_value);
	SERROR_CHECK(str_global_pg_host = escape_conninfo_value(str_temp_conninfo_value, &int_global_len), "escape_conninfo_value failed");
	SFREE(str_temp_conninfo_value);
	
	str_temp_conninfo_value = str_global_pg_port;
	int_global_len = strlen(str_temp_conninfo_value);
	SERROR_CHECK(str_global_pg_port = escape_conninfo_value(str_temp_conninfo_value, &int_global_len), "escape_conninfo_value failed");
	SFREE(str_temp_conninfo_value);
	
	str_temp_conninfo_value = str_global_pg_database;
	int_global_len = strlen(str_temp_conninfo_value);
	SERROR_CHECK(str_global_pg_database = escape_conninfo_value(str_temp_conninfo_value, &int_global_len), "escape_conninfo_value failed");

	str_temp_conninfo_value = str_global_pg_sslmode;
	int_global_len = strlen(str_temp_conninfo_value);
	SERROR_CHECK(str_global_pg_sslmode = escape_conninfo_value(str_temp_conninfo_value, &int_global_len), "escape_conninfo_value failed");
	SFREE(str_temp_conninfo_value);
	
	SERROR_SNCAT(str_global_pg_conn_string, &int_global_len, "", (size_t)0);
	SERROR_SNFCAT(str_global_pg_conn_string, &int_global_len, " host=", 	(size_t)6, str_global_pg_host, strlen(str_global_pg_host));
	SERROR_SNFCAT(str_global_pg_conn_string, &int_global_len, " port=", 	(size_t)6, str_global_pg_port, strlen(str_global_pg_port));
	SERROR_SNFCAT(str_global_pg_conn_string, &int_global_len, " dbname=", 	(size_t)8, str_global_pg_database, strlen(str_global_pg_database));
	SERROR_SNFCAT(str_global_pg_conn_string, &int_global_len, " sslmode=", 	(size_t)9, str_global_pg_sslmode, strlen(str_global_pg_sslmode));
	
	SINFO("Connection string: %s", str_global_pg_conn_string);

	bol_error_state = false;
	return true;
error:
	SFREE(str_temp_conninfo_value);
	bol_error_state = false;
	return false;
}

void usage() {
	printf("Usage: " SUN_PROGRAM_LOWER_NAME "\012");
	printf("\t[-h                             | --help]\012");
	printf("\t[-v                             | --version]\012");
	printf("\t[-c <config-file>               | --config-file=<config-file>]\012");
	printf("\t[-p <port>                      | ---port=<port>]\012");
	printf("\t[-l <log-level>                 | --log-level=<log-level>]\012");
	printf("\t[-d <device-table>              | --device-table=<device-table>]\012");
	printf("\t[-n <notification-table>        | --notification-table=<notification-table>]\012");
	printf("\t[-a <account-table>             | --account-table=<account-table>]\012");
	printf("\t[-k <account-table-pk-column>   | --account-table-pk-column=<account-table-pk-column>]\012");
	printf("\t[-g <account-table-guid-column> | --account-table-guid-column=<account-table-guid-column>]\012");
	printf("\t[-N <listen-channel>            | --listen-channel=<listen-channel>]\012");
	printf("\t[-U <pg-user>                   | --pg-user=<pg-user>]\012");
	printf("\t[-W <pg-password>               | --pg-password=<pg-password>]\012");
	printf("\t[-H <pg-host>                   | --pg-host=<pg-host>]\012");
	printf("\t[-P <pg-port>                   | --pg-port=<pg-port>]\012");
	printf("\t[-D <pg-database>               | --pg-database=<pg-database>]\012");
	printf("\t[-S <pg-sslmode>                | --pg-sslmode=<pg-sslmode>]\012");
	printf("\t[-M <apns-mode>                 | --apns-mode=<apns-mode>]\012");
	printf("\t[-C <apns-cert>                 | --apns-cert=<apns-cert>]\012");
	printf("\012");
	printf("For more information, run `man " SUN_PROGRAM_LOWER_NAME "`\012");
}
