#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/OpaaxTypes.h"
#include "World/IWorldSystem.h"

namespace Opaax
{
    /**
     * @class WorldSubsystem
     *
     * Engine subsystem that owns the ordered list of world-render systems (IWorldSystem) — the systems
     * WorldRenderPass dispatches between Renderer2D::Begin/End to draw the world.
     *
     * Replaces the former static TPolymorphicList<IWorldSystem>: a function-local static inside a
     * non-exported template specialization gets ONE copy per module, so a system registered from a game
     * exe was invisible to the engine-DLL render pass (the M11 blank-window bug). As a single
     * engine-owned instance reached via GetSubsystem<WorldSubsystem>(), there is exactly one list.
     *
     * Registered AFTER RenderSubsystem so reverse-order Shutdown clears the systems — and any GPU
     * resources they own — while the render device is still alive (else Vulkan/VMA asserts at exit).
     * Self-registers the engine's WorldRenderSystem at Startup; games append their own systems from
     * CoreEngineApp::OnStartup (subsystems are constructed in StartupAll, which runs AFTER OnInitialize,
     * so OnInitialize is too early). Registration order == draw order.
     */
    class OPAAX_API WorldSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(WorldSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        WorldSubsystem() = default;
        explicit WorldSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~WorldSubsystem() override = default;

        WorldSubsystem(const WorldSubsystem&)            = delete;
        WorldSubsystem& operator=(const WorldSubsystem&) = delete;
        WorldSubsystem(WorldSubsystem&&)                 = default;
        WorldSubsystem& operator=(WorldSubsystem&&)      = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Append a world-render system. Registration order is draw order: the engine self-registers
        // WorldRenderSystem first, games append after (from CoreEngineApp::OnStartup).
        void Register(UniquePtr<IWorldSystem> InSystem);

        //------------------------------------------------------------------------------
        //  Get - Set
    public:
        const TDynArray<UniquePtr<IWorldSystem>>& GetSystems() const noexcept { return m_RenderSystems; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;
        void Shutdown() override;
        //~End EngineSubsystemBase interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<UniquePtr<IWorldSystem>> m_RenderSystems;
    };
}
