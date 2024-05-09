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

void test_all(void) {
	printf("test arguments\n");
	test_arguments();
	printf("all tests passed\n");
}
