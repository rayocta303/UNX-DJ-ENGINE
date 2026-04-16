# XDJ-UNX-C Multi-platform Makefile
# Supports Windows (x86_64) and Linux (ARM64 / DRM-KMS)

PLATFORM ?= WINDOWS_X64

# Compiler settings
# Using Zig for cross-compilation on Windows, or native GCC on Linux
ifeq ($(PLATFORM),WINDOWS_X64)
    CC = t:\zig\zig.exe cc
    CXX = t:\zig\zig.exe c++
    TARGET = xdjunx.exe
    CFLAGS = -O2 -target x86_64-windows -D_WIN32 -DKS_STR_ENCODING_WIN32API
    CXXFLAGS = -O2 -target x86_64-windows -std=c++17 -D_WIN32 -DKS_STR_ENCODING_WIN32API
    LDFLAGS = -Llib -lraylib -lgdi32 -lwinmm -lopengl32
else ifeq ($(PLATFORM),LINUX_ARM64)
    # Native compilation or cross-compilation for Aarch64
    CC ?= gcc
    CXX ?= g++
    TARGET = xdjunx
    CFLAGS = -O2 -DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2 -DKS_STR_ENCODING_NONE
    CXXFLAGS = -O2 -std=c++17 -DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2 -DKS_STR_ENCODING_NONE
    LDFLAGS = -Llib/linux_arm64 -lraylib -lGLESv2 -lEGL -ldrm -lgbm -lasound -lpthread -ldl -lm
endif

CFLAGS += -Wall -Wextra -Isrc -Ilib -Ilib/kaitai
CXXFLAGS += -Wall -Wextra -Isrc -Ilib -Ilib/kaitai -Ilib/rekordbox-metadata

# Source files
SRC_C = src/main.c \
        src/ui/components/theme.c \
        src/ui/components/fonts.c \
        src/ui/components/helpers.c \
        src/ui/views/topbar.c \
        src/ui/views/info.c \
        src/ui/views/splash.c \
        src/ui/views/settings.c \
        src/ui/player/bottomstrip.c \
        src/ui/player/beatfx.c \
        src/ui/player/deckinfo.c \
        src/ui/player/deckstrip.c \
        src/ui/player/waveform.c \
        src/ui/player/player.c \
        src/audio/engine.c \
        src/input/keyboard.c \
        src/ui/browser/browser.c \
        src/core/audio_backend.c \
        src/core/logic/quantize.c \
        src/core/logic/sync.c \
        src/core/logic/settings_io.c \
        src/core/logic/control_object.c \
        src/engine/fx/dsp_utils.c \
        src/core/midi/midi_handler.c \
        src/core/midi/midi_mapper.c \
        src/core/midi/midi_backend_win.c \
        $(wildcard src/engine/fx/colorfx/*.c) \
        $(wildcard src/engine/fx/beatfx/*.c)

SRC_CXX = lib/kaitai/kaitai/kaitaistream.cpp \
          lib/rekordbox-metadata/rekordbox_anlz.cpp \
          lib/rekordbox-metadata/rekordbox_pdb.cpp \
          lib/serato/serato_parser.cpp \
          lib/serato/serato_waveform.cpp \
          src/library/rekordbox_reader.cpp \
          src/library/serato_reader.cpp

OBJ = $(SRC_C:.c=.o) $(SRC_CXX:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
