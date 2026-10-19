# Proyecto fxSDK para fx-CG50. Compilar con:  fxsdk build-cg
cmake_minimum_required(VERSION 3.15)
project(Ouroboros)

include(GenerateG3A)
find_package(Gint 2.9 REQUIRED)

set(SOURCES
    src/main.cpp
    src/core/game.cpp
    src/core/profile.cpp
    src/platform/save_io.cpp
    src/ui/render.cpp
)

add_executable(ouroboros ${SOURCES})
target_compile_options(ouroboros PRIVATE -Wall -Wextra -O2 -fno-exceptions -fno-rtti)
target_link_libraries(ouroboros Gint::Gint -lstdc++)

generate_g3a(TARGET ouroboros OUTPUT "Ouroboros.g3a"
    NAME "Ouroboros" ICONS assets-cg/icon-uns.png assets-cg/icon-sel.png)
