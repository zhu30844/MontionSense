# Rockit — Rockchip Media Process Interface (librockit.so).
# Owns rk_mpi_*.h / rk_comm_*.h; depends on mpp and rga at runtime.
#
# Two sources for the same binaries:
#   standalone  -> the snapshot vendored under this directory
#   Luckfox SDK -> -DMS_RK_MEDIA_DIR=<sdk>/output/out/media_out, so the app
#                  tracks whatever the SDK just built instead of a stale copy.
if(MS_RK_MEDIA_DIR)
    set(_rockit_inc "${MS_RK_MEDIA_DIR}/include")
    set(_rockit_lib "${MS_RK_MEDIA_DIR}/lib/librockit.so")
else()
    set(_rockit_inc "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(_rockit_lib "${CMAKE_CURRENT_SOURCE_DIR}/lib/librockit.so")
endif()

add_library(rk::rockit SHARED IMPORTED GLOBAL)
set_target_properties(rk::rockit PROPERTIES
    IMPORTED_LOCATION  "${_rockit_lib}"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${_rockit_inc}"
    INTERFACE_LINK_LIBRARIES "rk::mpp;rk::rga")
