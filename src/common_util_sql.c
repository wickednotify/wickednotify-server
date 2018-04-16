#include "common_util_sql.h"

bool query_is_safe(char *str_query) {
	DArray *arr_sql = DArray_sql_split(str_query);
	SERROR_CHECK(arr_sql != NULL && DArray_end(arr_sql) == 1, "SQL Injection detected!");

	DArray_clear_destroy(arr_sql);
	return true;
error:
	DArray_clear_destroy(arr_sql);
	return false;
}
