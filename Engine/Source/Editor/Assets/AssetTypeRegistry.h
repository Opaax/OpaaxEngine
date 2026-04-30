#pragma once

#if OPAAX_WITH_EDITOR

#include "IAssetTypeActions.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /**
     * @class AssetTypeRegistry
     * Maps OpaaxStringID type tags to IAssetTypeActions implementations.
     * Call Register() at startup (EditorSubsystem::Startup).
     * Unknown types fall back to icon "[ ? ]" and no-op load/reload.
     */
    class OPAAX_API AssetTypeRegistry
    {
    public:
        static void Register(UniquePtr<IAssetTypeActions> InActions);

        /**
         * @return nullptr if no actions are registered for InTypeID.
         */
        static IAssetTypeActions* Find(OpaaxStringID InTypeID);

        /**
         * @return "[ ? ]" fallback if InTypeID has no registered actions.
         */
        static const char* GetIcon(OpaaxStringID InTypeID);

        /**
         * All registered types in registration order — used by toolbar filter buttons.
         */
        static const TDynArray<UniquePtr<IAssetTypeActions>>& GetAll();

        static void Clear();
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
