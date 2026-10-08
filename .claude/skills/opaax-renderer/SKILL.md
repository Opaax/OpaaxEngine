---
name: opaax-renderer
description: Change the Opaax 2D renderer - sprites, batching, lights, shadows, ambient occlusion, bloom, tonemapping, shaders and the OpenGL backend - and verify the result on screen. Use for any rendering or shader work.
---

# Change the renderer

## Where things are

- `Engine/Source/RHI/`: the device abstraction; `RHI/OpenGL/` is the only backend (OpenGL 4.1 core
  everywhere, macOS included). `GLSLPort` turns the GLSL 4.50 sources into 4.10 at load.
- `Engine/Source/Renderer/`: `Renderer2D` (quad batching, sampler slots), `RendererManager` (what a
  frame draws: worlds, overlays, UI), `Lighting/` (lights packed for the shader, shadow and occlusion
  layouts, gizmos), `Post/ScenePipeline2D` (occlusion, shadow map, ambient occlusion, bloom, tonemap).
- `Engine/Assets/Shaders/`: `Sprite`, `Shadow2D`, `AmbientOcclusion2D`, `Bloom2D`, `Tonemap`.

## Rules

- A world is lit only with an `EnvironmentComponent`; its frame is drawn in linear HDR (RGBA16F), then
  composited. Colours are authored in screen space and decoded with `pow(2.2)`.
- Shaders: GLSL 4.50 with explicit bindings (bindings table in Docs/Customizing.md). Sprite samplers
  are `u_Textures[16]`, indexed by constants only; slots 14 and 15 hold the ambient occlusion and
  shadow maps in lit passes.
- Uniform blocks: std140 with `Vector4F` members; the C++ struct has a `static_assert` on its size.
  A constant repeated in a shader (counts, slots, sizes) is checked by
  `Engine/Tests/Renderer/EngineShaderTests.cpp`: change both sides and that test.
- Keep the CPU side testable: packing, culling, layouts and limits are pure functions with tests
  (`Lighting2DTests`, `BatchPlanTests`, `EnvironmentTests`).
- Vertex size changes: update the `static_assert` in `Renderer2D.cpp` and the batch tooltip it names.
- Prefer Mesa-safe GLSL: no implicit-derivative sampling inside non-uniform loops (use `textureLod`),
  no dynamic sampler indexing, no `binding` left after the port.

## Verify

1. `OpaaxTests` (renderer suites) and the build of every target.
2. Look at it: `TestWorld --exec TestWorld/Tests/Lighting.json` writes `Lighting.png` (run from the
   binary's folder), or build a scene with the editor's automation and take a screenshot. Compare with
   the same scene before the change when the look should not move.
3. CI's Linux jobs render with Mesa (software OpenGL): a shader that compiles on a desktop driver but
   not there fails the "Render check" step with the compile error in the log.
