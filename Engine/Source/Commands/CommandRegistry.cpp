#include "Commands/CommandRegistry.h"

#include "Core/Algo/Algo.h"
#include "Core/Hash/OpaaxHash.h"
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
    CommandRegistry::CommandRegistry()
        : m_State(MakeShared<CommandRegistryState>())
    {
        Add("commands.list", "Lists every command with its help.",
            [this](const nlohmann::json&)
            {
                return CommandResult::Ok(nlohmann::json{ { "commands", Describe() } });
            });

        Add("frames.wait", "Lets {count} frames run (default 1) before the next request.",
            [](const nlohmann::json& InParams)
            {
                CommandResult lResult;
                lResult.WaitFrames = Maths::Max(InParams.value("count", 1u), 1u);
                return lResult;
            });
    }

    CommandRegistry::~CommandRegistry()
    {
        // Handles still alive find nothing to remove.
        m_State->Commands.clear();
    }

    Uint64 CommandRegistry::Insert(CommandSpec InSpec, CommandHandler InHandler)
    {
        const Uint64 lId = m_State->NextId++;

        TSharedPtr<RegisteredCommand> lCommand = MakeShared<RegisteredCommand>();
        lCommand->Spec    = Move(InSpec);
        lCommand->Handler = Move(InHandler);
        lCommand->Id      = lId;

        const OpaaxString lName = lCommand->Spec.GetName();
        m_State->Commands[lName].push_back(Move(lCommand));
        return lId;
    }

    void CommandRegistry::Add(CommandSpec InSpec, CommandHandler InHandler)
    {
        Insert(Move(InSpec), Move(InHandler));
    }

    void CommandRegistry::Add(const OpaaxString& InName, const OpaaxString& InHelp, CommandHandler InHandler)
    {
        Insert(CommandSpec(InName, InHelp), Move(InHandler));
    }

    CommandHandle CommandRegistry::Register(CommandSpec InSpec, CommandHandler InHandler)
    {
        OpaaxString  lName = InSpec.GetName();
        const Uint64 lId   = Insert(Move(InSpec), Move(InHandler));
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

    bool CommandRegistry::Has(const OpaaxString& InName) const
    {
        return Find(InName) != nullptr;
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
}
