# Test Modified Main.

The Openwhisk platform allows for the definition of a custom main in the /init route.
{
    "value": {
        ...
        "main": <JSON.value>,
        ...
    }
}

The test code is a hello world lua function.

return {
  <Json.value> = function (args) return { ["msg"]="Hello World!" } end
}

## Expected Return value:
{"msg":"Hello World!"}