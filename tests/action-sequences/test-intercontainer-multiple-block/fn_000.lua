local function sleep(n)
	os.execute("/usr/bin/sleep " .. tonumber(n))
end

local function msg0(args)
	sleep(5)
	return {["msg0"]="Hello World!"}
end

local function msg1(args)
	sleep(30)
	return {["msg1"]="Hello World!"}
end

return {
	main0 = msg0,
	main1 = msg1
}
