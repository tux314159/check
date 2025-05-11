#include "../check.h"
#include "testenv.h"

void
test(struct TestEnv *env)
{
	test_output(msgt_warn, "expected to fail", 0);
	assert_int_eq(69, 420);
}
