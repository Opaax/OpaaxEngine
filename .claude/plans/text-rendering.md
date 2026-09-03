# ⑥ S4 — TEXT RENDERING

## Context

Sequence block ⑥ continues after S3 Animation. The engine can draw **no text anywhere** outside the
editor's own ImGui chrome — `DebugDraw` says so in its own header (*"Lines and BOXES. Everything else
(circles, text, persistent durations) waits for a caller"*), and the `Game.exe` stats overlay is
blocked on it.

The legacy M5 implementation (`Engine/Source/Legacy/Renderer/Text/`, plan
`.claude/Old/milestone/M5_Text_Rendering.md`) is worth salvaging for its *maths* — the layout walker,
the V-flip at bake, the `xadvance`-is-already-pixels trap — but not its shape: it is `IAsset`-based
(retired vocabulary), ASCII-only (`char`, a 95-slot array), and one file = one font with no way to
pick a weight.

The user has dropped **162 Roboto TTFs** into `Engine/Assets/Fonts/Roboto/` — 9 subsets × 9 weights ×
2 styles — and asked for Google-Fonts-shaped selection: pick the subset, the weight, italic.

**Four decisions locked with the user (2026-09-03):**

| | Decision |
|---|---|
| Atlas | **Static per-face bake** — bake every glyph the file actually contains. CJK later swaps the atlas internals inside `FontFaceResource`; the glyph API above it does not move. |
| Selection | **A generated `Roboto.opaaxfont`** shipped as engine content. `AnimationLibraryData`'s alias-table shape. **No new editor verb** — the asset-creation Factory (**AN8**) stays unpre-empted. |
| Missing glyph | **Tofu box (□)**, no cross-subset fallback. Loud, and free: `DrawQuadOutline` exists. |
| Editor UI font | **Through the GUI seam**, so a Qt swap stays easy (**MR2d**). |

**Three findings that shape the work:**

- **There is no width axis in these files.** fontsource's static Roboto has none — width is a
  *separate family* (`Roboto_Condensed`) or the variable font. `EFontWidth` is in the key so a
  condensed family drops in later, but only `Normal` resolves today. Stated, not hidden.
- **Greek and Cyrillic force UTF-8.** The subset dimension is meaningless without codepoint decoding,
  so the glyph table is codepoint-keyed, not a 95-slot ASCII array.
- **The R8 GPU path already survived the refresh.** `OpenGLTexture2D::Upload`
  (`Engine/Source/RHI/OpenGL/OpenGLTexture2D.cpp:51`) has the `GL_R8` / `GL_RED` / swizzle /
  `UNPACK_ALIGNMENT` branch, and `RenderSystem::CreateTexture` documents `1 = R8 coverage`. Legacy
  M5's Step 1 is already paid for.

**Not in scope, and why:** screen-space text (a stats overlay pinned to a corner) needs a second
ortho pass — that is **multi-view**, which `CLAUDE.local.md` already sequences right after this.
World-space text is the primitive multi-view then consumes; it is blocked, not deferred by choice.
Alignment, word-wrap, rotation, outline/shadow and SDF have no caller.

---

## Layering

`Renderer/` is portable and may not reach the Resources layer. `TextureResource` includes
`RHI/Texture.h`, so **Resources → Renderer is the allowed direction**; the split falls out of that:

```
Renderer/Text/            portable, no host reach, unit-testable with no GL
  FontStyle.h             EFontSubset / EFontWeight / EFontWidth + FontStyleKey (+ OPAAX_ENUM_VALUES)
  FontFaceData.h          Glyph / FontVMetrics / KerningPair / FontFaceData + GetGlyph, GetKerning
  FontBake.{h,cpp}        TTF bytes -> FontFaceData + R8 pixels. Owns STB_TRUETYPE_IMPLEMENTATION.
  TextDrawParams.h        Size / Color / LineHeightScale / bKerning
  Text2D.{h,cpp}          DrawString + Measure over Renderer2D

Engine/Subsystems/Resources/Types/     the CResource adapters
  FontFaceResource.{h,cpp}             .ttf/.otf — TextureResource's two-phase shape exactly
  FontFamilyData.h / FontFamilyFile.{h,cpp} / FontFamilyResource.h    .opaaxfont — the SpriteSheet trio
```

`Text2D` consumes a **`FontFaceView { const FontFaceData* Data; ITexture2D* Atlas; }`** — the
`RenderView` idiom. `RendererManager` builds it (the one adapter allowed to reach the
ResourceManager), so `Renderer/` never names a resource type.

---

## S1 — Face resource, bake, UTF-8

**`Core/String/OpaaxUtf8.h`** — add a decoder section beside the existing path-boundary one:
`Utf8::Decode(const char*& InOutCursor)` → codepoint, advancing the cursor; malformed input degrades
to U+FFFD and always advances (the file's stated no-throw contract). Same file, new `// ====`
section — not a new header.

**`Renderer/Text/FontBake.{h,cpp}`** — free, pure, no GL, no file IO (takes bytes):
- **Coverage scan** `stbtt_FindGlyphIndex` over `0x0020 .. 0x33FF` — the window stops immediately
  before CJK Extension A (0x3400), which is exactly the line the user drew. ~13k probes, a few ms.
  Whatever the file has gets baked, so **any** `.ttf` works, not only fontsource subsets.
- Pack with `stbtt_PackFontRanges` + `array_of_unicode_codepoints`, 32 px, oversample (2,2), atlas
  512 → 1024 → 2048 by doubling, **fail loud past the cap**.
- **V-flip the UVs at bake** (stb_truetype writes top-down, Renderer2D is Y-up) — M5's `F-Text-1`.
- Kerning: pairs over the covered set via `stbtt_GetCodepointKernAdvance`, zero-advance dropped,
  sorted by `(First, Second)` for binary search. **Guarded**: above 512 glyphs the N² scan is skipped
  with one warning — `symbols`/`math` are the big subsets and do not kern.

**`Types/FontFaceResource.{h,cpp}`** — `OPAAX_RESOURCE_FORMAT("Font Face", ".ttf", ".otf")`,
`FailPolicy::Placeholder`. `Load` (any thread) reads via `Utf8::ToFsPath` + bakes; `Initialize`
(main thread) uploads R8 through `IEngine` → `RendererManager::CreateTexture` and frees the pixels;
`Placeholder()` is an **empty face** — no glyphs, so every character draws tofu, which is visible
rather than absent. Registered in `Engine.cpp:93`'s block.

**`Engine/CMakeLists.txt:429`** — add `Fonts` to the copy list and **fix the comment above it**,
which currently claims fonts belonged to the retired Legacy manifest. Without this, `/Engine/Fonts/…`
resolves in a dev build and to nothing shipped — the exact silent failure that list exists to prevent.

**Gate:** new `Engine/Tests/Core/Engine/Subsystems/Resources/FontTests.cpp` (**added to
`Engine/Tests/CMakeLists.txt` explicitly, and the case count checked**) — UTF-8 decode incl.
malformed input, bake of a real greek and a real latin subset, glyph present/absent, kern lookup.
Plus one smoke log line that **counts**: `FontFace 'roboto-greek-400-normal' baked 131 glyph(s),
atlas 512x512, 0 kern pair(s)`.

---

## S2 — Text2D, the family, the TextComponent  *(the visible one)*

**`Renderer/Text/Text2D.{h,cpp}`** — `DrawString(Renderer2D&, const char* InUtf8, const Vector2F&,
const FontFaceView&, const TextDrawParams&)` + `Measure(...)`. UTF-8 walk, `\n` line break, `\t` as 4
spaces, kerning, layer/order passthrough. A codepoint the face lacks — or a face with **no atlas at
all** — draws a **hollow box** via `Renderer2D::DrawQuadOutline` at half-size advance. Layout maths
salvaged from `Legacy/Renderer/Text/Text2D.cpp` (baseline = `pos.y - Ascent*scale`; `xadvance` is
already pixels, **not** FreeType's 1/64ths).

**`Renderer/Text/FontStyle.h`** — the four axes as reflected enums (`OPAAX_ENUM_VALUES` + `ToString`),
so the Inspector draws dropdowns with **zero editor code** — one constrained `TPropertyDrawer`
already serves every enum.

**`Types/FontFamily*`** — `.opaaxfont`, `FailPolicy::Placeholder`. `FontFamilyData { Name; Entries;
Default }` where an entry is `{ Subset, Weight, Width, bItalic, TResourcePath<FontFaceResource> }` —
`AnimationLibraryData`'s shape. Matching ladder, CSS's, with **subset never crossed** (asking Greek
and getting Latin draws a screenful of tofu, which is worse than the honest miss):
exact → nearest weight → flip italic → nearest width → nullptr. **Logged once per inexact request**,
so a fallback is never silent.

**`Engine/Assets/Fonts/Roboto.opaaxfont`** — 162 entries, generated once with a throwaway Python
script (`json.dumps(sort_keys=True, indent=4)`, no trailing newline, matching the writer). Engine
content, reachable as `/Engine/Fonts/Roboto.opaaxfont`.

**`World/Components/TextComponent.h`** — registered in `Engine.cpp:86`'s block:
`Text` · `Font` (family) + `Subset`/`Weight`/`Width`/`bItalic` · `Face` (a bare `.ttf`) · `Size` (px)
· `Color` · `LineHeightScale` · `bKerning` · `bVisible` · `Layer` · `OrderInLayer`.
**`Font` WINS over `Face` when set** — `SpriteComponent`'s Sheet-over-Texture precedence and its
reason: a one-off label must not need a family asset beside it. `NLOHMANN_..._WITH_DEFAULT` +
`OPAAX_PROPERTIES`; every field type already has a drawer, so the Inspector costs nothing.

**`Engine/Subsystems/Renderer/RendererManager`** — `ResolveFace` / `ResolveFamily` claim-and-keep
caches beside `m_TextureCache`/`m_SheetCache`, `ResolveTextDraw` (the one place the precedence lives),
and `DrawWorldTexts(World&, Renderer2D&)` beside `DrawWorldSprites`.

**Gate — their eyes.** Three entities: `Hello`, `Γειά σου κόσμε`, `Привет мир`. Flip
Weight/Italic/Subset in the Inspector and the glyphs change live. A Latin face asked for Greek draws
tofu boxes. Save the map, reopen, it survives.

---

## S3 — Editor chrome, atlas preview, and the editor's own UI font

**`EditorService.cpp:429`'s block** — `ResourceTypes().Register<FontFaceResource>()` (glyph `[F]`,
opens the preview) and `<FontFamilyResource>()` (glyph `[FF]`).

**`ResourcePreviewPanel`** — its header says it draws the content of *"exactly one, TextureResource.
That single branch is the honest shape while there is one previewable type; the growth point is named
in the .cpp."* This is that growth point: a font face shows its atlas image plus glyph count, kern
pairs, atlas size and vmetrics.

**The editor's UI typeface, through the seam.** ImGui's default is ProggyClean — ASCII only — so
Greek typed into the Inspector renders correctly in the *viewport* and as boxes in the *field*.
`Editor/UI/IEditorGui.h` gains a backend-agnostic value type and one virtual:

```cpp
struct EditorUIFont { OpaaxString Path; float SizePx = 16.f; };   // empty Path = the backend's default
virtual void SetUIFont(const EditorUIFont& InFont) = 0;
```

It states the *requirement* ("this typeface, this size"), never ImGui's spelling — Qt implements it
with `QApplication::setFont`. The ImGui side merges the latin / greek / cyrillic / vietnamese faces
into one `ImFont` via `ImFontConfig::MergeMode`; **ImGui 1.92.8 loads glyphs on demand**, so no glyph
ranges are needed. Driven by two new `EditorImguiConfigData` fields (the `_WITH_DEFAULT` macro makes
that additive), applied through the existing `CheckStyle()` route.

**Gate — their eyes.** Fonts appear in the browser with labels; double-click shows the atlas; the
Inspector's text field renders Greek and Cyrillic.

---

## Verification

Per-step, and **each step commits before the next starts** (**L17** — S2's gate is a diff shape).

```bash
OPAAX_NO_PAUSE=1 ./build.bat test </dev/null
```
Then run `build/debug-editor/bin/.../OpaaxTests.exe` directly — CTest reports "1 test" and hides the
case count. Grep the build output for `OPAAX_BUILD_OK`, **never the exit code** (**L8**).

```bash
OPAAX_NO_PAUSE=1 ./build.bat debug-editor </dev/null && OPAAX_NO_PAUSE=1 ./build.bat release </dev/null
```

Smoke: `ls -la` the exe against the DLL first (**L24**), **back up `Sandbox/Editor/Save/imgui.ini`
before every editor run and restore after**, then read the whole of
`Sandbox/Save/Log/OpaaxEngine.log` — not only the lines I went looking for (**L27**).

The eye gates in S2 and S3 are the user's; a smoke run never drags a resource onto a field, never
opens a dropdown and never types Greek into a text box (**L64**), so I will say each time which paths
it did not reach.

Working checklist goes in `.claude/task/todo.md`; durable outcomes land as `TX*` invariants in
`ARCHITECTURE.md` and lessons in `.claude/lessons.md` at close.

---

# RECORD — closed 2026-09-03, code-complete, EYES NOT YET GIVEN

5 commits `031227c` → `3cf251e`. **601 / 7744 / 7** (from 569 / 7621 / 7), zero warnings in
`test` / `debug-editor` / `release`. `resourceTypes=9`, `components=8`, `formats=9 over 14 ext`.
Durable → **TX1–TX10**. Lessons → **L77**, **L78**.

## What shipped, against the plan

All three steps landed as planned. Four things came out different:

1. **The preview route became a chrome facet, not a second `if`.** `ResourcePreviewPanel.cpp` carried
   a growth-point comment saying the second previewable type should turn its type-id branch into a
   `SetPreview` facet — so it did (**TX9**). The panel now holds an `IResourcePreviewClaim` and names
   no resource type. `TextureResource`'s own preview moved out with it, into
   `Editor/Resources/ResourcePreviewDrawers.cpp`, because `EditorService` has zero `ImGui::` and
   keeps it (**GIZ8**). Cost ~90 lines and deleted more knowledge than it added.
2. **`FontFaceData::Tofu()` was not in the plan** and had to exist: three call sites needed "an empty
   face that can still lay out", and a value-initialised one divides by its own zero `PixelHeight`.
3. **A text had no BOUNDS** — found by reading a number I had not come for (`Drawing 4 entity
   icon(s)`). It rendered but was unclickable, outlined as a 16px anchor, and drew a "nothing to
   render" icon over itself. `EntityQuery` now estimates its extent (**TX7**); icon count 4 → 1.
4. **A magic-check guard in `FontBake`**, added because a unit test SEGFAULTED: `stbtt_GetFontOffsetForIndex`
   answers -1 for a non-font and handing that to `stbtt_InitFont` indexes before the buffer. A `.ttf`
   that is really a PNG now reaches the placeholder policy. The test that found it stays.

## What the numbers say

- `roboto-greek-400-normal` → 77 glyphs, 512², **576 kern pairs**
- `roboto-cyrillic-500-italic` → 104 glyphs, 512², 1245 kern pairs
- `roboto-latin-700-normal` → 224 glyphs, **overflowed 512 and grew to 1024**, 2495 kern pairs

The grow-by-doubling path runs at every boot, and stb 1.26 reads GPOS — neither was theoretical.

## What is NOT verified, and cannot be by me

A smoke run never clicks, drags, double-clicks or types (**L64**). Owed to their eyes, listed in
`.claude/task/todo.md`: glyph orientation (M5's old F-Text-1 V-swap, reasoned but never seen), the
Inspector's Style fold changing glyphs live, tofu on a missing subset, click-to-select and the
outline, the atlas preview on double-click, and Greek typed into a text field.

## What went wrong (the part worth re-reading)

- **I smoke-tested the wrong binary for two whole steps.** `Sandbox.exe` is the game; the editor is
  `SandboxEditor.exe`. Every "the editor boots clean" claim rested on a log with zero `[Editor*]`
  lines in it. Caught only because a success line I had just added failed to appear (**L77**).
- **Two wrong claims about one log line went into commit messages**, both inferred rather than
  measured, and the first amend was wrong too (**L78**).
