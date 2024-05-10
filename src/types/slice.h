#pragma once

#include <stdint.h>
#include <sys/types.h>

// An unowned array of bytes.
typedef struct Slice {
	uint8_t *bytes;
	size_t len;
} Slice;

// Return a new zero-length slice.
Slice slice_new(void);

// Create a new slice of the first `len` bytes of `bytes`.
Slice slice_from_len(uint8_t *bytes, size_t len);

// Create a new slice from a NUL-terminated string.
Slice slice_from_cstr(const char *cstr);

// Returns true if the byte arrays `a` and `b` compare equal.
bool slice_equal(Slice a, Slice b);

// Return `slice` with the first `n` bytes removed.
// Panics if the slice has less than `n` bytes.
Slice slice_remove_start(Slice self, size_t n);

// Return a new slice of `self`'s last `n` bytes.
// Panics if that slice would be out-of-bounds.
Slice slice_keep_bytes_from_end(Slice self, size_t n);

// If `mut_slice` ends with `suffix`, change the slice so it no longer contains
// the suffix and return `true`. Otherwise, return `false.`
bool slice_remove_suffix(Slice *mut_slice, Slice suffix);

// ### Allocated slice

typedef Slice OwnedSlice;

OwnedSlice slice_clone(Slice slice);
OwnedSlice slice_concat(Slice a, Slice b);
void slice_free(OwnedSlice slice);
