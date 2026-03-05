CC = t:\zig\zig.exe cc
CXX = t:\zig\zig.exe c++
CFLAGS = -Wall -Wextra -Isrc -Ilib -O2 -target x86_64-windows -DKS_STR_ENCODING_WIN32API
CXXFLAGS = -Wall -Wextra -Isrc -Ilib -Ilib/kaitai -O2 -target x86_64-windows -std=c++11 -DKS_STR_ENCODING_WIN32API

LDFLAGS = -Llib -lraylib -lgdi32 -lwinmm -lopengl32

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
        src/ui/browser/browser.c

SRC_CXX = lib/kaitai/kaitai/kaitaistream.cpp \
          lib/rekordbox-metadata/rekordbox_anlz.cpp \
          lib/rekordbox-metadata/rekordbox_pdb.cpp \
          src/library/rekordbox_reader.cpp

OBJ = $(SRC_C:.c=.o) $(SRC_CXX:.cpp=.o)
TARGET = xdjunx.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

