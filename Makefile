CC     ?= clang
CFLAGS := -Wall -Wextra -Werror -pedantic -std=c99 $(shell pkg-config --cflags freetype2)
LIBS   := $(shell pkg-config --static --libs freetype2) -lm -lX11 -lGL
INC    = -Isrc -Idependencies/glad/include -Idependencies/stb_image

BUILD_DIR ?= build
INT_DIR   := $(BUILD_DIR)/int
SRC_DIR   =  src

SRCS := $(shell find $(SRC_DIR) -name "*.c")
HEADERS := $(shell find $(SRC_DIR) -name "*.h")
DEBUG_OBJS := $(patsubst $(SRC_DIR)/%.c, $(INT_DIR)/debug/%.o, $(SRCS))
RELEASE_OBJS := $(patsubst $(SRC_DIR)/%.c, $(INT_DIR)/release/%.o, $(SRCS))

.PHONY: all debug release clean validate

debug: $(BUILD_DIR)/gemdb

release: $(BUILD_DIR)/gem

validate:: EXTRACFLAGS = -DGEM_PT_VALIDATE
validate:: debug


all: debug release

clean:
	rm -rf $(BUILD_DIR)


$(BUILD_DIR)/gemdb: $(DEBUG_OBJS) $(INT_DIR)/dependencies/glad.o $(INT_DIR)/dependencies/stb_image.o 
	@echo 'Linking gem (debug)'
	@$(CC) $(CFLAGS) $(EXTRACFLAGS) $(INC) $(LIBS) -DGEM_DEBUG -ggdb -o $@ $^

$(BUILD_DIR)/gem: $(RELEASE_OBJS) $(INT_DIR)/dependencies/glad.o $(INT_DIR)/dependencies/stb_image.o
	@echo 'Linking gem (release)'
	@$(CC) $(CFLAGS) $(EXTRACFLAGS) $(INC) $(LIBS) -O3 -o $@ $^

$(DEBUG_OBJS): $(INT_DIR)/debug/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	@echo 'Making $@ (debug)'
	@$(CC) $(CFLAGS) $(EXTRACFLAGS) $(INC) -DGEM_DEBUG -ggdb -o $@ -c $<

$(RELEASE_OBJS): $(INT_DIR)/release/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	@echo 'Making $@ (release)'
	@$(CC) $(CFLAGS) $(EXTRACFLAGS) $(INC) -O3 -o $@ -c $<

$(INT_DIR)/dependencies/glad.o: dependencies/glad/src/glad.c
	@mkdir -p $(dir $@)
	@echo 'Making glad.o'
	@$(CC) -Idependencies/glad/include -O3 -o $@ -c $<

$(INT_DIR)/dependencies/stb_image.o: dependencies/stb_image/stb_image.c
	@mkdir -p $(dir $@)
	@echo 'Making stb_image.o'
	@$(CC) -Idependencies/std_image -O3 -o $@ -c $<
