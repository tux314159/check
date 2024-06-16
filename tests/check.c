#include <dirent.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

#include "check.h"

/* Configuration */
#define TEST_SUFFIX ".tst"

/* ANSI escapes */
#define T_DIM "\x1b[2m"
#define T_BOLD "\x1b[1m"
#define T_ITAL "\x1b[3m"
#define T_LINE "\x1b[4m"
#define T_GREEN "\x1b[38;5;2m"
#define T_YELLOW "\x1b[38;5;3m"
#define T_RED "\x1b[38;5;1m"
#define T_NORM "\x1b[0m"

extern struct TestEnv;

jmp_buf _assert_trampoline;

static size_t
max_strlen(char **ss)
{
	size_t res = 0;
	for (char **s = ss; *s; s++)
		res = MAX(res, strlen(*s));
	return res;
}

static int
wait_and_report(void)
{
	int child_stat;
	assert_int_neq(wait(&child_stat), -1);

	if (WEXITSTATUS(child_stat) == 0)
		printf(T_GREEN T_BOLD "OK\n" T_NORM);
	else
		printf(T_RED T_BOLD "FAIL\n" T_NORM);
	fflush(stdout);
	fflush(stderr);

	return child_stat;
}

static int
run_test_so(
	const char *path,
	struct TestEnv *env,
	void (*teardown_env)(struct TestEnv *)
)
{
	int child_stat;
	pid_t pid;

	void *test_obj;
	void (*test)(struct TestEnv *env);
	void (*setup_env)(struct TestEnv **env);

	pid = fork();
	assert_int_neq(pid, -1);
	if (pid) { // parent
		assert_int_neq(wait(&child_stat), -1);
	} else { // child
		test_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
		assert_not_null(test_obj);

		test = (void (*)(struct TestEnv *))dlsym(test_obj, "test");
		assert_not_null(test);
		if (!setjmp(_assert_trampoline))
			test(env);
		else
			goto fail;

		if (!setjmp(_assert_trampoline))
			teardown_env(env);
		else
			goto fail;

		dlclose(test_obj);
		_exit(0);
fail:
		dlclose(test_obj);
		_exit(1);
	}

	return child_stat;
}

static int
is_test_so(const struct dirent *ent)
{
	size_t name_len;

	name_len = strlen(ent->d_name);
	if (name_len <= 4 || !strcmp(ent->d_name, "setup.tst"))
		return 0;
	return !strcmp(ent->d_name + name_len - 4, ".tst");
}

/* Usage: $0 <seed> <suite> [<test1> <test2> ...]*/
int
main(int argc, char **argv)
{
	struct TestEnv *env;
	size_t n_tests;
	unsigned int seed;
	char **test_obj_basenames;

	char *path;
	size_t path_size;

	assert_int(argc, >=, 3);

	if (argc == 3) {
		// Find all tests
		struct dirent **test_ents;

		n_tests = (size_t)scandir(argv[2], &test_ents, is_test_so, alphasort);
		assert_ulong_neq(n_tests, -1UL);
		test_obj_basenames =
			malloc((n_tests + 1) * sizeof(*test_obj_basenames));
		for (size_t i = 0; i < n_tests; i++) {
			test_ents[i]->d_name[strlen(test_ents[i]->d_name) - 4] = '\0';
			test_obj_basenames[i] = test_ents[i]->d_name;
		}
	} else {
		n_tests = (unsigned)argc - 3;
		test_obj_basenames =
			malloc((n_tests + 1) * sizeof(*test_obj_basenames));
		for (size_t i = 0; i < n_tests; i++)
			test_obj_basenames[i] = argv[i + 3];
	}
	test_obj_basenames[n_tests] = NULL;

	/* Allocate path size large enough for all */
	path_size = strlen(argv[2]) + 1 +
		MAX(max_strlen(test_obj_basenames), strlen("setup")) +
		sizeof(TEST_SUFFIX);
	path = malloc(path_size);
	sprintf(path, "%s/setup.tst", argv[2]);

	seed = atol(argv[1]);
	srand(seed);

	printf(
		"Running test suite " T_BOLD "%s" T_NORM " (seed %d):\n",
		argv[2],
		seed
	);

	printf(" verifying setup... ");
	{
		void *setup_obj;
		void (*setup_env)(struct TestEnv **env);
		void (*teardown_env)(struct TestEnv *env);

		setup_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
		assert_not_null(setup_obj);
		setup_env = (void (*)(struct TestEnv **))dlsym(setup_obj, "setup_env");
		assert_not_null(setup_env);
		teardown_env =
			(void (*)(struct TestEnv *))dlsym(setup_obj, "teardown_env");
		assert_not_null(teardown_env);

		if (!setjmp(_assert_trampoline)) {
			setup_env(&env);
			printf(T_GREEN T_BOLD "OK\n" T_NORM);
		} else {
			printf(T_RED T_BOLD "FAIL\n" T_NORM);
		}

		for (size_t i = 0; i < n_tests; i++) {
			sprintf(path, "%s/%s.tst", argv[2], test_obj_basenames[i]);
			printf(" test " T_ITAL "%s" T_NORM "... ", test_obj_basenames[i]);
			if (run_test_so(path, env, teardown_env) == 0)
				printf(T_GREEN T_BOLD "OK\n" T_NORM);
			else
				printf(T_RED T_BOLD "FAIL\n" T_NORM);
		}


		dlclose(setup_obj);
	}

	free(test_obj_basenames);
}
