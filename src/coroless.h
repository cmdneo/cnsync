#ifndef STACKLESS_COROUTINE_H_INCLUDED
#define STACKLESS_COROUTINE_H_INCLUDED

#include <assert.h>
#include <string.h>

/// @brief Signalling values returned by coros, all of these are negative.
enum CoroSignal {
	// Unhandeled system-call error, errno should be checked if this happens.
	// Coroutines unwind and return to the caller(at CORO_RUN), if this value
	// is detected. A coroutine should not be resumed after it returns this
	// value without using CORO_INIT on it, doing so is an error.
	CORO_SYS_ERROR = -1,
	// IO/wait operation would block.
	CORO_PENDING = -31,
	// Coroutine succesfully completed.
	CORO_DONE,

	// Below codes are returned by primitive-async IO functions only.
	// The underlying IO device is not available for any operations.
	CORO_IO_CLOSED,
	// The underlying IO device has no more data to be read.
	// But it might be writable.
	CORO_IO_EOF,
};

typedef struct CoroContext {
	int step;
	int data_size;
	// For storing additional state information by the user
	void *data;
} CoroContext;

// If available as a compiler extension then use it,
// since the standard [[fallthrough]] is availabe from C23 onwards only.
#if (defined __GNUC__) || (defined __clang__)
#define coro_fallthrough__ __attribute__((__fallthrough__))
#else
#define fallthrough__
#endif

#define CORO_GET_DATA_PTR(state_ptr, defined_varname)         \
	assert(sizeof(*defined_varname) == state_ptr->data_size); \
	defined_varname = state_ptr->data;

#define CORO_BEGIN(state_ptr)       \
	CoroContext *crs__ = state_ptr; \
	switch (crs__->step) {          \
	case 0:

#define CORO_END()                                    \
	crs__->step = -1;                                 \
	return CORO_DONE;                                 \
	coro_fallthrough__;                               \
	default:                                          \
		assert(!"coroutine called after completion"); \
		}                                             \
		assert(!"unreachable");                       \
		return CORO_DONE

#define CORO_AWAIT(result, async_expr) \
	do {                               \
		crs__->step = __LINE__;        \
		coro_fallthrough__;            \
	case __LINE__:                     \
		result = (async_expr);         \
		if (result == CORO_PENDING)    \
			return CORO_PENDING;       \
	} while (0)

#define CORO_SUSPEND()          \
	do {                        \
		crs__->step = __LINE__; \
		return 0;               \
		fallthrough__;          \
	case __LINE__:              \
		return CORO_PENDING;    \
	} while (0)

#define CORO_RETURN()     \
	do {                  \
		crs__->step = -1; \
		return CORO_DONE; \
	} while (0)

#define CORO_RUN(result_var, call_expr) result_var = call_expr

#define CORO_INIT(context_ptr)                    \
	do {                                          \
		CoroContext *crs__ = context_ptr;         \
		crs__->step = 0;                          \
		memset(crs__->data, 0, crs__->data_size); \
	} while (0)

#endif
