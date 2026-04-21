#!/bin/bash
# Raylib Build Script for XDJ-UNX-C
# Usage: ./tools/build_raylib.sh [drm|desktop]

BACKEND=${1:-drm}
RAYLIB_PATH="lib/linux_arm64/raylib-5.0/src"
OUT_PATH="lib/linux_arm64"
ZIG=${ZIG:-zig}

echo "[Building Raylib for Backend: $BACKEND]"

COMMON_FLAGS="-O2 -fPIC -I$RAYLIB_PATH -I$RAYLIB_PATH/external/glfw/include -target aarch64-linux-gnu.2.36"

if [ "$BACKEND" == "desktop" ]; then
    PLATFORM_FLAGS="-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_ES2 -D_GLFW_WAYLAND -D_GLFW_X11"
else
    PLATFORM_FLAGS="-DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2"
fi

# Compile core files
FILES=("rcore.c" "rshapes.c" "rtextures.c" "rtext.c" "rmodels.c" "raudio.c" "utils.c")

for f in "${FILES[@]}"; do
    echo "  Compiling $f..."
    $ZIG cc $COMMON_FLAGS $PLATFORM_FLAGS -c "$RAYLIB_PATH/$f" -o "$RAYLIB_PATH/${f%.c}.o" || exit 1
done

# Compile GLFW if desktop
if [ "$BACKEND" == "desktop" ]; then
    echo "  Compiling rglfw.c..."
    $ZIG cc $COMMON_FLAGS $PLATFORM_FLAGS -c "$RAYLIB_PATH/rglfw.c" -o "$RAYLIB_PATH/rglfw.o" || exit 1
fi

# Create archive
echo "  Creating archive..."
$ZIG ar rcs "$OUT_PATH/libraylib.a" $RAYLIB_PATH/*.o

echo "[Done] Raylib built and moved to $OUT_PATH/libraylib.a"
