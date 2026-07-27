#include "Panels/SandboxPanel.h"

#include "Editor/EditorContext.h"

#include "Application/Services/ILogger.h"          // OPAAX_LOG + LogCategory
#include "World/WorldManager.h"
#include "World/Entity/Entity.h"
#include "World/Components/DummyComponent.h"

#include <imgui.h>

#include <string>

// OPAAX_LOG expands to an unqualified ToSpdLevel(...) — bring Opaax into scope, as the game module does.
using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxPanel{"SandboxPanel"};

    // Spawned quads tile in rows BELOW the module's three demo quads (which sit at y = 0), wrapping every
    // third one, so a handful of clicks stay inside the default viewport instead of piling up on one spot.
    constexpr float  SPAWN_ORIGIN_X = -200.f;
    constexpr float  SPAWN_ORIGIN_Y = -180.f;
    constexpr float  SPAWN_STEP_X   =  200.f;
    constexpr float  SPAWN_STEP_Y   = -140.f;
    constexpr Uint64 SPAWN_PER_ROW  =  3;
}

SandboxPanel::SandboxPanel(Opaax::Editor::EditorContext& InContext)
    : m_Context(InContext)
{
}

SandboxPanel::~SandboxPanel() = default;

void SandboxPanel::SpawnQuad()
{
    World* lWorld = m_Context.Worlds.GetActiveWorld();
    if (lWorld == nullptr)
    {
        return;
    }

    //TODO: Make OpaaxString Convertion with numeric value or Static convertor OpaaxString::FromInt etc...
    // Unique names so each spawn is its own distinguishable Hierarchy row.
    const std::string lName = "SpawnedQuad_" + std::to_string(m_SpawnCount);

    Entity          lEntity = lWorld->CreateEntity(OpaaxString(lName.c_str()));
    DummyComponent& lComp   = lEntity.Add<DummyComponent>();

    lComp.Position = { SPAWN_ORIGIN_X + SPAWN_STEP_X * static_cast<float>(m_SpawnCount % SPAWN_PER_ROW),
                       SPAWN_ORIGIN_Y + SPAWN_STEP_Y * static_cast<float>(m_SpawnCount / SPAWN_PER_ROW) };
    lComp.Size     = { 80.f, 80.f };
    lComp.Color    = { 1.f, 0.85f, 0.2f, 1.f };   // distinct from the module's red/green/blue

    ++m_SpawnCount;

    OPAAX_LOG(LogSandboxPanel, Info, "SandboxPanel: spawned quad #{} ('{}')", m_SpawnCount, lName.c_str())
}

void SandboxPanel::Draw()
{
    ImGui::SetNextWindowSize(ImVec2(260.f, 150.f), ImGuiCond_FirstUseEver);
    ImGui::Begin(m_Title.CStr());

    World* lWorld = m_Context.Worlds.GetActiveWorld();
    if (lWorld == nullptr)
    {
        ImGui::TextDisabled("No active world.");
        ImGui::End();
        return;
    }

    ImGui::Text("World:    %s", lWorld->GetName().CStr());
    ImGui::Text("Entities: %llu", static_cast<unsigned long long>(lWorld->GetEntityCount()));
    ImGui::Separator();

    if (ImGui::Button("Spawn Quad"))
    {
        SpawnQuad();
    }

    ImGui::End();
}
