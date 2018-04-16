// Hark ye onlooker: Adding UTIL_DEBUG to this file slows it down considerably. You have been warned.
#include "common_client.h"

// **************************************************************************************
// **************************************************************************************
// ************************************** READ READY ************************************
// **************************************************************************************
// **************************************************************************************

// client socket has data
void client_cb(EV_P, ev_io *w, int revents) {
	if (revents != 0) {
	} // get rid of unused parameter warning
	struct sock_ev_client *client = (struct sock_ev_client *)w;

	unsigned char *str_request = NULL;
	bool bol_request_finished = false;
	char *str_buffer = NULL;
	ssize_t int_len = 0;
	size_t int_content_length = 0;
	size_t int_response_len = 0;
	size_t int_uri_length = 0;

	SDEFINE_VAR_ALL(str_response, str_conninfo, str_query);
	SDEFINE_VAR_MORE(str_ip_address, str_upper_request, str_conn_index, str_uri_temp);

	SERROR_SALLOC(str_buffer, MAX_BUFFER + 1);

	SDEBUG("revents: %x", revents);
	SDEBUG("EV_READ: %s", revents & EV_READ ? "true" : "false");
	SDEBUG("EV_WRITE: %s", revents & EV_WRITE ? "true" : "false");
	SDEBUG("client->bol_handshake: %s", client->bol_handshake ? "true" : "false");

	// we haven't done the handshake yet
	if (client->bol_handshake == false) {
		errno = 0;
		int_len = read(client->int_sock, str_buffer, MAX_BUFFER);
		SDEBUG("client->int_request_len: %zu", client->int_request_len);
		SDEBUG("int_len: %zd", int_len);
		SWARN_CHECK(
			client->int_request_len > 0 || int_len != 0, "Libev said EV_READ, but there is nothing to read. Closing socket");

		if (int_len >= 0) {
			if ((client->int_request_len == 0 || int_len == 0) && bstrstr(str_buffer, (size_t)int_len, "HTTP", 4) == NULL) {
				SERROR("Someone is trying to connect with TLS!");
			}

			SERROR_SREALLOC(client->str_request, (size_t)((ssize_t)client->int_request_len + int_len + 1));
			memcpy(client->str_request + client->int_request_len, str_buffer, (size_t)int_len);
			client->int_request_len += (size_t)int_len;
			client->str_request[client->int_request_len] = '\0';
		} else if (int_len < 0) {
			SERROR("read() failed");
		}

		if (client->bol_upload == false) {
			SERROR_SALLOC(str_upper_request, client->int_request_len + 1);
			memcpy(str_upper_request, client->str_request, client->int_request_len);
			bstr_toupper(str_upper_request, client->int_request_len);
			client->bol_upload =
				(bstrstr(str_upper_request, client->int_request_len, "CONTENT-TYPE: MULTIPART/FORM-DATA; BOUNDARY=",
					 strlen("CONTENT-TYPE: MULTIPART/FORM-DATA; BOUNDARY=")) != NULL);
		}

		char *ptr_content_length =
			bstrstr(str_upper_request, client->int_request_len, "CONTENT-LENGTH", strlen("CONTENT-LENGTH"));
		if (client->bol_upload == true) {
			if (str_upper_request == NULL) {
				SERROR_SALLOC(str_upper_request, client->int_request_len + 1);
				memcpy(str_upper_request, client->str_request, client->int_request_len);
				bstr_toupper(str_upper_request, client->int_request_len);
			}
			if (client->str_boundary == NULL) {
				char *boundary_ptr =
					bstrstr(str_upper_request, client->int_request_len, "CONTENT-TYPE: MULTIPART/FORM-DATA; BOUNDARY=",
						strlen("CONTENT-TYPE: MULTIPART/FORM-DATA; BOUNDARY=")) +
					44;
				boundary_ptr = client->str_request + (boundary_ptr - str_upper_request);
				char *boundary_end_ptr =
					strchr(boundary_ptr, 13) != 0 ? strchr(boundary_ptr, '\015') : strchr(boundary_ptr, '\012');
				if (boundary_end_ptr != NULL) {
					client->int_boundary_length = (size_t)(boundary_end_ptr - boundary_ptr);
					SERROR_SALLOC(client->str_boundary, (size_t)client->int_boundary_length + 3); // extra and null byte
					memcpy(client->str_boundary, boundary_ptr, client->int_boundary_length);
					client->str_boundary[client->int_boundary_length + 0] = '-';
					client->str_boundary[client->int_boundary_length + 1] = '-';
					client->str_boundary[client->int_boundary_length + 2] = '\0';
					client->int_boundary_length += 2;
				} else {
					bol_request_finished = false;
				}
			}

			if (client->str_boundary != NULL) {
				if (bstrstr(client->str_request + client->int_request_len - int_len, (size_t)int_len, client->str_boundary,
						client->int_boundary_length) == NULL) {
					bol_request_finished = false;
				} else {
					bol_request_finished = true;
				}
			}

		} else if (bstrstr(str_upper_request, client->int_request_len, "CONTENT-LENGTH", strlen("CONTENT-LENGTH")) != NULL) {
			ptr_content_length += 14;
			while (*ptr_content_length != 0 && (*ptr_content_length < '0' || *ptr_content_length > '9')) {
				ptr_content_length += 1;
			}
			int_content_length = (size_t)strtol(ptr_content_length, NULL, 10);
			char *ptr_temp_unix = bstrstr(client->str_request, client->int_request_len, "\012\012", 2);
			char *ptr_temp_dos = bstrstr(client->str_request, client->int_request_len, "\015\012\015\012", 4);
			char *ptr_temp_mac = bstrstr(client->str_request, client->int_request_len, "\015\015", 2);

			char *ptr_temp = ptr_temp_unix != NULL ? ptr_temp_unix : ptr_temp_dos != NULL ? ptr_temp_dos : ptr_temp_mac;
			if (ptr_temp == NULL) {
				bol_request_finished = false;
			} else {
				while (*ptr_temp != 0 && (*ptr_temp == '\012' || *ptr_temp == '\015')) {
					ptr_temp += 1;
				}
				bol_request_finished = (client->int_request_len - (size_t)(ptr_temp - client->str_request)) == int_content_length;
			}

		} else {
			char *ptr_temp_unix = bstrstr(client->str_request, client->int_request_len, "\012\012", 2);
			char *ptr_temp_dos = bstrstr(client->str_request, client->int_request_len, "\015\012\015\012", 4);
			char *ptr_temp_mac = bstrstr(client->str_request, client->int_request_len, "\015\015", 2);

			char *ptr_temp = ptr_temp_unix != NULL ? ptr_temp_unix : ptr_temp_dos != NULL ? ptr_temp_dos : ptr_temp_mac;
			bol_request_finished = ptr_temp != NULL;
		}

		if (bol_request_finished) {
			size_t int_ip_address_len = 0;
			str_ip_address = request_header(client->str_request, client->int_request_len, "x-forwarded-for", &int_ip_address_len);
			if (str_ip_address != NULL && int_ip_address_len < (INET_ADDRSTRLEN - 1)) {
				SINFO("str_ip_address: %s", str_ip_address);
				memcpy(client->str_client_ip, str_ip_address, int_ip_address_len);

				// IIS adds the client port to the X-Forwarded-For header
				// remove it for convenience
				char *ptr_temp = bstrstr(client->str_client_ip, int_ip_address_len, ":", 1);
				if (ptr_temp != NULL) {
					*ptr_temp = 0;
					int_ip_address_len = (size_t)(ptr_temp - client->str_client_ip);
				}
				SFREE(str_ip_address);
			} else {
				bol_error_state = false;
				SFREE(str_global_error);
			}


			size_t int_i = 0, int_header_len = client->int_request_len;
			SDEBUG("client->bol_upload: %s", client->bol_upload ? "true" : "false");
			if (client->bol_upload) {
				char *ptr_temp_unix = bstrstr(client->str_request, client->int_request_len, "\012\012", 2);
				char *ptr_temp_dos = bstrstr(client->str_request, client->int_request_len, "\015\012\015\012", 4);
				char *ptr_temp_mac = bstrstr(client->str_request, client->int_request_len, "\015\015", 2);

				ptr_temp_unix = ptr_temp_unix != NULL ? ptr_temp_unix : client->str_request + client->int_request_len;
				ptr_temp_dos = ptr_temp_dos != NULL ? ptr_temp_dos : client->str_request + client->int_request_len;
				ptr_temp_mac = ptr_temp_mac != NULL ? ptr_temp_mac : client->str_request + client->int_request_len;

				char *ptr_temp = ptr_temp_unix < ptr_temp_dos ? ptr_temp_unix : ptr_temp_dos;
				ptr_temp = ptr_temp < ptr_temp_mac ? ptr_temp : ptr_temp_mac;

				int_header_len = (size_t)(ptr_temp - client->str_request);
			}
			SDEBUG("int_header_len: %d", int_header_len);
			while (int_i < int_header_len) {
				// Check if the character is not printable
				if ((unsigned char)(client->str_request[int_i]) != 9 && (unsigned char)(client->str_request[int_i]) != 10 &&
					(unsigned char)(client->str_request[int_i]) != 13 &&
					((unsigned char)(client->str_request[int_i]) < 32 ||
						((unsigned char)(client->str_request[int_i]) > 126 &&
							(unsigned char)(client->str_request[int_i]) < 160))) {
					char str_int_code[4] = {0};
					snprintf(str_int_code, 3, "%d", (unsigned char)(client->str_request[int_i]));
					SERROR_SNCAT(str_response, &int_response_len,
						"HTTP/1.1 400 Bad Request\015\012Connection: close\015\012\015\012An invalid character with the code '", (size_t)83,
						str_int_code, strlen(str_int_code),
						"' was found", (size_t)11);

					SBFREE_PWORD(str_upper_request, client->int_request_len);

					if ((int_len = write(client->int_sock, str_response, strlen(str_response))) < 0) {
						SERROR_NORESPONSE("write() failed");
					}

					SERROR_CLIENT_CLOSE(client);

					SFREE(str_request);
					SFREE(str_buffer);
					SFREE_PWORD_ALL();
					bol_error_state = false;
					return;
				}
				int_i += 1;
			}
			if (str_upper_request == NULL) {
				SERROR_SALLOC(str_upper_request, client->int_request_len + 1);
				memcpy(str_upper_request, client->str_request, client->int_request_len);
				bstr_toupper(str_upper_request, client->int_request_len);
			}
			if (bstrstr(str_upper_request, client->int_request_len, "SEC-WEBSOCKET-KEY", strlen("SEC-WEBSOCKET-KEY")) != NULL) {
				str_uri_temp = str_uri_path(client->str_request, client->int_request_len, &int_uri_length);
				SERROR_CHECK(str_uri_temp != NULL, "str_uri_path failed");

				SDEBUG("websocket request");

				if (client->que_message == NULL) {
					client->que_message = Queue_create();
				}

				////HANDSHAKE
				SERROR_CHECK(
					(str_response = WS_handshakeResponse(client->str_request, client->int_request_len, &int_response_len)) != NULL, "Error getting handshake response");

				SDEBUG("str_response       : %s", str_response);
				SDEBUG("client->str_request: %s", client->str_request);
				// return handshake response
				if ((int_len = write(client->int_sock, str_response, int_response_len)) < 0) {
					SERROR_CLIENT_CLOSE(client);
					SERROR("write() failed");
				}
				SFREE(str_response);

				SERROR_SNCAT(str_response, &int_response_len, "CONNECTED", (size_t)9);

				SERROR_CHECK(
					WS_sendFrame(EV_A, client, true, 0x01, str_response, int_response_len), "Failed to send message");

				client->bol_handshake = true;

			} else {
				char *_str_response = "This service does not support HTTP, only websockets";
				char str_length[50];
				snprintf(str_length, 50, "%zu", strlen(_str_response));				
				SERROR_SNCAT(str_response, &int_response_len,
					"HTTP/1.1 500 Internal Server Error\015\012Content-Length: ", (size_t)52,
					str_length, strlen(str_length),
					"\015\012Connection: close\015\012\015\012", (size_t)23,
					_str_response, strlen(_str_response));
				if (write(client->int_sock, str_response, int_response_len) < 0) {
					SERROR_NORESPONSE("write() failed");
				}
				if (str_response == client->str_response) {
					client->str_response = NULL;
				}
				SFREE(str_response);

				SERROR_CLIENT_CLOSE(client);
			}
		}

		// handshake already done, let's get down to business
	} else if (client->bol_handshake == true) {
		SDEBUG("Reading a frame");
		WS_readFrame(EV_A, client, client_frame_cb);
	}

	SDEBUG("Readable callback end");

	SFREE(str_request);
	SFREE(str_buffer);
	if (client != NULL) {
		SBFREE_PWORD(str_upper_request, client->int_request_len);
	}
	SFREE_PWORD_ALL();
	bol_error_state = false;
	return;

error:
	bol_error_state = false;
	if (client != NULL) {
		// This prevents an infinite loop if SERROR_CLIENT_CLOSE fails
		struct sock_ev_client *_client = client;
		client = NULL;

		SERROR_CLIENT_CLOSE(_client);
	}
	SFREE(str_request);
	SFREE(str_buffer);
	if (client != NULL) {
		SBFREE_PWORD(str_upper_request, client->int_request_len);
	}
	SFREE_PWORD_ALL();
}

// **************************************************************************************
// **************************************************************************************
// ************************************* FRAME READY ************************************
// **************************************************************************************
// **************************************************************************************

void client_frame_cb(EV_P, WSFrame *frame) {
	struct sock_ev_client *client = frame->parent;
	size_t int_old_length = 0;
	Registration_action *action = NULL;

	SDEBUG("Client %p->int_sock == %d", client, client->int_sock);

	// DEBUG("Read a frame");

	ev_io_start(EV_A, &client->io);

	// The specification requires that we disconnect if the client sends an
	// unmasked message
	if (frame->int_opcode == 0x01 && frame->bol_mask == false) {
		SERROR_CLIENT_CLOSE(client);

		WS_freeFrame(frame);

		SERROR("Client sent unmasked message, disconnected.");
	}

	if (frame->bol_fin == false) {
		if (client->str_message == NULL) {
			SERROR_SNCAT(client->str_message, &client->int_message_len,
				"", (size_t)0);
		}
		int_old_length = client->int_message_len;
		client->int_message_len += frame->int_length;
		SERROR_SREALLOC(client->str_message, client->int_message_len + 1);
		memcpy(client->str_message + int_old_length, frame->str_message, frame->int_length);
		client->str_message[client->int_message_len] = 0;

		WS_freeFrame(frame);
		// DEBUG("Concatenated");

		return;

	} else if (frame->int_opcode == 0x00 && frame->bol_fin == true) {
		int_old_length = client->int_message_len;
		client->int_message_len += frame->int_length;
		SERROR_SREALLOC(client->str_message, client->int_message_len + 1);
		memcpy(client->str_message + int_old_length, frame->str_message, frame->int_length);
		client->str_message[client->int_message_len] = 0;

		// DEBUG("Last concatenation");
		SFREE(frame->str_message);

		frame->int_length = client->int_message_len;
		frame->str_message = client->str_message;

		client->int_message_len = 0;
		client->str_message = NULL;
	}

	// opcode 0x08 is never more than one frame and is always less than 126
	// characters
	if (frame->int_opcode == 0x08) {
		SINFO("Got close frame", client);
#ifdef UTIL_DEBUG
		if (frame->int_length > 2) {
			unsigned short int_close_code =
				(unsigned short)((((unsigned short)frame->str_message[0]) << 8) | ((unsigned short)frame->str_message[1]));
			SDEBUG("Client closed connection with code %u for reason: \"%s\"", int_close_code, frame->str_message + 2);
		}
#endif // UTIL_DEBUG
		while (client->que_message->first != NULL) {
			SINFO("client->que_message->first: %p", client->que_message->first);
			struct sock_ev_client_message *client_message = client->que_message->first->value;
			ev_io_stop(EV_A, &client_message->io);
			WSFrame *frame_temp = client_message->frame;
			// This removes this node from the queue, so that the next element we want
			// is the first one
			WS_client_message_free(client_message);
			WS_freeFrame(frame_temp);
		}

		if (client->bol_is_open) {
			SERROR_CHECK(WS_sendFrame(EV_A, client, true, 0x08, frame->str_message, frame->int_length), "Failed to send message");
			SINFO("Sent close frame", client);

			client->bol_is_open = false;
		} else {
			goto error;
		}

		WS_freeFrame(frame);

	} else if (frame->int_opcode == 0x09) {
		SDEBUG("Got  ping frame", client);
		SERROR_CHECK(WS_sendFrame(EV_A, client, true, 0x0A, frame->str_message, frame->int_length), "Failed to send message");
		SDEBUG("Sent pong frame", client);
		WS_freeFrame(frame);

	} else if (frame->bol_fin == true && (frame->int_opcode == 0x02 || frame->int_opcode == 0x01 || frame->int_opcode == 0x00)) {
		if (strncmp(frame->str_message, "GUID=", 5) == 0) {
			SERROR_SALLOC(action, sizeof(Registration_action));

			action->client = client;
			action->type = REG_ACTION_GUID;
			SERROR_SNCAT(action->str_id, &action->int_id_len, frame->str_message + 5, frame->int_length - 5);

			Queue_send(que_registration, action);
			action = NULL;

			SINFO("send GUID");

			increment_idle(EV_A);
			
		} else if (strncmp(frame->str_message, "APNS=", 5) == 0) {
			SERROR_SALLOC(action, sizeof(Registration_action));

			action->client = client;
			action->type = REG_ACTION_APNS;
			SERROR_SNCAT(action->str_id, &action->int_id_len, frame->str_message + 5, frame->int_length - 5);

			Queue_send(que_registration, action);
			action = NULL;
			
			SINFO("send APNS");

			increment_idle(EV_A);
		} else if (strncmp(frame->str_message, "LATEST=", 7) == 0) {
			SERROR_SALLOC(action, sizeof(Registration_action));

			action->client = client;
			action->type = REG_ACTION_LATEST;
			SERROR_SNCAT(action->str_id, &action->int_id_len, frame->str_message + 7, frame->int_length - 7);

			Queue_send(que_registration, action);
			action = NULL;
			
			SINFO("send LATEST");

			increment_idle(EV_A);
		}
	}
	WS_freeFrame(frame);
	bol_error_state = false;
	return;

error:
	WS_freeFrame(frame);

	if (action) {
		SFREE(action->str_id);
		SFREE(action);
	}

	bol_error_state = false;
	if (client) {
		// This prevents an infinite loop if SERROR_CLIENT_CLOSE fails
		struct sock_ev_client *_client = client;
		client = NULL;

		SERROR_CLIENT_CLOSE(_client);
	}
}

// **************************************************************************************
// **************************************************************************************
// *************************** CHECK IF CLIENT IS STILL THERE ***************************
// **************************************************************************************
// **************************************************************************************

bool _socket_is_open(int int_sock) {
	int int_error = 0;
	socklen_t int_len;

	SDEBUG("int_sock == %d", int_sock);

	errno = 0;
	int_len = sizeof(int_error);
	int int_status = getsockopt(int_sock, SOL_SOCKET, SO_ERROR, &int_error, &int_len);

	SWARN_CHECK(int_status == 0, "Error getting socket error code: %d (%s)", errno, strerror(errno));
	SWARN_CHECK(int_error == 0, "Socket error, assuming it is closed: %d (%s), %d (%s)", errno, strerror(errno), int_error,
		strerror(int_error));

	errno = 0;
	bol_error_state = false;
	return true;
error:
	errno = 0;
	bol_error_state = false;
	return false;
}

// **************************************************************************************
// **************************************************************************************
// ********************************** CLOSING A CLIENT **********************************
// **************************************************************************************
// **************************************************************************************

bool client_close(struct sock_ev_client *client) {
	struct sock_ev_client_message *client_message = NULL;

	SINFO("Client %p closing", client);
	if (client->que_message != NULL && client->bol_handshake == true && client->bol_is_open == true) {
		while (client->que_message->first != NULL) {
			client_message = client->que_message->first->value;
			ev_io_stop(global_loop, &client_message->io);
			WSFrame *frame = client_message->frame;
			// This removes this node from the queue, so that the next element we want
			// is the first one
			WS_client_message_free(client_message);
			WS_freeFrame(frame);
		}
		WS_sendFrame(global_loop, client, true, 0x08, "\01\00", 2);
	}
	ev_io_stop(global_loop, &client->io);

	if ((client->que_message == NULL || client->que_message->first == NULL) && client->bol_socket_is_open == true) {
		if (client->que_message != NULL) {
			Queue_destroy(client->que_message);
			client->que_message = NULL;
		}
		// This must be done NOW, or else the browser will hang
		shutdown(client->int_sock, SHUT_RDWR);
		SDEBUG("Shutdown socket %i", client->int_sock);

		close(client->int_sock);
		SDEBUG("Closed socket %i", client->int_sock);

		errno = 0;
		client->bol_socket_is_open = false;
	}

	client_close_immediate(client);

	bol_error_state = false;
	return true;
}

void client_close_immediate(struct sock_ev_client *client) {
	SINFO("Client %p closing", client);
	struct WSFrame *frame = NULL;
	ev_io_stop(global_loop, &client->io);

	if (client->bol_socket_is_open == true) {
		if (client->que_message != NULL) {
			while (client->que_message->first != NULL) {
				struct sock_ev_client_message *client_message = client->que_message->first->value;
				ev_io_stop(global_loop, &client_message->io);
				frame = client_message->frame;
				// This removes this node from the queue, so that the next element we
				// want is the first one
				WS_client_message_free(client_message);
				WS_freeFrame(frame);
			}
			Queue_destroy(client->que_message);
			client->que_message = NULL;
		}

		shutdown(client->int_sock, SHUT_RDWR);
		SDEBUG("Shutdown socket %i", client->int_sock);

		close(client->int_sock);
		SDEBUG("Closed socket %i", client->int_sock);

		errno = 0;
		client->bol_socket_is_open = false;
	}
	client_timeout_prepare_free(client->client_timeout_prepare);

	SFREE(client->str_request);
	SFREE(client->str_response);
	SFREE(client->str_conn);
	SFREE(client->str_connname);
	SFREE(client->str_username);
	SFREE(client->str_database);
	SFREE(client->str_notice);
	SFREE(client->str_boundary);
	SFREE(client->str_connname_folder);
	SFREE(client->str_profile_id);

	// DEBUG("Client %p closed", client);
	List_remove(client->server->list_client, client->node);

	SFREE(client);
	bol_error_state = false;
	SDEBUG("client_close_immediate finished");
	return;
}

void _client_timeout_prepare_free(struct sock_ev_client_timeout_prepare *client_timeout_prepare) {
	if (client_timeout_prepare != NULL) {
		ev_prepare_stop(global_loop, &client_timeout_prepare->prepare);
		SFREE(client_timeout_prepare);
	}
}
