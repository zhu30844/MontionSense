# Rockchip ISP / 3A engine (librkaiq.so).
# Both sources use the same layout under their include root, so only the
# root differs; the subdirectory list below is shared.
if(MS_RK_MEDIA_DIR)
    set(_aiq_inc "${MS_RK_MEDIA_DIR}/include")
    set(_aiq_lib "${MS_RK_MEDIA_DIR}/lib/librkaiq.so")
else()
    set(_aiq_inc "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(_aiq_lib "${CMAKE_CURRENT_SOURCE_DIR}/lib/librkaiq.so")
endif()

add_library(rk::rkaiq SHARED IMPORTED GLOBAL)
set_target_properties(rk::rkaiq PROPERTIES
    IMPORTED_LOCATION  "${_aiq_lib}"
    IMPORTED_NO_SONAME TRUE)

target_include_directories(rk::rkaiq INTERFACE
    "${_aiq_inc}"
    "${_aiq_inc}/rkaiq"
    "${_aiq_inc}/rkaiq/uAPI2"
    "${_aiq_inc}/rkaiq/common"
    "${_aiq_inc}/rkaiq/xcore"
    "${_aiq_inc}/rkaiq/algos"
    "${_aiq_inc}/rkaiq/iq_parser"
    "${_aiq_inc}/rkaiq/iq_parser_v2"
    "${_aiq_inc}/rkaiq/smartIr")
