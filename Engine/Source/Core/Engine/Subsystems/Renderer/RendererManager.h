#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// ResourceManager — routing. The first engine subsystem, a thin core service.
//
//   Owns one ResourcePool<T> per type (lazy-created, indexed by ResourceTypeID),
//   plus the dependency graph. Public surface is FROZEN (review C5):
//       Load / Resolve / Pin / FlushAll / Update
//   Every future capability (hot reload, cooking, streaming, editor type info) is
//   a separate system CONSUMING this API — never a manager feature.
//
//   This header is the umbrella: the crossing template bodies (ResourceRef<T>,
//   LoadContext::Acquire, ResourceManager::Load...) are defined at the bottom,
//   where the manager, the pools, the refs and the context are all complete —
//   which is what breaks the Ref/Context <-> Manager template dependency cycle.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogRendererManager{"RendererManager"};
    
    class OPAAX_API RendererManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(RendererManager)
        
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        RendererManager() = default;
        /***/
        ~RendererManager() override;
    };
}