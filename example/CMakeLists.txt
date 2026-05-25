cmake_minimum_required(VERSION 3.5)

project(platform LANGUAGES C)

if(NOT DEFINED APR_INCLUDE_DIR)
	message(FATAL_ERROR "APR_INCLUDE_DIR must point to the APR header directory")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake_config.cmake)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib
	             ${CMAKE_BINARY_DIR}/lib)

add_executable(platform platform.c)
target_link_libraries(platform PRIVATE ecoa apr-1 pthread rt m)
