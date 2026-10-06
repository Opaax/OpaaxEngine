#pragma once

#include <concepts>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"

#include "ResourceConcept.hpp"

// =============================================================================
// ResourceFormat — a resource type's display name and file extensions.
//   One type can have several extensions (Texture: .png, .jpg, .tga).
//   Optional (BinaryResource has none).
// =============================================================================
namespace Opaax
{
    /**
     * A resource type's on-disk identity. Plain pointers to string literals, usable as static constexpr.
     * Not OpaaxStringID: interning during static init is unsafe. Interned at registration instead.
     */
    struct ResourceFormat
    {
        const char*        Label;            // "Opaax Level"
        const char* const* Extensions;       // { ".png", ".jpg", ... }, normalized on registration
        Uint32             ExtensionCount;
    };

    /**
     * Declares the file formats of a resource type. Put it inside the struct.
     * @param LABEL Display name ("Texture")
     * @param ... One or more extensions, with or without dot
     */
    #define OPAAX_RESOURCE_FORMAT(LABEL, ...)                                                \
        static constexpr const char*            Extensions[] = { __VA_ARGS__ };              \
        static constexpr ::Opaax::ResourceFormat Format{                                     \
            LABEL, Extensions,                                                               \
            static_cast<::Opaax::Uint32>(sizeof(Extensions) / sizeof(*Extensions)) };

    /**
     * A resource type with OPAAX_RESOURCE_FORMAT. Required by ResourceFormatRegistry::Register.
     */
    template<typename T>
    concept CResourceFormat = CResource<T> && requires
    {
        { T::Format } -> std::convertible_to<const ResourceFormat&>;
    };

    /**
     * Normalizes an extension: lower case, with a leading dot, interned.
     * Used for both registration and file scanning.
     * @param InExtension Raw extension ("wave", ".WAVE")
     * @return The interned id, or an invalid id if InExtension is empty
     */
    OpaaxStringID NormalizeExtension(OpaaxStringView InExtension);
}
