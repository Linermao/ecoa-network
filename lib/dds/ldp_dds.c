#include "ldp_dds_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <apr_errno.h>
#include <apr_poll.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"

static ldp_interface_dds* dds_local(ldp_interface_ctx* interface_ctx)
{
	return (ldp_interface_dds*)&interface_ctx->inter.local;
}

static ldp_dds_runtime* g_runtime = NULL;

static ldp_status_t dds_log_error(const char* operation, dds_return_t ret)
{
	if (ret < 0) {
		fprintf(stderr, "[DDS] %s failed: %s\n", operation, dds_strretcode(-ret));
	} else {
		fprintf(stderr, "[DDS] %s failed\n", operation);
	}
	return LDP_ERROR;
}

static uint32_t dds_port_or_zero(const ldp_socket_info* info)
{
	return info != NULL && info->port > 0 ? (uint32_t)info->port : 0;
}

static bool dds_target_port_filter(const void* sample, void* arg)
{
	const LDP_DDS_Packet* packet = (const LDP_DDS_Packet*)sample;
	const ldp_dds_runtime* runtime = (const ldp_dds_runtime*)arg;

	if (packet == NULL || runtime == NULL) {
		return false;
	}

	for (size_t i = 0; i < runtime->target_port_count; ++i) {
		if (runtime->target_ports[i] == packet->target_port) {
			return true;
		}
	}

	return false;
}

static ldp_status_t dds_runtime_add_target_port(ldp_dds_runtime* runtime, uint32_t port)
{
	if (runtime == NULL || port == 0) {
		return LDP_ERROR;
	}

	for (size_t i = 0; i < runtime->target_port_count; ++i) {
		if (runtime->target_ports[i] == port) {
			return LDP_SUCCESS;
		}
	}

	if (runtime->target_port_count >= LDP_DDS_MAX_TARGET_PORTS) {
		fprintf(stderr, "[DDS] too many target ports registered\n");
		return LDP_ERROR;
	}

	runtime->target_ports[runtime->target_port_count++] = port;
	return LDP_SUCCESS;
}

static ldp_status_t dds_create_runtime(ldp_dds_runtime** runtime_out)
{
	ldp_dds_runtime* runtime = calloc(1, sizeof(*runtime));
	dds_return_t ret;
	if (runtime == NULL) {
		return LDP_ERROR;
	}

	runtime->participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
	if (runtime->participant < 0) {
		dds_log_error("dds_create_participant", runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	runtime->data_write_topic = dds_create_topic(runtime->participant,
	                                             &LDP_DDS_Packet_desc,
	                                             LDP_DDS_DATA_TOPIC,
	                                             NULL,
	                                             NULL);
	if (runtime->data_write_topic < 0) {
		dds_log_error("dds_create_topic(write)", runtime->data_write_topic);
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	runtime->data_read_topic = dds_create_topic(runtime->participant,
	                                            &LDP_DDS_Packet_desc,
	                                            LDP_DDS_DATA_TOPIC,
	                                            NULL,
	                                            NULL);
	if (runtime->data_read_topic < 0) {
		dds_log_error("dds_create_topic(read)", runtime->data_read_topic);
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	ret = dds_set_topic_filter_and_arg(runtime->data_read_topic,
	                                   dds_target_port_filter,
	                                   runtime);
	if (ret < 0) {
		dds_log_error("dds_set_topic_filter_and_arg", ret);
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	runtime->data_writer = dds_create_writer(runtime->participant,
	                                         runtime->data_write_topic,
	                                         NULL,
	                                         NULL);
	if (runtime->data_writer < 0) {
		dds_log_error("dds_create_writer", runtime->data_writer);
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	runtime->data_reader = dds_create_reader(runtime->participant,
	                                         runtime->data_read_topic,
	                                         NULL,
	                                         NULL);
	if (runtime->data_reader < 0) {
		dds_log_error("dds_create_reader", runtime->data_reader);
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	*runtime_out = runtime;
	return LDP_SUCCESS;
}

static ldp_status_t dds_acquire_runtime(ldp_dds_runtime** runtime_out)
{
	if (g_runtime == NULL) {
		if (dds_create_runtime(&g_runtime) != LDP_SUCCESS) {
			return LDP_ERROR;
		}
	}

	g_runtime->ref_count++;
	*runtime_out = g_runtime;
	return LDP_SUCCESS;
}

static void dds_release_runtime(ldp_dds_runtime* runtime)
{
	if (runtime == NULL) {
		return;
	}

	if (runtime->ref_count > 0) {
		runtime->ref_count--;
	}

	if (runtime->ref_count != 0) {
		return;
	}

	if (runtime->participant >= 0) {
		dds_delete(runtime->participant);
	}
	if (runtime == g_runtime) {
		g_runtime = NULL;
	}
	free(runtime);
}

static ldp_status_t dds_publish_bytes(ldp_interface_dds* interface,
                                      const char* payload,
                                      int payload_size,
                                      net_data_w_dds* data_w)
{
	LDP_DDS_Packet packet;
	dds_return_t ret;

	UNUSED(data_w);

	if (interface == NULL || interface->backend == NULL || !interface->initialized) {
		return LDP_ERROR;
	}

	if (payload == NULL || payload_size < 0) {
		return LDP_ERROR;
	}

	memset(&packet, 0, sizeof(packet));
	packet.source_port = dds_port_or_zero(interface->info_r);
	packet.target_port = dds_port_or_zero(interface->info_w != NULL ? interface->info_w : interface->info_r);
	packet.kind = LDP_DDS_LDP_DDS_PACKET_DATA;
	packet.payload._maximum = (uint32_t)payload_size;
	packet.payload._length = (uint32_t)payload_size;
	packet.payload._buffer = (uint8_t*)payload;
	packet.payload._release = false;

	ret = dds_write(interface->backend->runtime->data_writer, &packet);
	if (ret < 0) {
		return dds_log_error("dds_write", ret);
	}

	return LDP_SUCCESS;
}

ldp_status_t ldp_create_interface_dds(ldp_interface_dds* interface, apr_pool_t* mp)
{
	ldp_socket_info* info_r = NULL;
	ldp_socket_info* info_w = NULL;
	ldp_dds_role role = LDP_DDS_ROLE_UNSET;
	ldp_dds_backend* backend = NULL;
	ldp_dds_runtime* runtime = NULL;

	UNUSED(mp);

	if (interface == NULL) {
		return LDP_ERROR;
	}

	info_r = interface->info_r;
	info_w = interface->info_w;
	role = interface->role;
	memset(interface, 0, sizeof(*interface));
	interface->info_r = info_r;
	interface->info_w = info_w;
	interface->role = role;
	interface->initialized = false;

	backend = calloc(1, sizeof(*backend));
	if (backend == NULL) {
		return LDP_ERROR;
	}

	if (dds_acquire_runtime(&runtime) != LDP_SUCCESS) {
		free(backend);
		return LDP_ERROR;
	}

	if (dds_runtime_add_target_port(runtime, dds_port_or_zero(info_r)) != LDP_SUCCESS) {
		dds_release_runtime(runtime);
		free(backend);
		return LDP_ERROR;
	}

	backend->runtime = runtime;
	interface->backend = backend;
	interface->initialized = true;
	return LDP_SUCCESS;
}

ldp_status_t ldp_dds_prepare_interface_ctx(ldp_interface_ctx* interface_ctx,
                                           ldp_socket_info* info_r,
                                           ldp_socket_info* info_w,
                                           ldp_dds_role role,
                                           apr_pool_t* mp)
{
	if (interface_ctx == NULL || info_r == NULL) {
		return LDP_ERROR;
	}

	interface_ctx->type = LDP_LOCAL_IP;
	interface_ctx->inter.local.info_r = info_r;
	interface_ctx->inter.local.info_w = info_w != NULL ? info_w : info_r;
	interface_ctx->inter.local.role = role;
	return ldp_create_interface_dds(&interface_ctx->inter.local, mp);
}

ldp_dds_runtime* ldp_dds_get_runtime(ldp_interface_dds* interface)
{
	if (interface == NULL || interface->backend == NULL) {
		return NULL;
	}
	return interface->backend->runtime;
}

void ldp_destroy_interface_dds(ldp_interface_dds* interface)
{
	if (interface == NULL) {
		return;
	}

	if (interface->backend != NULL) {
		dds_release_runtime(interface->backend->runtime);
		free(interface->backend);
	}

	interface->backend = NULL;
	interface->initialized = false;
}

ldp_status_t ldp_IP_write(ldp_interface_ctx* sock_interface,
                          char* msg,
                          int length,
                          net_data_w* data_w)
{
	if (sock_interface == NULL || msg == NULL || length < 0) {
		return LDP_ERROR;
	}

	return dds_publish_bytes(dds_local(sock_interface),
	                         msg,
	                         length,
	                         (net_data_w_dds*)data_w);
}

ldp_status_t ldp_IP_read(ldp_interface_ctx* sock_interface, char* msg, apr_size_t* len)
{
	UNUSED(sock_interface);
	UNUSED(msg);

	if (len != NULL) {
		*len = 0;
	}

	/*
	 * DDS is expected to deliver data through reader callbacks or waitsets.
	 * The callback should call ldp_dds_deliver_to_component/main instead of
	 * pulling bytes through this socket-style API.
	 */
	return APR_ENOTIMPL;
}

ldp_status_t ldp_dds_deliver_to_component(ldp_PDomain_ctx* ctx,
                                          ldp_interface_ctx* interface_ctx,
                                          char* payload,
                                          uint32_t payload_size)
{
	uint32_t op_ID = 0;
	uint32_t param_size = 0;

	UNUSED(payload_size);

	if (ctx == NULL || interface_ctx == NULL || payload == NULL) {
		return LDP_ERROR;
	}

	if (!ldp_read_IP_header(ctx, payload, &op_ID, &param_size)) {
		return LDP_ERROR;
	}

	return domain_proc_consume_msg(ctx, payload, param_size, op_ID, interface_ctx);
}

ldp_status_t ldp_dds_deliver_to_main(ldp_Main_ctx* ctx,
                                     char* payload,
                                     uint32_t payload_size,
                                     ldp_interface_ctx* read_interface_ctx,
                                     ldp_interface_ctx* interface_ctx_array)
{
	apr_pollfd_t fake_fd;

	if (ctx == NULL || payload == NULL || payload_size == 0 ||
	    read_interface_ctx == NULL || interface_ctx_array == NULL) {
		return LDP_ERROR;
	}

	if ((uint8_t)payload[0] == LDP_ID_CLIENT_FAULT_ERROR) {
		ECOA__asset_id asset_id = 0;
		ECOA__asset_type asset_type = 0;
		ECOA__error_type error_type = 0;
		ECOA__uint32 error_code = 0;

		if (payload_size < LDP_FAULT_ERROR_MSG_SIZE) {
			return LDP_ERROR;
		}

		ldp_read_IP_fault_error(&payload[1], &asset_id, &asset_type, &error_type, &error_code);
		ldp_fault_error_notification(ctx, asset_id, asset_type, error_type, error_code);
		return LDP_SUCCESS;
	}

	memset(&fake_fd, 0, sizeof(fake_fd));
	fake_fd.client_data = read_interface_ctx;
	return main_proc_consume_msg(ctx,
	                             payload,
	                             &fake_fd,
	                             read_interface_ctx,
	                             interface_ctx_array);
}
