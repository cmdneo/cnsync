#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <stdint.h>

#include "bufio.h"
#include "request.h"
#include "coroutine.h"

// Address as: a.b.c.d:port
typedef struct IPv4Address {
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint16_t port;
} IPv4Address;

/// @brief Returns a string representation of `addr`
/// @param addr
/// @return Return value resides in static memory, therefore,
///         future calls to this function might modify it.
const char *fmt_ipv4_addr(IPv4Address addr);

/// @brief Connection information
typedef struct Connection {
	bool is_open;
	int sock_fd;
	time_t estb_time;
	IPv4Address addr;
	// Do not zero this out, while creating a new connection.
	// Since, it holds the allocated stack address.
	CoroContext coro_ctx;
} Connection;

typedef struct Server Server;

/// @brief Allocates a server and binds it to the address.
/// @param addr_ipv4
/// @param port
/// @return Returns NULL on failure
Server *server_create(IPv4Address addr);

/// @brief Start listening and serving requests.
/// @param s The server created with server_create
/// @param callback It must be a coro-function.
/// @return Retuns only on failure
int server_listen(Server *s, CoroFunction callback);

/// @brief Closes the connection.
/// @param c Connection pointer
void close_connection(Connection *c);

#endif
