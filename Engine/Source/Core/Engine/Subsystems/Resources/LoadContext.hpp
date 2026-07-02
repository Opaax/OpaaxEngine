#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"

#include "ResourceConcept.hpp"
#include "ResourceRef.hpp"
#include "ResourceDependencyGraph.hpp"

// =============================================================================
// LoadContext — composite loading (Level -> Textures), refcounts chain.
//
//   A loader receives LoadContext& and Acquire()s its hard dependencies through
//   it: refs chain (releasing the parent releases them), and every Acquire records
//   a forward+reverse edge in the dependency graph. The context carries the
//   in-flight load chain, so a path already loading is a HARD CYCLE — Acquire
//   fails it loudly. The hard-reference graph is therefore a DAG by construction.
//
//   Templated Acquire<TSub> is declared here and DEFINED in ResourceManager.h
//   (needs the complete manager) — same cycle-break as ResourceRef.
// =============================================================================
namespace Opaax
{
    class ResourceManager;

    class LoadContext final
    {
        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        LoadContext(ResourceManager& InManager, ResourceDependencyGraph& InDeps) noexcept
            : m_Manager(InManager)
            , m_Deps(InDeps)
        {
        }

        LoadContext(const LoadContext&)            = delete;
        LoadContext& operator=(const LoadContext&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Acquire a hard sub-dependency. Recursion into the manager; the returned Ref
        // keeps it alive as long as the parent payload holds it. A path already in the
        // in-flight chain returns an INVALID ref (cycle) — the parent's Load then fails.
        template<CResource TSub>
        ResourceRef<TSub> Acquire(const char* InPath); // defined in ResourceManager.h

        // ----- in-flight chain protocol (used by ResourceManager::LoadInternal) -----
        // Push a path id entering its load; false if it is already loading (a cycle).
        bool PushLoading(Uint32 InId)
        {
            if (IsLoading(InId)) { return false; }
            m_Chain.push_back(InId);
            return true;
        }
        void PopLoading() noexcept { if (!m_Chain.empty()) { m_Chain.pop_back(); } }

        bool IsLoading(Uint32 InId) const noexcept
        {
            for (const Uint32 lId : m_Chain) { if (lId == InId) { return true; } }
            return false;
        }

        // The resource whose Load is currently running (edge parent) — None at root.
        Uint32 CurrentParent() const noexcept
        {
            return m_Chain.empty() ? OpaaxGlobal::ID_None : m_Chain.back();
        }

        /*----------------------------- Get -------------------------------*/
        ResourceManager&         GetManager() const noexcept { return m_Manager; }
        ResourceDependencyGraph& GetDeps()    const noexcept { return m_Deps;    }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ResourceManager&         m_Manager;
        ResourceDependencyGraph& m_Deps;
        TDynArray<Uint32>        m_Chain; // interned path ids currently in-flight
    };
}
