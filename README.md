# GMLua

GMLua is a tool for GameMaker developers to implement Lua scripting in their games, built upon the foundation of Lua 5.4.8.

## Files

- `gml_lua_bridge.h` - the API service
- `gml_lua_bridge.c` - the implementation
- `test_harness.c` - a debugging tool to use outside of GameMaker

## How It Works

- Every script runs as its own **Lua coroutine**, not the main thread of the Lua state. That's what makes `game_call()` the only function scripts have to communicate directly with the game.
- When a script calls `game_call("functionName", "...")`, the C side parks that co-routine (so it survives GameMaker's garbage collection) and drops a request into a queue. From there, you can set up an in-game persistent object's Step Event to poll that queue once a frame and resume the script exactly where it left off.

## Compiling

>[!IMPORTANT]
>Currently, you cannot use any version of Lua above 5.4.8, it is **highly recommended** you use the source for Lua 5.4.7 or **Lua 5.4.8**.

**You need gcc to compile this! You can use `sudo apt install build-essentials` on Ubuntu with WSL, for example to quickly install it.**

### Windows
[Windows Subsystem for Linux](https://github.com/microsoft/WSL) is required to run the following:
```
x86_64-w64-mingw32-gcc -shared -O2 -I path/to/lua-5.4/src gml_lua_bridge.c path/to/lua-5.4/src/liblua54.a -o gml_lua_bridge.dll -lm -Wl,--out-implib,gml_lua_bridge.lib
```

>[!NOTE]
>You'll need to replace `path/to/lua-5.4` with the path to your copy of Lua 5.4.8's source, as well as build `liblua54.a` before compiling anything.

>[!WARNING]
>**Do not use `onelua.c` directly without excluding `lua.c`/`luac.c`** as it auto-defines `MAKE_LUA` if you don't override it, which pulls in a `main()` that collides with anything else linking the library. This builds Lua from the individual source files instead, which sidesteps that trap entirely.

## How to Use

### Pre-Compiled
1. In the IDE: **Tools > Import Local Package**
2. Import the extension into your project!

### Built from Source
1. In the IDE: create a new Extension resource manually, pointing at the compiled `.dll`/`.dylib`/`.so` for each target platform.
2. Declare each function from `gml_lua_bridge.h` in the extension's function list, matching argument/return types:
   - `gml_lua_create_context` - Returns `double`, no arguments
   - `gml_lua_destroy_context` - Returns `double`, argument0 = `double`
   - `gml_lua_run_script` - Returns `double`, argument0 = `double`, argument1 = `string`
   - `gml_lua_call_global` - Returns `double`, argument0 = `double`, argument1 = `string`, argument2 = `string`
   - `gml_lua_poll_queue` - Returns `string`, argument0 = `string`
   - `gml_lua_resolve_call` - Returns `double`, argument0 = `double`, argument1 = `double`, argument2 = `string`
   - `gml_lua_get_last_error` - Returns `string`, argument0 = `double`
3. See `sample_usage.gml` for the GML-side controller object pattern (one persistent object per active mod context, polling the queue in its Step Event).

### GameMaker Side
To write functions that use `game_call`, you can add them to the list of cases in the Step Event of your Lua controller object (read `sample_usage.gml` and `sample_usage2.gml` for more info):
```
case "SetCurrentRoom":
    var _parts = string_split(_args, ",");
    var _room = _parts[0];
    _room = asset_get_index(_room);

    if room_exists(_room) {
        room_goto(_room);
        _result = room_get_name(_room);
    } else {
        _result = -1;	
    }
break;
```

`_result` is what gets sent back to Lua, you can assign the value to variables later in Lua

## Other Examples

If you want an example of how it's used in a proper game, I have a personal project, "Sunrise", that includes this library, [complete with documentation on its own custom functions.](https://wushroomstudios.com/docs/#/)
