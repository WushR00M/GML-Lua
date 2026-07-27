/* gml_lua_bridge.h

...
I don't got anything to say here lol
If you want to add more functions on the GML side then this is where you'd do the first step for that

*/

#ifndef GML_LUA_BRIDGE_H
#define GML_LUA_BRIDGE_H

#ifdef _WIN32
  #define GML_EXPORT __declspec(dllexport)
#else
  #define GML_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Returns context_id (>=0) or -1 on failure (context table full / lua_newstate failed). */
GML_EXPORT double gml_lua_create_context(void);

/* Tears down a context and frees all resources */
GML_EXPORT double gml_lua_destroy_context(double context_id);

/* Loads and runs `source` as a new coroutine under the given context. Returns:
0: ran to completion with no game_call() yields pending
1: yielded on a game_call(); check gml_lua_poll_queue()
-1: compile error (see gml_lua_get_last_error)
-2: runtime error (see gml_lua_get_last_error)
*/
GML_EXPORT double gml_lua_run_script(double context_id, char* source);

/* Invokes a named global Lua function (e.g. a registered "on_step" callback) as a new coroutine */
GML_EXPORT double gml_lua_call_global(double context_id, char* func_name, char* args_json);

/* Drains all newly-queued game_call() requests for this context since the last poll.
Returns a serialized string, one request per line: "id|function|args"
Returns "" (empty string) if nothing is pending. */
GML_EXPORT char* gml_lua_poll_queue(double context_id);

/* Delivers a result back to a suspended coroutine and resumes it. Returns same status codes as gml_lua_run_script. */
GML_EXPORT double gml_lua_resolve_call(double context_id, double request_id, char* result_json);

/* Returns the last compile/runtime error message for this context, or "" if none. */
GML_EXPORT char* gml_lua_get_last_error(double context_id);

#ifdef __cplusplus
}
#endif

#endif
