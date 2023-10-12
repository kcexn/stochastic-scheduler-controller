local cjson = require("cjson")
local main = require(arg[1])[arg[2]]

-- Notify the controller that we are ready for execution.
out = io.open("/proc/self/fd/3", "w")
out:write("\0")
out:flush()

-- Begin execution.
input = io.stdin:read()
if(input == nil) then
    out:write(
        cjson.encode({["error"]="Action input was empty."})
    )
    out:flush()
    os.exit(false)
end
params = cjson.decode(input)

res = main(params)

out:write(
    cjson.encode(res)
)
out:flush()


