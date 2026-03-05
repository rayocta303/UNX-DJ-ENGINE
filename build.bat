@echo off
set CC=t:\zig\zig.exe cc
set CXX=t:\zig\zig.exe c++
set CFLAGS=-Wall -Wextra -Isrc -Ilib -O2 -target x86_64-windows -DKS_STR_ENCODING_WIN32API
set CXXFLAGS=-Wall -Wextra -Isrc -Ilib -Ilib/kaitai -O2 -target x86_64-windows -std=c++11 -DKS_STR_ENCODING_WIN32API

set LDFLAGS=-Llib -lraylib -lgdi32 -lwinmm -lopengl32

echo Building C files...
%CC% %CFLAGS% -c src/main.c -o src/main.o
%CC% %CFLAGS% -c src/ui/components/theme.c -o src/ui/components/theme.o
%CC% %CFLAGS% -c src/ui/components/fonts.c -o src/ui/components/fonts.o
%CC% %CFLAGS% -c src/ui/components/helpers.c -o src/ui/components/helpers.o
%CC% %CFLAGS% -c src/ui/views/topbar.c -o src/ui/views/topbar.o
%CC% %CFLAGS% -c src/ui/views/info.c -o src/ui/views/info.o
%CC% %CFLAGS% -c src/ui/views/splash.c -o src/ui/views/splash.o
%CC% %CFLAGS% -c src/ui/views/settings.c -o src/ui/views/settings.o
%CC% %CFLAGS% -c src/ui/player/bottomstrip.c -o src/ui/player/bottomstrip.o
%CC% %CFLAGS% -c src/ui/player/beatfx.c -o src/ui/player/beatfx.o
%CC% %CFLAGS% -c src/ui/player/deckinfo.c -o src/ui/player/deckinfo.o
%CC% %CFLAGS% -c src/ui/player/deckstrip.c -o src/ui/player/deckstrip.o
%CC% %CFLAGS% -c src/ui/player/waveform.c -o src/ui/player/waveform.o
%CC% %CFLAGS% -c src/ui/player/player.c -o src/ui/player/player.o
%CC% %CFLAGS% -c src/audio/engine.c -o src/audio/engine.o
%CC% %CFLAGS% -c src/input/keyboard.c -o src/input/keyboard.o
%CC% %CFLAGS% -c src/ui/browser/browser.c -o src/ui/browser/browser.o

echo Building C++ files...
%CXX% %CXXFLAGS% -c lib/kaitai/kaitai/kaitaistream.cpp -o lib/kaitai/kaitai/kaitaistream.o
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_anlz.cpp -o lib/rekordbox-metadata/rekordbox_anlz.o
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_pdb.cpp -o lib/rekordbox-metadata/rekordbox_pdb.o
%CXX% %CXXFLAGS% -c src/library/rekordbox_reader.cpp -o src/library/rekordbox_reader.o

echo Linking...
%CXX% %CXXFLAGS% src/main.o src/ui/components/theme.o src/ui/components/fonts.o src/ui/components/helpers.o src/ui/views/topbar.o src/ui/views/info.o src/ui/views/splash.o src/ui/views/settings.o src/ui/player/bottomstrip.o src/ui/player/beatfx.o src/ui/player/deckinfo.o src/ui/player/deckstrip.o src/ui/player/waveform.o src/ui/player/player.o src/audio/engine.o src/input/keyboard.o src/ui/browser/browser.o lib/kaitai/kaitai/kaitaistream.o lib/rekordbox-metadata/rekordbox_anlz.o lib/rekordbox-metadata/rekordbox_pdb.o src/library/rekordbox_reader.o %LDFLAGS% -o xdjunx.exe

echo Done.
