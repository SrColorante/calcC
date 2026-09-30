cmake_minimum_required(VERSION 3.14)
project(CalcC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# Senza questo, CMAKE_BUILD_TYPE resta vuoto e TUTTO - applicazione
# compresa, e raylib compresa - viene compilato a -O0. Con la sola
# app a -O0 il frame costava 0.70 ms; a -O2 si dimezza.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo di build" FORCE)
endif()

# 1) Fix compatibility with newer CMake for fetching raylib
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# 2) Wayland e' il backend di riferimento. ATTENZIONE: USE_X11 OFF NON
#    disabilita X11 - raylib non lo propaga a GLFW_BUILD_X11, quindi
#    nella libreria restano compilati entrambi i backend e GLFW sceglie
#    a runtime (preferendo Wayland). Lo si dichiara per coerenza, non
#    perche' elimini qualcosa.
set(USE_WAYLAND ON CACHE BOOL "Prefer Wayland" FORCE)
set(USE_X11 OFF CACHE BOOL "Non richiesto: Wayland ha la precedenza" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Examples" FORCE)

# 3) Il thread che interroga il tema di sistema (vedi ui/theme.c)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

include(FetchContent)
# URL_HASH: senza, l'archivio e' pinnato solo per tag e non verificato,
# e il configure scarica 32 MB dalla rete.
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
    URL_HASH SHA256=98f049b9ea2a9c40a14e4e543eeea1a7ec3090ebdcd329c4ca2cf98bc9793482
)
FetchContent_MakeAvailable(raylib)

add_executable(calcC
    main.c
    logic/eval.c
    ui/button.c
    ui/renderer.c
    ui/theme.c
)

target_include_directories(calcC PRIVATE . logic ui)
target_link_libraries(calcC PRIVATE raylib m Threads::Threads)
