# Test the Action MAnifest with Default Entry Points.

This test checks that the action-manifest parser parses the data in the action manifest correctly under trivial conditions.

Construct an action-manifest.json file with all default settings, i.e.;
{
    "main": {
        "depends": [],
        "file": "main.lua"
    }
}

provide a main program in main.lua
return {
  main = function (args) return { ["msg"]="Hello World!" } end
}

That has as an entrypoint, the key "main".

## Expectation.

the expected result is a return value of:

{"main": {msg":"Hello World!"}}