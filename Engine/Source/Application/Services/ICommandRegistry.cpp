#include "Application/Services/ICommandRegistry.h"

#include "Commands/CommandHandle.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullCommandRegistry — keeps nothing.
        // =====================================================================
        class NullCommandRegistry final : public ICommandRegistry
        {
        public:
            bool          IsNull() const noexcept override { return true; }
            void          Add(CommandSpec, CommandHandler) override {}
            CommandHandle Register(CommandSpec, CommandHandler) override { return CommandHandle(); }

            TSharedPtr<const RegisteredCommand> Find(const OpaaxString&) const override { return nullptr; }

            nlohmann::json Describe() const override { return nlohmann::json::array(); }
            TDynArray<CommandSpec> GetSpecs() const override { return {}; }
            bool                   AllowsDevOnly() const override { return false; }

            void NotifySucceeded(const RegisteredCommand&) override {}
            bool HaveCheatsRun() const override { return false; }
            void ClearCheatsRun() override {}
            void SetCheatListener(TFunction<void(const CheatUsed&)>) override {}
        };
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
    // =========================================================================
    ServiceTypeID ICommandRegistry::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    ICommandRegistry& ICommandRegistry::Null()
    {
        static NullCommandRegistry s_Null;
        return s_Null;
    }

    // =========================================================================
    // ICommandRegistry
    // =========================================================================
    void ICommandRegistry::Add(const OpaaxString& InName, const OpaaxString& InHelp, CommandHandler InHandler)
    {
        Add(CommandSpec(InName, InHelp), Move(InHandler));
    }

    bool ICommandRegistry::Has(const OpaaxString& InName) const
    {
        return Find(InName) != nullptr;
    }
}
