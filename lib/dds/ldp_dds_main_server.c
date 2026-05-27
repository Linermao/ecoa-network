#include "ldp_dds_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"

static ldp_interface_ctx* find_main_interface_by_target_port(ldp_Main_ctx* ctx,
                                                             ldp_interface_ctx* interface_ctx_array,
                                                             uint32_t target_port)
{
	for (int i = 0; i < ctx->PD_number; ++i) {
		ldp_interface_ctx* interface_ctx = &interface_ctx_array[i];
		if (interface_ctx->type == LDP_LOCAL_IP &&
		    interface_ctx->info_r.port == (int)target_port) {
			return interface_ctx;
		}
	}

	return NULL;
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
	ldp_interface_ctx* target_interface = find_main_interface_by_target_port(ctx,
	                                                                         interface_ctx_array,
	                                                                         packet->target_port);
	char* delivery_buffer = NULL;

	if (target_interface == NULL) {
		ldp_log_PF_log_var(ECOA_LOG_WARN_PF,
		                   "WARNING",
		                   ctx->logger_PF,
		                   "[DDS] main drop packet for unknown target port %" PRIu32,
		                   packet->target_port);
		return LDP_SUCCESS;
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

void ldp_start_father_server(ldp_Main_ctx* ctx,
                             ldp_interface_ctx* interface_ctx_array,
                             uint32_t PF_links_num)
{
	UNUSED(PF_links_num);
	ldp_dds_runtime* runtime = NULL;

	if (ctx == NULL || interface_ctx_array == NULL) {
		return;
	}

	ctx->interface_ctx_array = interface_ctx_array;

	for (int i = 0; i < ctx->PD_number; ++i) {
		ldp_interface_ctx* interface_ctx = &interface_ctx_array[i];

		interface_ctx->type = LDP_LOCAL_IP;
		interface_ctx->info_s = interface_ctx->info_r;
		interface_ctx->info_s.port += 5000;

		if (ldp_dds_prepare_interface_ctx(interface_ctx,
		                                  &interface_ctx->info_r,
		                                  &interface_ctx->info_s,
		                                  LDP_DDS_ROLE_MAIN,
		                                  ctx->mem_pool) != LDP_SUCCESS) {
			ldp_log_PF_log(ECOA_LOG_ERROR_PF,
			               "ERROR",
			               ctx->logger_PF,
			               "[DDS] cannot initialize main DDS interface");
			return;
		}

		if (runtime == NULL) {
			runtime = ldp_dds_get_runtime(&interface_ctx->inter.local);
		}
	}

	if (ctx->mcast_reader_interface_num > 0 || ctx->mcast_sender_interface_num > 0) {
		ldp_log_PF_log(ECOA_LOG_WARN_PF,
		               "WARNING",
		               ctx->logger_PF,
		               "[DDS] ELI multicast interfaces are not handled by the DDS main server");
	}

	if (runtime != NULL) {
		run_dds_main_server(ctx, interface_ctx_array, runtime);
	}

	for (int i = 0; i < ctx->PD_number; ++i) {
		if (interface_ctx_array[i].type == LDP_LOCAL_IP) {
			ldp_destroy_interface_dds(&interface_ctx_array[i].inter.local);
		}
	}
}
