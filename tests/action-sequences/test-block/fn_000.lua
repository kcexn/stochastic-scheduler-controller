local function sleep(n)
	os.execute("/usr/bin/sleep " .. tonumber(n))
end

local function msg0(args)
	sleep(30)
	return { ["msg0"]="Hello World!" }
end

return {
	main = msg0,
}
