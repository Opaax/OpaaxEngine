#pragma once

#include "Engine/Subsystems/Subsystem.h"

namespace Opaax
{
    class World;
    struct WorldContext;

    // =============================================================================
    // Tick order of world subsystems (GetTickOrder): lower ticks first, equal orders tick by class name.
    // =============================================================================
    namespace WorldSubsystemOrder
    {
        /** Turns input into intent. */
        inline constexpr Int32 Input        = 100;
        /** Behaviours and game rules: before physics, so what they apply is simulated this step. */
        inline constexpr Int32 Gameplay     = 200;
        /** The physics step. */
        inline constexpr Int32 Physics      = 300;
        /** Reacts to this step's poses (the mover). */
        inline constexpr Int32 PostPhysics  = 400;
        /** Anything that does not care. */
        inline constexpr Int32 Default      = 500;
        /** Animation, camera, HUD: reads the final state of the frame. */
        inline constexpr Int32 Presentation = 600;
        /** Debug drawing, last. */
        inline constexpr Int32 Debug        = 700;
    }

    // =============================================================================
    // WorldSubsystem — a subsystem that lives as long as its world. Every world makes one of each
    //   registered concrete class its ShouldCreate accepts, in GetTickOrder() order.
    // =============================================================================
    class WorldSubsystem : public Subsystem
    {
        OPAAX_ABSTRACT_OBJECT(WorldSubsystem, Subsystem)

    public:
        /**
         * Whether InWorld gets one. Asked on the class default object at every world creation (keep it
         * cheap); a refused class is never made for that world.
         */
        virtual bool ShouldCreate(const World& /*InWorld*/) const { return true; }

        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Default; }

        /** The world's services. Valid from Startup to Shutdown. */
        WorldContext& GetContext() const;

        /** The world this subsystem belongs to. */
        World& GetWorld() const;

    private:
        friend class WorldSubsystemMgr;

        WorldContext* m_Context = nullptr;   // owned by the World
    };

    // =============================================================================
    // WorldSubsystemMgr — owns and ticks one world's subsystems, and hands each the world's context
    //   before its Startup.
    // =============================================================================
    class WorldSubsystemMgr final : public TSubsystemCollection<WorldSubsystem>
    {
    public:
        /** The context every subsystem gets. Set by the World with its own. */
        void SetContext(WorldContext* InContext) noexcept { m_Context = InContext; }

    protected:
        void OnCreated(WorldSubsystem& InSubsystem) override { InSubsystem.m_Context = m_Context; }

    private:
        WorldContext* m_Context = nullptr;   // owned by the World
    };
}
