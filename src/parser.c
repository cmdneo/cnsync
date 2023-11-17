#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "request.h"
#include "common.h"
#include "parser.h"

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

/// Compare the strings, case-insesetive.
static inline bool eq_case_string(String s1, String s2)
{
	return s1.len == s2.len && !strncasecmp(s1.data, s2.data, s1.len);
}

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
static bool parse_request_line(Scanner *s, Request *r)
{
	// Parse method type.
	r->method = METHOD_UNKNOWN;
	SCANNER_CONSUME(s, TOK_NAME);

	for (int i = 0; i < METHOD_UNKNOWN; ++i) {
		String name = HTTP_METHOD_NAMES[i];
		if (eq_case_string(name, name)) {
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
	if (eq_case_string(ver.lexeme, CSTRING("HTTP/1.0"))) {
		r->version.major = 1;
		r->version.minor = 0;
	} else if (eq_case_string(ver.lexeme, CSTRING("HTTP/1.1"))) {
		r->version.major = 1;
		r->version.minor = 1;
	} else {
		return false;
	}

	SCANNER_CONSUME(s, TOK_CRLF);
	return true;
}

/// @brief Parse request fields: (<name> ':' <value> CRLF)* CRLF
/// @param r
/// @return true on success
static bool parse_request_fields(Scanner *s, Request *r)
{
	while (1) {
		Token name = scanner_next_token(s);
		// CRLF CRLF marks the end of header
		if (name.ttype == TOK_CRLF)
			return true;
		// Too many fields
		if (r->field_cnt == FIELDS_MAX)
			return false;

		HeaderField *f = &r->fields[r->field_cnt++];

		// A field is like: name ':' blanks? value CRLF
		TOKEN_EXPECT(name, TOK_NAME);
		f->name = s->current.lexeme;

		SCANNER_CONSUME(s, TOK_COLON);
		if (isblank(scanner_peekc(s)))
			SCANNER_CONSUME(s, TOK_BLANKS);

		scanner_skip_while(s, is_not_crlf);
		f->value = s->current.lexeme;

		SCANNER_CONSUME(s, TOK_CRLF);
	}
}

// static bool parse_request_data(Scanner *s, Request *r) {}

bool parse_request(Request *r)
{
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
