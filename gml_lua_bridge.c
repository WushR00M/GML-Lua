/* gml_lua_bridge.c

Here it is! The big one, though for the information about how this works, go to the README
as I'm lazy to write everything AGAIN in the comments of a C file :D

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "gml_lua_bridge.h"

#define MAX_CONTEXTS		32
#define MAX_PENDING_CALLS   256
#define INSTRUCTION_BUDGET  1000000 /* Lua instructions between hook checks before we abort a "runaway" script. */
#define ERROR_BUF_SIZE	  2048
#define QUEUE_BUF_SIZE	  (MAX_PENDING_CALLS * 320)

static int STATE_TO_CONTEXT_KEY; /* address used as a registry key */

typedef struct {
	int	request_id;
	int	context_id;
	char func_name[128];
	char args_json[512];
	int	coro_ref; /* luaL_ref into LUA_REGISTRYINDEX */
	int	dispatched; /* already handed to GML via poll? */
	int	in_use;
} PendingCall;

typedef struct {
	lua_State* L;
	int active;
	char last_error[ERROR_BUF_SIZE];
} LuaContext;

static LuaContext g_contexts[MAX_CONTEXTS];
static PendingCall g_queue[MAX_PENDING_CALLS];
static int g_next_request_id = 1;

static char g_scratch_str[QUEUE_BUF_SIZE];
static char g_scratch_err[ERROR_BUF_SIZE];

static void register_state_context(lua_State* L, lua_State* thread, int context_id) {
	/* table[lightuserdata(thread)] = context_id, stored in the "registry" */
	lua_pushlightuserdata(L, &STATE_TO_CONTEXT_KEY);
	lua_gettable(L, LUA_REGISTRYINDEX);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushlightuserdata(L, &STATE_TO_CONTEXT_KEY);
		lua_pushvalue(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
	lua_pushlightuserdata(L, thread);
	lua_pushinteger(L, context_id);
	lua_settable(L, -3);
	lua_pop(L, 1);
}

static int lookup_state_context(lua_State* L, lua_State* thread) {
	int result = -1;
	lua_pushlightuserdata(L, &STATE_TO_CONTEXT_KEY);
	lua_gettable(L, LUA_REGISTRYINDEX);
	if (lua_istable(L, -1)) {
		lua_pushlightuserdata(L, thread);
		lua_gettable(L, -2);
		if (lua_isinteger(L, -1)) {
			result = (int)lua_tointeger(L, -1);
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return result;
}

static void instruction_hook(lua_State* L, lua_Debug* ar) {
	(void)ar;
	luaL_error(L, "script exceeded instruction budget (possible infinite loop)");
}

static void arm_watchdog(lua_State* co) {
	lua_sethook(co, instruction_hook, LUA_MASKCOUNT, INSTRUCTION_BUDGET);
}

static void open_safe_libs(lua_State* L) {
	static const luaL_Reg safe_libs[] = {
		{ "_G",		luaopen_base },
		{ LUA_TABLIBNAME,  luaopen_table },
		{ LUA_STRLIBNAME,  luaopen_string },
		{ LUA_MATHLIBNAME, luaopen_math },
		{ LUA_UTF8LIBNAME, luaopen_utf8 },
		{ LUA_COLIBNAME,   luaopen_coroutine },
		{ NULL, NULL }
	};
	for (const luaL_Reg* lib = safe_libs; lib->func; lib++) {
		luaL_requiref(L, lib->name, lib->func, 1);
		lua_pop(L, 1);
	}

	lua_pushnil(L); lua_setglobal(L, "dofile");
	lua_pushnil(L); lua_setglobal(L, "loadfile");
	lua_pushnil(L); lua_setglobal(L, "collectgarbage"); /* avoid gc-timing based side channels for now */
}

/* game_call */
static int lua_game_call(lua_State* co) {
	const char* func_name = luaL_checkstring(co, 1);
	const char* args_json = luaL_optstring(co, 2, "");

	/* Find owning context via the main registry */
	int context_id = lookup_state_context(co, co);
	if (context_id < 0) {
		return luaL_error(co, "game_call() invoked outside of a tracked script context");
	}

	int slot = -1;
	for (int i = 0; i < MAX_PENDING_CALLS; i++) {
		if (!g_queue[i].in_use) { slot = i; break; } /* Find a free queue slot */
	}
	if (slot < 0) {
		return luaL_error(co, "game_call queue full -- too many pending calls");
	}

	/* Park this coroutine so it isn't garbage collected while suspended. */
	lua_pushthread(co);	/* [.., thread] */
	int coro_ref = luaL_ref(co, LUA_REGISTRYINDEX); /* pops thread  */

	PendingCall* pc = &g_queue[slot];
	pc->request_id  = g_next_request_id++;
	pc->context_id  = context_id;
	pc->coro_ref	= coro_ref;
	pc->dispatched  = 0;
	pc->in_use	  = 1;
	snprintf(pc->func_name, sizeof(pc->func_name), "%s", func_name);
	snprintf(pc->args_json, sizeof(pc->args_json), "%s", args_json);

	return lua_yield(co, 0);
}

static void set_error(LuaContext* ctx, const char* msg) {
	snprintf(ctx->last_error, sizeof(ctx->last_error), "%s", msg ? msg : "");
}

double gml_lua_create_context(void) {
	for (int i = 0; i < MAX_CONTEXTS; i++) {
		if (!g_contexts[i].active) {
			lua_State* L = luaL_newstate();
			if (!L) return -1;
			open_safe_libs(L);

			lua_pushcfunction(L, lua_game_call);
			lua_setglobal(L, "game_call");

			g_contexts[i].L = L;
			g_contexts[i].active = 1;
			g_contexts[i].last_error[0] = '\0';

			register_state_context(L, L, i);

			return (double)i;
		}
	}
	return -1;
}

double gml_lua_destroy_context(double context_id_d) {
	int context_id = (int)context_id_d;
	if (context_id < 0 || context_id >= MAX_CONTEXTS) return -1;
	LuaContext* ctx = &g_contexts[context_id];
	if (!ctx->active) return -1;

	for (int i = 0; i < MAX_PENDING_CALLS; i++) {
		if (g_queue[i].in_use && g_queue[i].context_id == context_id) {
			luaL_unref(ctx->L, LUA_REGISTRYINDEX, g_queue[i].coro_ref);
			g_queue[i].in_use = 0;
		}
	}

	lua_close(ctx->L);
	ctx->L = NULL;
	ctx->active = 0;
	return 0;
}

static double resume_and_report(LuaContext* ctx, lua_State* co, int nargs) {
	arm_watchdog(co);

	int nres = 0;
	int status = lua_resume(co, ctx->L, nargs, &nres);

	switch (status) {
		case LUA_OK:
			return 0; /* finished cleanly */
		case LUA_YIELD:
			return 1; /* parked on a game_call(), a queue entry now exists */
		default: {
			const char* msg = lua_isstring(co, -1) ? lua_tostring(co, -1) : "(non-string error)";
			set_error(ctx, msg);
			return -2; /* runtime error */
		}
	}
}

double gml_lua_run_script(double context_id_d, char* source) {
	int context_id = (int)context_id_d;
	if (context_id < 0 || context_id >= MAX_CONTEXTS) return -1;
	LuaContext* ctx = &g_contexts[context_id];
	if (!ctx->active) return -1;

	lua_State* co = lua_newthread(ctx->L);
	register_state_context(ctx->L, co, context_id);
	arm_watchdog(co);

	if (luaL_loadstring(co, source) != LUA_OK) {
		set_error(ctx, lua_tostring(co, -1));
		/* Return the error and let it get garbage collected */
		return -1;
	}

	return resume_and_report(ctx, co, 0);
}

double gml_lua_call_global(double context_id_d, char* func_name, char* args_json) {
	int context_id = (int)context_id_d;
	if (context_id < 0 || context_id >= MAX_CONTEXTS) return -1;
	LuaContext* ctx = &g_contexts[context_id];
	if (!ctx->active) return -1;

	lua_State* co = lua_newthread(ctx->L);
	register_state_context(ctx->L, co, context_id);
	arm_watchdog(co);

	lua_getglobal(co, func_name);
	if (!lua_isfunction(co, -1)) {
		set_error(ctx, "global function not found");
		return -1;
	}
	lua_pushstring(co, args_json ? args_json : "");

	return resume_and_report(ctx, co, 1);
}

double gml_lua_resolve_call(double context_id_d, double request_id_d, char* result_json) {
	int context_id = (int)context_id_d;
	int request_id = (int)request_id_d;
	if (context_id < 0 || context_id >= MAX_CONTEXTS) return -1;
	LuaContext* ctx = &g_contexts[context_id];
	if (!ctx->active) return -1;

	PendingCall* pc = NULL;
	for (int i = 0; i < MAX_PENDING_CALLS; i++) {
		if (g_queue[i].in_use && g_queue[i].request_id == request_id
			&& g_queue[i].context_id == context_id) {
			pc = &g_queue[i];
			break;
		}
	}
	if (!pc) return -1;

	lua_rawgeti(ctx->L, LUA_REGISTRYINDEX, pc->coro_ref);
	lua_State* co = lua_tothread(ctx->L, -1);
	lua_pop(ctx->L, 1);

	luaL_unref(ctx->L, LUA_REGISTRYINDEX, pc->coro_ref);
	pc->in_use = 0;

	if (!co) return -1;

	lua_pushstring(co, result_json ? result_json : "");
	return resume_and_report(ctx, co, 1);
}

char* gml_lua_poll_queue(double context_id_d) {
	int context_id = (int)context_id_d;
	g_scratch_str[0] = '\0';
	if (context_id < 0 || context_id >= MAX_CONTEXTS) return g_scratch_str;

	size_t used = 0;
	for (int i = 0; i < MAX_PENDING_CALLS; i++) {
		PendingCall* pc = &g_queue[i];
		if (pc->in_use && pc->context_id == context_id && !pc->dispatched) {
			pc->dispatched = 1;
			int written = snprintf(g_scratch_str + used, QUEUE_BUF_SIZE - used, "%d|%s|%s\n", pc->request_id, pc->func_name, pc->args_json);
			if (written < 0 || (size_t)written >= QUEUE_BUF_SIZE - used) break;
			used += (size_t)written;
		}
	}
	return g_scratch_str;
}

char* gml_lua_get_last_error(double context_id_d) {
	int context_id = (int)context_id_d;
	g_scratch_err[0] = '\0';
	if (context_id < 0 || context_id >= MAX_CONTEXTS) return g_scratch_err;
	snprintf(g_scratch_err, sizeof(g_scratch_err), "%s", g_contexts[context_id].last_error);
	return g_scratch_err;
}
