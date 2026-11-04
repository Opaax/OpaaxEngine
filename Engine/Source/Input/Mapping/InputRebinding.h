#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Input/InputCodes.h"

namespace Opaax
{
    // =============================================================================
    // EKeyConflictPolicy — what a rebind does when another mapping of the same context is on the key.
    // =============================================================================
    enum class EKeyConflictPolicy : Uint8
    {
        /** Nothing changes; the result lists the mappings on the key, for the menu to ask. */
        Refuse,

        /** The other mapping takes the key the rebound one had. */
        Swap,

        /** The other mapping loses the key: it drives nothing until given another. */
        Replace
    };

    /** Enum to string. */
    inline const char* ToString(const EKeyConflictPolicy InPolicy) noexcept
    {
        switch (InPolicy)
        {
        case EKeyConflictPolicy::Refuse:  return "Refuse";
        case EKeyConflictPolicy::Swap:    return "Swap";
        case EKeyConflictPolicy::Replace: return "Replace";
        }
        return "Refuse";
    }

    // =============================================================================
    // ERebindResult — what a rebind did.
    // =============================================================================
    enum class ERebindResult : Uint8
    {
        /** The mapping has the key (it may have had it already). */
        Rebound,

        /** No known context has that mapping: its asset never added or registered, or no such action and key. */
        NoSuchMapping,

        /** The context is not the player's to change (InputMappingContextAsset::bRebindable off). */
        Locked,

        /** Not a keyboard key or mouse button (AnyKey, a gamepad code). */
        InvalidKey,

        /** Another mapping of the context is on the key, and the policy was Refuse. */
        Conflict,

        /** A locked context uses the key: it is never taken from it, whatever the policy. */
        Reserved,

        /** The game's key check refused it (InputRebindResult::Why says why). */
        RefusedByGame
    };

    /** Enum to string. */
    inline const char* ToString(const ERebindResult InResult) noexcept
    {
        switch (InResult)
        {
        case ERebindResult::Rebound:       return "Rebound";
        case ERebindResult::NoSuchMapping: return "NoSuchMapping";
        case ERebindResult::Locked:        return "Locked";
        case ERebindResult::InvalidKey:    return "InvalidKey";
        case ERebindResult::Conflict:      return "Conflict";
        case ERebindResult::Reserved:      return "Reserved";
        case ERebindResult::RefusedByGame: return "RefusedByGame";
        }
        return "NoSuchMapping";
    }
}

OPAAX_ENUM_VALUES(Opaax::EKeyConflictPolicy, Refuse, Swap, Replace)
OPAAX_ENUM_VALUES(Opaax::ERebindResult, Rebound, NoSuchMapping, Locked, InvalidKey, Conflict, Reserved, RefusedByGame)

namespace Opaax
{
    // =============================================================================
    // InputKeySlot — one mapping of a context asset, as a rebind names it: the context's asset, the
    //   action, and the key the asset gives it. The key the asset gives tells an action's keys apart
    //   (Move's W and S), and stays the same whatever the player binds.
    // =============================================================================
    struct InputKeySlot
    {
        /** The context asset's path ("Input/Controls.opaaxasset"). */
        OpaaxString Context;

        OpaaxStringID Action;

        /** The key in the asset. */
        EKeyCode DefaultKey = EKeyCode::None;

        bool operator==(const InputKeySlot& InOther) const noexcept
        {
            return Action == InOther.Action && DefaultKey == InOther.DefaultKey && Context == InOther.Context;
        }
    };

    /** A mapping and the key the player has on it: what a controls menu lists. */
    struct InputKeyMapping
    {
        InputKeySlot Slot;
        EKeyCode     Key = EKeyCode::None;

        /** True when the player changed it. */
        bool IsRebound() const noexcept { return Key != Slot.DefaultKey; }
    };

    /** A context's mappings as its asset lists them, and whether the player may change them. */
    struct InputContextLayout
    {
        /** The context asset's path. */
        OpaaxString Context;

        bool bRebindable = true;

        /** In the asset's order; each slot's Context is this one's. */
        TDynArray<InputKeySlot> Slots;
    };

    /** What a rebind did. */
    struct InputRebindResult
    {
        ERebindResult Result = ERebindResult::Rebound;

        /**
         * Conflict and Reserved: the mappings on the key, as they are. Swap and Replace: the other
         * mappings, with the key each was given.
         */
        TDynArray<InputKeyMapping> Others;

        /** RefusedByGame: the game's reason, for the menu to show. */
        OpaaxString Why;

        bool IsOk() const noexcept { return Result == ERebindResult::Rebound; }
    };

    /**
     * The game's say on a key for a mapping (a key it keeps for itself, a key an action must not have):
     * false refuses it, with OutWhy for the menu. EKeyCode::None is a mapping cleared.
     */
    using FInputKeyCheck = TFunction<bool(const InputKeySlot& InSlot, EKeyCode InKey, OpaaxString& OutWhy)>;

    // =============================================================================
    // InputRebinds — the player's keys over the contexts' own, by slot. Pure: told the layouts of the
    //   contexts it may check against, it rebinds, finds conflicts and reads and writes them as the
    //   settings' Keys section keeps them. A rebind of a context not known yet is kept: it applies once
    //   the context is.
    // =============================================================================
    class InputRebinds
    {
        // =============================================================================
        // Keys
        // =============================================================================
    public:
        /** The key InSlot has: the player's, else the asset's. */
        EKeyCode GetKey(const InputKeySlot& InSlot) const noexcept;

        /**
         * Gives InSlot the key InKey (None clears it). Checked against InLayouts: the slot must be in a
         * rebindable one; another slot of its context on the key is a conflict, handled by InPolicy; a
         * locked context's slot on it reserves it. InCheck, when set, has the last say on every key it
         * would give, the one swapped included. Nothing changes unless the result is Rebound.
         */
        InputRebindResult Rebind(const TDynArray<InputContextLayout>& InLayouts, const InputKeySlot& InSlot,
                                 EKeyCode InKey, EKeyConflictPolicy InPolicy, const FInputKeyCheck& InCheck = {});

        /** InSlot gets the asset's key again. */
        void Reset(const InputKeySlot& InSlot);

        /** Every slot of InContext gets the asset's key again. */
        void ResetContext(const OpaaxString& InContext);

        /** Every slot gets the asset's key again. */
        void ResetAll() noexcept { m_Rebinds.clear(); }

        bool   IsEmpty() const noexcept { return m_Rebinds.empty(); }
        Uint64 Num()     const noexcept { return static_cast<Uint64>(m_Rebinds.size()); }

        /** True when a slot of InContext is rebound. */
        bool HasRebinds(const OpaaxString& InContext) const noexcept;

        /**
         * Drops the rebinds of InLayout's context that it does not allow: every one of a locked context,
         * and those of a slot it no longer has (the project changed the asset's keys).
         * @return How many were dropped, each said in OutDropped when given
         */
        Uint64 DropUnknown(const InputContextLayout& InLayout, TDynArray<OpaaxString>* OutDropped = nullptr);

        // =============================================================================
        // As the settings keep them
        // =============================================================================
    public:
        /**
         * The rebinds by context, action and the asset's key: {"Input/Controls.opaaxasset": {"Move":
         * {"W": "Up", "S": "None"}}}. Null when there are none.
         */
        nlohmann::json ToJson() const;

        /**
         * Replaces the rebinds with InJson's (as ToJson writes them); an entry that is not one (no such
         * key name, not a string) is dropped, each said in OutDropped when given.
         */
        void FromJson(const nlohmann::json& InJson, TDynArray<OpaaxString>* OutDropped = nullptr);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct Entry
        {
            InputKeySlot Slot;
            EKeyCode     Key = EKeyCode::None;
        };

        /** The rebind of InSlot, or null. */
        const Entry* Find(const InputKeySlot& InSlot) const noexcept;

        /** Gives InSlot InKey; the asset's key takes the rebind away. */
        void Set(const InputKeySlot& InSlot, EKeyCode InKey);

        TDynArray<Entry> m_Rebinds;
    };
}
