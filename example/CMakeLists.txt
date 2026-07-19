cmake_minimum_required(VERSION 3.5)

project(platform LANGUAGES C)

if(NOT DEFINED APR_INCLUDE_DIR)
	message(FATAL_ERROR "APR_INCLUDE_DIR must point to the APR header directory")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../cmake_config.cmake)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib
	             ${CMAKE_BINARY_DIR}/lib)
target_include_directories(ecoa PUBLIC ${CMAKE_CURRENT_LIST_DIR})

add_executable(platform main.c)
target_link_libraries(platform PRIVATE ecoa apr-1 pthread rt m)

add_executable(PD_sender_PD PD_sender_PD.c)
target_link_libraries(PD_sender_PD PRIVATE ecoa apr-1 pthread rt m)

add_executable(PD_receiver_PD PD_receiver_PD.c)
target_link_libraries(PD_receiver_PD PRIVATE ecoa apr-1 pthread rt m)

add_executable(dds_route_test dds_route_test.c)
target_link_libraries(dds_route_test PRIVATE ecoa apr-1 pthread rt m)
