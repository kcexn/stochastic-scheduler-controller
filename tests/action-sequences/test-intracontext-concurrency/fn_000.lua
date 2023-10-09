local function sleep(n)
	os.execute("/usr/bin/sleep " .. tonumber(n))
end

local function msg0(args)
	sleep(10)
	return { ["msg0"]="Hello World!" }
end

local function msg1(args)
	sleep(20)
	return { ["msg1"]="Hello World!" }
end

return {
	main0 = msg0,
	main1 = msg1
}
