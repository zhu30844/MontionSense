# Rockchip MPP — hardware codec layer (librockchip_mpp.so).
# Note the include layout differs between the two sources: the vendored
# snapshot keeps the headers flat, the SDK puts them under include/rockchip.
if(MS_RK_MEDIA_DIR)
    set(_mpp_inc "${MS_RK_MEDIA_DIR}/include/rockchip")
    set(_mpp_lib "${MS_RK_MEDIA_DIR}/lib/librockchip_mpp.so")
else()
    set(_mpp_inc "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(_mpp_lib "${CMAKE_CURRENT_SOURCE_DIR}/lib/librockchip_mpp.so")
endif()

add_library(rk::mpp SHARED IMPORTED GLOBAL)
set_target_properties(rk::mpp PROPERTIES
    IMPORTED_LOCATION  "${_mpp_lib}"
    IMPORTED_NO_SONAME TRUE
    INTERFACE_INCLUDE_DIRECTORIES "${_mpp_inc}")
