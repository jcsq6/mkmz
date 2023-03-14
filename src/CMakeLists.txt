cmake_minimum_required(VERSION 3.4)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS "*.cpp")

add_executable(mkmz ${SRC})

if(MSVC)
	target_compile_options(mkmz PUBLIC $<$<CONFIG:RELEASE>:/O2 /MT> $<$<CONFIG:DEBUG>:/MTd> /W2)
else()
	target_compile_options(mkmz PUBLIC $<$<CONFIG:DEBUG>:-g> $<$<CONFIG:RELEASE>:-O3> -Wall)
endif()

find_package(PNG REQUIRED)

target_link_libraries(mkmz PUBLIC PNG::PNG)