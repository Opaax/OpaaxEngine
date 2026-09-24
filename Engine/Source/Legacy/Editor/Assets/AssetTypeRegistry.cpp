#include "AssetTypeRegistry.h"

#if OPAAX_WITH_EDITOR

namespace Opaax::Editor
{
    IAssetTypeActions* AssetTypeRegistry::Find(OpaaxStringID InTypeID)
    {
        for (auto& lAction : GetStorage())
        {
            if (lAction->GetTypeID() == InTypeID)
            {
                return lAction.get();
            }
        }
        return nullptr;
    }

    const char* AssetTypeRegistry::GetIcon(OpaaxStringID InTypeID)
    {
        const IAssetTypeActions* lAction = Find(InTypeID);
        return lAction ? lAction->GetIcon() : "[ ? ]";
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
