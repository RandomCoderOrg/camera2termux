cmake_minimum_required(VERSION 3.22)
project(camera2termux C)
set(CMAKE_C_STANDARD 11)
add_library(fauxdev-camera SHARED
    camera_socket.c
    ../../../../fauxdev/android/camera_broker.c
    ../../../../fauxdev/android/yuv_rgb.c
    ../../../../fauxdev/src/transport.c)
target_compile_definitions(fauxdev-camera PRIVATE _GNU_SOURCE)
target_compile_options(fauxdev-camera PRIVATE -Wall -Wextra -Werror)
target_include_directories(fauxdev-camera PRIVATE
    ../../../../fauxdev/include
    ../../../../fauxdev/src)
target_link_libraries(fauxdev-camera camera2ndk mediandk android log)
