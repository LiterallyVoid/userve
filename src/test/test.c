#include "test/test.h"

#include "test/arguments.h"
#include "test/http_parser.h"

#include "warble/test.h"

#include <stdio.h>

Error test_all(void) {
	TestContext ctx;

	test_context_init(&ctx);

	printf("test arguments\n");
	test_arguments(&ctx);

	printf("test http parser\n");
	test_http_parser(&ctx);

	test_context_report(&ctx);

	return ERR_SUCCESS;
}
