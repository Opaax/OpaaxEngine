#pragma once

#include "Core/OpaaxTypes.h"

#include "Resources/ResourceConcept.hpp"
#include "Resources/ResourceManager.h"   // ResourceRef<T> members are defined there
#include "Resources/ResourceRef.hpp"

// =============================================================================
// ResourceHold — keeps a resource loaded without knowing its type (only its type id).
//   Created through ResourceFormatEntry::Acquire.
// =============================================================================
namespace Opaax
{
    class IResourceHold
    {
    public:
        virtual ~IResourceHold() = default;

        /** False for a FailFast type that failed to load. */
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

    /** Typed load behind ResourceFormatEntry::Acquire. */
    template<CResource T>
    TUniquePtr<IResourceHold> AcquireHold(ResourceManager& InManager, const char* InAbsPath)
    {
        return MakeUnique<TResourceHold<T>>(InManager.Load<T>(InAbsPath));
    }

    using ResourceAcquireFn = TUniquePtr<IResourceHold> (*)(ResourceManager&, const char*);
}
