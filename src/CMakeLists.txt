cmake_minimum_required (VERSION 3.8)

add_executable (mkmz "main.cpp" "maze.h" )

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(PNG REQUIRED)
find_package(Threads REQUIRED)

if(MSVC)
	target_compile_options(mkmz PRIVATE "/MT")
endif()

target_link_libraries(mkmz PRIVATE PNG::PNG Threads::Threads)
