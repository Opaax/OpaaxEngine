#include "Editor/Menus/EditorMenuCategory.h"

#include "Editor/Menus/EditorMenuSeparatorNode.h"

#include <imgui.h>

using namespace Opaax; // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace Opaax::Editor
{
    EditorMenuCategory* EditorMenuCategory::FindCategory(const OpaaxStringID InID)
    {
        for (const TUniquePtr<IEditorMenuNode>& lChild : m_Children)
        {
            if (lChild->GetID() == InID) { return lChild->AsCategory(); }
        }

        return nullptr;
    }

    EditorMenuCommandNode* EditorMenuCategory::FindCommand(const OpaaxStringID InID)
    {
        for (const TUniquePtr<IEditorMenuNode>& lChild : m_Children)
        {
            if (lChild->GetID() == InID) { return lChild->AsCommand(); }
        }

        return nullptr;
    }

    EditorMenuCategory& EditorMenuCategory::SubCategory(const OpaaxStringID InID)
    {
        if (EditorMenuCategory* lExisting = FindCategory(InID))
        {
            return *lExisting;
        }

        m_Children.push_back(MakeUnique<EditorMenuCategory>(InID, GetPath()));
        return *m_Children.back()->AsCategory();
    }

    EditorMenuCommandNode& EditorMenuCategory::AddCommand(const OpaaxStringID InLabel, const OpaaxTag& InCommand)
    {
        if (EditorMenuCommandNode* lExisting = FindCommand(InLabel))
        {
            OPAAX_LOG(LogEditorMenu, Warn,
                      "'{}/{}' is already registered — the second entry is dropped and '{}' keeps the first",
                      GetPath().CStr(), InLabel.CStr(), lExisting->GetCommand());
            return *lExisting;
        }

        m_Children.push_back(MakeUnique<EditorMenuCommandNode>(InLabel, GetPath(), InCommand));
        return *m_Children.back()->AsCommand();
    }

    void EditorMenuCategory::AddSeparator()
    {
        m_Children.push_back(MakeUnique<EditorMenuSeparatorNode>(GetPath()));
    }

    EditorMenuCategory& EditorMenuCategory::SetEnabled(FMenuPredicate InPredicate)
    {
        m_IsEnabled = Move(InPredicate);
        return *this;
    }

    void EditorMenuCategory::Draw(EditorContext& InContext) const
    {
        // An empty menu would open onto nothing — a category someone declared and never filled is
        // simply not on the bar.
        if (m_Children.empty()) { return; }

        const bool bEnabled = !m_IsEnabled || m_IsEnabled(InContext);

        if (!ImGui::BeginMenu(GetLabel(), bEnabled))
        {
            return;
        }

        for (const TUniquePtr<IEditorMenuNode>& lChild : m_Children)
        {
            lChild->Draw(InContext);
        }

        ImGui::EndMenu();
    }

    Uint64 EditorMenuCategory::CountCommands() const noexcept
    {
        Uint64 lCount = 0;
        for (const TUniquePtr<IEditorMenuNode>& lChild : m_Children)
        {
            lCount += lChild->CountCommands();
        }

        return lCount;
    }
}
