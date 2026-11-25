#include "Renderer/Materials/MaterialShaderCache.h"

#include "Renderer/Materials/SpriteShader.h"

namespace Opaax
{
    IShader* MaterialShaderCache::Resolve(const OpaaxString& InPath, const SpriteShader* InAsset)
    {
        for (const Slot& lSlot : m_Slots)
        {
            if (lSlot.Path == InPath)
            {
                return lSlot.Shader.get();
            }
        }

        Slot& lSlot = m_Slots.emplace_back();
        lSlot.Path  = InPath;

        if (InAsset == nullptr)
        {
            OPAAX_LOG(LogMaterialShader, Error, "shader '{}' did not load: its sprites use the engine's sprite shader",
                      InPath.CStr());
            return nullptr;
        }

        if (!InAsset->HasStages())
        {
            OPAAX_LOG(LogMaterialShader, Error,
                      "shader '{}' has no '#type vertex' and '#type fragment' sections: its sprites use the "
                      "engine's sprite shader", InPath.CStr());
            return nullptr;
        }

        TUniquePtr<IShader> lShader = m_Create ? m_Create(InAsset->GetDesc()) : TUniquePtr<IShader>();
        if (lShader == nullptr || !lShader->IsValid())
        {
            OPAAX_LOG(LogMaterialShader, Error, "shader '{}' does not compile: its sprites use the engine's sprite shader",
                      InPath.CStr());
            return nullptr;
        }

        lSlot.Shader = Move(lShader);
        return lSlot.Shader.get();
    }

    TDynArray<MaterialShaderCache::Entry> MaterialShaderCache::GetEntries() const
    {
        TDynArray<Entry> lEntries;
        lEntries.reserve(m_Slots.size());
        for (const Slot& lSlot : m_Slots)
        {
            lEntries.emplace_back(Entry{ lSlot.Path, lSlot.Shader != nullptr });
        }
        return lEntries;
    }

    void MaterialShaderCache::Clear(const TFunction<void(const IShader&)>& InBeforeDestroy)
    {
        for (const Slot& lSlot : m_Slots)
        {
            if (lSlot.Shader != nullptr && InBeforeDestroy)
            {
                InBeforeDestroy(*lSlot.Shader);
            }
        }
        m_Slots.clear();
    }
}
