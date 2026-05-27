#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <apr.h>
#include <apr_time.h>

#include "ldp_network.h"
#include "ldp_structures.h"

#define SENDER_CONTROL_PORT 46001
#define MAIN_SENDER_PORT 41001
#define RECEIVER_DATA_PORT 30002

typedef struct sender_process {
	ldp_PDomain_ctx ctx;
	ldp_interface_ctx interfaces[3];
	ldp_socket_info control_target;
	ldp_socket_info receiver_target;
	ldp_socket_info main_target;
	char msg_buffer[256];
} sender_process;

static ldp_status_t send_ldp_message(ldp_interface_ctx* interface_ctx,
                                     uint32_t op_id,
                                     const char* payload)
{
	char msg_buffer[256];
	const uint32_t payload_size = payload != NULL ? (uint32_t)strlen(payload) : 0;
	const uint32_t msg_size = LDP_HEADER_TCP_SIZE + payload_size;
	net_data_w data_w;

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

static ldp_status_t send_main_control(ldp_interface_ctx* interface_ctx, uint8_t control_id)
{
	net_data_w data_w;
	memset(&data_w, 0, sizeof(data_w));
	return ldp_IP_write(interface_ctx, (char*)&control_id, sizeof(control_id), &data_w);
}

static void sender_route(ldp_PDomain_ctx* ctx,
                         uint32_t operation_id,
                         char* msg,
                         int size_msg,
                         ldp_interface_ctx* socket_sender,
                         int port_nb,
                         uint32_t ELI_sequence_num,
                         ECOA__uint32 sender_PF_ID)
{
	UNUSED(ctx);
	UNUSED(msg);
	UNUSED(size_msg);
	UNUSED(socket_sender);
	UNUSED(ELI_sequence_num);
	UNUSED(sender_PF_ID);

	printf("[PD_sender_PD] local route op=%u port=%d\n", operation_id, port_nb);
	fflush(stdout);
}

static void* run_sender_server(void* arg)
{
	sender_process* process = (sender_process*)arg;
	ldp_start_comp_server(&process->ctx);
	return NULL;
}

int main(void)
{
	apr_pool_t* mem_pool = NULL;
	sender_process process;
	pthread_t server_thread;

	apr_initialize();
	apr_pool_create(&mem_pool, NULL);
	memset(&process, 0, sizeof(process));

	process.control_target.port = SENDER_CONTROL_PORT;
	process.receiver_target.port = RECEIVER_DATA_PORT;
	process.main_target.port = MAIN_SENDER_PORT;

	process.interfaces[0].type = LDP_LOCAL_IP;
	process.interfaces[0].info_r = (ldp_tcp_info){ SENDER_CONTROL_PORT, "127.0.0.1", 1, true };
	process.interfaces[0].inter.local.info_w = &process.control_target;

	process.interfaces[1].type = LDP_LOCAL_IP;
	process.interfaces[1].info_r = (ldp_tcp_info){ SENDER_CONTROL_PORT, "127.0.0.1", 1, true };
	process.interfaces[1].inter.local.info_w = &process.receiver_target;

	process.interfaces[2].type = LDP_LOCAL_IP;
	process.interfaces[2].info_r = (ldp_tcp_info){ SENDER_CONTROL_PORT, "127.0.0.1", 1, true };
	process.interfaces[2].inter.local.info_w = &process.main_target;

	process.ctx.name = "PD_sender_PD";
	process.ctx.mem_pool = mem_pool;
	process.ctx.nb_client = 3;
	process.ctx.nb_server = 0;
	process.ctx.interface_ctx_array = process.interfaces;
	process.ctx.route_function_ptr = sender_route;
	process.ctx.msg_buffer_size = sizeof(process.msg_buffer) - LDP_HEADER_TCP_SIZE;
	process.ctx.msg_buffer = process.msg_buffer;
	process.ctx.state = PDomain_RUNNING;

	if (pthread_create(&server_thread, NULL, run_sender_server, &process) != 0) {
		fprintf(stderr, "[PD_sender_PD] cannot start server thread\n");
		return 1;
	}

	apr_sleep(apr_time_from_sec(2));

	printf("[PD_sender_PD] send data to receiver\n");
	fflush(stdout);
	send_ldp_message(&process.interfaces[1], 42, "hello-from-sender-pd");

	apr_sleep(apr_time_from_msec(300));

	printf("[PD_sender_PD] stop receiver and main\n");
	fflush(stdout);
	send_ldp_message(&process.interfaces[1], LDP_ID_KILL, NULL);
	send_main_control(&process.interfaces[2], LDP_ID_KILL);

	pthread_join(server_thread, NULL);
	apr_pool_destroy(mem_pool);
	apr_terminate();
	return 0;
}
