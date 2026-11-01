# The whole program is this one component: every file under src/ for the ESP32 platform.
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ClankerSources.cmake)
clanker_collect_sources(ESP32 CLANKER_SOURCES CLANKER_HEADERS)

idf_component_register(
    SRCS ${CLANKER_SOURCES}
    INCLUDE_DIRS "${CLANKER_SOURCE_DIR}"
    REQUIRES esp_driver_gpio esp_timer esp_hw_support mbedtls pthread)

clanker_platform_definitions(${COMPONENT_LIB} ESP32)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
