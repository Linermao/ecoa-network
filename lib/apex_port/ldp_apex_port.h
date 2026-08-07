/**
 * @file ldp_apex_port.h
 * @brief LDP local transport implemented with ARINC 653 APEX queuing ports.
 *
 * This backend deliberately exposes message-oriented APEX semantics.  It does
 * not emulate sockaddr, TCP connections, byte streams, EOF, or pollable file
 * descriptors.
 */

#ifndef _LDP_APEX_PORT_H
#define _LDP_APEX_PORT_H

#include <stdbool.h>

#include <a653Queuing.h>
#include <a653Time.h>

#include "ECOA.h"
#include "ldp_status_error.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ldp_socket_info ldp_socket_info;
typedef struct ldp_interface_ctx ldp_interface_ctx;
typedef struct ldp_Main_ctx_t ldp_Main_ctx;
typedef struct ldp_PDomain_ctx_t ldp_PDomain_ctx;

/*
 * APEX queuing ports are unidirectional.  An LDP interface that used to be a
 * bidirectional TCP connection therefore owns up to two port identifiers:
 *
 *   info_r.addr -> DESTINATION queuing-port name
 *   info_s.addr -> SOURCE queuing-port name
 *
 * Both names must match the target module's static ARINC 653 configuration.
 */
typedef struct ldp_interface_apex_port {
	QUEUING_PORT_ID_TYPE receive_port_id;
	QUEUING_PORT_ID_TYPE send_port_id;

	bool receive_port_attached;
	bool send_port_attached;

	MESSAGE_SIZE_TYPE receive_max_message_size;
	MESSAGE_SIZE_TYPE send_max_message_size;
	MESSAGE_RANGE_TYPE receive_queue_depth;
	MESSAGE_RANGE_TYPE send_queue_depth;

	SYSTEM_TIME_TYPE receive_timeout;
	SYSTEM_TIME_TYPE send_timeout;
} ldp_interface_apex_port;

//! APEX does not need the UDP per-writer fragmentation state.
typedef struct net_data_w_apex_port {
	ECOA__uint16 module_id;
	ECOA__uint16 msg_id;
} net_data_w_apex_port;

#ifndef LDP_APEX_PORT_NO_WAIT
#define LDP_APEX_PORT_NO_WAIT ((SYSTEM_TIME_TYPE)0)
#endif

#ifndef LDP_APEX_PORT_INFINITE_TIME
#ifdef INFINITE_TIME_VALUE
#define LDP_APEX_PORT_INFINITE_TIME ((SYSTEM_TIME_TYPE)INFINITE_TIME_VALUE)
#else
#define LDP_APEX_PORT_INFINITE_TIME ((SYSTEM_TIME_TYPE)-1)
#endif
#endif

#ifndef LDP_APEX_PORT_IDLE_WAIT
#define LDP_APEX_PORT_IDLE_WAIT ((SYSTEM_TIME_TYPE)1000000)
#endif

/**
 * Attach one LDP interface to configured SOURCE and DESTINATION ports.
 *
 * Port objects and their capacities are owned by the target system
 * configuration.  This function never creates them: it resolves each name
 * with GET_QUEUING_PORT_ID, reads its status, and verifies its direction.
 */
ldp_status_t ldp_attach_interface_apex_port(
	ldp_interface_apex_port* interface,
	const ldp_socket_info* receive_info,
	const ldp_socket_info* send_info);

/**
 * Attach every Main-to-PD port pair.
 *
 * info_r.addr and info_s.addr are expected to contain the queuing-port names
 * produced from the target system configuration.
 */
ldp_status_t ldp_apex_attach_father_ports(
	ldp_Main_ctx* ctx,
	ldp_interface_ctx* interface_ctx_array);

/**
 * Attach every local port pair used by a protection domain.
 */
ldp_status_t ldp_apex_attach_component_ports(ldp_PDomain_ctx* ctx);

/**
 * Forget locally cached port identifiers.
 *
 * ARINC 653 does not define a DELETE_QUEUING_PORT service.  Port lifetime is
 * owned by the partition, so this function does not destroy an OS object.
 */
void ldp_detach_interface_apex_port(ldp_interface_apex_port* interface);

//! Translate an APEX return code into the status convention used by LDP.
ldp_status_t ldp_apex_port_status(RETURN_CODE_TYPE return_code);

//! Yield briefly when a server scan did not receive a message.
ldp_status_t ldp_apex_port_idle_wait(void);

#if defined(__cplusplus)
}
#endif

#endif /* _LDP_APEX_PORT_H */
