#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "coroutine.h"

// No physical memory is allocated until we write to it. Demand paging FTW.
#define CORO_STACK_SIZE (1 << 20) /* 4 MiB */

static_assert(CORO_STACK_SIZE >= 2 * SIGSTKSZ, "CORO_STACK_SIZE is too small");

// We use signals to setup the jump point for the coroutine.
// Since we cannot pass any arguments to signal handlers directly, we use
// global variables to indirectly pass and setup the arguments for the coroutine.
_Thread_local CoroContext *initing_coro_context__;
_Thread_local void *initing_coro_arg__;
_Thread_local CoroValue returned_coro_value__;

int coro_init_n_bind(CoroContext *ctx, CoroFunction callback)
{
	const long PAGE_SIZE = sysconf(_SC_PAGESIZE);

	char *mem = mmap(
		NULL, CORO_STACK_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0
	);
	if (mem == MAP_FAILED) {
		perror("coro_init_n_bind: mmap");
		return -1;
	}

	// The x86 and ARM stack grows downward, the first page is at stack end.
	// We make the first page a guard page.
	if (mprotect(mem, PAGE_SIZE, PROT_NONE) < 0) {
		perror("coro_init_n_bind: mprotect");
		return -1;
	}

	stack_t stack = {
		.ss_size = CORO_STACK_SIZE - PAGE_SIZE,
		.ss_sp = mem + PAGE_SIZE,
	};
	ctx->stack = mem;
	ctx->is_pending = false;

	if (sigaltstack(&stack, NULL) < 0) {
		perror("coro_init_n_bind: sigaltstack");
		return -1;
	}

	struct sigaction act = {
		.sa_flags = SA_STACK,
		.sa_handler = callback,
	};

	// We use sigsetjmp to save and then restore the signal mask on siglongjmp.
	// If signal mask is not saved, then subsequent signals will be blocked and
	// we won't be setup our coroutines which will result in undefined behaviour.
	if (sigsetjmp(ctx->yield_ctx, 1) == 0) {
		if (sigaction(SIGUSR1, &act, NULL) < 0) {
			perror("coro_init_n_bind: register sigaction");
			return -1;
		}

		initing_coro_context__ = ctx;
		raise(SIGUSR1);
	} else {
		act.sa_flags = 0;
		act.sa_handler = SIG_DFL;
		if (sigaction(SIGUSR1, &act, NULL) < 0) {
			perror("coro_init_n_bind: deregister sigaction");
			return -1;
		}
	}

	return 0;
}

int coro_deinit(CoroContext *ctx)
{
	assert(ctx->stack != NULL);
	if (munmap(ctx->stack, CORO_STACK_SIZE) < 0) {
		perror("coro_init_n_bind: munmap");
		return -1;
	}

	*ctx = (CoroContext){0};
	return 0;
}

void coro_abort__(const char *msg, const char *name)
{
	fprintf(stderr, "ERROR in coroutine %s: %s\n", name, msg);
	abort();
}
