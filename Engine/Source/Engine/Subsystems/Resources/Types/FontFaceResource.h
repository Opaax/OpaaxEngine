#pragma once

#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Renderer/Text/FontFaceData.h"
#include "RHI/Texture.h"

// =============================================================================
// FontFaceResource — one `.ttf` as a RESOURCE: a baked glyph atlas plus the metrics that lay it out.
//
//   TextureResource's shape exactly, and for its exact reason — the CResource two-phase split. Load
//   runs on ANY thread and does file IO plus the rasterisation, which is the expensive part and the
//   only part that can be made parallel; Initialize runs on the MAIN thread at the pump and uploads
//   the single-channel atlas.
//
//   A FACE, not a font. This type is one file: one subset, one weight, one slant. Choosing between
//   162 of them is FontFamilyResource's job, and keeping the two apart is what lets a game reference
//   a single `.ttf` with no family asset beside it.
//
//   Placeholder policy: an EMPTY face. It has no glyphs, so every character in every string draws a
//   tofu box — the typographic convention for "this font cannot show that", and loud enough that
//   nobody ships it by accident. The magenta-texture rule, applied to text.
//
//   NOT under Renderer/: that module is portable and may not reach host services (it reaches the
//   device through IEngine, the route TextureResource already uses). The pure DATA half does live
//   there — FontFaceData — which is why the layout walker needs nothing from this layer.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogFontFaceResource{"FontFaceResource"};

    struct OPAAX_API FontFaceResource final
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        FontFaceResource()  = default;
        ~FontFaceResource() = default;

        // =========================================================================
        // Copy delete - Move
        // =========================================================================
        //
        // OPAAX_API instantiates every implicitly-declared member (I6 corollary), and the owned
        // TUniquePtr<ITexture2D> is move-only — so these are REQUIRED to compile, not hygiene.
        FontFaceResource(const FontFaceResource&)            = delete;
        FontFaceResource& operator=(const FontFaceResource&) = delete;
        FontFaceResource(FontFaceResource&&)                 = default;
        FontFaceResource& operator=(FontFaceResource&&)      = default;

        // =========================================================================
        // CResource contract
        // =========================================================================
    public:
        /** The two spellings a desktop font ships as. The only place the engine names either. */
        OPAAX_RESOURCE_FORMAT("Font Face", ".ttf", ".otf")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /**
         * ANY THREAD: read the file and bake its atlas. No GPU call, no service, no shared state.
         *
         * @param InPath Absolute UTF-8 path.
         * @return The baked face, or nullopt when the file is unreadable or is not a font.
         */
        static std::optional<FontFaceResource> Load(const char* InPath, LoadContext& InCtx);

        /**
         * MAIN THREAD, once, at the pump: upload the coverage atlas as R8 and release the CPU copy.
         * Idempotent, and a no-op with no pixels — so a second pass, or a face published with no
         * device, is safe.
         */
        void Initialize();

        /** An empty face with plausible vertical metrics, so a string of tofu still lays out. */
        static FontFaceResource Placeholder();

        /**
         * Atlas bytes plus the tables. Stated the same before and after Initialize frees the CPU
         * copy, so the pool's accounting does not depend on WHEN it asks.
         */
        Uint64 ByteSize() const noexcept;

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The coverage atlas, or null until Initialize has run (and forever, with no device). */
        ITexture2D* GetAtlas() const noexcept { return Gpu.get(); }

        bool IsUploaded() const noexcept { return Gpu != nullptr; }

        // =========================================================================
        // Members — public, like every other resource payload in the tree.
        // =========================================================================
    public:
        FontFaceData           Face;      // glyph table, metrics, kerning
        TDynArray<Uint8>       Pixels;    // R8 coverage; EMPTY after Initialize
        TUniquePtr<ITexture2D> Gpu;
    };
}
