#include "AssetTypeRegistry.h"

#if OPAAX_WITH_EDITOR

namespace Opaax::Editor
{
    namespace
    {
        TDynArray<UniquePtr<IAssetTypeActions>>& GetRegistry()
        {
            static TDynArray<UniquePtr<IAssetTypeActions>> s_Actions;
            return s_Actions;
        }
    }

    void AssetTypeRegistry::Register(UniquePtr<IAssetTypeActions> InActions)
    {
        GetRegistry().push_back(Move(InActions));
    }

    IAssetTypeActions* AssetTypeRegistry::Find(OpaaxStringID InTypeID)
    {
        for (auto& lAction : GetRegistry())
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

    const TDynArray<UniquePtr<IAssetTypeActions>>& AssetTypeRegistry::GetAll()
    {
        return GetRegistry();
    }

    void AssetTypeRegistry::Clear()
    {
        GetRegistry().clear();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
