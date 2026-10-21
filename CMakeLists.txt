cmake_minimum_required(VERSION 3.22)

project(ClankerCleaner
    VERSION 0.1.0
    DESCRIPTION "Auto-drain and fill controller for the MOVA P10 Ultra dock"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# libgpiod 2.x C++ bindings (Raspberry Pi OS Trixie), installed by scripts/setup-pi.sh.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiodcxx>=2.0)
message(STATUS "libgpiod ${GPIOD_VERSION}")

find_package(Threads REQUIRED)

add_executable(clanker_cleaner
    src/main.cpp)

target_compile_definitions(clanker_cleaner PRIVATE
    CLANKER_VERSION="${PROJECT_VERSION}")

target_compile_options(clanker_cleaner PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(clanker_cleaner PRIVATE
    PkgConfig::GPIOD
    Threads::Threads)
