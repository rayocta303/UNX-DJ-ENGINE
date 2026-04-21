@echo off
set ZIG=t:\zig\zig.exe
set SRC_DIR=p:\XDJ-UNX-C\lib\linux_arm64\raylib-5.0\src
set OUT_DIR=p:\XDJ-UNX-C\lib\linux_arm64
set TARGET=aarch64-linux-gnu.2.36
set FLAGS=-target %TARGET% -O3 -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_ES2 -D_GNU_SOURCE -I%SRC_DIR% -I%SRC_DIR%/external/glfw/include -I%OUT_DIR%/include

echo [Building Raylib Desktop ARM64...]
"%ZIG%" cc -c "%SRC_DIR%\rcore.c" %FLAGS% -o rcore_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\rshapes.c" %FLAGS% -o rshapes_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\rtextures.c" %FLAGS% -o rtextures_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\rtext.c" %FLAGS% -o rtext_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\rmodels.c" %FLAGS% -o rmodels_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\raudio.c" %FLAGS% -o raudio_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\utils.c" %FLAGS% -o utils_desktop.o
"%ZIG%" cc -c "%SRC_DIR%\rglfw.c" %FLAGS% -o rglfw_desktop.o

echo [Creating Static Library...]
"%ZIG%" ar rcs "%OUT_DIR%\libraylib_desktop.a" rcore_desktop.o rshapes_desktop.o rtextures_desktop.o rtext_desktop.o rmodels_desktop.o raudio_desktop.o utils_desktop.o rglfw_desktop.o

echo [Cleanup...]
del *.o
echo Done.
