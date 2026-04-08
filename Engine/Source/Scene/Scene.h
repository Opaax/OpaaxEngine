#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Core/World/World.h"

namespace Opaax
{
    /**
     * @class Scene
     *
     * Owns a World. Represents one game scene (level, menu, cutscene, etc.)
     *
     * Lifecycle:
     *      OnLoad()   — called once when the scene is pushed onto the stack.
     *      OnUnload() — called once when the scene is popped.
     *      OnEnter()  — called every time the scene becomes the active scene.
     *      OnExit()   — called every time another scene is pushed on top.
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
         * Load assets, create entities here.
         */
        virtual void OnLoad()   {}
        
        /**
         * Called once when the scene is popped from the stack.
         * Release assets, cleanup here.
         */
        virtual void OnUnload() {}
        
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
         * Called every time another scene is pushed on top of this one.
         * Pause music, disable input, etc.
         */
        virtual void SaveScene()   {}
        
        //------------------------------------------------------------------------------
        // Per-frame
    public:
        virtual void OnUpdate(double DeltaTime)          {}
        virtual void OnFixedUpdate(double FixedDeltaTime){}
        virtual void OnRender(double Alpha)              {}

        //------------------------------------------------------------------------------
        // Get - Set
    public:
        FORCEINLINE const OpaaxString&  GetName()   const noexcept { return m_Name; }
        FORCEINLINE World&              GetWorld()  noexcept       { return m_World; }
        FORCEINLINE const World&        GetWorld()  const noexcept { return m_World; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString m_Name;
        World       m_World;
    };

} // namespace Opaax