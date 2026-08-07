#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <apr.h>
#include <apr_strings.h>
#include <apr_thread_proc.h>

#include "ldp_network.h"
#include "ldp_structures.h"
#include "route.h"

static apr_pool_t* g_mem_pool = NULL;
static ldp_Main_ctx* g_main_ctx = NULL;
static ldp_process_infos g_pd_processes[2];

static char* child_executable_path(apr_pool_t* mem_pool, const char* platform_path, const char* child_name)
{
	const char* last_slash = strrchr(platform_path, '/');
	if (last_slash == NULL) {
		return apr_pstrcat(mem_pool, "./", child_name, NULL);
	}

	char* directory = apr_pstrndup(mem_pool, platform_path, (apr_size_t)(last_slash - platform_path + 1));
	return apr_pstrcat(mem_pool, directory, child_name, NULL);
}

static void check_ended_process(int exitcode, apr_exit_why_e exitwhy, apr_proc_t proc)
{
	for (int i = 0; i < g_main_ctx->PD_number; ++i) {
		if (g_pd_processes[i].proc.pid == proc.pid) {
			g_pd_processes[i].state = PROCESS_STOPPED;
			g_pd_processes[i].proc_exit_infos.pid = proc.pid;
			g_pd_processes[i].proc_exit_infos.exitcode = exitcode;
			g_pd_processes[i].proc_exit_infos.exitwhy = exitwhy;
			return;
		}
	}
}

static void stop_children(int sig)
{
	UNUSED(sig);

	for (int i = 0; i < g_main_ctx->PD_number; ++i) {
		if (g_pd_processes[i].proc.pid > 0) {
			kill(g_pd_processes[i].proc.pid, SIGTERM);
		}
	}
}

static void start_child_processes(ldp_Main_ctx* ctx, const char* platform_path)
{
	const char* names[2] = {
		"PD_sender_PD",
		"PD_receiver_PD",
	};

	for (int i = 0; i < ctx->PD_number; ++i) {
		apr_status_t ret;

		memset(&g_pd_processes[i], 0, sizeof(g_pd_processes[i]));
		g_pd_processes[i].prog_names = child_executable_path(ctx->mem_pool, platform_path, names[i]);
		g_pd_processes[i].prog_argv = apr_pcalloc(ctx->mem_pool, 2 * sizeof(char*));
		g_pd_processes[i].prog_argv[0] = g_pd_processes[i].prog_names;
		g_pd_processes[i].prog_argv[1] = NULL;
		g_pd_processes[i].state = PROCESS_STOPPED;

		ret = apr_procattr_create(&g_pd_processes[i].procattr, ctx->mem_pool);
		assert(ret == APR_SUCCESS);

		ret = apr_procattr_cmdtype_set(g_pd_processes[i].procattr, APR_PROGRAM_ENV);
		assert(ret == APR_SUCCESS);

		ret = apr_proc_create(&g_pd_processes[i].proc,
		                      g_pd_processes[i].prog_names,
		                      (const char**)g_pd_processes[i].prog_argv,
		                      NULL,
		                      g_pd_processes[i].procattr,
		                      ctx->mem_pool);
		assert(ret == APR_SUCCESS);

		g_pd_processes[i].state = PROCESS_RUNNING;
		printf("[MAIN] started %s pid=%d\n", names[i], g_pd_processes[i].proc.pid);
	}
}

static void wait_child_processes(ldp_Main_ctx* ctx)
{
	for (int i = 0; i < ctx->PD_number; ++i) {
		int exitcode = 0;
		apr_exit_why_e exitwhy = 0;

		if (g_pd_processes[i].proc.pid <= 0) {
			continue;
		}

		if (apr_proc_wait(&g_pd_processes[i].proc, &exitcode, &exitwhy, APR_WAIT) == APR_CHILD_DONE) {
			check_ended_process(exitcode, exitwhy, g_pd_processes[i].proc);
			printf("[MAIN] stopped %s pid=%d exitcode=%d exitwhy=%d\n",
			       g_pd_processes[i].prog_names,
			       g_pd_processes[i].proc_exit_infos.pid,
			       exitcode,
			       exitwhy);
		}
	}
}

int main(int argc, char** argv)
{
	ldp_Main_ctx ctx;
	ldp_interface_ctx interfaces[2];
	supervision_struct supervision;
	apr_status_t ret;

	UNUSED(argc);

	ret = apr_initialize();
	assert(ret == APR_SUCCESS);
	apr_pool_create(&g_mem_pool, NULL);

	memset(&ctx, 0, sizeof(ctx));
	memset(interfaces, 0, sizeof(interfaces));
	memset(&supervision, 0, sizeof(supervision));

	ctx.mem_pool = g_mem_pool;
	ctx.PD_number = 2;
	ctx.interface_ctx_array = interfaces;
	ctx.pd_processes_array = g_pd_processes;
	ctx.superv_tools = &supervision;
	ctx.superv_tools->path_to_launcher_t = "launcher.txt";
	g_main_ctx = &ctx;

	interfaces[0].info_r = (ldp_tcp_info){ MAIN_SENDER_PORT, MAIN_SENDER_ADDR, 1, false };
	interfaces[1].info_r = (ldp_tcp_info){ MAIN_RECEIVER_PORT, MAIN_RECEIVER_ADDR, 1, false };

	signal(SIGINT, stop_children);
	signal(SIGTERM, stop_children);

	start_child_processes(&ctx, argv[0]);
	ldp_start_father_server(&ctx, interfaces, 0);
	wait_child_processes(&ctx);

	apr_pool_destroy(g_mem_pool);
	apr_terminate();
	return 0;
}
