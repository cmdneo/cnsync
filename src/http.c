#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "coroless.h"
#include "server.h"
#include "request.h"
#include "parser.h"
#include "bufio.h"

#define MARK_USED(var) ((void)(sizeof(var)))

static char message[1 << 16]; // 64 KiB payload: aaaaaaaaaaa...!
static const char headers[] = "Content-Length: 65536\r\n"
							  "Content-Type: text/html; charset=utf-8\r\n\r\n";

const char *fmt_response_line(int status, int *len)
{
	static char buffer[256];
	*len = snprintf(
		buffer, sizeof buffer, VERSION_STR " %d %.*s\r\n", status,
		STATUS_MESSAGES[status].len, STATUS_MESSAGES[status].data
	);

	return buffer;
}

const char *fmt_datetime(void)
{
	static char buffer[256];

	time_t t = time(NULL);
	strftime(buffer, sizeof buffer, "%F %T", localtime(&t));
	return buffer;
}

typedef struct HTTPCoroState {
	BufReader reader;
	BufWriter writer;
	Request req;
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

		if (CV req.header_len == HEADER_MAX) {
			CV status = STATUS_HEADER_LARGE;
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
			"[%s] %d -- \"%.*s\"\n", fmt_datetime(), CV status,
			CV req.first_line.len, CV req.first_line.data
		);

	const char *response = fmt_response_line(CV status, &len);

	writer_put_data(&CV writer, response, len);
	CORO_AWAIT(len, async_writer_drain(&CV writer));
	if (CV writer.is_closed)
		goto conn_closed;

	writer_put_data(&CV writer, headers, strlen(headers));
	CORO_AWAIT(len, async_writer_drain(&CV writer));
	if (CV writer.is_closed)
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
