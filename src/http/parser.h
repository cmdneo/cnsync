#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

#include "common.h"
#include "request.h"

/// @brief Parse the request and put all data in request.
/// @param r
/// @return true on success
bool parse_request(HTTPHeader *request);

#endif
