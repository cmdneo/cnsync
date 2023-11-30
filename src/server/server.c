/**
 * @file server.c
 * @brief Asynchronous TCP socket server for Linux.
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"
#include "config.h"
#include "server/server.h"
#include "io/bufio.h"
#include "coroless.h"

/// @brief The TCP Server along with HTTP-request state
typedef struct Server {
	int sock_fd;
	int epoll_fd;
	int active_cnt;
	IPv4Address listen_addr;
	Connection connections[CONNECTIONS_MAX];
} Server;

#define AS_SADDRP(addrp) ((struct sockaddr *)(addrp))

static IPv4Address sockaddr_to_ipv4_addr(struct sockaddr_in *addrp)
{
	// sin_addr part is stored in network byte order(big-endian).
	uint8_t *bytes = (uint8_t *)&addrp->sin_addr.s_addr;
	return (IPv4Address){
		.a = bytes[0],
		.b = bytes[1],
		.c = bytes[2],
		.d = bytes[3],
		.port = ntohs(addrp->sin_port),
	};
}

static struct sockaddr_in ipv4_addr_to_sockaddr(IPv4Address addr)
{
	uint32_t net_addr = addr.a << 24 | addr.b << 24 | addr.c << 8 | addr.d;

	return (struct sockaddr_in){
		.sin_addr.s_addr = htonl(net_addr),
		.sin_port = htons(addr.port),
	};
}

static Connection *find_free_connection(Connection *list, int size)
{
	Connection *ret = list;
	while (ret->is_open) {
		ret++;
		(void)size; // assert not in release mode, so prevent unused warning.
		assert(ret - list < size);
	}

	return ret;
}

static int setnonblocking(int fd) { return fcntl(fd, F_SETFL, O_NONBLOCK); }

/// @brief Initializes the server and binds it to the specified address.
/// @param s Pointer to server
/// @param addr_ipv4
/// @param port
/// @return 0 on success, -1 on failure
int server_init(Server *s, IPv4Address address)
{

	struct sockaddr_in sock_addr = ipv4_addr_to_sockaddr(address);
	sock_addr.sin_family = AF_INET;

	int sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (sock_fd < 0)
		ERRNO_FATAL("socket");

	// #ifndef NDEBUG
	// Allow binding to the same address immdeiately after killing the application.
	int val = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	// #endif

	if (bind(sock_fd, AS_SADDRP(&sock_addr), sizeof sock_addr) < 0)
		ERRNO_FATAL("bind");

	// Create epoll
	// We add server FD to epoll when we start listening on it, not here.
	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0)
		ERRNO_FATAL("epoll_create1");

	// Query the address again, in case the user provides 0 for port number,
	// OS assigns a random free port to it.
	socklen_t size = sizeof(struct sockaddr_in);
	getsockname(sock_fd, AS_SADDRP(&sock_addr), &size);

	*s = (Server){
		.active_cnt = 0,
		.epoll_fd = epoll_fd,
		.sock_fd = sock_fd,
		.listen_addr = sockaddr_to_ipv4_addr(&sock_addr),
	};

	return 0;
}

Server *server_create(IPv4Address addr)

{
	Server *s = ALLOCATE(Server);
	if (!s) {
		ERRNO_FATAL("calloc");
		return NULL;
	}

	server_init(s, addr);
	return s;
}

const char *fmt_ipv4_addr(IPv4Address addr)
{
	// Enough for: "xxx.xxx.xxx.xxx:ppppp\0"
	static char addr_str[32];

	snprintf(
		addr_str, sizeof addr_str, "%u.%u.%u.%u:%u", addr.a, addr.b, addr.c,
		addr.d, addr.port
	);

	return addr_str;
}

int handle_conn_event(
	Server *s, Connection *conn, uint32_t events, ConnCallback callback
)
{
	if (events & EPOLLIN || events & EPOLLOUT) {
		int result = 0;
		CORO_RUN(result, callback(&conn->coro_ctx, conn));
		if (result == CORO_SYS_ERROR)
			ERRNO_FATAL("coro for connection failed");
		if (result == CORO_DONE && conn->is_open)
			close_connection(conn);
	}

	// If sender hangs up
	if (events & EPOLLRDHUP && conn->is_open) {
		close_connection(conn);
	}

	// Closed FDs are auto removed from epoll interest list.
	if (!conn->is_open) {
		// A connection is always from server's list of connections.
		conn->is_open = false;
		s->active_cnt--;
	}

	return 0;
}

/// @brief Accepts a single connection if available
/// @param s
/// @return Returns 1 if connection accepted, -1 on error and 0 otherwise.
int handle_server_event(Server *s)
{
	if (s->active_cnt == CONNECTIONS_MAX)
		return 0;

	struct sockaddr_in conn_addr = {0};
	socklen_t addr_len = sizeof conn_addr;
	int conn_fd = accept(s->sock_fd, AS_SADDRP(&conn_addr), &addr_len);

	assert(addr_len == sizeof conn_addr);
	if (conn_fd < 0) {
		// If connection not available or was dropped before accepting.
		if (is_blocking_error(errno) || errno == ECONNABORTED)
			return 0;
		ERRNO_FATAL("accept");
	}
	// Make the connection async
	if (setnonblocking(conn_fd) < 0)
		ERRNO_FATAL("setnonblocking");

	// Find an empty slot, it must exist since we check for it above.
	Connection *conn = find_free_connection(s->connections, CONNECTIONS_MAX);
	s->active_cnt++;

	CORO_INIT(&conn->coro_ctx);
	conn->addr = sockaddr_to_ipv4_addr(&conn_addr);
	conn->sock_fd = conn_fd;
	conn->is_open = true;
	conn->estb_time = time(NULL);

	// We want to detect read/write availability and if the connection was closed.
	struct epoll_event event = {
		.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET,
		.data.ptr = conn,
	};
	if (epoll_ctl(s->epoll_fd, EPOLL_CTL_ADD, conn_fd, &event) < 0)
		ERRNO_FATAL("epoll_ctl");

	LOG_DEBUG("Connection recieved %s", fmt_ipv4_addr(conn->addr));
	return 1;
}

// static double timespec_diff(struct timespec s, struct timespec e)
// {
// 	long nano_diff = e.tv_nsec - s.tv_nsec;
// 	return e.tv_sec - s.tv_sec + (nano_diff / 1000000000.0);
// }

int server_listen(Server *s, ConnCallback callback, size_t data_size)
{
	struct epoll_event events[EVENTS_MAX] = {0};

	char *coro_data = ALLOCATE_SIZED_ARRAY(data_size, CONNECTIONS_MAX);
	if (coro_data == NULL)
		ERRNO_FATAL("malloc");
	// Initialize data for all connection coros
	for (int i = 0; i < CONNECTIONS_MAX; ++i) {
		s->connections[i].coro_ctx.data = coro_data + i * data_size;
		s->connections[i].coro_ctx.data_size = data_size;
	}

	// Register the server with epoll
	struct epoll_event event = {
		.events = EPOLLIN,
		.data.ptr = s,
	};
	if (epoll_ctl(s->epoll_fd, EPOLL_CTL_ADD, s->sock_fd, &event) < 0)
		ERRNO_FATAL("epoll_ctl");

	// Start listening
	if (listen(s->sock_fd, BACKLOG_MAX) < 0)
		ERRNO_FATAL("listen");
	LOG_INFO("Listening on %s", fmt_ipv4_addr(s->listen_addr));

	// Main event loop
	while (1) {
		int event_cnt = epoll_wait(s->epoll_fd, events, EVENTS_MAX, -1);
		if (event_cnt < 0)
			ERRNO_FATAL("epoll_wait");

		for (int i = 0; i < event_cnt; ++i) {
			struct epoll_event ev = events[i];
			if (ev.data.ptr == s) {
				// Accept as much as possible at once
				while (handle_server_event(s) > 0)
					/* nothing */;
			} else {
				handle_conn_event(s, ev.data.ptr, ev.events, callback);
			}
		}
	}

	return 0;
}

void close_connection(Connection *c)
{
	assert(c->is_open);

	if (shutdown(c->sock_fd, SHUT_RDWR) < 0 && errno == ENOTCONN) {
		LOG_DEBUG("Connection dropped  %s", fmt_ipv4_addr(c->addr));
	} else
		LOG_DEBUG("Connection closed   %s", fmt_ipv4_addr(c->addr));

	close(c->sock_fd);
	c->is_open = false;
}
