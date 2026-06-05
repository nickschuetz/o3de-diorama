----------------------------------------------------------------------------------------------------
-- Copyright (c) Contributors to the Open 3D Engine Project.
-- For complete copyright and license terms please see the LICENSE at the root of this distribution.
--
-- SPDX-License-Identifier: Apache-2.0 OR MIT
----------------------------------------------------------------------------------------------------

-- Pooled hit-burst shard for the Diorama twin-stick sample.
--
-- This sprite does NOT connect any EBus handler (no TickBus) and is never spawned
-- or destroyed during gameplay: the FX manager (twin_stick_fx.lua) pre-spawns a
-- pool of them once, then reuses them for every explosion, animating each by id.
-- It exists only to register itself in a shared pool so the manager can find it.
--
-- Why: driving N short-lived script entities (each TickBus.Connect + self-destruct)
-- per kill churns Lua EBus handlers, whose GC finalization corrupts the heap. See
-- o3de/o3de#19802. One long-lived manager handler over a reused pool avoids that
-- entirely. (It is also just better practice: no per-effect spawn/destroy/GC.)

TwinStickFXPool = rawget(_G, "TwinStickFXPool") or {}
_G.TwinStickFXPool = TwinStickFXPool

local TwinStickFxShard = {}

function TwinStickFxShard:OnActivate()
    self.key = tostring(self.entityId)
    TwinStickFXPool[self.key] = self.entityId
    -- Billboard, and start hidden (zero size) until the manager activates us.
    DioramaSpriteRequestBus.Event.SetBillboard(self.entityId, true)
    DioramaSpriteRequestBus.Event.SetSize(self.entityId, 0.0, 0.0)
end

function TwinStickFxShard:OnDeactivate()
    if self.key then
        TwinStickFXPool[self.key] = nil
    end
end

return TwinStickFxShard
