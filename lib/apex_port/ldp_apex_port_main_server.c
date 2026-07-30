/**
 * @file ldp_apex_port_main_server.c
 * @brief Main-process LDP server using ARINC 653 APEX queuing ports.
 */

#include <stdbool.h>

#include <apr_pools.h>

#include "ldp_apex_port.h"
#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_status_error.h"
#include "ldp_structures.h"

static ldp_status_t ldp_apex_attach_main_interface(
	ldp_Main_ctx* ctx,
	ldp_interface_ctx* interface_ctx)
{
	ldp_status_t status;

	interface_ctx->type = LDP_LOCAL_IP;
	status = ldp_attach_interface_apex_port(
		&interface_ctx->inter.local,
		&interface_ctx->info_r,
		&interface_ctx->info_s);
	if (status != LDP_SUCCESS) {
		ldp_error_status_log(
			ctx->logger_PF,
			status,
			"[APEX] cannot attach main queuing ports RX=%s TX=%s. ",
			interface_ctx->info_r.addr != NULL ? interface_ctx->info_r.addr : "(null)",
			interface_ctx->info_s.addr != NULL ? interface_ctx->info_s.addr : "(null)");
		return status;
	}

	/*
	 * Main uses each PD link in both directions: it receives CLIENT_* and
	 * fault messages, and sends lifecycle control messages back to the PD.
	 */
	if (!interface_ctx->inter.local.receive_port_attached ||
	    !interface_ctx->inter.local.send_port_attached) {
		ldp_log_PF_log(
			ECOA_LOG_ERROR_PF,
			"ERROR",
			ctx->logger_PF,
			"[APEX] main interface requires both RX and TX queuing-port names");
		ldp_detach_interface_apex_port(&interface_ctx->inter.local);
		return LDP_ERROR;
	}

	return LDP_SUCCESS;
}

static void ldp_apex_detach_main_interfaces(
	ldp_interface_ctx* interface_ctx_array,
	int attached_count)
{
	for (int i = 0; i < attached_count; ++i) {
		ldp_detach_interface_apex_port(&interface_ctx_array[i].inter.local);
	}
}

static MESSAGE_SIZE_TYPE ldp_apex_main_receive_capacity(
	const ldp_interface_ctx* interface_ctx_array,
	int interface_count)
{
	MESSAGE_SIZE_TYPE capacity = 0;

	for (int i = 0; i < interface_count; ++i) {
		const ldp_interface_apex_port* interface =
			&interface_ctx_array[i].inter.local;

		if (interface->receive_port_attached &&
		    interface->receive_max_message_size > capacity) {
			capacity = interface->receive_max_message_size;
		}
	}

	return capacity;
}

ldp_status_t ldp_apex_attach_father_ports(
	ldp_Main_ctx* ctx,
	ldp_interface_ctx* interface_ctx_array)
{
	if (ctx == NULL || interface_ctx_array == NULL || ctx->PD_number <= 0) {
		return APR_EINVAL;
	}

	ctx->interface_ctx_array = interface_ctx_array;
	for (int i = 0; i < ctx->PD_number; ++i) {
		if (ldp_apex_attach_main_interface(
			    ctx,
			    &interface_ctx_array[i]) != LDP_SUCCESS) {
			ldp_apex_detach_main_interfaces(interface_ctx_array, i);
			return LDP_ERROR;
		}
	}

	return LDP_SUCCESS;
}

void ldp_start_father_server(ldp_Main_ctx* ctx,
                             ldp_interface_ctx* interface_ctx_array,
                             uint32_t PF_links_num)
{
	char* message_buffer;
	MESSAGE_SIZE_TYPE receive_capacity;
	bool server_is_running = true;

	UNUSED(PF_links_num);

	if (ctx == NULL || interface_ctx_array == NULL || ctx->PD_number <= 0) {
		return;
	}

	ctx->interface_ctx_array = interface_ctx_array;
	if (ldp_apex_attach_father_ports(
		    ctx,
		    interface_ctx_array) != LDP_SUCCESS) {
		return;
	}

	receive_capacity = ldp_apex_main_receive_capacity(
		interface_ctx_array,
		ctx->PD_number);
	if (receive_capacity <= 0) {
		ldp_apex_detach_main_interfaces(interface_ctx_array,
		                                ctx->PD_number);
		return;
	}

	message_buffer = apr_palloc(ctx->mem_pool,
	                            (apr_size_t)receive_capacity);
	if (message_buffer == NULL) {
		ldp_apex_detach_main_interfaces(interface_ctx_array,
		                                ctx->PD_number);
		return;
	}

	/*
	 * TODO: ELI multicast represents external-platform communication and is
	 * not an ARINC 653 queuing-port concept.  Add a separately configured
	 * APEX/IO-partition route when the target platform integration is known.
	 */
	while (server_is_running) {
		bool received_message = false;

		for (int i = 0; i < ctx->PD_number; ++i) {
			ldp_interface_ctx* interface_ctx = &interface_ctx_array[i];
			apr_size_t message_length =
				(apr_size_t)receive_capacity;
			ldp_status_t status;

			status = ldp_IP_read(interface_ctx,
			                     message_buffer,
			                     &message_length);
			if (status == APR_EAGAIN || status == APR_TIMEUP) {
				continue;
			}
			if (status != LDP_SUCCESS) {
				ldp_error_status_log(
					ctx->logger_PF,
					status,
					"[APEX] main cannot receive from queuing port %s. ",
					interface_ctx->info_r.addr != NULL
						? interface_ctx->info_r.addr
						: "(null)");
				continue;
			}

			received_message = true;
			if (message_length == 0) {
				continue;
			}

			status = main_proc_consume_complete_msg(
				ctx,
				message_buffer,
				message_length,
				interface_ctx,
				interface_ctx_array);
			if (status != LDP_SUCCESS) {
				server_is_running = false;
				break;
			}
		}

		if (server_is_running && !received_message) {
			(void)ldp_apex_port_idle_wait();
		}
	}

	ldp_apex_detach_main_interfaces(interface_ctx_array,
	                                ctx->PD_number);
}
