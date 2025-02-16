cmake_minimum_required(VERSION 3.10)
# Configure the compiler
set(SDK_PATH $ENV{LUCKFOX_SDK_PATH})
if(NOT SDK_PATH)
    message(FATAL_ERROR "Please Set Luckfox-pico SDK Path. Such as:export LUCKFOX_SDK_PATH=/home/user/luckfox-pico")
    return()
endif()
set(CMAKE_C_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-gcc"
)
set(CMAKE_CXX_COMPILER "${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-g++"
)

# Display the compiler information
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")


# Project Name
project("MotionSense")

add_compile_options(-g -Wall
                    -DISP_HW_V30 -DRKPLATFORM=ON -DARCH64=OFF
                    -DUAPI2  -DMG_ENABLE_DIRLIST=0
                    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 
                    )
# -DROCKIVA 
#set(CMAKE_PREFIX_PATH "${BUILDROOT_PATH}/usr")
#include_directories("${BUILDROOT_PATH}/usr/include")
#link_directories("${BUILDROOT_PATH}/usr/lib")
#set(CMAKE_BUILD_RPATH "${BUILDROOT_PATH}/usr/lib")
#set(CMAKE_INSTALL_RPATH "${BUILDROOT_PATH}/usr/lib")
#set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# Set the library directory
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/ 3rdparty.out)

set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
add_definitions(-DRV1106_1103)

#Thread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Add source files
file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" 
"${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mongoose/mongoose.c")
add_executable(${PROJECT_NAME} ${SRC_FILES})

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_CURRENT_SOURCE_DIR}/lib:/usr/lib")

target_link_libraries(${PROJECT_NAME}  
                    ${LIBRKNNRT}
                    Threads::Threads
                    rockiva
                    sample_comm
                    rockit
                    rockchip_mpp
                    rkaiq
                    pthread
                    sqlite3
                    hls
                    mpeg
                    )

target_include_directories(${PROJECT_NAME} PRIVATE                        
                            ${OpenCV_INCLUDE_DIRS}
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_CURRENT_SOURCE_DIR}/utils
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/rknpu2/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server/libdash/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server/libflv/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server/libhls/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server/libmkv/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server/libmov/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/media-server/libmpeg/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mongoose
                            ${CMAKE_CURRENT_SOURCE_DIR}/common 
                            ${CMAKE_CURRENT_SOURCE_DIR}/common/isp3.x   
                            ${CMAKE_CURRENT_SOURCE_DIR}/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rknn
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/uAPI2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/common
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/xcore
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/algos
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser_v2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/smartIr
                          )
                          

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install/${PROJECT_NAME}")
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})    

# sqlite so files
file(GLOB SQLITE_SO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libsqlite3.so*")
message(STATUS "SQLite SO files found: ${SQLITE_SO_FILES}")
install(FILES ${SQLITE_SO_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

