#pragma once

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
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // Asset — the base of every type saved in an .opaaxasset file (Godot's Resource). Abstract: a
    //   file always holds one of its subclasses.
    // =============================================================================
    class Asset : public Object
    {
        OPAAX_ABSTRACT_OBJECT(Asset, Object)
    };
}
