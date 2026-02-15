CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude
SRC_DIR = src
BUILD_DIR = build
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
TARGET = $(BUILD_DIR)/cougfs

all: $(TARGET)
$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "✓ Built: $@"
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
test: all
	@echo "✓ Ready for testing"
clean:
	rm -rf $(BUILD_DIR) *.o disk.img
	@echo "✓ Clean complete"
.PHONY: all test clean
