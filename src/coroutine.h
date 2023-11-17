/******************************************************************************
We define a limited version stackful coroutines using setjmp and longjmp and a
set of macros (and some magic), here is how to use them.

Every coro has a context associated with it which contains the information
about resuming, renewing and suspending the coroutine.
First we need to call coro_init_n_bind on an empty CoroContext to allocate
stack space and bind the coro with a coro-function.

To make a function coro-function put macros CORO_BEGIN and CORO_END at start
and end of the function. The function should not use any return statements.

CORO_BEGIN macro takes one argument of form: <typename> <varname>.
The argument passed to CORO_SETUP_TASK is assigned to <varname>. The argument
should be a pointer type convertible to void pointer.
This is how we pass arguments to coroutines.

Before running a coroutine via CORO_RUN_TASK we need to pass an argument and setup
the coroutine for a new call via CORO_SETUP_TASK.
After it we can run the coroutine using CORO_RUN_TASK. A suspended coroutine can
be resumed with CORO_RUN_TASK. Return value of CORO_RUN_TASK should be checked to
determine the status of the corutine called.

CORO_SETUP_TASK should not be used on a pending coroutine as it is has not
finished its job, doing so is an error. Only after a coroutine returns CORO_DONE
signal or has not been called before should it be renewed using CORO_SETUP_TASK.

Do not try to run a completed coroutine with CORO_RUN_TASK, doing so is an error.
First renew it with CORO_SETUP_TASK using new arguments and then call it using
CORO_RUN_TASK. Doing so should be preferred over creating a new coroutine for every
call, because coro_init_n_bind takes a long time(upto 1ms) to run.

We can use CORO_AWAIT to call an primitive-async function from within a
coroutine if the call would block then it should return CORO_PENDING, if so the
couroutine will be suspend and will jump back to from where it was called.
Currently nesting user-defined coroutines is not supported.

A primitive-async function is a coroutine which does need any context. It is
usually a collection system calls for doing IO and are the basic building
blocks for all user-defined coroutines.

CORO_SUSPEND can be used to suspend a coroutine at any point.
CORO_RETURN can be used complete a coroutine and return a value from it.
CORO_RETURN supports returning any integer type, character type and pointer.
A coroutine ended with CORO_RETURN or CORO_END should not be called again.
Stack of a coroutine is preserved after its completion as long as it has not
been renewed using CORO_SETUP_TASK or freed using coro_deinit.

Event loop, scheduling and execution has to be managed by the user.

NOTE: The signal SIGUSR1 must not be blocked for binding to work.

EXAMPLE:
	void countdown_cb(int ignored) {
		CORO_BEGIN(int *cnt);
		while (*cnt > 0) {
			*cnt -= 1;
			CORO_SUSPEND();
		}
		CORO_END();
	}

	// Inside any function do...
	CoroContext ctx = {0};
	int counter = 20, coro_result = CORO_PENDING;
	coro_init_n_bind(&ctx, countdown_cb);

	CORO_SETUP_TASK(&ctx, &counter);
	
	while (coro_result == CORO_PENDING) {
		CORO_RUN_TASK(coro_result, &ctx);
		printf("%d\n", counter);
	}

	coro_deinit(&ctx);
******************************************************************************/

#ifndef STACKFUL_COROUTINE_H_INCLUDED
#define STACKFUL_COROUTINE_H_INCLUDED

#include <stdbool.h>
#include <setjmp.h>
#include <stdint.h>

typedef struct CoroContext {
	jmp_buf reset_ctx;
	jmp_buf resume_ctx;
	jmp_buf yield_ctx;
	bool is_pending;
	void *stack;
} CoroContext;

/// @brief Signalling values returned by coros, all of these are negative.
enum CoroSignal {
	// Unhandeled system-call error, errno should be checked if this happens.
	// Coroutines unwind and return to the caller(at CORO_RUN_TASK), if this value
	// is detected. A coroutine should not be resumed after it returns this
	// value without using CORO_SETUP_TASK on it, doing so is an error.
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

typedef union CoroValue {
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	char c;
	void *ptr;
} CoroValue;

typedef struct CoroResult {
	enum CoroSignal signal;
	CoroValue value;
} CoroResult;

typedef void (*CoroFunction)(int);

/* For internal use only */
extern _Thread_local CoroContext *initing_coro_context__;
extern _Thread_local void *initing_coro_arg__;
extern _Thread_local CoroValue returned_coro_value__;
void coro_abort__(const char *msg, const char *name);

// TODO Make returning values more general and useful.

// Suspend the coroutines yielding a specific signal.
// Generally this should not be used in user code.
#define CORO_SUSPEND_SIGNAL(coro_signal) \
	if (setjmp(cctx__->resume_ctx) == 0) \
	longjmp(cctx__->yield_ctx, coro_signal)

/// @brief Suspend the coroutine, signalling that it is still pending.
#define CORO_SUSPEND() CORO_SUSPEND_SIGNAL(CORO_PENDING)

/// @brief Suspend the coroutine returning a value, and signal that it is complete.
/// Running the coroutine after it has returned is an error.
#define CORO_RETURN(value)                                        \
	do {                                                          \
		_Generic(                                                 \
			(value),                                              \
			int8_t: returned_coro_value__.i8,                     \
			int16_t: returned_coro_value__.i16,                   \
			int32_t: returned_coro_value__.i32,                   \
			int64_t: returned_coro_value__.i64,                   \
			uint8_t: returned_coro_value__.u8,                    \
			uint16_t: returned_coro_value__.u16,                  \
			uint32_t: returned_coro_value__.u32,                  \
			uint64_t: returned_coro_value__.u64,                  \
			char: returned_coro_value__.c,                        \
			default: returned_coro_value__.ptr                    \
		) = (value);                                              \
		cctx__->is_pending = false;                               \
		CORO_SUSPEND_SIGNAL(CORO_DONE);                           \
		coro_abort__("cannot resume after completion", __func__); \
	} while (0)

// If setjmp returned from longjmp(fake-return), then it means that the coroutine
// was resumed due to some IO event, so we re-execute primitive_async_call.
// If async_io_expr completed(not pending) then we exit the await block.
/// @brief Await a primitive coroutine.
#define CORO_AWAIT(result_var, primitive_async_call)             \
	do {                                                         \
		result_var = (primitive_async_call);                     \
		if (result_var == CORO_SYS_ERROR) {                      \
			CORO_SUSPEND_SIGNAL(CORO_SYS_ERROR);                 \
			coro_abort__("called after system error", __func__); \
		} else if (result_var == CORO_PENDING) {                 \
			if (setjmp(cctx__->resume_ctx) == 0)                 \
				longjmp(cctx__->yield_ctx, CORO_PENDING);        \
			continue;                                            \
		}                                                        \
		break;                                                   \
	} while (1)

/// @brief Run a coroutine from the event loop.
#define CORO_RUN_TASK(status_var, context_ptr)         \
	do {                                               \
		status_var = setjmp((context_ptr)->yield_ctx); \
		if (status_var == 0)                           \
			longjmp((context_ptr)->resume_ctx, 1);     \
	} while (0)

/// @brief Renew and setup arguments for a coroutine which is not pending.
#define CORO_SETUP_TASK(context_ptr, arg_ptr)      \
	do {                                           \
		initing_coro_arg__ = (arg_ptr);            \
		if (setjmp((context_ptr)->yield_ctx) == 0) \
			longjmp((context_ptr)->reset_ctx, 1);  \
	} while (0)

// The initialization(first) call is invoked by raising a signal which
// setups the jump point for calling the coroutine, and then we
// suspend the function by jumping back to wherever from sigsetjmp was called.
// We use siglongjmp which restores the signal mask, which ensures that
// the signals for setting up more coroutines are not blocked.
// We also save a reset point, for setting up arguments and also renewing the
// coroutine to without binding it again using `coro_init_n_bind`, since
// it is an expensive function.
/// @brief Put at the top of a coro-function.
#define CORO_BEGIN(typed_argp)                                 \
	CoroContext *cctx__ = initing_coro_context__;              \
	if (setjmp(cctx__->resume_ctx) == 0) {                     \
		if (setjmp(cctx__->reset_ctx) == 0)                    \
			siglongjmp(cctx__->yield_ctx, 1);                  \
	} else {                                                   \
		coro_abort__("setup not done", __func__);              \
	}                                                          \
	if (cctx__->is_pending)                                    \
		coro_abort__("cannot renew a pending coro", __func__); \
	(cctx__)->is_pending = true;                               \
	typed_argp = initing_coro_arg__;                           \
	CORO_SUSPEND()

/// @brief Put at the end of a coro-function.
#define CORO_END() CORO_RETURN(0)

/// @brief Allocates stack space and binds the context with a `callback`.
/// @param ctx Context
/// @param callback A coro-function
/// @return 0 on success, -1 otherwise.
///
/// @note Binding coro with function takes time, so bind once and then renew via
/// CORO_SETUP_TASK to call the coroutine again with the same function.
int coro_init_n_bind(CoroContext *ctx, CoroFunction callback);

/// @brief Frees the stack allocated for the coro.
/// @param ctx Conetxt
/// @return 0 on success, -1 otherwise
int coro_deinit(CoroContext *ctx);

#endif
