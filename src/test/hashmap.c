#include "test/hashmap.h"
#include "types/hashmap.h"

void test_hashmap(TestContext *ctx) {
	test(ctx, "hashmap");

	HashMap h;
	hashmap_init(&h);
}


