-- Feature implementations for Journey Mos
-- Move/fly: anim stride + surface wind (Vars.Dude alone does not affect walk).
-- Time: SetDebugContThrottle (re-apply after ProcessDebugKeys).

ModFeatures = ModFeatures or {}

local LEVEL_ORDER = {
  "Level_Graveyard",
  "Level_Barrens",
  "Level_Desert",
  "Level_Canyon",
  "Level_Cave",
  "Level_Ruins",
  "Level_Mountain",
  "Level_Summit",
  "Level_Credits",
}

-- Vanilla Vars.Dude defaults (from game binary)
local DUDE_DEFAULTS = {
  kSpeedMinBoost = 30,
  kSpeedWindMin = 10,
  kSpeedWindMid = 20,
  kSpeedWindMax = 30,
  kClothSlideMinSpd = 3,
  kClothSlideMaxSpd = 6,
  kClothLiftForce = 0.05,
  kContrailSpd = 10,
  kDudeSpdMltAtMaxWind = 0.3,
  kMaxWindStrength = 20,
  kScarfMaxCapacity = 30,
  kAmbientWindSpeed = 0.0025,
}

-- Remaining / target Steam achievements (user's locked set first)
local ACHIEVEMENTS_TARGET = {
  "ACH_Mirage",
  "ACH_Explore",
  "ACH_Wonder",
  "ACH_Threshold",
  "ACH_Adventure",
}

local ACHIEVEMENTS_ALL = {
  "ACH_Adventure",
  "ACH_Ancestors",
  "ACH_Companion",
  "ACH_Crossing",
  "ACH_Explore",
  "ACH_History",
  "ACH_Mirage",
  "ACH_Rebirth",
  "ACH_Reflection",
  "ACH_Return",
  "ACH_Threshold",
  "ACH_Transcendence",
  "ACH_Trials",
  "ACH_Wonder",
}

local function pad(g)
  local ok, p = pcall(function() return g:input():GetPad(0) end)
  return ok and p or nil
end

local function dude(g)
  local ok, d = pcall(function() return g:playerBarn():GetLocalDude() end)
  return ok and d or nil
end

local function setPos(d, g, pos)
  local ok = pcall(function()
    d:DebugTeleport(pos, g:cameraSystem())
  end)
  if not ok then
    ok = pcall(function() d:SetPos(pos) end)
  end
  return ok
end

local function ensureCheats(g)
  pcall(function()
    if Vars and Vars.Game and Vars.Game.cheatsEnabled then
      Vars.Game.cheatsEnabled(true)
    end
  end)
end

local function setDudeVar(name, value)
  if not (Vars and Vars.Dude and Vars.Dude[name]) then
    return false
  end
  local ok = pcall(function() Vars.Dude[name](value) end)
  return ok
end

local function clamp(v, lo, hi)
  if v < lo then return lo end
  if v > hi then return hi end
  return v
end


function ModFeatures.hookDebugKeys()
  if ModState.debugKeysHooked then return end
  if type(ProcessDebugKeys) ~= "function" then return end
  local prev = ProcessDebugKeys
  ProcessDebugKeys = function(game, input)
    prev(game, input)
    -- DebugKeys resets throttle to 1 when R2/L2 are up — put ours back.
    pcall(function()
      local spd = tonumber(ModConfig and ModConfig.gameSpeed) or 1.0
      if ModConfig and ModConfig.skipCinematic and spd < 4 then
        spd = 4
      end
      if spd < 0.05 then spd = 0.05 end
      if spd > 8 then spd = 8 end
      local mp = false
      pcall(function() mp = game:playerBarn():IsMultiplayer() end)
      if not mp then
        game:gameTiming():SetDebugContThrottle(spd)
      end
    end)
  end
  ModState.debugKeysHooked = true
end

function ModFeatures.fillScarf(g, force)
  local d = dude(g)
  if not d then return false end
  -- Official debug key B uses FillScarf(99, ...). Do NOT call DebugSpawnStrands
  -- every frame — that can hitch/freeze when combined with slider retunes.
  if not force then
    ModState.fillCounter = (ModState.fillCounter or 0) + 1
    if ModState.fillCounter < 3 then return true end
    ModState.fillCounter = 0
  end
  return pcall(function()
    d:FillScarf(99, g:soundBarn())
  end)
end

local function flattenXZ(v)
  local x = (type(v) == "table" and (v[1] or 0)) or 0
  local z = (type(v) == "table" and (v[3] or 0)) or 0
  local len = math.sqrt(x * x + z * z)
  if len < 1e-5 then return 0, 0 end
  return x / len, z / len
end

local function camAxes(g)
  local fx, fz, rx, rz = 0, 1, 1, 0
  pcall(function()
    local node = g:cameraSystem():GetCameraNode()
    fx, fz = flattenXZ(node:GetForward())
    rx, rz = flattenXZ(node:GetRight())
  end)
  return fx, fz, rx, rz
end

local function applyAnimStride(g, moveMult)
  local d = dude(g)
  if not d then return false end
  local barn
  if not pcall(function() barn = d:GetAnimBarn() end) or not barn then return false end
  local n = 0
  pcall(function() n = barn:GetNumAnimStates() or 0 end)
  if n <= 0 or n > 256 then return false end

  if ModState.animBase == nil then
    ModState.animBase = {}
    for i = 0, n - 1 do
      local st
      pcall(function() st = barn:GetAnimState(i) end)
      if st then
        local s, smin, smax, am
        pcall(function() s = st:stride() end)
        pcall(function() smin = st:speedMin() end)
        pcall(function() smax = st:speedMax() end)
        pcall(function() am = st:animSpeedMult() end)
        ModState.animBase[i] = { s = s, smin = smin, smax = smax, am = am }
      end
    end
  end

  for i = 0, n - 1 do
    local b = ModState.animBase[i]
    if b then
      local st
      pcall(function() st = barn:GetAnimState(i) end)
      if st then
        if type(b.s) == "number" and b.s > 0 then
          pcall(function() st:stride(b.s * moveMult) end)
        end
        if type(b.smax) == "number" then
          pcall(function() st:speedMax(b.smax * moveMult) end)
        end
        if type(b.am) == "number" then
          pcall(function() st:animSpeedMult(b.am * moveMult) end)
        end
      end
    end
  end
  return true
end

local function applyWindAssist(g, moveMult, flyMult)
  local p = pad(g)
  if not p then return end
  local sx, sy = 0, 0
  pcall(function()
    local stick = p:Left()
    if type(stick) == "table" then
      sx = stick[1] or 0
      sy = stick[2] or 0
    end
  end)
  local stickMag = math.sqrt(sx * sx + sy * sy)
  local flying = false
  pcall(function() flying = p:IsButtonDown(12) end)
  if stickMag < 0.12 and not flying then
    -- clear wind when idle so we don't shove the player
    if ModState.windAssistOn then
      pcall(function() g:surfaceWindBarn():SetVelocity({ 0, 0, 0 }) end)
      ModState.windAssistOn = false
    end
    return
  end

  -- Match official DebugKeys wind edit:
  --   forward *= stickY, right *= -stickX  (our old -sy was reversed)
  local fx, fz, rx, rz = camAxes(g)
  local ix = fx * sy + rx * (-sx)
  local iz = fz * sy + rz * (-sx)
  local im = math.sqrt(ix * ix + iz * iz)
  if im > 1e-4 then
    ix, iz = ix / im, iz / im
  elseif flying then
    ix, iz = fx, fz
  else
    return
  end

  -- Strength scales with (mult-1); at mult=1 no extra wind
  local moveExtra = math.max(moveMult - 1.0, 0)
  local flyExtra = math.max(flyMult - 1.0, 0)
  local strength = 8.0 * moveExtra
  if flying then
    strength = strength + 10.0 * flyExtra
  end
  if strength < 0.5 and not (flying and flyExtra > 0) then return end

  local lift = 0
  if flying then
    lift = 4.0 + 6.0 * flyExtra
  end
  local mag = math.max(stickMag, flying and 0.75 or 0)
  pcall(function()
    g:surfaceWindBarn():SetVelocity({
      ix * strength * mag,
      lift,
      iz * strength * mag,
    })
  end)
  ModState.windAssistOn = true

  -- Physics impulse event (not teleport)
  if strength > 1 then
    pcall(function()
      SpawnEvent {
        SetHorizontalVelocity = {
          duration = 0.05,
          acceleration = strength * 2.5,
          direction = { ix, 0, iz },
        }
      }
    end)
  end
end

-- Move/fly: wind every frame; stride/Vars retune is rate-limited (prevents freezes).
function ModFeatures.applySpeed(g)
  ensureCheats(g)
  local moveMult = 1.0
  if ModConfig.speedBoost then
    moveMult = clamp(tonumber(ModConfig.speedMult) or 1.0, 0.25, 20)
  end
  local flyMult = 1.0
  if ModConfig.flyBoost then
    flyMult = clamp(tonumber(ModConfig.flyBoostMult) or 1.0, 0.25, 20)
  end

  local key = string.format("%.2f:%.2f", moveMult, flyMult)
  local now = 0
  pcall(function() now = g:gameTiming():GetTotalTime() end)
  local due = (ModState.dudeTuneKey ~= key)
  local cooled = (now - (ModState.dudeTuneAt or 0)) >= 0.45
  if due and cooled then
    ModState.dudeTuneKey = key
    ModState.dudeTuneAt = now
    local D = DUDE_DEFAULTS
    setDudeVar("kSpeedMinBoost", D.kSpeedMinBoost * moveMult)
    setDudeVar("kSpeedWindMin", D.kSpeedWindMin * moveMult)
    setDudeVar("kSpeedWindMid", D.kSpeedWindMid * moveMult)
    setDudeVar("kSpeedWindMax", D.kSpeedWindMax * moveMult)
    setDudeVar("kClothSlideMinSpd", D.kClothSlideMinSpd * moveMult)
    setDudeVar("kClothSlideMaxSpd", D.kClothSlideMaxSpd * moveMult)
    setDudeVar("kContrailSpd", D.kContrailSpd * moveMult)
    if ModConfig.noWind then
      setDudeVar("kDudeSpdMltAtMaxWind", 1.0)
      setDudeVar("kMaxWindStrength", 0.01)
    else
      setDudeVar("kDudeSpdMltAtMaxWind", clamp(D.kDudeSpdMltAtMaxWind * moveMult, 0.05, 1.5))
      setDudeVar("kMaxWindStrength", D.kMaxWindStrength * math.max(moveMult, flyMult))
    end
    setDudeVar("kClothLiftForce", D.kClothLiftForce * flyMult)
    -- Anim stride only when move mult changes meaningfully (heavy)
    local animOk = applyAnimStride(g, moveMult)
    ModState.setVelOk = animOk
    print(string.format("[JourneyMos] speed move=x%.2f fly=x%.2f anim=%s", moveMult, flyMult, tostring(animOk)))
  end

  if moveMult > 1.05 or flyMult > 1.05 then
    applyWindAssist(g, moveMult, flyMult)
  elseif ModState.windAssistOn then
    pcall(function() g:surfaceWindBarn():SetVelocity({ 0, 0, 0 }) end)
    ModState.windAssistOn = false
  end
end

-- Exact official debug: SetMaxCloth{ changeAmount = 3 } (DebugKeys '=')
-- No enforceMinimum / resetToAmount — those were breaking length growth.
function ModFeatures.clothStep(g, amount)
  amount = tonumber(amount) or 3
  pcall(function()
    SpawnEvent { SetMaxCloth = { changeAmount = amount } }
  end)
  ModFeatures.fillScarf(g, true)
end

function ModFeatures.maxCloth(g)
  -- Grow length the same way debug keys do, many times; then refill energy.
  pcall(function() Vars.Dude.kScarfMaxCapacity(30) end)
  for _ = 1, 20 do
    pcall(function()
      SpawnEvent { SetMaxCloth = { changeAmount = 3 } }
    end)
  end
  ModFeatures.fillScarf(g, true)
  ModNotify("围巾已拉满", 1.4)
end

-- steps: how many official +3 cloth bumps (1–20).
function ModFeatures.setClothSteps(g, steps)
  steps = math.floor(tonumber(steps) or 1)
  if steps < 1 then steps = 1 end
  if steps > 20 then steps = 20 end
  pcall(function() Vars.Dude.kScarfMaxCapacity(30) end)
  for _ = 1, steps do
    pcall(function()
      SpawnEvent { SetMaxCloth = { changeAmount = 3 } }
    end)
  end
  ModFeatures.fillScarf(g, true)
  ModNotify(string.format("围巾长度 ×%d (+3×%d)", steps, steps), 1.2)
end

-- Mirage flower marker (EventOnShout). Y slightly above sand.
local FLOWER_POS = { 187.951, 63.8, 316.302 }
local FLOWER_TRIGGERS = {
  "5714d6a88a943149a4574524e5f07f92", -- SpawnShout Flower
  "9dfaab28ef7b0aaf4de8c8cdd88a1703", -- EventOnShout
}

local HUB_POS = { 251.7, 14.2, 465.8 }

local function fireTriggers(list)
  local n = 0
  if type(list) ~= "table" then return 0 end
  for _, name in ipairs(list) do
    if ActivateTriggerByName and name then
      local ok = pcall(function() ActivateTriggerByName(name) end)
      if ok then n = n + 1 end
    end
  end
  return n
end

function ModFeatures.tpFlower(g)
  ensureCheats(g)
  local level = ""
  pcall(function() level = tostring(g:GetCurrentLevelString() or "") end)
  if level ~= "Level_Desert" then
    ModState.pendingFlowerSnap = true
    ModState.flowerSnapFrames = 0
    ModNotify("先去粉沙漠，到关后会自动贴花", 1.5)
    ModFeatures.queueLevel(g, "Level_Desert")
    return
  end
  local d = dude(g)
  if not d then return end
  setPos(d, g, { FLOWER_POS[1], FLOWER_POS[2], FLOWER_POS[3] })
  -- Level scripts often yank the player back — re-snap for a short window.
  ModState.pendingFlowerSnap = true
  ModState.flowerSnapFrames = 0
  ModNotify("已到幻象花 · 自动二次贴地", 1.6)
end

function ModFeatures.tickFlowerSnap(g)
  if not ModState.pendingFlowerSnap then return end
  local level = ""
  pcall(function() level = tostring(g:GetCurrentLevelString() or "") end)
  if level ~= "Level_Desert" then return end
  local d = dude(g)
  if not d then return end
  ModState.flowerSnapFrames = (ModState.flowerSnapFrames or 0) + 1
  local f = ModState.flowerSnapFrames
  -- Re-snap a few times while desert scripts settle — one button press.
  if f == 20 or f == 55 or f == 100 then
    setPos(d, g, { FLOWER_POS[1], FLOWER_POS[2], FLOWER_POS[3] })
  end
  if f < 100 then return end
  ModState.pendingFlowerSnap = false
  ModState.flowerSnapFrames = 0
end

function ModFeatures.triggerFlower(g)
  ensureCheats(g)
  local level = ""
  pcall(function() level = tostring(g:GetCurrentLevelString() or "") end)
  if level ~= "Level_Desert" then
    ModNotify("请在粉沙漠触发幻象花", 1.4)
    return
  end
  local d = dude(g)
  if d then
    setPos(d, g, { FLOWER_POS[1], FLOWER_POS[2], FLOWER_POS[3] })
  end
  local n = fireTriggers(FLOWER_TRIGGERS)
  ModNotify(n > 0 and "已强制触发幻象花事件" or "幻象花触发失败", 1.6)
end

-- Real chirp = OverrideButtonDown(13). One-shot via fire_chirp / 立刻大叫.
local CHIRP_BUTTON = 13  -- sit=0, fly=12, chirp=13, pause=19
local FORCE_CHIRP_HOLD = 6

local function applyFatShoutVars(rad)
  rad = tonumber(rad) or 32
  if rad < 4 then rad = 4 end
  if rad > 60 then rad = 60 end
  setDudeVar("kShoutRadInitial", rad)
  setDudeVar("kShoutRadInc", math.max(4, rad * 0.3))
  setDudeVar("kShoutDurInitial", 2.5)
  setDudeVar("kShoutRadMin", math.max(3, rad * 0.35))
  setDudeVar("kShoutChargeTime", 0.01)
  pcall(function() Vars.ShoutBarn.kShoutRadMlt(1.8) end)
  pcall(function() Vars.ShoutBarn.kShoutHDRInt(2.2) end)
  return rad
end

local function pressChirp(g)
  pcall(function()
    local pad = g:input():GetPad(0)
    if pad and pad.OverrideButtonDown then
      pad:OverrideButtonDown(CHIRP_BUTTON)
    end
  end)
  pcall(function()
    g:input():GetPad(0):OverrideButtonDown(CHIRP_BUTTON, true)
  end)
end

function ModFeatures.fireChirp(g, rad)
  rad = applyFatShoutVars(rad)
  ModState.forceChirpFrames = FORCE_CHIRP_HOLD
  ModState.forceChirpRad = rad
  return true
end

function ModFeatures.tickForceChirp(g)
  local frames = ModState.forceChirpFrames or 0
  if frames <= 0 then return end
  ModState.forceChirpFrames = frames - 1
  applyFatShoutVars(ModState.forceChirpRad or 32)
  pressChirp(g)
end

function ModFeatures.noCold(g)
  ensureCheats(g)
  setDudeVar("kDudeSpdColdnessPenalty", 0)
  setDudeVar("kTripSpd", 99)
  ModNotify("寒冷减速已关闭", 1.2)
end

-- #5 ScaleTime — official cinematic-style time warp (separate from gameSpeed throttle).
function ModFeatures.scaleTime(g, mult, duration)
  ensureCheats(g)
  mult = tonumber(mult) or 0.3
  duration = tonumber(duration) or 4
  local ok = pcall(function()
    SpawnEvent {
      ScaleTime = {
        duration = duration,
        fadeIn = 0.4,
        fadeOut = 0.8,
        timeMult = mult,
      }
    }
  end)
  ModNotify(ok and string.format("ScaleTime x%.2f · %.0fs", mult, duration) or "ScaleTime 失败", 1.4)
end

-- Game built-in DisplayText only (no overlay). LOCAL only — companions cannot see it.
function ModFeatures.showText(g, text, duration, style)
  text = tostring(text or "")
  if text == "" then text = "Journey Mos" end
  duration = tonumber(duration) or 3
  style = tostring(style or "center")

  local scale = 0.055
  local x, y = 0.0, 0.35
  if style == "big" or style == "center" then
    scale = 0.09
    y = 0.32
  elseif style == "top" then
    scale = 0.07
    y = 0.10
  elseif style == "bottom" then
    scale = 0.07
    y = 0.55
  elseif style == "left" then
    scale = 0.06
    x, y = -0.55, 0.30
  elseif style == "right" then
    scale = 0.06
    x, y = 0.55, 0.30
  end

  return pcall(function()
    SpawnEvent {
      DisplayText = {
        text = text,
        x = x,
        y = y,
        duration = duration,
        fadeTime = 0.25,
        scale = scale,
        leftJustify = false,
      }
    }
  end)
end

-- Camera / pad kick — same event the game uses for hard landings & loud chirps.
function ModFeatures.cameraKick(g, intensity, duration)
  intensity = tonumber(intensity) or 0.7
  duration = tonumber(duration) or 0.5
  local origin = { 0, 0, 0 }
  pcall(function()
    local p = g:playerBarn():GetLocalDude():GetPos()
    origin = { p[1], p[2], p[3] }
  end)
  return pcall(function()
    SpawnEvent {
      Rumble = {
        cameraRumbleDuration = duration,
        cameraRumbleFrequency = 2.4,
        cameraRumbleIntensity = intensity,
        cameraRumbleFadeDur = duration * 0.45,
        falloffMinDist = 5,
        falloffRadius = 40,
        falloffUseCamPos = false,
        largeRumbleDuration = duration,
        largeRumbleFalloff = duration * 0.5,
        largeRumbleIntensity = intensity,
        originPos = origin,
        smallRumbleDuration = duration * 0.55,
        useCameraRumble = true,
        useControllerRumble = true,
        useOriginPos = true,
        notInAir = false,
      }
    }
  end)
end

-- Menu “镜头震动” — short hard camera punch, not a vague rumble toy.
function ModFeatures.rumble(g, intensity, duration)
  ensureCheats(g)
  intensity = tonumber(intensity) or 0.85
  duration = tonumber(duration) or 0.55
  local ok = ModFeatures.cameraKick(g, intensity, duration)
  -- Second hit for a physical “thump”.
  if ok then
    pcall(function()
      SpawnEvent {
        Rumble = {
          cameraRumbleDuration = duration * 0.35,
          cameraRumbleFrequency = 3.5,
          cameraRumbleIntensity = intensity * 0.55,
          cameraRumbleFadeDur = 0.12,
          falloffMinDist = 5,
          falloffRadius = 30,
          falloffUseCamPos = false,
          largeRumbleDuration = duration * 0.35,
          largeRumbleFalloff = 0.12,
          largeRumbleIntensity = intensity * 0.7,
          originPos = { 0, 0, 0 },
          smallRumbleDuration = duration * 0.2,
          useCameraRumble = true,
          useControllerRumble = true,
          useOriginPos = false,
          notInAir = false,
        }
      }
    end)
  end
  ModNotify(ok and "镜头震动" or "震动失败", 0.8)
end

local function spawnSuperGlow(duration, fadeIn, fadeOut)
  return pcall(function()
    SpawnEvent {
      DudeSuperGlow = {
        duration = duration,
        fadeInTime = fadeIn or 0.4,
        fadeOutTime = fadeOut or 0.6,
        fadePow = 2,
        useRemote = false,
      }
    }
  end)
end

-- Toggleable sticky glow. Click again to cancel (near-zero duration).
function ModFeatures.toggleSuperGlow(g, long)
  ensureCheats(g)
  local key = long and "longGlowOn" or "pulseGlowOn"
  ModState[key] = not (ModState[key] and true or false)
  if ModState[key] then
    local dur = long and 1e9 or 8
    local ok = spawnSuperGlow(dur, 1.0, 1.5)
    ModNotify(ok and (long and "长发光 ON（再点关闭）" or "发光 ON（再点关闭）") or "发光失败", 1.4)
  else
    spawnSuperGlow(0.05, 0, 0.2)
    ModNotify(long and "长发光 OFF" or "发光 OFF", 1.0)
  end
end

-- Back-compat one-shot (commands)
function ModFeatures.superGlow(g, long)
  ModFeatures.toggleSuperGlow(g, long)
end

-- #23 Explode / blast yourself
function ModFeatures.explodeDude(g)
  ensureCheats(g)
  local ok = pcall(function()
    SpawnEvent {
      ExplodeDude = {
        angle = { 0, 1, 0 },
        strength = 18,
      }
    }
  end)
  ModNotify(ok and "轰！" or "爆炸失败", 1.0)
end

local function detectWearingWhite(g)
  local d = dude(g)
  local wearing = false
  if d then
    pcall(function() wearing = d:IsWearingWhiteOutfit() and true or false end)
  end
  if not wearing then
    pcall(function() wearing = IsWearingWhiteOutfit(g) and true or false end)
  end
  if not wearing and ModState.robeWhite == true then wearing = true end
  return wearing
end

local function applyWhiteCostume(g)
  pcall(function()
    SpawnEvent {
      SetDudeAge = {
        age = 0,
        useMaxAge = false,
        useRemote = false,
        whiteCostumeAge = true,
      }
    }
  end)
end

-- Cycle embroidery / journey tier. Preserve white robe across SetOutfit.
function ModFeatures.cycleOutfitTier(g)
  ensureCheats(g)
  local keepWhite = detectWearingWhite(g)
  ModState.robeWhite = keepWhite
  local tier = ((ModState.outfitTier or 0) + 1) % 4
  ModState.outfitTier = tier
  pcall(function()
    g:playerBarn():GetLocalDude():SetOutfit(tier)
  end)
  if keepWhite then
    applyWhiteCostume(g)
    -- SetOutfit often wins same-frame — re-apply for a short window.
    ModState.pendingRestoreWhite = true
    ModState.restoreWhiteFrames = 0
  end
  ModNotify(
    keepWhite and ("花纹档 " .. tostring(tier) .. " · 保持白袍")
      or ("花纹档 " .. tostring(tier)),
    1.3
  )
end

function ModFeatures.tickRestoreWhite(g)
  if not ModState.pendingRestoreWhite then return end
  ModState.restoreWhiteFrames = (ModState.restoreWhiteFrames or 0) + 1
  if (ModState.restoreWhiteFrames % 5) == 0 then
    applyWhiteCostume(g)
  end
  if ModState.restoreWhiteFrames >= 45 then
    ModState.pendingRestoreWhite = false
    ModState.restoreWhiteFrames = 0
    ModState.robeWhite = true
  end
end

-- Mute BGM (official DebugKeys trailer capture helper).
function ModFeatures.muteMusic(g)
  local ok = pcall(function()
    g:soundBarn():ToggleMuteMusic()
  end)
  ModNotify(ok and "音乐静音已切换" or "静音失败", 1.2)
end

function ModFeatures.netGuiSelect(g)
  ensureCheats(g)
  local ok = pcall(function()
    g:netGui():ExecuteSelectedItem(g)
  end)
  ModNotify(ok and "已执行 NetGui 当前选项" or "NetGui 执行失败（先开面板）", 1.5)
end

function ModFeatures.netGuiNext(g)
  ensureCheats(g)
  local ok = pcall(function() g:netGui():SelectNextItem() end)
  ModNotify(ok and "NetGui → 下一项" or "NetGui 失败", 0.9)
end

function ModFeatures.netGuiPrev(g)
  ensureCheats(g)
  local ok = pcall(function() g:netGui():SelectPreviousItem() end)
  ModNotify(ok and "NetGui ← 上一项" or "NetGui 失败", 0.9)
end

local function warpToHubNow(g)
  local d = dude(g)
  if not d then return false end
  local ok = setPos(d, g, { HUB_POS[1], HUB_POS[2], HUB_POS[3] })
  -- Write continue = Graveyard so title shows 继续游戏 after quit.
  pcall(function()
    SpawnEvent { SaveGameTrigger = {} }
  end)
  return ok
end

-- Real menu Chapter Select ≈ Graveyard hub pit (not desert intro start).
function ModFeatures.gotoChapterSelect(g)
  ensureCheats(g)
  local level = ""
  pcall(function() level = tostring(g:GetCurrentLevelString() or "") end)
  if level == "Level_Graveyard" then
    if warpToHubNow(g) then
      ModNotify("已传到选择章节中心", 1.8)
    else
      ModNotify("选择章节传送失败", 1.2)
    end
    return
  end
  ModState.pendingHubWarp = true
  ModState.hubWarpFrames = 0
  ModFeatures.queueLevel(g, "Level_Graveyard")
  ModNotify("加载墓园后会自动传到选择章节中心", 2.0)
end

function ModFeatures.tickHubWarp(g)
  if not ModState.pendingHubWarp then return end
  local level = ""
  pcall(function() level = tostring(g:GetCurrentLevelString() or "") end)
  if level ~= "Level_Graveyard" then return end
  local d = dude(g)
  if not d then return end
  ModState.hubWarpFrames = (ModState.hubWarpFrames or 0) + 1
  -- Wait for level settle / intro cam before snapping to hub.
  if ModState.hubWarpFrames < 100 then return end
  warpToHubNow(g)
  -- Re-snap a few times in case intro scripts yank the player back.
  if ModState.hubWarpFrames < 160 then return end
  ModNotify("已到选择章节 · 已写入存档(继续=墓园)", 2.2)
  ModState.pendingHubWarp = false
  ModState.hubWarpFrames = 0
end

function ModFeatures.saveNow(g)
  ensureCheats(g)
  local ok = pcall(function()
    SpawnEvent { SaveGameTrigger = {} }
  end)
  ModNotify(ok and "已写入存档（继续=本关起点）" or "存档失败", 1.6)
end

-- SetOutfit(n) = embroidery / journey tier (周目花纹), NOT red/white.
-- Wardrobe uses SetDudeAge { whiteCostumeAge = true/false }.
function ModFeatures.setRobeColor(g, white)
  ensureCheats(g)
  local ok = false
  if white then
    ok = pcall(function()
      SpawnEvent {
        SetDudeAge = {
          age = 0,
          useMaxAge = false,
          useRemote = false,
          whiteCostumeAge = true,
        }
      }
    end)
    if ok then
      ModState.robeWhite = true
      pcall(function()
        SpawnEvent {
          SetMaxCloth = {
            applyToRemote = false,
            changeAmount = 10,
            enforceMinimum = true,
            resetToAmount = false,
          }
        }
      end)
      ModFeatures.fillScarf(g, true)
    end
    ModNotify(ok and "已切换白袍（衣柜同款）" or "白袍切换失败", 1.6)
  else
    ok = pcall(function()
      SpawnEvent {
        SetDudeAge = {
          age = 0,
          useMaxAge = true,
          useRemote = false,
          whiteCostumeAge = false,
        }
      }
    end)
    if ok then
      ModState.robeWhite = false
    end
    ModNotify(ok and "已切换红袍（衣柜同款）" or "红袍切换失败", 1.6)
  end
end

-- Optional: change embroidery tier only (0–3-ish). Kept separate from robe color.
function ModFeatures.setOutfitTier(g, outfitId)
  ensureCheats(g)
  outfitId = tonumber(outfitId) or 0
  local ok = pcall(function()
    g:playerBarn():GetLocalDude():SetOutfit(outfitId)
  end)
  ModNotify(ok and ("周目花纹档 " .. tostring(outfitId)) or "花纹切换失败", 1.2)
end

function ModFeatures.disconnectCompanion(g)
  local ok = pcall(function()
    g:pweb3():ResetSocket()
  end)
  ModNotify(ok and "已断开同伴连接" or "断开失败", 1.2)
end

-- Reload current level → official spawn / 出生点.
function ModFeatures.respawnLevelStart(g)
  ensureCheats(g)
  local level = "Level_Graveyard"
  pcall(function() level = tostring(g:GetCurrentLevelString() or level) end)
  ModFeatures.changeLevelSaved(g, level)
  ModNotify("重进本关出生点 → " .. tostring(level), 1.6)
end

-- Proper level transition with save=true (so Continue tracks progress).
function ModFeatures.changeLevelSaved(g, levelName)
  ensureCheats(g)
  levelName = levelName or "Level_Barrens"
  local ok = pcall(function()
    SpawnEvent {
      ChangeLevel = {
        level = levelName,
        changeDelay = 0.8,
        save = true,
        saveDelay = 0.2,
        isEndOfGame = false,
        isGraveyardTeleporter = false,
      }
    }
  end)
  if ok then
    ModNotify("正式跳关并存档 → " .. tostring(levelName), 1.8)
  else
    ModFeatures.queueLevel(g, levelName)
  end
end

local function tryUnlockTrophy(idOrName)
  local ok = false
  -- Event used by game trophy modules
  ok = pcall(function()
    SpawnEvent {
      UnlockTrophyTrigger = {
        trophyId = idOrName,
        queueUnlock = true,
        unlockQueuedTrophies = true,
      }
    }
  end) or ok
  ok = pcall(function()
    if Trophy_SetUnlockingEnabled then Trophy_SetUnlockingEnabled(true) end
  end) or ok
  ok = pcall(function()
    if Trophy_QueueUnlockTrophy then Trophy_QueueUnlockTrophy(idOrName) end
  end) or ok
  ok = pcall(function()
    if Trophy_UnlockTrophy then Trophy_UnlockTrophy(idOrName) end
  end) or ok
  ok = pcall(function()
    if UnlockTrophy then UnlockTrophy(idOrName) end
  end) or ok
  ok = pcall(function() UnlockAchievement(idOrName) end) or ok
  return ok
end

function ModFeatures.unlockAchievements(g, onlyTarget)
  ensureCheats(g)
  pcall(function()
    if Trophy_SetUnlockingEnabled then Trophy_SetUnlockingEnabled(true) end
  end)

  local list = onlyTarget and ACHIEVEMENTS_TARGET or ACHIEVEMENTS_ALL
  local unlocked = 0
  for _, name in ipairs(list) do
    if tryUnlockTrophy(name) then unlocked = unlocked + 1 end
  end
  for id = 0, 20 do
    tryUnlockTrophy(id)
  end
  pcall(function()
    if CheckWonderTrophy then CheckWonderTrophy(g) end
  end)
  pcall(function()
    if Trophy_UnlockNextTrophyInQueue then Trophy_UnlockNextTrophyInQueue() end
  end)

  ModNotify(string.format("全部成就已请求 %d 名（看 Steam）", unlocked), 2.5)
  print(string.format("[JourneyMos] trophy attempts names=%d", unlocked))
end

function ModFeatures.toggleNetGui(g)
  ensureCheats(g)
  local ok = pcall(function()
    g:netGui():ToggleEnabled()
    ModNotify("NetGui 已切换", 1.0)
  end)
  if not ok then ModNotify("NetGui 失败", 1.0) end
end

-- Emotes: QueueAnimationSequence / QueueAnimation (local, validated by journey_modding)
-- + PlayDudeAnim useLocal (companion-visible official event).
local EMOTE_LABELS = {
  IdleStretch = "伸懒腰",
  IdleCheckFootL = "看左脚",
  IdleCheckFootR = "看右脚",
  LookBackL = "左回头",
  LookBackR = "右回头",
  LookUp = "抬头",
  TurnLookBackL = "转身左看",
  TurnLookBackR = "转身右看",
  Act01Surprise = "惊讶",
  Act01LookAround = "环顾",
  Act01LookDown = "低头看",
  Act01Hold = "静立",
  Act06LookAround = "环顾2",
  Act06LookLR = "左右看",
  Flap1 = "振翅1",
  Flap3 = "振翅",
  BoostFlap = "振翅加速",
  BoostFlap2 = "振翅加速2",
  BoostFlap3 = "振翅加速3",
  BoostSpiralL = "螺旋左",
  BoostSpiralR = "螺旋右",
  ShoutL = "大叫",
  ShoutR = "大叫右",
  ShoutM = "中叫",
  ShoutBL = "大叫后左",
  ShoutBR = "大叫后右",
  Charge = "蓄力",
  ChargeAir = "空中蓄力",
  ChargeWalk = "走蓄力",
  ChargeRun = "跑蓄力",
  ChargeWeak = "弱蓄力",
  StandTurnL = "原地左转",
  StandTurnR = "原地右转",
  LiftA = "抬起A",
  LiftA1 = "抬起A1",
  LiftEL = "抬起左",
  LiftER = "抬起右",
  LiftF = "抬起F",
  AirFlipL = "空翻左",
  AirFlipR = "空翻右",
  RollF = "前滚",
  RollB = "后滚",
  RollL = "左滚",
  RollR = "右滚",
  LieF = "趴下",
  LieB = "仰躺",
  GetUpF = "爬起",
  GetUpB = "仰起",
  JumpRoll = "跳滚",
  HopL = "小跳",
  Crouch = "蹲下",
  Crouching = "蹲着",
  Kneel = "跪下",
  Kneeling = "跪着",
  Med = "打坐",
  ReadyToMed = "准备打坐",
  StopMed = "结束打坐",
  FixCloth = "整理围巾",
  FixCloth2 = "整理围巾2",
  FixCloth3 = "整理围巾3",
  FixCloth4 = "整理围巾4",
  FixClothAir = "空中整理",
  SpinF = "前转",
  MPFSpinL = "自旋左",
  MPFSpinR = "自旋右",
  NodHead = "点头",
  ShakeHead = "摇头",
  ShakeOff = "抖一抖",
  Cough = "咳嗽",
  Collapse = "倒下",
  Collapsing = "倒地中",
  Revive = "起身",
  Reviving = "起身中",
  Collapse2 = "倒下2",
  Revive2 = "起身2",
  HitBodyL = "挨打左",
  HitBodyR = "挨打右",
  HitBodyM = "挨打中",
  BlownBack = "吹飞",
  FallForward = "前倒",
  PrepareToLaunch = "准备起飞",
  ReadyToLaunch = "待飞",
  MPAirSwing1L = "空中摆左",
  MPAirSwing1R = "空中摆右",
  MPRunFlip1L = "跑翻左",
  MPRunFlip1R = "跑翻右",
}

function ModFeatures.playDudeAnim(g, animName)
  animName = animName or "IdleStretch"
  ensureCheats(g)
  local localOk = false
  local netOk = false

  -- Local playback (journey_modding: QueueAnimationSequence / QueueAnimation).
  pcall(function()
    local barn = g:playerBarn():GetLocalDude():GetAnimBarn()
    if not barn then return end
    if type(QueueAnimationSequence) == "function"
      and type(animationSequences) == "table"
      and animationSequences[animName] then
      QueueAnimationSequence(barn, animName)
      localOk = true
    elseif barn.QueueAnimation then
      -- blendIn, blendOut, animSpeed, numLoops, duration (PlayDudeAnimation defaults)
      barn:QueueAnimation(animName, 0.1, 0.1, 1.0, 0, -1)
      localOk = true
    end
  end)

  -- Companion-visible path (official trigger).
  netOk = pcall(function()
    SpawnEvent {
      PlayDudeAnim = {
        animName = animName,
        clearAnimQueue = true,
        useLocal = true,
      }
    }
  end)

  local label = EMOTE_LABELS[animName] or tostring(animName)
  if localOk or netOk then
    ModNotify("表情 · " .. label, 1.0)
  else
    ModNotify("播放失败", 1.0)
  end
end

function ModFeatures.applyGameSpeed(g)
  ensureCheats(g)
  ModFeatures.hookDebugKeys()
  local spd = tonumber(ModConfig.gameSpeed) or 1.0
  if ModConfig.skipCinematic and spd < 4 then
    spd = math.max(spd, 4)
  end
  if spd < 0.05 then spd = 0.05 end
  if spd > 8 then spd = 8 end

  local mp = false
  pcall(function() mp = g:playerBarn():IsMultiplayer() end)
  if mp then
    ModState.lastGameSpeed = 1
    return
  end

  pcall(function() g:gameTiming():SetDebugContThrottle(spd) end)
  ModState.lastGameSpeed = spd
end

function ModFeatures.applySkipCinematic(g)
  if not ModConfig.skipCinematic then return end
  -- Rush cutscenes via time scale (handled in applyGameSpeed).
  -- Also try to drop debug-cam cinematic lock if active.
  pcall(function()
    local cs = g:cameraSystem()
    if cs:UsingDebugCam() then
      local dc = cs:GetDebugCam()
      if dc and dc.GetCinematicMode and dc:GetCinematicMode() then
        dc:SetCinematicMode(false)
      end
    end
  end)
end

function ModFeatures.applyDebugCam(g)
  local want = ModConfig.debugCam and true or false
  if ModState.debugCamActive == want then return end
  ModState.debugCamActive = want
  pcall(function()
    if want then
      g:cameraSystem():StartDebugCam()
    else
      g:cameraSystem():StopDebugCam()
    end
  end)
  ModNotify(want and "Debug cam ON" or "Debug cam OFF", 1.0)
end

function ModFeatures.applyForceGlow(g)
  local want = ModConfig.forceGlow and true or false
  if ModState.forceGlowActive == want then return end
  -- ForceGlowEnabled is a toggle — call on both edges.
  pcall(function() ForceGlowEnabled(g) end)
  ModState.forceGlowActive = want
  ModNotify(want and "强制发光 ON" or "强制发光 OFF", 1.0)
end

-- 吸符文：纯内存，由 JourneyMos.dll GameUpdate 完成（读玩家坐标 + 写符文槽 + 强制收集）。
-- Lua 只保留开关同步；不再写 mos_attract_pos.bin。
function ModFeatures.tickAttractGlyphs(g)
  -- no-op: native Partner hook owns vacuum
end

function ModFeatures.teleportToCompanion(g)
  local ok = pcall(function()
    local localDude = g:playerBarn():GetLocalDude()
    local remote = g:playerBarn():GetRemoteDude()
    local pos = remote:GetPos()
    if type(pos) == "table" then
      setPos(localDude, g, pos)
      ModNotify("传送到同伴", 1.2)
    end
  end)
  if not ok then ModNotify("没有同伴", 1.2) end
end

function ModFeatures.teleportToCamera(g)
  local ok = pcall(function()
    local node = g:cameraSystem():GetCameraNode()
    local m = node:GetWorldMatrix()
    local pos
    if type(m) == "table" then
      if type(m[4]) == "table" then
        pos = { m[4][1], m[4][2], m[4][3] }
      elseif m[13] then
        pos = { m[13], m[14], m[15] }
      end
    end
    if pos then
      setPos(g:playerBarn():GetLocalDude(), g, pos)
      ModNotify("传送到相机", 1.0)
    else
      ModNotify("读不到相机坐标", 1.0)
    end
  end)
  if not ok then ModNotify("传送失败", 1.0) end
end

function ModFeatures.toggleDeathMatch(g)
  local ok = pcall(function()
    ToggleDMActive(g)
    ModState.deathMatch = not (ModState.deathMatch or false)
    ModNotify(
      ModState.deathMatch
        and "死亡竞赛 ON · 鸣叫=火箭 · 连按飞跃=跳（同伴看不见爆炸）"
        or "死亡竞赛 OFF",
      2.4
    )
  end)
  if not ok then ModNotify("死亡竞赛不可用", 1.0) end
end

function ModFeatures.queueLevel(g, levelName)
  local ok = pcall(function()
    g:QueueLevel(levelName, true, true)
    ModNotify("前往 " .. tostring(levelName), 1.4)
  end)
  if not ok then ModNotify("关卡跳转失败", 1.0) end
  return ok
end

function ModFeatures.currentLevelIndex(g)
  local level = ""
  pcall(function() level = tostring(g:GetCurrentLevelString() or "") end)
  for i, name in ipairs(LEVEL_ORDER) do
    if level == name or level:find(name, 1, true) then
      return i, level
    end
  end
  return nil, level
end

function ModFeatures.nextLevel(g, withSave)
  local idx = ModFeatures.currentLevelIndex(g)
  local nextName
  if idx then
    nextName = LEVEL_ORDER[(idx % #LEVEL_ORDER) + 1]
  else
    nextName = "Level_Barrens"
  end
  if withSave then
    ModFeatures.changeLevelSaved(g, nextName)
  else
    ModFeatures.queueLevel(g, nextName)
  end
end

function ModFeatures.setAutoLobby(enabled)
  pcall(function()
    Vars.Lobby.kUseAutoLobby(enabled and true or false)
    ModNotify("AutoLobby " .. (enabled and "ON" or "OFF"), 1.0)
  end)
end

function ModFeatures.cycleDebugHud(g)
  pcall(function()
    g:debugHud():CycleMode()
  end)
end

-- Lobby radii + SetLobbyFlag pulse (Lua-side "meet everywhere" attempt).
local LOBBY_DEFAULT = { near = 15, far = 80, leave = 100 }

function ModFeatures.tickMeetEverywhere(g)
  if not ModConfig.meetEverywhere then
    if ModState.meetEverywhereOn then
      pcall(function()
        Vars.Lobby.kJoinRadiusNear(LOBBY_DEFAULT.near)
        Vars.Lobby.kJoinRadiusFar(LOBBY_DEFAULT.far)
        Vars.Lobby.kLeaveRadius(LOBBY_DEFAULT.leave)
      end)
      ModState.meetEverywhereOn = false
      ModState.meetPulseAt = 0
    end
    return
  end
  ensureCheats(g)
  if not ModState.meetEverywhereOn then
    ModState.meetEverywhereOn = true
    pcall(function()
      Vars.Lobby.kUseAutoLobby(true)
      Vars.Lobby.kJoinRadiusNear(500)
      Vars.Lobby.kJoinRadiusFar(2000)
      Vars.Lobby.kLeaveRadius(5000)
    end)
    pcall(function()
      SpawnEvent { AddNpIdsToPlayersMet = {} }
    end)
  end
  local now = 0
  pcall(function() now = g:gameTiming():GetTotalTime() end)
  if now - (ModState.meetPulseAt or 0) >= 2.5 then
    ModState.meetPulseAt = now
    pcall(function()
      SpawnEvent { SetLobbyFlag = {} }
    end)
  end
end

-- Soften environmental wind shove (Vars). Does NOT remove BoundaryRailPush walls —
-- that needs a script/CE mod like No Windwalls; KillTrigger at runtime had no effect.
function ModFeatures.tickNoWind(g)
  if not ModConfig.noWind then
    if ModState.noWindOn then
      local D = DUDE_DEFAULTS
      setDudeVar("kDudeSpdMltAtMaxWind", D.kDudeSpdMltAtMaxWind)
      setDudeVar("kMaxWindStrength", D.kMaxWindStrength)
      ModState.noWindOn = false
      ModState.dudeTuneKey = nil
    end
    return
  end
  if not ModState.noWindOn then
    ModState.noWindOn = true
    ModState.dudeTuneKey = nil
  end
  setDudeVar("kDudeSpdMltAtMaxWind", 1.0)
  setDudeVar("kMaxWindStrength", 0.01)
  setDudeVar("kDudeSpdColdnessPenalty", 0)
  if not ModConfig.speedBoost then
    pcall(function() g:surfaceWindBarn():SetVelocity({ 0, 0, 0 }) end)
  end
end

-- Real NG+ farm: cheats + warp tools. autoFarm advances chapters on a timer.
function ModFeatures.ngPlusKit(g)
  ModConfig.infiniteEnergy = true
  ModConfig.speedBoost = true
  ModConfig.speedMult = math.max(tonumber(ModConfig.speedMult) or 1, 6.0)
  ModConfig.flyBoost = true
  ModConfig.flyBoostMult = math.max(tonumber(ModConfig.flyBoostMult) or 1, 3.0)
  ModConfig.skipCinematic = true
  ModConfig.disableAutoLobby = true
  ModConfig.gameSpeed = math.max(tonumber(ModConfig.gameSpeed) or 1, 5.0)
  ModConfig.autoFarm = true
  ModConfig.farmInterval = tonumber(ModConfig.farmInterval) or 12
  ModState.dudeTuneKey = nil
  ModState.animBase = nil
  ModFeatures.applySpeed(g)
  ModFeatures.setAutoLobby(false)
  ModFeatures.fillScarf(g, true)
  ModState.farmTimer = 0
  ModNotify("刷周目：步幅/风力加速+自动跳关", 2.0)
end

function ModFeatures.applyAutoFarm(g)
  if not ModConfig.autoFarm then return end
  local interval = tonumber(ModConfig.farmInterval) or 12
  if interval < 4 then interval = 4 end
  local now = 0
  pcall(function() now = g:gameTiming():GetTotalTime() end)
  if (ModState.farmTimer or 0) <= 0 then
    ModState.farmTimer = now
    return
  end
  if now - ModState.farmTimer < interval then return end
  ModState.farmTimer = now
  ModFeatures.nextLevel(g)
end

local function mat4flat(m)
  local out = {}
  if type(m) ~= "table" then return out end
  if type(m[1]) == "table" then
    for r = 1, 4 do
      local row = m[r] or {}
      for c = 1, 4 do out[#out + 1] = tonumber(row[c]) or 0 end
    end
  else
    for i = 1, 16 do out[i] = tonumber(m[i]) or 0 end
  end
  return out
end

-- Journey's Lua has no math.frexp / math.ldexp / string.pack — pure version.
local function pack_f32(x)
  x = tonumber(x) or 0
  if x ~= x then return string.char(0, 0, 0xC0, 0x7F) end
  local sign = 0
  if x < 0 or (x == 0 and 1 / x < 0) then
    sign = 1
    x = -x
  end
  if x == math.huge then
    return string.char(0, 0, 0x80, 0x7F + sign * 0x80)
  end
  if x == 0 then
    return string.char(0, 0, 0, sign * 0x80)
  end
  -- frexp substitute: x = m * 2^e with m in [0.5, 1)
  local e = 0
  if x >= 1 then
    while x >= 1 do
      x = x * 0.5
      e = e + 1
      if e > 128 then
        return string.char(0, 0, 0x80, 0x7F + sign * 0x80)
      end
    end
  else
    while x < 0.5 do
      x = x * 2
      e = e - 1
      if e < -150 then
        return string.char(0, 0, 0, sign * 0x80)
      end
    end
  end
  -- IEEE: mantissa in [1,2) → shift
  local m = x * 2
  e = e - 1
  local exp = e + 127
  if exp <= 0 then
    -- denormal
    while exp < 0 do
      m = m * 0.5
      exp = exp + 1
      if m < 1e-40 then
        return string.char(0, 0, 0, sign * 0x80)
      end
    end
    m = m * 0.5
    exp = 0
  elseif exp >= 255 then
    return string.char(0, 0, 0x80, 0x7F + sign * 0x80)
  else
    m = m - 1
  end
  local mant = math.floor(m * 8388608 + 0.5)
  if mant >= 8388608 then
    mant = 0
    exp = exp + 1
    if exp >= 255 then
      return string.char(0, 0, 0x80, 0x7F + sign * 0x80)
    end
  end
  local b1 = mant % 256
  local b2 = math.floor(mant / 256) % 256
  local b3 = math.floor(mant / 65536) % 128 + (exp % 2) * 128
  local b4 = math.floor(exp / 2) + sign * 128
  return string.char(b1, b2, b3, b4)
end

local function pack_u32(v)
  v = math.floor(tonumber(v) or 0) % 4294967296
  if v < 0 then v = v + 4294967296 end
  return string.char(v % 256, math.floor(v / 256) % 256,
    math.floor(v / 65536) % 256, math.floor(v / 16777216) % 256)
end

-- Legacy no-op (vacuum is native-only now).
function ModFeatures.flushAttractPos()
end

-- Companion detection: IsMultiplayer() is often false even with a remote dude.
local function readCompanion(g)
  local mp, cx, cy, cz = false, 0, 0, 0
  pcall(function()
    if g:playerBarn():IsMultiplayer() then mp = true end
  end)
  pcall(function()
    local remote = g:playerBarn():GetRemoteDude()
    if remote ~= nil then
      mp = true
      local p = remote:GetPos()
      if type(p) == "table" then
        cx, cy, cz = tonumber(p[1]) or 0, tonumber(p[2]) or 0, tonumber(p[3]) or 0
      end
    end
  end)
  return mp, cx, cy, cz
end

local function readCamera(g)
  local fov, aspect, nearP, farP = 60, 1.777, 0.1, 2000
  local cam = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 }
  local proj, hasProj = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 }, 0
  pcall(function()
    local node = g:cameraSystem():GetCameraNode()
    local flat = mat4flat(node:GetWorldMatrix())
    if #flat >= 16 then cam = flat end
    pcall(function() fov = node:GetFov() end)
    pcall(function() aspect = node:GetAspect() end)
    pcall(function() nearP = node:GetNearPlane() end)
    pcall(function() farP = node:GetFarPlane() end)
    pcall(function()
      if node.GetProjMatrix then
        local pf = mat4flat(node:GetProjMatrix())
        if #pf >= 16 then
          proj, hasProj = pf, 1
        end
      end
    end)
  end)
  while #cam < 16 do cam[#cam + 1] = 0 end
  return cam, proj, hasProj, fov, aspect, nearP, farP
end

-- Fast companion/cam snap for native name-tag (mos_hud.bin).
function ModFeatures.writeHudSnap(g)
  if not ModConfig.showCompanionName then return end
  local mp, cx, cy, cz = readCompanion(g)
  local cam, proj, hasProj, fov, aspect, nearP, farP = readCamera(g)

  local okWrite, errWrite = pcall(function()
    local chunks = {
      pack_u32(0x48534F4D),
      string.char(1, mp and 1 or 0, hasProj, 0),
      pack_f32(cx), pack_f32(cy), pack_f32(cz),
    }
    for i = 1, 16 do chunks[#chunks + 1] = pack_f32(cam[i] or 0) end
    for i = 1, 16 do chunks[#chunks + 1] = pack_f32(proj[i] or 0) end
    chunks[#chunks + 1] = pack_f32(fov)
    chunks[#chunks + 1] = pack_f32(aspect)
    chunks[#chunks + 1] = pack_f32(nearP)
    chunks[#chunks + 1] = pack_f32(farP)
    local body = table.concat(chunks)
    -- Atomic replace avoids sharing conflicts with the overlay reader.
    local tmp = "mos_hud.bin.tmp"
    local f = io.open(tmp, "wb")
    if not f then error("open mos_hud.bin.tmp failed") end
    f:write(body)
    f:close()
    -- Best-effort replace (Lua 5.1 has no os.rename on all builds — try both).
    pcall(function() os.remove("mos_hud.bin") end)
    local renamed = false
    pcall(function()
      if os.rename then
        renamed = os.rename(tmp, "mos_hud.bin")
      end
    end)
    if not renamed then
      local out = io.open("mos_hud.bin", "wb")
      if out then
        local src = io.open(tmp, "rb")
        if src then
          out:write(src:read("*a") or "")
          src:close()
        end
        out:close()
      end
      pcall(function() os.remove(tmp) end)
    end
  end)
  if not okWrite and not ModState.hudWriteErr then
    ModState.hudWriteErr = true
    pcall(function()
      local f = io.open("mos_error.txt", "a")
      if f then f:write("writeHudSnap: " .. tostring(errWrite) .. "\n") f:close() end
    end)
  end
end

function ModFeatures.writeStatus(g)
  local level, pos = "?", { 0, 0, 0 }
  pcall(function() level = g:GetCurrentLevelString() end)
  pcall(function()
    local p = g:playerBarn():GetLocalDude():GetPos()
    pos = { p[1], p[2], p[3] }
  end)

  local mp, cx, cy, cz = readCompanion(g)
  local cam, proj, hasProj, fov, aspect, nearP, farP = readCamera(g)

  local body = string.format(
    '{"ok":true,"level":"%s","pos":[%.3f,%.3f,%.3f],"energy":%s,"speed":%s,"speedMult":%.2f,"flyBoostMult":%.2f,"gameSpeed":%.2f,"debugCam":%s,"skipCinematic":%s,"autoFarm":%s,"seq":%d,"tuneOk":%s,"mp":%s,"cname":"","cpos":[%.3f,%.3f,%.3f],"showName":%s,"fov":%.3f,"aspect":%.4f,"near":%.4f,"far":%.2f,"cam":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f]}',
    tostring(level):gsub('"', ""),
    pos[1] or 0, pos[2] or 0, pos[3] or 0,
    ModConfig.infiniteEnergy and "true" or "false",
    ModConfig.speedBoost and "true" or "false",
    tonumber(ModConfig.speedMult) or 1,
    tonumber(ModConfig.flyBoostMult) or 1,
    tonumber(ModConfig.gameSpeed) or 1,
    ModConfig.debugCam and "true" or "false",
    ModConfig.skipCinematic and "true" or "false",
    ModConfig.autoFarm and "true" or "false",
    ModState.cmdSeq or 0,
    ModState.setVelOk and "true" or "false",
    mp and "true" or "false",
    cx, cy, cz,
    ModConfig.showCompanionName and "true" or "false",
    tonumber(fov) or 60, tonumber(aspect) or 1.777,
    tonumber(nearP) or 0.1, tonumber(farP) or 2000,
    cam[1], cam[2], cam[3], cam[4], cam[5], cam[6], cam[7], cam[8],
    cam[9], cam[10], cam[11], cam[12], cam[13], cam[14], cam[15], cam[16]
  )
  pcall(function()
    local f = io.open("mod_status.json", "w+")
    if f then f:write(body) f:close() end
  end)
end

function ModFeatures.runCommand(g, cmd)
  if type(cmd) ~= "table" or not cmd.op then return end
  local op = cmd.op
  if op == "fill_scarf" then
    ModFeatures.fillScarf(g, true)
    ModNotify("围巾已满", 1.0)
  elseif op == "max_cloth" then
    ModFeatures.maxCloth(g)
  elseif op == "cloth_plus" then
    ModFeatures.clothStep(g, 3)
    ModNotify("围巾 +3", 0.8)
  elseif op == "cloth_minus" then
    ModFeatures.clothStep(g, -3)
    ModNotify("围巾 -3", 0.8)
  elseif op == "cloth_set" then
    ModFeatures.setClothSteps(g, cmd.steps or 8)
  elseif op == "respawn_start" then
    ModFeatures.respawnLevelStart(g)
  elseif op == "tp_flower" then
    ModFeatures.tpFlower(g)
  elseif op == "trigger_flower" then
    ModFeatures.triggerFlower(g)
  elseif op == "disconnect" then
    ModFeatures.disconnectCompanion(g)
  elseif op == "tp_companion" then
    ModFeatures.teleportToCompanion(g)
  elseif op == "chapter_select" then
    ModFeatures.gotoChapterSelect(g)
  elseif op == "unlock_achievements" then
    ModFeatures.unlockAchievements(g, false)
  elseif op == "unlock_achievements_all" then
    ModFeatures.unlockAchievements(g, false)
  elseif op == "netgui" then
    ModFeatures.toggleNetGui(g)
  elseif op == "netgui_select" then
    ModFeatures.netGuiSelect(g)
  elseif op == "netgui_next" then
    ModFeatures.netGuiNext(g)
  elseif op == "netgui_prev" then
    ModFeatures.netGuiPrev(g)
  elseif op == "fire_chirp" then
    ModFeatures.fireChirp(g, tonumber(cmd.rad) or 32)
    ModNotify("冲击波大叫", 0.9)
  elseif op == "no_cold" then
    ModFeatures.noCold(g)
  elseif op == "scale_time" then
    ModFeatures.scaleTime(g, cmd.mult or 0.3, cmd.duration or 4)
  elseif op == "show_text" then
    ModFeatures.showText(g, cmd.text or "Journey Mos", cmd.duration or 3, cmd.style or "center")
  elseif op == "rumble" then
    ModFeatures.rumble(g, cmd.intensity or 0.45, cmd.duration or 1.2)
  elseif op == "super_glow" then
    ModFeatures.toggleSuperGlow(g, cmd.long and true or false)
  elseif op == "toggle_glow" then
    ModFeatures.toggleSuperGlow(g, cmd.long and true or false)
  elseif op == "explode_dude" then
    ModFeatures.explodeDude(g)
  elseif op == "cycle_outfit_tier" then
    ModFeatures.cycleOutfitTier(g)
  elseif op == "mute_music" then
    ModFeatures.muteMusic(g)
  elseif op == "save_now" then
    ModFeatures.saveNow(g)
  elseif op == "outfit_red" then
    ModFeatures.setRobeColor(g, false)
  elseif op == "outfit_white" then
    ModFeatures.setRobeColor(g, true)
  elseif op == "outfit_tier" then
    ModFeatures.setOutfitTier(g, cmd.tier or 0)
  elseif op == "play_anim" then
    ModFeatures.playDudeAnim(g, cmd.anim or "IdleStretch")
  elseif op == "tp_camera" then
    ModFeatures.teleportToCamera(g)
  elseif op == "toggle_dm" then
    ModFeatures.toggleDeathMatch(g)
  elseif op == "queue_level" then
    ModFeatures.queueLevel(g, cmd.level or "Level_Barrens")
  elseif op == "change_level_save" then
    ModFeatures.changeLevelSaved(g, cmd.level or "Level_Barrens")
  elseif op == "next_level" then
    ModFeatures.nextLevel(g, false)
  elseif op == "next_level_save" then
    ModFeatures.nextLevel(g, true)
  elseif op == "autolobby" then
    ModFeatures.setAutoLobby(cmd.enabled)
  elseif op == "debug_hud" then
    ModFeatures.cycleDebugHud(g)
  elseif op == "ng_plus_kit" then
    ModFeatures.ngPlusKit(g)
  elseif op == "stop_farm" then
    ModConfig.autoFarm = false
    ModNotify("已停止自动跳关", 1.0)
  elseif op == "reload_config" then
    -- Soft reload: dofile config only. Do NOT wipe dudeTuneKey/animBase —
    -- that forced heavy stride retunes and froze the game after menu sync.
    pcall(dofile, "config.lua")
    -- applySpeed/gameSpeed pick up new ModConfig values on their own.
  elseif op == "notify" then
    ModNotify(tostring(cmd.text or ""), 1.2)
  end
end

function ModFeatures.consumeCommands(g)
  local ok = pcall(dofile, "commands.lua")
  if not ok then pcall(dofile, "mod/commands.lua") end
  if type(ModCommands) ~= "table" then return end
  local seq = tonumber(ModCommands.seq) or 0
  if seq <= (ModState.cmdSeq or 0) then return end
  ModState.cmdSeq = seq
  local items = ModCommands.items or {}
  -- Clear BEFORE running so a crashing command cannot replay on next boot.
  pcall(function()
    local body = string.format(
      "ModCommands = {\n  seq = %d,\n  items = {\n  }\n}\n",
      seq
    )
    local f = io.open("commands.lua", "w+")
    if f then f:write(body) f:close() end
  end)
  for _, cmd in ipairs(items) do
    ModFeatures.runCommand(g, cmd)
  end
end
