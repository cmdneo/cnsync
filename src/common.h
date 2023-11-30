#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

static inline bool is_blocking_error(int e)
{
	return e == EAGAIN || e == EWOULDBLOCK;
}

#endif
