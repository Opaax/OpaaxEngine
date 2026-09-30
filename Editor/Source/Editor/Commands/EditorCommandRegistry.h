// EditorCommandRegistry.h
#pragma once

#include <typeindex>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/EditorContext.h"
#include "Editor/Commands/IEditorCommand.h"

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(EditorCommandRegistry);

    // =============================================================================
    // EditorCommandRegistry — the storage behind EditorExtensionRegistrar::Commands().
    //   Keyed by tag, type-erased: menus, the Resource Browser and game modules dispatch by OpaaxTag
    //   with a payload, without knowing the command's type. The payload type is checked at dispatch
    //   (a mismatch is logged and refused).
    // =============================================================================
    class EditorCommandRegistry
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorCommandRegistry() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Stores T under InTag. A tag registered twice keeps the first command and warns. */
        template<EditorCommand<EditorContext> T>
        void Register(const OpaaxTag& InTag)
        {
            if (m_Commands.contains(InTag))
            {
                OPAAX_LOG(LogEditorCommandRegistry, Warn,
                          "Command '{}' is already registered — the second registration is dropped", InTag);
                return;
            }

            m_Commands.emplace(InTag, EditorCommandRegisteryEntry{
                                   .ParamsType = typeid(typename T::Params),
                                   .Create = []()
                                   {
                                       return IEditorCommand(T{});
                                   }
                               });
        }

        /**
         * Runs the command registered under InTag. Const: dispatching does not change the registry.
         * @return False (logged) when no command has that tag, or TParams is not its registered type
         */
        template<typename TParams>
        bool Execute(const OpaaxTag& InTag, EditorContext& InContext, const TParams& InParams) const
        {
            const auto lEntry = m_Commands.find(InTag);

            if (lEntry == m_Commands.end())
            {
                OPAAX_LOG(LogEditorCommandRegistry, Warn, "No command registered for '{}'", InTag);
                return false;
            }

            const EditorCommandRegisteryEntry& lCommandEntry = lEntry->second;

            if (lCommandEntry.ParamsType != typeid(TParams))
            {
                OPAAX_LOG(LogEditorCommandRegistry, Error,
                          "Command '{}' expects {} but was given {} — not executed", InTag,
                          lCommandEntry.ParamsType.name(), typeid(TParams).name());
                return false;
            }

            // No undo bracketing here: each action records its own undo step.
            IEditorCommand lCommand = lCommandEntry.Create();
            lCommand.Execute(InContext, InParams);

            return true;
        }

        /** The argument-less form (commands registered with NoParams). */
        bool Execute(const OpaaxTag& InTag, EditorContext& InContext) const
        {
            return Execute(InTag, InContext, NoParams{});
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return How many commands were registered. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Commands.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct EditorCommandRegisteryEntry
        {
            std::type_index ParamsType;
            TFunction<IEditorCommand()> Create;
        };

        TUnorderedMap<OpaaxTag, EditorCommandRegisteryEntry> m_Commands;
    };
}
