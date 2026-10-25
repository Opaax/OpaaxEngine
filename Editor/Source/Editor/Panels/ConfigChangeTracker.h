#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ConfigChangeTracker — decides when the Config panel calls Config::NotifyChanged: once per
    //   committed edit (after a drag, pick or typing ends), never every frame. A drag ending where it
    //   started announces nothing. Compares this frame's text with the last announced text.
    // =============================================================================
    class ConfigChangeTracker
    {
    public:
        /**
         * Feeds one frame of the shown config.
         * @param InConfig    The shown config's name. A different one resets the baseline silently.
         * @param InText      Its current text (Config::ToText)
         * @param bInEditing  A widget is still being edited
         * @return True on the frame the edit is committed (the caller notifies)
         */
        bool Update(const OpaaxStringID InConfig, const OpaaxString& InText, const bool bInEditing)
        {
            if (InConfig != m_Config)
            {
                m_Config    = InConfig;
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
        OpaaxStringID m_Config;
        OpaaxString   m_Announced;
    };
}
