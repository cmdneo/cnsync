#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#include <stdbool.h>

#include "request.h"

/// @brief Parse the request and put all data in request.
/// @param r
/// @return true on success
bool parse_request(Request *request);

#endif
