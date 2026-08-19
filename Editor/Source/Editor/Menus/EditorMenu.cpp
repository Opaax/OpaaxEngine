#include "Editor/Menus/EditorMenu.h"

#include <imgui.h>

namespace Opaax::Editor
{
    EditorMenuCategory& EditorMenu::Category(const OpaaxStringID InID)
    {
        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            if (lCategory->GetID() == InID) { return *lCategory; }
        }

        m_Categories.push_back(MakeUnique<EditorMenuCategory>(InID));
        return *m_Categories.back();
    }

    void EditorMenu::Draw(EditorContext& InContext) const
    {
        if (!ImGui::BeginMainMenuBar())
        {
            return;
        }

        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            lCategory->Draw(InContext);
        }

        ImGui::EndMainMenuBar();
    }

    Uint64 EditorMenu::Count() const noexcept
    {
        Uint64 lCount = 0;
        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            lCount += lCategory->CountCommands();
        }

        return lCount;
    }
}
