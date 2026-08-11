cmake_minimum_required(VERSION 3.5)

project(ecoa)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake_config.cmake)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

file(GLOB ${PROJECT_NAME}_src *.c eli/*.c)
# ldp_core_udp.c implements the UDP transport and is not a common source.
list(REMOVE_ITEM ${PROJECT_NAME}_src
	${CMAKE_CURRENT_LIST_DIR}/ldp_core_udp.c)

if(${LDP_LOG_USE} STREQUAL "lttng")
	list(REMOVE_ITEM ${PROJECT_NAME}_src ${CMAKE_CURRENT_LIST_DIR}/ldp_log-log4cplus.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-console.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-zlog.c)
elseif(${LDP_LOG_USE} STREQUAL "console")
	list(REMOVE_ITEM ${PROJECT_NAME}_src ${CMAKE_CURRENT_LIST_DIR}/ldp_log-log4cplus.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-lttng.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-zlog.c)
elseif(${LDP_LOG_USE} STREQUAL "zlog")
	list(REMOVE_ITEM ${PROJECT_NAME}_src ${CMAKE_CURRENT_LIST_DIR}/ldp_log-log4cplus.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-lttng.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-console.c)
else()
	list(REMOVE_ITEM ${PROJECT_NAME}_src ${CMAKE_CURRENT_LIST_DIR}/ldp_log-lttng.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-console.c ${CMAKE_CURRENT_LIST_DIR}/ldp_log-zlog.c)
endif()

if(${CMAKE_USE_DDS_PROTO})
	find_package(CycloneDDS REQUIRED)
	file(GLOB ${PROJECT_NAME}_proto_src dds/*.c)
elseif(${CMAKE_USE_UDP_PROTO})
	file(GLOB ${PROJECT_NAME}_proto_src udp/*.c)
	list(APPEND ${PROJECT_NAME}_proto_src
		${CMAKE_CURRENT_LIST_DIR}/ldp_core_udp.c)
elseif(${CMAKE_USE_APEX_PORT})
	file(GLOB ${PROJECT_NAME}_proto_src apex_port/*.c)
else()
	file(GLOB ${PROJECT_NAME}_proto_src tcp/*.c)
endif()

if(USE_APEX_API)
	list(REMOVE_ITEM ${PROJECT_NAME}_src
		${CMAKE_CURRENT_LIST_DIR}/ldp_trigger.c)
	set(${PROJECT_NAME}_platform_src
		${CMAKE_CURRENT_LIST_DIR}/apex/ldp_thread.c
		${CMAKE_CURRENT_LIST_DIR}/apex/ldp_fine_grain_deployment.c
		${CMAKE_CURRENT_LIST_DIR}/apex/ldp_time_manager.c
		${CMAKE_CURRENT_LIST_DIR}/apex/ldp_trigger.c)
elseif(UNIX)
	file(GLOB ${PROJECT_NAME}_platform_src unix/*.c)
elseif(WIN32)
	file(GLOB ${PROJECT_NAME}_platform_src win32/*.c)
else(UNIX)

endif(UNIX)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../)
include_directories(${CMAKE_CURRENT_LIST_DIR}/.)
include_directories(${CMAKE_CURRENT_LIST_DIR}/eli)

if(${CMAKE_USE_DDS_PROTO})
	include_directories(${CMAKE_CURRENT_LIST_DIR}/dds)
elseif(${CMAKE_USE_UDP_PROTO})
	include_directories(${CMAKE_CURRENT_LIST_DIR}/udp)
elseif(${CMAKE_USE_APEX_PORT})
	include_directories(${CMAKE_CURRENT_LIST_DIR}/apex_port)
else()
	include_directories(${CMAKE_CURRENT_LIST_DIR}/tcp)
endif()

#####################
add_library(ecoa ${LDP_LINK_TYPE} ${${PROJECT_NAME}_src} ${${PROJECT_NAME}_platform_src} ${${PROJECT_NAME}_proto_src})

if(${CMAKE_USE_APEX_PORT} AND NOT USE_APEX_API)
	message(FATAL_ERROR "LDP_LOCAL_TRANSPORT=APEX requires USE_APEX_API=ON.")
endif()

if(USE_APEX_API)
	target_compile_definitions(ecoa PUBLIC USE_APEX_API=1)
	target_include_directories(ecoa BEFORE PUBLIC ${CMAKE_CURRENT_LIST_DIR}/apex_apr/include)
	target_sources(ecoa PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_errno.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_general.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_pools.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_strings.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_thread_cond.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_thread_mutex.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_thread_proc.c
		${CMAKE_CURRENT_LIST_DIR}/apex_apr/src/apr_time.c
	)
endif()

if(${CMAKE_USE_DDS_PROTO})
	idlc_generate(
		TARGET ldp_dds_packet_idl
		FILES ${CMAKE_CURRENT_LIST_DIR}/dds/ldp_dds_packet.idl)
	target_link_libraries(ecoa PRIVATE ldp_dds_packet_idl CycloneDDS::ddsc)
endif()

if(${CMAKE_USE_DDS_PROTO})
	target_compile_definitions(ecoa PUBLIC USE_DDS_PROTO=1 USE_UDP_PROTO=0 USE_APEX_PORT=0)
elseif(${CMAKE_USE_UDP_PROTO})
	target_compile_definitions(ecoa PUBLIC USE_DDS_PROTO=0 USE_UDP_PROTO=1 USE_APEX_PORT=0)
elseif(${CMAKE_USE_APEX_PORT})
	target_compile_definitions(ecoa PUBLIC USE_DDS_PROTO=0 USE_UDP_PROTO=0 USE_APEX_PORT=1)
else()
	target_compile_definitions(ecoa PUBLIC USE_DDS_PROTO=0 USE_UDP_PROTO=0 USE_APEX_PORT=0)
endif()

target_include_directories(ecoa PUBLIC ${APR_INCLUDE_DIR})
target_include_directories(ecoa PUBLIC .)
target_include_directories(ecoa PUBLIC eli/)
target_include_directories(ecoa PUBLIC dds/)

if(${LDP_LOG_USE} STREQUAL "lttng")
	target_link_libraries(ecoa PRIVATE lttng-ust)
elseif(${LDP_LOG_USE} STREQUAL "log4cplus")
	target_link_libraries(ecoa PRIVATE log4cplus::log4cplus)
elseif(${LDP_LOG_USE} STREQUAL "zlog")
	target_link_libraries(ecoa PRIVATE zlog)
endif()

target_include_directories(ecoa PUBLIC ./udp)
target_include_directories(ecoa PUBLIC ./tcp)
target_include_directories(ecoa PUBLIC ./dds)
target_include_directories(ecoa PUBLIC ./apex_port)
