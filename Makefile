CC     = clang
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 $(shell pkg-config --cflags glfw3)
LIBS   := $(shell pkg-config --static --libs glfw3)
INC    = -Isrc -Idependencies/glad/include

BUILD_DIR ?= build
INT_DIR   := $(BUILD_DIR)/int
SRC_DIR   =  src

SRCS := $(shell find $(SRC_DIR) -name "*.c")
DEBUG_OBJS := $(patsubst $(SRC_DIR)/%.c, $(INT_DIR)/debug/%.o, $(SRCS))
RELEASE_OBJS := $(patsubst $(SRC_DIR)/%.c, $(INT_DIR)/release/%.o, $(SRCS))

.PHONY: all debug release clean

debug: $(BUILD_DIR)/gemdb

release: $(BUILD_DIR)/gem

all: debug release

clean:
	rm -rf build

$(BUILD_DIR)/gemdb: $(DEBUG_OBJS) $(INT_DIR)/dependencies/glad.o
	@$(CC) $(CFLAGS) $(INC) $(LIBS) -DGEM_DEBUG -ggdb -o $@ $^
	@echo 'Linking gem (debug)'

$(BUILD_DIR)/gem: $(RELEASE_OBJS) $(INT_DIR)/dependencies/glad.o
	@$(CC) $(CFLAGS) $(INC) -O3 -o $@ $^
	@echo 'Linking gem (release)'

$(DEBUG_OBJS): $(INT_DIR)/debug/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo 'Making $@ (debug)'
	@$(CC) $(CFLAGS) $(INC) -DGEM_DEBUG -ggdb -o $@ -c $<

$(RELEASE_OBJS): $(INT_DIR)/release/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo 'Making $@ (release)'
	@$(CC) $(CFLAGS) $(INC) -O3 -o $@ -c $<

$(INT_DIR)/dependencies/glad.o: dependencies/glad/src/glad.c
	@mkdir -p $(dir $@)
	@echo 'Making glad.o'
	@$(CC) $(CFLAGS) $(INC) -O3 -o $@ -c $<
