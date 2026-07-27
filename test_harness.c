/* test_harness.c

So this is the actual debugging stuff, 
the way this works is it puts the gml_lua_bridge.c code up against 4 tests
to make sure everything is working as it's supposed to.

*/

#include <stdio.h>
#include <string.h>
#include "gml_lua_bridge.h"

static void run_step_loop_until_done(double ctx, double status) {
	int step = 0;
	while (status == 1) { /* 1 = waiting on game_call() */
		step++;
		printf("[harness] step %d: polling queue...\n", step);
		char* queue = gml_lua_poll_queue(ctx);
		if (queue[0] == '\0') {
			printf("[harness] queue empty but status said yielded -- BUG\n");
			break;
		}
		printf("[harness] queue contents:\n%s", queue);

		/* Parse the one line we expect: id|function|args */
		int request_id = 0;
		char func_name[128] = {0};
		char args_json[256] = {0};
		sscanf(queue, "%d|%127[^|]|%255[^\n]", &request_id, func_name, args_json);
		printf("[harness] dispatching call #%d -> %s(%s)\n", request_id, func_name, args_json);

		/* Simulate how GML would interact with the game properly */
		char result[64];
		snprintf(result, sizeof(result), "{\"ok\":true,\"echo\":\"%s\"}", args_json);

		status = gml_lua_resolve_call(ctx, (double)request_id, result);
		printf("[harness] resolve_call returned status=%.0f\n", status);
	}

	if (status == 0) {
		printf("[harness] script finished cleanly.\n\n");
	} else if (status < 0) {
		printf("[harness] script errored: %s\n\n", gml_lua_get_last_error(ctx));
	}
}

int main(void) {
	printf("Test 1: basic game_call yield/resume roundtrip\n");
	{
		double ctx = gml_lua_create_context();
		printf("[harness] created context %.0f\n", ctx);
		
		/* Here, we're just using made-up functions for the tests */
		const char* script =
			"print('script: starting')\n"
			"local result = game_call('SpawnObject', 'obj_coin,100,200')\n"
			"print('script: got result back: ' .. tostring(result))\n"
			"local result2 = game_call('GetPlayerHealth', '')\n"
			"print('script: second call result: ' .. tostring(result2))\n"
			"print('script: done')\n";

		double status = gml_lua_run_script(ctx, (char*)script);
		printf("[harness] run_script returned status=%.0f\n", status);
		run_step_loop_until_done(ctx, status);

		gml_lua_destroy_context(ctx);
	}

	printf("Test 2: sandbox actually blocks io/os\n");
	{
		double ctx = gml_lua_create_context();
		double status = gml_lua_run_script(ctx, (char*)"os.execute('echo SHOULD_NOT_RUN')");
		printf("[harness] status=%.0f error=%s\n\n", status, gml_lua_get_last_error(ctx));
		gml_lua_destroy_context(ctx);
	}

	printf("Test 3: instruction-budget watchdog stops an infinite loop\n");
	{
		double ctx = gml_lua_create_context();
		double status = gml_lua_run_script(ctx, (char*)"while true do local x = 1 + 1 end");
		printf("[harness] status=%.0f error=%s\n\n", status, gml_lua_get_last_error(ctx));
		gml_lua_destroy_context(ctx);
	}

	printf("Test 4: multiple contexts stay isolated\n");
	{
		double ctx_a = gml_lua_create_context();
		double ctx_b = gml_lua_create_context();
		gml_lua_run_script(ctx_a, (char*)"my_var = 111");
		gml_lua_run_script(ctx_b, (char*)"my_var = 222");
		double sa = gml_lua_call_global(ctx_a, (char*)"print", (char*)"unused");
		(void)sa;
		gml_lua_run_script(ctx_a, (char*)"print('ctx_a my_var = ' .. tostring(my_var))");
		gml_lua_run_script(ctx_b, (char*)"print('ctx_b my_var = ' .. tostring(my_var))");
		gml_lua_destroy_context(ctx_a);
		gml_lua_destroy_context(ctx_b);
	}

	return 0;
}
