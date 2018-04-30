#include <ev.h>

#include "common_util_sql.h"
#include "common_server.h"
#include "apns.h"
#include "registration.h"
#include "notification.h"
#include <stdio.h>

int int_sock = -1;

ev_signal sigint_watcher;
ev_signal sigterm_watcher;

void connect_cb(EV_P, void *cb_data, DB_conn *conn);
void sigint_cb(EV_P, ev_signal *w, int revents);

/*
Program entry point
*/
int main(int argc, char *const *argv) {
	memset(&sigint_watcher, 0, sizeof(ev_signal));
	memset(&sigterm_watcher, 0, sizeof(ev_signal));
	SERROR_CHECK(getcwd(cwd, sizeof(cwd)) != NULL, "getcwd failed");

	// This would be an SERROR_CHECK, but the parse_options function already
	// prints an error if it fails
	if (parse_options(argc, argv) == false) {
		goto error;
	}
	SINFO("Configuration finished");

	global_loop = ev_default_loop(0);
	SERROR_CHECK(global_loop != NULL, "ev_default_loop failed!");

	ev_signal_init(&sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(global_loop, &sigint_watcher);
	ev_signal_init(&sigterm_watcher, sigint_cb, SIGTERM);
	ev_signal_start(global_loop, &sigterm_watcher);

	memset(&_server, 0, sizeof(_server));

	// clear sigpipe handler,
	//    otherwise we get killed instead of errno=EPIPE
	signal(SIGPIPE, SIG_IGN);

	SERROR_CHECK(DB_init_framework(), "DB_init_framework() failed");

	// Initialize SSL (for HTTP/2 connection to APNS)
	SSL_load_error_strings();
	SSL_library_init();
	
	global_conn = DB_connect(global_loop, NULL, str_global_pg_conn_string,
		str_global_pg_user, strlen(str_global_pg_user), str_global_pg_password, strlen(str_global_pg_password),
		"", connect_cb);

	ev_run(global_loop, 0);

	return 0;
error:
	if (global_loop != NULL) {
		program_exit();
	}
	return 1;
}

void connect_cb(EV_P, void *cb_data, DB_conn *conn) {
	struct addrinfo hints;
	struct addrinfo *res;

	// This is the function that calls LISTEN
	global_connect_cb(EV_A, cb_data, conn);

	SERROR_CHECK(APNS_connect(global_loop), "Could not connect to APNS");
	
	// Get address info
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	int int_status = getaddrinfo(NULL, str_global_port, &hints, &res);
	SERROR_CHECK(int_status == 0, "getaddrinfo failed: %d (%s)", int_status, gai_strerror(int_status));

	// Get socket to bind
	int_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	SERROR_CHECK(int_sock != -1, "Failed to create socket");

	// Set socket to reuse the address
	int bol_reuseaddr = 1;
	SERROR_CHECK(setsockopt((int)int_sock, SOL_SOCKET, SO_REUSEADDR, &bol_reuseaddr, sizeof(int)) != -1, "setsockopt failed");

	((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr = htonl(INADDR_ANY);
	// Bind socket
	SERROR_CHECK(bind(int_sock, res->ai_addr, (socklen_t)res->ai_addrlen) != -1, "bind failed");

	freeaddrinfo(res);

	// Listen on socket
	SERROR_CHECK(listen(int_sock, 10) != -1, "listen failed");

	// Set socket to NONBLOCK
	setnonblock(int_sock);

	// Create Client List
	_server.list_client = List_create();
	_server.int_sock = int_sock;

	// Add callback for readable data
	ev_io_init(&_server.io, server_cb, int_sock, EV_READ);
	ev_io_start(global_loop, &_server.io);

	fprintf(stderr, SUN_PROGRAM_WORD_NAME " is Started\n");

	return;
error:
	if (global_loop != NULL) {
		program_exit();
	}
}

void sigint_cb(EV_P, ev_signal *w, int revents) {
	if (EV_A != NULL) {
	} // get rid of unused parameter warning
	if (w != NULL) {
	} // get rid of unused parameter warning
	if (revents != 0) {
	} // get rid of unused parameter warning
	program_exit();
}

void program_exit() {
	fprintf(stderr, SUN_PROGRAM_UPPER_NAME" IS SHUTTING DOWN\n");
	if (global_loop != NULL) {
		ev_io_stop(global_loop, &_server.io);
		ev_signal_stop(global_loop, &sigint_watcher);
		ev_signal_stop(global_loop, &sigterm_watcher);
		
		if (_server.list_client != NULL) {
			while (List_first(_server.list_client) != NULL) {
				client_close_immediate(List_first(_server.list_client));
			}
			List_destroy(_server.list_client);
			_server.list_client = NULL;
		}

		if (que_registration != NULL) {
			while (Queue_peek(que_registration) != NULL) {
				Registration_action *action = Queue_recv(que_registration);
				
				SFREE(action->str_id);
				SFREE(action);
			}
			Queue_destroy(que_registration);
			que_registration = NULL;
		}

		if (que_notification != NULL) {
			while (Queue_peek(que_notification) != NULL) {
				char *str_notify = Queue_recv(que_notification);
				SFREE(str_notify);
			}
			Queue_destroy(que_notification);
			que_notification = NULL;
		}

		SFREE(str_global_port);
		SFREE(str_global_error);
		SFREE(str_global_config_file);
		SFREE(str_global_log_level);

		SFREE(str_global_device_table);
		SFREE(str_global_notification_table);
		SFREE(str_global_account_table);
		SFREE(str_global_account_table_pk_column);
		SFREE(str_global_account_table_guid_column);
		SFREE(str_global_listen_channel);
		SFREE(str_global_pg_user);
		SFREE(str_global_pg_password);
		SFREE(str_global_pg_host);
		SFREE(str_global_pg_port);
		SFREE(str_global_pg_database);
		SFREE(str_global_pg_sslmode);

		SFREE(str_global_query_validate_guid);
		SFREE(str_global_query_delete_device);
		SFREE(str_global_query_insert_device);
		SFREE(str_global_query_get_notification);

		SFREE(str_global_apns_cert);

		SFREE(str_global_pg_conn_string);
		
		APNS_disconnect(global_loop);

		EVP_cleanup();
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
		ERR_remove_thread_state(NULL);
#endif

		if (global_registration_watcher) {
			ev_check_stop(global_loop, global_registration_watcher);
			SFREE(global_registration_watcher);
		}
		if (global_notification_queue_watcher) {
			ev_check_stop(global_loop, global_notification_queue_watcher);
			SFREE(global_notification_queue_watcher);
		}
		if (global_notify_watcher) {
			ev_io_stop(global_loop, global_notify_watcher);
			SFREE(global_notify_watcher);
		}

		DB_finish(global_conn);
		ev_break(global_loop, EVBREAK_ALL);
		ev_loop_destroy(global_loop);
		global_loop = NULL;

		close(int_sock);
	}
	DB_finish_framework();
}
