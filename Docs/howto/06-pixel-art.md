# How-To: Crisp Pixel Art (point filtering + no mipmaps)

By default Atom samples textures with **linear filtering** and generates
**mipmaps**, both of which smooth an image. That is what you want for
photographic art, but it blurs pixel art: a 16x16 sprite scaled up on screen
turns into a soft, muddy blob instead of crisp, blocky pixels.

Getting crisp pixel art in Diorama has two halves, and you usually want both:

1. **The sampler** (magnification): nearest-neighbor instead of linear, so an
   enlarged texel stays a hard square. This is the Sprite component's
   **Point Filter** toggle.
2. **Mipmaps** (minification): a linear-mipped texture still softens when the
   sprite is drawn smaller than the source, because the GPU reads a pre-averaged
   mip. For crispness at any scale, import the texture with **no mipmaps**.

A ready example ships at `Assets/Diorama/Textures/pixel_sprite.png` (a 16x16
sprite).

## 1. Turn on Point Filter

In the editor, select the entity, find the **Sprite** component, and enable
**Point Filter (pixel art)**.

From a script, Python, or Script Canvas, use the bus verb:

```lua
DioramaSpriteRequestBus.Event.SetPointFilter(self.entityId, true)
```

That alone fixes the common case: a small texture drawn large now shows crisp,
blocky pixels instead of a bilinear smear.

## 2. Import the texture with no mipmaps

Point filtering fixes magnification; mipmaps are the other half. Two ways:

**A. In the editor (recommended):** right-click the texture in the Asset Browser
and open **Texture Settings**. Either set the **Preset** to
`UserInterface_Lossless` (a built-in preset that is uncompressed, sRGB, and
generates no mipmaps), or uncheck **Create Mipmaps** on your current preset.
Click **Apply**.

**B. With a `.assetinfo` sidecar (scriptable):** drop a file named
`<texture>.png.assetinfo` next to the texture so the Asset Processor applies the
no-mip preset without opening the dialog:

```xml
<ObjectStream version="3">
	<Class name="TextureSettings" version="2" type="{980132FF-C450-425D-8AE0-BD96A8486177}">
		<Class name="AZ::Uuid" field="PresetID" value="{83003128-F63E-422B-AEC2-68F0A947225F}" type="{E152C105-A133-4D03-BBF8-3D4B2FBA3E2A}"/>
		<Class name="Name" field="Preset" value="UserInterface_Lossless" type="{3D2B920C-9EFD-40D5-AAE0-DF131C3D4931}"/>
		<Class name="bool" field="EngineReduce" value="false" type="{A0CA880C-AFE4-43CB-926C-59AC48496112}"/>
		<Class name="bool" field="EnableMipmap" value="false" type="{A0CA880C-AFE4-43CB-926C-59AC48496112}"/>
	</Class>
</ObjectStream>
```

The `UserInterface_Lossless` UUID above (`{83003128-...}`) is the built-in
preset's id. Confirm the import in the editor the first time; the dialog is the
authoritative GUI for these settings.

## Why both, and the future

Point filtering is the **sampler** dial; the no-mip preset is the **asset** dial.

The two halves relate to a possible future engine feature differently. The
**asset** dial is engine-side already: a no-mip import preset benefits any content,
so if O3DE adds a first-class pixel-art import path it lines up with this step
rather than fighting it. The **sampler** dial is gem-owned: Diorama sprites render
through their own shader (not Atom's standard material system), so the Point Filter
toggle is self-contained. If the engine later adds nearest-neighbor filtering to
standard materials, that would help material-based content; Diorama's sprite
sampler stays the gem's, so the two are complementary rather than the gem
inheriting the engine feature.

(This came out of an O3DE community thread where the conclusion was that the engine
"should explicitly have a nearest neighbour filtering" option. Diorama adds it for
its own world-space sprites; the engine-side ask is a separate, larger effort.)

## Mixing pixel art and smooth sprites

Fine to do. Point Filter is part of the sprite batch key, so a point-filtered
sprite simply draws in its own batch. You pay one extra draw call per filtering
group, not per sprite.
