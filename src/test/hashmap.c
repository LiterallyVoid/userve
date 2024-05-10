#include "test/hashmap.h"
#include "types/hashmap.h"

#include <stdlib.h>

void test_hashmap(TestContext *ctx) {
	test(ctx, "hashmap");

	Slice keys[] = {
		slice_from_cstr("test"),
	};

	HashMap h;

	hashmap_init(&h, sizeof(int));

	HashMapEntry entry;

	entry = hashmap_get(&h, keys[0]);
	EXPECT(ctx, entry.key_ptr == NULL);
	EXPECT(ctx, entry.value_ptr == NULL);
	EXPECT(ctx, !entry.occupied);

	Error err = hashmap_put(&h, keys[0], &entry);
	EXPECT(ctx, err == ERR_SUCCESS);

	EXPECT(ctx, entry.key_ptr != NULL);
	EXPECT(ctx, entry.value_ptr != NULL);
	EXPECT(ctx, entry.occupied);

	// The hashmap won't be consistent until a key is written into `entry.key`.
	hashmap_check_consistency(&h);

	*entry.key_ptr = keys[0];
	*(int*) entry.value_ptr = 12345;

	entry = hashmap_get(&h, keys[0]);
	EXPECT(ctx, entry.key_ptr != NULL);
	EXPECT(ctx, entry.value_ptr != NULL);
	EXPECT(ctx, entry.occupied);

	EXPECT(ctx, *(int*) entry.value_ptr == 12345);

	hashmap_deinit(&h);
}
