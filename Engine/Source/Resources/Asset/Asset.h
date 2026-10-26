#pragma once

#include "Core/OpaaxTypes.h"
#include "Object/ObjectMacros.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// An asset type is a class with fields; the file, the loading, the sharing and the editor come
// with the base class:
//
//   class EnemyStats final : public Opaax::Asset
//   {
//       OPAAX_OBJECT(EnemyStats, Opaax::Asset)
//
//   public:
//       float       Speed     = 120.f;
//       Opaax::Int32 MaxHealth = 30;
//
//       OPAAX_PROPERTIES(EnemyStats, OPAAX_PROP(Speed).SetRange(0.f, 1000.f), OPAAX_PROP(MaxHealth))
//   };
//   OPAAX_REGISTER_OBJECT(EnemyStats);
//
// Saved as an .opaaxasset file (AssetFile.h); a field names one with TAssetRef<EnemyStats> and
// LoadAsset gives a TAssetHandle that reads it (AssetHandle.h); the editor opens it in a window of
// its own on double-click.
//
// A class can be read from files of its own instead (a texture from a .png): it lists their
// extensions with OPAAX_ASSET_SOURCE and reads one in LoadSource. Such an asset is never saved.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

// =============================================================================
// Declares the files an asset class is read from, instead of .opaaxasset files. Put it after
// OPAAX_OBJECT; the class then implements Asset::LoadSource.
//
//   class Texture2D final : public Opaax::Asset
//   {
//       OPAAX_OBJECT(Texture2D, Opaax::Asset)
//       OPAAX_ASSET_SOURCE(".png", ".jpg")
//       ...
//   };
// =============================================================================
#define OPAAX_ASSET_SOURCE(...)                                                                     \
public:                                                                                             \
    using SourceOwnerType = ThisClass;                                                              \
    static constexpr const char* SourceExtensions[] = { __VA_ARGS__ };

namespace Opaax
{
    // =============================================================================
    // Asset — the base of every type a file holds (Godot's Resource): an .opaaxasset, or a file of the
    //   class's own format (OPAAX_ASSET_SOURCE). Abstract: a file always holds one of its subclasses.
    // =============================================================================
    class Asset : public Object
    {
        OPAAX_ABSTRACT_OBJECT(Asset, Object)

        // =============================================================================
        // Loading
        // =============================================================================
    public:
        /**
         * Reads the asset from InAbsPath, a file of one of its class's OPAAX_ASSET_SOURCE extensions. May
         * run on a loading thread: fill the object, nothing else (no GPU, no shared state).
         * @return False when the file cannot be read: the load fails (log why)
         */
        virtual bool LoadSource(const char* /*InAbsPath*/) { return false; }

        /**
         * On the main thread, once the asset is read (loaded or reloaded): what needs the main thread,
         * like handing pixels to the GPU.
         */
        virtual void InitializeOnMainThread() {}

        /** The memory the asset holds, for the resource statistics. The class's size by default. */
        virtual Uint64 GetResourceSize() const { return GetClass().GetSize(); }
    };
}
