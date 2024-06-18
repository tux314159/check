#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
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

struct Suite {
	size_t n_tests;

	char *name;
	char **test_basenames;
};

/* Globals */

jmp_buf _assert_trampoline;
char *testdir_path;

/* Utility functions */

static size_t
max_strlen(char **ss)
{
	size_t res = 0;
	for (char **s = ss; *s; s++)
		res = MAX(res, strlen(*s));
	return res;
}

/* scandir(3p) filters */

static int
is_test_so(const struct dirent *ent)
{
	size_t name_len;

	name_len = strlen(ent->d_name);
	if (name_len <= 4 || !strcmp(ent->d_name, "setup.tst"))
		return 0;
	return !strcmp(ent->d_name + name_len - 4, ".tst");
}

static int
is_suite_dir(const struct dirent *ent)
{
	struct stat sb;
	char *buf;

	buf = malloc(strlen(testdir_path) + strlen(ent->d_name) + 2);
	assert_not_null(buf);
	sprintf(buf, "%s/%s", testdir_path, ent->d_name);
	assert_int_neq(stat(buf, &sb), -1);
	free(buf);

	return S_ISDIR(sb.st_mode) && strcmp(ent->d_name, ".") &&
		strcmp(ent->d_name, "..");

	// TODO: actual robust checks
}

/* Actual logic */

static void
redirect_io_begin(int output_file)
{
	fflush(stdout);
	fflush(stderr);
	dup2(output_file, STDOUT_FILENO);
	dup2(output_file, STDERR_FILENO);
}

static char *
redirect_io_end(int output_file, int stdout_save, int stderr_save)
{
	char *output;
	size_t output_len;

	dup2(stdout_save, STDOUT_FILENO);
	dup2(stderr_save, STDERR_FILENO);
	output_len = (size_t)lseek(output_file, 0, SEEK_END);
	output = malloc((size_t)output_len + 1);
	output[output_len] = '\0';
	lseek(output_file, 0, SEEK_SET);
	read(output_file, output, output_len);
	ftruncate(output_file, 0);

	return output;
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

	return WEXITSTATUS(child_stat);
}

static void
run_suite(const struct Suite *suite)
{
	void *setup_obj;
	void (*setup_env)(struct TestEnv **env);
	void (*teardown_env)(struct TestEnv *env);

	char *output;
	int output_file;
	char output_file_name[] = "/tmp/check-XXXXXX";
	int stdout_save, stderr_save;

	char *path;
	size_t path_size;

	struct TestEnv *env;

	/* Allocate buffer large enough for all test paths */
	path_size = max_strlen(suite->test_basenames);
	path_size = strlen(testdir_path) + 1 + MAX(path_size, strlen("setup")) +
		sizeof(TEST_SUFFIX);
	path = malloc(path_size);

	sprintf(path, "%s/%s/setup.tst", testdir_path, suite->name);
	setup_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	assert_not_null(setup_obj);
	setup_env = (void (*)(struct TestEnv **))dlsym(setup_obj, "setup_env");
	assert_not_null(setup_env);
	teardown_env = (void (*)(struct TestEnv *))dlsym(setup_obj, "teardown_env");
	assert_not_null(teardown_env);

	// Look, realistically dup(2) and fflush won't fail
	stdout_save = dup(STDOUT_FILENO);
	stderr_save = dup(STDERR_FILENO);
	output_file = mkstemp(output_file_name);
	assert_int_neq(output_file, -1);

	redirect_io_begin(output_file);
	if (!setjmp(_assert_trampoline)) {
		setup_env(&env);
		output = redirect_io_end(output_file, stdout_save, stderr_save);
		printf(T_GREEN T_BOLD "OK" T_NORM "\n");
		printf("%s", output);
		free(output);
	} else {
		output = redirect_io_end(output_file, stdout_save, stderr_save);
		printf(T_RED T_BOLD "FAIL [skipping test suite]" T_NORM "\n");
		printf("%s", output);
		free(output);
		exit(0);
	}

	for (size_t i = 0; i < suite->n_tests; i++) {
		int exit_status;

		sprintf(
			path,
			"%s/%s/%s.tst",
			testdir_path,
			suite->name,
			suite->test_basenames[i]
		);
		printf(" test " T_ITAL "%s" T_NORM "... ", suite->test_basenames[i]);
		redirect_io_begin(output_file);
		exit_status = run_test_so(path, env, teardown_env);
		output = redirect_io_end(output_file, stdout_save, stderr_save);

		if (exit_status == 0)
			printf(T_GREEN T_BOLD "OK" T_NORM "\n");
		else
			printf(T_RED T_BOLD "FAIL" T_NORM "\n");
		printf("%s", output);
		free(output);
	}
	close(output_file);
	dlclose(setup_obj);
}

/* Usage: $0 <seed> <suite> [<test1> <test2> ...]*/
int
main(int argc, char **argv)
{
	size_t n_suites;
	struct Suite *suites;
	char **suite_paths;

	unsigned int seed;
	int run_all_flag;

	/* Initial setup */

	if (setjmp(_assert_trampoline)) {
		perror("Error");
		exit(1);
	}

	testdir_path = malloc(strlen(argv[0]) + 1);
	assert_not_null(testdir_path);
	strcpy(testdir_path, argv[0]);
	strcpy(testdir_path, dirname(testdir_path));

	/* Parse options */

	opterr = 0;
	run_all_flag = 0;
	srand(time(NULL));
	seed = (unsigned)rand() / 2;
	{
		char c;
		char *bad_char;

		while ((c = getopt(argc, argv, ":s:a")) != -1) {
			switch (c) {
			case 's':
				seed = strtol(optarg, &bad_char, 10);
				if (*bad_char) {
					fprintf(stderr, "Invalid seed: %s\n", optarg);
					exit(1);
				}
				break;
			case 'a':
				run_all_flag = 1;
				break;
			case '?':
				fprintf(stderr, "Unknown option: -%c\n", optopt);
				exit(1);
			case ':':
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
				exit(1);

			default:
				assert_int(0, ==, 1);
			}
		}
	}

	/* Find tests to run */

	/* Find suites */
	if (run_all_flag) {
		struct dirent **suite_dirs;
		suite_dirs = NULL;

		n_suites =
			(size_t)scandir(testdir_path, &suite_dirs, is_suite_dir, alphasort);
		assert_ulong_neq(n_suites, -1UL);
		suite_paths = malloc((n_suites + 1) * sizeof(*suite_paths));
		assert_not_null(suite_dirs);
		for (size_t i = 0; i < n_suites; i++) {
			suite_paths[i] = suite_dirs[i]->d_name;
		}
	} else {
		n_suites = (unsigned)(argc - optind);
		suite_paths = malloc((n_suites + 1) * sizeof(*suite_paths));
		assert_not_null(suite_paths);
		for (size_t i = 0; i < n_suites; i++)
			suite_paths[i] = argv[(unsigned)optind + i];
	}
	suite_paths[n_suites] = NULL;
	suites = malloc(n_suites * sizeof(*suites));

	/* Collect all the tests */
	{
		char *suite_path;
		struct dirent **test_ents;

		suite_path = malloc(strlen(testdir_path) + max_strlen(suite_paths) + 2);
		assert_not_null(suite_path);
		for (size_t i = 0; i < n_suites; i++) {
			sprintf(suite_path, "%s/%s", testdir_path, suite_paths[i]);
			suites[i].n_tests =
				(size_t)scandir(suite_path, &test_ents, is_test_so, alphasort);
			assert_quiet((long)suites[i].n_tests != -1);

			suites[i].name = suite_paths[i];
			suites[i].test_basenames = malloc(
				(suites[i].n_tests + 1) * sizeof(*suites[i].test_basenames)
			);
			assert_not_null(suites[i].test_basenames);

			for (size_t j = 0; j < suites[i].n_tests; j++) {
				// strip ".tst"
				test_ents[j]->d_name[strlen(test_ents[j]->d_name) - 4] = '\0';
				suites[i].test_basenames[j] = test_ents[j]->d_name;
			}
			suites[i].test_basenames[suites[i].n_tests] = NULL;
		}
		free(suite_path);
	}

	/* Run setup and tests */

	{
		printf("Using seed %d\n", seed);
		srand(seed);
		for (size_t i = 0; i < n_suites; i++) {
			printf(
				"Running test suite " T_BOLD "%s" T_NORM ":\n",
				suites[i].name
			);

			printf(" verifying setup... ");
			run_suite(suites + i);
		}
	}

	free(suite_paths);
	free(testdir_path);
}
