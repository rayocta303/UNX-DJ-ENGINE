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
%CC% %CFLAGS% -c src/logic/quantize.c -o src/logic/quantize.o
%CC% %CFLAGS% -c src/logic/sync.c -o src/logic/sync.o

%CC% %CFLAGS% -c src/audio/fx/dsp_utils.c -o src/audio/fx/dsp_utils.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/space.c -o src/audio/fx/colorfx/space.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/dub_echo.c -o src/audio/fx/colorfx/dub_echo.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/sweep.c -o src/audio/fx/colorfx/sweep.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/noise.c -o src/audio/fx/colorfx/noise.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/crush.c -o src/audio/fx/colorfx/crush.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/filter.c -o src/audio/fx/colorfx/filter.o
%CC% %CFLAGS% -c src/audio/fx/colorfx/colorfx_manager.c -o src/audio/fx/colorfx/colorfx_manager.o

%CC% %CFLAGS% -c src/audio/fx/beatfx/delay.c -o src/audio/fx/beatfx/delay.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/echo.c -o src/audio/fx/beatfx/echo.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/pingpong.c -o src/audio/fx/beatfx/pingpong.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/spiral.c -o src/audio/fx/beatfx/spiral.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/roll.c -o src/audio/fx/beatfx/roll.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/sliproll.c -o src/audio/fx/beatfx/sliproll.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/reverb.c -o src/audio/fx/beatfx/reverb.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/helix.c -o src/audio/fx/beatfx/helix.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/flanger.c -o src/audio/fx/beatfx/flanger.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/phaser.c -o src/audio/fx/beatfx/phaser.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/bfilter.c -o src/audio/fx/beatfx/bfilter.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/trans.c -o src/audio/fx/beatfx/trans.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/pitch.c -o src/audio/fx/beatfx/pitch.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/vinylbrake.c -o src/audio/fx/beatfx/vinylbrake.o
%CC% %CFLAGS% -c src/audio/fx/beatfx/beatfx_manager.c -o src/audio/fx/beatfx/beatfx_manager.o

echo Building C++ files...
%CXX% %CXXFLAGS% -c lib/kaitai/kaitai/kaitaistream.cpp -o lib/kaitai/kaitai/kaitaistream.o
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_anlz.cpp -o lib/rekordbox-metadata/rekordbox_anlz.o
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_pdb.cpp -o lib/rekordbox-metadata/rekordbox_pdb.o
%CXX% %CXXFLAGS% -c src/library/rekordbox_reader.cpp -o src/library/rekordbox_reader.o

echo Linking...
%CXX% %CXXFLAGS% src/main.o src/ui/components/theme.o src/ui/components/fonts.o src/ui/components/helpers.o src/ui/views/topbar.o src/ui/views/info.o src/ui/views/splash.o src/ui/views/settings.o src/ui/player/bottomstrip.o src/ui/player/beatfx.o src/ui/player/deckinfo.o src/ui/player/deckstrip.o src/ui/player/waveform.o src/ui/player/player.o src/audio/engine.o src/audio/fx/dsp_utils.o src/audio/fx/colorfx/space.o src/audio/fx/colorfx/dub_echo.o src/audio/fx/colorfx/sweep.o src/audio/fx/colorfx/noise.o src/audio/fx/colorfx/crush.o src/audio/fx/colorfx/filter.o src/audio/fx/colorfx/colorfx_manager.o src/audio/fx/beatfx/delay.o src/audio/fx/beatfx/echo.o src/audio/fx/beatfx/pingpong.o src/audio/fx/beatfx/spiral.o src/audio/fx/beatfx/roll.o src/audio/fx/beatfx/sliproll.o src/audio/fx/beatfx/reverb.o src/audio/fx/beatfx/helix.o src/audio/fx/beatfx/flanger.o src/audio/fx/beatfx/phaser.o src/audio/fx/beatfx/bfilter.o src/audio/fx/beatfx/trans.o src/audio/fx/beatfx/pitch.o src/audio/fx/beatfx/vinylbrake.o src/audio/fx/beatfx/beatfx_manager.o src/input/keyboard.o src/ui/browser/browser.o src/logic/quantize.o src/logic/sync.o lib/kaitai/kaitai/kaitaistream.o lib/rekordbox-metadata/rekordbox_anlz.o lib/rekordbox-metadata/rekordbox_pdb.o src/library/rekordbox_reader.o %LDFLAGS% -o xdjunx.exe

echo Done.
