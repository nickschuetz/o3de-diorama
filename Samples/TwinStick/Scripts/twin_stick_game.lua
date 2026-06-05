----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Game controller and HUD for the Diorama twin-stick sample.
--
-- This shows the division of labor in a 2.5D O3DE game: Diorama draws the world
-- (player, enemies, projectiles, tilemap arena) while LyShine draws the HUD. The
-- controller counts befriended haters, loads the LyShine canvas, and updates its text.
--
-- The count is shared through a global, _G.TwinStickBefriended, which each hater
-- bumps when it fills with love and floats away (twin_stick_enemy.lua BecomeContent).
-- This controller polls it each tick and updates the "ScoreText" element. The shared
-- global is the same pattern the rest of the sample uses (TwinStickHits, etc.), and
-- avoids a cross-entity GameplayNotification (all game scripts share one Lua VM).

-- The HUD canvas product path. A constant rather than a ScriptComponent property
-- because the runtime applies no .lua property default (an unbaked property reads
-- nil and logs a "property not found" stack trace), and the path is fixed.
local HUD_CANVAS = "diorama/twinstick/ui/hud.uicanvas"

local TwinStickGame = {
    Properties = {},
}

function TwinStickGame:OnActivate()
    self.befriended = 0
    self.scoreElement = EntityId()  -- the "ScoreText" UI element, resolved below

    -- Load the HUD canvas and resolve the score text element. UiCanvasBus reflects
    -- "FindElementByName" to script bound to the C++ FindElementEntityIdByName, so it
    -- returns the element's EntityId directly (the id UiTextBus is addressed by) -- do
    -- NOT call :GetId() on it. Guarded so the game still runs if the canvas is missing.
    self.canvasId = UiCanvasManagerBus.Broadcast.LoadCanvas(HUD_CANVAS)
    if self.canvasId ~= nil and self.canvasId:IsValid() then
        self.scoreElement = UiCanvasBus.Event.FindElementByName(self.canvasId, "ScoreText")
    end

    -- Poll the shared befriended count each tick and refresh the HUD when it changes.
    self.tickHandler = TickBus.Connect(self)

    -- Game controls. Esc quits the launcher (otherwise the window is awkward to
    -- close); P toggles pause. Pause is a shared global the gameplay tick scripts
    -- honor (player/enemy/projectile/spawner/fx early-out when it is set). This
    -- controller is its own entity, so its input handlers keep working while paused.
    self.idQuit = InputEventNotificationId("quit")
    self.idPause = InputEventNotificationId("pause")
    self.hQuit = InputEventNotificationBus.Connect(self, self.idQuit)
    self.hPause = InputEventNotificationBus.Connect(self, self.idPause)

    self:UpdateHud()
end

function TwinStickGame:OnDeactivate()
    if self.tickHandler then self.tickHandler:Disconnect() end
    if self.hQuit then self.hQuit:Disconnect() end
    if self.hPause then self.hPause:Disconnect() end
    -- Clear the shared pause flag so a reloaded level never starts paused.
    _G.TwinStickPaused = false
    if self.canvasId ~= nil and self.canvasId:IsValid() then
        UiCanvasManagerBus.Broadcast.UnloadCanvas(self.canvasId)
    end
end

function TwinStickGame:OnPressed(value)
    local id = InputEventNotificationBus.GetCurrentBusId()
    if id == self.idQuit then
        self:BeginQuit()
    elseif id == self.idPause then
        _G.TwinStickPaused = not rawget(_G, "TwinStickPaused")
        self:UpdateHud()
    end
end

-- Destroy every entity id held in a shared registry. Snapshot the ids first: each
-- destroy runs the entity's OnDeactivate, which mutates the registry, so iterating
-- it directly while destroying would skip entries.
function TwinStickGame:DestroyTracked(registry)
    if registry == nil then
        return
    end
    local ids = {}
    for _, id in pairs(registry) do
        if id ~= nil then
            table.insert(ids, id)
        end
    end
    for _, id in ipairs(ids) do
        if id:IsValid() then
            GameEntityContextRequestBus.Broadcast.DestroyGameEntityAndDescendants(id)
        end
    end
end

-- Local stopgap for the shutdown crash (o3de/o3de#19804/#19805): a scripted entity
-- that is still alive at exit is torn down AFTER the Lua VM is freed, use-after-freeing
-- it. So on quit we freeze, destroy every live spawned scripted entity NOW (while the
-- VM is alive, so they deactivate cleanly), wait a few ticks for the spawn manager to
-- process the destroys, then quit. NOTE: partial -- closing the window skips this path,
-- and it only clears spawned entities; remove once the engine fix lands.
function TwinStickGame:BeginQuit()
    if self.quitting then
        return
    end
    self.quitting = true
    self.quitTicks = 4
    _G.TwinStickPaused = true
    self:DestroyTracked(rawget(_G, "TwinStickFXPool"))
    self:DestroyTracked(rawget(_G, "TwinStickEnemies"))
    self:DestroyTracked(rawget(_G, "TwinStickProjectiles"))
    self:UpdateHud()
end

function TwinStickGame:OnTick(deltaTime, scriptTime)
    if self.quitting then
        -- Give the spawn manager a few frames to process the destroys before quitting.
        self.quitTicks = self.quitTicks - 1
        if self.quitTicks <= 0 then
            ConsoleRequestBus.Broadcast.ExecuteConsoleCommand("quit")
            self.quitting = false -- quit is queued; do not issue it again
        end
        return
    end
    -- Each hater bumps this global when it floats away happy (twin_stick_enemy.lua).
    local count = rawget(_G, "TwinStickBefriended") or 0
    if count ~= self.befriended then
        self.befriended = count
        self:UpdateHud()
    end
end

function TwinStickGame:UpdateHud()
    if self.scoreElement ~= nil and self.scoreElement:IsValid() then
        local text = "Befriended: " .. tostring(self.befriended)
        if self.quitting then
            text = text .. "   [quitting...]"
        elseif rawget(_G, "TwinStickPaused") then
            text = text .. "   [PAUSED - press P]"
        end
        UiTextBus.Event.SetText(self.scoreElement, text)
    end
end

return TwinStickGame
