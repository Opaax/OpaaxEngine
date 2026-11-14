---
name: opaax-renderer
description: Change the Opaax 2D renderer - sprites, batching, lights, shadows, ambient occlusion, bloom, tonemapping, anti-aliasing (texture filtering, multisampling, FXAA, render scale), shaders and the OpenGL backend - and verify the result on screen. Use for any rendering or shader work.
---

# Change the renderer

## Where things are

- `Engine/Source/RHI/`: the device abstraction; `RHI/OpenGL/` is the only backend (OpenGL 4.1 core
  everywhere, macOS included). `GLSLPort` turns the GLSL 4.50 sources into 4.10 at load.
  `TextureSampling.h` (filters, sampler descriptions, `BleedAlphaEdges`), `RHICapabilities.h` (the
  driver's limits). The context reaches its window through `IGLSurface` (`Platform/Glfw/GlfwGLSurface`):
  no GLFW under `RHI/`.
- `Engine/Source/Renderer/`: `Renderer2D` (quad batching, sampler slots), `RendererManager` (what a
  frame draws: worlds, overlays, UI; applies `Renderer.config` live), `Lighting/` (lights packed for
  the shader, shadow and occlusion layouts, gizmos), `Post/ScenePipeline2D` (occlusion, shadow map,
  ambient occlusion, bloom, tonemap, FXAA), `Post/SceneTargetPlan` (`PlanSceneTarget`: whether a world
  view goes through a scene target, its size, format, samples), `Post/RenderTargetCache` (targets kept
  per size, none recreated in a steady frame).
- `Engine/Assets/Shaders/`: `Sprite`, `Shadow2D`, `AmbientOcclusion2D`, `Bloom2D`, `Tonemap` (also the
  copy of an unlit scene target), `FXAA`.
- TestWorld: `Levels/Lighting`, `Levels/Filtering`, `Levels/AntiAliasing`, `Levels/AntiAliasingLit`
  with their scripts in `TestWorld/Tests/`.

## Rules

- A world is lit only with an `EnvironmentComponent`; its frame is drawn in linear HDR (RGBA16F), then
  composited. Colours are authored in screen space and decoded with `pow(2.2)`.
- With every setting off, an unlit world is drawn straight into its view's target; any of
  `Multisampling`, `bFXAA`, `RenderScale` (or lighting) sends it through a scene target. The defaults
  keep today's pictures: a change that moves a default picture says so.
- Overlays over the world and the UI are drawn after the scene target, at the view's own pixels and
  one sample: never put them under FXAA or the render scale.
- Shaders: GLSL 4.50 with explicit bindings (bindings table in Docs/Customizing.md). Sprite samplers
  are `u_Textures[16]`, indexed by constants only; slots 14 and 15 hold the ambient occlusion and
  shadow maps in lit passes.
- Uniform blocks: std140 with `Vector4F` members; the C++ struct has a `static_assert` on its size.
  A block is bound by the pass's bind group, never when the buffer is created: list it in the group
  of every pass that reads it. A constant repeated in a shader (counts, slots, sizes) is checked by
  `Engine/Tests/Renderer/EngineShaderTests.cpp`: change both sides and that test.
- Framebuffers have no depth-stencil unless they ask for it (`FramebufferSpec::DepthStencil`): no
  pass tests depth.
- Keep the CPU side testable: packing, culling, layouts, plans and limits are pure functions with
  tests (`Lighting2DTests`, `BatchPlanTests`, `EnvironmentTests`, `SceneTargetPlanTests`,
  `RenderTargetCacheTests`, `TextureSamplingTests`).
- Vertex size changes: update the `static_assert` in `Renderer2D.cpp` and the batch tooltip it names.
- Prefer Mesa-safe GLSL: no implicit-derivative sampling inside non-uniform loops (use `textureLod`
  or `textureGrad`), no dynamic sampler indexing, no `binding` left after the port.
- A wrong GL call is an Error line in the `OpenGL` category in a dev build: CI fails a render check on it.

## Verify

1. `OpaaxTests` (renderer suites) and the build of every target. On this project the user builds and
   runs the tests: hand them the commands.
2. Look at it: `TestWorld --exec TestWorld/Tests/Lighting.json` writes `Lighting.png` (run from the
   binary's folder), or build a scene with the editor's automation and take a screenshot. Compare with
   the same scene before the change when the look should not move.
3. Check it without eyes: `renderer.info` answers what is used (samples given, scene targets, filter,
   `targets.recreated`, `gpuFrameMs`); `frame.sample` counts the colours across an edge (2 hard, more
   smoothed). In lit levels expect bounds (`$atLeast`, `$atMost`): Mesa differs by a grey level.
4. CI's Linux jobs render with Mesa (software OpenGL): a shader that compiles on a desktop driver but
   not there fails the "Render check" step with the compile error in the log.
