#!/bin/sh

# Generate a seed and pass it to each test, and report errors.
# Optionally fix the seed. If no non-option arguments are specified,
# run all tests.

# ANSI escapes
t_dim="\x1b[2m"
t_bold="\x1b[1m"
t_ital="\x1b[3m"
t_line="\x1b[4m"
t_green="\x1b[38;5;2m"
t_yellow="\x1b[38;5;3m"
t_red="\x1b[38;5;1m"
t_norm="\x1b[0m"

# Format and print output from running test
show_output () {
	sed -e "s/^INFO /${t_dim}/g" \
		-e "s/^WARN /${t_yellow}/g" \
		-e "s/^ERROR /${t_red}/g" \
			<"$1" | sed "s/^/${t_norm} >\t/g"
	printf "$t_norm"
}

run_test () {
	if ! eval "$1" "$2" >"$3" 2>&1 ; then
		printf "${t_bold}${t_red}FAIL${t_norm}\n"
		r=1
	else
		printf "${t_bold}${t_green}OK${t_norm}\n"
		r=0
	fi
	show_output "$3"
	return $r
}

manualseed=0
while getopts "h?s:?" opt; do
	case $opt in
		h)	printf 'Usage: %s [-h] [-s <seed>] <test1> <test2> ...\n' "$0"
			exit 0;;
		s)	manualseed=1
			seed="$OPTARG";;
		?)	exit 1;;
	esac
done
shift $((OPTIND - 1))

# Get tests and sort by suite
if [ -z "$*" ]; then
	set -- "$(dirname "$0")"/*.tst
fi

set -- $(printf "%s" "$*" | tr ' ' '\n' | sort)

if [ $manualseed -eq 0 ]; then
	seed=$(seq 0 65535 | shuf -n1)
fi

printf "Test suites to run: ${t_bold}%s${t_norm}\n" \
	"$(printf "%s" \
		"$(for s in $(printf "%s" "$*" | tr ' ' '\n' | cut -d'-' -f1 | uniq);
			do basename "$s";
		done | tr '\n' ' ' | xargs)" \
	| sed "s/ /${t_norm}, ${t_bold}/g")"

printf "Using seed %d\n" "$seed"

output=$(mktemp /tmp/run-tests.XXXXXX)

cur_suite=
while [ -n "$1" ]; do
	this_suite="$(printf "%s" "$(basename "$1")" | cut -d'-' -f1)"
	if [ "$this_suite" != "$cur_suite" ]; then
		setup_failed=0
		cur_suite="$this_suite"
		printf "Running test suite ${t_bold}%s${t_norm}:\n" "$cur_suite"
		printf " verifying setup... "
		if ! run_test "./$(dirname "$0")/${this_suite}-setup.tst" "$seed" "$output"; then
			printf " skipping this entire suite!\n"
			setup_failed=1
		fi
	fi
	# Skip the rest if setup failed
	if [ "$setup_failed" -eq 1 ]; then
		shift
		continue
	fi

	# Skip setup if encountered, already ran it at the start
	if [ "$(basename "$1")" = "${cur_suite}-setup.tst" ]; then
		shift
		continue
	fi
	printf " test ${t_ital}$(basename "$1" .tst | cut -d'-' -f2)${t_norm}... "
	run_test "./${1}" "$seed" "$output"
	shift
done

rm "$output"
