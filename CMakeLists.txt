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

# Target platform; see cmake/ClankerSources.cmake for how platform files are picked.
include(cmake/ClankerSources.cmake)
set(CLANKER_PLATFORM "RPi" CACHE STRING "Target platform: ${CLANKER_PLATFORMS}")
set_property(CACHE CLANKER_PLATFORM PROPERTY STRINGS ${CLANKER_PLATFORMS})
if(NOT CLANKER_PLATFORM IN_LIST CLANKER_PLATFORMS)
    message(FATAL_ERROR "Unknown CLANKER_PLATFORM '${CLANKER_PLATFORM}'; expected one of: ${CLANKER_PLATFORMS}")
endif()
if(CLANKER_PLATFORM STREQUAL "ESP32")
    message(FATAL_ERROR "The ESP32 firmware builds with ESP-IDF from esp32/: scripts\\esp-idf.cmd build")
endif()
message(STATUS "Platform ${CLANKER_PLATFORM}")

include(FetchContent)

set(CLANKER_PLATFORM_LIBRARIES)
if(CLANKER_PLATFORM STREQUAL "RPi")
    # libgpiod 2.x C++ bindings (Raspberry Pi OS Trixie), installed by scripts/setup-pi.sh.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiodcxx>=2.0)
    message(STATUS "libgpiod ${GPIOD_VERSION}")
    list(APPEND CLANKER_PLATFORM_LIBRARIES PkgConfig::GPIOD)

    # toml++ (header-only) for the config file; only Config.RPi.cpp includes it.
    FetchContent_Declare(tomlplusplus
        URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(tomlplusplus)
    list(APPEND CLANKER_PLATFORM_LIBRARIES tomlplusplus::tomlplusplus)

    # cpp-httplib (header-only) for HTTPS to the Dreame/MOVA cloud; only HttpsClient.RPi.cpp
    # includes it. Needs libssl-dev and zlib1g-dev, installed by scripts/setup-pi.sh.
    set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
    set(HTTPLIB_REQUIRE_ZLIB ON CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(httplib
        URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(httplib)
    find_package(OpenSSL REQUIRED)
    # TlsStream.RPi.cpp (MQTT push) uses OpenSSL directly.
    list(APPEND CLANKER_PLATFORM_LIBRARIES httplib::httplib OpenSSL::SSL OpenSSL::Crypto)
endif()

find_package(Threads REQUIRED)

# spdlog, downloaded and built by CMake on first configure (cached in build/_deps).
FetchContent_Declare(spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(spdlog)

# argparse (header-only command-line parser), fetched the same way.
FetchContent_Declare(argparse
    URL https://github.com/p-ranav/argparse/archive/refs/tags/v3.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(argparse)

# nlohmann/json for the cloud API's requests and responses.
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# Every source and header under src/ for this platform.
clanker_collect_sources(${CLANKER_PLATFORM} CLANKER_SOURCES CLANKER_HEADERS)

add_executable(clanker_cleaner
    ${CLANKER_SOURCES}
    ${CLANKER_HEADERS})

# Headers include relative to src/, e.g. "hardware/Probe.h".
target_include_directories(clanker_cleaner PRIVATE "${CMAKE_SOURCE_DIR}/src")

clanker_platform_definitions(clanker_cleaner ${CLANKER_PLATFORM})

target_compile_options(clanker_cleaner PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(clanker_cleaner PRIVATE
    ${CLANKER_PLATFORM_LIBRARIES}
    spdlog::spdlog
    argparse::argparse
    nlohmann_json::nlohmann_json
    Threads::Threads)
