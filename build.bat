@echo off
set PLATFORM=%1
if "%PLATFORM%"=="" set PLATFORM=windows

if "%PLATFORM%"=="clean" (
    echo Cleaning...
    del /s /q *.o xdjunx xdjunx.exe >nul 2>&1
    exit /b 0
)

set ZIG=t:\zig\zig.exe
set CC=%ZIG% cc
set CXX=%ZIG% c++

echo [Building Asset Bundle...]
if not exist tools\bin2c.exe (
    %CC% tools\bin2c.c -o tools\bin2c.exe
)

if exist src\ui\components\assets_bundle.h del src\ui\components\assets_bundle.h
echo #ifndef ASSETS_BUNDLE_H > src\ui\components\assets_bundle.h
echo #define ASSETS_BUNDLE_H >> src\ui\components\assets_bundle.h
.\tools\bin2c.exe "assets\fonts\otfs\Font Awesome 5 Free-Solid-900.otf" src\ui\components\assets_bundle.h font_awesome_solid
.\tools\bin2c.exe "assets\fonts\otfs\Font Awesome 5 Free-Regular-400.otf" src\ui\components\assets_bundle.h font_awesome_regular
.\tools\bin2c.exe "assets\fonts\otfs\Font Awesome 5 Brands-Regular-400.otf" src\ui\components\assets_bundle.h font_awesome_brand
.\tools\bin2c.exe "assets\images\Pioneer.png" src\ui\components\assets_bundle.h pioneer_logo
echo #endif >> src\ui\components\assets_bundle.h

if "%PLATFORM%"=="linux" (
    echo [Target: Linux ARM64]
    set TARGET_FLAGS=-target aarch64-linux-gnu.2.36 -DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2 -DKS_STR_ENCODING_NONE -Ilib/linux_arm64/include
    set LDFLAGS=-Llib/linux_arm64 -lraylib -lGLESv2 -lEGL -ldrm -lgbm -lpthread -ldl -lm
    set TARGET=xdjunx
) else (
    echo [Target: Windows x64]
    set TARGET_FLAGS=-target x86_64-windows -D_WIN32 -DKS_STR_ENCODING_WIN32API
    set LDFLAGS=-Llib -lraylib -lgdi32 -lwinmm -lopengl32
    set TARGET=xdjunx.exe
)

set CFLAGS=-Wall -Wextra -Isrc -Ilib -O2 %TARGET_FLAGS%
set CXXFLAGS=-Wall -Wextra -Isrc -Ilib -Ilib/kaitai -Ilib/rekordbox-metadata -O2 -std=c++11 %TARGET_FLAGS%

echo Building C files...
for %%f in (src/main.c src/ui/components/theme.c src/ui/components/fonts.c src/ui/components/helpers.c src/ui/views/topbar.c src/ui/views/info.c src/ui/views/splash.c src/ui/views/settings.c src/ui/player/bottomstrip.c src/ui/player/beatfx.c src/ui/player/deckinfo.c src/ui/player/deckstrip.c src/ui/player/waveform.c src/ui/player/player.c src/audio/engine.c src/input/keyboard.c src/ui/browser/browser.c src/logic/quantize.c src/logic/sync.c) do (
    %CC% %CFLAGS% -c %%f -o %%~pf%%~nf.o || exit /b 1
)

for %%f in (src/audio/fx/dsp_utils.c src/audio/fx/colorfx/space.c src/audio/fx/colorfx/dub_echo.c src/audio/fx/colorfx/sweep.c src/audio/fx/colorfx/noise.c src/audio/fx/colorfx/crush.c src/audio/fx/colorfx/filter.c src/audio/fx/colorfx/colorfx_manager.c) do (
    %CC% %CFLAGS% -c %%f -o %%~pf%%~nf.o || exit /b 1
)

for %%f in (src/audio/fx/beatfx/delay.c src/audio/fx/beatfx/echo.c src/audio/fx/beatfx/pingpong.c src/audio/fx/beatfx/spiral.c src/audio/fx/beatfx/roll.c src/audio/fx/beatfx/sliproll.c src/audio/fx/beatfx/reverb.c src/audio/fx/beatfx/helix.c src/audio/fx/beatfx/flanger.c src/audio/fx/beatfx/phaser.c src/audio/fx/beatfx/bfilter.c src/audio/fx/beatfx/trans.c src/audio/fx/beatfx/pitch.c src/audio/fx/beatfx/vinylbrake.c src/audio/fx/beatfx/beatfx_manager.c) do (
    %CC% %CFLAGS% -c %%f -o %%~pf%%~nf.o || exit /b 1
)

echo Building C++ files...
%CXX% %CXXFLAGS% -c lib/kaitai/kaitai/kaitaistream.cpp -o lib/kaitai/kaitai/kaitaistream.o || exit /b 1
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_anlz.cpp -o lib/rekordbox-metadata/rekordbox_anlz.o || exit /b 1
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_pdb.cpp -o lib/rekordbox-metadata/rekordbox_pdb.o || exit /b 1
%CXX% %CXXFLAGS% -c src/library/rekordbox_reader.cpp -o src/library/rekordbox_reader.o || exit /b 1

if "%2"=="check" (
    echo [Check Only] Compilation successful.
    goto end
)

echo Linking...
%CXX% %CXXFLAGS% src/main.o src/ui/components/theme.o src/ui/components/fonts.o src/ui/components/helpers.o src/ui/views/topbar.o src/ui/views/info.o src/ui/views/splash.o src/ui/views/settings.o src/ui/player/bottomstrip.o src/ui/player/beatfx.o src/ui/player/deckinfo.o src/ui/player/deckstrip.o src/ui/player/waveform.o src/ui/player/player.o src/audio/engine.o src/audio/fx/dsp_utils.o src/audio/fx/colorfx/space.o src/audio/fx/colorfx/dub_echo.o src/audio/fx/colorfx/sweep.o src/audio/fx/colorfx/noise.o src/audio/fx/colorfx/crush.o src/audio/fx/colorfx/filter.o src/audio/fx/colorfx/colorfx_manager.o src/audio/fx/beatfx/delay.o src/audio/fx/beatfx/echo.o src/audio/fx/beatfx/pingpong.o src/audio/fx/beatfx/spiral.o src/audio/fx/beatfx/roll.o src/audio/fx/beatfx/sliproll.o src/audio/fx/beatfx/reverb.o src/audio/fx/beatfx/helix.o src/audio/fx/beatfx/flanger.o src/audio/fx/beatfx/phaser.o src/audio/fx/beatfx/bfilter.o src/audio/fx/beatfx/trans.o src/audio/fx/beatfx/pitch.o src/audio/fx/beatfx/vinylbrake.o src/audio/fx/beatfx/beatfx_manager.o src/input/keyboard.o src/ui/browser/browser.o src/logic/quantize.o src/logic/sync.o lib/kaitai/kaitai/kaitaistream.o lib/rekordbox-metadata/rekordbox_anlz.o lib/rekordbox-metadata/rekordbox_pdb.o src/library/rekordbox_reader.o %LDFLAGS% -o %TARGET% || exit /b 1

:end
echo Done.
