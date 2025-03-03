//No Pragma once since we want to use it to implement our enums.
#ifndef ASSET_ENUM_TYPE_H
#define ASSET_ENUM_TYPE_H

// ----------------------------------------------------------------------- //
//
// MODULE  : AssetEnumType.h
//
// PURPOSE : Enums and string constants for assets.
//
// CREATED : 02/27/2025
// ----------------------------------------------------------------------- //

//
// The following macros allow the enum entries to be included as the 
// body of an enum, or the body of a const char* string list.
//


#ifdef ADD_ACTION_TYPE
    #undef ADD_ACTION_TYPE
#endif

#ifdef ACTION_TYPE_AS_ENUM
    #define ADD_ACTION_TYPE(label) EAsset_##label,
#elif ACTION_TYPE_AS_STRING
    #define ADD_ACTION_TYPE(label) #label,
#else
    #define ADD_ACTION_TYPE(label)  // No action by default
#endif

ADD_ACTION_TYPE(Font)
ADD_ACTION_TYPE(Texture)
ADD_ACTION_TYPE(SpriteSheet)

#endif // ASSET_ENUM_TYPE_H
#undef ASSET_ENUM_TYPE_H