#include "Editor/Menus/EditorTitleBarCategory.h"

#include "Editor/EditorContext.h"
#include "Editor/Menus/EditorTitleBarSeparatorNode.h"
#include "Editor/UI/IEditorGui.h"

using namespace Opaax; // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace Opaax::Editor
{
    EditorTitleBarCategory* EditorTitleBarCategory::FindCategory(const OpaaxStringID InID)
    {
        for (const TUniquePtr<IEditorTitleBarNode>& lChild : m_Children)
        {
            if (lChild->GetID() == InID) { return lChild->AsCategory(); }
        }

        return nullptr;
    }

    EditorTitleBarCommandNode* EditorTitleBarCategory::FindCommand(const OpaaxStringID InID)
    {
        for (const TUniquePtr<IEditorTitleBarNode>& lChild : m_Children)
        {
            if (lChild->GetID() == InID) { return lChild->AsCommand(); }
        }

        return nullptr;
    }

    EditorTitleBarCategory& EditorTitleBarCategory::SubCategory(const OpaaxStringID InID)
    {
        if (EditorTitleBarCategory* lExisting = FindCategory(InID))
        {
            return *lExisting;
        }

        m_Children.emplace_back(MakeUnique<EditorTitleBarCategory>(InID, GetPath()));
        return *m_Children.back()->AsCategory();
    }

    EditorTitleBarCommandNode& EditorTitleBarCategory::AddCommand(const OpaaxStringID InLabel, const OpaaxTag& InCommand)
    {
        if (EditorTitleBarCommandNode* lExisting = FindCommand(InLabel))
        {
            OPAAX_LOG(LogEditorMenu, Warn,
                      "'{}/{}' is already registered — the second entry is dropped and '{}' keeps the first",
                      GetPath().CStr(), InLabel.CStr(), lExisting->GetCommand());
            return *lExisting;
        }

        m_Children.emplace_back(MakeUnique<EditorTitleBarCommandNode>(InLabel, GetPath(), InCommand));
        return *m_Children.back()->AsCommand();
    }

    void EditorTitleBarCategory::AddSeparator()
    {
        m_Children.emplace_back(MakeUnique<EditorTitleBarSeparatorNode>(GetPath()));
    }

    EditorTitleBarCategory& EditorTitleBarCategory::SetEnabled(FMenuPredicate InPredicate)
    {
        m_IsEnabled = Move(InPredicate);
        return *this;
    }

    void EditorTitleBarCategory::Draw(EditorContext& InContext) const
    {
        // An empty menu would open onto nothing — a category someone declared and never filled is
        // simply not on the bar.
        if (m_Children.empty()) { return; }

        const bool bEnabled = !m_IsEnabled || m_IsEnabled(InContext);

        IEditorGui& lGui = InContext.Gui;

        if (!lGui.BeginMenu(GetLabel(), bEnabled))
        {
            return;
        }

        for (const TUniquePtr<IEditorTitleBarNode>& lChild : m_Children)
        {
            lChild->Draw(InContext);
        }

        lGui.EndMenu();
    }

    Uint64 EditorTitleBarCategory::CountCommands() const noexcept
    {
        Uint64 lCount = 0;
        for (const TUniquePtr<IEditorTitleBarNode>& lChild : m_Children)
        {
            lCount += lChild->CountCommands();
        }

        return lCount;
    }
}
