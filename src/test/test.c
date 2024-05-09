#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../arguments.h"
static void test_arguments(void) {
	Arguments arguments;

	arguments_parse(&arguments, 2, (const char*[]) { "@test0", "--address", "foobar" });
	assert(strcmp(arguments.address, "localhost") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test1", "--address", "foobar" });
	assert(strcmp(arguments.address, "foobar") == 0);

	arguments_parse(&arguments, 2, (const char*[]) { "@test2", "--address=1234" });
	assert(strcmp(arguments.address, "1234") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test3", "-a", "--address" });
	assert(strcmp(arguments.address, "--address") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test4", "-p", "-t" });
	assert(strcmp(arguments.port, "-t") == 0);
	assert(!arguments.test);

	arguments_parse(&arguments, 4, (const char*[]) { "@test5", "-p", "", "-t" });
	assert(strcmp(arguments.port, "") == 0);
	assert(arguments.test);
}

#include "../buffer.h"
static void test_buffer(void) {
	Buffer buffer;

	buffer_init(&buffer);

	buffer_concat(&buffer, slice_from_cstr("12345"));
	buffer_concat(&buffer, slice_from_cstr("678"));
	assert(buffer.len == 8);
	assert(buffer_slice(&buffer).len == 8);
	assert(memcmp(buffer.bytes, "12345678", 8) == 0);

	buffer_deinit(&buffer);


	buffer_init(&buffer);

	buffer_concat_printf(&buffer, "hey %d", 123);
	assert(slice_equal(buffer_slice(&buffer), slice_from_cstr("hey 123")));

	buffer_deinit(&buffer);


	buffer_init(&buffer);

	buffer_reserve_total(&buffer, 10);
	assert(buffer_slice(&buffer).len == 0);
	assert(buffer_uninitialized(&buffer).len >= 10);

	buffer_reserve_total(&buffer, 1000);
	assert(buffer_slice(&buffer).len == 0);
	assert(buffer_uninitialized(&buffer).len >= 1000);

	buffer_reserve_additional(&buffer, 1020);
	assert(buffer_slice(&buffer).len == 0);
	assert(buffer_uninitialized(&buffer).len >= 1020);

	for (int i = 0; i < 102; i++) {
		buffer_concat(&buffer, slice_from_cstr("123456789A"));
	}
	assert(buffer_slice(&buffer).len == 1020);

	buffer_clear(&buffer);
	assert(buffer_slice(&buffer).len == 0);
	assert(buffer_uninitialized(&buffer).len >= 1020);

	buffer_clear_capacity(&buffer);
	assert(buffer_slice(&buffer).len == 0);
	assert(buffer_uninitialized(&buffer).len == 0);

	buffer_deinit(&buffer);
}

static void test_slice(void) {
	Slice a = slice_from_cstr("abc xyz def");

	assert(slice_equal(a, slice_from_cstr("abc xyz def")));
	assert(!slice_equal(a, slice_from_cstr("abc xyz")));
	assert(!slice_equal(a, slice_from_cstr("abc xyz defg")));
	assert(!slice_equal(a, slice_from_cstr("12345678")));

	assert(slice_equal(a, slice_from_len((uint8_t*) "abc xyz def zyx", 11)));

	assert(slice_equal(slice_remove_start(a, 0), a));
	assert(slice_equal(slice_remove_start(a, 4), slice_from_cstr("xyz def")));
	assert(slice_equal(slice_remove_start(a, 11), slice_new()));

	assert(slice_equal(slice_keep_bytes_from_end(a, 0), slice_from_cstr("")));
	assert(slice_equal(slice_keep_bytes_from_end(a, 2), slice_from_cstr("ef")));
	assert(slice_equal(slice_keep_bytes_from_end(a, 11), a));
}

void test_all(void) {
	printf("test arguments\n");
	test_arguments();

	printf("test buffer\n");
	test_buffer();

	printf("test slice\n");
	test_slice();

	printf("all tests passed\n");
}
