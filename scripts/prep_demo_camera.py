#!/usr/bin/env python3
"""
Add a launcher-ready camera to a Diorama demo level prefab, headlessly (no editor).

It copies the twin-stick camera entity's known-good structure (an Atom Camera
component), repositions it, and attaches the make_active_camera.lua script so the
camera designates itself the active render view on the first tick. Without an active
view a non-startup level renders gray in the standalone GameLauncher.

Usage:
  prep_demo_camera.py <LevelName> <tx> <ty> <tz> <rx> <ry> <rz> [--ortho HALFWIDTH]

The rotation is XYZ euler degrees. O3DE camera forward = entity +Y rotated by it
(identity -> +Y; Rotate (-90,0,0) -> -Z, a front view of XY-plane content).
"""
import json, copy, sys, os

# make_active_camera.lua product reference (guid from the lowercased source path).
SCRIPT_GUID = "{99AB7C02-81D9-5D52-918E-7BE26AFB4DF6}"
SCRIPT_HINT = "diorama/examples/make_active_camera.luac"
PROJ = os.environ.get("DIORAMA_PROJECT", os.path.expanduser("~/PROJECTS/DioramaSandbox"))
TWIN = PROJ + "/Levels/DefaultLevel/DefaultLevel.prefab"

def find_named(o, n):
    if isinstance(o, dict):
        if o.get("Name") == n:
            return o
        for v in o.values():
            r = find_named(v, n)
            if r:
                return r
    elif isinstance(o, list):
        for v in o:
            r = find_named(v, n)
            if r:
                return r

def main():
    a = sys.argv
    level = a[1]
    tx, ty, tz, rx, ry, rz = (float(a[i]) for i in range(2, 8))
    ortho = None
    if "--ortho" in a:
        ortho = float(a[a.index("--ortho") + 1])

    pf = "%s/Levels/%s/%s.prefab" % (PROJ, level, level)
    d = json.load(open(pf))
    container = d["ContainerEntity"]["Id"]

    # Source the camera entity structure from the twin-stick camera.
    src = copy.deepcopy(find_named(json.load(open(TWIN)), "TS_CameraNew"))
    assert src, "TS_CameraNew not found in twin-stick prefab"

    # Drop any prior CaptureCam (idempotent re-runs).
    for k in [k for k, e in d["Entities"].items() if e.get("Name") == "CaptureCam"]:
        del d["Entities"][k]

    NEW = "Entity_[770077007700]"
    src["Id"] = NEW
    src["Name"] = "CaptureCam"
    base = 770077007700000000
    i = 0
    for ck, cc in src["Components"].items():
        i += 1
        cc["Id"] = base + i
        if "TransformComponent" in cc.get("$type", ""):
            cc["Parent Entity"] = container
            cc["Transform Data"] = {"Translate": [tx, ty, tz], "Rotate": [rx, ry, rz]}

    # Attach make_active_camera.lua. ScriptEditorComponent needs BOTH the inner
    # ScriptComponent.Script and a sibling top-level ScriptAsset, or the exporter
    # drops the component. Optional Orthographic props frame flat 2D content.
    props = []
    if ortho is not None:
        props = [
            {"$type": "AzFramework::ScriptPropertyBoolean", "id": 1, "name": "Orthographic", "value": True},
            {"$type": "AzFramework::ScriptPropertyNumber", "id": 2, "name": "OrthographicHalfWidth", "value": ortho},
        ]
    asset_ref = {"assetId": {"guid": SCRIPT_GUID, "subId": 1}, "assetHint": SCRIPT_HINT}
    src["Components"]["ActiveCamScript"] = {
        "$type": "ScriptEditorComponent",
        "Id": base + 99,
        "ScriptComponent": {"Properties": {"Properties": props}, "Script": asset_ref},
        "ScriptAsset": asset_ref,
    }

    d["Entities"][NEW] = src
    for ck, cc in d["ContainerEntity"].get("Components", {}).items():
        if "EditorEntitySortComponent" in cc.get("$type", ""):
            order = [e for e in cc.get("Child Entity Order", []) if e in d["Entities"]]
            order.append(NEW)
            cc["Child Entity Order"] = order

    json.dump(d, open(pf, "w"), indent=4)
    print("CaptureCam added to %s at (%s,%s,%s) rot (%s,%s,%s)%s"
          % (level, tx, ty, tz, rx, ry, rz, "" if ortho is None else " ortho hw=%s" % ortho))

if __name__ == "__main__":
    main()
