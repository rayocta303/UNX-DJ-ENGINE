@echo off
set BACKEND=%1
if "%BACKEND%"=="" set BACKEND=drm
set RAYLIB_PATH=lib\linux_arm64\raylib-5.0\src
set OUT_PATH=lib\linux_arm64
if "%ZIG%"=="" set ZIG=t:\zig\zig.exe

echo [Building Raylib for Backend: %BACKEND%]

set COMMON_FLAGS=-O2 -fPIC -I%RAYLIB_PATH% -I%RAYLIB_PATH%\external\glfw\include -target aarch64-linux-gnu.2.36

if "%BACKEND%"=="desktop" (
    set PLATFORM_FLAGS=-DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_ES2 -D_GLFW_WAYLAND -D_GLFW_X11
) else (
    set PLATFORM_FLAGS=-DPLATFORM_DRM -DGRAPHICS_API_OPENGL_ES2
)

echo   Compiling core files...
for %%f in (rcore.c rshapes.c rtextures.c rtext.c rmodels.c raudio.c utils.c) do (
    echo     %%f
    %ZIG% cc %COMMON_FLAGS% %PLATFORM_FLAGS% -c %RAYLIB_PATH%\%%f -o %RAYLIB_PATH%\%%~nf.o || exit /b 1
)

if "%BACKEND%"=="desktop" (
    echo   Compiling rglfw.c...
    %ZIG% cc %COMMON_FLAGS% %PLATFORM_FLAGS% -c %RAYLIB_PATH%\rglfw.c -o %RAYLIB_PATH%\rglfw.o || exit /b 1
)

echo   Creating archive...
%ZIG% ar rcs %OUT_PATH%\libraylib.a %RAYLIB_PATH%\*.o

echo [Done] Raylib built and moved to %OUT_PATH%\libraylib.a
