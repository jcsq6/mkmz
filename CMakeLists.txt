cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)

project ("mkmz+")

add_executable(mkmz src/maze.cpp src/main.cpp src/image.cpp)

if(MSVC)
	target_compile_options(mkmz PUBLIC $<$<CONFIG:RELEASE>:/O2 /MT> $<$<CONFIG:DEBUG>:/MTd> /W2)
else()
	target_compile_options(mkmz PUBLIC $<$<CONFIG:DEBUG>:-g -fno-inline-functions> $<$<CONFIG:RELEASE>:-O3> -Wall)
endif()

find_package(PNG REQUIRED)

target_link_libraries(mkmz PUBLIC PNG::PNG)