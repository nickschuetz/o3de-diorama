#!/usr/bin/env python3
"""Generate twin_stick.inputbindings (O3DE InputEventBindingsAsset ObjectStream).

Authoring note: the mouse and the gamepad right-stick mean DIFFERENT things for
aiming -- the stick gives an absolute held direction (point-to-aim), the mouse
gives only per-frame deltas (O3DE does not expose the cursor position or a screen
unproject to game scripts). So they are bound to SEPARATE events:
  aim_x / aim_y         -> gamepad right stick (absolute direction)
  aimdelta_x / aimdelta_y -> mouse deltas (relative steering, integrated in Lua)
This lets twin_stick_player.lua keep the stick path direct while integrating the
mouse deltas into a persistent aim direction, so each device feels right.

The type/specialization GUIDs are taken verbatim from the editor-authored asset,
so the regenerated file is byte-compatible with what the editor would write.
"""
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "twin_stick.inputbindings")

# GUIDs from the editor-authored asset (do not change).
G_ASSET = "{25971C7A-26E2-4D08-A146-2EFCC1C36B0C}"
G_BINDINGS = "{14FFD4A8-AE46-4E23-B45B-6A7C4F787A91}"
G_VEC = "{2BADE35A-6F1B-4698-B2BC-3373D010020C}"
G_VEC_GROUPS = "{E7A38039-E414-5322-B384-210475DA7732}"
G_VEC_GENS = "{CD522420-8A74-5B73-8FFE-0488D0645663}"
G_GROUP = "{25143B7E-2FEC-4CC5-92FE-270B67E79734}"
G_STR = "{EF8FF807-DDEE-4EB0-B678-4CA3A2C490A4}"
G_STR_SPEC = "{03AAAB3F-5C47-5A66-9EBC-D5FA4DB353C9}"
G_FLOAT = "{EA2C3E90-AFBE-44D4-A90D-FAAF79BAF93D}"
G_INPUT = "{546C9EBC-90EF-4F03-891A-0736BE2A487E}"

# (event name, [(device, input name, multiplier, dead zone), ...])
EVENTS = [
    ("move_x", [("keyboard", "keyboard_key_alphanumeric_D", 1.0, 0.2),
                ("keyboard", "keyboard_key_alphanumeric_A", -1.0, 0.2),
                ("gamepad", "gamepad_thumbstick_l_x", 1.0, 0.2)]),
    ("move_y", [("keyboard", "keyboard_key_alphanumeric_W", 1.0, 0.2),
                ("keyboard", "keyboard_key_alphanumeric_S", -1.0, 0.2),
                ("gamepad", "gamepad_thumbstick_l_y", 1.0, 0.2)]),
    # Absolute aim direction: gamepad right stick, plus arrow keys as a digital
    # 8-way aim (they feed the same absolute-direction path as the stick).
    ("aim_x", [("gamepad", "gamepad_thumbstick_r_x", 1.0, 0.2),
               ("keyboard", "keyboard_key_navigation_arrow_right", 1.0, 0.0),
               ("keyboard", "keyboard_key_navigation_arrow_left", -1.0, 0.0)]),
    ("aim_y", [("gamepad", "gamepad_thumbstick_r_y", 1.0, 0.2),
               ("keyboard", "keyboard_key_navigation_arrow_up", 1.0, 0.0),
               ("keyboard", "keyboard_key_navigation_arrow_down", -1.0, 0.0)]),
    # Mouse deltas = relative aim steering (integrated in the player script).
    ("aimdelta_x", [("mouse", "mouse_delta_x", 0.1, 0.0)]),
    ("aimdelta_y", [("mouse", "mouse_delta_y", -0.1, 0.0)]),
    # Fire: left mouse, gamepad right trigger, or spacebar.
    ("fire", [("mouse", "mouse_button_left", 1.0, 0.0),
              ("gamepad", "gamepad_trigger_r2", 1.0, 0.2),
              ("keyboard", "keyboard_key_edit_space", 1.0, 0.0)]),
    # Quit the game (closes the launcher): Esc or gamepad Start.
    ("quit", [("keyboard", "keyboard_key_edit_escape", 1.0, 0.0),
              ("gamepad", "gamepad_button_start", 1.0, 0.0)]),
    # Pause / resume: P or gamepad Select.
    ("pause", [("keyboard", "keyboard_key_alphanumeric_P", 1.0, 0.0),
               ("gamepad", "gamepad_button_select", 1.0, 0.0)]),
]


def s(field, value):
    return (f'<Class name="AZStd::string" field="{field}" type="{G_STR}" '
            f'value="{value}" specializationTypeId="{G_STR_SPEC}"/>')


def f(field, value):
    return (f'<Class name="float" field="{field}" type="{G_FLOAT}" '
            f'value="{value:.7f}" specializationTypeId="{G_FLOAT}"/>')


def generator(device, name, mult, dz):
    return (
        f'<Class name="Input" field="element" type="{G_INPUT}" version="2" specializationTypeId="{G_INPUT}">'
        f'{s("Input Device Type", device)}{s("Input Name", name)}'
        f'{f("Event Value Multiplier", mult)}{f("Dead Zone", dz)}'
        f'</Class>')


def group(name, gens):
    inner = "".join(generator(*g) for g in gens)
    return (
        f'<Class name="InputEventGroup" field="element" type="{G_GROUP}" version="1" specializationTypeId="{G_GROUP}">'
        f'{s("Event Name", name)}'
        f'<Class name="AZStd::vector" field="Event Generators" type="{G_VEC}" specializationTypeId="{G_VEC_GENS}">'
        f'{inner}</Class></Class>')


def main():
    groups = "".join(group(n, g) for n, g in EVENTS)
    doc = (
        '<ObjectStream version="2">'
        f'<Class name="InputEventBindingsAsset" type="{G_ASSET}" specializationTypeId="{G_ASSET}">'
        f'<Class name="InputEventBindings" field="Bindings" type="{G_BINDINGS}" version="1" specializationTypeId="{G_BINDINGS}">'
        f'<Class name="AZStd::vector" field="Input Event Groups" type="{G_VEC}" specializationTypeId="{G_VEC_GROUPS}">'
        f'{groups}</Class></Class></Class></ObjectStream>\n')
    with open(OUT, "w") as fh:
        fh.write(doc)
    print("wrote", OUT, "(%d events)" % len(EVENTS))


if __name__ == "__main__":
    main()
