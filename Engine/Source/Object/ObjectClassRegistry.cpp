#include "Object/ObjectClassRegistry.h"

#include <spdlog/fmt/ranges.h>   // fmt::join

#include "Core/Algo/Algo.h"
#include "Resources/ResourceFormat.h"   // NormalizeExtension

namespace Opaax
{
    bool ObjectClassRegistry::Register(const ObjectClass& InClass)
    {
        if (m_bSealed)
        {
            OPAAX_LOG(LogObjectClassRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). Register classes with "
                      "OPAAX_REGISTER_OBJECT.", InClass.GetName());
            return false;
        }

        // The name is what files store, so it must name one class only.
        if (FindByName(InClass.GetName()) != nullptr || FindByTypeId(InClass.GetTypeId()) != nullptr)
        {
            OPAAX_LOG(LogObjectClassRegistry, Error, "Register '{}' — that name or type is already registered.",
                      InClass.GetName());
            return false;
        }

        // One class reads the files of one extension.
        TDynArray<OpaaxStringID> lExtensions;
        for (const char* lRaw : InClass.GetSourceExtensions())
        {
            const OpaaxStringID lExtension = NormalizeExtension(OpaaxStringView(lRaw));
            if (!lExtension.IsValid())
            {
                OPAAX_LOG(LogObjectClassRegistry, Error, "Register '{}' — OPAAX_ASSET_SOURCE lists an empty extension.",
                          InClass.GetName());
                return false;
            }

            const ObjectClass* lOwner = FindBySourceExtension(lExtension);
            if (lOwner != nullptr || Algo::Contains(lExtensions, lExtension))
            {
                OPAAX_LOG(LogObjectClassRegistry, Error, "Register '{}' — '{}' files are read by '{}' already.",
                          InClass.GetName(), lExtension, lOwner != nullptr ? lOwner->GetName() : InClass.GetName());
                return false;
            }

            lExtensions.emplace_back(lExtension);
        }

        m_Classes.emplace_back(&InClass);
        m_ByName.emplace(InClass.GetName().GetId(), &InClass);

        for (const OpaaxStringID lExtension : lExtensions)
        {
            m_BySourceExtension.emplace(lExtension.GetId(), &InClass);
        }

        return true;
    }

    bool ObjectClassRegistry::AddAlias(const OpaaxStringID InOldName, const OpaaxStringID InName)
    {
        if (m_bSealed)
        {
            OPAAX_LOG(LogObjectClassRegistry, Error, "Alias '{}' — registry is SEALED.", InOldName);
            return false;
        }

        const ObjectClass* lClass = FindByName(InName);
        if (lClass == nullptr)
        {
            OPAAX_LOG(LogObjectClassRegistry, Error, "Alias '{}' -> '{}' — no class named '{}' is registered.",
                      InOldName, InName, InName);
            return false;
        }

        // One file name, one class.
        if (FindByName(InOldName) != nullptr)
        {
            OPAAX_LOG(LogObjectClassRegistry, Error, "Alias '{}' — that name is already taken.", InOldName);
            return false;
        }

        m_ByName.emplace(InOldName.GetId(), lClass);
        return true;
    }

    void ObjectClassRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        TDynArray<OpaaxStringID> lNames;
        for (const ObjectClass* lClass : m_Classes) { lNames.push_back(lClass->GetName()); }

        OPAAX_LOG(LogObjectClassRegistry, Info, "Sealed with {} class(es): {}",
                  static_cast<Uint64>(m_Classes.size()), fmt::join(lNames, ", "));
    }

    const ObjectClass* ObjectClassRegistry::FindByName(const OpaaxStringID InName) const noexcept
    {
        const auto lIt = m_ByName.find(InName.GetId());
        return lIt != m_ByName.end() ? lIt->second : nullptr;
    }

    const ObjectClass* ObjectClassRegistry::FindByTypeId(const TypeId InTypeId) const noexcept
    {
        for (const ObjectClass* lClass : m_Classes)
        {
            if (lClass->GetTypeId() == InTypeId) { return lClass; }
        }

        return nullptr;
    }

    const ObjectClass* ObjectClassRegistry::FindBySourceExtension(const OpaaxStringID InExtension) const noexcept
    {
        const auto lIt = m_BySourceExtension.find(InExtension.GetId());
        return lIt != m_BySourceExtension.end() ? lIt->second : nullptr;
    }

    TDynArray<const ObjectClass*> ObjectClassRegistry::GetClassesOf(const ObjectClass& InBase,
                                                                    const bool bInConcreteOnly) const
    {
        TDynArray<const ObjectClass*> lClasses;

        for (const ObjectClass* lClass : m_Classes)
        {
            if (lClass->IsChildOf(InBase) && !(bInConcreteOnly && lClass->IsAbstract()))
            {
                lClasses.emplace_back(lClass);
            }
        }

        return lClasses;
    }
}
