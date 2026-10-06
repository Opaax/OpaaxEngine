#pragma once

#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Renderer/Text/FontFaceData.h"
#include "RHI/Texture.h"

// =============================================================================
// FontFaceResource — one .ttf as a resource: a baked glyph atlas and its metrics.
//   Load (any thread) reads the file and bakes the atlas; Initialize (main thread) uploads it.
//   One file = one face; choosing between faces is FontFamilyResource's job.
//   Placeholder policy: an empty face, so every character draws as a box.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogFontFaceResource{"FontFaceResource"};

    struct FontFaceResource final
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
        // The TUniquePtr member is move-only.
        FontFaceResource(const FontFaceResource&)            = delete;
        FontFaceResource& operator=(const FontFaceResource&) = delete;
        FontFaceResource(FontFaceResource&&)                 = default;
        FontFaceResource& operator=(FontFaceResource&&)      = default;

        // =========================================================================
        // CResource contract
        // =========================================================================
    public:
        /** Font file extensions. */
        OPAAX_RESOURCE_FORMAT("Font Face", ".ttf", ".otf")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /**
         * Any thread: reads the file and bakes the atlas. No GPU.
         * @param InPath Absolute UTF-8 path
         * @return The face, or nullopt if the file is unreadable or not a font
         */
        static std::optional<FontFaceResource> Load(const char* InPath, LoadContext& InCtx);

        /**
         * Main thread, once: uploads the atlas (R8) and frees the CPU copy. Safe to call twice.
         */
        void Initialize();

        /** An empty face with plausible metrics, so boxes still lay out. */
        static FontFaceResource Placeholder();

        /**
         * Atlas bytes plus tables. Same value before and after Initialize.
         */
        Uint64 ByteSize() const noexcept;

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The atlas texture, or null before Initialize (or without a device). */
        ITexture2D* GetAtlas() const noexcept { return Gpu.get(); }

        bool IsUploaded() const noexcept { return Gpu != nullptr; }

        // =========================================================================
        // Members
        // =========================================================================
    public:
        FontFaceData           Face;      // glyph table, metrics, kerning
        TDynArray<Uint8>       Pixels;    // R8 coverage; empty after Initialize
        TUniquePtr<ITexture2D> Gpu;
    };
}
