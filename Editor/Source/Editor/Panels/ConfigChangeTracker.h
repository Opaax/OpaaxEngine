#pragma once

#include "Core/Config/IConfig.h"            // ConfigTypeID
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ConfigChangeTracker — WHEN the Config panel calls IConfig::NotifyChanged.
    //
    //   Once per COMMITTED edit, never per frame of a gesture: while a widget is still mid-edit (a
    //   drag, a colour pick, typing) the change is held, and it is announced on the first frame
    //   nothing is being edited. A drag that ends where it started announces nothing.
    //
    //   Derived, like the panel's dirty flag: this frame's text against the text last announced.
    //   Header-only and ImGui-free — the panel passes "is a widget active" in as a bool.
    // =============================================================================
    class ConfigChangeTracker
    {
    public:
        /**
         * Feed one frame of the shown config.
         *
         * @param InId        which config is shown. A different one re-baselines SILENTLY —
         *                    selecting a config is not editing it.
         * @param InText      what it serializes to now (IConfig::ToText).
         * @param bInEditing  a widget is still mid-gesture.
         * @return true on the frame the edit is committed — the caller notifies.
         */
        bool Update(const ConfigTypeID InId, const OpaaxString& InText, const bool bInEditing)
        {
            if (InId != m_Id)
            {
                m_Id        = InId;
                m_Announced = InText;
                return false;
            }

            if (bInEditing || InText == m_Announced)
            {
                return false;
            }

            m_Announced = InText;
            return true;
        }

    private:
        ConfigTypeID m_Id = 0;
        OpaaxString  m_Announced;
    };
}
