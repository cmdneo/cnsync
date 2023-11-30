#ifndef REQUEST_H_INCLUDED
#define REQUEST_H_INCLUDED

#include "common.h"
#include "http/http.h"

/// @brief Checks if header has CRLF CRLF at end. Even an LF is treated as a CRLF.
/// @param r HTTPHeader object
/// @return boolean
static inline bool is_request_header_end(HTTPHeader *r)
{
	int len = r->header_len >= 4 ? 4 : r->header_len;
	const char *end = r->header_data + r->header_len;
	const char *start = end - len;
	int prev = 0;

	for (; start != end; ++start) {
		if (*start == '\n' && prev == '\n')
			return true;
		if (*start == '\r')
			continue;
		prev = *start;
	}

	return false;
}

#endif