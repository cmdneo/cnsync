#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "config.h"
#include "mystr.h"
#include "http/http.h"
#include "http/parser.h"
#include "http/http.h"

enum TokenType {
	TOK_NAME,
	TOK_NUMBER,
	TOK_COLON,
	// '\r\n', we also allow '\n'
	TOK_CRLF,
	// Sequence of blanks: space or tab
	TOK_BLANKS,
	TOK_EOF,
	TOK_ERROR,
	// Special token type, returned by `scanner_skip_while` only.
	TOK_CUSTOM,
};

typedef struct Token {
	String lexeme;
	int ttype;
} Token;

typedef struct Scanner {
	const char *source;
	const char *start;
	const char *at;
	const char *end;
	Token current;
} Scanner;

/// Returns false from the function if the next token does not match tok_type,
/// otherwise does-nothing.
#define SCANNER_CONSUME(scanner_ptr, tok_type) \
	TOKEN_EXPECT(scanner_next_token(scanner_ptr), tok_type)

/// Returns false from the function if token does not match the tok_type,
/// otherwise does-nothing.
#define TOKEN_EXPECT(token, tok_type) \
	do {                              \
		if (tok_type != token.ttype)  \
			return false;             \
	} while (0)

// Functions for different character classes we want
static inline int is_name_char(int c)
{
	return isalnum(c) || c == '_' || c == '-';
}

static inline int is_uri_char(int c) { return isgraph(c); }

static inline int is_not_crlf(int c) { return c != '\r' && c != '\n'; }

static inline Token make_token(enum TokenType tt, Scanner *s)
{
	return (Token){
		.ttype = tt,
		.lexeme = STRING(s->start, s->at - s->start),
	};
}

static void scanner_init(Scanner *s, const char *source, int len)
{
	s->source = source;
	s->at = source;
	s->end = source + len;
	s->start = NULL;
	s->current = (Token){.ttype = TOK_ERROR};
}

static inline void scanner_ungetc(Scanner *s)
{
	if (s->at - s->source > 0)
		s->at--;
}

static inline int scanner_getc(Scanner *s)
{
	if (s->at == s->end)
		return EOF;
	return *s->at++;
}

static inline int scanner_peekc(const Scanner *s)
{
	if (s->at == s->end)
		return EOF;
	return *s->at;
}

static inline Token skip_while_impl(Scanner *s, int (*predicate)(int))
{
	int c = 0;
	while ((c = scanner_getc(s)) != EOF && predicate(c))
		/* nothing */;

	scanner_ungetc(s); // Unconsume the unmatched char
	return make_token(TOK_CUSTOM, s);
}

static inline void scanner_skip_blanks(Scanner *s)
{
	skip_while_impl(s, isblank);
}

/// @brief Returns a token spanning from current position to position
///        upto which predicate is satisfied by the characters.
/// @param s Scanner
/// @param predicate Unary predicate
/// @return Token
static inline Token scanner_skip_while(Scanner *s, int (*predicate)(int))
{
	s->start = s->at;
	s->current = skip_while_impl(s, predicate);
	return s->current;
}

static Token next_token_impl(Scanner *s)
{
	s->start = s->at;
	int c = scanner_getc(s);

	if (c == EOF)
		return make_token(TOK_EOF, s);
	if (c == ':')
		return make_token(TOK_COLON, s);
	if (c == '\n')
		return make_token(TOK_CRLF, s);

	if (c == '\r' && scanner_peekc(s) == '\n') {
		scanner_getc(s);
		return make_token(TOK_CRLF, s);
	}
	if (isblank(c)) {
		Token tok = skip_while_impl(s, isblank);
		tok.ttype = TOK_BLANKS;
		return tok;
	}
	if (is_name_char(c)) {
		Token tok = skip_while_impl(s, is_name_char);
		tok.ttype = TOK_NAME;
		return tok;
	}

	return make_token(TOK_ERROR, s);
}

/// @brief Returns the next token in stream
/// @param s Scanner
/// @return Token
static Token scanner_next_token(Scanner *s)
{
	s->current = next_token_impl(s);
	return s->current;
}

/// @brief Parse request line: <method <uri> 'HTTP'/<digit>'.'<digit> CRLF
/// @param r
/// @return true on success
static bool parse_request_line(Scanner *s, HTTPHeader *r)
{
	// Parse method type.
	r->method = METHOD_UNKNOWN;
	SCANNER_CONSUME(s, TOK_NAME);

	for (int i = 0; i < METHOD_UNKNOWN; ++i) {
		String name = METHOD_NAME_STRINGS[i];
		if (string_eq_case(name, name)) {
			r->method = i;
			break;
		}
	}
	SCANNER_CONSUME(s, TOK_BLANKS);

	// Parse URI
	Token uri = scanner_skip_while(s, is_uri_char);
	r->uri = (RequestURI){.full = uri.lexeme};
	SCANNER_CONSUME(s, TOK_BLANKS);

	// Parse HTTP version
	Token ver = scanner_skip_while(s, is_not_crlf);
	if (string_eq_case(ver.lexeme, CSTRING("HTTP/1.0")))
		r->version = 10;
	else if (string_eq_case(ver.lexeme, CSTRING("HTTP/1.1")))
		r->version = 11;
	else
		return false;

	SCANNER_CONSUME(s, TOK_CRLF);
	return true;
}

/// @brief Parse request fields: (<name> ':' <value> CRLF)* CRLF
/// @param r
/// @return true on success
static bool parse_request_fields(Scanner *s, HTTPHeader *r)
{
	while (1) {
		Token name = scanner_next_token(s);
		// CRLF CRLF marks the end of header
		if (name.ttype == TOK_CRLF)
			return true;

		// A field is like: name ':' blanks? value CRLF
		TOKEN_EXPECT(name, TOK_NAME);
		String header_name = s->current.lexeme;

		SCANNER_CONSUME(s, TOK_COLON);
		scanner_skip_blanks(s);

		scanner_skip_while(s, is_not_crlf);
		String value = s->current.lexeme;

		SCANNER_CONSUME(s, TOK_CRLF);

		// Check if it is a standard header name we know.
		int std_header = -1;
		for (int i = 0; i < HNAME_COUNT; ++i) {
			if (!string_eq_case(header_name, HEADER_NAME_STRINGS[i]))
				continue;

			// We do not allow repeating any std header names.
			if (!string_is_null(r->std_fields[i]))
				return false;
			std_header = i;
			break;
		}

		if (std_header != -1) {
			r->std_fields[std_header] = value;
		} else {
			if (r->extra_field_cnt == EXTRA_FIELDS_MAX)
				return false;

			HeaderField field = {.name = header_name, .value = value};
			r->extra_fields[r->extra_field_cnt++] = field;
		}

		return true;
	}
}

// static bool parse_request_data(Scanner *s, HTTPRequest *r) {}

bool parse_request(HTTPHeader *r)
{
	// Make all field values null strings, because that's how we check if a
	// specific header-field has been seen or not for headers we track.
	memset(r->std_fields, 0, sizeof(r->std_fields));
	r->extra_field_cnt = 0;

	Scanner s;
	scanner_init(&s, r->header_data, r->header_len);

	// Grab the first line first.
	for (int i = 0; i < r->header_len; ++i) {
		char c = r->header_data[i];
		if (c != '\n' && c != '\r')
			continue;
		r->first_line = STRING(r->header_data, i);
		break;
	}

	if (!parse_request_line(&s, r))
		return false;

	if (!parse_request_fields(&s, r))
		return false;

	return true;
}
