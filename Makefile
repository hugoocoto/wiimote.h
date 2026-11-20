CC      ?= cc
CXX     ?= c++
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -Werror -g
SAN     ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
LDLIBS  = -lbluetooth

BUILD = build

all: check

$(BUILD):
	mkdir -p $@

# the example only has to build: running it needs a Wiimote
$(BUILD)/demo: examples/demo.c wiimote.h | $(BUILD)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BUILD)/test_read: tests/test_read.c wiimote.h | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) $< -o $@ $(LDLIBS)

$(BUILD)/two_tu: tests/two_tu_a.c tests/two_tu_b.c wiimote.h | $(BUILD)
	$(CC) $(CFLAGS) tests/two_tu_a.c tests/two_tu_b.c -o $@ $(LDLIBS)

$(BUILD)/cxx_header.o: tests/cxx_header.cpp wiimote.h | $(BUILD)
	$(CXX) -std=c++11 -Wall -Wextra -Wpedantic -Werror -c $< -o $@

test: $(BUILD)/test_read $(BUILD)/two_tu
	./$(BUILD)/test_read
	./$(BUILD)/two_tu

check: $(BUILD)/demo $(BUILD)/cxx_header.o test

clean:
	rm -rf $(BUILD)

.PHONY: all test check clean
