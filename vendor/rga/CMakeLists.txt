# Rockchip RGA — 2D graphics accelerator (librga.so).
# MotionSense doesn't use the rga API directly; rockit links it transitively.
if(MS_RK_MEDIA_DIR)
    set(_rga_lib "${MS_RK_MEDIA_DIR}/lib/librga.so")
else()
    set(_rga_lib "${CMAKE_CURRENT_SOURCE_DIR}/lib/librga.so")
endif()

add_library(rk::rga SHARED IMPORTED GLOBAL)
set_target_properties(rk::rga PROPERTIES
    IMPORTED_LOCATION  "${_rga_lib}"
    IMPORTED_NO_SONAME TRUE)
