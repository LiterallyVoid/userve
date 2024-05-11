#pragma once

#include "warble/slice.h"
#include "warble/error.h"

// Write all of `slice` to `fd`, returning an error if `write` fails.
Error write_all_to_fd(int fd, Slice slice);

// Doesn't really belong in this file, but whatever.
Slice detect_content_type(Slice path);
