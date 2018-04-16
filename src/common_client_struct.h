#pragma once

#include "db_framework.h"
#include "util_darray.h"
#include "util_list_queue.h"
#include <arpa/inet.h>
#include <ev.h>

typedef bool (*sock_ev_client_query_callback_function)(EV_P, PGresult *, ExecStatusType, struct sock_ev_client_request *);

struct sock_ev_client_query_callback_watcher {
	ev_io io;
	struct sock_ev_client_request *client_request;
	sock_ev_client_query_callback_function callback;
};

#ifndef SOCKET
#define UNDEF_SOCKET
#define SOCKET int
#endif

/*
This header is just a bunch of structs to represent the client
*/

struct sock_ev_client_cnxn {
	ev_io io;

	struct sock_ev_client *parent;
};

struct sock_ev_client_reconnect_timer {
	ev_prepare prepare;

	ev_tstamp close_time;

	struct sock_ev_client *parent;
};

struct sock_ev_client_timeout_prepare {
	ev_prepare prepare;

	ev_tstamp close_time;

	struct sock_ev_client *parent;
};
#define client_timeout_prepare_free(A)                                                                                           \
	_client_timeout_prepare_free(A);                                                                                             \
	A = NULL;
void _client_timeout_prepare_free(struct sock_ev_client_timeout_prepare *client_timeout_prepare);

typedef struct WSFrame {
	uint64_t int_length;
	uint16_t int_orig_length;
	int8_t int_opcode;
	bool bol_fin;
	bool bol_mask;
	char *str_mask;
	char *str_message;
	struct sock_ev_client *parent;
	void (*cb)(EV_P, struct WSFrame *);
} WSFrame;

struct sock_ev_client_message {
	ev_io io;
	WSFrame *frame;
	bool bol_have_header;
	uint8_t int_ioctl_count;
	uint64_t int_message_header_length;
	uint64_t int_length;
	uint64_t int_written;
	uint64_t int_position;
	uint64_t int_message_num;
};

struct sock_ev_client {
	ev_io io;

	connect_cb_t connect_cb;

	bool bol_is_open;
	bool bol_socket_is_open;
	bool bol_ssl_handshake;

	char *str_profile_id;
	size_t int_profile_id_len;

	char *str_username;
	char *str_database;
	char *str_connname;
	char *str_connname_folder;
	char *str_conn;

	size_t int_username_len;
	size_t int_database_len;
	size_t int_connname_len;
	size_t int_connname_folder_len;
	size_t int_conn_len;

	ListNode *node;
	char str_client_ip[INET_ADDRSTRLEN];
	bool bol_handshake;

	bool bol_public;

	bool bol_upload;
	char *str_boundary;
	size_t int_boundary_length;

	char *str_request;
	char *str_response;

	int int_ev_sock;
	SOCKET int_sock;
	struct sock_ev_serv *server;

	char *str_message;
	size_t int_message_len;
	char *str_notice;

	Queue *que_message;

	struct sock_ev_client_timeout_prepare *client_timeout_prepare;
	struct sock_ev_client_reconnect_timer *client_reconnect_timer;

	size_t int_request_len;
};

enum {
	REQ_BEGIN = 0,
	REQ_COMMIT = 1,
	REQ_ROLLBACK = 2,
	REQ_INFO = 3,
	REQ_ACTION = 4,
	REQ_ACCEPT = 5,
	REQ_AUTH = 6,
	REQ_STANDARD = 7
};


typedef struct sock_ev_client_request_data *sock_ev_client_request_datap;
typedef void(*sock_ev_client_request_data_free_func)(sock_ev_client_request_datap);
struct sock_ev_client_request_data {
	sock_ev_client_request_data_free_func free;
};

struct sock_ev_client_copy_check {
	ev_check check;

	DB_result *res;
	size_t int_i;
	size_t int_len;
	ssize_t int_response_len;
	ssize_t int_written;
	char *str_response;
	struct sock_ev_client_request *client_request;
};

#ifdef UNDEF_SOCKET
#undef SOCKET
#endif
