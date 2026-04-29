# ============================================================
# IMX6ULL FTP Server Makefile
# 支持本地编译和交叉编译
# ============================================================

# 交叉编译工具链前缀（留空则使用本地gcc）
# 对于Windows MSYS2/MinGW环境，使用：
#   make CROSS_COMPILE=arm-linux-gnueabihf-
#
# 对于Linux原生编译ARM：
#   make CROSS_COMPILE=arm-linux-gnueabihf-
#
# 本地x86编译（测试用）：
#   make

ifdef CROSS_COMPILE
    CC = $(CROSS_COMPILE)gcc
    STRIP = $(CROSS_COMPILE)strip
    TARGET := server
else
    CC = gcc
    STRIP = strip
    # Windows MinGW 使用 .exe 后缀
    ifeq ($(OS),Windows_NT)
        TARGET := server.exe
    else
        TARGET := server
    endif
endif

# 目录
SRC_DIR = src
INC_DIR = include
PROTOCOL_DIR = ../protocol
BUILD_DIR = build
BIN_DIR = .

# 源文件
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# 头文件路径
INCLUDES = -I$(INC_DIR) -I$(PROTOCOL_DIR)

# 编译选项
CFLAGS = -Wall -Wextra -g -O2
CFLAGS += $(INCLUDES)
# 启用POSIX扩展（用于线程等）
CFLAGS += -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
# Windows MinGW 需要定义
ifneq ($(findstring MINGW,$(shell $(CC) -E -dM - < /dev/null 2>/dev/null | grep -i "mingw" || echo "")),)
    CFLAGS += -D_WIN32_WINNT=0x0600
endif

# 链接选项
LDFLAGS = -lpthread

# ============================================================
# 目标
# ============================================================

.PHONY: all clean mkdir

all: mkdir $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $@..."
	$(CC) -o $@ $^ $(LDFLAGS)
ifneq ($(CROSS_COMPILE),)
	@echo "Stripping symbols (ARM build)..."
	$(STRIP) $@
endif
	@echo "Build complete: $@"
	@file $@ 2>/dev/null || echo "Built: $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | mkdir
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

mkdir:
	@mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET:.exe=.exe)

# ============================================================
# 默认目标
# ============================================================
.DEFAULT_GOAL := all
