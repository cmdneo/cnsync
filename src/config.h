#ifndef CONSTANTS_H_INCLUDED
#define CONSTANTS_H_INCLUDED

enum IOConfig {
	BUFFER_SIZE = 4096,
};

enum ServerConfig {
	CONNECTIONS_MAX = 256,
	BACKLOG_MAX = 64,
	EVENTS_MAX = 64,
};

enum HTTPConfig {
	// Max size of request and response headers.
	HEADER_SIZE_MAX = 8190,
	URI_SIZE_MAX = 4096,
	// Max number of header-fields besides those we keep track for.
	EXTRA_FIELDS_MAX = 64,
};

#endif
