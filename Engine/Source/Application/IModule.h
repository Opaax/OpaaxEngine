#pragma once

namespace Opaax
{
    // =============================================================================
    // IModule — the shared base for a game's host modules: the runtime module (IRuntimeModule, plugged
    //   into the engine registries, D9) and the editor module (IEditorModule, plugged into the editor's
    //   extension routes, D10). Both describe the SAME role — a game plugging content INTO a host — so they
    //   share one type + lifetime here. Registration itself does NOT live on the base: each side registers
    //   into a different registrar (ModuleRegistrar vs EditorExtensionRegistrar), so OnRegister stays on the
    //   derived interface with its own signature. Marker only for now; identity/lifecycle can be added if
    //   something ever holds an IModule* polymorphically.
    //
    // Header-only pure interface, no OPAAX_API: no exported symbols, no shared state, no cross-module
    // identity tag (unlike services/subsystems — I2). DLL-safe by construction.
    // =============================================================================
    class IModule
    {
    public:
        virtual ~IModule() = default;
    };
}
