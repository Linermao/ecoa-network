#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <apr.h>

#include "ldp_network.h"
#include "ldp_structures.h"

#define RECEIVER_DATA_PORT 30002
#define RECEIVER_CONTROL_PORT 46002
#define MAIN_RECEIVER_PORT 41002

static void receiver_route(ldp_PDomain_ctx* ctx,
                           uint32_t operation_id,
                           char* msg,
                           int size_msg,
                           ldp_interface_ctx* socket_sender,
                           int port_nb,
                           uint32_t ELI_sequence_num,
                           ECOA__uint32 sender_PF_ID)
{
	UNUSED(ctx);
	UNUSED(socket_sender);
	UNUSED(ELI_sequence_num);
	UNUSED(sender_PF_ID);

	printf("[PD_receiver_PD] receive op=%u port=%d payload=%.*s\n",
	       operation_id,
	       port_nb,
	       size_msg,
	       msg);
	fflush(stdout);
}

int main(void)
{
	apr_pool_t* mem_pool = NULL;
	ldp_PDomain_ctx ctx;
	ldp_interface_ctx interfaces[2];
	ldp_socket_info data_target;
	ldp_socket_info main_target;
	char msg_buffer[256];

	apr_initialize();
	apr_pool_create(&mem_pool, NULL);
	memset(&ctx, 0, sizeof(ctx));
	memset(interfaces, 0, sizeof(interfaces));
	memset(&data_target, 0, sizeof(data_target));
	memset(&main_target, 0, sizeof(main_target));
	memset(msg_buffer, 0, sizeof(msg_buffer));

	data_target.port = RECEIVER_DATA_PORT;
	main_target.port = MAIN_RECEIVER_PORT;

	interfaces[0].type = LDP_LOCAL_IP;
	interfaces[0].info_r = (ldp_tcp_info){ RECEIVER_DATA_PORT, "127.0.0.1", 1, true };
	interfaces[0].inter.local.info_w = &data_target;

	interfaces[1].type = LDP_LOCAL_IP;
	interfaces[1].info_r = (ldp_tcp_info){ RECEIVER_CONTROL_PORT, "127.0.0.1", 1, true };
	interfaces[1].inter.local.info_w = &main_target;

	ctx.name = "PD_receiver_PD";
	ctx.mem_pool = mem_pool;
	ctx.nb_client = 2;
	ctx.nb_server = 0;
	ctx.interface_ctx_array = interfaces;
	ctx.route_function_ptr = receiver_route;
	ctx.msg_buffer_size = sizeof(msg_buffer) - LDP_HEADER_TCP_SIZE;
	ctx.msg_buffer = msg_buffer;
	ctx.state = PDomain_RUNNING;

	ldp_start_comp_server(&ctx);

	apr_pool_destroy(mem_pool);
	apr_terminate();
	return 0;
}
