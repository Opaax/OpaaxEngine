#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // AssetBrowserPanel
    //
    // Displays all assets currently loaded in AssetRegistry.
    // Read-only for now — drag & drop to Inspector comes in a future pass.
    //
    // TODO: Directory browsing + load-on-click (M8)
    // TODO: Drag & drop to SpriteComponent texture field (M8)
    // =============================================================================
    class AssetBrowserPanel
    {
    public:
        AssetBrowserPanel()  = default;
        ~AssetBrowserPanel() = default;

        void Draw();
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR