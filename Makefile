# Thanks to Job Vranish
# (https://spin.atomicobject.com/2016/08/26/makefile-c-projects/)
TARGET_EXEC := realmz

BUILD_DIR := ./build
SRC_DIRS := ./src

$(info CC is $(CC))

# Find all the C and C++ files we want to compile. Note the single quotes around
# the * expressions. The shell will incorrectly expand these otherwise, but we
# want to send the * directly to the find command.
SRCS := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c')

# Files to be excluded from compilation
EXCLUDES := ./src/realmz_orig/gammafade.c

SRCS := $(filter-out $(EXCLUDES),$(SRCS))
$(info SRCS is $(SRCS))

# Prepends BUILD_DIR and appends .o to every src file As an example,
# ./your_dir/hello.cpp turns into ./build/./your_dir/hello.cpp.o
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# String substitution (suffix version without %). As an example,
# ./build/hello.cpp.o turns into ./build/hello.cpp.d
DEPS := $(OBJS:.o=.d)

# Every folder in ./src will need to be passed to GCC so that it can find header
# files
INC_DIRS := $(shell find $(SRC_DIRS) -type d)
# Add a prefix to INC_DIRS. So moduleA would become -ImoduleA. GCC understands
# this -I flag
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
# INC_FLAGS += -I/Library/Frameworks/SDL2.framework/Headers

FRAMEWORK_DIRS := /Library/Frameworks/SDL3.xcframework/macos-arm64_x86_64/
FRAMEWORK_FLAGS := $(addprefix -F,$(FRAMEWORK_DIRS))

LDFLAGS += -L/usr/local/lib/ $(FRAMEWORK_FLAGS)
LDLIBS = -lresource_file -lphosg -lz -lSDL3

# The -MMD and -MP flags together generate Makefiles for us! These files will
# have .d instead of .o as the output.
CPPFLAGS := $(INC_FLAGS) -MMD -MP

CFLAGS += -g --std gnu89 -fpascal-strings -DREALMZ_COCOA=1 -O0 $(FRAMEWORK_FLAGS)

CXXFLAGS += -g --std=c++20 -O0 $(FRAMEWORK_FLAGS)

# The final build step.
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	MACOSX_DEPLOYMENT_TARGET=11.7 $(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# Build step for C source
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Build step for C++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want
# those errors to show up.
-include $(DEPS)