#include <stdlib.h>
#include <sys/socket.h>
#include <stdint.h>

#include "logger.h"
#include "common.h"
#include "bufio.h"
#include "coroless.h"

void writer_put_data(BufWriter *b, const char *data, int len)
{
	if (!b->data) {
		LOG_FATAL("%s: Cannot put new data without draining.", __func__);
		abort();
	}

	b->data = data;
	b->len = len;
	b->done_len = 0;
}

int async_writer_drain(BufWriter *b)
{
	int remaining = b->len - b->done_len;

	while (b->done_len < b->len) {
		int len =
			send(b->sock_fd, b->data + b->done_len, remaining, MSG_NOSIGNAL);
		if (len < 0) {
			if (is_blocking_error(errno))
				return CORO_PENDING;
			if (errno == EPIPE || errno == ECONNRESET) {
				b->is_closed = true;
				return CORO_IO_CLOSED;
			}
			ERRNO_FATAL("send");
		}

		b->done_len += len;
	}

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
