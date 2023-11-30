#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "mystr.h"
#include "logger.h"
#include "memory.h"

static inline bool is_blocking_error(int e)
{
	return e == EAGAIN || e == EWOULDBLOCK;
}

#endif
