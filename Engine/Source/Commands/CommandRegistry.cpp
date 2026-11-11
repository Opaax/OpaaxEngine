#include "Commands/CommandRegistry.h"

#include "Commands/CommandText.h"
#include "Core/Algo/Algo.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/Log/Logger.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    // =========================================================================
    // State
    // =========================================================================

    /** What the registry and its handles share: a handle that outlives the registry finds it empty. */
    struct CommandRegistryState
    {
        /** Per name, its registrations, the newest last. */
        TUnorderedMap<OpaaxString, TDynArray<TSharedPtr<RegisteredCommand>>> Commands;
        Uint64 NextId = 1;

        void Remove(const OpaaxString& InName, Uint64 InId)
        {
            const auto lIt = Commands.find(InName);
            if (lIt == Commands.end())
            {
                return;
            }

            TDynArray<TSharedPtr<RegisteredCommand>>& lStack = lIt->second;
            Algo::RemoveIf(lStack, [InId](const TSharedPtr<RegisteredCommand>& InCommand) { return InCommand->Id == InId; });
            if (lStack.empty())
            {
                Commands.erase(lIt);
            }
        }
    };

    // =========================================================================
    // CommandHandle
    // =========================================================================
    CommandHandle::CommandHandle(TSharedPtr<CommandRegistryState> InState, OpaaxString InName, Uint64 InId)
        : m_State(Move(InState))
        , m_Name(Move(InName))
        , m_Id(InId)
    {
    }

    CommandHandle::~CommandHandle()
    {
        Reset();
    }

    CommandHandle::CommandHandle(CommandHandle&& InOther) noexcept
        : m_State(Move(InOther.m_State))
        , m_Name(Move(InOther.m_Name))
        , m_Id(InOther.m_Id)
    {
        InOther.m_State = nullptr;
        InOther.m_Id    = 0;
    }

    CommandHandle& CommandHandle::operator=(CommandHandle&& InOther) noexcept
    {
        if (this != &InOther)
        {
            Reset();
            m_State = Move(InOther.m_State);
            m_Name  = Move(InOther.m_Name);
            m_Id    = InOther.m_Id;
            InOther.m_State = nullptr;
            InOther.m_Id    = 0;
        }
        return *this;
    }

    void CommandHandle::Reset()
    {
        if (m_State != nullptr)
        {
            m_State->Remove(m_Name, m_Id);
            m_State = nullptr;
            m_Id    = 0;
        }
    }

    // =========================================================================
    // CommandRegistry
    // =========================================================================
    CommandRegistry::CommandRegistry(const bool bInDevBuild)
        : m_State(MakeShared<CommandRegistryState>())
        , m_bDevBuild(bInDevBuild)
    {
        Add("commands.list", "Lists every command with its help.",
            [this](const nlohmann::json&)
            {
                return CommandResult::Ok(nlohmann::json{ { "commands", Describe() } });
            });

        Add(CommandSpec("commands.has", "Whether {name} is a command now: {has}.")
                .Param("name", ECommandParamType::String, "the command's name"),
            [this](const nlohmann::json& InParams)
            {
                const OpaaxString lName = InParams.value("name", OpaaxString());
                if (lName.IsEmpty())
                {
                    return CommandResult::Fail("commands.has needs {name}, a command's name");
                }
                return CommandResult::Ok(nlohmann::json{ { "has", Has(lName) } });
            });

        Add("cheats.info", "Whether a cheat answered ok since the game started: {cheatsRun}.",
            [this](const nlohmann::json&)
            {
                return CommandResult::Ok(nlohmann::json{ { "cheatsRun", HaveCheatsRun() } });
            });

        Add(CommandSpec("frames.wait", "Lets {count} frames run (default 1) before the next request.")
                .Param("count", ECommandParamType::Int, "frames (1)").Optional(),
            [](const nlohmann::json& InParams)
            {
                CommandResult lResult;
                lResult.WaitFrames = Maths::Max(InParams.value("count", 1u), 1u);
                return lResult;
            });

        Add(CommandSpec("console.run",
            "Runs a {line} as the console reads it (\"time.scale 0.25\", \"level.play Levels/Main.opaaxlevel\"): "
            "its command answers, and waits, as it would. An error names what is wrong with the line.")
                .Param("line", ECommandParamType::String, "the command and its words")
                .DevOnly(),
            [this](const nlohmann::json& InParams)
            {
                const CommandTextResult lLine = ParseCommandText(InParams.value("line", OpaaxString()), *this);
                if (!lLine.bOk)
                {
                    return CommandResult::Fail(lLine.Error);
                }

                // Held while it runs; a cheat is told for the command the line ran, not for console.run.
                const TSharedPtr<const RegisteredCommand> lCommand = Find(lLine.Command);
                CommandResult lResult = lCommand->Handler(lLine.Params);
                if (lResult.bOk)
                {
                    NotifySucceeded(*lCommand);
                }
                return lResult;
            });
    }

    CommandRegistry::~CommandRegistry()
    {
        // Handles still alive find nothing to remove.
        m_State->Commands.clear();
    }

    void CommandRegistry::Add(CommandSpec InSpec, CommandHandler InHandler)
    {
        if (IsRefused(InSpec))
        {
            OPAAX_LOG(LogCommands, Trace, "Command '{}' is dev only: not registered in a ship build", InSpec.GetName());
            return;
        }

        TSharedPtr<RegisteredCommand> lCommand = MakeShared<RegisteredCommand>();
        lCommand->Spec    = Move(InSpec);
        lCommand->Handler = Move(InHandler);
        lCommand->Id      = m_State->NextId++;

        const OpaaxString lName = lCommand->Spec.GetName();
        m_State->Commands[lName].push_back(Move(lCommand));
    }

    CommandHandle CommandRegistry::Register(CommandSpec InSpec, CommandHandler InHandler)
    {
        if (IsRefused(InSpec))
        {
            OPAAX_LOG(LogCommands, Trace, "Command '{}' is dev only: not registered in a ship build", InSpec.GetName());
            return CommandHandle();
        }

        OpaaxString  lName = InSpec.GetName();
        const Uint64 lId   = m_State->NextId;
        Add(Move(InSpec), Move(InHandler));
        return CommandHandle(m_State, Move(lName), lId);
    }

    TSharedPtr<const RegisteredCommand> CommandRegistry::Find(const OpaaxString& InName) const
    {
        const auto lIt = m_State->Commands.find(InName);
        if (lIt == m_State->Commands.end() || lIt->second.empty())
        {
            return nullptr;
        }
        return lIt->second.back();
    }

    nlohmann::json CommandRegistry::Describe() const
    {
        TDynArray<OpaaxString> lNames;
        lNames.reserve(m_State->Commands.size());
        for (const auto& [lName, lStack] : m_State->Commands)
        {
            lNames.push_back(lName);
        }
        Algo::Sort(lNames);

        nlohmann::json lList = nlohmann::json::array();
        for (const OpaaxString& lName : lNames)
        {
            lList.push_back(Find(lName)->Spec.ToJson());
        }
        return lList;
    }

    void CommandRegistry::NotifySucceeded(const RegisteredCommand& InCommand)
    {
        if (!InCommand.Spec.IsCheat())
        {
            return;
        }

        m_bCheatsRun = true;
        OPAAX_LOG(LogCommands, Info, "Cheat used: {}", InCommand.Spec.GetName());

        if (m_CheatListener)
        {
            m_CheatListener(CheatUsed{ OpaaxStringID(InCommand.Spec.GetName()) });
        }
    }

    void CommandRegistry::SetCheatListener(TFunction<void(const CheatUsed&)> InListener)
    {
        m_CheatListener = Move(InListener);
    }
}
