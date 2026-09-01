#include "Editor/TitleBar/TitleBarRegistry.h"

namespace Opaax::Editor
{
    EditorMenuCategory& TitleBarRegistry::Category(const OpaaxStringID InID)
    {
        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            if (lCategory->GetID() == InID) { return *lCategory; }
        }

        m_Categories.emplace_back(MakeUnique<EditorMenuCategory>(InID));
        return *m_Categories.back();
    }

    Uint64 TitleBarRegistry::Count() const noexcept
    {
        Uint64 lCount = 0;
        for (const TUniquePtr<EditorMenuCategory>& lCategory : m_Categories)
        {
            lCount += lCategory->CountCommands();
        }

        return lCount;
    }
}
