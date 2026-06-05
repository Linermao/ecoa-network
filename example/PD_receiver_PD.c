#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <apr.h>

#include "ldp_network.h"
#include "ldp_structures.h"

#define RECEIVER_DATA_PORT 30002
#define MAIN_RECEIVER_PORT 41002
#define SENDER_CONTROL_PORT 46001
#define OP_DATA 42
#define OP_ACK 43

static ldp_status_t send_ldp_message(ldp_interface_ctx* interface_ctx,
                                     uint32_t op_id,
                                     const char* payload)
{
	char msg_buffer[256];
	const uint32_t payload_size = payload != NULL ? (uint32_t)strlen(payload) : 0;
	const uint32_t msg_size = LDP_HEADER_TCP_SIZE + payload_size;
	net_data_w data_w = {0};

	if (msg_size > sizeof(msg_buffer)) {
		return LDP_ERROR;
	}

	memset(&data_w, 0, sizeof(data_w));
	memset(msg_buffer, 0, sizeof(msg_buffer));
	ldp_written_IP_header(msg_buffer, payload_size, op_id);
	if (payload_size > 0) {
		memcpy(&msg_buffer[LDP_HEADER_TCP_SIZE], payload, payload_size);
	}

	return ldp_IP_write(interface_ctx, msg_buffer, (int)msg_size, &data_w);
}

static void receiver_route(ldp_PDomain_ctx* ctx,
                           uint32_t operation_id,
                           char* msg,
                           int size_msg,
                           ldp_interface_ctx* socket_sender,
                           int port_nb,
                           uint32_t ELI_sequence_num,
                           ECOA__uint32 sender_PF_ID)
{
	UNUSED(socket_sender);
	UNUSED(ELI_sequence_num);
	UNUSED(sender_PF_ID);

	printf("[PD_receiver_PD] receive op=%u port=%d payload=%.*s\n",
	       operation_id,
	       port_nb,
	       size_msg,
	       msg);
	fflush(stdout);

	if (operation_id == OP_DATA) {
		send_ldp_message(&ctx->interface_ctx_array[2], OP_ACK, "ack-from-receiver-pd");
	}
}

int main(void)
{
	apr_pool_t* mem_pool = NULL;
	ldp_PDomain_ctx ctx;
	ldp_interface_ctx interfaces[3];
	char msg_buffer[256];

	apr_initialize();
	apr_pool_create(&mem_pool, NULL);
	memset(&ctx, 0, sizeof(ctx));
	memset(interfaces, 0, sizeof(interfaces));
	memset(msg_buffer, 0, sizeof(msg_buffer));

	interfaces[0].type = LDP_LOCAL_IP;
	interfaces[0].info_r = (ldp_tcp_info){ RECEIVER_DATA_PORT, "127.0.0.1", 1, true };

	interfaces[1].type = LDP_LOCAL_IP;
	interfaces[1].info_r = (ldp_tcp_info){ MAIN_RECEIVER_PORT, "127.0.0.1", 1, true };

	interfaces[2].type = LDP_LOCAL_IP;
	interfaces[2].info_r = (ldp_tcp_info){ SENDER_CONTROL_PORT, "127.0.0.1", 1, true };

	ctx.name = "PD_receiver_PD";
	ctx.mem_pool = mem_pool;
	ctx.nb_client = 1;
	ctx.nb_server = 2;
	ctx.interface_ctx_array = interfaces;
	ctx.route_function_ptr = receiver_route;
	ctx.msg_buffer_size = sizeof(msg_buffer) - LDP_HEADER_TCP_SIZE;
	ctx.msg_buffer = msg_buffer;
	ctx.state = PDomain_IDLE;

	ldp_start_comp_server(&ctx);

	apr_pool_destroy(mem_pool);
	apr_terminate();
	return 0;
}
