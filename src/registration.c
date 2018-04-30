#include "registration.h"

ev_check *global_registration_watcher = NULL;
Queue *que_registration = NULL;
bool bol_query_in_process = false;

// Handle registration actions
void registration_cb(EV_P, ev_check *w, int revents) {
	(void)w;
	(void)revents;
	Registration_action *action = NULL;
	char *str_id_literal = NULL;
	char *str_query = NULL;
	size_t int_query_len = 0;

	if (!bol_query_in_process && Queue_peek(que_registration)) {
		action = Queue_recv(que_registration);

		str_id_literal = DB_escape_literal(global_conn, action->str_id, action->int_id_len);
		SERROR_CHECK(str_id_literal != NULL, "DB_escape_literal failed");

		if (action->type == REG_ACTION_GUID) {
			SERROR_SNCAT(str_query, &int_query_len, str_global_query_validate_guid, strlen(str_global_query_validate_guid));
			SERROR_BREPLACE(str_query, &int_query_len, "{{GUID}}", str_id_literal, "g");
	
			SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
			SERROR_CHECK(DB_exec(EV_A, global_conn, action, str_query, registration_guid_query_cb), "DB_exec failed");
			
		} else if (action->type == REG_ACTION_APNS) {
			SERROR_SNCAT(str_query, &int_query_len, str_global_query_delete_device, strlen(str_global_query_delete_device));
			SERROR_BREPLACE(str_query, &int_query_len, "{{APNS}}", str_id_literal, "g");
	
			SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
			SERROR_CHECK(DB_exec(EV_A, global_conn, action, str_query, registration_apns_delete_query_cb), "DB_exec failed");
		} else if (action->type == REG_ACTION_LATEST) {
			SERROR_SNCAT(str_query, &int_query_len, str_global_query_get_unsent_notifications, strlen(str_global_query_get_unsent_notifications));
			SERROR_BREPLACE(str_query, &int_query_len, "{{NOTIFYID}}", str_id_literal, "g");
			SERROR_BREPLACE(str_query, &int_query_len, "{{PROFILEID}}", action->client->str_profile_id, "g");

			SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
			SERROR_CHECK(DB_exec(EV_A, global_conn, action, str_query, notification_unsent_query_cb), "DB_exec failed");
		}

		bol_query_in_process = true;

		decrement_idle(EV_A);
		action = NULL;
	}
error:
	bol_error_state = false;
	if (action != NULL) {
		SFREE(action->str_id);
		SFREE(action);
	}
	SFREE(str_query);
	SFREE(str_id_literal);
	return;
}

bool registration_guid_query_cb(EV_P, void *cb_data, DB_result *res) {
	bool bol_ret = true;
	Registration_action *action = cb_data;
	DArray *arr_values = NULL;
	char *_str_response = NULL;

	DB_fetch_status status = DB_FETCH_OK;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_TUPLES_OK, "DB_exec failed");

	while ((status = DB_fetch_row(res)) != DB_FETCH_END) {
		SERROR_CHECK(status != DB_FETCH_ERROR, "DB_fetch_row failed");

		arr_values = DB_get_row_values(res);
		SERROR_CHECK(arr_values != NULL, "DB_get_row_values failed");

		SERROR_SNCAT(action->client->str_profile_id, &action->client->int_profile_id_len,
			DArray_get(arr_values, 0), strlen(DArray_get(arr_values, 0)));

		DArray_clear_destroy(arr_values);
		arr_values = NULL;
	}

	if (strncmp(action->client->str_profile_id, "NO MATCH", 8) == 0) {
		SERROR_CHECK(WS_sendFrame(EV_A, action->client, true, 0x01, "NO MATCH", 8), "Failed to send message");
		SFREE(action->client->str_profile_id);
		action->client->int_profile_id_len = 0;
	} else {
		SERROR_CHECK(WS_sendFrame(EV_A, action->client, true, 0x01, "MATCH", 5), "Failed to send message");
	}

error:
	SFREE(action->str_id);
	SFREE(action);
	bol_query_in_process = false;
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
	}
	bol_error_state = false;

	DB_free_result(res);
	SFREE(_str_response);
	if (arr_values != NULL) {
		DArray_clear_destroy(arr_values);
		arr_values = NULL;
	}
	bol_error_state = false;
	return bol_ret;
}

bool registration_apns_delete_query_cb(EV_P, void *cb_data, DB_result *res) {
	bool bol_ret = true;
	Registration_action *action = cb_data;
	char *_str_response = NULL;
	char *str_query = NULL;
	char *str_id_literal = NULL;
	size_t int_query_len = 0;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_COMMAND_OK, "DB_exec failed");

	str_id_literal = DB_escape_literal(global_conn, action->str_id, action->int_id_len);
	SERROR_CHECK(str_id_literal != NULL, "DB_escape_literal failed");

	SERROR_SNCAT(str_query, &int_query_len, str_global_query_insert_device, strlen(str_global_query_insert_device));
	SERROR_BREPLACE(str_query, &int_query_len, "{{PROFILEID}}", action->client->str_profile_id, "g");
	SERROR_BREPLACE(str_query, &int_query_len, "{{APNS}}", str_id_literal, "g");

	SERROR_CHECK(query_is_safe(str_query), "SQL Injection detected");
	SERROR_CHECK(DB_exec(EV_A, global_conn, action, str_query, registration_apns_insert_query_cb), "DB_exec failed");

error:
	SFREE(str_query);
	SFREE(str_id_literal);

	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
		bol_query_in_process = false;
	}
	bol_error_state = false;

	DB_free_result(res);
	SFREE(_str_response);
	bol_error_state = false;
	return bol_ret;
}

bool registration_apns_insert_query_cb(EV_P, void *cb_data, DB_result *res) {
	bool bol_ret = true;
	Registration_action *action = cb_data;
	char *_str_response = NULL;

	SERROR_CHECK(res != NULL, "DB_exec failed");
	SERROR_CHECK(res->status == DB_RES_COMMAND_OK, "DB_exec failed");

	SERROR_CHECK(WS_sendFrame(EV_A, action->client, true, 0x01, "OK", 2), "Failed to send message");

error:
	SFREE(action->str_id);
	SFREE(action);
	bol_query_in_process = false;
	if (bol_error_state) {
		_str_response = DB_get_diagnostic(global_conn, res);
		SERROR_NORESPONSE("%s", _str_response);
	}
	bol_error_state = false;
	
	DB_free_result(res);
	SFREE(_str_response);
	bol_error_state = false;
	return bol_ret;
}
