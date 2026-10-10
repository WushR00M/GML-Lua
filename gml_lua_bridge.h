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

GML_EXPORT double gml_lua_create_context(void);
GML_EXPORT double gml_lua_destroy_context(double context_id);
GML_EXPORT double gml_lua_run_script(double context_id, char* source);
GML_EXPORT double gml_lua_call_global(double context_id, char* func_name, char* args_json);
GML_EXPORT char* gml_lua_poll_queue(double context_id);
GML_EXPORT double gml_lua_resolve_call(double context_id, double request_id, char* result_json);
GML_EXPORT char* gml_lua_get_last_error(double context_id);

#ifdef __cplusplus
}
#endif

#endif
