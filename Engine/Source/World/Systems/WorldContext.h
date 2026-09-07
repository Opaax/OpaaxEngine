#pragma once

namespace Opaax
{
    class World;
    class ResourceManager;
    class EngineEventBus;
    class DebugDraw;
    class FrameProfiler;
    class IPaths;
    class InputManager;
    struct EngineConfigData;

    // =============================================================================
    // WorldContext — the flat struct of references a world subsystem is constructed with.
    //   Modelled on Editor/EditorContext.h (Editor.md D3): resolved ONCE, injected BY
    //   CONSTRUCTOR, and nothing downstream ever sees the AppServiceLocator.
    //
    // WHY THIS EXISTS — Editor.md §3's stated injection rule cannot work.
    //   §3 says a world subsystem "receives its world and nothing else", and that a subsystem
    //   needing an app service has the REGISTRATION SITE capture it into the factory. But the
    //   registration site is `WorldSubsystems().Register<T>()`, which takes no arguments and is
    //   frozen by MR1 — there is nowhere to capture anything. Without a context, a subsystem
    //   that needs ResourceManager would have to reach the locator, which D3 forbids. So the
    //   dependency arrives the way the editor already solved it: by constructor.
    //
    // PER-WORLD, which is the whole reason it is not one WorldManager member: `OwningWorld`
    //   differs per instance, and PIE means two worlds are live at once (S4/S5). The owning
    //   World holds it, so a subsystem may store `WorldContext&` and it stays valid exactly as
    //   long as the world does.
    //
    // NOT INCLUDED: EngineRegistries. A registry is TYPE METADATA (MR0), not something a
    //   running subsystem should poke, and nothing needs it — adding a member later is one line
    //   and breaks no existing subsystem, so this starts at what has callers.
    //   *Paths arrived exactly that way (⑥ S3): SpriteAnimationSubsystem is the first world
    //   subsystem to load an ASSET, and ResourceManager::Load takes an absolute path.
    //   Config followed (⑦-A P1) for PhysicsSubsystem, and Input (⑦-A P5b) for the mover's
    //   control system. Each time the NULL-GUARD in
    //   WorldManager::CreateSubsystemsFor had to grow with the member — a sibling resolved but
    //   never checked is the failure this struct's own history keeps producing.*
    // =============================================================================
    struct WorldContext
    {
        /** The world this subsystem belongs to. Its entities are the subsystem's whole job. */
        World& OwningWorld;

        /** Load-by-path resources (textures, shaders). Engine-owned, shared by every world. */
        ResourceManager& Resources;

        /**
         * Asset-relative -> absolute, which is what ResourceManager::Load needs and what a
         * TResourcePath deliberately is not (MP8). Const: a subsystem asks where things are, it
         * never reconfigures the project's layout.
         */
        const IPaths& Paths;

        /** The engine bus. A subsystem reacts to engine/world events without touching the locator. */
        EngineEventBus& Events;

        /**
         * This frame's input. A gameplay subsystem READS intent from it; nothing here clears it —
         * the frame boundary is the host loop's (**IN2**) and stays there.
         *
         * The third member to arrive by the growth clause below (⑦-A P5b), for the mover: a
         * control system turns keys into a MoverComponent's intent, and a world subsystem has no
         * other route to input without reaching the locator, which D3 forbids.
         */
        const InputManager& Input;

        /**
         * The engine's boot configuration, read-only — a subsystem is configured BY it, it never
         * reconfigures the project.
         *
         * Arrived by the growth clause below, the way Paths did (⑦-A P1): PhysicsSubsystem builds
         * its world from Config.Physics, and a world subsystem has no other route to a config —
         * IConfigSystem is an app service and reaching the locator is what D3 forbids. The WHOLE
         * block rather than a physics-shaped slice, because it is one config object and the next
         * reader (Render.Interpolation) wants a different part of it.
         */
        const EngineConfigData& Config;

        /**
         * Per-frame debug lines. IMMEDIATE MODE by contract (F4): nothing is retained, so a
         * subsystem re-submits every frame it wants something visible. This is also the ONLY
         * way a world subsystem draws — there is deliberately no Render hook (see WorldManager).
         */
        DebugDraw& Debug;

        /**
         * Where OPAAX_STAT_SCOPE records (ST1). Here for the reason this struct exists at all: the
         * registration site takes no arguments, so a subsystem has nowhere else to be handed one.
         *
         * A POINTER, and NULL is normal — it is what a build with stats disabled hands out, and
         * OPAAX_STAT_SCOPE no-ops on it. The only reference here that may be absent, because it is
         * the only one whose absence is a supported configuration rather than a boot failure.
         *
         * OPT-IN — a subsystem that names no scope simply never appears in the frame tree. Nothing
         * measures a tick on the author's behalf.
         */
        FrameProfiler* Profiler;
    };
}
