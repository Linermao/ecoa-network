#ifndef _LDP_DDS_INTERNAL_H
#define _LDP_DDS_INTERNAL_H

#include "ldp_dds.h"
#include "ldp_dds_packet.h"
#include <dds/dds.h>

#define LDP_DDS_DATA_TOPIC "LdpLocalPeerData"
#define LDP_DDS_CONTROL_TOPIC "LdpLocalControl"
#define LDP_DDS_MAX_TARGET_PORTS 64

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

ldp_status_t ldp_dds_prepare_interface_ctx(ldp_interface_ctx* interface_ctx,
                                           ldp_socket_info* info_r,
                                           ldp_socket_info* info_w,
                                           ldp_dds_role role,
                                           apr_pool_t* mp);

ldp_dds_runtime* ldp_dds_get_runtime(ldp_interface_dds* interface);

#endif /* _LDP_DDS_INTERNAL_H */
