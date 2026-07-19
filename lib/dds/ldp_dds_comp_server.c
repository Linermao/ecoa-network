#include "ldp_dds_internal.h"

#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldp_log_platform.h"
#include "ldp_comp_util.h"
#include "ldp_network.h"
#include "ldp_structures.h"
#include "ldp_VD.h"
#include "ldp_ELI.h"
#include "ldp_ELI_udp.h"
#include "ECOA_simple_types_serialization.h"

static ldp_status_t send_msg_to_father(ldp_PDomain_ctx* ctx, uint8_t op_ID)
{
	int interface_count = 0;
	net_data_w data_w = {0};

	if (ctx == NULL || ctx->interface_ctx_array == NULL) {
		return LDP_ERROR;
	}

	interface_count = ctx->nb_client + ctx->nb_server;
	if (ctx->nb_client < 0 || ctx->nb_client >= interface_count) {
		return LDP_ERROR;
	}

	return ldp_IP_write(&ctx->interface_ctx_array[ctx->nb_client],
	                    (char*)&op_ID,
	                    sizeof(op_ID),
	                    &data_w);
}

static ldp_socket_info* dds_copy_socket_info(const ldp_socket_info* source, int port_offset)
{
	ldp_socket_info* copy = NULL;

	if (source == NULL) {
		return NULL;
	}

	copy = malloc(sizeof(*copy));
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, source, sizeof(*copy));
	copy->port += port_offset;
	return copy;
}

static void destroy_component_interfaces(ldp_PDomain_ctx* ctx, int interface_count)
{
	if (ctx == NULL || ctx->interface_ctx_array == NULL) {
		return;
	}

	for (int i = 0; i < interface_count; ++i) {
		if (ctx->interface_ctx_array[i].type == LDP_LOCAL_IP &&
		    ctx->interface_ctx_array[i].inter.local.initialized) {
			ldp_destroy_interface_dds(&ctx->interface_ctx_array[i].inter.local);
		}
	}
}

static void initialize_mcast_links(ldp_interface_ctx* interface_ctx, uint32_t default_buffer_size)
{
	if (interface_ctx == NULL || interface_ctx->type != LDP_ELI_MCAST) {
		return;
	}

	interface_ctx->inter.mcast.ip_info = &interface_ctx->info_r;
	for (uint32_t i = 0; i < interface_ctx->inter.mcast.link_num; ++i) {
		ldp_PF_link_ctx* link_ctx = &interface_ctx->inter.mcast.PF_links_ctx[i].link_ctx;
		if (link_ctx->buffer_size == 0) {
			link_ctx->buffer_size = default_buffer_size;
		}
		if (link_ctx->channels == NULL && link_ctx->channel_num > 0) {
			ldp_initialized_PF_link(link_ctx);
		}
	}
}

static ldp_status_t create_component_interface(ldp_PDomain_ctx* ctx,
                                               ldp_interface_ctx* interface_ctx,
                                               int interface_index)
{
	ldp_status_t status = LDP_SUCCESS;
	ldp_socket_info* info_r = &interface_ctx->info_r;
	ldp_socket_info* info_w = interface_ctx->inter.local.info_w;

	if (info_w == NULL && interface_index < ctx->nb_client) {
		info_w = dds_copy_socket_info(&interface_ctx->info_r, LDP_DDS_REPLY_PORT_OFFSET);
		if (info_w == NULL) {
			return LDP_ERROR;
		}
		interface_ctx->inter.local.owns_info_w = true;
	} else if (info_w == NULL) {
		info_r = dds_copy_socket_info(&interface_ctx->info_r, LDP_DDS_REPLY_PORT_OFFSET);
		if (info_r == NULL) {
			return LDP_ERROR;
		}
		info_w = &interface_ctx->info_r;
		interface_ctx->inter.local.owns_info_r = true;
	}

	interface_ctx->type = LDP_LOCAL_IP;
	interface_ctx->inter.local.info_r = info_r;
	interface_ctx->inter.local.info_w = info_w;
	interface_ctx->inter.local.role = LDP_DDS_ROLE_COMPONENT;
	status = ldp_create_interface_dds(&interface_ctx->inter.local, ctx->mem_pool);
	if (status != LDP_SUCCESS) {
		ldp_destroy_interface_dds(&interface_ctx->inter.local);
	}
	return status;
}

static ldp_interface_ctx* find_interface_by_target_port(ldp_PDomain_ctx* ctx,
                                                        uint32_t target_port,
                                                        int* interface_index_out)
{
	const int interface_count = ctx->nb_client + ctx->nb_server;

	if (interface_index_out != NULL) {
		*interface_index_out = -1;
	}

	for (int i = 0; i < interface_count; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->interface_ctx_array[i];
		ldp_socket_info* receive_info = interface_ctx->inter.local.info_r != NULL
		                                ? interface_ctx->inter.local.info_r
		                                : &interface_ctx->info_r;
		if (interface_ctx->type == LDP_LOCAL_IP &&
		    receive_info->port == (int)target_port) {
			if (interface_index_out != NULL) {
				*interface_index_out = i;
			}
			return interface_ctx;
		}
	}

	for (int i = 0; i < ctx->mcast_read_interface_num; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->mcast_read_interface[i];
		if (interface_ctx->type == LDP_ELI_MCAST &&
		    interface_ctx->info_r.port == (int)target_port) {
			if (interface_index_out != NULL) {
				*interface_index_out = interface_count + i;
			}
			return interface_ctx;
		}
	}

	return NULL;
}

static ldp_interface_ctx* find_interface_by_packet(ldp_PDomain_ctx* ctx,
                                                   const LDP_DDS_Packet* packet,
                                                   int* interface_index_out)
{
	const ldp_dds_control_route* control_route = NULL;
	const ldp_dds_wire_route* wire_route = NULL;
	uint32_t target_port = 0;

	if (packet == NULL) {
		return NULL;
	}

	control_route = ldp_dds_find_control_route_by_interface(packet->interface_id);
	if (control_route != NULL) {
		target_port = (uint32_t)control_route->target_legacy_port;
	} else {
		wire_route = ldp_dds_find_wire_route_by_interface(packet->interface_id);
		if (wire_route != NULL) {
			target_port = (uint32_t)wire_route->target_legacy_port;
		} else {
			target_port = packet->target_port;
		}
	}

	ldp_interface_ctx* interface_ctx = find_interface_by_target_port(ctx,
	                                                               target_port,
	                                                               interface_index_out);
	if (interface_ctx == NULL && target_port != packet->target_port) {
		interface_ctx = find_interface_by_target_port(ctx,
		                                             packet->target_port,
		                                             interface_index_out);
	}
	return interface_ctx;
}

static char* copy_payload_for_delivery(ldp_PDomain_ctx* ctx, LDP_DDS_Packet* packet)
{
	char* delivery_buffer = NULL;

	if (packet->payload._length == 0 || packet->payload._buffer == NULL) {
		return NULL;
	}

	if (ctx->msg_buffer != NULL && packet->payload._length <= ctx->msg_buffer_size + LDP_HEADER_TCP_SIZE) {
		memcpy(ctx->msg_buffer, packet->payload._buffer, packet->payload._length);
		return ctx->msg_buffer;
	}

	delivery_buffer = malloc(packet->payload._length);
	if (delivery_buffer == NULL) {
		return NULL;
	}

	memcpy(delivery_buffer, packet->payload._buffer, packet->payload._length);
	return delivery_buffer;
}

static void release_delivery_buffer(ldp_PDomain_ctx* ctx, char* delivery_buffer)
{
	if (delivery_buffer != NULL && delivery_buffer != ctx->msg_buffer) {
		free(delivery_buffer);
	}
}

static ldp_status_t process_eli_management_msg(ldp_PDomain_ctx* ctx,
                                               ldp_interface_ctx* read_interface_ctx,
                                               ldp_ELI_header* ELI_header,
                                               unsigned char* payload)
{
	switch (ELI_header->msg_ID) {
		case LDP_ELI_PLATFORM_STATUS:
		case LDP_ELI_PLATFORM_STATUS_REQUEST:
			return LDP_SUCCESS;
		case LDP_ELI_VERSIONED_DATA_PULL:
		{
			ECOA__uint32 VD_ID = 0;
			ECOA__boolean8 push_something = ECOA__FALSE;

			if (ELI_header->payload_size < sizeof(VD_ID)) {
				return LDP_ERROR;
			}

			deserialize_ECOA__uint32(&VD_ID, payload, sizeof(VD_ID));
			ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
			                   "INFO",
			                   ctx->logger_PF,
			                   "VD data pull received. ID=%i",
			                   VD_ID);

			for (int i = 0; i < ctx->num_VD_repo; ++i) {
				push_something |= ldp_push_VD_ELI(ctx,
				                                  &ctx->VD_repo_array[i],
				                                  ELI_header->platform_ID,
				                                  VD_ID);
			}

			if (push_something == ECOA__FALSE) {
				ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
				                   "INFO",
				                   ctx->logger_PF,
				                   "Nothing to push after VD PULL ID=%i",
				                   VD_ID);
			}
			return LDP_SUCCESS;
		}
		case LDP_ELI_UNKNOWN_OPERATION:
			return LDP_SUCCESS;
		case LDP_ELI_RESERVED:
		default:
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "Platform message error: reserved ELI message ID '%i' on (%s:%i)",
			                   ELI_header->msg_ID,
			                   read_interface_ctx->info_r.addr,
			                   read_interface_ctx->info_r.port);
			return LDP_ERROR;
	}
}

static ldp_status_t deliver_eli_datagram(ldp_PDomain_ctx* ctx,
                                         ldp_interface_ctx* interface_ctx,
                                         LDP_DDS_Packet* packet)
{
	ECOA__uint32 read_bytes = 0;
	ldp_ELI_UDP_header UDP_header;
	ldp_PF_link* PF_link = NULL;
	ldp_ELI_UDP_channel* channel = NULL;
	ldp_ELI_header ELI_header;
	ldp_status_t status;

	if (packet->payload._length < LDP_ELI_UDP_HEADER_SIZE ||
	    packet->payload._buffer == NULL) {
		return LDP_ERROR;
	}

	ldp_read_ELI_UDP_header(&UDP_header,
	                        packet->payload._buffer,
	                        0,
	                        &read_bytes);
	PF_link = ldp_mcast_find_PF_link(interface_ctx, UDP_header.platform_ID);
	if (PF_link == NULL) {
		ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
		                   "INFO",
		                   ctx->logger_PF,
		                   "No PF link found. Drop DDS ELI message from UDP PF %i",
		                   UDP_header.platform_ID);
		return LDP_SUCCESS;
	}

	status = ldp_ELI_udp_msg_defragment(&PF_link->link_ctx,
	                                    &UDP_header,
	                                    &packet->payload._buffer[read_bytes],
	                                    packet->payload._length - read_bytes,
	                                    &channel);
	if (status == ELI_STATUS__INCOMPLETE_MSG) {
		return LDP_SUCCESS;
	}
	if (status != ELI_STATUS__OK || channel == NULL) {
		return LDP_ERROR;
	}

	status = ldp_read_ELI_header(&ELI_header, channel->buffer, 0, &read_bytes);
	if (status != ELI_STATUS__OK) {
		channel->is_used = false;
		return LDP_ERROR;
	}

	if (ELI_header.payload_size != channel->offset - read_bytes) {
		ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
		                   "ERROR",
		                   ctx->logger_PF,
		                   "DDS ELI message error: payload size mismatch (%i/%i)(%s:%i)",
		                   ELI_header.payload_size,
		                   channel->offset - read_bytes,
		                   interface_ctx->info_r.addr,
		                   interface_ctx->info_r.port);
		channel->is_used = false;
		return LDP_ERROR;
	}

	if (ELI_header.domain == LDP_ELI_PLATFORM_MNG) {
		status = process_eli_management_msg(ctx,
		                                    interface_ctx,
		                                    &ELI_header,
		                                    &channel->buffer[read_bytes]);
		channel->is_used = false;
		return status;
	}

	if (ELI_header.domain == LDP_ELI_SERVICE_OP) {
		ctx->route_function_ptr(ctx,
		                        ELI_header.msg_ID,
		                        (char*)&channel->buffer[read_bytes],
		                        ELI_header.payload_size,
		                        PF_link->sender_interface,
		                        interface_ctx->info_r.port,
		                        ELI_header.sequence_num,
		                        UDP_header.platform_ID);
		channel->is_used = false;
		return LDP_SUCCESS;
	}

	ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
	                   "ERROR",
	                   ctx->logger_PF,
	                   "DDS ELI domain is neither platform management nor service operation: '%i' on (%s:%i)",
	                   ELI_header.domain,
	                   interface_ctx->info_r.addr,
	                   interface_ctx->info_r.port);
	channel->is_used = false;
	return LDP_ERROR;
}

static ldp_status_t deliver_packet(ldp_PDomain_ctx* ctx, LDP_DDS_Packet* packet)
{
	ldp_status_t status = LDP_SUCCESS;
	int target_index = -1;
	ldp_interface_ctx* target_interface = find_interface_by_packet(ctx,
	                                                               packet,
	                                                               &target_index);
	char* delivery_buffer = NULL;
	uint32_t op_ID = 0;
	uint32_t param_size = 0;
	bool header_is_valid = false;

	ldp_dds_trace_packet("take.component", ctx->name, target_index, packet);

	if (target_interface == NULL) {
		ldp_log_PF_log_var(ECOA_LOG_WARN_PF,
		                   "WARNING",
		                   ctx->logger_PF,
		                   "[DDS] drop packet for unknown target port %" PRIu32,
		                   packet->target_port);
		return LDP_SUCCESS;
	}

	if (target_interface->type == LDP_ELI_MCAST) {
		status = deliver_eli_datagram(ctx, target_interface, packet);
		if (status == LDP_SUCCESS) {
			ldp_dds_trace_packet("route.mcast.component", ctx->name, target_index, packet);
		}
		return status;
	}

	delivery_buffer = copy_payload_for_delivery(ctx, packet);
	if (delivery_buffer == NULL) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] drop packet with empty payload or allocation failure");
		return LDP_SUCCESS;
	}

	if (packet->payload._length < LDP_HEADER_TCP_SIZE) {
		ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
		                   "ERROR",
		                   ctx->logger_PF,
		                   "[DDS] drop packet for target port %" PRIu32 " with payload shorter than LDP header",
		                   packet->target_port);
		release_delivery_buffer(ctx, delivery_buffer);
		return LDP_SUCCESS;
	}

	header_is_valid = ldp_read_IP_header(ctx, delivery_buffer, &op_ID, &param_size);
	if (!header_is_valid) {
		release_delivery_buffer(ctx, delivery_buffer);
		return LDP_ERROR;
	}

	status = ldp_dds_deliver_to_component(ctx,
	                                      target_interface,
	                                      delivery_buffer,
	                                      packet->payload._length);
	if (status == LDP_SUCCESS) {
		ldp_dds_trace_packet("route.component", ctx->name, target_index, packet);
	}

	if (status == LDP_SUCCESS &&
	    op_ID == LDP_ID_INIT_MOD &&
	    ctx->state == PDomain_INIT &&
	    all_module_ready(ctx)) {
		if (send_msg_to_father(ctx, LDP_ID_CLIENT_READY) == LDP_SUCCESS) {
			ctx->state = PDomain_READY;
		} else {
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "[%s] Cannot send CLIENT_READY to father process",
			                   ctx->name);
		}
	}

	release_delivery_buffer(ctx, delivery_buffer);
	return status;
}

static ldp_status_t run_dds_component_server(ldp_PDomain_ctx* ctx, ldp_dds_runtime* runtime)
{
	while (true) {
		bool took_sample = false;

		for (size_t endpoint_i = 0; endpoint_i < runtime->endpoint_count; ++endpoint_i) {
			ldp_dds_topic_endpoint* endpoint = &runtime->endpoints[endpoint_i];
			void* samples[8] = { NULL };
			dds_sample_info_t infos[8];
			memset(infos, 0, sizeof(infos));

			dds_return_t ret = dds_take(endpoint->reader, samples, infos, 8, 8);
			if (ret < 0) {
				fprintf(stderr, "[DDS] dds_take failed: %s\n", dds_strretcode(-ret));
				return LDP_ERROR;
			}

			for (int i = 0; i < ret; ++i) {
				if (infos[i].valid_data &&
				    ldp_dds_packet_targets_runtime(runtime,
				                                   (LDP_DDS_Packet*)samples[i])) {
					ldp_status_t status = deliver_packet(ctx, (LDP_DDS_Packet*)samples[i]);
					if (status == LDP_ERROR) {
						dds_return_loan(endpoint->reader, samples, ret);
						return LDP_ERROR;
					}
				}
			}

			if (ret > 0) {
				took_sample = true;
				dds_return_loan(endpoint->reader, samples, ret);
			}
		}

		if (!took_sample) {
			dds_sleepfor(DDS_MSECS(10));
		}
	}
}

void ldp_start_comp_server(ldp_PDomain_ctx* ctx)
{
	int interface_count = 0;
	int initialized_count = 0;
	ldp_dds_runtime* runtime = NULL;

	if (ctx == NULL) {
		return;
	}

	interface_count = ctx->nb_client + ctx->nb_server;
	if (ctx->interface_ctx_array == NULL || interface_count <= 0) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] component server has no local interfaces to listen on");
		return;
	}

	/*
	 * LDP generated PDs use interface_ctx_array[nb_client] as the link to
	 * the main platform. The DDS transport relies on the same convention for
	 * CLIENT_INIT, CLIENT_READY, KILL and fault notifications.
	 */
	if (ctx->nb_client < 0 || ctx->nb_client >= interface_count) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] component server has no main platform control interface");
		return;
	}

	for (int i = 0; i < interface_count; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->interface_ctx_array[i];
		if (interface_ctx->type == LDP_ELI_MCAST) {
			interface_ctx->inter.mcast.ip_info = &interface_ctx->info_r;
			continue;
		}

		if (create_component_interface(ctx, interface_ctx, i) != LDP_SUCCESS) {
			ldp_log_PF_log(ECOA_LOG_ERROR_PF,
			               "ERROR",
			               ctx->logger_PF,
			               "[DDS] cannot initialize component DDS interface");
			destroy_component_interfaces(ctx, initialized_count);
			return;
		}
		initialized_count++;

		if (runtime == NULL) {
			runtime = ldp_dds_get_runtime(&interface_ctx->inter.local);
		}
	}

	if (runtime == NULL) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] component server has no DDS runtime");
		destroy_component_interfaces(ctx, initialized_count);
		return;
	}

	{
		const ldp_dds_entity_route* entity_route = ldp_dds_find_entity_route_by_name(ctx->name);
		if (entity_route == NULL ||
		    ldp_dds_runtime_add_target_entity(runtime, entity_route->id) != LDP_SUCCESS) {
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "[DDS] cannot register PD entity '%s'",
			                   ctx->name);
			destroy_component_interfaces(ctx, initialized_count);
			return;
		}
	}

	for (int i = 0; i < ctx->mcast_read_interface_num; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->mcast_read_interface[i];
		interface_ctx->type = LDP_ELI_MCAST;
		initialize_mcast_links(interface_ctx,
		                       ctx->msg_buffer_size + LDP_ELI_UDP_HEADER_SIZE + LDP_ELI_HEADER_SIZE);
		if (interface_ctx->info_r.port > 0 &&
		    ldp_dds_runtime_add_target_port(runtime, (uint32_t)interface_ctx->info_r.port) != LDP_SUCCESS) {
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "[DDS] cannot register ELI multicast read port %i",
			                   interface_ctx->info_r.port);
			destroy_component_interfaces(ctx, initialized_count);
			return;
		}
	}

	ctx->state = PDomain_IDLE;
	if (send_msg_to_father(ctx, LDP_ID_CLIENT_INIT) == LDP_SUCCESS) {
		ctx->state = PDomain_INIT;
	} else {
		ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
		                   "ERROR",
		                   ctx->logger_PF,
		                   "[%s] Cannot send CLIENT_INIT to father process",
		                   ctx->name);
	}

	run_dds_component_server(ctx, runtime);

	destroy_component_interfaces(ctx, interface_count);
}
