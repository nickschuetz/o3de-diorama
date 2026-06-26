"""
Palette recolor demo: one sprite sheet, four color identities.

Builds a row of the SAME sprite (the O3DE mascot) in its own level. Each copy is
recolored at GAME-FREE build time straight off the bus: the leftmost keeps its own
colors (palette strength 0, the reference), and the other three remap luminance
through a three-stop shadow -> mid -> highlight ramp (P1 cool, P2 warm, Alt green)
at full strength. This is the no-index-art way to get P1 / P2 / alt-color variants
from a single sheet (DioramaSpriteRequestBus.SetPaletteStrength + SetPaletteColors).

The recolor is applied in the sprite shader before lighting, so it reads as a true
palette swap (not a tint multiply). Nothing here needs game mode or a script: the
row is recolored the moment the level opens. Look at it in the editor viewport, or
be the DemoCamera and enter game mode.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/palette_demo.py
"""
import azlmbr
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

diorama = azlmbr.diorama

LEVEL_NAME = "DioramaPaletteDemo"
SPRITE_TEXTURE = "diorama/textures/o3de_mascot.png"

# (label, strength, shadow rgb, mid rgb, highlight rgb)
VARIANTS = [
    ("Original", 0.0, (0.0, 0.0, 0.0), (0.5, 0.5, 0.5), (1.0, 1.0, 1.0)),
    ("P1 cool", 1.0, (0.05, 0.07, 0.25), (0.15, 0.45, 0.85), (0.75, 0.95, 1.0)),
    ("P2 warm", 1.0, (0.22, 0.04, 0.04), (0.80, 0.22, 0.12), (1.0, 0.92, 0.55)),
    ("Alt green", 1.0, (0.04, 0.16, 0.06), (0.18, 0.60, 0.22), (0.80, 1.0, 0.70)),
]


def log(msg):
    print("DIORAMA_PALETTE: {}".format(msg))


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


def make_entity(name, pos, type_ids):
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", pos, azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    if type_ids:
        editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    return eid


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create the demo level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite_type = find_type_id("Sprite")
    if sprite_type is None:
        log("FAIL: Diorama Sprite component not found (is the Diorama gem enabled?)")
        return
    cam_ctrl_type = find_type_id("2D Camera Controller")
    atom_camera_type = find_type_id("Camera")

    spacing = 4.5
    n = len(VARIANTS)
    for i, (label, strength, sh, mid, hi) in enumerate(VARIANTS):
        x = (i - (n - 1) * 0.5) * spacing
        e = make_entity("Mascot_{}".format(label.replace(" ", "_")), math.Vector3(x, 0.0, 1.0), [sprite_type])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", e, SPRITE_TEXTURE)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", e, 3.0, 3.85)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", e, True)
        if strength > 0.0:
            diorama.DioramaSpriteRequestBus(
                bus.Event, "SetPaletteColors", e,
                sh[0], sh[1], sh[2], mid[0], mid[1], mid[2], hi[0], hi[1], hi[2])
            diorama.DioramaSpriteRequestBus(bus.Event, "SetPaletteStrength", e, strength)
        log("built '{}' (strength {})".format(label, strength))

    cam_types = ([cam_ctrl_type] if cam_ctrl_type is not None else []) + \
        ([atom_camera_type] if atom_camera_type is not None else [])
    make_entity("DemoCamera", math.Vector3(0.0, -16.0, 1.5), cam_types)

    general.idle_wait_frames(30)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Built {} recolored mascots in level '{}'.".format(n, LEVEL_NAME))
    log("Leftmost = original colors; the other three are P1 / P2 / Alt palette ramps.")
    log("Look in the viewport, or select DemoCamera -> Be this camera -> enter game mode.")
    log("done")


main()
