#pragma once

#include "Application/Services/IAppService.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Save/SaveResult.h"

namespace Opaax
{
    class IPaths;

    inline constexpr LogCategory LogSaveStorage{"SaveStorage"};

    // =============================================================================
    // ISaveStorage — where a player's saves live: one file per slot, <Slot>.opaaxsave, in the save
    //   folder (<UserDir>/SaveGames, see IPaths::UserDir). Files and text only: what a slot holds is
    //   the game's (SaveGameFile). A write is whole or not at all (FileIO::WriteAllTextAtomic), so a
    //   crash keeps the last good save. Calls on different slots may run on any thread at once; one
    //   slot is used by one caller at a time.
    //   Virtual so a platform with a save API of its own (a console) can replace the files.
    // =============================================================================
    class ISaveStorage : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(ISaveStorage)

        /** The extension of a slot's file. */
        static constexpr const char* SLOT_EXTENSION = ".opaaxsave";

        /** The longest slot name. */
        static constexpr Uint32 MAX_SLOT_NAME_LENGTH = 64;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Whether InSlot can name a slot: 1 to 64 letters, digits, '_' or '-', and not a name Windows
         * keeps for a device (CON, NUL, COM1...). A slot name is a file name on every platform: a game
         * maps what a player types to a name of its own.
         */
        static bool IsValidSlotName(OpaaxStringView InSlot);

        /** The folder holding the slots. Empty when nothing is saved. */
        virtual OpaaxString GetSaveDirectory() const = 0;

        /** InSlot's file: <SaveDirectory>/<InSlot>.opaaxsave. Empty for an invalid name. */
        OpaaxString GetSlotPath(OpaaxStringView InSlot) const;

        /**
         * Reads InSlot's file.
         * @param OutText Receives the text. Untouched on failure.
         * @return Missing when the slot has no file; InvalidSlot; IoError when it cannot be read
         */
        virtual ESaveResult Read(OpaaxStringView InSlot, OpaaxString& OutText) const = 0;

        /**
         * Writes InText as InSlot's file, whole or not at all. Creates the save folder.
         * @return InvalidSlot; IoError when the disk refuses (the slot keeps its last file)
         */
        virtual ESaveResult Write(OpaaxStringView InSlot, const OpaaxString& InText) = 0;

        /** True when InSlot has a file. */
        virtual bool Exists(OpaaxStringView InSlot) const = 0;

        /**
         * Deletes InSlot's file, and what a crashed write left beside it.
         * @return Missing when it had none; InvalidSlot; IoError when it could not be deleted
         */
        virtual ESaveResult Delete(OpaaxStringView InSlot) = 0;

        /** The slots that have a file, sorted by name. Other files in the folder are left out. */
        virtual TDynArray<OpaaxString> ListSlots() const = 0;

        //----- null object ----------------------------------------------------
        /** A storage that keeps nothing: every write fails, every slot is missing. */
        static ISaveStorage& Null();
    };

    // =============================================================================
    // SaveStorage — the slots as files in one folder: <UserDir>/SaveGames in a game, any folder in a
    //   test or a tool.
    // =============================================================================
    class SaveStorage final : public ISaveStorage
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        /** Slots in <InPaths.UserDir()>/SaveGames. */
        explicit SaveStorage(const IPaths& InPaths);

        /** Slots in InSaveDirectory. */
        explicit SaveStorage(OpaaxString InSaveDirectory) : m_SaveDirectory(Move(InSaveDirectory)) {}

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ISaveStorage interface
    public:
        OpaaxString            GetSaveDirectory() const override { return m_SaveDirectory; }
        ESaveResult            Read(OpaaxStringView InSlot, OpaaxString& OutText) const override;
        ESaveResult            Write(OpaaxStringView InSlot, const OpaaxString& InText) override;
        bool                   Exists(OpaaxStringView InSlot) const override;
        ESaveResult            Delete(OpaaxStringView InSlot) override;
        TDynArray<OpaaxString> ListSlots() const override;
        //~End ISaveStorage interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString m_SaveDirectory;
    };
}
