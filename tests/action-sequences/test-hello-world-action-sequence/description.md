# Test Hello World

This test contains two hello world lua functions:

function main0 (args)
	return { ["msg0"]="Hello World!" }
end

function main1 (args)
	return { ["main0"]=args["main0"], ["msg1"]="Hello World!" }
end

where the output of main1 depends on the input of main0.

such that the action-manifest looks like:
{
	"main1": {
		"depends": ["main0"],
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
	"main1": { "main0":{"msg0":"Hello World!"}, "msg1": "Hello World!" }
}