cmake_minimum_required(VERSION 3.13)

# ------------------------------------------------------------------
# Toolchain
# Skipped when CMAKE_C_COMPILER is already set (e.g. by Buildroot).
# ------------------------------------------------------------------
if(NOT DEFINED CMAKE_C_COMPILER)
    set(MS_TOOLCHAIN
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf")
    set(CMAKE_C_COMPILER
        "${MS_TOOLCHAIN}/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc")
    set(CMAKE_CXX_COMPILER
        "${MS_TOOLCHAIN}/bin/arm-rockchip830-linux-uclibcgnueabihf-g++")
endif()

project("MotionSense" C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Buildroot sets CMAKE_INSTALL_PREFIX via -D; standalone falls back here.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install/MotionSense"
        CACHE PATH "" FORCE)
endif()

add_compile_options(
    -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
    -DISP_HW_V30 -DRKPLATFORM=ON -DARCH64=OFF -DUAPI2
    -DRV1106_1103
    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
)

# Disable asserts by default: a failed assert() in a vendored lib (libhls,
# sqlite, ...) calls abort() and would take down the whole daemon in production.
# Keep them only for an explicit `-DCMAKE_BUILD_TYPE=Debug` build. Must stay
# before add_subdirectory() so it also covers the vendored libraries below.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-DNDEBUG)
endif()

# ------------------------------------------------------------------
# Dependencies — all sourced from the tree (vendor/ and 3rdparty/).
# ------------------------------------------------------------------
add_subdirectory(vendor/mpp)
add_subdirectory(vendor/rga)
add_subdirectory(vendor/rkaiq)
add_subdirectory(vendor/rockit)

add_subdirectory(3rdparty/sqlite)
add_subdirectory(3rdparty/iconv EXCLUDE_FROM_ALL)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/libyaml EXCLUDE_FROM_ALL)
add_library(yaml::yaml ALIAS yaml)

set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS   OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/freetype EXCLUDE_FROM_ALL)
add_library(freetype::freetype ALIAS freetype)

add_subdirectory(3rdparty/media_server)
add_subdirectory(3rdparty/osd)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ------------------------------------------------------------------
# Executable
# ------------------------------------------------------------------
add_executable(${PROJECT_NAME}
    src/main.c
    src/config.c
    src/frame_queue.c
    src/media.c
    src/utils.c
    src/storage.c
    src/storage_db.c
    # src/web.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    rk::rockit
    rk::rkaiq
    sqlite3::sqlite3
    media::hls
    osd::osd
    yaml::yaml
)

# Standalone installs a self-contained dir (binary + lib/ next to it).
# The SDK installs the binary to /oem/usr/bin and the RK libs land in
# /oem/usr/lib, one level up, so the RPATH differs.
if(MS_RK_MEDIA_DIR)
    set(MS_RPATH "\$ORIGIN/../lib")
else()
    set(MS_RPATH "\$ORIGIN/lib")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "${MS_RPATH}"
    BUILD_RPATH   "${MS_RPATH}")

# ------------------------------------------------------------------
# Install
# ------------------------------------------------------------------
install(TARGETS ${PROJECT_NAME} DESTINATION ".")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.yaml DESTINATION ".")
# install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/www/ DESTINATION "www")

# Vendor shared libraries. Only needed for the standalone layout: under the
# Luckfox SDK these already ship in /oem/usr/lib (build.sh copies media_out/lib
# there), so a second copy would waste oem space and could drift from the SDK.
if(NOT MS_RK_MEDIA_DIR)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/rockit/lib/librockit.so
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/rkaiq/lib/librkaiq.so
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mpp/lib/librockchip_mpp.so
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mpp/lib/librockchip_mpp.so.0
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mpp/lib/librockchip_mpp.so.1
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/rga/lib/librga.so
        DESTINATION "lib")
endif()
