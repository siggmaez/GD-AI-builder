cmake_minimum_required(VERSION 3.21)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")

project(GDAIBuilder VERSION 0.5.2)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})
if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
	message(FATAL_ERROR "Define the GEODE_SDK environment variable")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})
