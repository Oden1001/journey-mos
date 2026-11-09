-- Bootstrap helpers. Keep side-effect free on load.

gameTickBoot = gameTickBoot or 0

function RunOnceDetect()
  if gameTickBoot < 7 then
    gameTickBoot = gameTickBoot + 1
  elseif gameTickBoot == 7 then
    gameTickBoot = 9001
    pcall(function()
      dofile("RunOnce.lua")
    end)
  end
end

function print(s)
  pcall(function()
    local f = io.open("stdout.txt", "a")
    if f then
      f:write(tostring(s) .. "\n")
      f:close()
    end
  end)
end

function SpawnEvent(eventTable)
  local g = _G["game"]
  if g == nil then return end
  local eventBarn = g:eventBarn()
  if eventBarn == nil then return end
  for eventType, eventParams in pairs(eventTable) do
    if _G[eventType] == nil then
      return
    end
    local event = eventBarn:MetaAddEvent(eventType)
    local eventImplPtr = _G[eventType].cast(event)
    for key, val in pairs(eventParams) do
      if eventImplPtr[key] ~= nil then
        eventImplPtr[key](eventImplPtr, val)
      end
    end
    eventImplPtr:Start()
  end
end

-- From Journey Lua Modder Useful Functions: fire a level trigger by ObjectName
function ActivateTriggerByName(triggerName)
  pcall(function()
    print("ActivateTriggerByName " .. tostring(triggerName))
    Names[triggerName]:Start()
  end)
end

-- Game built-in DisplayText subtitle.
function ModNotify(text, duration)
  if (gameTick or 0) < 90 then
    print("[JourneyMos] " .. tostring(text))
    return
  end
  duration = duration or 1.5
  pcall(function()
    SpawnEvent {
      DisplayText = {
        text = tostring(text),
        x = 0.0,
        y = 0.35,
        duration = duration,
        fadeTime = 0.25,
        scale = 0.055,
      }
    }
  end)
end

function DumpMetaSys()
  pcall(function()
    local out = io.open("TempMetasystem.lua", "w+")
    if out and PrintMeta then
      PrintMeta(out)
      out:close()
    end
  end)
end

print("[JourneyMos] StartLua loaded")
