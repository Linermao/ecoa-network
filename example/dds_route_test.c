#include <assert.h>
#include <string.h>

#include "ldp_dds.h"
#include "route.h"

int main(void)
{
	const ldp_dds_wire_route* route = NULL;

	assert(LDP_DDS_DOMAIN_ID == 0);

	route = ldp_dds_find_wire_route_by_operation(OP_DATA, 1);
	assert(route != NULL);
	assert(route->interface_id == LDP_DDS_IFACE_SENDER_TO_RECEIVER);
	assert(strcmp(route->topic_name, LDP_DDS_TOPIC_MISSION_SENDER_RECEIVER) == 0);

	route = ldp_dds_find_wire_route_by_operation(OP_DATA, 2);
	assert(route != NULL);
	assert(route->interface_id == LDP_DDS_IFACE_RECEIVER_TO_SENDER);

	route = ldp_dds_find_wire_route_by_operation(OP_ACK, 2);
	assert(route != NULL);
	assert(route->interface_id == LDP_DDS_IFACE_RECEIVER_TO_SENDER);
	assert(strcmp(route->topic_name, LDP_DDS_TOPIC_MISSION_SENDER_RECEIVER) == 0);

	assert(ldp_dds_find_wire_route_by_operation(OP_ACK, 99) == NULL);
	return 0;
}
