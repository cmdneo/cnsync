#ifndef DEBUG_H_DEFINED
#define DEBUG_H_DEFINED

#include "request.h"
#include "constants.h"
#include "logger.h"

static void debug_request(const Request *r)
{
	PRINTE("Method:  %s\n", HTTP_METHOD_NAMES[r->method].data);
	PRINTE("Version: HTTP/%d.%d\n", r->version.major, r->version.minor);
	PRINTE("URI:     %.*s\n", r->uri.full_len, r->uri.full);

	PRINTE("Fields:\n");

	for (int i = 0; i < r->field_cnt; ++i) {
		HeaderField f = r->fields[i];
		PRINTE("\t%.*s: %.*s\n", f.name_len, f.name, f.value_len, f.value);
	}
}

#endif
