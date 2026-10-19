#pragma once

#include "Core/OpaaxTypes.h"

#include "Resources/ResourceFormat.h"
#include "Renderer/Textures/SpriteSheetData.h"
#include "Renderer/Textures/SpriteSheetFile.h"

namespace Opaax
{
    // =============================================================================
    // SpriteSheetResource — a .opaaxsheet as a resource.
    //   Placeholder policy: an empty sheet (the sprite draws the magenta texture).
    //   Its texture is loaded by RendererManager (needs IPaths).
    // =============================================================================
    struct SpriteSheetResource final
    {
        SpriteSheetData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Sprite Sheet", SpriteSheetFile::SHEET_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static TOptional<SpriteSheetResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            SpriteSheetResource lResource;
            if (!SpriteSheetFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return NullOpt;   // SpriteSheetFile already logged why
            }

            return lResource;
        }

        /**
         * An empty sheet with no texture: the sprite draws the magenta placeholder.
         */
        static SpriteSheetResource Placeholder() { return SpriteSheetResource{}; }

        /** Size of the frame records. */
        Uint64 ByteSize() const noexcept
        {
            return sizeof(SpriteSheetResource)
                 + static_cast<Uint64>(Data.Frames.size()) * sizeof(SpriteFrame)
                 + Data.Texture.Path.GetLength();
        }
    };
}
