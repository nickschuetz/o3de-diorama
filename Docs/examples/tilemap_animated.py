"""
Animated tiles: build a Diorama tilemap whose cells cycle through atlas frames,
authored entirely through the AI-native DioramaTilemapRequestBus, then confirm it
with GetTilemapInfo (animatedTileCount) -- no screenshot needed for the readback.

This is the on-screen verification for the tilemap-v2 animated-tile feature. It
reuses the committed 4-cell atlas (atlas_2x2.png: red, green, blue, yellow), fills
a small grid with one painted index, and defines an animation that cycles that
index through all four cells at 2 fps. On screen every cell pulses together
red -> green -> blue -> yellow (one full cycle every 2 seconds), because all cells
sharing a definition advance off one map-wide clock.

The presenter ticks animated tiles in BOTH the editor preview and game mode, so the
viewport animates live as soon as the script runs -- you do not have to enter game
mode to see it. Entering game mode (the script does not) would look identical.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/tilemap_animated.py

Results print to user/log/Editor.log as DIORAMA_ANIMTILE lines.
"""
import azlmbr
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

# The Diorama buses are reflected under azlmbr.diorama (the bus's Module attribute).
diorama = azlmbr.diorama

# atlas_2x2.png cells (left-to-right, top-to-bottom): 0 red, 1 green, 2 blue,
# 3 yellow. The animation cycles a painted tile through all four.
RED, GREEN, BLUE, YELLOW = 0, 1, 2, 3

LEVEL_NAME = "DioramaAnimTiles"
COLUMNS, ROWS = 6, 6
FPS = 2.0  # one full red->green->blue->yellow cycle every 2 seconds (4 frames / 2 fps)


def log(msg):
    print("DIORAMA_ANIMTILE: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


def open_or_create_level(level_name):
    """Open the named level, creating a fresh one from a built-in template if it
    does not exist, so the example builds in its OWN level and never lands on top
    of another scene (this project's DefaultLevel is the twin-stick sample)."""
    general.idle_enable(True)
    booted = 0
    while general.get_current_level_name() in ("", "Untitled") and booted < 600:
        general.idle_wait_frames(1)
        booted += 1
    general.idle_wait_frames(30)

    def now_in(name):
        return general.get_current_level_name() == name

    try:
        general.open_level_no_prompt(level_name)
    except Exception as e:
        log("open_level raised: {}".format(e))
    waited = 0
    while not now_in(level_name) and waited < 200:
        general.idle_wait_frames(1)
        waited += 1

    if not now_in(level_name):
        for template in ("Default_Level", "Empty", "Basic"):
            try:
                general.create_level_no_prompt(template, level_name, 128, 1, 512, False)
            except Exception as e:
                log("create_level('{}') raised: {}".format(template, e))
                continue
            waited = 0
            while not now_in(level_name) and waited < 400:
                general.idle_wait_frames(1)
                waited += 1
            if now_in(level_name):
                break

    general.idle_wait_frames(30)
    if not now_in(level_name):
        try:
            general.open_level_no_prompt(level_name)
        except Exception as e:
            log("reopen raised: {}".format(e))
        waited = 0
        while not now_in(level_name) and waited < 200:
            general.idle_wait_frames(1)
            waited += 1
    return now_in(level_name)


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    tilemap_type = find_type_id("Tilemap")
    if tilemap_type is None:
        log("FAIL: Tilemap component type not found (is the Diorama gem enabled?)")
        return

    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", math.Vector3(0.0, 0.0, 0.0), azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, "AnimatedTilemap")
    editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, [tilemap_type])

    # Configure the tilemap, then fill every cell with the RED index so the whole
    # grid animates as one block.
    diorama.DioramaTilemapRequestBus(bus.Event, "SetAtlasByPath", eid, "diorama/textures/atlas_2x2.png")
    diorama.DioramaTilemapRequestBus(bus.Event, "SetAtlasGrid", eid, 2, 2)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetGridSize", eid, COLUMNS, ROWS)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetTileSize", eid, 1.0, 1.0)
    diorama.DioramaTilemapRequestBus(bus.Event, "Fill", eid, RED)

    # Define the animation: every cell painted RED (0) cycles through cells 0,1,2,3
    # at FPS, looping. Re-defining the same tile index would replace it; an empty
    # frame list would remove it; ClearAnimatedTiles() drops all animation.
    diorama.DioramaTilemapRequestBus(bus.Event, "DefineAnimatedTile", eid, RED, [RED, GREEN, BLUE, YELLOW], FPS, True)

    # Close the loop: confirm the definition registered without a screenshot.
    info = diorama.DioramaTilemapRequestBus(bus.Event, "GetTilemapInfo", eid)
    log("grid={}x{} filledTileCount={} animatedTileCount={}".format(
        info.columns, info.rows, info.filledTileCount, info.animatedTileCount))
    log("atlasLoaded={} visible={}".format(info.atlasLoaded, info.visible))

    expected_filled = COLUMNS * ROWS
    ok = (info.filledTileCount == expected_filled and info.animatedTileCount == 1)
    log("readback {}".format("OK" if ok else "MISMATCH"))

    # On screen: the whole grid should now pulse red -> green -> blue -> yellow in
    # lockstep, one full cycle every 2 seconds. The grid lies in the entity's local
    # X/Z plane; nudge the editor camera to frame the origin if it is off-screen.
    log("WATCH the viewport: all cells cycle red->green->blue->yellow together (2s/cycle).")
    log("done")


main()
