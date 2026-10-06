#include "SceneFactory.h"

#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    TDynArray<SceneFactory::Entry>& SceneFactory::Registry()
    {
        static TDynArray<Entry> s_Registry;
        return s_Registry;
    }

    void SceneFactory::Register(const char* InName, Factory InFactory)
    {
        OPAAX_CORE_ASSERT(InName && InName[0] != '\0')
        OPAAX_CORE_ASSERT(InFactory)

        const OpaaxStringID lID(InName);

        auto& lRegistry = Registry();
        for (const Entry& lExisting : lRegistry)
        {
            if (lExisting.Name == lID)
            {
                OPAAX_CORE_WARN("SceneFactory::Register — '{}' already registered, skipping.", InName);
                return;
            }
        }

        lRegistry.push_back({ lID, std::move(InFactory) });
        OPAAX_CORE_TRACE("SceneFactory: registered '{}'.", InName);
    }

    UniquePtr<Scene> SceneFactory::Create(const OpaaxStringID& InName)
    {
        for (const Entry& lEntry : Registry())
        {
            if (lEntry.Name == InName)
            {
                return lEntry.Make();
            }
        }
        return nullptr;
    }

    bool SceneFactory::Has(const OpaaxStringID& InName)
    {
        for (const Entry& lEntry : Registry())
        {
            if (lEntry.Name == InName) { return true; }
        }
        return false;
    }

    void SceneFactory::Clear()
    {
        Registry().clear();
    }
}
