#include "Editor/Application/Services/IEditorService.h"

namespace Opaax::Editor
{
    namespace
    {
        // NullEditorService — used when no editor service is provided. Does nothing.
        class NullEditorService final : public IEditorService
        {
        public:
            bool IsNull() const noexcept override { return true; }
            void Initialize()        override {}
            void BeginFrame()        override {}
            void EndFrame()          override {}
            bool RouteInput(Event&)  override { return false; }   // consumes nothing
            void RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>&) override {}
            void StopPlay()          override {}
            void RegisterAutomation(AutomationRunner&, bool) override {}
        };
    }

    // Type tag, defined here (shared across the lib).
    ServiceTypeID IEditorService::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IEditorService& IEditorService::Null()
    {
        static NullEditorService s_Null;
        return s_Null;
    }
}
