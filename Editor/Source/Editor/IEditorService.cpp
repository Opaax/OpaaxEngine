#include "Editor/IEditorService.h"

namespace Opaax::Editor
{
    namespace
    {
        // NullEditorService — locator fallback so Get<IEditorService>() never dereferences null. Inert:
        // an editor executable always provides the real one, so this is only the template's safety net.
        class NullEditorService final : public IEditorService
        {
        public:
            bool IsNull() const noexcept override { return true; }
            void Initialize()        override {}
            void BeginFrame()        override {}
            void EndFrame()          override {}
            bool RouteInput(Event&)  override { return false; }   // inert: consumes nothing
        };
    }

    // Out-of-line type tag — one instance across the lib, DLL-safe (mirrors OPAAX_SERVICE_TYPE's rule).
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
