#pragma once

#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "RHI/Texture.h"

// =============================================================================
// TextureResource — an image file as a resource.
//   Load (any thread) reads and decodes; Initialize (main thread) uploads through IEngine.
//   Placeholder policy: a magenta square.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogTextureResource{"TextureResource"};

    struct TextureResource final
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        TextureResource()  = default;
        ~TextureResource() = default;

        // =========================================================================
        // Copy delete - Move
        // =========================================================================
        // The TUniquePtr member is move-only.
        TextureResource(const TextureResource&)            = delete;
        TextureResource& operator=(const TextureResource&) = delete;
        TextureResource(TextureResource&&)                 = default;
        TextureResource& operator=(TextureResource&&)      = default;

        // =========================================================================
        // CResource contract
        // =========================================================================
    public:
        /**
         * Image file extensions.
         */
        OPAAX_RESOURCE_FORMAT("Texture", ".png", ".jpg", ".jpeg", ".tga", ".bmp")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /**
         * Any thread: reads the file and decodes it. No GPU.
         * @param InPath Absolute UTF-8 path
         * @return The image, or nullopt if the file is unreadable or not an image
         */
        static std::optional<TextureResource> Load(const char* InPath, LoadContext& InCtx);

        /**
         * Main thread, once: uploads the pixels and frees them. Safe to call twice.
         */
        void Initialize();

        /** A 2x2 magenta square. */
        static TextureResource Placeholder();

        /**
         * Image bytes. Same value before and after Initialize.
         */
        Uint64 ByteSize() const noexcept;

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The GPU texture, or null before Initialize (or without a device). */
        ITexture2D* GetTexture() const noexcept { return Gpu.get(); }

        bool IsUploaded() const noexcept { return Gpu != nullptr; }

        // =========================================================================
        // Members
        // =========================================================================
    public:
        TDynArray<Uint8>       Pixels;          // CPU pixels; empty after Initialize
        Uint32                 Width    = 0;
        Uint32                 Height   = 0;
        Int32                  Channels = 0;    // 4 = RGBA8, 3 = RGB8, 1 = R8
        TUniquePtr<ITexture2D> Gpu;
    };
}
