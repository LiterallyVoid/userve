#include "slice.h"
#include "../types/slice.h"

void test_slice(TestContext *ctx) {
	test(ctx, "slice");
	Slice a = slice_from_cstr("abc xyz def");

	test(ctx, "slice equal");
	EXPECT(ctx, slice_equal(a, slice_from_cstr("abc xyz def")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("abc xyz")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("abc xyz defg")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("12345678")));

	test(ctx, "slice from_len");
	EXPECT(ctx, slice_equal(a, slice_from_len((uint8_t*) "abc xyz def zyx", 11)));

	test(ctx, "slice remove_start");
	EXPECT(ctx, slice_equal(slice_remove_start(a, 0), a));
	EXPECT(ctx, slice_equal(slice_remove_start(a, 4), slice_from_cstr("xyz def")));
	EXPECT(ctx, slice_equal(slice_remove_start(a, 11), slice_new()));

	test(ctx, "slice keep_bytes_from_end");
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 0), slice_from_cstr("")));
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 2), slice_from_cstr("ef")));
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 11), a));
}


