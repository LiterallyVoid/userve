#include "test/arguments.h"
#include "main/arguments.h"

#include <string.h>

void test_arguments(TestContext *ctx) {
	test(ctx, "arguments");

	Arguments arguments;

	arguments_parse(&arguments, 3, (const char*[]) { "@test1", "--address", "foobar" });
	EXPECT(ctx, strcmp(arguments.address, "foobar") == 0);

	arguments_parse(&arguments, 2, (const char*[]) { "@test2", "--address=1234" });
	EXPECT(ctx, strcmp(arguments.address, "1234") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test3", "-a", "--address" });
	EXPECT(ctx, strcmp(arguments.address, "--address") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test4", "-p", "-t" });
	EXPECT(ctx, strcmp(arguments.port, "-t") == 0);
	EXPECT(ctx, !arguments.test);

	arguments_parse(&arguments, 4, (const char*[]) { "@test5", "-p", "", "-t" });
	EXPECT(ctx, strcmp(arguments.port, "") == 0);
	EXPECT(ctx, arguments.test);
}


