#include "SceneTypeActions.h"

#if OPAAX_WITH_EDITOR

#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"
#include "Scene/SceneManager.h"

namespace Opaax::Editor
{
    void SceneTypeActions::Load(OpaaxStringID InID)
    {
        SceneManager* lSceneMgr = m_EngineApp ? m_EngineApp->GetSceneManager() : nullptr;
        if (!lSceneMgr)
        {
            OPAAX_CORE_WARN("SceneTypeActions::Load — no SceneManager available.");
            return;
        }

        // SceneManager owns the registry-membership invariant (unload prior, load new,
        // resolve path, swap world, track current). SceneTypeActions just dispatches.
        lSceneMgr->LoadScene(InID);
    }

    void SceneTypeActions::Reload(OpaaxStringID InID)
    {
        Load(InID);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
