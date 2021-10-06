cmake_minimum_required (VERSION 3.8)

add_executable (mkmz "main.cpp" "maze.h" )

find_package(PNG REQUIRED)

target_link_libraries(mkmz PRIVATE PNG::PNG pthread)
