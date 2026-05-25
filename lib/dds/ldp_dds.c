#include "ldp_dds_internal.h"

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

static ldp_status_t dds_publish_bytes(ldp_interface_dds* interface,
                                      const char* payload,
                                      int payload_size,
                                      net_data_w_dds* data_w)
{
	UNUSED(interface);
	UNUSED(payload);
	UNUSED(payload_size);
	UNUSED(data_w);

	/*
	 * TODO(DDS): publish payload as a CycloneDDS C sample.
	 *
	 * The LDP payload format is already complete before this function is
	 * called. The DDS adapter should wrap it with routing metadata, then write
	 * it to LDP_DDS_DATA_TOPIC or LDP_DDS_CONTROL_TOPIC.
	 */
	return APR_ENOTIMPL;
}

ldp_status_t ldp_create_interface_dds(ldp_interface_dds* interface, apr_pool_t* mp)
{
	ldp_socket_info* info_r = NULL;
	ldp_socket_info* info_w = NULL;
	ldp_dds_role role = LDP_DDS_ROLE_UNSET;

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

	/*
	 * TODO(DDS): allocate struct ldp_dds_backend here and fill it with
	 * CycloneDDS C handles, such as participant, topic, writer, reader and
	 * waitset/listener entities.
	 */
	return APR_ENOTIMPL;
}

void ldp_destroy_interface_dds(ldp_interface_dds* interface)
{
	if (interface == NULL) {
		return;
	}

	/*
	 * TODO(DDS): delete CycloneDDS entities owned by interface->backend, then
	 * release the backend allocation.
	 */
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
