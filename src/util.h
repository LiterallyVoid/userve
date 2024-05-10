#pragma once

#include "types/buffer.h"
#include "types/error.h"

#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>

// If the next power of two is unrepresentable, this function will panic.
size_t next_power_of_two(size_t num);

// Mark `len` bytes of `ptr` as undefined by setting each byte to 0xAA.
// Intended to make reads of undefined values *very* loud.
void set_undefined(void *ptr, size_t len);

// Write all of `slice` to `fd`, returning an error if `write` fails.
Error write_all_to_fd(int fd, Slice slice);
