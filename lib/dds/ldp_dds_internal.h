#ifndef _LDP_DDS_INTERNAL_H
#define _LDP_DDS_INTERNAL_H

#include "ldp_dds.h"
#include "ldp_dds_route.h"
#include "ldp_dds_packet.h"
#include <dds/dds.h>

#if defined(__has_include)
# if __has_include("route.h")
#  include "route.h"
#  define LDP_DDS_HAS_GENERATED_ROUTE 1
# endif
#endif

#ifndef LDP_DDS_HAS_GENERATED_ROUTE
# define LDP_DDS_HAS_GENERATED_ROUTE 0
# define LDP_DDS_DOMAIN_ID DDS_DOMAIN_DEFAULT
# define LDP_DDS_CONTROL_TOPIC "LdpLocalControl"
# define LDP_DDS_DEFAULT_TOPIC "LdpLocalPeerData"
# define LDP_DDS_PD_MAIN 0
# define LDP_DDS_ENTITY_ROUTE_COUNT 0
# define LDP_DDS_CONTROL_ROUTE_COUNT 0
# define LDP_DDS_WIRE_ROUTE_COUNT 0
# define LDP_DDS_OPERATION_ROUTE_COUNT 0
static const ldp_dds_entity_route* const LDP_DDS_ENTITY_ROUTES = NULL;
static const ldp_dds_control_route* const LDP_DDS_CONTROL_ROUTES = NULL;
static const ldp_dds_wire_route* const LDP_DDS_WIRE_ROUTES = NULL;
static const ldp_dds_operation_route* const LDP_DDS_OPERATION_ROUTES = NULL;
#endif

#define LDP_DDS_FALLBACK_DATA_TOPIC "LdpLocalPeerData"
#define LDP_DDS_MAX_TARGET_PORTS 64
#define LDP_DDS_MAX_TARGET_ENTITIES 64
#define LDP_DDS_MAX_TOPIC_ENDPOINTS \
	(1U + LDP_DDS_CONTROL_ROUTE_COUNT + LDP_DDS_WIRE_ROUTE_COUNT)
#define LDP_DDS_REPLY_PORT_OFFSET 5000

typedef struct ldp_dds_topic_endpoint {
	const char* name;
	dds_entity_t topic;
	dds_entity_t writer;
	dds_entity_t reader;
} ldp_dds_topic_endpoint;

typedef struct ldp_dds_runtime {
	dds_entity_t participant;
	ldp_dds_topic_endpoint endpoints[LDP_DDS_MAX_TOPIC_ENDPOINTS];
	size_t endpoint_count;
	uint32_t target_ports[LDP_DDS_MAX_TARGET_PORTS];
	size_t target_port_count;
	uint32_t target_entities[LDP_DDS_MAX_TARGET_ENTITIES];
	size_t target_entity_count;
	unsigned int ref_count;
} ldp_dds_runtime;

struct ldp_dds_backend {
	ldp_dds_runtime* runtime;
};

ldp_dds_runtime* ldp_dds_get_runtime(ldp_interface_dds* interface);
ldp_status_t ldp_dds_runtime_add_target_port(ldp_dds_runtime* runtime, uint32_t port);
ldp_status_t ldp_dds_runtime_add_target_entity(ldp_dds_runtime* runtime, uint32_t entity_id);
bool ldp_dds_packet_targets_runtime(const ldp_dds_runtime* runtime,
                                    const LDP_DDS_Packet* packet);
const ldp_dds_entity_route* ldp_dds_find_entity_route_by_name(const char* name);
const ldp_dds_control_route* ldp_dds_find_control_route_by_ports(uint32_t source_port, uint32_t target_port);
const ldp_dds_control_route* ldp_dds_find_control_route_by_interface(uint32_t interface_id);
const ldp_dds_wire_route* ldp_dds_find_wire_route_by_target_port(uint32_t target_port);
const ldp_dds_wire_route* ldp_dds_find_wire_route_by_interface(uint32_t interface_id);
const ldp_dds_wire_route* ldp_dds_find_wire_route_by_operation(uint32_t operation_id,
                                                               uint32_t source_platform_id);
bool ldp_dds_trace_enabled(void);
uint32_t ldp_dds_trace_op_id(const LDP_DDS_Packet* packet);
const char* ldp_dds_trace_kind_name(LDP_DDS_PacketKind kind);
void ldp_dds_trace_packet(const char* stage,
                          const char* owner,
                          int interface_index,
                          const LDP_DDS_Packet* packet);

#endif /* _LDP_DDS_INTERNAL_H */
