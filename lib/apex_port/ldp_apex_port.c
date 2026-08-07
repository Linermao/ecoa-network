/**
 * @file ldp_apex_port.c
 * @brief APEX queuing-port implementation of the LDP byte-buffer API.
 */

#include <stddef.h>
#include <string.h>

#include "ldp_network.h"
#include "ldp_apex_port.h"
#include "ldp_structures.h"

static bool ldp_apex_copy_port_name(NAME_TYPE destination, const char* source)
{
	size_t source_length;

	if (source == NULL || source[0] == '\0') {
		return false;
	}

	source_length = strlen(source);
	if (source_length >= sizeof(NAME_TYPE)) {
		return false;
	}

	memset(destination, 0, sizeof(NAME_TYPE));
	memcpy(destination, source, source_length);
	return true;
}

static ldp_status_t ldp_apex_attach_port(
	const char* configured_name,
	PORT_DIRECTION_TYPE expected_direction,
	QUEUING_PORT_ID_TYPE* port_id,
	QUEUING_PORT_STATUS_TYPE* port_status)
{
	NAME_TYPE apex_name;
	RETURN_CODE_TYPE return_code = NO_ERROR;
	ldp_status_t status;

	if (port_id == NULL || port_status == NULL ||
	    !ldp_apex_copy_port_name(apex_name, configured_name)) {
		return APR_EINVAL;
	}

	GET_QUEUING_PORT_ID(apex_name, port_id, &return_code);
	status = ldp_apex_port_status(return_code);
	if (status != LDP_SUCCESS) {
		return status;
	}

	memset(port_status, 0, sizeof(*port_status));
	GET_QUEUING_PORT_STATUS(*port_id, port_status, &return_code);
	status = ldp_apex_port_status(return_code);
	if (status != LDP_SUCCESS) {
		return status;
	}

	if (port_status->PORT_DIRECTION != expected_direction ||
	    port_status->MAX_MESSAGE_SIZE <= 0 ||
	    port_status->MAX_NB_MESSAGE <= 0) {
		return APR_EINVAL;
	}

	return LDP_SUCCESS;
}

ldp_status_t ldp_attach_interface_apex_port(
	ldp_interface_apex_port* interface,
	const ldp_socket_info* receive_info,
	const ldp_socket_info* send_info)
{
	QUEUING_PORT_STATUS_TYPE port_status;
	ldp_status_t status;
	bool has_receive_name;
	bool has_send_name;

	if (interface == NULL) {
		return APR_EINVAL;
	}

	memset(interface, 0, sizeof(*interface));
	interface->receive_timeout = LDP_APEX_PORT_NO_WAIT;
	interface->send_timeout = LDP_APEX_PORT_INFINITE_TIME;

	has_receive_name = receive_info != NULL &&
	                   receive_info->addr != NULL &&
	                   receive_info->addr[0] != '\0';
	has_send_name = send_info != NULL &&
	                send_info->addr != NULL &&
	                send_info->addr[0] != '\0';

	if (!has_receive_name && !has_send_name) {
		return APR_EINVAL;
	}

	if (has_receive_name) {
		status = ldp_apex_attach_port(
			receive_info->addr,
			DESTINATION,
			&interface->receive_port_id,
			&port_status);
		if (status != LDP_SUCCESS) {
			return status;
		}
		interface->receive_port_attached = true;
		interface->receive_max_message_size =
			port_status.MAX_MESSAGE_SIZE;
		interface->receive_queue_depth =
			port_status.MAX_NB_MESSAGE;
	}

	if (has_send_name) {
		status = ldp_apex_attach_port(
			send_info->addr,
			SOURCE,
			&interface->send_port_id,
			&port_status);
		if (status != LDP_SUCCESS) {
			ldp_detach_interface_apex_port(interface);
			return status;
		}
		interface->send_port_attached = true;
		interface->send_max_message_size =
			port_status.MAX_MESSAGE_SIZE;
		interface->send_queue_depth =
			port_status.MAX_NB_MESSAGE;
	}

	return LDP_SUCCESS;
}

void ldp_detach_interface_apex_port(ldp_interface_apex_port* interface)
{
	if (interface == NULL) {
		return;
	}

	/*
	 * There is no standard DELETE_QUEUING_PORT service.  Do not clear the
	 * receive queue here either: unread messages belong to the partition's
	 * communication lifecycle and should only be discarded deliberately.
	 */
	memset(interface, 0, sizeof(*interface));
}

ldp_status_t ldp_apex_port_status(RETURN_CODE_TYPE return_code)
{
	switch (return_code) {
		case NO_ERROR:
		case NO_ACTION:
			return LDP_SUCCESS;
		case NOT_AVAILABLE:
			return APR_EAGAIN;
		case TIMED_OUT:
			return APR_TIMEUP;
		case INVALID_PARAM:
			return APR_EINVAL;
		case INVALID_CONFIG:
		case INVALID_MODE:
		default:
			return LDP_ERROR;
	}
}

ldp_status_t ldp_apex_port_idle_wait(void)
{
	RETURN_CODE_TYPE return_code = NO_ERROR;

	TIMED_WAIT(LDP_APEX_PORT_IDLE_WAIT, &return_code);
	return ldp_apex_port_status(return_code);
}

ldp_status_t ldp_IP_write(ldp_interface_ctx* interface_ctx,
                          char* message,
                          int length,
                          net_data_w* data_w)
{
	ldp_interface_apex_port* apex_interface;
	RETURN_CODE_TYPE return_code = NO_ERROR;

	UNUSED(data_w);

	if (interface_ctx == NULL || message == NULL || length < 0 ||
	    interface_ctx->type != LDP_LOCAL_IP) {
		return APR_EINVAL;
	}

	apex_interface = &interface_ctx->inter.local;
	if (!apex_interface->send_port_attached ||
	    length > apex_interface->send_max_message_size) {
		return LDP_ERROR;
	}

	SEND_QUEUING_MESSAGE(
		apex_interface->send_port_id,
		(MESSAGE_ADDR_TYPE)message,
		(MESSAGE_SIZE_TYPE)length,
		apex_interface->send_timeout,
		&return_code);

	return ldp_apex_port_status(return_code);
}

ldp_status_t ldp_IP_read(ldp_interface_ctx* interface_ctx,
                         char* message,
                         apr_size_t* length)
{
	ldp_interface_apex_port* apex_interface;
	MESSAGE_SIZE_TYPE received_length = 0;
	RETURN_CODE_TYPE return_code = NO_ERROR;

	if (interface_ctx == NULL || message == NULL || length == NULL ||
	    interface_ctx->type != LDP_LOCAL_IP) {
		return APR_EINVAL;
	}

	apex_interface = &interface_ctx->inter.local;
	if (!apex_interface->receive_port_attached) {
		*length = 0;
		return LDP_ERROR;
	}

	/*
	 * RECEIVE_QUEUING_MESSAGE does not accept a destination capacity.  The
	 * caller must allocate at least the configured MAX_MESSAGE_SIZE bytes.
	 */
	if (*length <
	    (apr_size_t)apex_interface->receive_max_message_size) {
		*length = 0;
		return APR_ENOSPC;
	}

	RECEIVE_QUEUING_MESSAGE(
		apex_interface->receive_port_id,
		apex_interface->receive_timeout,
		(MESSAGE_ADDR_TYPE)message,
		&received_length,
		&return_code);

	if (return_code != NO_ERROR) {
		*length = 0;
		return ldp_apex_port_status(return_code);
	}

	*length = (apr_size_t)received_length;
	return LDP_SUCCESS;
}
