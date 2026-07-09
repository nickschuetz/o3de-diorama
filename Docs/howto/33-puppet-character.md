# How-To: An Animated 2D Character (Bone-Driven Puppet)

Build a little humanoid puppet that idles in the world: a readable figure - head, torso,
jointed arms and legs - whose parts are moved by a bone hierarchy, so it sways, swings its
arms, and bobs. This is the bone-driven counterpart to the [surface-deform](32-surface-deform.md)
water demo (which warps a mesh); here rigid quads ride a skeleton, the classic cutout
character. It uses the same **Skinned Sprite (mesh deform)** component and the open
DragonBones format, and it is generated entirely from a script, so there is no third-party
art.

## Generate the rig

```
python3 scripts/gen_puppet_rig.py
```

writes three files under `Assets/Diorama/Examples/Skinned/`:

- `puppet_ske.json` - the armature: an 11-bone humanoid tree (pelvis, torso, head, two arms
  with elbows, two legs with knees) plus the mesh and an `idle` clip.
- `puppet_tex.json` + `puppet_tex.png` - the atlas and a character texture (a face, a
  shirt, skin, and pants, laid out in four regions).

How the rig works, if you want to author your own:

- **Bones** are a tree of local transforms (`parent` + `transform`), listed parent-first.
- **The body is one mesh** of ten quads, one per limb. Every vertex of a limb quad is
  weighted fully (`[1, boneIndex, 1.0]`) to that limb's bone, so the quad moves rigidly with
  the bone - the cutout technique, expressed in the weighted-mesh format. `bonePose` gives
  each bone's bind-pose world so skinning places the quads correctly.
- **The quads' UVs** address the four texture regions, which is what gives the figure a
  face and clothes rather than flat color.
- **The `idle` clip** is per-bone rotate keyframes (torso sways, head counter-sways, arms
  swing in opposition, legs follow) plus a root translate (the bob), sampled from sines and
  closed into a seamless loop.

## Run the demo

```
<engine>/bin/Linux/profile/Default/Editor \
  --project-path=/path/to/YourProject \
  --runpython /path/to/o3de-diorama/Docs/examples/puppet_demo.py
```

builds a `DioramaPuppetDemo` level with the puppet, a soft backdrop, and a `DemoCamera`. Be
`DemoCamera` (or Game -> Simulate) to watch the idle loop. Because the animated geometry
keeps the viewport live, it also plays in the editor preview.

The demo bakes the rig and texture references plus `animationName = "idle"` into the saved
prefab. To drive it from script instead, play any clip by name over the request bus:

```lua
DioramaSkinnedSpriteRequestBus.Event.PlayAnimation(self.entityId, "idle", true)
```

and layer a pose on top (for example, raise an arm) with `SetBoneRotation`:

```lua
-- bend the left upper arm 40 degrees on top of the idle
DioramaSkinnedSpriteRequestBus.Event.SetBoneRotation(self.entityId, "armL_up", 40.0)
```

See the [Skinned Sprite component reference](../reference/skinned-sprite-component.md) for
every parameter and verb.
