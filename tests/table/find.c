#include <string.h>

#include "../check.h"
#include "table.h"
#include "testenv.h"

/* Test whether the initial insert was correct */
void
test(struct TestEnv *env)
{
	for (unsigned int i = 0; i < env->N; i++) {
		unsigned long x;
		void **res;

		x = 1;
		for (size_t j = 0; j < strlen(env->keys[i]); j++) {
			x *= (unsigned long)env->keys[i][j];
		}
		res = table_find(&env->tbl, env->keys[i]);
		assert_not_null(res);
		assert_ulong_eq((unsigned long)*res, x);
	}
}
