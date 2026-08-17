// EditorCommandRegistry.h
#pragma once

#include <typeindex>

#include "Core/OpaaxTypes.h"
#include "Editor/EditorContext.h"
#include "Editor/Commands/IEditorCommand.h"
#include "Core/Tag/OpaaxTag.h"

namespace Opaax::Editor
{
    class EditorCommandRegistry
    {
    public:
        EditorCommandRegistry() = default;

    private:
        struct EditorCommandRegisteryEntry
        {
            std::type_index ParamsType;
            TFunction<IEditorCommand()> Create;
        };

        TUnorderedMap<OpaaxTag, EditorCommandRegisteryEntry> m_Commands;

    public:
        template<EditorCommand<EditorContext> T>
        void Register(const OpaaxTag& Tag)
        {
            if (m_Commands.contains(Tag))
            {
                //TODO Log
                return;
            }


            m_Commands.emplace(Tag, EditorCommandRegisteryEntry{
                    .ParamsType = typeid(typename T::Params),
                    .Create = []()
                    {
                        return IEditorCommand(T{});
                    }
                });
        }

        template<typename TParams>
        bool ExecuteCommand( const OpaaxTag& Tag, EditorContext& Context, const TParams& Params)
        {
            auto lEntry = m_Commands.find(Tag);

            if (lEntry == m_Commands.end())
            {
                //TODO Log
                return false;
            }

            const EditorCommandRegisteryEntry& lCommandEntry = lEntry->second;

            if (lCommandEntry.ParamsType != typeid(TParams))
            {
                //TODO Log
                return false;
            }

            IEditorCommand Command = lCommandEntry.Create();
            Command.Execute(Context, Params);

            return true;
        }
    };
}
