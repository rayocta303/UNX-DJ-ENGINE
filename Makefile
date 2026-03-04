CC = t:\zig\zig.exe cc
CFLAGS = -Wall -Wextra -Iinclude -O2 -target x86_64-windows
LDFLAGS = -Llib -lraylib -lgdi32 -lwinmm -lopengl32

SRC = src/main.c \
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
      src/ui/player/player.c

OBJ = $(SRC:.c=.o)
TARGET = xdjunx.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
