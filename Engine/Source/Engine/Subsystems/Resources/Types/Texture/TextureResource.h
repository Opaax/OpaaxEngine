#pragma once

#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "RHI/Texture.h"

// =============================================================================
// TextureResource — an image file as a RESOURCE, and the first GPU-backed one.
//
//   The two-phase split in CResource exists for exactly this type: Load runs on ANY thread and
//   does file IO + CPU decode only, Initialize runs on the MAIN thread at the pump and uploads.
//   Nothing else in the engine may open a GL context on a worker, so the decode is the only part
//   that can be made parallel — and it is the expensive part.
//
//   It reaches the device through IEngine (the route ViewportPanel already uses for its
//   framebuffer), never through a device pointer of its own: the pool's Initialize hook takes no
//   arguments, and a resource that cached an IRHIDevice* would outlive it on a device reset.
//
//   Placeholder policy: a missing texture degrades to a visible MAGENTA square. That is the
//   ResourceConcept rule applied — a renderable substitute keeps the frame correct, and magenta
//   is loud enough that nobody ships it by accident.
//
//   NOT under Renderer/: that module is portable and may not reach host services (RendererManager
//   is the one adapter allowed to). A resource is Gregory's Resources layer, which is here.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogTextureResource{"TextureResource"};

    struct OPAAX_API TextureResource final
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
        //
        // OPAAX_API instantiates every implicitly-declared member (I6 corollary), and the owned
        // TUniquePtr<ITexture2D> is move-only — so these declarations are REQUIRED to compile, not
        // hygiene. Move is what the pool needs (emplace(Move(*loaded))).
        TextureResource(const TextureResource&)            = delete;
        TextureResource& operator=(const TextureResource&) = delete;
        TextureResource(TextureResource&&)                 = default;
        TextureResource& operator=(TextureResource&&)      = default;

        // =========================================================================
        // CResource contract
        // =========================================================================
    public:
        /**
         * Every extension one decoder claims. The whole point of the format table being MANY
         * extensions to ONE type — and the only place in the engine that names an image extension.
         */
        OPAAX_RESOURCE_FORMAT("Texture", ".png", ".jpg", ".jpeg", ".tga", ".bmp")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        /**
         * ANY THREAD: read the file and decode it to pixels. No GPU call, no service, no shared
         * state — the requirement the concept states and the reason this is the half that can run
         * on a worker.
         *
         * @param InPath Absolute UTF-8 path.
         * @return The decoded image, or nullopt when the file is unreadable or not an image.
         */
        static std::optional<TextureResource> Load(const char* InPath, LoadContext& InCtx);

        /**
         * MAIN THREAD, once, at the pump: upload the pixels and release them. Idempotent, and a
         * no-op with no pixels — so a second pass, or a payload published with no device, is safe.
         */
        void Initialize();

        /** A 2x2 magenta square. Uploaded like any other texture: the pool initialises it too. */
        static TextureResource Placeholder();

        /**
         * Decoded (and therefore uploaded) image bytes. Stated the same before and after
         * Initialize frees the CPU copy, so the pool's accounting does not depend on WHEN it asks.
         */
        Uint64 ByteSize() const noexcept;

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The GPU texture, or null until Initialize has run (and forever, with no device). */
        ITexture2D* GetTexture() const noexcept { return Gpu.get(); }

        bool IsUploaded() const noexcept { return Gpu != nullptr; }

        // =========================================================================
        // Members — public, like every other resource payload in the tree.
        // =========================================================================
    public:
        TDynArray<Uint8>       Pixels;          // CPU decode; EMPTY after Initialize
        Uint32                 Width    = 0;
        Uint32                 Height   = 0;
        Int32                  Channels = 0;    // 4 = RGBA8, 3 = RGB8, 1 = R8 coverage
        TUniquePtr<ITexture2D> Gpu;
    };
}
