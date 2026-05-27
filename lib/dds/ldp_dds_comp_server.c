#include "ldp_dds_internal.h"

#include <stddef.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"

static ldp_interface_ctx* find_interface_by_target_port(ldp_PDomain_ctx* ctx, uint32_t target_port)
{
	const int interface_count = ctx->nb_client + ctx->nb_server;

	for (int i = 0; i < interface_count; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->interface_ctx_array[i];
		if (interface_ctx->type == LDP_LOCAL_IP &&
		    interface_ctx->info_r.port == (int)target_port) {
			return interface_ctx;
		}
	}

	return NULL;
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

static ldp_status_t deliver_packet(ldp_PDomain_ctx* ctx, LDP_DDS_Packet* packet)
{
	ldp_status_t status = LDP_SUCCESS;
	ldp_interface_ctx* target_interface = find_interface_by_target_port(ctx, packet->target_port);
	char* delivery_buffer = NULL;

	if (target_interface == NULL) {
		ldp_log_PF_log_var(ECOA_LOG_WARN_PF,
		                   "WARNING",
		                   ctx->logger_PF,
		                   "[DDS] drop packet for unknown target port %" PRIu32,
		                   packet->target_port);
		return LDP_SUCCESS;
	}

	delivery_buffer = copy_payload_for_delivery(ctx, packet);
	if (delivery_buffer == NULL) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] drop packet with empty payload or allocation failure");
		return LDP_SUCCESS;
	}

	status = ldp_dds_deliver_to_component(ctx,
	                                      target_interface,
	                                      delivery_buffer,
	                                      packet->payload._length);
	release_delivery_buffer(ctx, delivery_buffer);
	return status;
}

static ldp_status_t run_dds_component_server(ldp_PDomain_ctx* ctx, ldp_dds_runtime* runtime)
{
	while (true) {
		void* samples[8] = { NULL };
		dds_sample_info_t infos[8];
		memset(infos, 0, sizeof(infos));

		dds_return_t ret = dds_take(runtime->data_reader, samples, infos, 8, 8);
		if (ret < 0) {
			fprintf(stderr, "[DDS] dds_take failed: %s\n", dds_strretcode(-ret));
			return LDP_ERROR;
		}

		for (int i = 0; i < ret; ++i) {
			if (infos[i].valid_data) {
				ldp_status_t status = deliver_packet(ctx, (LDP_DDS_Packet*)samples[i]);
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

void ldp_start_comp_server(ldp_PDomain_ctx* ctx)
{
	int interface_count = 0;
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

	for (int i = 0; i < interface_count; ++i) {
		ldp_interface_ctx* interface_ctx = &ctx->interface_ctx_array[i];
		ldp_socket_info* info_w = interface_ctx->inter.local.info_w;
		if (interface_ctx->type == LDP_ELI_MCAST) {
			continue;
		}

		if (info_w == NULL) {
			info_w = &interface_ctx->info_r;
		}

		if (ldp_dds_prepare_interface_ctx(interface_ctx,
		                                  &interface_ctx->info_r,
		                                  info_w,
		                                  LDP_DDS_ROLE_COMPONENT,
		                                  ctx->mem_pool) != LDP_SUCCESS) {
			ldp_log_PF_log(ECOA_LOG_ERROR_PF,
			               "ERROR",
			               ctx->logger_PF,
			               "[DDS] cannot initialize component DDS interface");
			return;
		}

		if (runtime == NULL) {
			runtime = ldp_dds_get_runtime(&interface_ctx->inter.local);
		}
	}

	if (runtime == NULL) {
		ldp_log_PF_log(ECOA_LOG_ERROR_PF,
		               "ERROR",
		               ctx->logger_PF,
		               "[DDS] component server has no DDS runtime");
		return;
	}

	ctx->state = PDomain_IDLE;
	run_dds_component_server(ctx, runtime);

	for (int i = 0; i < interface_count; ++i) {
		if (ctx->interface_ctx_array[i].type == LDP_LOCAL_IP) {
			ldp_destroy_interface_dds(&ctx->interface_ctx_array[i].inter.local);
		}
	}
}
