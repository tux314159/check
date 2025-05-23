#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
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

struct TestEnv;

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

static void
usage(char *exe)
{
	printf("Usage: %s [-h] [-a] [-s <seed>] [<suite1> <suite1> ...]\n", exe);
}

static void *
malloc_s(size_t n)
{
	void *p;
	p = malloc(n);
	if (n > 0)
		assert_not_null(p);
	return p;
}

void *
calloc_s(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);
	if (nmemb * size != 0)
		assert_not_null(p);
	return p;
}

void *
realloc_s(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (size > 0)
		assert_not_null(p);
	return p;
}

void
free_s(void *ptr)
{
	free(ptr);
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

	buf = malloc_s(strlen(testdir_path) + strlen(ent->d_name) + 2);
	sprintf(buf, "%s/%s", testdir_path, ent->d_name);
	assert_int_neq(stat(buf, &sb), -1);
	free(buf);

	return S_ISDIR(sb.st_mode) && strcmp(ent->d_name, ".") &&
		strcmp(ent->d_name, "..");

	/* TODO: actual robust checks */
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
	output = malloc_s((size_t)output_len + 1);
	output[output_len] = '\0';
	lseek(output_file, 0, SEEK_SET);
	read(output_file, output, output_len);
	ftruncate(output_file, 0);

	return output;
}

static int
run_test_so(const char *path, struct TestEnv *env)
{
	int child_stat, exit_status;
	pid_t pid;

	void *test_obj;
	void (*test)(struct TestEnv *env);

	pid = fork();
	assert_int_neq(pid, -1);
	if (pid) { /* parent */
		assert_int_neq(wait(&child_stat), -1);
		exit_status = WEXITSTATUS(child_stat);
	} else { /* child */
		test_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
		assert_not_null(test_obj);

		test = (void (*)(struct TestEnv *))dlsym(test_obj, "test");
		assert_not_null(test);
		if (!setjmp(_assert_trampoline)) {
			test(env);
			exit_status = 0;
		} else {
			exit_status = 1;
		}

		dlclose(test_obj);
		_exit(exit_status);
	}

	return exit_status;
}

/* clang-format off */
static int
str_split_next(char **s, char c)
{
	int len = 0;
	do {
		if (!**s)
			break;
		len++;
	} while (*++*s != c);
	*s +=!!** s;
	return len;
}
/* clang-format on */

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
	path = malloc_s(path_size);

	sprintf(path, "%s/%s/setup.tst", testdir_path, suite->name);
	setup_obj = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
	assert_not_null(setup_obj);
	setup_env = (void (*)(struct TestEnv **))dlsym(setup_obj, "setup_env");
	assert_not_null(setup_env);
	teardown_env = (void (*)(struct TestEnv *))dlsym(setup_obj, "teardown_env");
	assert_not_null(teardown_env);

	/* Look, realistically dup(2) and fflush won't fail */
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
		exit_status = run_test_so(path, env);
		output = redirect_io_end(output_file, stdout_save, stderr_save);

		if (exit_status == 0)
			printf(T_GREEN T_BOLD "OK" T_NORM "\n");
		else
			printf(T_RED T_BOLD "FAIL" T_NORM "\n");

		int len;
		for (char *line = output, *next = output; *line;
		    len = str_split_next(&next, '\n')) {
			if (len)
				printf(" | %.*s\n", len, line);
			line = next;
		}

		free(output);
	}

	printf(" tearing down environment... ");
	if (!setjmp(_assert_trampoline)) {
		teardown_env(env);
		printf(T_GREEN T_BOLD "OK" T_NORM "\n");
	} else {
		printf(T_RED T_BOLD "FAIL" T_NORM "\n");
	}

	dlclose(setup_obj);
	close(stdout_save);
	close(stderr_save);
	close(output_file);
	free(path);
}

int
main(int argc, char **argv)
{
	ssize_t n_suites;
	struct Suite *suites;
	char **suite_paths;

	unsigned int seed;
	int run_all_flag;

	/* Initial setup */

	if (setjmp(_assert_trampoline)) {
		if (errno)
			perror("Error");
		usage(argv[0]);
		exit(1);
	}

	/* Why the fuck, all this for a basename. Fuck posix */
	{
		char *testdir_path_tmp;
		size_t testdir_path_len;

		testdir_path = malloc_s(strlen(argv[0]) + 1);
		strcpy(testdir_path, argv[0]);
		testdir_path_tmp = dirname(testdir_path);
		testdir_path_len = strlen(testdir_path_tmp);
		testdir_path = realloc_s(testdir_path_tmp, testdir_path_len + 1);
		memmove(testdir_path, testdir_path_tmp, testdir_path_len + 1);
	}

	/* Parse options */

	opterr = 0;
	run_all_flag = 0;
	srand(time(NULL));
	seed = (unsigned)rand() / 2;
	{
		int c;
		char *bad_char;

		while ((c = getopt(argc, argv, ":has:")) != -1) {
			switch (c) {
			case 'h':
				usage(argv[0]);
				exit(0);
			case 'a':
				run_all_flag = 1;
				break;
			case 's':
				seed = strtol(optarg, &bad_char, 10);
				if (*bad_char) {
					fprintf(stderr, "Invalid seed: %s\n", optarg);
					assert_quiet(0);
				}
				break;
			case '?':
				fprintf(stderr, "Unknown option: -%c\n", optopt);
				exit(1);
			case ':':
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
				exit(1);

			default:
				assert_quiet(0);
			}
		}
	}

	/* Find tests to run */

	/* Find suites */
	if (run_all_flag) {
		struct dirent **suite_dirs;

		if (argc != optind)
			assert_quiet(0);

		suite_dirs = NULL;
		n_suites = scandir(testdir_path, &suite_dirs, is_suite_dir, alphasort);
		assert_long_neq(n_suites, -1L);
		suite_paths = malloc_s(((size_t)n_suites + 1) * sizeof(*suite_paths));
		for (ssize_t i = 0; i < n_suites; i++) {
			suite_paths[i] = suite_dirs[i]->d_name;
		}
	} else {
		n_suites = (unsigned)(argc - optind);
		suite_paths = malloc_s(((size_t)n_suites + 1) * sizeof(*suite_paths));
		for (ssize_t i = 0; i < n_suites; i++)
			suite_paths[i] = argv[(unsigned)optind + i];
	}
	suite_paths[n_suites] = NULL;
	suites = malloc_s((size_t)n_suites * sizeof(*suites));

	/* Collect all the tests */
	{
		char *suite_path;
		struct dirent **test_ents;

		suite_path =
			malloc_s(strlen(testdir_path) + max_strlen(suite_paths) + 2);
		for (ssize_t i = 0; i < n_suites; i++) {
			sprintf(suite_path, "%s/%s", testdir_path, suite_paths[i]);
			suites[i].n_tests =
				(size_t)scandir(suite_path, &test_ents, is_test_so, alphasort);
			assert_quiet((long)suites[i].n_tests != -1);

			suites[i].name = suite_paths[i];
			suites[i].test_basenames = malloc_s(
				(suites[i].n_tests + 1) * sizeof(*suites[i].test_basenames)
			);
			assert_not_null(suites[i].test_basenames);

			for (size_t j = 0; j < suites[i].n_tests; j++) {
				/* strip ".tst" */
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
		for (ssize_t i = 0; i < n_suites; i++) {
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
