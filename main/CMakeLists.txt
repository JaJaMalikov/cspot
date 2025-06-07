cmake_minimum_required(VERSION 3.10)

project(cspot)

# Configurable options
# Only build the library for ESP-IDF; external dependencies are removed
option(CSPOT_DISABLE_TESTS "Disable cspot unit tests" ON)

# CMake options
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

if(MSVC)
    add_compile_definitions(NOMINMAX _WINSOCK_DEPRECATED_NO_WARNINGS _CRT_SECURE_NO_WARNINGS)
    add_definitions(/wd4068 /wd4244 /wd4018 /wd4101 /wd4102 /wd4142 /wd4996)
endif()

# Main library sources
file(GLOB SOURCES "src/*.cpp" "src/events/*.cpp" "src/*.c" "src/api/*.cpp" "src/crypto/*.cpp" "src/proto/*.cpp")

# Enable testing
if(NOT CSPOT_DISABLE_TESTS)
    enable_testing()
    add_subdirectory("../test" ${CMAKE_CURRENT_BINARY_DIR}/test)
endif()

list(APPEND EXTRA_LIBS esp_websocket_client esp_http_client protobuf-c cjson)


# Use protobuf-c and cJSON from ESP-IDF

# target_compile_definitions(cspot-proto PUBLIC PB_FIELD_32BIT)
add_library(cspot STATIC ${SOURCES})

target_link_libraries(cspot PUBLIC ${EXTRA_LIBS})
target_include_directories(cspot PUBLIC "include" ${CMAKE_CURRENT_BINARY_DIR})
