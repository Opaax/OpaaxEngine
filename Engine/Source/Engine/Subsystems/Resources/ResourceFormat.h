#pragma once

#include <concepts>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"

#include "ResourceConcept.hpp"

// =============================================================================
// ResourceFormat — what a resource type looks like ON DISK: its display name, and every
//   extension its loader claims.
//
//   MANY extensions to ONE type, by design: TextureResource is .png AND .jpg AND .tga — one
//   loader, one entry. That is why this table belongs to the engine and not to the editor,
//   whose registry is keyed per extension and would need one entry per spelling, each
//   repeating the same icon and the same double-click.
//
//   OPTIONAL, and deliberately not folded into CResource: BinaryResource is a
//   format-agnostic byte blob, and requiring the facet would force it to invent an extension.
// =============================================================================
namespace Opaax
{
    /**
     * One resource type's on-disk identity. A POD of pointers to string literals — no allocation,
     * no interning, usable as a `static constexpr` member.
     *
     * `const char*` and never `OpaaxStringID`: a static member holding an interned id would reach
     * the string pool during static initialization (**I2**, the hazard that killed the out-of-line
     * `String_None`). Interning happens at registration instead.
     */
    struct ResourceFormat
    {
        const char*        Label;            // "Opaax Level" — the format family's name
        const char* const* Extensions;       // { ".png", ".jpg", … } — raw text, normalized on registration
        Uint32             ExtensionCount;
    };

    /**
     * Declare the on-disk formats a resource type claims. Goes INSIDE the struct, beside
     * `FailPolicy` — this is a facet of the type, the way `OPAAX_PROPERTIES` sits inside a component.
     *
     * @param LABEL Display name for the whole family ("Texture"), shown when nothing overrides it.
     * @param ... One or more extensions, dotted or not.
     */
    #define OPAAX_RESOURCE_FORMAT(LABEL, ...)                                                \
        static constexpr const char*            Extensions[] = { __VA_ARGS__ };              \
        static constexpr ::Opaax::ResourceFormat Format{                                     \
            LABEL, Extensions,                                                               \
            static_cast<::Opaax::Uint32>(sizeof(Extensions) / sizeof(*Extensions)) };

    /**
     * A resource type that also declares its on-disk formats. `ResourceFormatRegistry::Register`
     * constrains on this, so a type missing `OPAAX_RESOURCE_FORMAT` fails at the registration line
     * rather than registering an entry nothing can ever match.
     */
    template<typename T>
    concept CResourceFormat = CResource<T> && requires
    {
        { T::Format } -> std::convertible_to<const ResourceFormat&>;
    };

    /**
     * Turn raw extension text into the id both sides of the lookup agree on: lower-cased (Windows
     * paths are case-insensitive, so ".PNG" and ".png" are one format) with a guaranteed leading dot.
     * The ONE place an extension becomes comparable — registration and file scanning both come here.
     *
     * Out-of-line so a game module's registration interns into the engine's pool (**I2**).
     *
     * @param InExtension Raw extension, with or without its dot ("wave", ".WAVE").
     * @return The normalized interned id, or an INVALID id when InExtension is empty.
     */
    OPAAX_API OpaaxStringID NormalizeExtension(OpaaxStringView InExtension);
}
