#include "ldp_network.h"
#include "ldp_structures.h"

int main()
{
	ldp_Main_ctx main_ctx = {};
	ldp_interface_ctx interfaces[1] = {};
	ldp_start_father_server(&main_ctx, interfaces, 0);

	ldp_PDomain_ctx pd_ctx = {};
	ldp_start_comp_server(&pd_ctx);
	return 0;
}
