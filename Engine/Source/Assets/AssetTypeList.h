// AssetTypeList.h
//
// Single source of truth for built-in asset types. Add a new type by
// inserting one OPAAX_ASSET_TYPE(Name) line below — the consumer
// (IAsset.hpp) re-includes this file with different macro definitions
// to expand the list into both the AssetType enum body and the parallel
// g_AssetTypeNames string array.
//
// Do NOT add #pragma once — this file is intentionally re-included.

OPAAX_ASSET_TYPE(Texture2D)
OPAAX_ASSET_TYPE(Shader)
OPAAX_ASSET_TYPE(Scene)
OPAAX_ASSET_TYPE(Font)
