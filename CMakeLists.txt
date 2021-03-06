cmake_minimum_required(VERSION 3.5)
project(beevdp)

# Require C++17 (and position independent code)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BUILD_VDP_TESTS "Enables the BeeVDP test suite." OFF)

set(BEEVDP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(BEEVDP_TEST_SOURCES
	beevdp-tests.cpp)

set(BEEVDP_HEADER
	beevdp.h)

set(BEEVDP_SOURCE
	beevdp.cpp)

add_library(beevdp ${BEEVDP_SOURCE} ${BEEVDP_HEADER})
target_include_directories(beevdp PUBLIC ${BEEVDP_INCLUDE_DIR})
add_library(libbeevdp ALIAS beevdp)

if (BUILD_VDP_TESTS STREQUAL "ON")
    project(beevdp-tests)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSDL_MAIN_HANDLED")
    add_executable(${PROJECT_NAME} ${BEEVDP_TEST_SOURCES})
    include_directories(${PROJECT_NAME} ${BEEVDP_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} libbeevdp)
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})

    if (TARGET SDL2::SDL2)
	target_link_libraries(${PROJECT_NAME} SDL2::SDL2)
    else()
	target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
    endif()
endif()


if (WIN32)
    message(STATUS "Operating system is Windows.")
    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	target_compile_options(beevdp PRIVATE -Wall)
	message(STATUS "Compiler is MinGW.")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL MSVC)
	message(WARNING "MSVC support is HIGHLY experimental, and may not even compile correctly, so be EXTREMELY careful here!")
	target_compile_options(beevdp PRIVATE /W4)
	message(STATUS "Compiler is MSVC.")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
	message(WARNING "Clang support on Windows is HIGHLY experimental, and may not even compile correctly, so be EXTREMELY careful here!")
	target_compile_options(beevdp PRIVATE -Wall)
	message(STATUS "Compiler is Clang.")
    else()
	message(SEND_ERROR "Compiler not supported.")
	return()
    endif()
elseif(UNIX AND NOT APPLE)
    message(STATUS "Operating system is Linux.")
    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	target_compile_options(beevdp PRIVATE -Wall)
	message(STATUS "Compiler is GCC.")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
	target_compile_options(beevdp PRIVATE -Wall)
	message(STATUS "Compiler is Clang.")
    else()
	message(SEND_ERROR "Compiler not supported.")
	return()
    endif()
elseif(APPLE)
    message(STATUS "Operating system is Mac.")
    if (CMAKE_CXX_COMPILER_ID STREQUAL AppleClang)
	target_compile_options(beevdp PRIVATE -Wall)
	message(STATUS "Compiler is AppleClang.")
    else()
	message(SEND_ERROR "Compiler not supported.")
	return()
    endif()
else()
    message(SEND_ERROR "Operating system not supported.")
    return()
endif()
