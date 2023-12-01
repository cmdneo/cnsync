#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <time.h>

#include "common.h"
#include "config.h"
#include "coroless.h"
#include "mystr.h"
#include "io/bufio.h"
#include "server/server.h"
#include "http/request.h"
#include "http/parser.h"

static char message[1 << 16]; // 64 KiB payload: aaaaaaaaaaa...!
static const String html_mimetype = CSTRING("text/html; charset=utf-8");

const char *get_local_datetime(void)
{
	static char buffer[256];
	time_t t = time(NULL);
	strftime(buffer, sizeof buffer, "%F %T", localtime(&t));
	return buffer;
}

/// @brief Get datetime in HTTP-datetime format
/// @return Returns a statically allocated string.
static String get_http_datetime(void)
{
	static char buffer[256];

	time_t t = time(NULL);
	// <WWW>, <DD> <MMM> <YYYY> <HH>:<MM>:<SS> GMT
	int len = strftime(
		buffer, sizeof buffer, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t)
	);

	assert(len != 0);
	return (String){.data = buffer, .len = len};
}

static void
add_std_header(HTTPHeader *resp, enum HTTPHeaderName hname, String val)
{
	assert(string_is_null(resp->std_fields[hname]));
	resp->std_fields[hname] = val;
}

// Append to string builder sb: an unsigned number or String
#define ADD(num_or_str)                                                          \
	_Generic(num_or_str, String: string_append, unsigned: string_append_number)( \
		&sb, num_or_str                                                          \
	)

#define APPEND_FIELD(name, value_num_str) \
	(ADD(name), ADD(CSTRING(": ")), ADD(value_num_str), ADD(CSTRING("\r\n")))

static bool fill_response_header_data(HTTPHeader *resp, unsigned content_length)
{
	StringBuilder sb = {.data = resp->header_data, .cap = HEADER_SIZE_MAX};

	// Response status line
	ADD(CSTRING(HTTP_VERSION_STR " "));
	ADD(resp->status);
	ADD(CSTRING(" "));
	ADD(STATUS_CODE_STRINGS[resp->status]);
	ADD(CSTRING("\r\n"));

	// Headers
	for (int i = 0; i < HNAME_COUNT; ++i) {
		if (!string_is_null(resp->std_fields[i]))
			APPEND_FIELD(HEADER_NAME_STRINGS[i], resp->std_fields[i]);
	}

	for (int i = 0; i < resp->extra_field_cnt; ++i) {
		HeaderField f = resp->extra_fields[i];
		APPEND_FIELD(f.name, f.value);
	}

	// Add content-length only if it has not been added
	if (string_is_null(resp->std_fields[HNAME_CONTENT_LENGTH]))
		APPEND_FIELD(HEADER_NAME_STRINGS[HNAME_CONTENT_LENGTH], content_length);

	// End CRLF
	if (!ADD(CSTRING("\r\n"))) {
		LOG_ERROR("HTTP-response header too long.");
		return false;
	}

	resp->header_len = sb.len;
	return true;
}

#undef APPEND_FIELD
#undef ADD

typedef struct HTTPCoroState {
	BufReader reader;
	BufWriter writer;
	HTTPHeader req;
	HTTPHeader resp;
	enum HTTPStatusCode status;
} HTTPCoroState;

#define CV variables->

int handle_http_request(CoroContext *state, Connection *conn)
{
	int c = 0, len = 0;

	HTTPCoroState *variables = NULL;
	CORO_GET_DATA_PTR(state, variables);
	CORO_BEGIN(state);

	CV reader = (BufReader){.sock_fd = conn->sock_fd, .is_eof = false};
	CV writer = (BufWriter){.sock_fd = conn->sock_fd, .is_closed = false};
	CV status = STATUS_BAD_REQUEST;

	while (1) {
		CORO_AWAIT(c, async_reader_getc(&CV reader));
		if (c == CORO_IO_EOF)
			break;

		if (CV req.header_len == HEADER_SIZE_MAX) {
			CV status = STATUS_HEADER_TOO_LARGE;
			break;
		}
		CV req.header_data[CV req.header_len++] = c;

		if (c == '\n' && is_request_header_end(&CV req))
			break;
	}

	if (CV req.header_len == 0)
		goto conn_closed;

	if (parse_request(&CV req))
		CV status = STATUS_OK;

	if (CV req.first_line.len > 0)
		PRINTE(
			"[%s] %d -- \"%.*s\"\n", get_local_datetime(), CV status,
			CV req.first_line.len, CV req.first_line.data
		);

	CV resp.status = CV status;
	add_std_header(&CV resp, HNAME_CONTENT_TYPE, html_mimetype);
	add_std_header(&CV resp, HNAME_SERVER, CSTRING("cnsync"));
	add_std_header(&CV resp, HNAME_DATE, get_http_datetime());

	if (!fill_response_header_data(&CV resp, sizeof(message)))
		goto conn_closed;

	writer_put_data(&CV writer, CV resp.header_data, CV resp.header_len);
	CORO_AWAIT(len, async_writer_drain(&CV writer));
	if (CV writer.is_closed)
		goto conn_closed;

	// Do not write body if HEAD method
	if (CV req.method == METHOD_HEAD)
		goto conn_closed;

	writer_put_data(&CV writer, message, sizeof(message));
	CORO_AWAIT(len, async_writer_drain(&CV writer));
	if (CV writer.is_closed)
		goto conn_closed;

conn_closed:
	close_connection(conn);
	CORO_END();
}

#undef CV

int main(void)
{
	IPv4Address addr = {127, 0, 0, 1, 5000};
	Server *s = server_create(addr);
	if (s == NULL) {
		LOG_FATAL("Cannot create server");
		return 2;
	}

	memset(message, 'a', sizeof message);
	message[sizeof message - 1] = '!';

	server_listen(s, handle_http_request, sizeof(HTTPCoroState));

	return 0;
}
