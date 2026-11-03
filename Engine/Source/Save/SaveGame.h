#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Object/ObjectMacros.h"

namespace Opaax
{
    // =============================================================================
    // SaveGame — what a game keeps for a player between launches (best times, unlocked levels): an
    //   Object class saved by its OPAAX_PROPERTIES, one file per slot (SaveGameFile). A game derives
    //   from it, gives every field a default (a file written before the field existed still loads) and
    //   registers it with one line:
    //
    //   class BhopSave final : public Opaax::SaveGame
    //   {
    //       OPAAX_OBJECT(BhopSave, Opaax::SaveGame)
    //
    //   public:
    //       Opaax::Int32 Coins = 0;
    //       OPAAX_PROPERTIES(BhopSave, OPAAX_PROP(Coins))
    //   };
    //   OPAAX_REGISTER_OBJECT(BhopSave);
    //
    //   A change the defaults cannot absorb (a renamed field, a value in other units) raises
    //   GetSaveVersion and rewrites the older files' properties in UpgradeProperties.
    // =============================================================================
    class SaveGame : public Object
    {
        OPAAX_ABSTRACT_OBJECT(SaveGame, Object)

        // =========================================================================
        // Versions
        // =========================================================================
    public:
        /**
         * The version of the class's fields, written in each file. A file of a higher one is refused, so
         * an older game never overwrites what a newer one saved.
         */
        virtual Int32 GetSaveVersion() const { return 1; }

        /**
         * Rewrites the properties of a file written at InFileVersion, lower than GetSaveVersion(), into
         * the current fields before they are read: rename a key, convert a value. A key left missing
         * keeps its default. May run on a loading thread: touch nothing but InOutProperties.
         */
        virtual void UpgradeProperties(Int32 /*InFileVersion*/, nlohmann::json& /*InOutProperties*/) const {}
    };
}
