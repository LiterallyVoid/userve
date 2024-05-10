#include "test/arraylist.h"

#include "types/arraylist.h"

typedef struct Item {
	int value;
	int unused[15];
} Item;

void test_arraylist(TestContext *ctx) {
	test(ctx, "arraylist");

	Error err;

	test(ctx, "arraylist append");

	ArrayList list;
	arraylist_init(&list, sizeof(Item));

	err = arraylist_append(&list, &(Item) {
		.value = 40,
	});
	EXPECT(ctx, err == ERR_SUCCESS);

	for (int i = 1; i < 12; i++) {
		err = arraylist_append(&list, &(Item) {
			.value = i,
		});
		EXPECT(ctx, err == ERR_SUCCESS);
	}

	err = arraylist_append(&list, &(Item) {
		.value = 100,
	});
	EXPECT(ctx, err == ERR_SUCCESS);

	EXPECT(ctx, ((Item*) arraylist_get(&list, 0))->value == 40);
	EXPECT(ctx, ((Item*) arraylist_get(&list, 1))->value == 1);

	EXPECT(ctx, ((Item*) arraylist_get(&list, 11))->value == 11);
	EXPECT(ctx, ((Item*) arraylist_get(&list, 12))->value == 100);

	arraylist_deinit(&list);

	test(ctx, "arraylist reserve");

	arraylist_init(&list, sizeof(Item));

	arraylist_reserve_total(&list, 7);
	EXPECT(ctx, list.cap >= 7);

	arraylist_reserve_total(&list, 8);
	EXPECT(ctx, list.cap >= 8);

	// The arraylist is empty; `reserve_additional` reserves more starting from
	// `len`, not from `cap`. Warning: this is a test of the implementation;
	// efficiency is not part of the guarantees that ArrayList provides.
	arraylist_reserve_additional(&list, 8);
	EXPECT(ctx, list.cap < 16);

	// Room for eight items should be guaranteed.
	for (int i = 0; i < 8; i++) {
		arraylist_append_assume_capacity(&list, &(Item) {
			.value = 1234,
		});
	}

	arraylist_deinit(&list);
}
