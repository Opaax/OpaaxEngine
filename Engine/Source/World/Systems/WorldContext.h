#pragma once

namespace Opaax
{
    class World;
    class ResourceManager;
    class EngineEventBus;
    class DebugDraw;

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
    // =============================================================================
    struct WorldContext
    {
        /** The world this subsystem belongs to. Its entities are the subsystem's whole job. */
        World& OwningWorld;

        /** Load-by-path resources (textures, shaders). Engine-owned, shared by every world. */
        ResourceManager& Resources;

        /** The engine bus. A subsystem reacts to engine/world events without touching the locator. */
        EngineEventBus& Events;

        /**
         * Per-frame debug lines. IMMEDIATE MODE by contract (F4): nothing is retained, so a
         * subsystem re-submits every frame it wants something visible. This is also the ONLY
         * way a world subsystem draws — there is deliberately no Render hook (see WorldManager).
         */
        DebugDraw& Debug;
    };
}
