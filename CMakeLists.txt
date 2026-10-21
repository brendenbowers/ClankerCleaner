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

# spdlog, downloaded and built by CMake on first configure (cached in build/_deps).
include(FetchContent)
FetchContent_Declare(spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(spdlog)

# argparse (header-only command-line parser), fetched the same way.
FetchContent_Declare(argparse
    URL https://github.com/p-ranav/argparse/archive/refs/tags/v3.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(argparse)

# Every source and header under src/; CONFIGURE_DEPENDS re-checks the folders on each build.
file(GLOB_RECURSE CLANKER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE CLANKER_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_SOURCE_DIR}/src/*.h")

add_executable(clanker_cleaner
    ${CLANKER_SOURCES}
    ${CLANKER_HEADERS})

# Headers include relative to src/, e.g. "hardware/Probe.h".
target_include_directories(clanker_cleaner PRIVATE "${CMAKE_SOURCE_DIR}/src")

target_compile_definitions(clanker_cleaner PRIVATE
    CLANKER_VERSION="${PROJECT_VERSION}")

target_compile_options(clanker_cleaner PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(clanker_cleaner PRIVATE
    PkgConfig::GPIOD
    spdlog::spdlog
    argparse::argparse
    Threads::Threads)
