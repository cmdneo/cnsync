#ifndef MYSTR_H_INCLUDED
#define MYSTR_H_INCLUDED

#include <stdbool.h>
#include <string.h>

/// @brief Makes a String.
#define STRING(ptr, length) \
	(String) { .data = (ptr), .len = (length) }

/// @brief Makes a StringBuilder.
#define STRING_BUILDER(ptr, capacity) \
	(StringBuilder) { .data = (ptr), .cap = (capacity) }

/// @brief Makes a String from a string literal.
#define CSTRING(str_literal) STRING(str_literal, sizeof(str_literal) - 1)

/// @brief Defines a string buffer as an unnamed struct having fields: data and len.
#define DEF_STRING_BUFFER(name, capacity) \
	struct {                              \
		char data[(capacity)];            \
		int len;                          \
	} name

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

static inline int string_findc(String a, char c)
{
	if (!a.data)
		return -1;

	char *at = memchr(a.data, c, a.len);
	return at ? at - a.data : -1;
}

/// @brief Partition the string into parts before pos and after pos
///        the char at pos is not included in any of the strings.
/// @param a String to be partitioned, it can be `left` or `right`.
/// @param pos Index of parition
/// @param left Left result argument
/// @param right Right result argument
/// @return false if pos is invalid, otherwise partion and return true
static inline bool
string_partition(String a, int pos, String *left, String *right)
{
	if (pos >= a.len || pos < 0)
		return false;

	String s = a; // As left or right might be a, so we copy it.
	*left = (String){.data = s.data, .len = pos};
	*right = (String){.data = s.data + pos + 1, .len = s.len - pos - 1};
	return true;
}

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

	memmove(s->data + s->len, t.data, t.len);
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

	if (num == 0)
		buffer[len++] = '0';

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
