#pragma once

#if OPAAX_WITH_EDITOR

#include "Editor/Assets/IAssetTypeActions.h"

namespace Opaax
{
    class CoreEngineApp;
}

namespace Opaax::Editor
{
    // =============================================================================
    // SceneTypeActions
    // =============================================================================
    /**
     * @class SceneTypeActions
     * IAssetTypeActions for SceneAsset.
     *
     * Load (= primary action): resolves the SceneAsset descriptor through
     * AssetRegistry, then opens that scene in the editor via SceneManager::Open.
     * Reload: refreshes the descriptor and reopens the scene.
     *
     * Holds a non-owning CoreEngineApp* — required because type-actions are not
     * subsystems and have no inherited GetEngineApp() accessor. The pointer is
     * injected at registration time by EditorSubsystem.
     */
    class OPAAX_API SceneTypeActions final : public IAssetTypeActions
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        explicit SceneTypeActions(CoreEngineApp* InEngineApp) : m_EngineApp(InEngineApp) {}

        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IAssetTypeActions interface
    public:
        OpaaxStringID GetTypeID() const override { return OpaaxStringID("Scene"); }
        const char*   GetIcon()   const override { return "[ S ]"; }
        const char*   GetLabel()  const override { return "Scene"; }

        void Load  (OpaaxStringID InID) override;
        void Reload(OpaaxStringID InID) override;
        //~ End IAssetTypeActions interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        CoreEngineApp* m_EngineApp = nullptr;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
