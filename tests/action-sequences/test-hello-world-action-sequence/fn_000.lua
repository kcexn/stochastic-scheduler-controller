return {
	main0 = function (args) return { ["msg0"]="Hello World!" } end,
	main1 = function (args) return { ["main0"]=args["main0"], ["msg1"]="Hello World!"} end
}