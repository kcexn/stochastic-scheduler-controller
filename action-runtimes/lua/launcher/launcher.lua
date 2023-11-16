local cjson = require("cjson")
local main = require(arg[1])[arg[2]]

-- Notify the controller that we are ready for execution.
local out = io.open("/proc/self/fd/3", "w")
out:write("\0")
out:flush()

-- Begin execution.
local input = io.stdin:read()
if(not input) then
    out:write(
        cjson.encode({["error"]="Action input was empty."})
    )
    out:flush()
    os.exit(false)
end
local params = cjson.decode(input)

local res = main(params)

-- For debugging purposes.
local action_path = os.getenv("__OW_ACTIONS")
if(action_path) then
    local manifest, emsg, ec = io.open(action_path .. "/action-manifest.json")
    if(manifest) then
        manifest:close()
        local ow_activation_id = os.getenv("__OW_ACTIVATION_ID")
        if(ow_activation_id) then
            res["activation_id"] = ow_activation_id
        end
    end
end

local output = cjson.encode(res)
out:write(
    output
)
out:flush()


