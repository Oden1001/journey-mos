function PrintMeta(out) end
function DumpMetaSys() end
function DumpClassSizes() end

mos_boot = 0
mos_hooked = false
function Tick(game, gameTiming, input)
	Tick_game = game
	Tick_gameTiming = gameTiming
	Tick_input = input
	_G.game = game
	mos_boot = mos_boot + 1
	if mos_boot < 300 then
		return
	end
	-- Load once (every-frame dofile of heavy scripts was crashy)
	if mos_hooked then
		pcall(function()
			if MosFrame then MosFrame() end
		end)
		return
	end
	mos_hooked = true
	pcall(function()
		dofile("TickHook.lua")
	end)
end
