#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectPtr.h"
#include "Save/SaveGame.h"
#include "Save/SaveResult.h"

namespace Opaax
{
    class ObjectClassRegistry;

    // =============================================================================
    // SaveGameFile — the text of a save file, one per slot:
    //
    //   { "Class": "BhopSave", "ClassVersion": 2, "Properties": { ...fields by name... }, "Version": 1 }
    //
    //   "Version" is the file format's, as in an asset file; "ClassVersion" is the save class's own
    //   (SaveGame::GetSaveVersion). Text only, no disk: writing a file whole is the storage's job.
    // =============================================================================
    namespace SaveGameFile
    {
        /** The format this engine writes. A file from a newer one is refused. */
        inline constexpr Int32 FORMAT_VERSION = 1;

        /** A file's contents, before a class reads them. */
        struct Contents
        {
            OpaaxStringID  Class;
            nlohmann::json Properties   = nlohmann::json::object();
            Int32          ClassVersion = 1;
        };

        /** InSave's class, versions and properties, keys sorted and indented: a person can read it. */
        OpaaxString Serialize(const SaveGame& InSave);

        /**
         * Reads a file's text into OutContents (untouched on failure). A file written by hand may leave
         * out the versions (taken as 1) and the properties (all keep their defaults).
         * @param OutError Why it failed (optional)
         * @return Corrupt when InText is not an object with a "Class" name; NewerVersion when its format
         *   is newer than FORMAT_VERSION
         */
        ESaveResult Parse(const OpaaxString& InText, Contents& OutContents, OpaaxString* OutError = nullptr);

        /**
         * A new save of the class InContents names, its properties upgraded (SaveGame::UpgradeProperties)
         * then read. A value of the wrong type keeps its default and is named in OutBadFields: the rest of
         * a player's progress still loads.
         * @param InExpected The class asked for: the file's must be it or derive from it
         * @param InName The save's name (its slot)
         * @return WrongClass when the class is unknown, abstract, not a SaveGame or not InExpected;
         *   NewerVersion when the file's ClassVersion is above the class's
         */
        ESaveResult Instantiate(const Contents& InContents, const ObjectClassRegistry& InClasses,
                                const ObjectClass& InExpected, OpaaxStringID InName,
                                TStrongObjectPtr<SaveGame>& OutSave, OpaaxString* OutError = nullptr,
                                TDynArray<OpaaxString>* OutBadFields = nullptr);

        /** Parse, then Instantiate as a T. OutSave is untouched on failure. */
        template<typename T>
        requires CDerivedFrom<T, SaveGame>
        ESaveResult Read(const OpaaxString& InText, const ObjectClassRegistry& InClasses, const OpaaxStringID InName,
                         TStrongObjectPtr<T>& OutSave, OpaaxString* OutError = nullptr,
                         TDynArray<OpaaxString>* OutBadFields = nullptr)
        {
            Contents          lContents;
            const ESaveResult lParsed = Parse(InText, lContents, OutError);
            if (lParsed != ESaveResult::Ok)
            {
                return lParsed;
            }

            TStrongObjectPtr<SaveGame> lSave;
            const ESaveResult lMade = Instantiate(lContents, InClasses, T::StaticClass(), InName, lSave,
                                                  OutError, OutBadFields);
            if (lMade == ESaveResult::Ok)
            {
                OutSave = Cast<T>(lSave);
            }
            return lMade;
        }
    }
}
