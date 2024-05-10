#pragma once

#include "types/error.h"
#include "types/slice.h"

#include <stdint.h>
#include <sys/types.h>

// A mutable array of bytes.
typedef struct Buffer {
	// An allocation of size `cap`, of which the first `len` bytes are initialized.
	uint8_t *bytes;

	// invariant: 0 <= len < self->cap
	size_t len;

	// The capacity of this buffer.
	size_t cap;
} Buffer;

void buffer_init(Buffer *self);
void buffer_deinit(Buffer *self);

// Make sure `self` has capacity to store `total` bytes in total.
//
// This function can return ERR_OUT_OF_MEMORY.
Error buffer_reserve_total(Buffer *self, size_t total);

// Make sure `self` has capacity to store `additional` more bytes.
//
// This function can return ERR_OUT_OF_MEMORY.
Error buffer_reserve_additional(Buffer *self, size_t additional);

// Empty `self`, without affecting capacity.
void buffer_clear(Buffer *self);

// Empty `self` and free capacity.
void buffer_clear_capacity(Buffer *self);

// Concatenate `slice` to the end of `self`.
Error buffer_concat(Buffer *self, Slice slice);

// Concatenate `slice` to the end of `self`, panicking if `self` doesn't have
// the capacity for it.
void buffer_concat_assume_capacity(Buffer *self, Slice slice);

// Concatenate a `printf`-formatted string to `self`. If `printf` encounters
// an error, this function returns ERR_UNKNOWN. Otherwise, return `ERR_SUCCESS`.
__attribute__((__format__(__printf__, 2, 3)))
Error buffer_concat_printf(Buffer *self, const char *fmt, ...);

// Return a slice to the buffer's content, i.e. `self.buffer[0..self.len]`
Slice buffer_slice(Buffer *self);

// Return a slice to the uninitialized portion of this buffer.
Slice buffer_uninitialized(Buffer *self);
