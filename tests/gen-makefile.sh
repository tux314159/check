#!/bin/sh

# Generate the sub-makefile to compile all the tests (should descend into tests
# subdir).
# NOTE: $(OBJS) must be passed in as arguments.

test_dir="tests"

# We generate these targets
# - The common test library;
# - for each test suite, the setup code;
# - for each unit test, the test code;
# - the actual test target to run all the tests.

gen_makefile_name="$test_dir/Makefile.gen"

make_unit_tmpl="
TSUITE!-TNAME!.tst: TSUITE!/TNAME!.o TSUITE!/setup.o $test_dir/check.o \$(OBJS)
	\$(CC) \$(LDFLAGS) -o \$@ \$(LDLIBS) TSUITE!/TNAME!.o TSUITE!/setup.o $test_dir/check.o \$(OBJS)
TSUITE!/TNAME!.o: TSUITE!/TNAME!.c $test_dir/check.h
"
make_setup_tmpl="
TSUITE!/setup.o: TSUITE!/setup.c $test_dir/check.h
TSUITE!-setup.tst: TSUITE!/setup.o $test_dir/test_setup.o \$(OBJS)
	\$(CC) \$(LDFLAGS) -o \$@ \$(LDLIBS) TSUITE!/setup.o $test_dir/test_setup.o \$(OBJS)
"

rm -f "$gen_makefile_name"

printf "%s/check.o: %s/check.c %s/check.h\n" "$test_dir" "$test_dir" "$test_dir" >>"$gen_makefile_name"
printf "%s/test_setup.o: %s/check.c %s/check.h\n\t\$(CC) -c \$(CFLAGS) -D_TEST_SETUP_ONLY -o \$@ %s/check.c
" "$test_dir" "$test_dir" "$test_dir" "$test_dir">>"$gen_makefile_name"
find "$(dirname "$0")" -mindepth 1 -maxdepth 1 -type d -exec sh -c '
	sed -e "s|TSUITE!|$1|g" >>$3 <<<"$2"' \
	sh {} "$make_setup_tmpl" "$gen_makefile_name" \;
# setup.c gets special treatment
find "$(dirname "$0")" -mindepth 2 -maxdepth 2 -name "*.c" ! -name "setup.c" -exec sh -c '
	sed -e "s|TNAME!|$(basename "$1" .c)|g" -e "s|TSUITE!|$(dirname "$1")|g" >>$3 <<<"$2"' \
	sh {} "$make_unit_tmpl" "$gen_makefile_name" \;

tests=$(find "$(dirname "$0")" -mindepth 2 -maxdepth 2 -name "*.c" -exec sh -c '
	printf "$(dirname "$1")-$(basename "$1" .c).tst "' \
	sh {} \;)
printf "
test_real: %s
	%s/run-tests.sh
	rm -f %s
" "$tests" "$test_dir" "$gen_makefile_name" >>"$gen_makefile_name"
