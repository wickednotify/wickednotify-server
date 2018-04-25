#pragma once

#include "util_darray.h"
#include <ev.h>

typedef struct APNS_notification_response *APNS_notification_responsep;
typedef void (*APNS_notification_cb_t)(EV_P, APNS_notification_responsep response, void *cb_data);

typedef struct {
	char *str_name;
	size_t int_name_len;
	char *str_value;
	size_t int_value_len;
} http2_header;

typedef struct APNS_notification_response {
	DArray *arr_header;
	char *str_payload;
	size_t int_payload_len;
	APNS_notification_cb_t cb;
	void *cb_data;
} APNS_notification_response;

#include "common_server.h"
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <nghttp2/nghttp2.h>

typedef enum {
	APNS_MODE_PRODUCTION = 0,
	APNS_MODE_DEVELOPMENT = 1
} APNS_mode;

extern APNS_mode int_global_apns_mode;
extern char *str_global_apns_cert;

typedef struct {
	uint16_t int_port;

	// Authority is just the host
	char *str_authority;
	size_t int_authority_len;

	int32_t int_stream_id;
} http2_stream_data;

typedef struct {
	ev_io io;
	ev_periodic periodic;
	int int_sock;
	SSL *ssl;
	nghttp2_session *session;
	http2_stream_data *stream_data;
} http2_session_data;

extern http2_session_data *apns_session;
extern SSL_CTX *apns_ssl_ctx;

SSL_CTX *create_ssl_ctx(void);
SSL *create_ssl(SSL_CTX *ssl_ctx);
http2_stream_data *create_http2_stream_data(uint16_t int_port, char *str_authority, size_t int_authority_len);
void free_http2_stream_data(http2_stream_data *data);
http2_session_data *create_http2_session_data();
void free_http2_session_data(EV_P, http2_session_data *data);
int connect_to_server(char *str_host, uint16_t int_port);
bool session_send(http2_session_data *session_data);
ssize_t send_callback(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data);
int on_begin_headers_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data);
int on_header_callback(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data);
int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data);
int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data);
int on_stream_close_callback(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data);
void read_write_cb(EV_P, ev_io *w, int revents);
bool initialize_nghttp2_session(http2_session_data *session_data);
bool send_client_connection_header(http2_session_data *session_data);
ssize_t request_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data);
bool initiate_http2(http2_session_data *session_data);
bool initiate_connection(EV_P, SSL_CTX *ssl_ctx, http2_session_data *session_data);

typedef struct {
	char *str_payload;
	size_t int_payload_len;
} APNS_notification_payload;

bool APNS_connect(EV_P);
bool APNS_send(EV_P, APNS_notification_cb_t cb, void *cb_data, char *str_apns_id, size_t int_apns_id_len, char *str_payload, size_t int_payload_len);
void APNS_free_notification_response(APNS_notification_response *response);
bool APNS_disconnect(EV_P);
