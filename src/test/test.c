#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../arguments.h"
static void test_arguments(void) {
	Arguments arguments;

	arguments_parse(&arguments, 2, (const char*[]) { "--address", "--address", "foobar" });
	assert(strcmp(arguments.address, "localhost") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "--address", "--address", "foobar" });
	assert(strcmp(arguments.address, "foobar") == 0);

	arguments_parse(&arguments, 2, (const char*[]) { "--address", "--address=1234" });
	assert(strcmp(arguments.address, "1234") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "--address", "-a", "--address" });
	assert(strcmp(arguments.address, "--address") == 0);
}

void test_all(void) {
	printf("test arguments\n");
	test_arguments();
	printf("all tests passed\n");
}
