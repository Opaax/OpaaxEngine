#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyData.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyFile.h"

namespace Opaax
{
    // =============================================================================
    // FontFamilyResource — a `.opaaxfont` as a RESOURCE. SpriteSheetResource's shape exactly: a
    //   plain struct satisfying CResource whose whole body is an adapter, so the file format stays
    //   in FontFamilyFile and going through the ResourceManager buys dedup and a lifetime nobody
    //   hand-manages.
    //
    //   PLACEHOLDER, not FailFast — the deciding question is whether a degraded substitute keeps
    //   things CORRECT, and a family describes a RENDERABLE: an empty one resolves no face, and a
    //   text draw with no face is a row of tofu boxes. Loud and survivable, exactly as a missing
    //   texture draws magenta.
    //
    //   IT DOES NOT Acquire ITS FACES, and that is the same placement fact SpriteSheetResource
    //   states about its texture: LoadContext::Acquire takes an ABSOLUTE path, and
    //   asset-relative -> absolute lives in IPaths, an app service the Resources layer does not
    //   reach. So the family stays pure data and RendererManager resolves both, through the cache it
    //   already owns for exactly that lifetime job.
    //
    //   No Initialize(): nothing here touches the GPU, so the payload is complete when Load returns.
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
                return std::nullopt;   // FontFamilyFile already logged which reason it was
            }

            return lResource;
        }

        /** An empty family naming no face. Every style request misses, so the text draws as tofu. */
        static FontFamilyResource Placeholder() { return FontFamilyResource{}; }

        /** Structural size — the entry records, which is all a family holds. */
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
