cmake_minimum_required(VERSION 3.14)
project(CalcC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# 1) Fix compatibility with newer CMake for fetching raylib
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# 2) Forziamo Raylib a compilarsi SOLO per Wayland ignorando X11
set(USE_WAYLAND ON CACHE BOOL "Force Wayland" FORCE)
set(USE_X11 OFF CACHE BOOL "Disable X11" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Examples" FORCE)

include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
)
FetchContent_MakeAvailable(raylib)

add_executable(calcC 
    main.c
    logic/eval.c
    ui/button.c
)

target_include_directories(calcC PRIVATE . graphics logic ui)
target_link_libraries(calcC PRIVATE raylib m)
