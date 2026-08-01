/* sample_usage.gml

This bit of code is an example of how you would use the functions within your
GameMaker project, hopefully can get around to cleaning things up :P

*/
// The following goes in the "Create" event.

context_id = gml_lua_create_context();

var _script_source = @"
	print('hello from lua')
	game_call('SetGravityMultiplier', string.format('0'))
"; // This code will print "hello from lua" upon game boot and then set a variable (grav_multiplier) in an object (obj_player).

run_status = gml_lua_run_script(context_id, _script_source);

if (run_status == -1 || run_status == -2) {
	show_debug_message("Lua error: " + gml_lua_get_last_error(context_id));
}

// ...and the rest goes in the Step event, in a persistant object

if (run_status == 1) { // 1 = yielded, waiting on game_call
	var _queue = gml_lua_poll_queue(context_id);

	if (_queue != "") {
		var _lines = string_split(_queue, "\n");
		for (var i = 0; i < array_length(_lines); i++) {
			var _line = _lines[i];
			if (_line == "") continue;

			var _parts = string_split(_line, "|");
			var _req_id = real(_parts[0]);
			var _func = _parts[1];
			var _args = array_length(_parts) > 2 ? _parts[2] : "";

			var _result = "";

			// This is where you write custom functions for game_call
			switch (_func) {
				case "MyCustomFunction":
					var _sg_parts = string_split(_args, ",");
					var _argu0 = real(_sg_parts[0]); // it MUST be in range to use it (no _sg_parts[1] because there's not an extra argument)
					// From there, you can interact with all of your arguments normally, via "_sg_parts[#] going into each of their own variables"
					// Here, I just made it return the argument you just passed in Lua
					_result = string(_argu0);
					break;

				default:
					// Error handling against 
					_result = "error:unknown_function";
					break;
			}

			run_status = gml_lua_resolve_call(context_id, _req_id, _result);
		}
	}
}

if (run_status == -2) {
	show_debug_message("Lua runtime error: " + gml_lua_get_last_error(context_id));
	// placeholder ^^^ go put something here yourself :P
}


// Optional but a good idea, you should probably put this in the Clean-Up / Destory event :)
gml_lua_destroy_context(context_id);
