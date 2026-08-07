/**
 * @file ldp_apex_port_comp_server.c
 * @brief Protection-domain LDP server using APEX queuing ports.
 */

#include <stdbool.h>

#include <apr_pools.h>

#include "ldp_apex_port.h"
#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_status_error.h"
#include "ldp_structures.h"

static ldp_status_t ldp_apex_send_msg_to_father(
	ldp_PDomain_ctx* ctx,
	uint8_t message_id)
{
	net_data_w data_w = {0};

	if (ctx == NULL || ctx->interface_ctx_array == NULL ||
	    ctx->nb_client < 0 ||
	    ctx->nb_client >= ctx->nb_client + ctx->nb_server) {
		return LDP_ERROR;
	}

	return ldp_IP_write(&ctx->interface_ctx_array[ctx->nb_client],
	                    (char*)&message_id,
	                    sizeof(message_id),
	                    &data_w);
}

static ldp_status_t ldp_apex_attach_component_interface(
	ldp_PDomain_ctx* ctx,
	ldp_interface_ctx* interface_ctx)
{
	ldp_status_t status;

	if (interface_ctx->type == LDP_ELI_MCAST) {
		return LDP_SUCCESS;
	}

	interface_ctx->type = LDP_LOCAL_IP;
	status = ldp_attach_interface_apex_port(
		&interface_ctx->inter.local,
		&interface_ctx->info_r,
		&interface_ctx->info_s);
	if (status != LDP_SUCCESS) {
		ldp_error_status_log(
			ctx->logger_PF,
			status,
			"[%s][APEX] cannot attach queuing ports RX=%s TX=%s. ",
			ctx->name,
			interface_ctx->info_r.addr != NULL ? interface_ctx->info_r.addr : "(null)",
			interface_ctx->info_s.addr != NULL ? interface_ctx->info_s.addr : "(null)");
	}

	return status;
}

static void ldp_apex_detach_component_interfaces(
	ldp_interface_ctx* interface_ctx_array,
	int interface_count)
{
	for (int i = 0; i < interface_count; ++i) {
		if (interface_ctx_array[i].type == LDP_LOCAL_IP) {
			ldp_detach_interface_apex_port(
				&interface_ctx_array[i].inter.local);
		}
	}
}

static ldp_status_t ldp_apex_dispatch_component_message(
	ldp_PDomain_ctx* ctx,
	ldp_interface_ctx* interface_ctx,
	char* message,
	apr_size_t message_length)
{
	uint32_t operation_id = 0;
	uint32_t parameter_size = 0;

	if (message_length < LDP_HEADER_TCP_SIZE) {
		return LDP_ERROR;
	}

	if (!ldp_read_IP_header(ctx,
	                        message,
	                        &operation_id,
	                        &parameter_size)) {
		return LDP_ERROR;
	}

	if ((apr_size_t)LDP_HEADER_TCP_SIZE + parameter_size > message_length) {
		return LDP_ERROR;
	}

	return domain_proc_consume_msg(ctx,
	                               message,
	                               parameter_size,
	                               operation_id,
	                               interface_ctx);
}

static MESSAGE_SIZE_TYPE ldp_apex_component_receive_capacity(
	const ldp_interface_ctx* interface_ctx_array,
	int interface_count)
{
	MESSAGE_SIZE_TYPE capacity = 0;

	for (int i = 0; i < interface_count; ++i) {
		const ldp_interface_apex_port* interface =
			&interface_ctx_array[i].inter.local;

		if (interface_ctx_array[i].type == LDP_LOCAL_IP &&
		    interface->receive_port_attached &&
		    interface->receive_max_message_size > capacity) {
			capacity = interface->receive_max_message_size;
		}
	}

	return capacity;
}

ldp_status_t ldp_apex_attach_component_ports(ldp_PDomain_ctx* ctx)
{
	ldp_interface_ctx* interface_ctx_array;
	int interface_count;

	if (ctx == NULL || ctx->interface_ctx_array == NULL) {
		return APR_EINVAL;
	}

	interface_ctx_array = ctx->interface_ctx_array;
	interface_count = ctx->nb_client + ctx->nb_server;
	if (interface_count <= 0 ||
	    ctx->nb_client < 0 ||
	    ctx->nb_client >= interface_count) {
		return APR_EINVAL;
	}

	for (int i = 0; i < interface_count; ++i) {
		if (ldp_apex_attach_component_interface(
			    ctx,
			    &interface_ctx_array[i]) != LDP_SUCCESS) {
			ldp_apex_detach_component_interfaces(
				interface_ctx_array,
				i);
			return LDP_ERROR;
		}
	}

	if (!interface_ctx_array[ctx->nb_client].inter.local.send_port_attached ||
	    !interface_ctx_array[ctx->nb_client].inter.local.receive_port_attached) {
		ldp_log_PF_log(
			ECOA_LOG_ERROR_PF,
			"ERROR",
			ctx->logger_PF,
			"[APEX] component-to-main interface requires RX and TX ports");
		ldp_apex_detach_component_interfaces(interface_ctx_array,
		                                     interface_count);
		return LDP_ERROR;
	}

	return LDP_SUCCESS;
}

void ldp_start_comp_server(ldp_PDomain_ctx* ctx)
{
	ldp_interface_ctx* interface_ctx_array;
	char* message_buffer;
	MESSAGE_SIZE_TYPE receive_capacity;
	int interface_count;
	bool server_is_running = true;

	if (ctx == NULL || ctx->interface_ctx_array == NULL) {
		return;
	}

	interface_ctx_array = ctx->interface_ctx_array;
	interface_count = ctx->nb_client + ctx->nb_server;
	if (interface_count <= 0 ||
	    ctx->nb_client < 0 ||
	    ctx->nb_client >= interface_count) {
		return;
	}

	if (ldp_apex_attach_component_ports(ctx) != LDP_SUCCESS) {
		return;
	}

	receive_capacity = ldp_apex_component_receive_capacity(
		interface_ctx_array,
		interface_count);
	if (receive_capacity < (MESSAGE_SIZE_TYPE)LDP_HEADER_TCP_SIZE) {
		ldp_apex_detach_component_interfaces(interface_ctx_array,
		                                     interface_count);
		return;
	}

	message_buffer = apr_palloc(ctx->mem_pool,
	                            (apr_size_t)receive_capacity);
	if (message_buffer == NULL) {
		ldp_apex_detach_component_interfaces(interface_ctx_array,
		                                     interface_count);
		return;
	}

	ctx->state = PDomain_IDLE;
	if (ldp_apex_send_msg_to_father(ctx, LDP_ID_CLIENT_INIT) !=
	    LDP_SUCCESS) {
		ldp_log_PF_log_var(
			ECOA_LOG_ERROR_PF,
			"ERROR",
			ctx->logger_PF,
			"[%s][APEX] cannot send CLIENT_INIT to main",
			ctx->name);
		ldp_apex_detach_component_interfaces(interface_ctx_array,
		                                     interface_count);
		return;
	}
	ctx->state = PDomain_INIT;

	/*
	 * APEX Part 1 has no generic poll call spanning several queuing ports.
	 * This first implementation performs non-blocking receives and yields
	 * with TIMED_WAIT when a complete scan receives nothing.
	 *
	 * TODO: Evaluate one APEX PROCESS per receive port if the target's
	 * scheduling and process budget make that model preferable.
	 * TODO: ELI multicast needs a target-specific I/O-partition transport.
	 */
	while (server_is_running) {
		bool received_message = false;

		for (int i = 0; i < interface_count; ++i) {
			ldp_interface_ctx* interface_ctx = &interface_ctx_array[i];
			apr_size_t message_length = (apr_size_t)receive_capacity;
			ldp_status_t status;

			if (interface_ctx->type != LDP_LOCAL_IP ||
			    !interface_ctx->inter.local.receive_port_attached) {
				continue;
			}

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
					"[%s][APEX] cannot receive from queuing port %s. ",
					ctx->name,
					interface_ctx->info_r.addr != NULL
						? interface_ctx->info_r.addr
						: "(null)");
				continue;
			}

			received_message = true;
			if (message_length == 0) {
				continue;
			}

			status = ldp_apex_dispatch_component_message(
				ctx,
				interface_ctx,
				message_buffer,
				message_length);
			if (status != LDP_SUCCESS) {
				server_is_running = false;
				break;
			}
		}

		if (server_is_running && !received_message) {
			(void)ldp_apex_port_idle_wait();
		}
	}

	ldp_apex_detach_component_interfaces(interface_ctx_array,
	                                     interface_count);
}
