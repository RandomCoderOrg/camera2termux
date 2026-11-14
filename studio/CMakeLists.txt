cmake_minimum_required(VERSION 3.21)
project(camera2termux_studio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

add_executable(camera2termux-studio
    main.cpp
    CaptureThread.cpp
    CameraEngine.cpp
    CameraEngine.h
    PreviewItem.cpp
    PreviewItem.h
)
target_link_libraries(camera2termux-studio PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_options(camera2termux-studio PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(camera2termux-studio PRIVATE STUDIO_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS camera2termux-studio RUNTIME DESTINATION bin)
install(FILES Main.qml DESTINATION share/camera2termux/studio)
