#pragma once

// Errors are guaranteed to be less than zero; zero or positive values may
// be used for the returned result.
typedef enum Error {
	ERR_SUCCESS = 0,
	ERR_UNKNOWN = -1,
	ERR_OUT_OF_MEMORY = -2,
	ERR_NO_SPACE = -3,
	ERR_NOT_FOUND = -4,

	ERR_PARSE_FAILED = -100,
	ERR_HTTP_NOT_FOUND = -101,

} Error;

// Returns a pointer to a NUL-terminated static string. As a consequence, this
// string can't contain the number of an unrecognized error (one that's not in
// the Error enum)
const char *error_to_string(Error self);
