# ireader/media-server — MIT licensed, built from source.
# https://github.com/ireader/media-server
#
# The tree is a submodule at 3rdparty/media-server, pinned and kept pristine.
# This file used to live inside it as a CMakeLists.txt, which is not something
# a submodule can carry, so the glue moved out here.
#
# Only libmpeg and libhls are built; the rest of the project (flv, mov, mkv,
# dash, rtsp, ...) is not used.

set(MS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server")
set(MS_LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server-local")

if(NOT EXISTS "${MS_ROOT}/libhls/source/hls-media.c")
    message(FATAL_ERROR
        "3rdparty/media-server is empty. Clone with --recurse-submodules, or "
        "run: git submodule update --init --recursive")
endif()

file(GLOB MPEG_SRCS "${MS_ROOT}/libmpeg/source/*.c")
add_library(mpeg STATIC ${MPEG_SRCS})
add_library(media::mpeg ALIAS mpeg)
target_include_directories(mpeg PUBLIC  "${MS_ROOT}/libmpeg/include"
                                PRIVATE "${MS_ROOT}/libmpeg/source")

file(GLOB HLS_SRCS "${MS_ROOT}/libhls/source/*.c")

# hls-fmp4.c depends on libmov (fmp4 container), which we don't need.
list(REMOVE_ITEM HLS_SRCS "${MS_ROOT}/libhls/source/hls-fmp4.c")

# hls-media.c is compiled from 3rdparty/media-server-local instead: upstream's
# continuity assert does not allow the duration==0 mode storage.c uses. See the
# README there.
list(REMOVE_ITEM HLS_SRCS "${MS_ROOT}/libhls/source/hls-media.c")
list(APPEND     HLS_SRCS "${MS_LOCAL}/hls-media.c")

add_library(hls STATIC ${HLS_SRCS})
add_library(media::hls ALIAS hls)
target_include_directories(hls PUBLIC  "${MS_ROOT}/libhls/include"
                                PRIVATE "${MS_ROOT}/libhls/source")
target_link_libraries(hls PUBLIC mpeg)
