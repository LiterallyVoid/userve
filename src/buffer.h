#pragma once

#include "error.h"

#include <sys/types.h>

/// TODO: rename this to bytes.{h,c} instead of buffer.

// An unowned array of bytes.
typedef struct Slice {
	uint8_t *bytes;
	size_t len;
} Slice;

// A mutable array of bytes.
typedef struct Buffer {
	// An allocation of size `cap`, of which the first `len` bytes are initialized.
	uint8_t *bytes;

	// invariant: 0 <= len < self->cap
	size_t len;

	// The capacity of this buffer.
	size_t cap;
} Buffer;

// Return a new zero-length slice.
Slice slice_new(void);

// Create a new slice of the first `len` bytes of `bytes`.
Slice slice_from_len(uint8_t *bytes, size_t len);

// Create a new slice from a NUL-terminated string.
Slice slice_from_cstr(const char *cstr);

// Return `slice` with the first `n` bytes removed.
// Panics if the slice has less than `n` bytes.
Slice slice_remove_start(Slice self, size_t n);

// Return a new slice of `self`'s last `n` bytes.
// Panics if that slice would be out-of-bounds.
Slice slice_keep_bytes_from_end(Slice self, size_t n);

void buffer_init(Buffer *self);

// Free a buffer's memory.
void buffer_deinit(Buffer *self);

// Make sure `self` has capacity to store `total` bytes in total.
//
// This function can return ERR_OUT_OF_MEMORY.
Error buffer_reserve_total(Buffer *self, size_t total);

// Make sure `self` has capacity to store `additional` more bytes.
//
// This function can return ERR_OUT_OF_MEMORY.
Error buffer_reserve_additional(Buffer *self, size_t additional);

// Concatenate `slice` to the end of `self`.
Error buffer_concat(Buffer *self, Slice slice);

// Concatenate a `printf`-formatted string to `self`. If `printf` encounters
// an error, this function returns ERR_UNKNOWN. Otherwise, return the number of
// bytes written.
__attribute__((__format__(__printf__, 2, 3)))
int buffer_concat_printf(Buffer *self, const char *fmt, ...);

// Return a slice to the buffer's content, i.e. `self.buffer[0..self.len]`
Slice buffer_slice(Buffer *self);

// Return a slice to the uninitialized portion of this buffer.
Slice buffer_uninitialized(Buffer *self);
