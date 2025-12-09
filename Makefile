TARGET_EXEC := ass3

BUILD_DIR := ./build
SRC_DIRS := ./src ./glad/src
INC_DIRS := ./include ./glad/include
LIBS := glfw3 assimp
CXXFLAGS := -std=c++20 # c++20 for map.contains(key) ._.
FLAGS := -Og -Wall

ifdef out
INSTALL_PREFIX := $(out)
else
INSTALL_PREFIX := ./out
endif

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp' -or -name '*.c' -or -name '*.s')

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CPPFLAGS := $(FLAGS) $(INC_FLAGS) $(shell pkg-config --cflags $(LIBS)) -MMD -MP

LDFLAGS := $(shell pkg-config --libs $(LIBS))

.PHONY: build
build: $(BUILD_DIR)/$(TARGET_EXEC)

.PHONY: run
run: build
	$(BUILD_DIR)/$(TARGET_EXEC)

.PHONY: install
install: build
	mkdir -p $(INSTALL_PREFIX)/bin
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(INSTALL_PREFIX)/bin

.PHONY: clean
clean:
	rm -r $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)
