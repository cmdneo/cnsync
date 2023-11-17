#ifndef LOGGER_H_INCLUDED
#define LOGGER_H_INCLUDED

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Log levels are:
 * Debug - 4
 * Info  - 3 [Default]
 * Warn  - 2
 * Error - 1
 * Fatal - * [ALWAYS ON]
 */

#define PRINTE(...) fprintf(stderr, __VA_ARGS__)

#define LOG_DEBUG(...)                                                  \
	(PRINTE("%s:%d [DEBUG] ", __func__, __LINE__), PRINTE(__VA_ARGS__), \
	 PRINTE("\n"))

#define LOG_INFO(...) (PRINTE("[INFO] "), PRINTE(__VA_ARGS__), PRINTE("\n"))

#define LOG_WARN(...) (PRINTE("[WARN] "), PRINTE(__VA_ARGS__), PRINTE("\n"))

#define LOG_ERROR(...) (PRINTE("[ERROR] "), PRINTE(__VA_ARGS__), PRINTE("\n"))

#define LOG_FATAL(...) (PRINTE("[FATAL] "), PRINTE(__VA_ARGS__), PRINTE("\n"))

/// Prints errno with reason and exits the program.
#define ERRNO_FATAL(prefix)                                                  \
	(PRINTE(                                                                 \
		 "%s:%d [FATAL] %s: %s (OS error %d)\n", __func__, __LINE__, prefix, \
		 strerror(errno), errno                                              \
	 ),                                                                      \
	 exit(2))

// Make sure that LOG_LEVEL is defined, use default if not defined,
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

// Disable loggers as per log level
#if LOG_LEVEL < 4
#undef LOG_DEBUG
#define LOG_DEBUG(...) (void)(sizeof(__VA_ARGS__))
#endif

#if LOG_LEVEL < 3
#undef LOG_INFO
#define LOG_INFO(...) (void)(sizeof(__VA_ARGS__))
#endif

#if LOG_LEVEL < 2
#undef LOG_WARN
#define LOG_WARN(...) (void)(sizeof(__VA_ARGS__))
#endif

#if LOG_LEVEL < 1
#undef LOG_ERROR
#define LOG_ERROR(...) (void)(sizeof(__VA_ARGS__))
#endif

#endif
