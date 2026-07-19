#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"
#include "World/WorldOld.h"

namespace Opaax
{
    /**
     * @class Scene
     *
     * Serializable contributor of entities into the shared WorldOld owned by
     * CoreEngineApp. Represents one game scene (level, menu, cutscene, etc.).
     * Entities created during OnLoad are auto-tagged with this scene's runtime
     * SceneID so they can be wiped together on OnUnload — see SceneIDComponent.
     *
     * Lifecycle:
     *      OnLoad(WorldOld&)   — called once when the scene is pushed onto the stack.
     *      OnUnload(WorldOld&) — called once when the scene is popped.
     *      OnEnter()        — called every time the scene becomes the active scene.
     *      OnExit()         — called every time another scene is pushed on top.
     *
     * Scene does not tick itself — SceneManager drives the lifecycle.
     * Override OnLoad/OnUnload/OnEnter/OnExit in derived classes for game logic.
     */
    class OPAAX_API Scene
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        explicit Scene(const char* InName)
            : m_Name(InName)
        {}

        virtual ~Scene() = default;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        
        Scene(const Scene&)            = delete;
        Scene& operator=(const Scene&) = delete;

        // =============================================================================
        // Move 
        // =============================================================================
        
        Scene(Scene&&)                 = default;
        Scene& operator=(Scene&&)      = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:

        //------------------------------------------------------------------------------
        // Life Cycle

        /**
         * Called once when the scene is first loaded onto the stack.
         * Load assets, create entities into InWorld here.
         */
        virtual void OnLoad(WorldOld& InWorld)   {}

        /**
         * Called once when the scene is popped from the stack.
         * Release assets, cleanup here. InWorld is the shared engine WorldOld
         * the scene's entities live in.
         */
        virtual void OnUnload(WorldOld& InWorld) {}

        /**
         * Called every time this scene becomes the active (top) scene.
         * Resume music, re-enable input, etc.
         */
        virtual void OnEnter()  {}

        /**
         * Called every time another scene is pushed on top of this one.
         * Pause music, disable input, etc.
         */
        virtual void OnExit()   {}

        /**
         * Persist the scene to disk. InWorld is the shared engine WorldOld the
         * serializer reads entities from.
         */
        virtual void SaveScene(WorldOld& InWorld) {}
        
        //------------------------------------------------------------------------------
        // Per-frame
    public:
        virtual void OnUpdate(double DeltaTime)          {}
        virtual void OnFixedUpdate(double FixedDeltaTime){}
        virtual void OnRender(double Alpha)              {}

        //------------------------------------------------------------------------------
        // Get - Set
    public:
        FORCEINLINE const OpaaxString&  GetName()       const noexcept { return m_Name; }

        // Runtime SceneID — stamped by WorldOld::PushScene on push. 0 = unassigned /
        // persistent. Used to filter entities contributed by this scene in the
        // shared WorldOld (see SceneIDComponent).
        FORCEINLINE Uint32 GetSceneID() const noexcept              { return m_SceneID; }
        FORCEINLINE void   SetSceneID(Uint32 InSceneID) noexcept    { m_SceneID = InSceneID; }

        // SourcePath: absolute path of the *.scene.json this scene was last loaded from
        // / saved to. Empty for code-built scenes that never touched disk. SceneManager
        // reads this post-OnLoad to populate its own current-scene-path tracking, so a
        // scene that boots itself from a hardcoded path (e.g. GameScene) can inform the
        // manager without needing a back-pointer.
        FORCEINLINE const OpaaxString&  GetSourcePath() const noexcept { return m_SourcePath; }
        FORCEINLINE void                SetSourcePath(const OpaaxString& InPath) noexcept { m_SourcePath = InPath; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString m_Name;
        OpaaxString m_SourcePath;
        Uint32      m_SceneID = 0;
    };

} // namespace Opaax