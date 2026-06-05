#ifndef _LDP_DDS_INTERNAL_H
#define _LDP_DDS_INTERNAL_H

#include "ldp_dds.h"
#include "ldp_dds_packet.h"
#include <dds/dds.h>

#define LDP_DDS_DATA_TOPIC "LdpLocalPeerData"
#define LDP_DDS_CONTROL_TOPIC "LdpLocalControl"
#define LDP_DDS_MAX_TARGET_PORTS 64
#define LDP_DDS_REPLY_PORT_OFFSET 5000

typedef struct ldp_dds_runtime {
	dds_entity_t participant;
	dds_entity_t data_write_topic;
	dds_entity_t data_read_topic;
	dds_entity_t data_writer;
	dds_entity_t data_reader;
	uint32_t target_ports[LDP_DDS_MAX_TARGET_PORTS];
	size_t target_port_count;
	unsigned int ref_count;
} ldp_dds_runtime;

struct ldp_dds_backend {
	ldp_dds_runtime* runtime;
};

ldp_dds_runtime* ldp_dds_get_runtime(ldp_interface_dds* interface);
ldp_status_t ldp_dds_runtime_add_target_port(ldp_dds_runtime* runtime, uint32_t port);
bool ldp_dds_trace_enabled(void);
uint32_t ldp_dds_trace_op_id(const LDP_DDS_Packet* packet);
const char* ldp_dds_trace_kind_name(LDP_DDS_PacketKind kind);
void ldp_dds_trace_packet(const char* stage,
                          const char* owner,
                          int interface_index,
                          const LDP_DDS_Packet* packet);

#endif /* _LDP_DDS_INTERNAL_H */
