#!/bin/bash
# XDJ-UNX-C Linux Build Script (Ported from build.bat)

ZIG=${ZIG:-zig}
CC="$ZIG cc"
CXX="$ZIG c++"

# Build Asset Bundle Tool
echo "[Building Asset Bundle Tool...]"
$CC tools/bin2c.c -o tools/bin2c

# Generate Asset Bundle Header
echo "[Generating Assets Bundle...]"
rm -f src/ui/components/assets_bundle.h
cat > src/ui/components/assets_bundle.h <<EOF
#ifndef ASSETS_BUNDLE_H
#define ASSETS_BUNDLE_H
EOF

./tools/bin2c "assets/fonts/otfs/Font Awesome 5 Free-Solid-900.otf" src/ui/components/assets_bundle.h font_awesome_solid append
./tools/bin2c "assets/fonts/otfs/Font Awesome 5 Free-Regular-400.otf" src/ui/components/assets_bundle.h font_awesome_regular append
./tools/bin2c "assets/fonts/otfs/Font Awesome 5 Brands-Regular-400.otf" src/ui/components/assets_bundle.h font_awesome_brand append
./tools/bin2c "assets/splash.png" src/ui/components/assets_bundle.h unx_logo append
./tools/bin2c "assets/icons/crown.png" src/ui/components/assets_bundle.h icon_crown append
./tools/bin2c "assets/icons/star.png" src/ui/components/assets_bundle.h icon_star append

$CC tools/gen_splash_bundle.c -o tools/gen_splash_bundle

echo "[Generating Splash Bundle...]"
./tools/gen_splash_bundle assets/splash src/ui/components/splash_bundle_tmp.h
cat src/ui/components/splash_bundle_tmp.h >> src/ui/components/assets_bundle.h
rm src/ui/components/splash_bundle_tmp.h

echo "#endif" >> src/ui/components/assets_bundle.h

# Targets and Flags
BACKEND=${1:-drm}
echo "[Selected Backend: $BACKEND]"

if [ "$BACKEND" == "desktop" ]; then
    TARGET_FLAGS="-target aarch64-linux-gnu.2.36 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_ES2 -DKS_STR_ENCODING_NONE -Ilib/linux_arm64/include"
    LDFLAGS="-Llib/linux_arm64 -lraylib -lGLESv2 -lEGL -lpthread -ldl -lm -lX11 -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon"
else
    TARGET_FLAGS="-target aarch64-linux-gnu.2.36 -DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2 -DKS_STR_ENCODING_NONE -Ilib/linux_arm64/include"
    LDFLAGS="-Llib/linux_arm64 -lraylib -lGLESv2 -lEGL -ldrm -lgbm -lpthread -ldl -lm"
fi

OUT_DIR="build/linux_$BACKEND"
TARGET="xdjunx"

mkdir -p "$OUT_DIR"

CFLAGS="-Wall -Wextra -Isrc -Isrc/core -Isrc/engine -Ilib -Ilib/soundtouch -O2 $TARGET_FLAGS"
CXXFLAGS="-Wall -Wextra -Isrc -Isrc/core -Isrc/engine -Ilib -Ilib/soundtouch -Ilib/kaitai -Ilib/rekordbox-metadata -O2 -std=c++17 $TARGET_FLAGS"

# Compiling C files
echo "Building C files..."
C_FILES=(
    src/main.c src/ui/components/theme.c src/ui/components/fonts.c src/ui/components/helpers.c
    src/ui/views/topbar.c src/ui/views/info.c src/ui/views/splash.c src/ui/views/settings.c
    src/ui/views/about.c src/ui/views/credits.c src/ui/views/mixer.c src/ui/player/bottomstrip.c src/ui/player/beatfx.c
    src/ui/player/deckinfo.c src/ui/player/deckstrip.c src/ui/player/waveform.c src/ui/player/player.c
    src/audio/scalers.c src/input/keyboard.c src/ui/browser/browser.c src/core/audio_backend.c
    src/core/logger.c src/core/system_info.c src/core/logic/quantize.c src/core/logic/sync.c src/core/logic/settings_io.c
    src/core/logic/control_object.c src/core/midi/midi_handler.c src/core/midi/midi_mapper.c
    src/core/midi/midi_backend_win.c
    src/engine/fx/dsp_utils.c src/engine/fx/colorfx/space.c src/engine/fx/colorfx/dub_echo.c
    src/engine/fx/colorfx/sweep.c src/engine/fx/colorfx/noise.c src/engine/fx/colorfx/crush.c
    src/engine/fx/colorfx/filter.c src/engine/fx/colorfx/colorfx_manager.c
    src/engine/fx/beatfx/delay.c src/engine/fx/beatfx/echo.c src/engine/fx/beatfx/pingpong.c
    src/engine/fx/beatfx/spiral.c src/engine/fx/beatfx/roll.c src/engine/fx/beatfx/sliproll.c
    src/engine/fx/beatfx/reverb.c src/engine/fx/beatfx/helix.c src/engine/fx/beatfx/flanger.c
    src/engine/fx/beatfx/phaser.c src/engine/fx/beatfx/bfilter.c src/engine/fx/beatfx/trans.c
    src/engine/fx/beatfx/pitch.c src/engine/fx/beatfx/vinylbrake.c src/engine/fx/beatfx/beatfx_manager.c
)

for f in "${C_FILES[@]}"; do
    $CC $CFLAGS -c "$f" -o "${f%.c}.o" || exit 1
done

# Compiling C++ files
echo "Building C++ files..."
CPP_FILES=(
    lib/kaitai/kaitai/kaitaistream.cpp
    lib/rekordbox-metadata/rekordbox_anlz.cpp
    lib/rekordbox-metadata/rekordbox_pdb.cpp
    lib/serato/serato_parser.cpp
    lib/serato/serato_waveform.cpp
    src/library/rekordbox_reader.cpp
    src/library/serato_reader.cpp
    src/engine/util/engine_math.cpp
    src/audio/engine.cpp
    lib/soundtouch/*.cpp
)

for f in "${CPP_FILES[@]}"; do
    if [[ $f == *"*"* ]]; then
        for sf in $f; do
            $CXX $CXXFLAGS -c "$sf" -o "${sf%.cpp}.o" || exit 1
        done
    else
        $CXX $CXXFLAGS -c "$f" -o "${f%.cpp}.o" || exit 1
    fi
done

# Linking
echo "Linking..."
OBJ_FILES=$(find src lib -name "*.o" ! -name "bin2c.o" ! -name "gen_splash_bundle.o" ! -path "*/soundtouch_temp/*")
$CXX $CXXFLAGS $OBJ_FILES $LDFLAGS -o "$OUT_DIR/$TARGET" || exit 1

if [ -d "controllers" ]; then
    cp -r controllers "$OUT_DIR/"
fi

echo "Done."
