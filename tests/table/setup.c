#include <stdlib.h>
#include <string.h>

#include "../check.h"
#include "table.h"
#include "testenv.h"

void
populate_table(struct TestEnv *env)
{
	/* Populate the table:
	 * tbl[key] = prod(key)
	 */
	const size_t width = 32;
	char buf[width];

	for (unsigned i = 0; i < env->N; i++) {
		unsigned long x;

		do {
			random_string(buf, width);
		} while (table_find(&env->tbl, buf));

		x = 1;
		for (size_t j = 0; j < width - 1; j++) {
			x *= (unsigned long)buf[j];
		}
		assert_int_neq(table_insert(&env->tbl, buf, (void *)x), -1);

		env->keys[i] = malloc(width);
		assert_not_null(env->keys[i]);
		memcpy(env->keys[i], buf, width);
	}
}

void
setup_env(struct TestEnv **env)
{
	*env = malloc(sizeof(**env));
	assert_not_null(env);

	(*env)->N = 1000000;

	(*env)->keys = malloc((*env)->N * sizeof(*(*env)->keys));
	assert_not_null((*env)->keys);
	assert_int_neq(table_init(&(*env)->tbl), -1);


	populate_table(*env);
}

void
teardown_env(struct TestEnv *env)
{
	(void)env;
	free(env->keys);
	table_destroy(&env->tbl);
	free(env);
}
