#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "constants.h"
#include "logger.h"

static inline bool is_blocking_error(int e)
{
	return e == EAGAIN || e == EWOULDBLOCK;
}

#endif
