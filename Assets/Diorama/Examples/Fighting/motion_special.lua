-- Motion-input special move: the classic "quarter-circle-forward + punch =
-- fireball" gate. Put this on a character entity that has a Diorama 2D Input Actions
-- component configured with:
--   * an Axis2D action named "move" bound to the stick / WASD (up must be +Y,
--     forward +X), set as the component's Direction action,
--   * a Button action named "punch",
--   * a motion named "qcf" with sequence 236 (down, down-forward, forward).
--
-- The detector records the stick direction each frame and recognizes the motion;
-- this script reads it the buffer-friendly way: when punch is pressed, ask whether
-- the motion was performed recently. If so the punch becomes the special.

local MotionSpecial = {
    Properties = {
        MotionName = { default = "qcf", description = "Motion that upgrades the punch to a special." },
        PunchAction = { default = "punch", description = "Button action that commits the move." },
    },
}

function MotionSpecial:OnActivate()
    -- Subscribe to action edges (press/release) on this entity's input component.
    self.inputHandler = DioramaInputNotificationBus.Connect(self, self.entityId)
end

function MotionSpecial:OnDeactivate()
    if self.inputHandler ~= nil then
        self.inputHandler:Disconnect()
        self.inputHandler = nil
    end
end

function MotionSpecial:OnActionPressed(action)
    if action ~= self.Properties.PunchAction then
        return
    end
    -- WasMotionPerformed stays true through the motion's buffer window, so gating it
    -- on the button edge is exactly the fighting-game "do the motion, then press" feel.
    if DioramaInputRequestBus.Event.WasMotionPerformed(self.entityId, self.Properties.MotionName) then
        self:DoSpecial()
    else
        self:DoNormal()
    end
end

function MotionSpecial:DoSpecial()
    -- Play the fireball animation, spawn the projectile, etc. The detector did its job:
    -- the motion was recognized and the button confirmed it this frame.
    Debug.Log("Special! (" .. tostring(self.Properties.MotionName) .. " + " .. tostring(self.Properties.PunchAction) .. ")")
end

function MotionSpecial:DoNormal()
    Debug.Log("Normal punch")
end

-- Optional: an event-driven path. OnMotionPerformed fires once the instant a motion
-- completes, for buffering a "the next punch is special" flag or charging meter.
function MotionSpecial:OnMotionPerformed(motion)
    -- e.g. flash the meter to telegraph the buffered special.
end

return MotionSpecial
