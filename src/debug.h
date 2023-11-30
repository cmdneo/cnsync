#ifndef DEBUG_H_DEFINED
#define DEBUG_H_DEFINED

#include "http/http.h"
#include "common.h"

static void debug_request(const HTTPHeader *r)
{
	PRINTE("Method:  %s\n", METHOD_NAME_STRINGS[r->method].data);
	PRINTE("Version: HTTP/1.%d\n", r->version % 1);
	PRINTE("URI:     %.*s\n", r->uri.full.len, r->uri.full);

	PRINTE("Fields:\n");

	for (int i = 0; i < HNAME_COUNT; ++i) {
		if (string_is_null(r->std_fields[i]))
			continue;

		String name = HEADER_NAME_STRINGS[i];
		String value = r->std_fields[i];
		PRINTE("\t%.*s: %.*s\n", name.len, name.data, value.len, value.data);
	}

	for (int i = 0; i < r->extra_field_cnt; ++i) {
		HeaderField f = r->extra_fields[i];
		PRINTE("\t%.*s: %.*s\n", f.name.len, f.name, f.value.len, f.value);
	}
}

#endif
