#pragma once

#if OPAAX_WITH_EDITOR

#include "IAssetTypeActions.h"
#include "Core/Container/TPolymorphicList.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /**
     * @class AssetTypeRegistry
     * Maps OpaaxStringID type tags to IAssetTypeActions implementations.
     * Call Register() at startup (EditorSubsystem::Startup).
     * Unknown types fall back to icon "[ ? ]" and no-op load/reload.
     *
     * Storage / Register / Clear / GetAll are inherited from TPolymorphicList.
     */
    class OPAAX_API AssetTypeRegistry : public TPolymorphicList<IAssetTypeActions>
    {
    public:
        /**
         * @return nullptr if no actions are registered for InTypeID.
         */
        static IAssetTypeActions* Find(OpaaxStringID InTypeID);

        /**
         * @return "[ ? ]" fallback if InTypeID has no registered actions.
         */
        static const char* GetIcon(OpaaxStringID InTypeID);
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
