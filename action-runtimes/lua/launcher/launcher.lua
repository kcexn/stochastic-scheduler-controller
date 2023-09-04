local cjson = require("cjson")
local main = require(arg[1]).main

-- Notify the controller that we are ready for execution.
io.stdout:write("\0")
io.stdout:flush()

-- Begin execution.
params = cjson.decode(
    io.stdin:read()
)

res = main(params)

io.stdout:write(
    cjson.encode(res)
)
io.stdout:flush()


