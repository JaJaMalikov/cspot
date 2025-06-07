cmake_minimum_required(VERSION 3.10)

# Build only the ESP32 target using ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(IDF_NO_INCLUDE ON)
add_subdirectory(targets/esp32)
project(cspot-esp32)
