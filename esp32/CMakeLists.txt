# ESP-IDF project for the ESP32-S3 firmware. It compiles the same src/ as the Pi build, with
# the *.ESP32.cpp platform files. Build it with scripts\esp-idf.cmd build, which sets up
# ESP-IDF from .cache; see the README.
cmake_minimum_required(VERSION 3.22)

include(${CMAKE_CURRENT_LIST_DIR}/../cmake/ClankerSources.cmake)
set(PROJECT_VER "${CLANKER_VERSION}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(clanker_cleaner)

# Header-only dependencies, the same versions as ../CMakeLists.txt. Only their headers are
# used, so their own CMake projects (written for a desktop build) aren't run.
include(FetchContent)
FetchContent_Declare(spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR headers-only)
FetchContent_Declare(argparse
    URL https://github.com/p-ranav/argparse/archive/refs/tags/v3.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR headers-only)
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR headers-only)
FetchContent_MakeAvailable(spdlog argparse nlohmann_json)

idf_component_get_property(clanker_main_lib main COMPONENT_LIB)
target_include_directories(${clanker_main_lib} SYSTEM PRIVATE
    "${spdlog_SOURCE_DIR}/include"
    "${argparse_SOURCE_DIR}/include"
    "${nlohmann_json_SOURCE_DIR}/include")
# The log pattern doesn't print the thread id, so spdlog needn't look it up for every message.
target_compile_definitions(${clanker_main_lib} PRIVATE SPDLOG_NO_THREAD_ID)
