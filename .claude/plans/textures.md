# ④ Textures & sprites — plan + review (2026-08-20)

Commits: `79a68bd` (S1) · `f17cc55` (S2) · `a896f26` (S3). Branch `refresh_engine`, nothing pushed.
Durable outcome → **I16** and the new **I15** bullet in `ARCHITECTURE.md`; post-mortem → [[L45]].

## Why

`Renderer2D` drew solid quads only, and three places in the tree said why in nearly the same words —
`Renderer2D.h:13`, `RHI/Texture.h:30`, `Sandbox/Assets/Textures/Readme.txt` — all of them waiting on
"the Texture CResource". That resource system had since been rebuilt twice (`CResource` +
`ResourceManager`, then `ResourceFormatRegistry`), and the format table's *entire* justification had
been *"what about texture later? png, jpg"* while shipping with **no production caller** claiming more
than one extension. This slice paid that debt and reconnected the path.

**Scope decided with the user up front:** texture → visible sprite, end to end. A new `SpriteComponent`
beside `DummyComponent` (existing maps untouched). Authoring by **drag & drop** from the Resource
Browser. Camera explicitly out — `RendererManager` still hard-codes its centred ortho.

## What shipped

**S1 — a texture is a resource.** `TextureResource` (`Engine/Subsystems/Resources/Types/`): `Load` =
`FileIO::ReadAllBytes` + `stbi_load_from_memory` on any thread, `Initialize` = upload through `IEngine`
on the pump then free the pixels, magenta `Placeholder`, `OPAAX_RESOURCE_FORMAT("Texture", .png .jpg
.jpeg .tga .bmp)`. `IRHIDevice::CreateTexture(pixels, w, h, channels)` added; `OpenGLTexture2D`'s path
ctor and its private `stb_image` copy **deleted**. `ResourcePool::PlaceholderOrNull` now initialises the
substitute it builds.

**S2 — it draws.** `Renderer2D::DrawSprite` + the slot walk salvaged from `cc502a5^`, both entry points
folded into one `SubmitQuad` (a coloured quad is a sprite on slot 0). `TResourcePath<T>` + json (bare
string). `SpriteComponent`. `ERenderLayer` gained `ToString` + `TEnumValues` from its own X-macro list.
`Int16` property drawer. `RendererManager` caches `ResourceRef`s by interned path and draws the sprite
pass.

**S3 — it authors.** One drag payload for every resource type (`[TypeId][path]`, variable length), a
generic drag source in the browser, and `TPropertyDrawer<TResourcePath<T>>` as the typed drop target.
Chrome for textures; the sprite drawer registered in the Sandbox editor module.

## Review

**What the design got right.** The three registries built in the previous slices did their job: adding
a texture touched **no** extension table, **no** editor per-type code, and **no** component base class.
`TResourcePath<T>` is the piece worth keeping in mind — the type parameter is what makes the drop target
able to refuse, and it means the next resource field (a sound, a font) is one line.

**The bug the user found, and what it cost.** `SpriteComponent` mirrored `DummyComponent`'s field names
by design, and ImGui identifies widgets by label → conflicting IDs the moment one entity carried both.
Fixed at the registry ENTRY (one `PushID` per drawn type), which also closed the same latent bug in the
Config panel. Three presets, 383 tests and two smoke runs were all green through it: the defect needs a
human hovering a row. → [[L45]].

**The ordering bug I caught while writing it.** Claiming a texture slot *before* the full-buffer flush
check would leave the index naming a slot the flush had just reset — the wrong image, silently.
`EnsureBatchRoom()` now runs first, and `Renderer2D.cpp` says why at the call site.

**One deviation from the approved plan, deliberate.** `TextureResource` lives under
`Engine/Subsystems/Resources/Types/`, not `Renderer/Texture/` as planned: `RendererManager.h`'s own
contract says the `Renderer/` module never reaches host services, and `Initialize()` must resolve
`IEngine`. Caught before writing the file, not after.

**One existing assertion changed.** `ResourceSystemTests` read `InitCount == 0` off the *placeholder*
while meaning the *pending payload*. Replaced with an identity check (`lLoaded != lWhileLoading`) plus a
new case pinning "the placeholder is initialised, once per pool".

**Content note.** The dogfood map I authored (`Maps/Sprites.opaaxmap`) was superseded by the user
building the real thing in their own `Main.opaaxmap` — Sprite component, dragged texture, `UI` layer,
order 155, tint, saved. They removed it from the level manifest and it was **deleted at session close**:
scaffolding the real dogfood replaced, judged by [[L23]]'s test — a first example earns its place by
SURVIVING, and this one did not. `Sandbox.exe` still draws the sprite, from their map.

## Not done, deliberately

Camera / `RenderView` producer (**the carry-forward fork: editor camera vs World camera**) · sprite
sheets and UV authoring (`DrawSprite` takes UVs; nothing authors them) · sprite rotation (waits for a
transform component, which is also where `Position` belongs) · hot reload · `DummyComponent`'s
retirement · **a texture thumbnail in the Inspector** — `IEditorUIBackend` names that gap itself
(*"GetTextureID (asset thumbnails) is M2"*) and `Legacy/Editor/Assets/Types/Texture2DTypeActions.cpp`
has the code including the V-flip convention, but a preview needs a context the `TPropertyDrawer`
contract deliberately withholds. That is its own decision.

**No unit tests for `DrawSprite` or the texture cache**: both need a GL context, and the `Renderer/`
suites only cover headless pieces. The batch has never had coverage; closing that wants a device stub
and is its own step.
