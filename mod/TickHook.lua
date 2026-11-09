-- Bootstrap once. MosFrame() is called every frame afterward.
pcall(function()
	dofile("StartLua.lua")
end)

pcall(function()
	dofile("GameTick.lua")
end)

function MosFrame()
	pcall(function()
		if JourneyMos_Tick then
			JourneyMos_Tick()
		end
	end)
end
