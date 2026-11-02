# The whole program is this one component: every file under src/ for the ESP32 platform.
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ClankerSources.cmake)
clanker_collect_sources(ESP32 CLANKER_SOURCES CLANKER_HEADERS)

idf_component_register(
    SRCS ${CLANKER_SOURCES}
    INCLUDE_DIRS "${CLANKER_SOURCE_DIR}"
    # esp32/, for Secrets.h (see Config/Secrets.ESP32.h).
    PRIV_INCLUDE_DIRS ".."
    REQUIRES esp_driver_gpio esp_timer esp_hw_support esp_rom mbedtls pthread
             esp-tls esp_http_client esp_netif esp_event esp_wifi esp_eth nvs_flash lwip)

clanker_platform_definitions(${COMPONENT_LIB} ESP32)
# Run runs on the main task, which isn't a pthread; EntryPoint.ESP32.cpp's wrapper gives it a
# thread id instead of ESP-IDF's assert.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=pthread_self")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
