// EditorCommandRegistry.h
#pragma once

#include <typeindex>

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/EditorContext.h"
#include "Editor/Commands/IEditorCommand.h"

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(EditorCommandRegistry);

    // =============================================================================
    // EditorCommandRegistry — the real storage behind EditorExtensionRegistrar::Commands().
    //
    //   TAG-KEYED and TYPE-ERASED, so a caller invokes a verb it cannot name: the menu bar, the
    //   Resource Browser and a game module all dispatch by OpaaxTag (I14) with a payload, and the
    //   command's own type stays private to the TU that registered it.
    //
    //   The PARAMS TYPE IS CHECKED AT DISPATCH, not at compile time — that is the price of the
    //   erasure, and the typeid gate is what makes a mismatch a logged refusal instead of a
    //   reinterpret_cast into the wrong struct.
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
        /** Store T under InTag. A tag registered twice keeps the FIRST command and warns. */
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
         * Run the command registered under InTag.
         *
         * CONST because dispatching is a read: only Register mutates. That is what lets a panel or a
         * menu closure invoke through EditorContext::Extensions, which is const by construction so
         * nothing can register after the seal.
         *
         * @return false — with a log saying which — when no command carries that tag, or when
         *   TParams is not the type it was registered with.
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

            // NOTHING IS BRACKETED HERE (⑤, **UN1**). A verb records its own undo step because it
            // is the one that knows what changed; the dispatch just runs it.
            IEditorCommand lCommand = lCommandEntry.Create();
            lCommand.Execute(InContext, InParams);

            return true;
        }

        /** The argument-less form — every command that takes nothing is registered with NoParams. */
        bool Execute(const OpaaxTag& InTag, EditorContext& InContext) const
        {
            return Execute(InTag, InContext, NoParams{});
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return How many commands were registered — the seal log reports it like every other route. */
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
