cmake_minimum_required(VERSION 3.13)

# ------------------------------------------------------------------
# Toolchain
#
# The cross toolchain is no longer vendored here: it is a byte-identical copy
# of the one the Luckfox SDK ships, and carrying a second 380MB of it (plus
# Rockchip's closed-source flashing tools) served no purpose once this tree
# lives at <sdk>/project/app/motionsense.
#
# Resolution order:
#   1. CMAKE_C_COMPILER already set  -> the SDK app build passes it in
#   2. $LF_TOOLCHAIN                 -> exported by the devcontainer
#   3. ../../../tools/...            -> this tree's place inside the SDK
# ------------------------------------------------------------------
if(NOT DEFINED CMAKE_C_COMPILER)
    if(DEFINED ENV{LF_TOOLCHAIN})
        set(MS_TOOLCHAIN "$ENV{LF_TOOLCHAIN}")
    else()
        set(MS_TOOLCHAIN
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf")
    endif()

    if(NOT EXISTS "${MS_TOOLCHAIN}/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc")
        message(FATAL_ERROR
            "Cross toolchain not found at ${MS_TOOLCHAIN}.\n"
            "Build from inside a Luckfox SDK checkout, set LF_TOOLCHAIN, or pass "
            "-DCMAKE_C_COMPILER explicitly.")
    endif()

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
# Dependencies — Rockchip libs from vendor/ or the SDK, distro libs from the
# buildroot sysroot, the rest built from source under 3rdparty/.
# ------------------------------------------------------------------
add_subdirectory(vendor/mpp)
add_subdirectory(vendor/rga)
add_subdirectory(vendor/rkaiq)
add_subdirectory(vendor/rockit)

# sqlite3, libyaml, libiconv and freetype come from the buildroot sysroot
# rather than vendored source; see cmake/SystemLibs.cmake.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SystemLibs.cmake)

# Still built from source: not distro packages, and patched/trimmed for us.
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
