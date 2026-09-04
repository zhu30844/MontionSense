# Third-party libraries taken from the buildroot sysroot instead of being
# built from vendored source.
#
# These four are ordinary distro packages; carrying our own copies meant
# compiling them on every build and shipping duplicates of libraries the
# rootfs already had. They are enabled in
# sysdrv/tools/board/buildroot/motionsense_defconfig, so buildroot both
# builds them into the sysroot we link against and installs them into the
# target rootfs we run against.
#
# The target names match what the vendored subdirectories exported, so
# nothing downstream of here had to change.
#
# Which packages we need is declared in buildroot-packages.fragment; run
# scripts/gen-buildroot-defconfig.sh after changing it.
#
# On the sqlite migration: the vendored copy was compiled with
# SQLITE_THREADSAFE=1, SQLITE_ENABLE_WAL and SQLITE_DEFAULT_WAL_SYNCHRONOUS=1.
# Buildroot passes --enable-threadsafe whenever BR2_TOOLCHAIN_HAS_THREADS is
# set, which it is here, and storage_db.c already issues PRAGMA journal_mode
# and PRAGMA synchronous at connection setup, so the other two were redundant.

if(NOT MS_SYSROOT)
    if(DEFINED ENV{LF_SYSROOT})
        set(MS_SYSROOT "$ENV{LF_SYSROOT}")
    else()
        set(MS_SYSROOT
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot")
    endif()
endif()

if(NOT EXISTS "${MS_SYSROOT}/usr/include")
    message(FATAL_ERROR
        "Buildroot sysroot not found at ${MS_SYSROOT}.\n"
        "Build the SDK once (./build.sh) so buildroot populates it, set "
        "LF_SYSROOT, or pass -DMS_SYSROOT.")
endif()

# $(1) target name, $(2) soname, $(3) extra include subdir under usr/include
function(ms_import_syslib target soname)
    find_library(${target}_LIB NAMES ${soname}
                 PATHS "${MS_SYSROOT}/usr/lib" "${MS_SYSROOT}/lib"
                 NO_DEFAULT_PATH)
    if(NOT ${target}_LIB)
        message(FATAL_ERROR
            "lib${soname} not found in ${MS_SYSROOT}.\n"
            "Declare the package in buildroot-packages.fragment, run "
            "scripts/gen-buildroot-defconfig.sh, then rebuild the SDK.")
    endif()
    add_library(${target} SHARED IMPORTED GLOBAL)
    set_target_properties(${target} PROPERTIES
        IMPORTED_LOCATION "${${target}_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${MS_SYSROOT}/usr/include;${ARGN}")
endfunction()

ms_import_syslib(ms_sqlite3  sqlite3)
ms_import_syslib(ms_yaml     yaml)
ms_import_syslib(ms_iconv    iconv)
ms_import_syslib(ms_freetype freetype "${MS_SYSROOT}/usr/include/freetype2")

# The shared libfreetype needs libz and libbz2, which the vendored static build
# did not. Declaring them here rather than relying on some other -L on the link
# line happening to supply them: the SDK app build did resolve them by accident,
# through the media_out library directory the Rockchip targets bring in, while a
# standalone build failed with undefined references to inflateEnd and
# BZ2_bzDecompressInit.
ms_import_syslib(ms_z    z)
ms_import_syslib(ms_bz2  bz2)
set_property(TARGET ms_freetype APPEND PROPERTY
             INTERFACE_LINK_LIBRARIES ms_z ms_bz2)

add_library(sqlite3::sqlite3   ALIAS ms_sqlite3)
add_library(yaml::yaml         ALIAS ms_yaml)
add_library(iconv::iconv       ALIAS ms_iconv)
add_library(freetype::freetype ALIAS ms_freetype)
