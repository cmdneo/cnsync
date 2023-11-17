#ifndef REQUEST_H_INCLUDED
#define REQUEST_H_INCLUDED

#include "common.h"

typedef struct HeaderField {
	String name;
	String value;
} HeaderField;

typedef struct RequestURI {
	String full;
	String path;
	String query;
	String segment;
} RequestURI;

typedef struct Request {
	enum HTTPMethod method;
	RequestURI uri;
	struct {
		char minor;
		char major;
	} version;
	String first_line;
	// Stores raw data for processing
	HeaderField fields[FIELDS_MAX];
	char header_data[HEADER_MAX];
	int header_len;
	int field_cnt;
} Request;

/// @brief Checks if header has CRLF CRLF at end. Even an LF is treated as a CRLF.
/// @param r Request object
/// @return boolean
static inline bool is_request_header_end(Request *r)
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
