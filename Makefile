.POSIX:

.PHONY: all test clean
.SUFFIXES: .c .o

# Feature tests, change these
HAVE_FORK = 1

# Compiler config
CC = cc
CWARN = -Wall -Wextra -Wbad-function-cast -Wcast-align -Wcast-qual \
		-Wfloat-equal -Wformat=2 -Wdeclaration-after-statement \
		-Wno-missing-declarations -Wmissing-include-dirs \
		-Wnested-externs -Wpointer-arith -Wsequence-point -Wshadow \
		-Wsign-conversion -Wstrict-prototypes -Wswitch -Wundef \
		-Wunreachable-code -Wno-unused-function -Wwrite-strings \
		-Wno-format-extra-args -Wno-unused-variable \
		-Wno-unused-parameter
CFLAGS = -std=c99 -O2 -fpic -Iinclude/ \
		 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE $(CWARN) \
		 -DHAVE_FORK=$(HAVE_FORK)
LDFLAGS = $(CFLAGS)
LDLIBS = -lpthread
OBJS = src/hash.o src/table.o

all: test

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

main: src/main.o $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(LDLIBS) src/main.o $(OBJS)

src/table.o: src/table.c include/table.h include/hash.h
src/hash.o: src/hash.c include/hash.h

# Tests
test:
	tests/gen-makefile.sh
	@$(MAKE) -fMakefile -ftests/Makefile.gen test_real

# Clean
clean:
	find . -name "*.o" -delete
	find . -name "*.tst" -delete
	find . -name "*.a" -delete
	find . -name "*.gen" -delete
