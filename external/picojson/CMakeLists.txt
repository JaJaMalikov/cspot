cmake_minimum_required(VERSION 3.16)
project(picojson)

# Add the library target
add_library(picojson INTERFACE)
target_include_directories(picojson INTERFACE ".")