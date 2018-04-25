#include "apns.h"

APNS_mode int_global_apns_mode = APNS_MODE_PRODUCTION;
char *str_global_apns_cert = NULL;

#define MAKE_NV(NAME, VALUE, VALUELEN)                                         \
{                                                                            \
(uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, VALUELEN,             \
	NGHTTP2_NV_FLAG_NONE                                                   \
}

#define MAKE_NV2(NAME, VALUE)                                                  \
{                                                                            \
(uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,    \
	NGHTTP2_NV_FLAG_NONE                                                   \
}

#define ARRLEN(x) (sizeof(x) / sizeof(x[0]))

http2_session_data *apns_session = NULL;
SSL_CTX *apns_ssl_ctx = NULL;
PKCS12 *pkcs12_cert = NULL;
X509 *cert = NULL;
EVP_PKEY *private_key = NULL;

SSL_CTX *create_ssl_ctx(void) {
	const SSL_METHOD *method = TLSv1_2_client_method();
	FILE *pkcs12_file = NULL;
	SSL_CTX *ssl_ctx = NULL;

	SERROR_CHECK(method != NULL, "Could not create SSL/TLS method: %s",
			ERR_error_string(ERR_get_error(), NULL));

	ssl_ctx = SSL_CTX_new(method);
	SERROR_CHECK(ssl_ctx != NULL, "Could not create SSL/TLS context: %s",
			ERR_error_string(ERR_get_error(), NULL));

	SSL_CTX_set_options(ssl_ctx,
		SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
		SSL_OP_NO_COMPRESSION |
		SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
	
	SSL_CTX_set_cipher_list(ssl_ctx, "EECDH+AESGCM:EDH+AESGCM:AES256+EECDH:AES256+EDH");

	#if OPENSSL_VERSION_NUMBER >= 0x10002000L	
	SSL_CTX_set_alpn_protos(ssl_ctx, (const unsigned char *)"\x02h2", 3);
	#endif

	pkcs12_file = fopen(str_global_apns_cert, "r");
	SERROR_CHECK(pkcs12_file != NULL, "Cannot open certificate")

	d2i_PKCS12_fp(pkcs12_file, &pkcs12_cert);
	fclose(pkcs12_file);
	pkcs12_file = NULL;

	SERROR_CHECK(PKCS12_parse(pkcs12_cert, NULL, &private_key, &cert, NULL), "Unable to use specified PKCS#12 file: %s",
					ERR_error_string(ERR_get_error(), NULL));

	PKCS12_free(pkcs12_cert);
	pkcs12_cert = NULL;

	SERROR_CHECK(SSL_CTX_use_certificate(ssl_ctx, cert), "Unable to use specified PKCS#12 file: %s",
					ERR_error_string(ERR_get_error(), NULL));

	SERROR_CHECK(SSL_CTX_use_PrivateKey(ssl_ctx, private_key), "Unable to use specified PKCS#12 file: %s",
					ERR_error_string(ERR_get_error(), NULL));
	EVP_PKEY_free(private_key);

	return ssl_ctx;
error:
	if (pkcs12_file) {
		fclose(pkcs12_file);
	}
	if (pkcs12_cert) {
		PKCS12_free(pkcs12_cert);
	}
	if (cert) {
		X509_free(cert);
	}
	if (private_key) {
		EVP_PKEY_free(private_key);
	}
	if (ssl_ctx) {
		SSL_CTX_free(ssl_ctx);
	}
	return NULL;
}

SSL *create_ssl(SSL_CTX *ssl_ctx) {
	SSL *ssl = SSL_new(ssl_ctx);
	if (!ssl) {
		SERROR_NORESPONSE("Could not create SSL/TLS session object: %s",
			ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}
	return ssl;
}

http2_stream_data *create_http2_stream_data(uint16_t int_port, char *str_authority, size_t int_authority_len) {
	http2_stream_data *ret;
	SERROR_SALLOC(ret, sizeof(http2_stream_data));

	ret->int_port = int_port;

	SERROR_SNCAT(ret->str_authority, &ret->int_authority_len, str_authority, int_authority_len);
	
	ret->int_stream_id = -1;

	return ret;
error:
	if (ret != NULL) {
		SFREE(ret->str_authority);
	}
	SFREE(ret);
	return NULL;
}

void free_http2_stream_data(http2_stream_data *data) {
	if (data != NULL) {
		SFREE(data->str_authority);
		free(data);
	}
}

http2_session_data *create_http2_session_data() {
	return salloc(sizeof(http2_session_data));
}

void free_http2_session_data(EV_P, http2_session_data *data) {
	if (data != NULL) {
		setblock(data->int_sock);
		if (data->ssl) {
			SSL_shutdown(data->ssl);
			SSL_free(data->ssl);
		}
		ev_io_stop(EV_A, &data->io);
		ev_periodic_stop(EV_A, &data->periodic);
		if (data->int_sock) {
			close(data->int_sock);
		}
		free_http2_stream_data(data->stream_data);
		if (data->session) {
			nghttp2_session_del(data->session);
		}
		free(data);
	}
}

int connect_to_server(char *str_host, uint16_t int_port) {
	int int_sock = -1;
	char str_port[10];
	int status;
	struct addrinfo hints;
	struct addrinfo *res;  // will point to the results

	memset(&hints, 0, sizeof hints); // make sure the struct is empty
	hints.ai_family = AF_INET;     // IPv4 
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

	snprintf(str_port, 9, "%d", int_port);
	SERROR_CHECK((status = getaddrinfo(str_host, str_port, &hints, &res)) == 0, "Could not get address info: %s\n", gai_strerror(status));

	int_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	SERROR_CHECK(int_sock != -1, "Could not create socket");
	SERROR_CHECK(connect(int_sock, res->ai_addr, (socklen_t)res->ai_addrlen) != -1, "Could not connect to host: %s", str_host);
	
	return int_sock;
error:
	freeaddrinfo(res);
	if (int_sock != -1) {
		close(int_sock);
	}
	return -1;
}

bool session_send(http2_session_data *session_data) {
	int rv = nghttp2_session_send(session_data->session);
	SERROR_CHECK(rv == 0, "Fatal error: %s", nghttp2_strerror(rv));
	return true;
error:
	return false;
}

ssize_t send_callback(nghttp2_session *session, const uint8_t *data, size_t length, int flags, void *user_data) {
	http2_session_data *session_data = (http2_session_data *)user_data;
	(void)session;
	(void)flags;
	
	ssize_t int_write_len = SSL_write(session_data->ssl, data, (int)length);

	if (int_write_len == SSL_ERROR_WANT_READ) {
		ev_io_stop(global_loop, &session_data->io);
		ev_io_set(&session_data->io, session_data->int_sock, EV_READ);
		ev_io_start(global_loop, &session_data->io);

		return NGHTTP2_ERR_WOULDBLOCK;
	} else if (int_write_len == SSL_ERROR_WANT_WRITE) {
		ev_io_stop(global_loop, &session_data->io);
		ev_io_set(&session_data->io, session_data->int_sock, EV_WRITE);
		ev_io_start(global_loop, &session_data->io);

		return NGHTTP2_ERR_WOULDBLOCK;
	} else if (int_write_len < 0) {
		SERROR("SSL error");
	}
	
	return (ssize_t)int_write_len;
error:
	return 0;
}

int on_begin_headers_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data) {
	(void)user_data;
	(void)session;

	switch (frame->hd.type) {
		case NGHTTP2_HEADERS:
		if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE && apns_session->stream_data->int_stream_id == frame->hd.stream_id) {
			SINFO("Response headers for stream ID=%d:", frame->hd.stream_id);
		}
		break;
	}
	return 0;
}

int on_header_callback(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data) {
	APNS_notification_response *response = nghttp2_session_get_stream_user_data(session, frame->hd.stream_id);
	(void)user_data;
	http2_header *header = NULL;
	(void)session;
	(void)flags;

	switch (frame->hd.type) {
		case NGHTTP2_HEADERS:
		if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE && apns_session->stream_data->int_stream_id == frame->hd.stream_id) {
			SERROR_SALLOC(header, sizeof(http2_header));
			SERROR_SNCAT(header->str_name, &header->int_name_len, name, namelen);
			SERROR_SNCAT(header->str_value, &header->int_value_len, value, valuelen);
			SINFO("response: %p", response);
			SINFO("response->arr_header: %p", response->arr_header);
			DArray_push(response->arr_header, header);
			header = NULL;

			break;
		}
	}
	return 0;
error:
	if (header != NULL) {
		SFREE(header->str_name);
		SFREE(header->str_value);
		SFREE(header);
	}
	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame, void *user_data) {
	(void)user_data;
	(void)session;

	switch (frame->hd.type) {
		case NGHTTP2_HEADERS:
		if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE && apns_session->stream_data->int_stream_id == frame->hd.stream_id) {
			SINFO("All headers received");
		}
		break;
	}
	return 0;
}

int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len, void *user_data) {
	APNS_notification_response *response = nghttp2_session_get_stream_user_data(session, stream_id);
	(void)user_data;
	(void)session;
	(void)flags;

	if (apns_session->stream_data->int_stream_id == stream_id) {
		SERROR_SNFCAT(response->str_payload, &response->int_payload_len, data, len);
	}
	return 0;
error:
	return NGHTTP2_ERR_CALLBACK_FAILURE;
}

int on_stream_close_callback(nghttp2_session *session, int32_t stream_id, uint32_t error_code, void *user_data) {
	APNS_notification_response *response = nghttp2_session_get_stream_user_data(session, stream_id);
	(void)user_data;
	(void)session;

	if (apns_session->stream_data->int_stream_id == stream_id) {
		SINFO("Stream %d closed with error_code=%u", stream_id, error_code);
		if (response->cb != NULL) {
			response->cb(global_loop, response, response->cb_data);
		}
		APNS_free_notification_response(response);

		// APNS doesn't allow multiple streams
		//rv = nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);
		//if (rv != 0) {
		//	return NGHTTP2_ERR_CALLBACK_FAILURE;
		//}
	}
	return 0;
}

void read_write_cb(EV_P, ev_io *w, int revents) {
	http2_session_data *session_data = (http2_session_data *)w;
	ssize_t int_read_len;
	ssize_t int_ng_read_len;
	uint8_t *buffer = NULL;
	unsigned long ssl_err;
	char ssl_err_buf[256];

	//if (revents & EV_READ) {
		if (nghttp2_session_want_read(session_data->session) != 0) {
			// read a bunch of stuff, then pass it to nghttp2
			SERROR_SALLOC(buffer, 4096);
			int_read_len = SSL_read(session_data->ssl, buffer, 4096);
	
			if (int_read_len == SSL_ERROR_WANT_READ) {
				ev_io_stop(EV_A, w);
				ev_io_set(w, w->fd, EV_READ);
				ev_io_start(EV_A, w);
			} else if (int_read_len == SSL_ERROR_WANT_WRITE) {
				ev_io_stop(EV_A, w);
				ev_io_set(w, w->fd, EV_WRITE);
				ev_io_start(EV_A, w);
			} else if (int_read_len < 0 && errno != EPIPE) {
				SERROR("SSL error");
			} else if (int_read_len <= 0) {
				// reconnect
				APNS_disconnect(global_loop);
				SERROR_CHECK(APNS_connect(global_loop), "Could not reconnect to APNS");
				SFREE(buffer);
				return;
			} else {
				int_ng_read_len = nghttp2_session_mem_recv(session_data->session, buffer, (size_t)int_read_len);
				SERROR_CHECK(int_ng_read_len >= 0, "nghttp2 could not receive data");
			}
		}

		SERROR_CHECK(session_send(session_data), "Could not send data");
	//} else if (revents & EV_WRITE) {
		SERROR_CHECK(session_send(session_data), "Could not send data");

		if (nghttp2_session_want_write(session_data->session) == 0 ||
			nghttp2_session_want_read(session_data->session) != 0) {
			ev_io_stop(EV_A, w);
			ev_io_set(w, w->fd, EV_READ);
			ev_io_start(EV_A, w);
		}
	//}

	SINFO("revents: %d", revents);
	SINFO("revents & EV_READ: %d", revents & EV_READ);
	SINFO("revents & EV_WRITE: %d", revents & EV_WRITE);

	SFREE(buffer);
	return;
error:
	while ((ssl_err = ERR_get_error()) != 0) { 
		ERR_error_string_n(ssl_err, ssl_err_buf, 255); 
		SERROR_NORESPONSE("%s\n", ssl_err_buf); 
	}
	SFREE(buffer);
}

bool initialize_nghttp2_session(http2_session_data *session_data) {
	nghttp2_session_callbacks *callbacks;

	nghttp2_session_callbacks_new(&callbacks);
	nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, on_data_chunk_recv_callback);
	nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);
	nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
	nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers_callback);
	SERROR_CHECK(nghttp2_session_client_new(&session_data->session, callbacks, session_data) == 0, "nghttp2_session_client_new");
	nghttp2_session_callbacks_del(callbacks);
	return true;
error:
	return false;
}

bool send_client_connection_header(http2_session_data *session_data) {
	nghttp2_settings_entry iv[1] = {
		{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, 100}};
	int rv;

	// client 24 bytes magic string will be sent by nghttp2 library
	rv = nghttp2_submit_settings(session_data->session, NGHTTP2_FLAG_NONE, iv,
								ARRLEN(iv));
	SERROR_CHECK(rv == 0, "Could not submit SETTINGS: %s", nghttp2_strerror(rv));

	return true;
error:
	return false;
}

ssize_t request_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data) {
	(void)session;
	(void)stream_id;
	(void)user_data;
	(void)length;

	APNS_notification_payload *notification_payload = source->ptr;
	memcpy(buf, notification_payload->str_payload, notification_payload->int_payload_len);
	*data_flags |= NGHTTP2_DATA_FLAG_EOF;
	ssize_t int_length = (ssize_t)notification_payload->int_payload_len;
	SFREE(notification_payload->str_payload);
	SFREE(notification_payload);
	return int_length;
}

bool initiate_http2(http2_session_data *session_data) {
    const unsigned char *alpn = NULL;
    unsigned int alpnlen = 0;
	int val = 1;

	SSL_get0_next_proto_negotiated(session_data->ssl, &alpn, &alpnlen);
	#if OPENSSL_VERSION_NUMBER >= 0x10002000L	
	if (alpn == NULL) {
		SSL_get0_alpn_selected(session_data->ssl, &alpn, &alpnlen);
	}
	#endif

	SERROR_CHECK(alpn == NULL || (alpnlen == 2 && memcmp("h2", alpn, 2) == 0), "h2 is not negotiated");

    setsockopt(session_data->int_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&val, sizeof(val));
    SERROR_CHECK(initialize_nghttp2_session(session_data), "Could not initialize the HTTP/2 connection");
    SERROR_CHECK(send_client_connection_header(session_data), "Could not send client connection header");
	
	SERROR_CHECK(session_send(session_data), "Could not send data");

	return true;
error:
	return false;
}

bool initiate_connection(EV_P, SSL_CTX *ssl_ctx, http2_session_data *session_data) {
	// #if OPENSSL_VERSION_NUMBER >= 0x10100000L	
	// X509_VERIFY_PARAM *param = NULL;
	// #endif
	int status;
	unsigned long ssl_err;
	char ssl_err_buf[256];
	
	session_data->ssl = create_ssl(ssl_ctx);
	SERROR_CHECK(session_data->ssl, "Could not create SSL session");
	// #if OPENSSL_VERSION_NUMBER >= 0x10100000L	
	// param = SSL_get0_param(session_data->ssl);
	// #endif
	
	session_data->int_sock = connect_to_server(session_data->stream_data->str_authority, session_data->stream_data->int_port);
	SERROR_CHECK(session_data->int_sock != -1, "Could not connect to host: %s", session_data->stream_data->str_authority);
	
	//#if OPENSSL_VERSION_NUMBER >= 0x10100000L	
	//X509_VERIFY_PARAM_set_hostflags(param, 0);
	//SERROR_CHECK((status = X509_VERIFY_PARAM_set1_host(param, "*.apple.com", 11)) == 1, "Could not set hostname to validate")
	//
	//SSL_set_verify(session_data->ssl, SSL_VERIFY_PEER, 0);
	//#endif
	SSL_set_fd(session_data->ssl, session_data->int_sock);
	
	SERROR_CHECK((status = SSL_connect(session_data->ssl)) == 1, "Could not initiate SSL connection")
	
	SERROR_CHECK(initiate_http2(session_data), "Could not initiate HTTP/2 connection");

	setnonblock(session_data->int_sock);

	ev_io_init(&session_data->io, read_write_cb, session_data->int_sock, EV_READ | EV_WRITE);
	ev_io_start(EV_A, &session_data->io);

	return true;
error:
	while ((ssl_err = ERR_get_error()) != 0) { 
		ERR_error_string_n(ssl_err, ssl_err_buf, 255); 
		SERROR_NORESPONSE("%s\n", ssl_err_buf); 
	}
	return false;
}

void ping_cb(EV_P, ev_periodic *w, int revents) {
	(void)EV_A;
	(void)w;
	(void)revents;
	nghttp2_submit_ping(apns_session->session, 0, NULL);
	SINFO("PING");
}

bool APNS_connect(EV_P) {
	char *str_authority = NULL;
	size_t int_authority_len;
	
	apns_ssl_ctx = create_ssl_ctx();
	SERROR_CHECK(apns_ssl_ctx != NULL, "Couldn't create SSL Context to connect to APNS");

	if (int_global_apns_mode == APNS_MODE_PRODUCTION) {
		SERROR_SNCAT(str_authority, &int_authority_len, "api.push.apple.com", (size_t)18);
	} else {
		SERROR_SNCAT(str_authority, &int_authority_len, "api.development.push.apple.com", (size_t)30);
	}

	apns_session = create_http2_session_data();
	SERROR_CHECK(apns_session, "Couldn't create APNS session data");
	apns_session->stream_data = create_http2_stream_data(443, str_authority, int_authority_len);
	SERROR_CHECK(apns_session, "Couldn't create APNS stream data");
	
	SFREE(str_authority);
	SERROR_CHECK(initiate_connection(EV_A, apns_ssl_ctx, apns_session), "Could not connect to APNS");

	ev_periodic_init(&apns_session->periodic, ping_cb, 0, 1200, 0);
	ev_periodic_start(EV_A, &apns_session->periodic);

	return true;
error:
	SFREE(str_authority);
	return false;
}

bool APNS_send(EV_P, APNS_notification_cb_t cb, void *cb_data, char *str_apns_id, size_t int_apns_id_len, char *str_payload, size_t int_payload_len) {
	(void)cb;
	(void)cb_data;
	(void)EV_A;
	char *str_path = NULL;
	size_t int_path_len;
	APNS_notification_response *response = NULL;

	int32_t int_stream_id;
	http2_stream_data *stream_data = apns_session->stream_data;
	APNS_notification_payload *notification_payload = NULL;
	nghttp2_data_provider data_prd;

	SERROR_SALLOC(response, sizeof(APNS_notification_response));
	response->arr_header = DArray_create(sizeof(http2_header *), 5);
	SINFO("response: %p", response);
	SINFO("response->arr_header: %p", response->arr_header);
	SERROR_CHECK(response->arr_header != NULL, "DArray_create failed");
	response->cb = cb;
	response->cb_data = cb_data;
	SERROR_SNFCAT(response->str_payload, &response->int_payload_len, "", (size_t)0);
	SERROR_SNCAT(str_path, &int_path_len, "/3/device/", (size_t)10, str_apns_id, int_apns_id_len);
	
	nghttp2_nv hdrs[] = {
		MAKE_NV2(":method", "POST"),
		MAKE_NV(":authority", stream_data->str_authority, stream_data->int_authority_len),
		MAKE_NV(":path", str_path, int_path_len)
	};

	SERROR_SALLOC(notification_payload, sizeof(APNS_notification_payload));

	SERROR_SNCAT(notification_payload->str_payload, &notification_payload->int_payload_len, str_payload, int_payload_len);

	data_prd.source.ptr = notification_payload;
	data_prd.read_callback = request_read_callback;

	int_stream_id = nghttp2_submit_request(apns_session->session, NULL, hdrs, ARRLEN(hdrs), &data_prd, response);
	SERROR_CHECK(int_stream_id >= 0, "Could not submit HTTP request: %s", nghttp2_strerror(int_stream_id));
  
	stream_data->int_stream_id = int_stream_id;

	SERROR_CHECK(session_send(apns_session), "Could not send data");

	SFREE(str_path);

	return true;
error:
	APNS_free_notification_response(response);
	if (notification_payload) {
		SFREE(notification_payload->str_payload);
	}
	SFREE(notification_payload);
	SFREE(str_path);
	return false;
}

void APNS_free_notification_response(APNS_notification_response *response) {
	if (response != NULL) {
		size_t int_i = 0;
		while (int_i < DArray_count(response->arr_header)) {
			http2_header *header = DArray_get(response->arr_header, int_i);
			SFREE(header->str_name);
			SFREE(header->str_value);

			int_i += 1;
		}
		DArray_clear_destroy(response->arr_header);
		response->arr_header = NULL;
		SFREE(response->str_payload);
		SFREE(response);
	}
}

bool APNS_disconnect(EV_P) {
	if (apns_session) {
		free_http2_session_data(EV_A, apns_session);
		SSL_CTX_free(apns_ssl_ctx);

		if (pkcs12_cert) {
			PKCS12_free(pkcs12_cert);
		}
		X509_free(cert);
		//EVP_PKEY_free(private_key);
	}

	return true;
}
