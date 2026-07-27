/* sample_usage2.gml

I have another example of how you can use this!

Here, I wrote a function that scans a folder in the game_save_id directory titled "mods",
then runs a function to read each file, create a Lua controller object for each file,
add each line to each file's respective strings, and then let the controller object execute
the Lua code.

*/

// Functions (there's two global variables used here, global.mods_loaded and global.mod_count)

function load_lua_script(filename){
	var handler = instance_create_depth(0, 0, -1, <lua object name here>);
	handler.file = filename;
	handler.run_script = true;
	show_debug_message("Lua running " + string(filename));
}

function load_mods(scan_mode = false) {
	var _list = ds_list_create();
	
	var _dir = game_save_id + "mods/";
	var _file = file_find_first(_dir + "*.lua", 0);
    while (_file != "") {
        if (filename_ext(_file) == ".lua") {
			var _path = _dir + _file;
            if (file_exists(_path)) {
				if scan_mode == true {
					ds_list_add(_list, {
                        filename : _file
                    });
				} else {
					ds_list_add(_list, {
                        filename : _file
                    });
					show_debug_message("Loading Lua script: " + string(_path));
					load_lua_script(_path);
				}
			}
		}
		
		_file = file_find_next();
	}
	
	file_find_close();
	global.mods_loaded = true;
	global.mod_count = ds_list_size(_list);
	show_debug_message("Mods found: " + string(global.mod_count));
	return _list;
}

// The next goes in the actual Lua controller object
// Create Event:

context_id = gml_lua_create_context();
file = -1;

run_script = false;
script_ran = false;

// Step Event:

if run_script == true && script_ran == false {
	var _file = file_text_open_read(file);
	var _line;
	var _script = "";
	var _script_source = "";

    while (!file_text_eof(_file)) {
        _script += file_text_read_string(_file) + "\n";
        file_text_readln(_file);
    }

	file_text_close(_file);
	
	_script_source = _script;
	run_status = gml_lua_run_script(context_id, _script_source);
	
	global.mods_running += 1;
	if (run_status == -1 || run_status == -2) {
		show_debug_message("Lua error found: " + gml_lua_get_last_error(context_id));
	}
	
	script_ran = true;
}

if (run_status == 1) {
    var _queue = gml_lua_poll_queue(context_id);

    if (_queue != "") {
        var _lines = string_split(_queue, "\n");
        for (var i = 0; i < array_length(_lines); i++) {
            var _line = _lines[i];
            if (_line == "") continue;

            var _parts    = string_split(_line, "|");
            var _req_id   = real(_parts[0]);
            var _func     = _parts[1];
            var _args     = array_length(_parts) > 2 ? _parts[2] : "";

            var _result = "";

            switch (_func) {
				        // write functions here

                default:
                    _result = "error:unknown_function";
				break;
            }

            run_status = gml_lua_resolve_call(context_id, _req_id, _result);
        }
    }
}

if (run_status == -2) {
    show_debug_message("Lua runtime error: " + string(gml_lua_get_last_error(context_id)));
}

// ...and finally, the Clean-Up / Destory Event (optional):

gml_lua_destroy_context(context_id);
