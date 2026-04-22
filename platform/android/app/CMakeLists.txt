cmake_minimum_required(VERSION 3.10.2)
project(xdjunx)

set(RAYLIB_SRC "../../../lib/linux_arm64/raylib-5.0/src")
set(PROJECT_SRC "../../../src")
set(LIB_DIR "../../../lib")

# Definitions for Android
add_definitions(-DPLATFORM_ANDROID)
add_definitions(-DGRAPHICS_API_OPENGL_ES2)
add_definitions(-D_ANDROID)

# Include paths
include_directories(
    ${PROJECT_SRC}
    ${PROJECT_SRC}/core
    ${PROJECT_SRC}/engine
    ${LIB_DIR}
    ${LIB_DIR}/soundtouch
    ${LIB_DIR}/kaitai
    ${LIB_DIR}/rekordbox-metadata
    ${LIB_DIR}/serato
    ${RAYLIB_SRC}
    ${RAYLIB_SRC}/external/glfw/include
)

# Raylib Source Files
set(RAYLIB_FILES
    ${RAYLIB_SRC}/rcore.c
    ${RAYLIB_SRC}/rshapes.c
    ${RAYLIB_SRC}/rtextures.c
    ${RAYLIB_SRC}/rtext.c
    ${RAYLIB_SRC}/rmodels.c
    ${RAYLIB_SRC}/utils.c
    ${RAYLIB_SRC}/raudio.c
)

# Project Source Files (C)
set(C_FILES
    ${PROJECT_SRC}/main.c
    ${PROJECT_SRC}/ui/components/theme.c
    ${PROJECT_SRC}/ui/components/fonts.c
    ${PROJECT_SRC}/ui/components/helpers.c
    ${PROJECT_SRC}/ui/views/topbar.c
    ${PROJECT_SRC}/ui/views/info.c
    ${PROJECT_SRC}/ui/views/splash.c
    ${PROJECT_SRC}/ui/views/settings.c
    ${PROJECT_SRC}/ui/views/about.c
    ${PROJECT_SRC}/ui/views/mixer.c
    ${PROJECT_SRC}/ui/player/bottomstrip.c
    ${PROJECT_SRC}/ui/player/beatfx.c
    ${PROJECT_SRC}/ui/player/deckinfo.c
    ${PROJECT_SRC}/ui/player/deckstrip.c
    ${PROJECT_SRC}/ui/player/waveform.c
    ${PROJECT_SRC}/ui/player/player.c
    ${PROJECT_SRC}/audio/scalers.c
    ${PROJECT_SRC}/input/keyboard.c
    ${PROJECT_SRC}/ui/browser/browser.c
    ${PROJECT_SRC}/core/audio_backend.c
    ${PROJECT_SRC}/core/logger.c
    ${PROJECT_SRC}/core/system_info.c
    ${PROJECT_SRC}/core/logic/quantize.c
    ${PROJECT_SRC}/core/logic/sync.c
    ${PROJECT_SRC}/core/logic/settings_io.c
    ${PROJECT_SRC}/core/logic/control_object.c
    ${PROJECT_SRC}/core/midi/midi_handler.c
    ${PROJECT_SRC}/core/midi/midi_mapper.c
    ${PROJECT_SRC}/engine/fx/dsp_utils.c
    ${PROJECT_SRC}/engine/fx/colorfx/space.c
    ${PROJECT_SRC}/engine/fx/colorfx/dub_echo.c
    ${PROJECT_SRC}/engine/fx/colorfx/sweep.c
    ${PROJECT_SRC}/engine/fx/colorfx/noise.c
    ${PROJECT_SRC}/engine/fx/colorfx/crush.c
    ${PROJECT_SRC}/engine/fx/colorfx/filter.c
    ${PROJECT_SRC}/engine/fx/colorfx/colorfx_manager.c
    ${PROJECT_SRC}/engine/fx/beatfx/delay.c
    ${PROJECT_SRC}/engine/fx/beatfx/echo.c
    ${PROJECT_SRC}/engine/fx/beatfx/pingpong.c
    ${PROJECT_SRC}/engine/fx/beatfx/spiral.c
    ${PROJECT_SRC}/engine/fx/beatfx/roll.c
    ${PROJECT_SRC}/engine/fx/beatfx/sliproll.c
    ${PROJECT_SRC}/engine/fx/beatfx/reverb.c
    ${PROJECT_SRC}/engine/fx/beatfx/helix.c
    ${PROJECT_SRC}/engine/fx/beatfx/flanger.c
    ${PROJECT_SRC}/engine/fx/beatfx/phaser.c
    ${PROJECT_SRC}/engine/fx/beatfx/bfilter.c
    ${PROJECT_SRC}/engine/fx/beatfx/trans.c
    ${PROJECT_SRC}/engine/fx/beatfx/pitch.c
    ${PROJECT_SRC}/engine/fx/beatfx/vinylbrake.c
    ${PROJECT_SRC}/engine/fx/beatfx/beatfx_manager.c
)

# Project Source Files (CPP)
file(GLOB SOUNDTOUCH_FILES "${LIB_DIR}/soundtouch/*.cpp")
set(CPP_FILES
    ${LIB_DIR}/kaitai/kaitai/kaitaistream.cpp
    ${LIB_DIR}/rekordbox-metadata/rekordbox_anlz.cpp
    ${LIB_DIR}/rekordbox-metadata/rekordbox_pdb.cpp
    ${LIB_DIR}/serato/serato_parser.cpp
    ${LIB_DIR}/serato/serato_waveform.cpp
    ${PROJECT_SRC}/library/rekordbox_reader.cpp
    ${PROJECT_SRC}/library/serato_reader.cpp
    ${PROJECT_SRC}/engine/util/engine_math.cpp
    ${PROJECT_SRC}/audio/engine.cpp
    ${SOUNDTOUCH_FILES}
)

# Build shared library
add_library(xdjunx SHARED ${RAYLIB_FILES} ${C_FILES} ${CPP_FILES})

# Link libraries
target_link_libraries(xdjunx
    android
    log
    EGL
    GLESv2
)
