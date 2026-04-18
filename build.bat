@echo off
set PLATFORM=%1
if "%PLATFORM%"=="" set PLATFORM=windows

if "%PLATFORM%"=="clean" (
    echo Cleaning...
    del /s /q *.o xdjunx xdjunx.exe >nul 2>&1
    exit /b 0
)

if "%ZIG%"=="" set ZIG=t:\zig\zig.exe
set CC=%ZIG% cc
set CXX=%ZIG% c++

echo [Building Asset Bundle...]
if not exist tools\bin2c.exe (
    %CC% tools\bin2c.c -o tools\bin2c.exe
)

if exist src\ui\components\assets_bundle.h del src\ui\components\assets_bundle.h
echo #ifndef ASSETS_BUNDLE_H > src\ui\components\assets_bundle.h
echo #define ASSETS_BUNDLE_H >> src\ui\components\assets_bundle.h
".\tools\bin2c.exe" "assets\fonts\otfs\Font Awesome 5 Free-Solid-900.otf" src\ui\components\assets_bundle.h font_awesome_solid append
".\tools\bin2c.exe" "assets\fonts\otfs\Font Awesome 5 Free-Regular-400.otf" src\ui\components\assets_bundle.h font_awesome_regular append
".\tools\bin2c.exe" "assets\fonts\otfs\Font Awesome 5 Brands-Regular-400.otf" src\ui\components\assets_bundle.h font_awesome_brand append
".\tools\bin2c.exe" "assets\splash.png" src\ui\components\assets_bundle.h unx_logo append
".\tools\bin2c.exe" "assets\icons\crown.png" src\ui\components\assets_bundle.h icon_crown append

if not exist tools\gen_splash_bundle.exe (
    %CC% tools\gen_splash_bundle.c -o tools\gen_splash_bundle.exe
)

echo [Generating Splash Bundle...]
.\tools\gen_splash_bundle.exe assets\splash src\ui\components\splash_bundle_tmp.h
type src\ui\components\splash_bundle_tmp.h >> src\ui\components\assets_bundle.h
del src\ui\components\splash_bundle_tmp.h

echo #endif >> src\ui\components\assets_bundle.h

if "%PLATFORM%"=="linux" (
    echo [Target: Linux ARM64]
    set TARGET_FLAGS=-target aarch64-linux-gnu.2.36 -DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2 -DKS_STR_ENCODING_NONE -Ilib/linux_arm64/include
    set LDFLAGS=-Llib/linux_arm64 -lraylib -lGLESv2 -lEGL -ldrm -lgbm -lpthread -ldl -lm
    set OUT_DIR=build\linux
    set TARGET=xdjunx
) else (
    echo [Target: Windows x64]
    set TARGET_FLAGS=-target x86_64-windows -D_WIN32 -DKS_STR_ENCODING_WIN32API
    set LDFLAGS=-Llib -lraylib -lgdi32 -lwinmm -lopengl32
    set OUT_DIR=build\windows
    set TARGET=xdjunx.exe
)

if not exist %OUT_DIR% mkdir %OUT_DIR%

set CFLAGS=-Wall -Wextra -Isrc -Isrc/core -Isrc/engine -Ilib -O2 %TARGET_FLAGS%
set CXXFLAGS=-Wall -Wextra -Isrc -Isrc/core -Isrc/engine -Ilib -Ilib/kaitai -Ilib/rekordbox-metadata -O2 -std=c++17 %TARGET_FLAGS%

echo Building C files...
for %%f in (src/main.c src/ui/components/theme.c src/ui/components/fonts.c src/ui/components/helpers.c src/ui/views/topbar.c src/ui/views/info.c src/ui/views/splash.c src/ui/views/settings.c src/ui/views/about.c src/ui/views/mixer.c src/ui/player/bottomstrip.c src/ui/player/beatfx.c src/ui/player/deckinfo.c src/ui/player/deckstrip.c src/ui/player/waveform.c src/ui/player/player.c src/audio/engine.c src/input/keyboard.c src/ui/browser/browser.c src/core/logger.c src/core/audio_backend.c src/core/logic/quantize.c src/core/logic/sync.c src/core/logic/settings_io.c src/core/logic/control_object.c src/core/midi/midi_handler.c src/core/midi/midi_mapper.c src/core/midi/midi_backend_win.c) do (
    %CC% %CFLAGS% -c %%f -o %%~pf%%~nf.o || exit /b 1
)

for %%f in (src/engine/fx/dsp_utils.c src/engine/fx/colorfx/space.c src/engine/fx/colorfx/dub_echo.c src/engine/fx/colorfx/sweep.c src/engine/fx/colorfx/noise.c src/engine/fx/colorfx/crush.c src/engine/fx/colorfx/filter.c src/engine/fx/colorfx/colorfx_manager.c) do (
    %CC% %CFLAGS% -c %%f -o %%~pf%%~nf.o || exit /b 1
)

for %%f in (src/engine/fx/beatfx/delay.c src/engine/fx/beatfx/echo.c src/engine/fx/beatfx/pingpong.c src/engine/fx/beatfx/spiral.c src/engine/fx/beatfx/roll.c src/engine/fx/beatfx/sliproll.c src/engine/fx/beatfx/reverb.c src/engine/fx/beatfx/helix.c src/engine/fx/beatfx/flanger.c src/engine/fx/beatfx/phaser.c src/engine/fx/beatfx/bfilter.c src/engine/fx/beatfx/trans.c src/engine/fx/beatfx/pitch.c src/engine/fx/beatfx/vinylbrake.c src/engine/fx/beatfx/beatfx_manager.c) do (
    %CC% %CFLAGS% -c %%f -o %%~pf%%~nf.o || exit /b 1
)

echo Building C++ files...
%CXX% %CXXFLAGS% -c lib/kaitai/kaitai/kaitaistream.cpp -o lib/kaitai/kaitai/kaitaistream.o || exit /b 1
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_anlz.cpp -o lib/rekordbox-metadata/rekordbox_anlz.o || exit /b 1
%CXX% %CXXFLAGS% -c lib/rekordbox-metadata/rekordbox_pdb.cpp -o lib/rekordbox-metadata/rekordbox_pdb.o || exit /b 1
%CXX% %CXXFLAGS% -c lib/serato/serato_parser.cpp -o lib/serato/serato_parser.o || exit /b 1
%CXX% %CXXFLAGS% -c lib/serato/serato_waveform.cpp -o lib/serato/serato_waveform.o || exit /b 1
%CXX% %CXXFLAGS% -c src/library/rekordbox_reader.cpp -o src/library/rekordbox_reader.o || exit /b 1
%CXX% %CXXFLAGS% -c src/library/serato_reader.cpp -o src/library/serato_reader.o || exit /b 1
%CXX% %CXXFLAGS% -c src/engine/util/engine_math.cpp -o src/engine/util/engine_math.o || exit /b 1

if "%2"=="check" (
    echo [Check Only] Compilation successful.
    goto end
)

echo Linking...
%CXX% %CXXFLAGS% src/main.o src/ui/components/theme.o src/ui/components/fonts.o src/ui/components/helpers.o src/ui/views/topbar.o src/ui/views/info.o src/ui/views/splash.o src/ui/views/settings.o src/ui/views/about.o src/ui/views/mixer.o src/ui/player/bottomstrip.o src/ui/player/beatfx.o src/ui/player/deckinfo.o src/ui/player/deckstrip.o src/ui/player/waveform.o src/ui/player/player.o src/audio/engine.o src/engine/util/engine_math.o src/engine/fx/dsp_utils.o src/engine/fx/colorfx/space.o src/engine/fx/colorfx/dub_echo.o src/engine/fx/colorfx/sweep.o src/engine/fx/colorfx/noise.o src/engine/fx/colorfx/crush.o src/engine/fx/colorfx/filter.o src/engine/fx/colorfx/colorfx_manager.o src/engine/fx/beatfx/delay.o src/engine/fx/beatfx/echo.o src/engine/fx/beatfx/pingpong.o src/engine/fx/beatfx/spiral.o src/engine/fx/beatfx/roll.o src/engine/fx/beatfx/sliproll.o src/engine/fx/beatfx/reverb.o src/engine/fx/beatfx/helix.o src/engine/fx/beatfx/flanger.o src/engine/fx/beatfx/phaser.o src/engine/fx/beatfx/bfilter.o src/engine/fx/beatfx/trans.o src/engine/fx/beatfx/pitch.o src/engine/fx/beatfx/vinylbrake.o src/engine/fx/beatfx/beatfx_manager.o src/input/keyboard.o src/ui/browser/browser.o src/core/logger.o src/core/audio_backend.o src/core/logic/quantize.o src/core/logic/sync.o src/core/logic/settings_io.o src/core/logic/control_object.o src/core/midi/midi_handler.o src/core/midi/midi_mapper.o src/core/midi/midi_backend_win.o lib/kaitai/kaitai/kaitaistream.o lib/rekordbox-metadata/rekordbox_anlz.o lib/rekordbox-metadata/rekordbox_pdb.o lib/serato/serato_parser.o lib/serato/serato_waveform.o src/library/rekordbox_reader.o src/library/serato_reader.o %LDFLAGS% -o %OUT_DIR%\%TARGET% || exit /b 1

if "%PLATFORM%"=="windows" (
    echo [Copying Dependencies...]
    if exist lib\raylib.dll copy /y lib\raylib.dll %OUT_DIR% > nul
)

:end
echo Done.
