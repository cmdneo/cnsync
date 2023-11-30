#include <stdlib.h>
#include <sys/socket.h>

#include "common.h"
#include "config.h"
#include "coroless.h"
#include "io/bufio.h"

void writer_put_data(BufWriter *b, const char *data, int len)
{
	if (b->is_closed) {
		LOG_FATAL("%s: Cannot put data in a closed stream.", __func__);
		abort();
	}
	if (b->data != NULL) {
		LOG_FATAL(
			"%s: Cannot put in new data without draining the old data.",
			__func__
		);
		abort();
	}

	b->data = data;
	b->len = len;
}

int async_writer_drain(BufWriter *b)
{
	if (b->is_closed) {
		LOG_FATAL("%s: Attempt to write to a closed stream.", __func__);
		abort();
	}

	while (b->len > 0) {
		int len = send(b->sock_fd, b->data, b->len, MSG_NOSIGNAL);
		if (len < 0) {
			if (is_blocking_error(errno))
				return CORO_PENDING;
			if (errno == EPIPE || errno == ECONNRESET) {
				b->is_closed = true;
				return CORO_IO_CLOSED;
			}
			ERRNO_FATAL("send");
		}

		b->data += len;
		b->len -= len;
	}

	assert(b->len == 0);
	b->data = NULL;

	return CORO_DONE;
}

/// @brief Clears the buffer and reads in new data
/// @param b
/// @return Amount of data read, or a negative value from IOResult if error.
int async_read_to_buffer(BufReader *b)
{
	b->at = 0;
	b->count = 0;

	int len = recv(b->sock_fd, b->data, BUFFER_SIZE, 0);
	if (len == 0) {
		b->is_eof = true;
		return CORO_IO_EOF;
	}
	if (len < 0) {
		if (is_blocking_error(errno))
			return CORO_PENDING;
		ERRNO_FATAL("recv");
	}

	b->read_cnt += len;
	b->count = len;
	return len;
}
