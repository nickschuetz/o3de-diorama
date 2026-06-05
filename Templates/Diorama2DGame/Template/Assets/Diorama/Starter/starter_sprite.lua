-- Diorama 2.5D starter: turn the entity this script is on into a world-space sprite,
-- entirely through the DioramaSpriteRequestBus (the same API a human edits in the
-- Inspector). Attach this script (Lua Script component) to an entity that also has a
-- Diorama Sprite component, then enter game mode.
--
-- This is the smallest possible "it works" example. See the Diorama how-tos for the
-- full feature set (animation, tilemaps, camera, lighting, particles, HUD, and more);
-- every feature has a request bus you drive exactly like this.

local starter_sprite = {
    Properties = {
        -- Point this at your own texture's product path, e.g. "mygame/textures/hero.png".
        -- Until you add art, the sprite shows untextured (a tinted quad).
        TexturePath = { default = "" },
        Width = { default = 1.0 },
        Height = { default = 1.0 },
    },
}

function starter_sprite:OnActivate()
    local entityId = self.entityId

    if self.Properties.TexturePath ~= "" then
        DioramaSpriteRequestBus.Event.SetTextureByPath(entityId, self.Properties.TexturePath)
    end

    DioramaSpriteRequestBus.Event.SetSize(entityId, self.Properties.Width, self.Properties.Height)
    -- A gentle tint so the quad is visible even before you add a texture.
    DioramaSpriteRequestBus.Event.SetTint(entityId, 0.4, 0.7, 1.0, 1.0)
    -- Billboard so it always faces the camera (typical for 2.5D characters).
    DioramaSpriteRequestBus.Event.SetBillboard(entityId, true)
end

function starter_sprite:OnDeactivate()
end

return starter_sprite
