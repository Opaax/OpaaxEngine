#pragma once

#include "Core/OpaaxTypes.h"

#include "ResourceConcept.hpp"
#include "ResourceManager.h"   // ResourceRef<T>'s members are defined there (the Ref <-> Manager cycle-break)
#include "ResourceRef.hpp"

// =============================================================================
// ResourceHold — a claim on a resource whose TYPE the holder does not know (⑦-C P5b).
//
//   A hard reference is discovered from a component's json by name and ResourceTypeID, so whoever
//   acquires it has an integer, not a T. ResourceRef<T> cannot be stored without T; this is the
//   erased envelope around one — a virtual destructor releases through the typed ref, and
//   IsLoaded answers the only question the holder ever asks.
//
//   `AcquireHold<T>` is what `ResourceFormatRegistry::Register<T>` bakes into its entry, which is
//   how a runtime type id turns back into a typed Load: the registering site knows T, the table
//   keeps a pointer to this instantiation, and the ResourceManager's surface is not touched.
// =============================================================================
namespace Opaax
{
    class IResourceHold
    {
    public:
        virtual ~IResourceHold() = default;

        /** False for a FailFast type that could not load — its ref is null, and the holder warns. */
        virtual bool IsLoaded() const noexcept = 0;
    };

    template<CResource T>
    class TResourceHold final : public IResourceHold
    {
    public:
        explicit TResourceHold(ResourceRef<T> InRef) noexcept : m_Ref(Move(InRef)) {}

        bool IsLoaded() const noexcept override { return m_Ref.Get() != nullptr; }

    private:
        ResourceRef<T> m_Ref;
    };

    /** The typed Load behind an erased entry — see ResourceFormatEntry::Acquire. */
    template<CResource T>
    TUniquePtr<IResourceHold> AcquireHold(ResourceManager& InManager, const char* InAbsPath)
    {
        return MakeUnique<TResourceHold<T>>(InManager.Load<T>(InAbsPath));
    }

    using ResourceAcquireFn = TUniquePtr<IResourceHold> (*)(ResourceManager&, const char*);
}
