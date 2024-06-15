#include <dlfcn.h>
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
		// TODO
		n_tests = 0;
		test_obj_basenames = malloc(sizeof(*test_obj_basenames));
		test_obj_basenames[0] = NULL;
	} else {
		n_tests = (unsigned)argc - 3;
		test_obj_basenames = malloc((n_tests + 1) * sizeof(*test_obj_basenames));
		for (size_t i = 0; i < n_tests; i++){
			test_obj_basenames[i] = argv[i + 3];
		}
		test_obj_basenames[n_tests] = NULL;
	}

	/* Allocate path size large enough for all */
	path_size = strlen(argv[1]) + MAX(max_strlen(test_obj_basenames), strlen("setup")) + sizeof(TEST_SUFFIX);
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
		pid_t pid;
		int child_stat;

		void *setup_obj;
		void (*setup_env)(struct TestEnv **env);

		pid = fork();
		assert_int_neq(pid, -1);
		if (pid) { // parent
			wait_and_report();
		} else { // child
			setup_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
			assert_not_null(setup_obj);
			setup_env =
				(void (*)(struct TestEnv **))dlsym(setup_obj, "setup_env");
			assert_not_null(setup_env);
			setup_env(&env);
			dlclose(setup_obj);
			_exit(0);
		}
	}

	for (size_t i = 0; i < n_tests; i++) {
		pid_t pid;
		int child_stat;

		void *test_obj;
		void (*test)(struct TestEnv *env);

		printf(" test " T_ITAL "%s" T_NORM "... ", test_obj_basenames[i]);
		pid = fork();
		assert_int_neq(pid, -1);
		if (pid) { // parent
			wait_and_report();
		} else { // child

			sprintf(path, "%s/%s.tst", argv[2], test_obj_basenames[i]);
			test_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
			assert_not_null(test_obj);
			test = (void (*)(struct TestEnv *))dlsym(test_obj, "test");
			assert_not_null(test);
			test(env);
			dlclose(test_obj);
			_exit(0);
		}
	}

	free(test_obj_basenames);
}
