#pragma once

#include "util_sql_split.h"

/*
This function splits the query with DArray_sql_split and makes sure there is only one query
*/
bool query_is_safe(char *str_query);
