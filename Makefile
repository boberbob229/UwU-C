# Build UwUC

CC ?= clang
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -Isrc
LDFLAGS =

PLATFORM := $(shell uname -s)
ARCH     := $(shell uname -m)

# platform detection
ifeq ($(PLATFORM),Darwin)
    PLATFORM_NAME = macOS
    CFLAGS += -DUWUCC_PLATFORM_MACOS

    ifeq ($(ARCH),arm64)
        ARCH_NAME = arm64
        CFLAGS += -DUWUCC_ARCH_ARM64
    else ifeq ($(ARCH),x86_64)
        ARCH_NAME = x86_64
        CFLAGS += -DUWUCC_ARCH_X86_64
    else
        $(error unsupported macOS architecture: $(ARCH))
    endif

else ifeq ($(PLATFORM),Linux)
    PLATFORM_NAME = Linux
    CFLAGS += -DUWUCC_PLATFORM_LINUX
    LDFLAGS += -no-pie

    ifeq ($(ARCH),aarch64)
        ARCH_NAME = arm64
        CFLAGS += -DUWUCC_ARCH_ARM64
    else ifeq ($(ARCH),x86_64)
        ARCH_NAME = x86_64
        CFLAGS += -DUWUCC_ARCH_X86_64
    else
        $(error unsupported Linux architecture: $(ARCH))
    endif

else
    $(error unsupported platform: $(PLATFORM))
endif

DEBUG_FLAGS   = -g -O0 -DDEBUG -fsanitize=address
RELEASE_FLAGS = -O2 -DNDEBUG
TEST_FLAGS    = -g -O0 -DDEBUG --coverage

SRC_DIR   = src
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
BIN_DIR   = $(BUILD_DIR)/bin
STDLIB_DIR = stdlib

TESTS_DIR    = tests
EXAMPLES_DIR = examples

SRCS := $(filter-out src/parser_legacy.c , $(wildcard src/*.c))
OBJS := $(SRCS:src/%.c=$(OBJ_DIR)/%.o)

TARGET = $(BIN_DIR)/uwucc
STDLIB = $(STDLIB_DIR)/uwu_stdlib.o

all: CFLAGS += $(RELEASE_FLAGS)
all: $(TARGET) $(STDLIB)
	@echo "built for $(PLATFORM_NAME) ($(ARCH_NAME))"

debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET) $(STDLIB)

release: CFLAGS += $(RELEASE_FLAGS) -march=native
release: $(TARGET) $(STDLIB)

test-build: CFLAGS += $(TEST_FLAGS)
test-build: LDFLAGS += --coverage
test-build: $(TARGET) $(STDLIB)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(STDLIB): $(STDLIB_DIR)/uwu_stdlib.c
	$(CC) -c -O2 $(STDLIB_DIR)/uwu_stdlib.c -o $(STDLIB)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(STDLIB)
	rm -f *.gcov *.gcda *.gcno

distclean: clean
	rm -f *.o *.d

test: $(TARGET) $(STDLIB)
	@if [ -d "$(TESTS_DIR)" ]; then \
	   for t in $(TESTS_DIR)/*.uwu; do \
	      [ -f "$$t" ] || continue; \
	      ./$(TARGET) "$$t" -o "$(BUILD_DIR)/test_$$(basename $$t .uwu)" || exit 1; \
	   done; \
	fi

examples: $(TARGET) $(STDLIB)
	@if [ -d "$(EXAMPLES_DIR)" ]; then \
	   for e in $(EXAMPLES_DIR)/*.uwu; do \
	      [ -f "$$e" ] || continue; \
	      ./$(TARGET) "$$e" -o "$(BUILD_DIR)/example_$$(basename $$e .uwu)" || exit 1; \
	   done; \
	fi

install: $(TARGET) $(STDLIB)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/uwucc
	install -d $(DESTDIR)/usr/local/lib/uwucc
	install -m 644 $(STDLIB) $(DESTDIR)/usr/local/lib/uwucc/uwu_stdlib.o

uninstall:
	rm -f /usr/local/bin/uwucc
	rm -rf /usr/local/lib/uwucc

info:
	@echo "platform: $(PLATFORM_NAME)"
	@echo "arch:     $(ARCH_NAME)"
	@echo "cc:       $(CC)"

.DEFAULT_GOAL := all