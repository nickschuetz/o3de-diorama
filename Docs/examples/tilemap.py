"""
Build a Diorama tilemap entirely through the AI-native DioramaTilemapRequestBus,
then verify the result with GetTilemapInfo (no screenshot needed).

A tilemap is a grid of cells, each drawing one cell of a shared atlas texture.
Every cell goes through the same batched sprite feature processor, so a whole
layer that shares the atlas collapses into a single draw call. This is teaching
ladder rung 4, and a tilemap like this makes a natural arena floor for a
top-down game.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/tilemap.py
"""
import azlmbr
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

# The Diorama buses are reflected under azlmbr.diorama (the bus's Module
# attribute). Access it as an attribute of azlmbr rather than importing it.
diorama = azlmbr.diorama

# atlas_2x2.png cells (left-to-right, top-to-bottom): 0 red, 1 green, 2 blue,
# 3 yellow. A 2x2 atlas means atlas columns = rows = 2.
RED, GREEN, BLUE, YELLOW = 0, 1, 2, 3


def log(msg):
    print("DIORAMA_TILEMAP: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


LEVEL_NAME = "DioramaTilemap"


def open_or_create_level(level_name):
    """Open the named level, creating a fresh one from a built-in template if it
    does not exist, so the example builds in its OWN level and never lands on top
    of another scene (this project's DefaultLevel is the twin-stick sample)."""
    general.idle_enable(True)
    # Wait for the editor to finish booting and load its startup level FIRST, so our
    # open/create is not clobbered by a late default-level load (which would land
    # our entities in DefaultLevel -- the twin-stick scene -- and, if we then saved,
    # overwrite it).
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
        # create_level_no_prompt(templateName, levelName, heightmapRes, unitSize,
        # terrainTexSize, useTerrain); Default_Level gives the standard Atom env.
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

    # Final guard: ensure our level is current and stayed so (re-open once if a late
    # load drifted us off it).
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
    # A level must be open before creating entities (creating one with no
    # level/root prefab open asserts in the engine's prefab system). Build into our
    # own level so this never lands on top of another scene.
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
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, "ArenaTilemap")
    editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, [tilemap_type])

    # Configure the tilemap through typed, forgiving verbs. No property-path
    # strings, no math types to construct.
    columns, rows = 8, 8
    diorama.DioramaTilemapRequestBus(bus.Event, "SetAtlasByPath", eid, "diorama/textures/atlas_2x2.png")
    diorama.DioramaTilemapRequestBus(bus.Event, "SetAtlasGrid", eid, 2, 2)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetGridSize", eid, columns, rows)
    diorama.DioramaTilemapRequestBus(bus.Event, "SetTileSize", eid, 1.0, 1.0)

    # Paint a checkerboard floor with a blue border (a tiny arena).
    for row in range(rows):
        for column in range(columns):
            border = row == 0 or row == rows - 1 or column == 0 or column == columns - 1
            tile = BLUE if border else (RED if (row + column) % 2 == 0 else YELLOW)
            diorama.DioramaTilemapRequestBus(bus.Event, "SetTile", eid, column, row, tile)

    # Close the loop: query resolved state instead of eyeballing a screenshot.
    info = diorama.DioramaTilemapRequestBus(bus.Event, "GetTilemapInfo", eid)
    log("atlasPath={}".format(info.atlasPath))
    log("grid={}x{} atlas={}x{}".format(info.columns, info.rows, info.atlasColumns, info.atlasRows))
    log("tileSize={}x{} sortOffset={}".format(info.tileWidth, info.tileHeight, info.sortOffset))
    log("filledTileCount={} atlasLoaded={} visible={}".format(info.filledTileCount, info.atlasLoaded, info.visible))

    expected_filled = columns * rows  # every cell painted
    ok = (info.columns == columns and info.rows == rows and info.filledTileCount == expected_filled)
    log("config readback {}".format("OK" if ok else "MISMATCH"))

    # Tip: the grid lies in the entity's local X/Z plane. Rotate the entity -90
    # degrees about X to lay it flat as a floor for a top-down arena.
    log("done")


main()
