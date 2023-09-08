# Test Hello World

This test contains two hello world lua functions that return after sleeping for 
10 and 20 seconds respectively.

local function sleep(n)
	os.execute("/usr/bin/sleep " .. tonumber(n))
end

local function msg0(args)
	sleep(10)
	return { ["msg0"]="Hello World!" }
end

local function msg1(args)
	sleep(20)
	return { ["msg1"]="Hello World!" }
end

return {
	main0 = msg0,
	main1 = msg1
}

The action-manifest looks like:
{
	"main1": {
		"depends": [],
		"file": "fn_000.lua"
	},
	"main0": {
		"depends": [],
		"file": "fn_000.lua"
	}
}

## Expected Result
and emits an object (a map) of the following form:
{
	"main0": { "msg0": "Hello World!" },
	"main1": { "msg1": "Hello World!" }
}

the time taken to emit this object should be 10 seconds, (not 5 seconds, not 15 seconds).
