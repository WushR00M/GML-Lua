# GML + Lua

GML + Lua is a tool for developers to implement Lua scripting in GameMaker projects, built upon the foundation of Lua 5.4.8.

## Files

- `gml_lua_bridge.h` - the public API surface. This is what you'll declare as extension functions inside GameMaker's IDE.
- `gml_lua_bridge.c` - the implementation
- `test_harness.c` - a debugging tool to use outside of GameMaker.

## How It Works

- Every script runs as its own **Lua coroutine**, not the main thread of the Lua state. That's what makes `game_call()` the only function scripts have to communicate directly with the game
- When a script calls `game_call("FunctionHere", "...")`, the C side parks that coroutine (so it survives garbage collection) and drops a request into a queue. A potential Lua persistant object's GML Step Event polls that queue once a frame, does the real work, and hands the result back resumes the script exactly where it left off.
- The sandbox never opens `io`, `os`, `debug`, or `package`, scripts physically cannot touch the filesystem or spawn processes, full stop, regardless of what the script tries.
- An instruction-count hook aborts any script that runs too long without yielding or returning, so a script with a bad loop can't freeze the game.

## Compiling

>[!WARNING]
>Currently, you cannot use any version of Lua above 5.4.8, it is **highly recommended** you use the source for Lua 5.4.7 or **Lua 5.4.8**.

### Linux
```
gcc -shared -O2 -fPIC -I path/to/lua-5.4/src \
gml_lua_bridge.c path/to/liblua54.a \
-o libgml_lua_bridge.so -lm -Wl,--no-undefined
```

### Windows
You'll want MSVC or MinGW. With MinGW, the equivalent is:
```
gcc -shared -O2 -I path\to\lua-5.4\src gml_lua_bridge.c liblua54.a \
    -o gml_lua_bridge.dll -Wl,--out-implib,gml_lua_bridge.lib
```
With MSVC, compile Lua as a static lib first (`lua54.lib`), then:
```
cl /LD gml_lua_bridge.c lua54.lib /I path\to\lua-5.4\src /Fe:gml_lua_bridge.dll
```
You can also cross-compile the .dll with Windows Subsystem for Linux:
```
x86_64-w64-mingw32-gcc -shared -O2 -I path/to/lua-5.4/src gml_lua_bridge.c path/to/lua-5.4/src/liblua54.a -o gml_lua_bridge.dll -lm -Wl,--out-implib,gml_lua_bridge.lib
```

### macOS
```
clang -shared -O2 -fPIC -I path/to/lua-5.4/src \
    gml_lua_bridge.c liblua54.a -o libgml_lua_bridge.dylib -lm
```

You'll need to replace `path/to/lua-5.4` with the path to your copy of Lua 5.4.8's source, as well as build `liblua54.a` before compiling anything.
You can also grab a pre-built copy online but be sure it is a copy of Lua 5.4.X.

**Do not use `onelua.c` directly without excluding `lua.c`/`luac.c`** as it auto-defines `MAKE_LUA` if you don't override it, which pulls in a `main()` that collides with anything else linking the library. This builds Lua from the individual source files instead, which sidesteps that trap entirely.

## How to Use

1. In the IDE: **Tools > Import Local Package**, or create a new Extension resource manually, pointing at the compiled `.dll`/`.dylib`/`.so` for each target platform.
2. Declare each function from `gml_lua_bridge.h` in the extension's function list, matching argument/return types:
   - `gml_lua_create_context` - Returns `double`, no arguments
   - `gml_lua_destroy_context` - Returns `double`, argument0 = `double`
   - `gml_lua_run_script` - Returns `double`, argument0 = `double`, argument1 = `string`
   - `gml_lua_call_global` - Returns `double`, argument0 = `double`, argument1 = `string`, argument2 = `string`
   - `gml_lua_poll_queue` - Returns `string`, argument0 = `string`
   - `gml_lua_resolve_call` - Returns `double`, argument0 = `double`, argument1 = `double`, argument2 = `string`
   - `gml_lua_get_last_error` - Returns `string`, argument0 = `double`
3. See `sample_usage.gml` for the GML-side controller object pattern (one persistent object per active mod context, polling the queue in its Step Event).
