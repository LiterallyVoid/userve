#include "types/slice.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

Slice slice_new(void) {
	return (Slice) {
		.bytes = NULL,
		.len = 0,
	};
}

Slice slice_from_len(uint8_t *bytes, size_t len) {
	return (Slice) {
		.bytes = bytes,
		.len = len,
	};
}

Slice slice_from_cstr(const char *cstr) {
	return slice_from_len((uint8_t*) cstr, strlen(cstr));
}

bool slice_equal(Slice a, Slice b) {
	// If both strings are empty, don't look at their bytes.
	if (a.len == 0 && b.len == 0) return true;

	if (a.len != b.len) return false;
	if (memcmp(a.bytes, b.bytes, a.len) != 0) return false;

	return true;
}

Slice slice_remove_start(Slice self, size_t n) {
	assert(n <= self.len);
	return slice_from_len(
		self.bytes + n,
		self.len - n
	);
}

Slice slice_keep_bytes_from_end(Slice self, size_t n) {
	assert(n <= self.len);
	return slice_from_len(
		self.bytes + self.len - n,
		n
	);
}

bool slice_remove_suffix(Slice *mut_slice, Slice suffix) {
	if (mut_slice->len <= suffix.len) return false;

	if (memcmp(
		mut_slice->bytes + mut_slice->len - suffix.len,
		suffix.bytes,
		suffix.len
	) != 0) {
		return false;
	}

	mut_slice->len -= suffix.len;

	return true;
}

OwnedSlice slice_clone(Slice slice) {
	// @TODO: error handling?
	uint8_t *bytes = malloc(slice.len);
	assert(bytes != NULL);

	memcpy(bytes, slice.bytes, slice.len);

	return slice_from_len(bytes, slice.len);
}

OwnedSlice slice_concat(Slice a, Slice b) {
	// @TODO: error handling?
	uint8_t *bytes = malloc(a.len + b.len);
	assert(bytes != NULL);

	memcpy(bytes, a.bytes, a.len);
	memcpy(bytes + a.len, b.bytes, b.len);

	return slice_from_len(bytes, a.len + b.len);
}

void slice_free(OwnedSlice slice) {
	free(slice.bytes);
}
