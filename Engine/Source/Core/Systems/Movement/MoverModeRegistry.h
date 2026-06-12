#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"

namespace Opaax
{
    class IMoverMode;

    // =============================================================================
    // MoverModeRegistry
    // =============================================================================
    /**
     * @class MoverModeRegistry
     *
     * Process-global catalog of mover modes, keyed by OpaaxStringID — the single source of
     * truth both the MoverSubsystem (runtime dispatch) and the editor (the inspector's mode
     * combo) read. Static by design, mirroring ComponentRegistry: mode *definitions* are global
     * behaviour, not per-world state, and the editor has no path to a subsystem instance.
     *
     * Modes are stateless (all per-entity state lives on the MoverComponent), so one instance
     * serves every entity. New movement = Register a new IMoverMode by id; nothing else changes.
     */
    class OPAAX_API MoverModeRegistry
    {
    public:
        // Register (or replace) the mode run for entities whose MoverComponent.ModeId == InId.
        static void Register(OpaaxStringID InId, UniquePtr<IMoverMode> InMode);

        // The mode for InId, or nullptr if none is registered.
        static IMoverMode* Find(OpaaxStringID InId);

        // Every registered mode id — for the editor mode picker.
        static TDynArray<OpaaxStringID> GetModeIds();

        // Drop all modes (called from MoverSubsystem::Shutdown for deterministic teardown).
        static void Clear();

    private:
        // Keyed by OpaaxStringID::GetId() (interned mode name). Function-local static so the
        // map is constructed on first use and not subject to static-init ordering.
        static UnorderedMap<Uint32, UniquePtr<IMoverMode>>& Modes();
    };

} // namespace Opaax
