#include "Editor/Menus/EditorMenu.h"

#include "Editor/EditorContext.h"

namespace Opaax::Editor
{
    EditorMenuCategory& EditorMenu::Category(const OpaaxStringID InID)
    {
        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            if (lCategory->GetID() == InID) { return *lCategory; }
        }

        m_Categories.emplace_back(MakeUnique<EditorMenuCategory>(InID));
        return *m_Categories.back();
    }

    void EditorMenu::Draw(EditorContext& InContext) const
    {
        // CATEGORIES ONLY — the bar they sit in is opened by whoever hosts it (the title bar),
        // which is also what lets the window buttons share that same row.
        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            lCategory->Draw(InContext);
        }
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
