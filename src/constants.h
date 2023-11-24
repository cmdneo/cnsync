#ifndef CONSTANTS_H_INCLUDED
#define CONSTANTS_H_INCLUDED

#define VERSION_STR "HTTP/1.0"

#define CSTRING(str_literal) \
	(String) { (str_literal), sizeof(str_literal) - 1 }

#define STRING(ptr, len) \
	(String) { (ptr), (len) }

/// @brief String slice, stores the string along with it length.
/// From string literal using the `CSTRING` macro.
/// From pointer and length using the `STRING` macro.
typedef struct String {
	const char *data;
	int len;
} String;

enum IOConfig {
	BUFFER_SIZE = 8192,
};

enum ServerConfig {
	CONNECTIONS_MAX = 256,
	BACKLOG_MAX = 64,
	EVENTS_MAX = 64,
};

enum HTTPConfig {
	HEADER_MAX = 8190,
	URI_MAX = 2048,
	FIELDS_MAX = 64,
};

enum HTTPMethod {
	METHOD_GET,
	METHOD_POST,
	METHOD_PUT,
	METHOD_DELETE,
	METHOD_CONNECT,
	METHOD_HEAD,
	METHOD_PATCH,
	METHOD_TRACE,
	METHOD_UNKNOWN, // Keep this at last
};

static const String HTTP_METHOD_NAMES[] = {
	[METHOD_GET] = CSTRING("GET"),
	[METHOD_POST] = CSTRING("POST"),
	[METHOD_PUT] = CSTRING("PUT"),
	[METHOD_DELETE] = CSTRING("DELETE"),
	[METHOD_CONNECT] = CSTRING("CONNECT"),
	[METHOD_HEAD] = CSTRING("HEAD"),
	[METHOD_PATCH] = CSTRING("PATCH"),
	[METHOD_TRACE] = CSTRING("TRACE"),
	[METHOD_UNKNOWN] = CSTRING("<unknown-http-request-method>"),
};

enum HTTPStatusCode {
	STATUS_OK = 200,
	STATUS_BAD_REQUEST = 400,
	STATUS_NOT_FOUND = 404,
	STATUS_URI_LONG = 414,
	STATUS_TEAPOT = 418,
	STATUS_HEADER_LARGE = 431,
	STATUS_INTERNAL_ERROR = 500,
	STATUS_VERSION_UNSUPPORTED = 505,
};

static const String STATUS_MESSAGES[] = {
	[STATUS_OK] = CSTRING("OK"),
	[STATUS_BAD_REQUEST] = CSTRING("Bad Request"),
	[STATUS_NOT_FOUND] = CSTRING("Not Found"),
	[STATUS_URI_LONG] = CSTRING("Request URI Too Long"),
	[STATUS_TEAPOT] = CSTRING("I'm a Teapot"),
	[STATUS_HEADER_LARGE] = CSTRING("Request Header Too Large"),
	[STATUS_INTERNAL_ERROR] = CSTRING("Internal Server Error"),
	[STATUS_VERSION_UNSUPPORTED] = CSTRING("HTTP Version Not Supported"),
};

#endif
