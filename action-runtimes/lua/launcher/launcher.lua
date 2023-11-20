local cjson = require("cjson")
--print("launcher:", arg[1], arg[2])
local main = require(arg[1])[arg[2]]

local action_path = os.getenv("__OW_ACTIONS")
local concurrency = 1
local manifest_exists = false
if(action_path) then
    local manifest, emsg, ec = io.open(action_path .. "/action-manifest.json")
    if(manifest) then
        manifest_exists = true
        local action_manifest = cjson.decode(manifest:read("a"))
        if(action_manifest["__OW_NUM_CONCURRENCY"]) then
            concurrency = action_manifest["__OW_NUM_CONCURRENCY"]
        end
        manifest:close()
    end
end


-- Notify the controller that we are ready for execution.
local out = io.open("/proc/self/fd/3", "w")
out:write("\0")
out:flush()

-- Begin execution.
local input = io.stdin:read()
if(not input) then
    if(manifest_exists) then
        return nil
    else
        out:write(cjson.encode({["error"]="Action input was empty."}))
        out:flush()
        os.exit(false)
    end
end
--print("launcher:", input)
local params = cjson.decode(input)

local res = main(params)

-- for debugging
local ow_activation_id = os.getenv("__OW_ACTIVATION_ID")
if(ow_activation_id and manifest_exists) then
    res["activation_id"] = ow_activation_id
end


if(res["error"] and manifest_exists) then
    return nil
end
local output = cjson.encode(res)
out:write(
    output
)
out:flush()


