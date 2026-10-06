#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyData.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyFile.h"

namespace Opaax
{
    // =============================================================================
    // FontFamilyResource — a .opaaxfont as a resource.
    //   Placeholder policy: an empty family, so text draws as boxes.
    //   Its faces are loaded by RendererManager (needs IPaths).
    // =============================================================================
    struct FontFamilyResource final
    {
        FontFamilyData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Font Family", FontFamilyFile::FAMILY_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<FontFamilyResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            FontFamilyResource lResource;
            if (!FontFamilyFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // FontFamilyFile already logged why
            }

            return lResource;
        }

        /** An empty family: every style misses, so text draws as boxes. */
        static FontFamilyResource Placeholder() { return FontFamilyResource{}; }

        /** Size of the entries. */
        Uint64 ByteSize() const noexcept
        {
            Uint64 lBytes = sizeof(FontFamilyResource);

            for (const FontFamilyEntry& lEntry : Data.Entries)
            {
                lBytes += sizeof(FontFamilyEntry) + lEntry.Face.Path.GetLength();
            }

            return lBytes;
        }
    };
}
