#ifndef MYSTR_H_INCLUDED
#define MYSTR_H_INCLUDED

#include <stdbool.h>
#include <string.h>

#define STRING(ptr, len) \
	(String) { (ptr), (len) }

#define CSTRING(str_literal) STRING(str_literal, sizeof(str_literal) - 1)

/// @brief String slice, stores the string along with it length.
/// From string literal using the `CSTRING` macro.
/// From pointer and length using the `STRING` macro.
typedef struct String {
	const char *data;
	int len;
} String;

static inline bool string_eq(String a, String b)
{
	return a.len == b.len && !strncmp(a.data, b.data, a.len);
}

static inline bool string_eq_case(String a, String b)
{
	return a.len == b.len && !strncasecmp(a.data, b.data, a.len);
}

static inline bool string_is_null(String a) { return !a.data; }

/// @brief Build a string by appending to it step-by-step
typedef struct StringBuilder {
	char *data;
	int len;
	int cap;
} StringBuilder;

/// @brief Appends a String to the string
/// @param s The string builder
/// @param t The string to be appended
/// @return false if there is no space in `s`, otherwise true.
static inline bool string_append(StringBuilder *s, String t)
{
	if (s->cap - s->len < t.len)
		return false;

	memcpy(s->data + s->len, t.data, t.len);
	s->len += t.len;
	return true;
}

/// @brief Appends a number to the string
/// @param s The string builder
/// @param num The number to be appended
/// @return false if there is no space in `s`, otherwise true.
static inline bool string_append_number(StringBuilder *s, unsigned long num)
{
	static char buffer[64]; // Enough even for a 128-bit number
	int len = 0;

	while (num) {
		buffer[len++] = num % 10 + '0';
		num /= 10;
	}

	if (s->cap - s->len < len)
		return false;

	while (len)
		s->data[s->len++] = buffer[--len];

	return true;
}

#endif
