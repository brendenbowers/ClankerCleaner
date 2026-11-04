# Source list shared by the host build (../CMakeLists.txt) and the ESP-IDF build (../esp32).
#
# Platform-specific files are named <Name>.<Platform>.cpp/.h, e.g. GPIO/PinInterface.RPi.cpp,
# and only the selected platform's files are compiled.
set(CLANKER_PLATFORMS RPi ESP32)
set(CLANKER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../src")
cmake_path(NORMAL_PATH CLANKER_SOURCE_DIR)

# Version from the top-level project(), so both builds report the same one.
file(READ "${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt" _clanker_top_level)
string(REGEX MATCH "project\\(ClankerCleaner[^)]*VERSION ([0-9.]+)" _clanker_match "${_clanker_top_level}")
set(CLANKER_VERSION "${CMAKE_MATCH_1}")
unset(_clanker_top_level)
unset(_clanker_match)

# Publishing to and taking commands from an MQTT broker. Off, TELEMETRY_TRACK is a no-op and
# the publisher and command flow aren't built.
# option() isn't allowed in script mode, which ESP-IDF uses for its first pass.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    option(CLANKER_MQTT "Publish status and take commands over MQTT" ON)
endif()
if(NOT DEFINED CLANKER_MQTT)
    set(CLANKER_MQTT ON)
endif()

# Sets <sourcesVar> and <headersVar> to every file under src/ for <platform>.
function(clanker_collect_sources platform sourcesVar headersVar)
    if(NOT platform IN_LIST CLANKER_PLATFORMS)
        message(FATAL_ERROR "Unknown platform '${platform}'; expected one of: ${CLANKER_PLATFORMS}")
    endif()

    # CONFIGURE_DEPENDS re-checks the folders on each build; it isn't allowed in script mode,
    # which ESP-IDF uses for its first pass over the components.
    set(globFlags)
    if(NOT CMAKE_SCRIPT_MODE_FILE)
        set(globFlags CONFIGURE_DEPENDS)
    endif()
    file(GLOB_RECURSE sources ${globFlags} "${CLANKER_SOURCE_DIR}/*.cpp")
    file(GLOB_RECURSE headers ${globFlags} "${CLANKER_SOURCE_DIR}/*.hpp" "${CLANKER_SOURCE_DIR}/*.h")

    # Drop files belonging to the other platforms.
    foreach(otherPlatform IN LISTS CLANKER_PLATFORMS)
        if(NOT otherPlatform STREQUAL platform)
            list(FILTER sources EXCLUDE REGEX "\\.${otherPlatform}\\.cpp$")
            list(FILTER headers EXCLUDE REGEX "\\.${otherPlatform}\\.(h|hpp)$")
        endif()
    endforeach()

    if(NOT CLANKER_MQTT)
        list(FILTER sources EXCLUDE REGEX "/(Telemetry/Telemetry|Net/MqttPublisher|Flows/Commands/.*)\\.cpp$")
        list(FILTER headers EXCLUDE REGEX "/(Net/MqttPublisher|Flows/Commands/.*)\\.h$")
    endif()

    set(${sourcesVar} ${sources} PARENT_SCOPE)
    set(${headersVar} ${headers} PARENT_SCOPE)
endfunction()

# CLANKER_VERSION, CLANKER_PLATFORM, CLANKER_PLATFORM_<PLATFORM>=1 and CLANKER_MQTT (1 or 0)
# for <target>.
function(clanker_platform_definitions target platform)
    string(TOUPPER "${platform}" platformUpper)
    set(mqtt 0)
    if(CLANKER_MQTT)
        set(mqtt 1)
    endif()
    target_compile_definitions(${target} PRIVATE
        CLANKER_VERSION="${CLANKER_VERSION}"
        CLANKER_PLATFORM="${platform}"
        CLANKER_PLATFORM_${platformUpper}=1
        CLANKER_MQTT=${mqtt})
endfunction()
