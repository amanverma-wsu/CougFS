CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c11 -g -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = -lpthread

ifeq ($(ENABLE_FUSE),1)
FUSE_CFLAGS ?= $(shell pkg-config --cflags fuse 2>/dev/null)
FUSE_LIBS ?= $(shell pkg-config --libs fuse 2>/dev/null)
CFLAGS += -DENABLE_FUSE $(FUSE_CFLAGS)
LDFLAGS += $(FUSE_LIBS)
endif

SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/$(if $(filter 1,$(ENABLE_FUSE)),fuse,default)
TEST_DIR = tests

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BUILD_DIR)/cougfs

# Core objects (everything except main.o for test linking)
CORE_OBJS = $(filter-out $(OBJ_DIR)/main.o, $(OBJS))

TEST_SRCS = $(TEST_DIR)/test_main.c
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/tests/%.o, $(TEST_SRCS))
TEST_BIN = $(BUILD_DIR)/test_cougfs

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ Built: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(CORE_OBJS) $(TEST_OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) *.o disk.img test_disk.img
	@echo "✓ Clean complete"

valgrind: $(TEST_BIN)
	valgrind --leak-check=full --error-exitcode=1 ./$(TEST_BIN)

.PHONY: all test clean valgrind
