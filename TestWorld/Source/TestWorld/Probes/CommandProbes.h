#pragma once

#include <nlohmann/json.hpp>

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandEvents.h"
#include "Commands/CommandHandle.h"
#include "Core/Events/EventBus.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"
#include "World/World.h"
#include "World/WorldSpec.h"

namespace TestWorld
{
    // =============================================================================
    // CommandProbeSubsystem — a world subsystem of the Commands level's Play worlds: registers
    //   probe.ping for the world's lifetime. probe.ping answers {"world": its name, "pings": how many
    //   times this world was pinged}; in another level it is an unknown command. Counts the
    //   CheatUsed events of its world's lifetime: probe.cheats answers {"count", "last"}.
    // =============================================================================
    class CommandProbeSubsystem final : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(CommandProbeSubsystem, Opaax::WorldSubsystem)

        bool ShouldCreate(const Opaax::World& InWorld) const override
        {
            return InWorld.GetMode() == Opaax::EWorldMode::Play && InWorld.GetName() == Opaax::OpaaxStringID("Commands");
        }

        bool Startup() override
        {
            if (GetContext().Commands == nullptr)
            {
                return true;
            }

            GetContext().Events.GetEventBus().Subscribe(this, &CommandProbeSubsystem::OnCheatUsed);

            m_Cheats = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.cheats", "TestWorld: the CheatUsed events this world saw: {count, last}."),
                [this](const nlohmann::json&)
                {
                    return Opaax::CommandResult::Ok(nlohmann::json{ { "count", m_CheatCount }, { "last", m_LastCheat } });
                });

            m_Ping = GetContext().Commands->Register(
                Opaax::CommandSpec("probe.ping", "TestWorld: answers {world, pings} in the Commands level."),
                [this](const nlohmann::json&)
                {
                    ++m_Pings;
                    return Opaax::CommandResult::Ok(nlohmann::json{
                        { "world", GetContext().OwningWorld.GetName().ToString() }, { "pings", m_Pings } });
                });
            return true;
        }

        void Shutdown() override { GetContext().Events.GetEventBus().UnsubscribeAll(this); }

    private:
        void OnCheatUsed(const Opaax::CheatUsed& InCheat)
        {
            ++m_CheatCount;
            m_LastCheat = InCheat.Command.ToString();
        }

        Opaax::CommandHandle m_Ping;
        Opaax::CommandHandle m_Cheats;
        Opaax::Int32         m_Pings      = 0;
        Opaax::Int32         m_CheatCount = 0;
        Opaax::OpaaxString   m_LastCheat;
    };
}
