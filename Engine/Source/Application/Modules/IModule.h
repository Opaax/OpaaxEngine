#pragma once

namespace Opaax
{
    // =============================================================================
    // IModule — base for a game module plugged into a host
    //   (IRuntimeModule for the engine, IEditorModule for the editor).
    //   Each derived interface declares its own OnRegister.
    // =============================================================================
    class IModule
    {
    public:
        virtual ~IModule() = default;
    };
}
