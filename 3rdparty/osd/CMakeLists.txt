# OSD render layer — ported from rkipc/common/osd.
# Provides: FreeType-based text rendering into ARGB8888 buffers.
# Does NOT include osd.c (rkipc config framework); call font_factory APIs directly.
add_library(osd STATIC
    font_factory.c
    draw_paint.c
    color_table.c
    bmp_reader.c
)
add_library(osd::osd ALIAS osd)

target_include_directories(osd PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(osd PUBLIC freetype::freetype iconv::iconv)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/simsun_en.ttf" DESTINATION "fonts")
