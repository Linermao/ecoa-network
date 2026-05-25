#include "ldp_dds_internal.h"

#include <stddef.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"

void ldp_start_father_server(ldp_Main_ctx* ctx,
                             ldp_interface_ctx* interface_ctx_array,
                             uint32_t PF_links_num)
{
	UNUSED(interface_ctx_array);
	UNUSED(PF_links_num);

	if (ctx == NULL) {
		return;
	}

	ldp_log_PF_log(ECOA_LOG_ERROR_PF,
	               "ERROR",
	               ctx->logger_PF,
	               "[DDS] main server skeleton reached; DDS wait/read loop is not implemented");

	/*
	 * TODO(DDS):
	 * 1. Initialize one DDS reader/writer pair per protection domain link.
	 * 2. Subscribe to lifecycle/fault messages from PD processes.
	 * 3. For each received sample, call ldp_dds_deliver_to_main().
	 * 4. Use write_msg()/broadcast_to_client() through ldp_IP_write() for
	 *    lifecycle commands; the DDS writer path lives in ldp_dds.c.
	 */
}
