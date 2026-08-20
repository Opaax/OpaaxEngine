#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

// =============================================================================
// TResourcePath<T> — a reference to a resource FILE, carrying the type that loads it.
//
//   The path could have been a bare OpaaxString; the type parameter is the whole point. It is what
//   lets the editor's drop target REFUSE a `.wave` dragged onto a texture field, and what makes the
//   next resource-referencing field — a sound, a font, a wave — cost one line and no editor code:
//   one constrained TPropertyDrawer specialization serves every T (I15, the enum dropdown's shape).
//
//   T is only ever NAMED, never completed: ResourceTypeID::Get<T>() hashes a compiler-generated
//   signature, so a component declaring TResourcePath<TextureResource> needs one forward
//   declaration instead of the whole RHI. Nothing in this header includes the resource system.
//
//   ASSET-RELATIVE, always ("Textures/Hero.png"), which is what IPaths::AbsoluteToAsset produces
//   and AssetToAbsolute consumes (MP8). An absolute path here would bake a build machine's layout
//   into a map file.
//
//   Header-only value template: no OPAAX_API (I6), no state, no registry.
// =============================================================================
namespace Opaax
{
    template<typename TResource>
    struct TResourcePath
    {
        using ResourceType = TResource;

        OpaaxString Path;

        /** Empty is a REAL state — "no texture yet" — and never an error. */
        bool IsEmpty() const noexcept { return Path.IsEmpty(); }

        bool operator==(const TResourcePath& InOther) const noexcept { return Path == InOther.Path; }
        bool operator!=(const TResourcePath& InOther) const noexcept { return !(*this == InOther); }
    };
}
