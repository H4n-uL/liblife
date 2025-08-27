CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I$(BUILD_DIR) \
         -I/opt/homebrew/opt/portaudio/include \
         -I/opt/homebrew/opt/cjson/include \
         -O3 \
         -march=native \
         -mtune=native \
         -flto \
         -ffast-math \
         -funroll-loops \
         -ftree-vectorize \
         -fomit-frame-pointer \
         -finline-functions \
         -fprefetch-loop-arrays \
         -fno-strict-aliasing

LDFLAGS = -lm -lz \
          -L/opt/homebrew/opt/portaudio/lib -lportaudio \
          -L/opt/homebrew/opt/cjson/lib -lcjson \
          -lpthread \
          -flto \
          -O3

# Source directories
SRC_DIR = src
LIBFRAD_DIR = $(SRC_DIR)/libfrad
BUILD_DIR = build
BIN_DIR = bin
HELP_DIR = $(SRC_DIR)/help

# Help files
HELP_FILES = $(wildcard $(HELP_DIR)/*.txt)
HELP_OBJS = $(patsubst $(HELP_DIR)/%.txt,$(BUILD_DIR)/help/%.h,$(HELP_FILES))

# Target executable
TARGET = $(BIN_DIR)/life

# Main source files
MAIN_SRCS = $(SRC_DIR)/main.c \
            $(SRC_DIR)/app_common.c \
            $(SRC_DIR)/encoder.c \
            $(SRC_DIR)/decoder.c \
            $(SRC_DIR)/repairer.c \
            $(SRC_DIR)/header.c

# Tools source files
TOOLS_SRCS = $(SRC_DIR)/tools/cli.c \
             $(SRC_DIR)/tools/process.c \
             $(SRC_DIR)/tools/audio.c \
             $(SRC_DIR)/tools/pcmproc.c

# LibFrad source files
LIBFRAD_SRCS = $(LIBFRAD_DIR)/common.c \
               $(LIBFRAD_DIR)/encoder.c \
               $(LIBFRAD_DIR)/decoder.c \
               $(LIBFRAD_DIR)/repairer.c

# Backend source files
BACKEND_SRCS = $(LIBFRAD_DIR)/backend/backend.c \
               $(LIBFRAD_DIR)/backend/bitcvt.c

# Fourier source files
FOURIER_SRCS = $(LIBFRAD_DIR)/fourier/profile0.c \
               $(LIBFRAD_DIR)/fourier/profile1.c \
               $(LIBFRAD_DIR)/fourier/profile2.c \
               $(LIBFRAD_DIR)/fourier/profile4.c \
               $(LIBFRAD_DIR)/fourier/profiles.c \
               $(LIBFRAD_DIR)/fourier/compact.c

# Fourier backend source files
FOURIER_BACKEND_SRCS = $(LIBFRAD_DIR)/fourier/backend/dct_core.c \
                       $(LIBFRAD_DIR)/fourier/backend/signal.c \
                       $(LIBFRAD_DIR)/fourier/backend/u8pack.c \
                       $(LIBFRAD_DIR)/fourier/backend/pocketfft.c

# Fourier tools source files
FOURIER_TOOLS_SRCS = $(LIBFRAD_DIR)/fourier/tools/p1tools.c \
                     $(LIBFRAD_DIR)/fourier/tools/p2tools.c

# Tools source files
LIBFRAD_TOOLS_SRCS = $(LIBFRAD_DIR)/tools/asfh.c \
                     $(LIBFRAD_DIR)/tools/head.c \
                     $(LIBFRAD_DIR)/tools/ecc/reedsolo.c \
                     $(LIBFRAD_DIR)/tools/ecc/ecc.c

# All source files
ALL_SRCS = $(MAIN_SRCS) $(TOOLS_SRCS) $(LIBFRAD_SRCS) $(BACKEND_SRCS) \
           $(FOURIER_SRCS) $(FOURIER_BACKEND_SRCS) $(FOURIER_TOOLS_SRCS) \
           $(LIBFRAD_TOOLS_SRCS)

# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(ALL_SRCS))

# Default target
all: $(TARGET)

# Create directories
$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

# Create build directories
create_dirs:
	@mkdir -p $(BUILD_DIR)/tools
	@mkdir -p $(BUILD_DIR)/libfrad/backend
	@mkdir -p $(BUILD_DIR)/libfrad/fourier/backend
	@mkdir -p $(BUILD_DIR)/libfrad/fourier/tools
	@mkdir -p $(BUILD_DIR)/libfrad/tools/ecc
	@mkdir -p $(BUILD_DIR)/help

# Convert help files to headers
$(BUILD_DIR)/help/%.h: $(HELP_DIR)/%.txt
	@mkdir -p $(dir $@)
	xxd -i $< $@

# Build target
$(TARGET): create_dirs $(BIN_DIR) $(HELP_OBJS) $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(LIBFRAD_DIR) -c $< -o $@

# Clean build
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/life

.PHONY: all clean install uninstall
