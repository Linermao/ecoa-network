/**
 * @file ldp_fine_grain_deployment.c
 * @brief APEX-safe stubs for Unix deployment controls.
 */

#include "ldp_fine_grain_deployment.h"

cpu_mask ldp_create_cpu_mask(int nb_cpu, int *cpu_ids)
{
	cpu_mask mask = {0};
	(void)nb_cpu;
	(void)cpu_ids;
	return mask;
}

cpu_mask ldp_create_cpu_mask_full(void)
{
	cpu_mask mask = {0};
	return mask;
}

ldp_status_t ldp_set_proc_affinty(cpu_mask mask)
{
	(void)mask;
	return LDP_SUCCESS;
}

void log_thread_deployment_properties(ldp_logger_platform *logger)
{
	(void)logger;
}
