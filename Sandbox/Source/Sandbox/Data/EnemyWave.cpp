#include "Data/EnemyWave.h"

#include "Core/IO/FileIO.h"
#include "Core/Log/Logger.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Sandbox
{
    OPAAX_LOG_CATEGORY(EnemyWave);

    bool EnemyWave::LoadSource(const char* InAbsPath)
    {
        const Opaax::OpaaxString lText = Opaax::FileIO::ReadAllText(Opaax::OpaaxString(InAbsPath));
        const nlohmann::json     lJson = nlohmann::json::parse(lText.CStr(), nullptr, /*allow_exceptions*/ false);

        const auto lEnemies = lJson.is_object() ? lJson.find("enemies") : lJson.end();
        if (!lJson.is_object() || lEnemies == lJson.end() || !lEnemies->is_array())
        {
            OPAAX_LOG(LogEnemyWave, Error, "'{}' is not a wave: a JSON object with an \"enemies\" list", InAbsPath);
            return false;
        }

        WaveName = lJson.value("wave", Opaax::OpaaxString());

        Groups.clear();
        for (const nlohmann::json& lEnemy : *lEnemies)
        {
            if (!lEnemy.is_object())
            {
                continue;   // not a group: skipped
            }

            WaveGroup lGroup;
            lGroup.Type    = lEnemy.value("type", Opaax::OpaaxString());
            lGroup.Count   = lEnemy.value("count", 0);
            lGroup.Pattern = lEnemy.value("pattern", Opaax::OpaaxString());
            Groups.emplace_back(Opaax::Move(lGroup));
        }

        return true;
    }

    Opaax::Uint64 EnemyWave::GetResourceSize() const
    {
        Opaax::Uint64 lBytes = sizeof(EnemyWave) + WaveName.GetLength();

        for (const WaveGroup& lGroup : Groups)
        {
            lBytes += sizeof(WaveGroup) + lGroup.Type.GetLength() + lGroup.Pattern.GetLength();
        }

        return lBytes;
    }

    OPAAX_REGISTER_OBJECT(EnemyWave);
}
