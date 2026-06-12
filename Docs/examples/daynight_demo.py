"""
Day/night cycle: a small lit scene whose sun marches through the day.

Composes the day/night feature into its own level:
- a directional "sun" entity carrying a 2D Light plus a Day/Night Cycle that drives the
  light's color / intensity / direction over a fast 12-second day,
- a ground strip and a few billboarded props the sun lights, so the color shift from
  dawn to noon to dusk to night is visible,
- two lamp entities (2D point lights) that the daynight_lamp.lua script switches on at
  night by reading the cycle.

It is both a portfolio sample and an integration check of the day/night feature. The
cycle runs at game time; this builds the scene, then you enter game mode to watch the
day pass (and attach the lamp script to see the lamps react).

Setup after running (see Docs/howto/28-day-night.md):
  1. (Optional) Add a Lua Script to each Lamp entity, script
     diorama/examples/daynight/daynight_lamp.lua, and set its CycleEntity property to
     the Sun entity.
  2. Place a camera (or select an existing one -> Be this camera), then enter game mode.

Run in the editor:
  <engine>/bin/Linux/profile/Default/Editor \
    --project-path=/path/to/YourProject \
    --runpython /path/to/o3de-diorama/Docs/examples/daynight_demo.py
"""
import azlmbr
import azlmbr.asset
import azlmbr.bus as bus
import azlmbr.editor as editor
import azlmbr.entity
import azlmbr.legacy.general as general
import azlmbr.math as math

diorama = azlmbr.diorama

LEVEL_NAME = "DioramaDayNight"
QUAD_TEXTURE = "diorama/textures/white_sprite.png"
PROP_TEXTURE = "diorama/textures/o3de_mascot.png"


def log(msg):
    print("DIORAMA_DAYNIGHT: {}".format(msg))


def find_type_id(name):
    ids = editor.EditorComponentAPIBus(bus.Broadcast, "FindComponentTypeIdsByEntityType", [name], 0)
    if ids and len(ids) > 0 and not ids[0].IsNull():
        return ids[0]
    return None


def open_or_create_level(level_name):
    """Open the named level, creating a fresh one from a built-in template if it does
    not exist, so the example builds in its OWN level and never lands on top of another
    scene (this project's DefaultLevel is the twin-stick sample)."""
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


def make_entity(name, position, type_ids):
    eid = editor.ToolsApplicationRequestBus(
        bus.Broadcast, "CreateNewEntityAtPosition", position, azlmbr.entity.EntityId())
    editor.EditorEntityAPIBus(bus.Event, "SetName", eid, name)
    outcome = editor.EditorComponentAPIBus(bus.Broadcast, "AddComponentsOfType", eid, type_ids)
    comps = outcome.GetValue() if outcome.IsSuccess() else []
    return eid, comps


def set_prop(comp, path, value):
    editor.EditorComponentAPIBus(bus.Broadcast, "SetComponentProperty", comp, path, value)


def main():
    log("start")
    if not open_or_create_level(LEVEL_NAME):
        log("FAIL: could not open or create the demo level '{}'".format(LEVEL_NAME))
        return
    general.idle_wait_frames(20)

    sprite_type = find_type_id("Sprite")
    light_type = find_type_id("2D Light")
    daynight_type = find_type_id("Day/Night Cycle")
    if sprite_type is None or light_type is None or daynight_type is None:
        log("FAIL: required Diorama components not found (rebuild the gem?)")
        return

    # Ground strip + a row of props the sun lights, so the color/intensity sweep reads.
    ground, _ = make_entity("Ground", math.Vector3(0.0, -6.0, 1.0), [sprite_type])
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", ground, QUAD_TEXTURE)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", ground, 60.0, 4.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", ground, True)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetTint", ground, 0.6, 0.6, 0.62, 1.0)
    diorama.DioramaSpriteRequestBus(bus.Event, "SetSortOffset", ground, -8.0)

    for i, px in enumerate((-9.0, -3.0, 3.0, 9.0)):
        eid, _ = make_entity("Prop{}".format(i + 1), math.Vector3(px, -2.5, 1.0), [sprite_type])
        diorama.DioramaSpriteRequestBus(bus.Event, "SetTextureByPath", eid, PROP_TEXTURE)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetSize", eid, 3.0, 4.0)
        diorama.DioramaSpriteRequestBus(bus.Event, "SetBillboard", eid, True)

    # The sun: a directional 2D Light driven by a Day/Night Cycle. Fast 12s day, start at
    # dawn so the brightening is the first thing you see. Phase colors keep their defaults.
    # The light is configured through its typed bus (as lighting_demo.py does, avoiding
    # enum/math property paths); the cycle's timing is config set by property (the cycle
    # has no edit-time bus handler).
    sun, sun_comps = make_entity("Sun", math.Vector3(0.0, 8.0, 6.0), [light_type, daynight_type])
    diorama.DioramaLightRequestBus(bus.Event, "SetDirectional", sun, True)  # a sun
    diorama.DioramaLightRequestBus(bus.Event, "SetColor", sun, 1.0, 1.0, 1.0)
    diorama.DioramaLightRequestBus(bus.Event, "SetIntensity", sun, 1.0)
    if len(sun_comps) >= 2:
        set_prop(sun_comps[1], "Config|Cycle Seconds", 12.0)
        set_prop(sun_comps[1], "Config|Time Of Day", 0.25)

    # Two lamps: warm point lights the daynight_lamp.lua script lights at night. Authored
    # off (intensity 0); the script (attached by hand) turns them on after sunset.
    for i, lx in enumerate((-8.0, 8.0)):
        lamp = make_entity("Lamp{}".format(i + 1), math.Vector3(lx, -1.0, 2.0), [light_type])[0]
        diorama.DioramaLightRequestBus(bus.Event, "SetDirectional", lamp, False)  # point
        diorama.DioramaLightRequestBus(bus.Event, "SetColor", lamp, 1.0, 0.75, 0.4)
        diorama.DioramaLightRequestBus(bus.Event, "SetIntensity", lamp, 0.0)
        diorama.DioramaLightRequestBus(bus.Event, "SetRadius", lamp, 7.0)

    general.idle_wait_frames(40)
    if general.get_current_level_name() == LEVEL_NAME:
        try:
            general.save_level()
        except Exception as e:
            log("save_level raised: {}".format(e))
    else:
        log("WARN: current level is '{}', not '{}'; skipping save to avoid overwriting it".format(
            general.get_current_level_name(), LEVEL_NAME))

    log("Day/night scene built in level '{}'.".format(LEVEL_NAME))
    log("Point a camera at the props and enter game mode: the Sun's Day/Night Cycle runs")
    log("a 12s day, sweeping the light from dawn through noon, dusk, and night. Attach")
    log("diorama/examples/daynight/daynight_lamp.lua to each Lamp (CycleEntity = Sun) to")
    log("see the lamps switch on after sunset.")
    log("done")


main()
