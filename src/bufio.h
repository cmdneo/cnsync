#ifndef BUFIO_H_INCLUDED
#define BUFIO_H_INCLUDED

#include "common.h"
#include "coroutine.h"

typedef struct BufReader {
	int sock_fd;
	int count;
	int at;
	// Total bytes of data read from the sock_fd(socket)
	int read_cnt;
	bool is_eof;
	char data[BUFFER_SIZE];
} BufReader;

typedef struct BufWriter {
	int sock_fd;
	int len;
	int done_len;
	bool is_closed;
	const char *data;
} BufWriter;

/// @brief Puts data into the writer for writing to a connection.
///        No data is written to the socket, use `async_writer_drain` for that.
///        Attempt to put new data without draining all the old data is an error.
/// @param b BufWriter
/// @param data Data to be written. It must not change until all data has been
///        written to the socket using `async_writer_drain`.
/// @param len Length of the data
void writer_put_data(BufWriter *b, const char *data, int len);

/// @brief Writes the data to the socket as much as it can.
/// @param b BufWriter
/// @return Returns IoResult indicating status.
int async_writer_drain(BufWriter *b);

/// @brief Reads a byte from the socket
/// @param b
/// @return
static inline int async_reader_getc(BufReader *b)
{
	int async_read_to_buffer(BufReader * b);

	if (b->at == b->count) {
		if (b->is_eof)
			return CORO_IO_EOF;

		int res = async_read_to_buffer(b);
		if (res == CORO_SYS_ERROR)
			ERRNO_FATAL("async_read_to_buffer");
		if (res < 0)
			return res;
	}

	return b->data[b->at++];
}

#endif
