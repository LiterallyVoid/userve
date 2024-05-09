#include "buffer.h"
#include "../buffer.h"

#include <string.h>

void test_buffer(TestContext *ctx) {
	test(ctx, "buffer");

	Buffer buffer;

	test(ctx, "buffer concat");
	buffer_init(&buffer);

	buffer_concat(&buffer, slice_from_cstr("12345"));
	buffer_concat(&buffer, slice_from_cstr("678"));
	EXPECT(ctx, buffer.len == 8);
	EXPECT(ctx, buffer_slice(&buffer).len == 8);
	EXPECT(ctx, memcmp(buffer.bytes, "12345678", 8) == 0);

	buffer_deinit(&buffer);


	test(ctx, "buffer concat_printf");
	buffer_init(&buffer);

	buffer_concat_printf(&buffer, "hey %d", 123);
	EXPECT(ctx, slice_equal(buffer_slice(&buffer), slice_from_cstr("hey 123")));

	buffer_deinit(&buffer);


	test(ctx, "buffer reserve");
	buffer_init(&buffer);

	buffer_reserve_total(&buffer, 10);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 10);

	buffer_reserve_total(&buffer, 1000);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1000);

	buffer_reserve_additional(&buffer, 1020);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1020);

	for (int i = 0; i < 102; i++) {
		buffer_concat(&buffer, slice_from_cstr("123456789A"));
	}
	EXPECT(ctx, buffer_slice(&buffer).len == 1020);

	test(ctx, "buffer clear");

	buffer_clear(&buffer);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1020);

	buffer_clear_capacity(&buffer);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len == 0);

	buffer_deinit(&buffer);
}


