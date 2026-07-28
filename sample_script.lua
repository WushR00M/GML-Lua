-- sample_script.lua
-- This is an example in Lua that the game can load (learn HOW to load it via "sample_usage2.gml" and "sample_usage.gml")
-- It prints the output at game boot and game boot only; "PrintToLog" would be a custom function, however you can use native Lua print()

radianVal = math.rad(math.pi / 2) -- Convert pi / 2 into radians
game_call('PrintToLog', string.format(tostring(radianVal))) -- Spit out the output
