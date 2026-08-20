override CXXFLAGS = -std=c++20 -g -ferror-limit=0 -fno-common -Wall -Wextra -Wno-switch -g -O2

SRCS=$(wildcard *.cpp)
OBJS=$(SRCS:.cpp=.o)

TEST_SRCS=$(wildcard test/*.cpp)
TESTS=$(TEST_SRCS:.cpp=.exe)

override CC = g++

# Stage 1

opencc: $(OBJS)
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJS): opencc.h

test/%.exe: opencc test/%.cpp
	./opencc -Iinclude -Itest -c -o test/$*.o test/$*.cpp
	$(CC) -pthread -o $@ test/$*.o -xc test/common

test: $(TESTS)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh ./opencc

test-all: test test-stage2

# Stage 2

stage2/opencc: $(OBJS:%=stage2/%)
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

stage2/%.o: opencc %.cpp
	mkdir -p stage2/test
	./opencc -c -o $(@D)/$*.o $*.cpp

stage2/test/%.exe: stage2/opencc test/%.cpp
	mkdir -p stage2/test
	./stage2/opencc -Iinclude -Itest -c -o stage2/test/$*.o test/$*.cpp
	$(CC) -pthread -o $@ stage2/test/$*.o -xc test/common

test-stage2: $(TESTS:test/%=stage2/test/%)
	for i in $^; do echo $$i; ./$$i || exit 1; echo; done
	test/driver.sh ./stage2/opencc

# Misc.

clean:
	rm -rf opencc tmp* $(TESTS) test/*.s test/*.exe stage2
	find * -type f '(' -name '*~' -o -name '*.o' ')' -exec rm {} ';'

.PHONY: test clean test-stage2
