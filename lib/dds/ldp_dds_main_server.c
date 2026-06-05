#include "ldp_dds_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"
#include "ldp_ELI.h"
#include "ldp_ELI_udp.h"
#include "ECOA_simple_types_serialization.h"

static ldp_interface_ctx* find_main_interface_by_target_port(ldp_Main_ctx* ctx,
                                                             ldp_interface_ctx* interface_ctx_array,
                                                             uint32_t target_port,
                                                             int* interface_index_out)
{
	if (interface_index_out != NULL) {
		*interface_index_out = -1;
	}

	for (int i = 0; i < ctx->PD_number; ++i) {
		ldp_interface_ctx* interface_ctx = &interface_ctx_array[i];
		if (interface_ctx->type == LDP_LOCAL_IP &&
		    interface_ctx->info_r.port == (int)target_port) {
			if (interface_index_out != NULL) {
				*interface_index_out = i;
			}
			return interface_ctx;
		}
	}

	for (uint32_t i = 0; i < ctx->mcast_reader_interface_num; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->mcast_reader_interface[i];
		if (interface_ctx->type == LDP_ELI_MCAST &&
		    interface_ctx->info_r.port == (int)target_port) {
			if (interface_index_out != NULL) {
				*interface_index_out = ctx->PD_number + (int)i;
			}
			return interface_ctx;
		}
	}

	return NULL;
}

static ldp_platform_info* find_connected_platform(ldp_Main_ctx* ctx, uint32_t platform_ID)
{
	if (ctx == NULL) {
		return NULL;
	}

	for (uint32_t i = 0; i < ctx->connected_platform_num; ++i) {
		if (ctx->connected_platforms[i].ELI_platform_ID == platform_ID) {
			return &ctx->connected_platforms[i];
		}
	}
	return NULL;
}

static void send_platform_message(ldp_Main_ctx* ctx,
                                  ldp_platform_info* connected_PF,
                                  uint32_t message_id,
                                  uint32_t field,
                                  bool has_field)
{
	unsigned char buffer[LDP_ELI_UDP_HEADER_SIZE + LDP_ELI_HEADER_SIZE + sizeof(uint32_t)] = {0};
	uint64_t msg_size = LDP_ELI_UDP_HEADER_SIZE + LDP_ELI_HEADER_SIZE;

	if (ctx == NULL || connected_PF == NULL || connected_PF->sending_interface == NULL) {
		return;
	}

	if (has_field) {
		msg_size += sizeof(uint32_t);
	}

	write_ELI_UDP_platform_message(ctx->ELI_platform_ID,
	                               connected_PF->sending_interface->inter.mcast.UDP_current_PF_ID,
	                               0,
	                               buffer,
	                               message_id,
	                               field,
	                               has_field);
	ldp_mcast_send(&connected_PF->sending_interface->inter.mcast,
	              (char*)buffer,
	              &msg_size,
	              ctx->logger_PF);
}

static ldp_status_t process_main_eli_management_msg(ldp_Main_ctx* ctx,
                                                    ldp_interface_ctx* read_interface_ctx,
                                                    ldp_ELI_header* ELI_header,
                                                    char* payload)
{
	switch (ELI_header->msg_ID) {
		case LDP_ELI_PLATFORM_STATUS:
		{
			ldp_platform_state new_state = ELI_PF_UNKNOWN;
			ldp_platform_info* connected_PF = NULL;

			if (ELI_header->payload_size < sizeof(ECOA__uint32)) {
				ldp_log_PF_log(ECOA_LOG_ERROR_PF,
				               "ERROR",
				               ctx->logger_PF,
				               "[MAIN] Received PLATFORM_STATUS: message too small");
				return LDP_ERROR;
			}

			deserialize_ECOA__uint32(&new_state, payload, sizeof(ECOA__uint32));
			connected_PF = find_connected_platform(ctx, ELI_header->platform_ID);
			if (connected_PF == NULL) {
				ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
				                   "INFO",
				                   ctx->logger_PF,
				                   "[MAIN] Platform ID '%i' is not connected",
				                   ELI_header->platform_ID);
				return LDP_SUCCESS;
			}

			if (connected_PF->state == ELI_PF_DOWN && new_state == ELI_PF_UP) {
				connected_PF->state = new_state;
				ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
				                   "INFO",
				                   ctx->logger_PF,
				                   "[MAIN] Connection of Platform ID '%i'",
				                   ELI_header->platform_ID);
				send_platform_message(ctx, connected_PF, LDP_ELI_PLATFORM_STATUS, ELI_PF_UP, true);
				send_platform_message(ctx, connected_PF, LDP_ELI_VERSIONED_DATA_PULL, 0xFFFFFFFFU, true);
			} else if (connected_PF->state == ELI_PF_UP && new_state == ELI_PF_DOWN) {
				connected_PF->state = new_state;
				ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
				                   "INFO",
				                   ctx->logger_PF,
				                   "[MAIN] Platform ID '%i' is unconnected",
				                   ELI_header->platform_ID);
			} else if (connected_PF->state != new_state) {
				ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
				                   "ERROR",
				                   ctx->logger_PF,
				                   "[MAIN] Received an invalid state from PF '%i'",
				                   ELI_header->platform_ID);
				return LDP_ERROR;
			}
			return LDP_SUCCESS;
		}
		case LDP_ELI_PLATFORM_STATUS_REQUEST:
		{
			ldp_platform_info* connected_PF = find_connected_platform(ctx, ELI_header->platform_ID);
			if (connected_PF == NULL) {
				ldp_log_PF_log_var(ECOA_LOG_INFO_PF,
				                   "INFO",
				                   ctx->logger_PF,
				                   "[MAIN] Platform ID '%i' is not connected",
				                   ELI_header->platform_ID);
			} else {
				send_platform_message(ctx, connected_PF, LDP_ELI_PLATFORM_STATUS, ELI_PF_UP, true);
			}
			return LDP_SUCCESS;
		}
		case LDP_ELI_VERSIONED_DATA_PULL:
			return LDP_SUCCESS;
		case LDP_ELI_UNKNOWN_OPERATION:
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "[MAIN] Platform error with an unknown operation (%s:%i)",
			                   read_interface_ctx->info_r.addr,
			                   read_interface_ctx->info_r.port);
			return LDP_ERROR;
		case LDP_ELI_RESERVED:
		default:
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "[MAIN] Platform message error: reserved message ID '%i' on (%s:%i)",
			                   ELI_header->msg_ID,
			                   read_interface_ctx->info_r.addr,
			                   read_interface_ctx->info_r.port);
			return LDP_ERROR;
	}
}

static ldp_status_t deliver_main_eli_datagram(ldp_Main_ctx* ctx,
                                              ldp_interface_ctx* interface_ctx,
                                              LDP_DDS_Packet* packet)
{
	ECOA__uint32 read_bytes = 0;
	ldp_ELI_UDP_header UDP_header;
	ldp_ELI_header ELI_header;
	ldp_status_t status;

	if (packet->payload._length < LDP_ELI_UDP_HEADER_SIZE + LDP_ELI_HEADER_SIZE ||
	    packet->payload._buffer == NULL) {
		return LDP_ERROR;
	}

	ldp_read_ELI_UDP_header(&UDP_header,
	                        packet->payload._buffer,
	                        0,
	                        &read_bytes);
	if (UDP_header.msg_part != LDP_ELI_FULL) {
		return LDP_SUCCESS;
	}

	status = ldp_read_ELI_header(&ELI_header,
	                             packet->payload._buffer,
	                             LDP_ELI_UDP_HEADER_SIZE,
	                             &read_bytes);
	if (status != ELI_STATUS__OK) {
		return LDP_ERROR;
	}
	read_bytes += LDP_ELI_UDP_HEADER_SIZE;

	if (ELI_header.domain == LDP_ELI_PLATFORM_MNG) {
		return process_main_eli_management_msg(ctx,
		                                       interface_ctx,
		                                       &ELI_header,
		                                       (char*)&packet->payload._buffer[read_bytes]);
	}

	if (ELI_header.domain == LDP_ELI_SERVICE_OP) {
		return LDP_SUCCESS;
	}

	ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
	                   "ERROR",
	                   ctx->logger_PF,
	                   "[MAIN] DDS ELI domain is neither platform management nor service operation: '%i' on (%s:%i)",
	                   ELI_header.domain,
	                   interface_ctx->info_r.addr,
	                   interface_ctx->info_r.port);
	return LDP_ERROR;
}

static char* copy_main_payload(LDP_DDS_Packet* packet)
{
	char* delivery_buffer = NULL;

	if (packet->payload._length == 0 || packet->payload._buffer == NULL) {
		return NULL;
	}

	delivery_buffer = malloc(packet->payload._length);
	if (delivery_buffer == NULL) {
		return NULL;
	}

	memcpy(delivery_buffer, packet->payload._buffer, packet->payload._length);
	return delivery_buffer;
}

static ldp_status_t deliver_main_packet(ldp_Main_ctx* ctx,
                                        ldp_interface_ctx* interface_ctx_array,
                                        LDP_DDS_Packet* packet)
{
	ldp_status_t status = LDP_SUCCESS;
	int target_index = -1;
	ldp_interface_ctx* target_interface = find_main_interface_by_target_port(ctx,
	                                                                         interface_ctx_array,
	                                                                         packet->target_port,
	                                                                         &target_index);
	char* delivery_buffer = NULL;

	ldp_dds_trace_packet("take.main", "main", target_index, packet);

	if (target_interface == NULL) {
		ldp_log_PF_log_var(ECOA_LOG_WARN_PF,
		                   "WARNING",
		                   ctx->logger_PF,
		                   "[DDS] main drop packet for unknown target port %" PRIu32,
		                   packet->target_port);
		return LDP_SUCCESS;
	}

	if (target_interface->type == LDP_ELI_MCAST) {
		status = deliver_main_eli_datagram(ctx, target_interface, packet);
		if (status == LDP_SUCCESS) {
			ldp_dds_trace_packet("route.mcast.main", "main", target_index, packet);
		}
		return status;
	}

	delivery_buffer = copy_main_payload(packet);
	if (delivery_buffer == NULL) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] main drop packet with empty payload or allocation failure");
		return LDP_SUCCESS;
	}

	status = ldp_dds_deliver_to_main(ctx,
	                                 delivery_buffer,
	                                 packet->payload._length,
	                                 target_interface,
	                                 interface_ctx_array);
	if (status == LDP_SUCCESS) {
		ldp_dds_trace_packet("route.main", "main", target_index, packet);
	}
	free(delivery_buffer);
	return status;
}

static ldp_status_t run_dds_main_server(ldp_Main_ctx* ctx,
                                        ldp_interface_ctx* interface_ctx_array,
                                        ldp_dds_runtime* runtime)
{
	while (true) {
		void* samples[8] = { NULL };
		dds_sample_info_t infos[8];
		memset(infos, 0, sizeof(infos));

		dds_return_t ret = dds_take(runtime->data_reader, samples, infos, 8, 8);
		if (ret < 0) {
			fprintf(stderr, "[DDS] main dds_take failed: %s\n", dds_strretcode(-ret));
			return LDP_ERROR;
		}

		for (int i = 0; i < ret; ++i) {
			if (infos[i].valid_data) {
				ldp_status_t status = deliver_main_packet(ctx,
				                                          interface_ctx_array,
				                                          (LDP_DDS_Packet*)samples[i]);
				if (status == LDP_ERROR) {
					dds_return_loan(runtime->data_reader, samples, ret);
					return LDP_ERROR;
				}
			}
		}

		if (ret > 0) {
			dds_return_loan(runtime->data_reader, samples, ret);
		} else {
			dds_sleepfor(DDS_MSECS(10));
		}
	}
}

static ldp_status_t create_main_interface(ldp_Main_ctx* ctx,
                                          ldp_interface_ctx* interface_ctx)
{
	interface_ctx->type = LDP_LOCAL_IP;
	interface_ctx->info_s = interface_ctx->info_r;
	interface_ctx->info_s.port += LDP_DDS_REPLY_PORT_OFFSET;
	interface_ctx->inter.local.info_r = &interface_ctx->info_r;
	interface_ctx->inter.local.info_w = &interface_ctx->info_s;
	interface_ctx->inter.local.role = LDP_DDS_ROLE_MAIN;
	return ldp_create_interface_dds(&interface_ctx->inter.local, ctx->mem_pool);
}

static void destroy_main_interfaces(ldp_interface_ctx* interface_ctx_array, int interface_count)
{
	if (interface_ctx_array == NULL) {
		return;
	}

	for (int i = 0; i < interface_count; ++i) {
		if (interface_ctx_array[i].type == LDP_LOCAL_IP &&
		    interface_ctx_array[i].inter.local.initialized) {
			ldp_destroy_interface_dds(&interface_ctx_array[i].inter.local);
		}
	}
}

void ldp_start_father_server(ldp_Main_ctx* ctx,
                             ldp_interface_ctx* interface_ctx_array,
                             uint32_t PF_links_num)
{
	UNUSED(PF_links_num);
	ldp_dds_runtime* runtime = NULL;
	int initialized_count = 0;

	if (ctx == NULL || interface_ctx_array == NULL) {
		return;
	}

	ctx->interface_ctx_array = interface_ctx_array;

	for (int i = 0; i < ctx->PD_number; ++i) {
		ldp_interface_ctx* interface_ctx = &interface_ctx_array[i];

		if (create_main_interface(ctx, interface_ctx) != LDP_SUCCESS) {
			ldp_log_PF_log(ECOA_LOG_ERROR_PF,
			               "ERROR",
			               ctx->logger_PF,
			               "[DDS] cannot initialize main DDS interface");
			destroy_main_interfaces(interface_ctx_array, initialized_count);
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
		               "[DDS] main server has no DDS runtime");
		destroy_main_interfaces(interface_ctx_array, initialized_count);
		return;
	}

	for (uint32_t i = 0; i < ctx->mcast_reader_interface_num; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->mcast_reader_interface[i];
		interface_ctx->type = LDP_ELI_MCAST;
		interface_ctx->inter.mcast.ip_info = &interface_ctx->info_r;
		if (ldp_dds_runtime_add_target_port(runtime, (uint32_t)interface_ctx->info_r.port) != LDP_SUCCESS) {
			ldp_log_PF_log_var(ECOA_LOG_ERROR_PF,
			                   "ERROR",
			                   ctx->logger_PF,
			                   "[DDS] cannot register main ELI multicast read port %i",
			                   interface_ctx->info_r.port);
			destroy_main_interfaces(interface_ctx_array, initialized_count);
			return;
		}
	}

	for (uint32_t i = 0; i < ctx->mcast_sender_interface_num; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->mcast_sender_interface[i];
		interface_ctx->type = LDP_ELI_MCAST;
		interface_ctx->inter.mcast.ip_info = &interface_ctx->info_r;
	}

	if (runtime != NULL) {
		run_dds_main_server(ctx, interface_ctx_array, runtime);
	}

	destroy_main_interfaces(interface_ctx_array, ctx->PD_number);
}
