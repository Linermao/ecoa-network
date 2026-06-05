#include "ldp_dds_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <apr_errno.h>
#include <apr_poll.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"
#include "ldp_ELI.h"
#include "ldp_ELI_udp.h"

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

bool ldp_dds_trace_enabled(void)
{
	const char* value = getenv("LDP_DDS_TRACE");

	return value != NULL &&
	       value[0] != '\0' &&
	       strcmp(value, "0") != 0 &&
	       strcmp(value, "false") != 0 &&
	       strcmp(value, "FALSE") != 0;
}

const char* ldp_dds_trace_kind_name(LDP_DDS_PacketKind kind)
{
	switch (kind) {
		case LDP_DDS_LDP_DDS_PACKET_DATA:
			return "DATA";
		case LDP_DDS_LDP_DDS_PACKET_CONTROL:
			return "CONTROL";
		case LDP_DDS_LDP_DDS_PACKET_FAULT:
			return "FAULT";
		default:
			return "UNKNOWN";
	}
}

uint32_t ldp_dds_trace_op_id(const LDP_DDS_Packet* packet)
{
	uint32_t op_ID = 0;
	bool control_word = true;

	if (packet == NULL || packet->payload._buffer == NULL || packet->payload._length == 0) {
		return 0;
	}

	if (packet->payload._length <= sizeof(uint32_t)) {
		for (uint32_t i = 1; i < packet->payload._length; ++i) {
			if (packet->payload._buffer[i] != 0) {
				control_word = false;
				break;
			}
		}
		if (control_word) {
			return packet->payload._buffer[0];
		}
	}

	if (packet->payload._length >= LDP_HEADER_TCP_SIZE &&
	    packet->payload._buffer[0] == 0xE &&
	    packet->payload._buffer[1] == 0xC &&
	    packet->payload._buffer[2] == 0x0 &&
	    packet->payload._buffer[3] == 0xA) {
		memcpy(&op_ID, &packet->payload._buffer[8], LDP_OP_ID_SIZE);
		return op_ID;
	}

	if (packet->payload._length >= LDP_ELI_UDP_HEADER_SIZE + LDP_ELI_HEADER_SIZE &&
	    packet->payload._buffer[LDP_ELI_UDP_HEADER_SIZE] == LDP_ELI_VERSION) {
		memcpy(&op_ID,
		       &packet->payload._buffer[LDP_ELI_UDP_HEADER_SIZE + 8],
		       sizeof(op_ID));
		return op_ID;
	}

	return 0;
}

void ldp_dds_trace_packet(const char* stage,
                          const char* owner,
                          int interface_index,
                          const LDP_DDS_Packet* packet)
{
	if (!ldp_dds_trace_enabled() || packet == NULL) {
		return;
	}

	fprintf(stderr,
	        "[DDS %s] owner=%s iface=%d source=%" PRIu32 " target=%" PRIu32
	        " module=%" PRIu16 " msg=%" PRIu16 " kind=%s op=%" PRIu32
	        " size=%" PRIu32 "\n",
	        stage != NULL ? stage : "?",
	        owner != NULL ? owner : "?",
	        interface_index,
	        packet->source_port,
	        packet->target_port,
	        packet->module_id,
	        packet->msg_id,
	        ldp_dds_trace_kind_name(packet->kind),
	        ldp_dds_trace_op_id(packet),
	        packet->payload._length);
}

static LDP_DDS_PacketKind dds_packet_kind_from_payload(const char* payload,
                                                       int payload_size)
{
	uint32_t op_ID = 0;
	bool control_word = true;

	if (payload == NULL || payload_size <= 0) {
		return LDP_DDS_LDP_DDS_PACKET_DATA;
	}

	if ((uint8_t)payload[0] == LDP_ID_CLIENT_FAULT_ERROR) {
		return LDP_DDS_LDP_DDS_PACKET_FAULT;
	}

	if (payload_size == 1) {
		switch ((uint8_t)payload[0]) {
			case LDP_ID_CLIENT_INIT:
			case LDP_ID_CLIENT_READY:
			case LDP_ID_KILL:
			case LDP_ID_SHUTDOWN:
				return LDP_DDS_LDP_DDS_PACKET_CONTROL;
			case LDP_ID_CLIENT_FAULT_ERROR:
				return LDP_DDS_LDP_DDS_PACKET_FAULT;
			default:
				return LDP_DDS_LDP_DDS_PACKET_DATA;
		}
	}

	if (payload_size <= (int)sizeof(uint32_t)) {
		for (int i = 1; i < payload_size; ++i) {
			if (payload[i] != 0) {
				control_word = false;
				break;
			}
		}

		if (control_word) {
			switch ((uint8_t)payload[0]) {
				case LDP_ID_CLIENT_INIT:
				case LDP_ID_CLIENT_READY:
				case LDP_ID_KILL:
				case LDP_ID_SHUTDOWN:
					return LDP_DDS_LDP_DDS_PACKET_CONTROL;
				default:
					break;
			}
		}
	}

	if (payload_size >= LDP_HEADER_TCP_SIZE &&
	    payload[0] == 0xE &&
	    payload[1] == 0xC &&
	    payload[2] == 0x0 &&
	    payload[3] == 0xA) {
		memcpy(&op_ID, &payload[8], LDP_OP_ID_SIZE);
		switch (op_ID) {
			case LDP_ID_INIT_MOD:
			case LDP_ID_START_MOD:
			case LDP_ID_KILL:
			case LDP_ID_SHUTDOWN:
			case LDP_ID_SYNC:
				return LDP_DDS_LDP_DDS_PACKET_CONTROL;
			case LDP_ID_CLIENT_FAULT_ERROR:
				return LDP_DDS_LDP_DDS_PACKET_FAULT;
			default:
				return LDP_DDS_LDP_DDS_PACKET_DATA;
		}
	}

	return LDP_DDS_LDP_DDS_PACKET_DATA;
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

ldp_status_t ldp_dds_runtime_add_target_port(ldp_dds_runtime* runtime, uint32_t port)
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
	dds_qos_t* qos = NULL;
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

	qos = dds_create_qos();
	if (qos == NULL) {
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}
	dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
	dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
	dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 32);

	runtime->data_writer = dds_create_writer(runtime->participant,
	                                         runtime->data_write_topic,
	                                         qos,
	                                         NULL);
	if (runtime->data_writer < 0) {
		dds_log_error("dds_create_writer", runtime->data_writer);
		dds_delete_qos(qos);
		dds_delete(runtime->participant);
		free(runtime);
		return LDP_ERROR;
	}

	runtime->data_reader = dds_create_reader(runtime->participant,
	                                         runtime->data_read_topic,
	                                         qos,
	                                         NULL);
	dds_delete_qos(qos);
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

	if (interface == NULL || interface->backend == NULL || !interface->initialized) {
		return LDP_ERROR;
	}

	if (payload == NULL || payload_size < 0) {
		return LDP_ERROR;
	}

	memset(&packet, 0, sizeof(packet));
	packet.source_port = dds_port_or_zero(interface->info_r);
	packet.target_port = dds_port_or_zero(interface->info_w != NULL ? interface->info_w : interface->info_r);
	if (data_w != NULL) {
		data_w->msg_id++;
		packet.module_id = data_w->module_id;
		packet.msg_id = data_w->msg_id;
	}
	packet.kind = dds_packet_kind_from_payload(payload, payload_size);
	packet.payload._maximum = (uint32_t)payload_size;
	packet.payload._length = (uint32_t)payload_size;
	packet.payload._buffer = (uint8_t*)payload;
	packet.payload._release = false;

	ldp_dds_trace_packet("write", interface->role == LDP_DDS_ROLE_MAIN ? "main" : "component", -1, &packet);

	ret = dds_write(interface->backend->runtime->data_writer, &packet);
	if (ret < 0) {
		return dds_log_error("dds_write", ret);
	}

	return LDP_SUCCESS;
}

static ldp_status_t dds_publish_mcast_datagram(ldp_inter_mcast* interface,
                                               char* payload,
                                               uint64_t payload_size)
{
	LDP_DDS_Packet packet;
	dds_return_t ret;

	if (interface == NULL || payload == NULL || payload_size > UINT32_MAX) {
		return LDP_ERROR;
	}

	if (g_runtime == NULL && dds_acquire_runtime(&g_runtime) != LDP_SUCCESS) {
		return LDP_ERROR;
	}

	memset(&packet, 0, sizeof(packet));
	packet.source_port = interface->UDP_current_PF_ID;
	packet.target_port = dds_port_or_zero(interface->ip_info);
	packet.kind = LDP_DDS_LDP_DDS_PACKET_DATA;
	packet.payload._maximum = (uint32_t)payload_size;
	packet.payload._length = (uint32_t)payload_size;
	packet.payload._buffer = (uint8_t*)payload;
	packet.payload._release = false;

	ldp_dds_trace_packet("write.mcast", "eli", -1, &packet);

	ret = dds_write(g_runtime->data_writer, &packet);
	if (ret < 0) {
		return dds_log_error("dds_write(mcast)", ret);
	}

	return LDP_SUCCESS;
}

ldp_status_t ldp_create_interface_dds(ldp_interface_dds* interface, apr_pool_t* mp)
{
	ldp_socket_info* info_r = NULL;
	ldp_socket_info* info_w = NULL;
	ldp_dds_role role = LDP_DDS_ROLE_UNSET;
	bool owns_info_r = false;
	bool owns_info_w = false;
	ldp_dds_backend* backend = NULL;
	ldp_dds_runtime* runtime = NULL;

	UNUSED(mp);

	if (interface == NULL) {
		return LDP_ERROR;
	}

	info_r = interface->info_r;
	info_w = interface->info_w;
	role = interface->role;
	owns_info_r = interface->owns_info_r;
	owns_info_w = interface->owns_info_w;
	memset(interface, 0, sizeof(*interface));
	interface->info_r = info_r;
	interface->info_w = info_w;
	interface->role = role;
	interface->owns_info_r = owns_info_r;
	interface->owns_info_w = owns_info_w;
	interface->initialized = false;

	backend = calloc(1, sizeof(*backend));
	if (backend == NULL) {
		return LDP_ERROR;
	}

	if (dds_acquire_runtime(&runtime) != LDP_SUCCESS) {
		free(backend);
		return LDP_ERROR;
	}

	if (ldp_dds_runtime_add_target_port(runtime, dds_port_or_zero(info_r)) != LDP_SUCCESS) {
		dds_release_runtime(runtime);
		free(backend);
		return LDP_ERROR;
	}

	backend->runtime = runtime;
	interface->backend = backend;
	interface->initialized = true;
	return LDP_SUCCESS;
}

ldp_status_t ldp_dds_mcast_send(ldp_inter_mcast* interface, char* msg, uint64_t msg_size)
{
	return dds_publish_mcast_datagram(interface, msg, msg_size);
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

	if (interface->owns_info_r && interface->info_r != NULL) {
		free(interface->info_r);
	}
	if (interface->owns_info_w && interface->info_w != NULL) {
		free(interface->info_w);
	}

	interface->info_r = NULL;
	interface->info_w = NULL;
	interface->owns_info_r = false;
	interface->owns_info_w = false;
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

	if (payload_size < LDP_HEADER_TCP_SIZE) {
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
