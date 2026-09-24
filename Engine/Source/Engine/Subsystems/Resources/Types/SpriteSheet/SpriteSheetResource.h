#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetData.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetFile.h"

namespace Opaax
{
    // =============================================================================
    // SpriteSheetResource — a `.opaaxsheet` as a RESOURCE. MapResource's shape exactly: a plain
    //   struct satisfying CResource whose whole body is an adapter, so the file format stays in
    //   SpriteSheetFile and going through the ResourceManager buys dedup and a lifetime nobody
    //   hand-manages.
    //
    //   PLACEHOLDER, not FailFast — ResourceConcept's own deciding question is whether a degraded
    //   substitute keeps things CORRECT, and a sheet describes a RENDERABLE: a missing one draws
    //   the magenta texture exactly as a missing texture does, which is loud and survivable. (A
    //   map is FailFast because an empty map does not degrade, it lies.)
    //
    //   IT DOES NOT Acquire ITS TEXTURE, and that is a placement fact rather than an omission:
    //   LoadContext::Acquire takes an ABSOLUTE path, and asset-relative -> absolute lives in
    //   IPaths, an app service the Resources layer does not reach (RendererManager is the one
    //   adapter allowed to). So the sheet stays pure data and the renderer resolves both, through
    //   the texture cache it already owns for exactly that lifetime job. The day LoadContext
    //   carries a path resolver, maps and sheets adopt Acquire together — MapResource.hpp's own
    //   comment has been waiting for the same thing.
    //
    //   No Initialize(): nothing here touches the GPU, so the payload is complete when Load returns.
    // =============================================================================
    struct SpriteSheetResource final
    {
        SpriteSheetData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Sprite Sheet", SpriteSheetFile::SHEET_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<SpriteSheetResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            SpriteSheetResource lResource;
            if (!SpriteSheetFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // SpriteSheetFile already logged which reason it was
            }

            return lResource;
        }

        /**
         * An empty sheet naming no texture. What a sprite pointed at a missing `.opaaxsheet` gets:
         * no frames, so the renderer falls through to the whole-texture UVs and draws the texture
         * placeholder — magenta, loud, and impossible to ship by accident.
         */
        static SpriteSheetResource Placeholder() { return SpriteSheetResource{}; }

        /** Structural size — the frame records, which is all a sheet holds. */
        Uint64 ByteSize() const noexcept
        {
            return sizeof(SpriteSheetResource)
                 + static_cast<Uint64>(Data.Frames.size()) * sizeof(SpriteFrame)
                 + Data.Texture.Path.GetLength();
        }
    };
}
