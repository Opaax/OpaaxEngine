#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ESaveResult — how saving or loading a slot ended. One name for each failure, from the disk up to
    //   the game, so a game can tell "no save yet" from "a save it must not overwrite".
    // =============================================================================
    enum class ESaveResult : Uint8
    {
        Ok,

        /** No file for that slot: a first launch. */
        Missing,

        /** Not a save file: cut short, edited by hand, another format. */
        Corrupt,

        /** Written by a newer game or engine: refused, so an older one never overwrites it. */
        NewerVersion,

        /** Holds a class that is unknown, abstract, not a SaveGame, or not the one asked for. */
        WrongClass,

        /** The slot name is not a plain file name (ISaveStorage::IsValidSlotName). */
        InvalidSlot,

        /** The disk refused: no space, no rights, a locked file. */
        IoError
    };

    constexpr const char* ToString(const ESaveResult InResult) noexcept
    {
        switch (InResult)
        {
            case ESaveResult::Ok:           return "Ok";
            case ESaveResult::Missing:      return "Missing";
            case ESaveResult::Corrupt:      return "Corrupt";
            case ESaveResult::NewerVersion: return "NewerVersion";
            case ESaveResult::WrongClass:   return "WrongClass";
            case ESaveResult::InvalidSlot:  return "InvalidSlot";
            case ESaveResult::IoError:      return "IoError";
        }
        return "Unknown";
    }
}
