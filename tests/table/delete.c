#include "../check.h"
#include "table.h"
#include "testenv.h"

#include <stdbool.h>
#include <string.h>

/* Test whether deletion works */
void
test(struct TestEnv *env)
{
	bool *deleted;

	deleted = calloc(env->N, sizeof(*deleted));
	assert_not_null(deleted);

	/* Randomly delete a third of the values */
	for (unsigned int i = 0; i < env->N / 3; i++) {
		unsigned long x;

		x = random_ulong() % env->N;
		if (deleted[x])
			assert_int_eq(table_delete(&env->tbl, env->keys[x]), -1);
		else
			assert_int_eq(table_delete(&env->tbl, env->keys[x]), 0);
		deleted[x] = 1;
	}

	for (unsigned int i = 0; i < env->N; i++) {
		unsigned long x;
		void **res;

		x = 1;
		for (size_t j = 0; j < strlen(env->keys[i]); j++) {
			x *= (unsigned long)env->keys[i][j];
		}
		res = table_find(&env->tbl, env->keys[i]);
		if (deleted[i]) {
			assert_null(res);
		} else {
			assert_not_null(res);
			assert_ulong_eq((unsigned long)*res, x);
		}
	}

	free(deleted);
}
