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

## The CallFlow

1. Container 1 is initialized with the master execution context.
2. Container 2 is initialized with a secondary execution context.
3. Container 2 connects to contaienr 1 with an HTTP chunked PUT request to the /run route.
```
PUT /run HTTP/1.1\r\n
Content-Type: application/json\r\n
\r\n
5D\r\n
[{"execution_context":{"uuid":"a70ea480860c45e19a5385c68188d1ff","peers":["127.0.0.1:5200"]}}\r\n
```
4. Container 1 replies to Container 2 with an Http chunked CREATED response that contains an updated list
of peers, and the current state of the program.
```
HTTP/1.1 201 Created\r\n
Content-Type: application/json\r\n
\r\n
2A\r\n
[{"peers":["127.0.0.1:31001"],"result":{}}\r\n
```
5. Container 2 replies to Container 1 with a state update to the program.
```
2D\r\n
,{"result":{"main0":{"msg0":"Hello World!"}}}\r\n
```
6. Container 1 replies to Container 2 to notify it that the program has terminated by terminating the stream.
```
1\r\n
]\r\n
0\r\n
\r\n
```
7. Container 2 treats the stream termination as a directive and truncates the stream transmission.

## Expected Result
and emits an object (a map) of the following form:
{
	"main0": { "msg0": "Hello World!" },
	"main1": { "msg1": "Hello World!" }
}

the time taken to emit this object should be 10 seconds, (not 5 seconds, not 15 seconds).
