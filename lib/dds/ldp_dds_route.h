#ifndef _LDP_DDS_ROUTE_H
#define _LDP_DDS_ROUTE_H

#include <stdint.h>

#define LDP_DDS_TOPIC_SOURCE_AUTO 1U
#define LDP_DDS_TOPIC_SOURCE_USER 2U

typedef enum ldp_dds_entity_kind_t {
	LDP_DDS_ENTITY_PLATFORM = 1,
	LDP_DDS_ENTITY_NODE = 2,
	LDP_DDS_ENTITY_PD = 3,
	LDP_DDS_ENTITY_MAIN = 4
} ldp_dds_entity_kind;

typedef enum ldp_dds_interface_kind_t {
	LDP_DDS_IFACE_MAIN_CONTROL = 1,
	LDP_DDS_IFACE_LOCAL_WIRE = 2,
	LDP_DDS_IFACE_ELI_LINK = 3
} ldp_dds_interface_kind;

typedef struct ldp_dds_entity_route_t {
	uint32_t id;
	ldp_dds_entity_kind kind;
	const char* name;
	const char* platform_name;
	const char* node_name;
	const char* pd_name;
	uint32_t domain_id;
	const char* partition;
} ldp_dds_entity_route;

typedef struct ldp_dds_control_route_t {
	uint32_t interface_id;
	const char* name;
	uint32_t source_entity_id;
	uint32_t target_entity_id;
	uint32_t domain_id;
	const char* topic_name;
	const char* partition;
	int source_legacy_port;
	int target_legacy_port;
} ldp_dds_control_route;

typedef struct ldp_dds_wire_route_t {
	uint32_t wire_id;
	const char* wire_name;
	const char* source_component;
	const char* source_service;
	const char* target_component;
	const char* target_service;
	uint32_t source_pd_id;
	uint32_t target_pd_id;
	uint32_t domain_id;
	const char* default_topic;
	const char* topic_name;
	uint32_t topic_source;
	const char* partition;
	uint32_t interface_id;
	int target_legacy_port;
} ldp_dds_wire_route;

typedef struct ldp_dds_operation_route_t {
	uint32_t operation_id;
	uint32_t source_platform_id;
	uint32_t target_platform_id;
	uint32_t forward_interface_id;
	uint32_t reverse_interface_id;
} ldp_dds_operation_route;

#endif /* _LDP_DDS_ROUTE_H */
