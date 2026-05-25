/**
* @file ldp_dds.h
* @brief ECOA LDP DDS transport adapter.
*
* This header mirrors the TCP/UDP transport headers: it declares the local
* interface type selected by ldp_network.h and the write-side per-module data.
*/

#ifndef _LDP_DDS_H
#define _LDP_DDS_H

#include <stdbool.h>
#include <stdint.h>

#include <apr_pools.h>

#include "ECOA.h"
#include "ldp_status_error.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

typedef struct ldp_socket_info ldp_socket_info;       //!< defined in ldp_network.h
typedef struct ldp_interface_ctx ldp_interface_ctx;   //!< defined in ldp_network.h
typedef struct ldp_Main_ctx_t ldp_Main_ctx;           //!< defined in ldp_structures.h
typedef struct ldp_PDomain_ctx_t ldp_PDomain_ctx;     //!< defined in ldp_structures.h

typedef enum ldp_dds_role {
	LDP_DDS_ROLE_UNSET = 0,
	LDP_DDS_ROLE_MAIN,
	LDP_DDS_ROLE_COMPONENT
} ldp_dds_role;

typedef struct ldp_dds_backend ldp_dds_backend;       //!< private DDS backend state

//! DDS local transport context. The backend field is owned by the DDS adapter.
typedef struct ldp_interface_dds {
	ldp_socket_info* info_r; //!< logical receive endpoint
	ldp_socket_info* info_w; //!< logical write endpoint

	ldp_dds_backend* backend;
	ldp_dds_role role;
	bool initialized;
} ldp_interface_dds;

//! Per-module write data. Kept shape-compatible with TCP/UDP call sites.
typedef struct net_data_w_dds {
	char reserved[1];
	ECOA__uint16 module_id;
	ECOA__uint16 msg_id;
} net_data_w_dds;

ldp_status_t ldp_create_interface_dds(ldp_interface_dds* interface, apr_pool_t* mp);
void ldp_destroy_interface_dds(ldp_interface_dds* interface);

/**
 * Entry point for a DDS reader callback delivering a full LDP payload to a PD.
 *
 * The payload must be the same byte buffer that TCP/UDP would pass above the
 * transport layer: LDP header + operation id + serialized parameters.
 */
ldp_status_t ldp_dds_deliver_to_component(ldp_PDomain_ctx* ctx,
                                          ldp_interface_ctx* interface_ctx,
                                          char* payload,
                                          uint32_t payload_size);

/**
 * Entry point for a DDS reader callback delivering a full LDP payload to main.
 */
ldp_status_t ldp_dds_deliver_to_main(ldp_Main_ctx* ctx,
                                     char* payload,
                                     uint32_t payload_size,
                                     ldp_interface_ctx* read_interface_ctx,
                                     ldp_interface_ctx* interface_ctx_array);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* _LDP_DDS_H */
