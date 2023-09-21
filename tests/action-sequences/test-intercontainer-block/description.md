# Test Block.
## Description
This test blocks the application for 30s before unblocking the context and completing with a "Hello World!" echo response.
The test is designed as a helper test to impleemnt the SCTP peer interconnects.

## Function.

local function sleep(n)
	os.execute("/usr/bin/sleep " .. tonumber(n))
end

local function msg0(args)
	sleep(30)
	return { ["msg0"]="Hello World!" }
end

return {
	main = msg0,
}

## Test Sequence.
1. Hit the run route with the run-data.json
2. Using nc we can simulate a peer context.

## Peer Context Call Flow
1. Connect:
```
PUT /run HTTP/1.1\r\n
Content-Type: application/json\r\n
\r\n
5D\r\n
[{"execution_context":{"uuid":"a70ea480860c45e19a5385c68188d1ff","peers":["127.0.0.1:5200"]}}\r\n
```
2. Initial Reply from Server:
```
HTTP/1.1 201 Created\r\n
Content-Type: application/json\r\n
\r\n
2A\r\n
[{"peers":["127.0.0.1:31001"],"result":{}}\r\n
```

3. Subsequent Client State Update.
```
2C\r\n
,{"result":{"main":{"msg0":"Hello World!"}}}\r\n
```
