local cjson = require("cjson")
local main = require(arg[1])[arg[2]]

-- Notify the controller that we are ready for execution.
io.stdout:write("\0")
io.stdout:flush()

-- Begin execution.
input = io.stdin:read()
params = cjson.decode(input)

res = main(params)

io.stdout:write(
    cjson.encode(res)
)
io.stdout:flush()


