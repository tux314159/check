#!/bin/sh

# Generate the sub-makefile to compile all the tests (should descend into tests
# subdir).

test_dir="tests"

# We generate these targets
# - The common test library;
# - static library of all the code
# - for each test suite, the setup code;
# - for each unit test, the test code;
# - the actual test target to run all the tests.

gen_makefile_name="$test_dir/Makefile.gen"

make_unit_tmpl="$(printf "
TESTFILE!.tst: TESTFILE!.c %s/check.o %s/check.h %s/lib.a \$(OBJS)
\t\$(CC) \$(CFLAGS) -shared -o \$@ TESTFILE!.c %s/check.o \$(OBJS)" \
	"$test_dir" "$test_dir" "$test_dir" "$test_dir")
"

rm -f "$gen_makefile_name"


{

printf \
	"%s/lib.a: \$(OBJS)\n\tar rcs \$@ \$(OBJS)\n" \
	"$test_dir";

printf \
	"%s/check.o: %s/check.c %s/check.h\n" \
	"$test_dir" "$test_dir" "$test_dir";

find "$(dirname "$0")" -mindepth 2 -maxdepth 2 -name "*.c" -exec sh -c '
printf "%s" "$1" | sed "s|TESTFILE!|$(dirname "$2")/$(basename "$2" .c)|g"' \
	sh "$make_unit_tmpl" {} "$gen_makefile_name" \;;

} >>"$gen_makefile_name"


tests=$(find "$(dirname "$0")" -mindepth 2 -maxdepth 2 -name "*.c" -exec sh -c '
	printf "$(dirname "$1")/$(basename "$1" .c).tst "' \
	sh {} \;)

printf \
	"test_real: %s\n\t%s/run-tests.sh\n\t#rm -f %s\n" \
	"$tests" "$test_dir" "$gen_makefile_name" >>"$gen_makefile_name"
