cmake_minimum_required(VERSION 3.8)
project(PathORAM)

if (MSVC)
	set(DEBUG_OPTIONS /W3 /Od)
	set(RELEASE_OPTIONS /W3 /O2 /DNDEBUG)
else()
	set(DEBUG_OPTIONS -Wall -Wextra -O0 -g)
	set(RELEASE_OPTIONS -Wall -Wextra -O3 -DNDEBUG)
endif()

add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>)
target_compile_options(${PROJECT_NAME} PUBLIC $<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>)
