#ifndef _LDP_DDS_INTERNAL_H
#define _LDP_DDS_INTERNAL_H

#include "ldp_dds.h"

#define LDP_DDS_DATA_TOPIC "ldp.local.peer_data"
#define LDP_DDS_CONTROL_TOPIC "ldp.local.control"

struct ldp_dds_backend {
	void* reserved;
};

#endif /* _LDP_DDS_INTERNAL_H */
