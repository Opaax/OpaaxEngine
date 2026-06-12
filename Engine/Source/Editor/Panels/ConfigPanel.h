#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Editor/IEditorPanel.h"

namespace Opaax::Editor
{
    /**
     * @class ConfigPanel
     * Engine configuration view (engine.config.json). Splits fields by whether the running
     * engine can act on a change without a restart:
     *   - Editable section — values re-read live each frame (today: render interpolation).
     *     Edits take effect immediately; "Save to disk" persists them back to the config file.
     *   - Read-only section — startup/build-time values (backends, window, log, asset paths,
     *     physics world bounds) shown as info, since the engine only consumes them at boot.
     * Reads/writes the static EngineConfig directly, so it takes no external draw params.
     */
    class OPAAX_API ConfigPanel final : public IEditorPanel
    {
    public:
        ConfigPanel()           = default;
        ~ConfigPanel() override = default;

        //~Begin IEditorPanel interface
    public:
        void        Draw()               override;
        const char* GetPanelName() const override { return "Config"; }
        //~End IEditorPanel interface
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
