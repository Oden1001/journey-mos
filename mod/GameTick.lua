-- Loaded once. JourneyMos_Tick() runs every frame via MosFrame.

if ModState == nil then
	ModState = {
		bootstrapped = false,
		setVelOk = nil,
		fillCounter = 0,
		cmdSeq = 0,
		lastGameSpeed = nil,
		debugCamActive = nil,
		forceGlowActive = nil,
		deathMatch = false,
		statusTimer = 0,
		readyFrames = 0,
		libsLoaded = false,
		debugKeysHooked = false,
		farmTimer = 0,
		dudeTuneKey = nil,
		dudeTuneLogged = false,
		animBase = nil,
		windAssistOn = false,
		configReloadAt = 0,
	}
end

local function tryDofile(path)
	return pcall(dofile, path)
end

local function reloadConfig()
	-- Preserve runtime toggles that older config.lua files may omit.
	local ok = tryDofile("config.lua")
	if not ok then ok = tryDofile("mod/config.lua") end
	if ModConfig == nil then
		ModConfig = {
			infiniteEnergy = true,
			speedBoost = true,
			speedMult = 3.0,
			flyBoost = true,
			flyBoostMult = 2.0,
			gameSpeed = 1.0,
			debugCam = false,
			forceGlow = false,
			disableAutoLobby = true,
			skipCinematic = false,
			autoFarm = false,
			farmInterval = 12,
			meetEverywhere = false,
			noWind = false,
			showCompanionName = true,
			attractGlyphs = false,
		}
	end
	if ModConfig.autoFarm == nil then ModConfig.autoFarm = false end
	if ModConfig.farmInterval == nil then ModConfig.farmInterval = 12 end
	if ModConfig.skipCinematic == nil then ModConfig.skipCinematic = false end
	if ModConfig.meetEverywhere == nil then ModConfig.meetEverywhere = false end
	if ModConfig.noWind == nil then ModConfig.noWind = false end
	if ModConfig.showCompanionName == nil then ModConfig.showCompanionName = true end
	if ModConfig.attractGlyphs == nil then ModConfig.attractGlyphs = false end
	-- One-shot features reload when menu syncs config (not every frame).
	if ModConfig._mosReloadFeatures then
		ModConfig._mosReloadFeatures = nil
		ModState.libsLoaded = false
		ModFeatures = nil
	end
end

local function ensureLibs()
	-- Load once. To pick up script edits: toggle any menu option (reloads config)
	-- or restart the game. Periodic dofile was hitching the framerate.
	if ModState.libsLoaded and ModFeatures then return end
	local ok = tryDofile("features.lua")
	if not ok then tryDofile("mod/features.lua") end
	ModState.libsLoaded = (ModFeatures ~= nil)
end

local function getGame()
	return game or Tick_game
end

local function playerReady(g)
	local ok, d = pcall(function()
		return g:playerBarn():GetLocalDude()
	end)
	return ok and d ~= nil
end

local function bootstrapOnce(g)
	if ModState.bootstrapped then return end
	if not playerReady(g) then return end
	ModState.bootstrapped = true
	pcall(function()
		if ModConfig.disableAutoLobby and Vars and Vars.Lobby and Vars.Lobby.kUseAutoLobby then
			Vars.Lobby.kUseAutoLobby(false)
		end
	end)
	pcall(function()
		if Vars and Vars.Game and Vars.Game.cheatsEnabled then
			Vars.Game.cheatsEnabled(true)
		end
	end)
	pcall(function()
		if CONFIG == nil or CONFIG == "Gold" then
			CONFIG = "Debug"
		end
	end)
	pcall(function() ModFeatures.hookDebugKeys() end)
	print("[JourneyMos] ready")
	ModNotify("MOS 就绪 · F8 菜单", 2.0)
end

function JourneyMos_Tick()
	local ok, err = pcall(function()
		ModState.readyFrames = (ModState.readyFrames or 0) + 1

		ensureLibs()

		-- Config is written by the native menu to disk. Poll ~3x/sec — NEVER every frame
		-- (dofile every frame was hitching / freezing when the menu synced).
		if (ModState.readyFrames % 20) == 0 then
			reloadConfig()
		end

		local g = getGame()
		if g == nil or ModFeatures == nil then return end
		if ModState.readyFrames < 45 then return end

		bootstrapOnce(g)
		pcall(function() ModFeatures.consumeCommands(g) end)
		pcall(function() ModFeatures.applySkipCinematic(g) end)
		pcall(function() ModFeatures.applyGameSpeed(g) end)
		pcall(function() ModFeatures.applyAutoFarm(g) end)

		if not playerReady(g) then return end

		pcall(function() ModFeatures.tickForceChirp(g) end)
		pcall(function() ModFeatures.tickFlowerSnap(g) end)
		pcall(function() ModFeatures.tickRestoreWhite(g) end)
		pcall(function() ModFeatures.tickHubWarp(g) end)
		pcall(function() ModFeatures.tickMeetEverywhere(g) end)
		pcall(function() ModFeatures.tickNoWind(g) end)

		if ModConfig.infiniteEnergy then
			pcall(function() ModFeatures.fillScarf(g, false) end)
		end
		pcall(function() ModFeatures.applySpeed(g) end)
		pcall(function() ModFeatures.applyDebugCam(g) end)
		pcall(function() ModFeatures.applyForceGlow(g) end)
		pcall(function() ModFeatures.tickAttractGlyphs(g) end)
		pcall(function() ModFeatures.flushAttractPos() end)

		local now = 0
		pcall(function() now = g:gameTiming():GetTotalTime() end)
		-- Binary HUD ~120Hz for name-tag; JSON status stays ~2Hz for the menu.
		if ModConfig.showCompanionName then
			if now - (ModState.hudTimer or 0) > (1 / 120) then
				ModState.hudTimer = now
				pcall(function() ModFeatures.writeHudSnap(g) end)
			end
		end
		if now - (ModState.statusTimer or 0) > 0.5 then
			ModState.statusTimer = now
			pcall(function() ModFeatures.writeStatus(g) end)
		end
	end)
	if not ok then
		pcall(function()
			local f = io.open("mos_error.txt", "a")
			if f then
				f:write(tostring(err) .. "\n")
				f:close()
			end
		end)
	end
end

JourneyMos_Tick()
