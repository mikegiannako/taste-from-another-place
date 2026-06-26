CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g -I/usr/local/include -MMD -MP
LDFLAGS = -L/usr/local/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

TARGET = gamejam
SRC_DIR = src
BUILD_DIR = build
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

# Web / Emscripten build
EMSDK         = /mnt/c/Users/MikeG/emsdk
EMCC          = $(EMSDK)/upstream/emscripten/emcc
RAYLIB_INC    = raylib/src
RAYLIB_LIB    = raylib/build-web/raylib/libraylib.a
WEB_BUILD_DIR = obj-web
WEB_OUT       = web
WEB_OBJS      = $(SRCS:$(SRC_DIR)/%.cpp=$(WEB_BUILD_DIR)/%.o)
EMCXXFLAGS    = -std=c++17 -O2 -I$(RAYLIB_INC)
EMLDFLAGS     = $(RAYLIB_LIB) \
                -sUSE_GLFW=3 \
                -sASYNCIFY \
                -sALLOW_MEMORY_GROWTH=1 \
                -sTOTAL_MEMORY=256MB \
                -sINITIAL_HEAP=128MB \
                --preload-file assets \
                --shell-file shell.html \
                -sEXPORTED_RUNTIME_METHODS=ccall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)

build-web: $(WEB_OUT)/index.html

$(WEB_OUT)/index.html: $(WEB_OBJS) $(RAYLIB_LIB) | $(WEB_OUT)
	$(EMCC) $(WEB_OBJS) -o $@ $(EMLDFLAGS)

$(WEB_BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(WEB_BUILD_DIR)
	$(EMCC) $(EMCXXFLAGS) -c $< -o $@

$(WEB_BUILD_DIR):
	mkdir -p $@

$(WEB_OUT):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

clean-web:
	rm -rf $(WEB_BUILD_DIR) $(WEB_OUT) build-web

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean clean-web run build-web
