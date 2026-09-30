#pragma once

#include "Core/Config/IConfig.h"            // ConfigTypeID
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ConfigChangeTracker — decides when the Config panel calls IConfig::NotifyChanged: once per
    //   committed edit (after a drag, pick or typing ends), never every frame. A drag ending where it
    //   started announces nothing. Compares this frame's text with the last announced text.
    // =============================================================================
    class ConfigChangeTracker
    {
    public:
        /**
         * Feeds one frame of the shown config.
         * @param InId        The shown config. A different one resets the baseline silently.
         * @param InText      Its current text (IConfig::ToText)
         * @param bInEditing  A widget is still being edited
         * @return True on the frame the edit is committed (the caller notifies)
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
