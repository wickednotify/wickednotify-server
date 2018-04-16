#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ev.h>

#include "db_framework.h"

#include <unistd.h>
#include "common_client.h"
#include "common_config.h"
#include "util_darray.h"
#include "util_request.h"

// 1MB
#define MAX_BUFFER 1048576

extern struct ev_loop *global_loop;
extern struct sock_ev_serv _server;

// This function sets the non-blocking flag on a socket
int setnonblock(int int_sock);

/// This function unsets the non-blocking flag on a socket
int setblock(int int_sock);
// This function is called every time the listening socket has a connection ready
// 
// It will create a client struct for every accept()ed socket.
void server_cb(EV_P, ev_io *w, int revents);

struct sock_ev_serv {
	ev_io io;
	int int_sock;
	List *list_client;
};

void program_exit();
