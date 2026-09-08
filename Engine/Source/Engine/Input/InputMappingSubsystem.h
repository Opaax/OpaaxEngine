#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"

namespace Opaax
{
    struct GameInstanceContext;

    inline constexpr LogCategory LogInputMapping{"InputMapping"};

    // =============================================================================
    // InputMappingSubsystem — the layer that turns KEYS into MEANING, and the first tenant
    //   of the GameInstance tier.
    //
    //   The other half of the input chain. Engine/Subsystems/Input/InputManager is the raw
    //   end — what is held, what changed this frame, physical codes only, and its header says
    //   outright that "there is no 'Jump' in here". This is where there is one.
    //
    //   SESSION-SCOPED, not world-scoped, and that is the requirement that chose the tier: a
    //   pushed mapping context must survive level travel, and a UI context must outlive any
    //   single world. It also means the stack cannot leak across a PIE cycle — Stop destroys
    //   the whole game instance rather than resetting anything.
    //
    //   TICKS BEFORE EVERY WORLD (GameInstanceManager is registered before WorldManager), so
    //   this frame's action values are published before any gameplay subsystem reads them.
    //
    //   B0: the tier's tenant, with no contexts and no evaluation yet — those are B1/B2.
    // =============================================================================
    class OPAAX_API InputMappingSubsystem final : public GameInstanceSubsystemBase
    {
        // =========================================================================
        // Base Implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(InputMappingSubsystem)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        /** @param InContext BORROWED, and stable for the whole game (GameInstance owns it). */
        explicit InputMappingSubsystem(GameInstanceContext& InContext) noexcept;
        ~InputMappingSubsystem() override = default;

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        /** How many mapping contexts are currently pushed. Zero until B2 loads any. */
        Uint64 GetContextCount() const noexcept { return m_ContextCount; }

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IGameInstanceSubsystem interface
    public:
        bool Startup() override;
        void Shutdown() override;
        //~End IGameInstanceSubsystem interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        GameInstanceContext* m_Context = nullptr;

        Uint64 m_ContextCount = 0;
    };
}
