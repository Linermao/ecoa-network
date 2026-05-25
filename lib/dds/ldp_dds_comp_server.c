#include "ldp_dds_internal.h"

#include <stddef.h>

#include "ldp_log_platform.h"
#include "ldp_network.h"
#include "ldp_structures.h"

void ldp_start_comp_server(ldp_PDomain_ctx* ctx)
{
	if (ctx == NULL) {
		return;
	}

	ldp_log_PF_log(ECOA_LOG_ERROR_PF,
	               "ERROR",
	               ctx->logger_PF,
	               "[DDS] component server skeleton reached; DDS wait/read loop is not implemented");

	/*
	 * TODO(DDS):
	 * 1. Initialize ctx->interface_ctx_array[i].inter.local for each local link.
	 * 2. Create CycloneDDS readers/writers with ldp_create_interface_dds().
	 * 3. Block on a DDS WaitSet or listener-driven event loop.
	 * 4. For each received sample, map its target metadata to an interface.
	 * 5. Call ldp_dds_deliver_to_component().
	 */
}
