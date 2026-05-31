"""
Build the twin-stick player entity in the open level.

This is the AI-native build path: an agent (or a developer) runs this in the
editor to assemble the player from standard O3DE components plus a Diorama
sprite. It is step 1 of the twin-stick sample game (teaching ladder rung 6);
later steps add enemies, projectiles, and a HUD.

The player is built like a real O3DE game entity:
  - Transform                  (placement)
  - Diorama Sprite             (the visual; billboards to face the top-down camera)
  - PhysX Dynamic Rigid Body   (movement integrated by physics; gravity off)
  - PhysX Primitive Collider   (so walls and enemies collide)
  - Lua Script                 (twin_stick_player.lua: input -> velocity + facing)
  - Input                      (twin_stick.inputbindings: WASD/mouse/gamepad)

The Diorama sprite is configured through the typed DioramaSpriteRequestBus (no
property-path strings). The standard O3DE components are added by type and their
asset properties are set best-effort; see Samples/TwinStick/README.md for the
authoritative component/property spec to set by hand or in a prefab.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Samples/TwinStick/build_player.py
"""
import azlmbr
import azlmbr.asset as asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

diorama = azlmbr.diorama

PLAYER_TEXTURE = "diorama/textures/sample_sprite.png"
PLAYER_SCRIPT = "diorama/twinstick/scripts/twin_stick_player.lua"
# The Lua Script component references the compiled product (.luac), not the source.
PLAYER_SCRIPT_PRODUCT = "diorama/twinstick/scripts/twin_stick_player.luac"
PLAYER_INPUT = "diorama/twinstick/input/twin_stick.inputbindings"


def log(msg):
    print("DIORAMA_TWINSTICK: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


def add_component(eid, display_name):
    type_id = find_type_id(display_name)
    if type_id is None:
        log("WARN: component type not found: {}".format(display_name))
        return None
    outcome = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, [type_id])
    if not outcome.IsSuccess():
        log("WARN: failed to add component: {}".format(display_name))
        return None
    log("added component: {}".format(display_name))
    return outcome.GetValue()[0]


def asset_id(product_path):
    # Null type Uuid matches by product path regardless of asset type.
    return asset.AssetCatalogRequestBus(bus.Broadcast, "GetAssetIdByPath", product_path, math.Uuid(), False)


def set_property(component_id, path, value):
    # Best-effort: log a warning instead of failing the whole build if a property
    # path differs in this engine version.
    if component_id is None:
        return False
    ok = editor.EditorComponentAPIBus(bus.Broadcast, "SetComponentProperty", component_id, path, value).IsSuccess()
    if not ok:
        log("WARN: could not set property '{}'".format(path))
    return ok


def open_default_level():
    general.idle_enable(True)
    try:
        general.open_level_no_prompt("DefaultLevel")
    except Exception as e:
        log("open_level raised: {}".format(e))
    waited = 0
    while general.get_current_level_name() in ("", "Untitled") and waited < 400:
        general.idle_wait_frames(1)
        waited += 1
    general.idle_wait_frames(20)


def main():
    log("start")
    open_default_level()

    if find_type_id("Sprite") is None:
        log("FAIL: Diorama Sprite component not found (is the Diorama gem enabled?)")
        return

    # Create the player slightly above the arena floor.
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", math.Vector3(0.0, 0.0, 0.5), azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, "Player")

    # Visual: Diorama sprite, configured through the typed bus. Billboard so it
    # faces the top-down camera; facing left/right is handled at runtime by the
    # Lua script flipping the sprite.
    add_component(eid, "Sprite")
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, PLAYER_TEXTURE)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 1.0, 1.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", eid, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", eid, 10.0)

    # Gameplay: standard O3DE components, wired through the editor API so the
    # player is playable without manual steps. Display names match the editor's
    # Add Component menu (verified in 26.05).
    rigid_body = add_component(eid, "PhysX Dynamic Rigid Body")
    # Top-down game: no gravity; motion is velocity-driven in the XY plane.
    set_property(rigid_body, "Configuration|Gravity enabled", False)

    # The collider lets the player bump arena walls and enemies. If this warns,
    # the collider's Add Component display name differs in your engine version;
    # add it by hand and size it to the sprite.
    add_component(eid, "PhysX Primitive Collider")

    lua = add_component(eid, "Lua Script")
    set_property(lua, "Script", asset_id(PLAYER_SCRIPT_PRODUCT))

    # Component display name is "Input" (StartingPointInput); the bindings-asset
    # property is "Input to event bindings".
    inp = add_component(eid, "Input")
    set_property(inp, "Input to event bindings", asset_id(PLAYER_INPUT))

    tag = add_component(eid, "Tag")
    set_property(tag, "Tags", ["Player"])

    # Verify the Diorama side took (no screenshot needed).
    info = diorama.DioramaSpriteRequestBus(bus.Event, "GetSpriteInfo", eid)
    log("sprite texturePath={} billboard={} size={}x{}".format(
        info.texturePath, info.billboard, info.width, info.height))
    log("player entity id={}".format(eid.ToString() if hasattr(eid, "ToString") else eid))
    log("done")


main()
