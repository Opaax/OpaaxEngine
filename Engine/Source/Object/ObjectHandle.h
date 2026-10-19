#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ObjectHandle — an object's slot in the ObjectTable and the generation of that slot. A slot is
    //   reused once its object is gone, with the next generation, so a kept handle never names a
    //   newer object: it resolves to null instead. Generation 0 is no object.
    // =============================================================================
    struct ObjectHandle
    {
        Uint32 Index      = 0;
        Uint32 Generation = 0;

        bool IsValid() const noexcept { return Generation != 0; }

        bool operator==(const ObjectHandle& InOther) const noexcept
        {
            return Index == InOther.Index && Generation == InOther.Generation;
        }
        bool operator!=(const ObjectHandle& InOther) const noexcept { return !(*this == InOther); }
    };

    // =============================================================================
    // EObjectFlags — what the object table knows about an object besides its address (bitmask).
    // =============================================================================
    enum class EObjectFlags : Uint32
    {
        None         = 0,

        /** Made by NewObject: it lives on the heap and its last TStrongObjectPtr destroys it. */
        Managed      = BIT(0),

        /** Its class's default object: the defaults of every field, never destroyed. */
        ClassDefault = BIT(1),

        /** Its destruction has begun: handles to it already resolve to null. */
        Destroying   = BIT(2),
    };

    constexpr EObjectFlags operator|(const EObjectFlags InA, const EObjectFlags InB) noexcept
    {
        return static_cast<EObjectFlags>(static_cast<Uint32>(InA) | static_cast<Uint32>(InB));
    }

    constexpr EObjectFlags operator&(const EObjectFlags InA, const EObjectFlags InB) noexcept
    {
        return static_cast<EObjectFlags>(static_cast<Uint32>(InA) & static_cast<Uint32>(InB));
    }

    constexpr EObjectFlags operator~(const EObjectFlags InValue) noexcept
    {
        return static_cast<EObjectFlags>(~static_cast<Uint32>(InValue));
    }

    /** True if InValue has any of the flags in InFlags. */
    constexpr bool HasFlag(const EObjectFlags InValue, const EObjectFlags InFlags) noexcept
    {
        return (static_cast<Uint32>(InValue) & static_cast<Uint32>(InFlags)) != 0;
    }

    /**
     * A field that references an object by handle (TObjectPtr), recognised by its marker so that the
     * reflection headers need not include the pointer types.
     */
    template<typename T>
    concept CObjectPtrField = requires(T& InValue)
    {
        typename T::ObjectType;
        requires T::k_IsObjectPtr;
        { InValue.GetMutableHandle() } -> CSameAs<ObjectHandle&>;
    };
}
