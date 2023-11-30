/**
 * Defines constants and types for the HTTP/1.0 protocol.
 * It also includes some widely used extensions, many of which
 * are a part of the HTTP/1.1 protocol.
 */

#ifndef HTTP_H_INCLUDED
#define HTTP_H_INCLUDED

#include "mystr.h"
#include "common.h"

#define HTTP_VERSION_STR "HTTP/1.0"

enum HTTPMethod {
	// Get a resource identified by request-URI.
	METHOD_GET,
	// Post data to the server for the request-URI.
	METHOD_POST,
	// Get only header of the response, no body.
	METHOD_HEAD,
	METHOD_UNKNOWN, // Keep this at last
};

static const String METHOD_NAME_STRINGS[] = {
	[METHOD_GET] = CSTRING("GET"),
	[METHOD_POST] = CSTRING("POST"),
	[METHOD_HEAD] = CSTRING("HEAD"),
	[METHOD_UNKNOWN] = CSTRING("<unknown-method>"),
};

enum HTTPStatusCode {
	// HTTP 1.0 status codes, quite self-explanatory.
	STATUS_OK = 200,
	STATUS_CREATED = 201,
	STATUS_ACCEPTED = 202,
	STATUS_NO_CONTENT = 204,
	STATUS_MOVED_PERMA = 301,
	STATUS_MOVED_TEMP = 302,
	STATUS_NOT_MODIFIED = 304,
	STATUS_BAD_REQUEST = 400,
	STATUS_UNAUTHORIZED = 401,
	STATUS_FORBIDDEN = 403,
	STATUS_NOT_FOUND = 404,
	STATUS_INTERNAL_ERROR = 500,
	STATUS_NOT_IMPLEMENTED = 501,
	STATUS_BAD_GATEWAY = 502,
	STATUS_SERVICE_UNAVAILABLE = 503,

	// Extension status codes
	STATUS_TEAPOT = 418,
	STATUS_HEADER_TOO_LARGE = 431,
	STATUS_VERSION_UNSUPPORTED = 505,
};

static const String STATUS_CODE_STRINGS[] = {
	[STATUS_OK] = CSTRING("OK"),
	[STATUS_CREATED] = CSTRING("Created"),
	[STATUS_ACCEPTED] = CSTRING("Accepted"),
	[STATUS_NO_CONTENT] = CSTRING("No Content"),
	[STATUS_MOVED_PERMA] = CSTRING("Movec Permanently"),
	[STATUS_MOVED_TEMP] = CSTRING("Moved Temporarily"),
	[STATUS_NOT_MODIFIED] = CSTRING("Not Modified"),
	[STATUS_BAD_REQUEST] = CSTRING("Bad Request"),
	[STATUS_UNAUTHORIZED] = CSTRING("Unauthorized"),
	[STATUS_FORBIDDEN] = CSTRING("Forbidden"),
	[STATUS_NOT_FOUND] = CSTRING("Not Found"),
	[STATUS_TEAPOT] = CSTRING("I'm a Teapot"),
	[STATUS_HEADER_TOO_LARGE] = CSTRING("Request Header Too Large"),
	[STATUS_INTERNAL_ERROR] = CSTRING("Internal Server Error"),
	[STATUS_NOT_IMPLEMENTED] = CSTRING("Not Implemented"),
	[STATUS_BAD_GATEWAY] = CSTRING("Bad Gateway"),
	[STATUS_SERVICE_UNAVAILABLE] = CSTRING("Service Unavailable"),
	[STATUS_VERSION_UNSUPPORTED] = CSTRING("HTTP Version Not Supported"),
};

// HTTP-date-time: <WWW>, <DD> <MMM> <YYYY> <HH>:<MM>:<SS> GMT
// WWW is day-name and MMM is month-name using three letters of the alphabet.

enum HTTPHeaderName {
	// ------ Entity header fields ------
	// Set of methods supported for the request-URI
	HNAME_ALLOW,
	// Encoding applied to the body: gzip, compress or deflate.
	HNAME_CONTENT_ENCODING,
	// Length of body in bytes
	HNAME_CONTENT_LENGTH,
	// Media type or mimetype of the body, eg: text/html, image/png.
	HNAME_CONTENT_TYPE,
	// Date-time after the entity should be considered outdated.
	HNAME_EXPIRES,
	// Date-time since the resource was last modifed according to the server.
	HNAME_LAST_MODIFIED,

	// ------ General header fields ------
	// Implementation specific directives.
	HNAME_PRAGMA,
	// Date and time at which the message was originated.
	HNAME_DATE,

	// ------ Response header fields ------
	// Absolute URL for automatic redirection, used for 3xx responses.
	HNAME_LOCATION,
	// Information about the HTTP server, informative only.
	HNAME_SERVER,
	// Included with a 401(Unauthotized) response, containing information
	// about authentication scheme(s) applicable for the request-URI.
	HNAME_WWW_AUTHENTICATE,

	// ------ Request header fields ------
	// Authorization information, may be used if server returns 401.
	HNAME_AUTHORIZATION,
	// Email address of the person who controls the user-agent.
	HNAME_FROM,
	// Used with GET method, if the requested resource has not been modified
	// since the time specified then the server returns 304(Not Modified) without
	// any responce body. It is an optimization thing.
	HNAME_IF_MODIFIED_SINCE,
	// Address of the resource from which request-URI was obtained.
	HNAME_REFERER,
	// Information about the user-agent which initiated the request.
	HNAME_USER_AGENT,
	// It's in HTTP/1.1 standard not is HTTP/1.0 but is almost always required.
	// For host identification, has value <hostname>[:<port>]
	HNAME_HOST,

	HNAME_COUNT,
};

static const String HEADER_NAME_STRINGS[] = {
	[HNAME_ALLOW] = CSTRING("Allow"),
	[HNAME_CONTENT_ENCODING] = CSTRING("Content-Encoding"),
	[HNAME_CONTENT_LENGTH] = CSTRING("Content-Length"),
	[HNAME_CONTENT_TYPE] = CSTRING("Content-Type"),
	[HNAME_EXPIRES] = CSTRING("Expires"),
	[HNAME_LAST_MODIFIED] = CSTRING("Last-Modified"),
	[HNAME_PRAGMA] = CSTRING("Pragma"),
	[HNAME_DATE] = CSTRING("Date"),
	[HNAME_LOCATION] = CSTRING("Location"),
	[HNAME_SERVER] = CSTRING("Server"),
	[HNAME_WWW_AUTHENTICATE] = CSTRING("WWW-Authenticate"),
	[HNAME_AUTHORIZATION] = CSTRING("Authorization"),
	[HNAME_FROM] = CSTRING("From"),
	[HNAME_IF_MODIFIED_SINCE] = CSTRING("If-Modified-Since"),
	[HNAME_REFERER] = CSTRING("Referer"),
	[HNAME_USER_AGENT] = CSTRING("User-Agent"),
	[HNAME_HOST] = CSTRING("Host"),
};

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

/// @brief HTTP header data, can be used for both request and response.
/// For a request we parse the header data and populate its fields.
/// For a response we use its fields to fill header data.
typedef struct HTTPHeader {
	enum HTTPMethod method;
	enum HTTPStatusCode status;
	uint8_t version;
	RequestURI uri;

	// Standard header fields we know and check for.
	// Indexed using HTTPHeaderName(HNAME_*) values. If the string value for a
	// header name is a null string then that header field has not been used.
	String std_fields[HNAME_COUNT];
	// Header fields, which we do not track ourselves.
	HeaderField extra_fields[EXTRA_FIELDS_MAX];
	int extra_field_cnt;

	// Points to the first line of header data, used only for requests.
	String first_line;
	char header_data[HEADER_SIZE_MAX];
	int header_len;
} HTTPHeader;

#endif
