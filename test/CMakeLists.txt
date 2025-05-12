include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

find_package(Catch2 REQUIRED)
include(Catch)

add_executable(cspot-test
  main.cpp ApResolveTest.cpp
)
target_compile_options(cspot-test PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
target_link_libraries(cspot-test cspot Catch2::Catch2)

# catch_discover_tests(cspot-test)