#pragma once

#include <stdbool.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <arpa/inet.h>
#include <ev.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include "common_client_struct.h"
#include "db_framework.h"

#include "common_server.h"
#include "common_websocket.h"
#include "registration.h"

// This function is run when we have data on the client socket
//
// If we haven't received the headers yet, it reads them
//
// When we have all of the headers, it figures out if it is a plain http request, or a websocket handshake
// In the case of an http request, it returns an error, because this is strictly a WS service
// In the case of a handshake, we handle it
//
// If we know we already have the handshake, we read a frame with client_frame_cb
// as the callback
void client_cb(EV_P, ev_io *w, int revents);

// This function parses a websocket frame into a request structure and adds it to the queue
void client_frame_cb(EV_P, WSFrame *frame);

// This macro makes it easier to debug the socket_is_open function
#define socket_is_open(S)                                                                                                        \
	({                                                                                                                           \
		SDEBUG("Checking if socket %d is open", S);                                                                              \
		_socket_is_open(S);                                                                                                      \
	})

// This function checks to see if the given socket is still open
bool _socket_is_open(int int_sock);

// TODO: merge these functions
// This function closes the socket to the browser and calls client_close_immediate
bool client_close(struct sock_ev_client *client);
// This function free()s a client struct and it's members
void client_close_immediate(struct sock_ev_client *client);

#define SERROR_CLIENT_CLOSE_NORESPONSE(C)                                                                                        \
	SDEBUG("Closing client %p", C);                                                                                              \
	SERROR_CHECK_NORESPONSE(client_close(C), "Error closing Client");                                                            \
	C = NULL;
#define SERROR_CLIENT_CLOSE(C)                                                                                                   \
	SDEBUG("Closing client %p", C);                                                                                              \
	SERROR_CHECK(client_close(C), "Error closing Client");                                                                       \
	C = NULL;
#define SFINISH_CLIENT_CLOSE(C)                                                                                                  \
	SDEBUG("Closing client %p", C);                                                                                              \
	SFINISH_CHECK(client_close(C), "Error closing Client");                                                                      \
	C = NULL;
