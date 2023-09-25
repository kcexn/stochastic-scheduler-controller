# Test Hello World

This test contains two hello world lua functions:

function main0 (args)
	return { ["msg0"]="Hello World!" }
end

function main1 (args)
	return { ["msg1"]="Hello World!" }
end

and emits an object (a map) of the following form:
{
	"main0": { "msg": "Hello World!" },
	"main1": {" msg": "Hello World!" }
}