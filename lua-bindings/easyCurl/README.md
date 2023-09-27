# Lua Bindings
## easyCurl
This is a C lua binding to libcurl to provide HTTP(s) functionality natively from inside of a lua script.
The binding currently ONLY supports the synchronous easy libcurl C API.

### Basic Usage:
```
curl = require("easyCurl")
hnd = curl.easy_init()
curl.easy_setopt(hnd, "URL", "http://...")
--- other options ---
ret = curl.easy_perform()
if (ret != 0) then
  --- handle errors ---
else 
  --- handle the success case ---
end
hnd = curl.easy_cleanup(hnd)
```
the curl handle hnd must always be cleaned up by libcurl after the request is complete. The return value of curl.easy_cleanup is a nil value, and should be used to
overwrite the handle (or else the handle holds a pointer to an invalid section of memory).

### Some Further Examples.
By default, libcurl reads from the STDIN FILE* and writes to the STDOUT FILE*. To directly read the response data from an HTTP request (e.g. a GET request), we 
construct new data buffers, and new FILE* streams to pass into libcurl.

Example 1: GET request.
```
curl = require("easyCurl")
hnd = curl.easy_init()
curl.easy_setopt(hnd, "URL", "http://...")
--- Set other options ---
data, stream = curl.new_writebuf()
curl.easy_setopt(hnd, "WRITEDATA", stream)
ret = curl.easy_perform()
if (ret != 0) then
  --- handle errors ---
else 
  --- handle the success case ---
end
hnd = curl.easy_cleanup(hnd)
data, stream = curl.close_writebuf(data, stream)
--- data now contains the GET body as a string ---
data
```

Similarly, the close_writebuf apicall returns a string value, and a nil value, and should be used to overwrite the original lightuserdata values or else lua will hold light user
data values with a pointer to invalid memory.