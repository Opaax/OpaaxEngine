#pragma once

// Helpers shared by the audio suites: generated WAV files, an asset folder in the temp folder, and a
// job system that runs only when told (a load stays "loading" for as long as a test wants).

#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"
#include "Audio/AudioBank.h"
#include "Audio/AudioClipDef.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/AssetFile.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxString.hpp"

namespace AudioTest
{
    /** A mono 16-bit PCM WAV file: a sine at InHz, InSeconds long, peaking at InAmplitude. */
    inline Opaax::TDynArray<Opaax::Uint8> MakeWav(const Opaax::Uint32 InSampleRate, const float InSeconds,
                                                  const float InHz = 440.f, const float InAmplitude = 0.5f)
    {
        using namespace Opaax;

        const Uint32 lFrames    = static_cast<Uint32>(static_cast<float>(InSampleRate) * InSeconds);
        const Uint32 lDataBytes = lFrames * 2;

        TDynArray<Uint8> lOut;
        const auto lPut32  = [&lOut](const Uint32 InValue) { for (int i = 0; i < 4; ++i) { lOut.push_back(static_cast<Uint8>(InValue >> (8 * i))); } };
        const auto lPut16  = [&lOut](const Uint16 InValue) { lOut.push_back(static_cast<Uint8>(InValue & 0xFF)); lOut.push_back(static_cast<Uint8>(InValue >> 8)); };
        const auto lPutTag = [&lOut](const char* InTag) { lOut.insert(lOut.end(), InTag, InTag + 4); };

        lPutTag("RIFF"); lPut32(36 + lDataBytes); lPutTag("WAVE");
        lPutTag("fmt "); lPut32(16); lPut16(1); lPut16(1); lPut32(InSampleRate); lPut32(InSampleRate * 2); lPut16(2); lPut16(16);
        lPutTag("data"); lPut32(lDataBytes);

        for (Uint32 lFrame = 0; lFrame < lFrames; ++lFrame)
        {
            const double lPhase  = 2.0 * 3.14159265358979 * InHz * lFrame / InSampleRate;
            const auto   lSample = static_cast<Int16>(InAmplitude * 32767.0 * Maths::Sin(lPhase));
            lPut16(static_cast<Uint16>(lSample));
        }

        return lOut;
    }

    /** Writes InBytes at InPath, its folders created. */
    inline void WriteFile(const Opaax::OpaaxString& InPath, const Opaax::TDynArray<Opaax::Uint8>& InBytes)
    {
        Opaax::FileIO::WriteAllBytes(InPath, InBytes.data(), InBytes.size());
    }

    /** Every asset under one root, so AssetToAbsolute is a concatenation. */
    class TempPaths final : public Opaax::IPaths
    {
    public:
        explicit TempPaths(const Opaax::OpaaxString& InRoot) : m_Root(InRoot) {}

        Opaax::OpaaxString AssetToAbsolute(const Opaax::OpaaxString& InAssetRel) const override
        {
            return Opaax::PathString::Join(m_Root, InAssetRel);
        }

        Opaax::OpaaxString AbsoluteToAsset(const Opaax::OpaaxString& InAbsPath) const override
        {
            return Opaax::PathString::RelativeTo(InAbsPath, m_Root);
        }

        void               LogPaths()      const override {}
        Opaax::OpaaxString WorkspaceRoot() const override { return Root(); }
        Opaax::OpaaxString EngineRoot()    const override { return Root(); }
        Opaax::OpaaxString ProjectRoot()   const override { return Root(); }
        Opaax::OpaaxString ProjectFile()   const override { return Root(); }
        Opaax::OpaaxString AssetsDir()     const override { return Root(); }
        Opaax::OpaaxString ConfigsDir()    const override { return Root(); }
        Opaax::OpaaxString SourceDir()     const override { return Root(); }
        Opaax::OpaaxString SaveDir()       const override { return Root(); }
        Opaax::OpaaxString TempDir()       const override { return Root(); }

        Opaax::OpaaxString EngineToAbsolute(const Opaax::OpaaxString& InRel)  const override { return AssetToAbsolute(InRel); }
        Opaax::OpaaxString ProjectToAbsolute(const Opaax::OpaaxString& InRel) const override { return AssetToAbsolute(InRel); }

    private:
        Opaax::OpaaxString Root() const { return m_Root; }

        Opaax::OpaaxString m_Root;
    };

    /** Runs nothing until RunAll: what it was given stays queued. */
    class ManualJobs final : public Opaax::IJobSystem
    {
    public:
        Opaax::JobHandle Submit(Opaax::TFunction<void()> InWork) override { return Submit(Opaax::Move(InWork), {}); }

        Opaax::JobHandle Submit(Opaax::TFunction<void()> InWork, Opaax::TFunction<void()> InOnComplete) override
        {
            m_Jobs.emplace_back(Opaax::Move(InWork), Opaax::Move(InOnComplete));
            return Opaax::JobHandle{};
        }

        void ParallelFor(const Opaax::Uint32 InCount, const Opaax::TFunction<void(Opaax::Uint32)>& InBody, Opaax::Uint32) override
        {
            for (Opaax::Uint32 lIndex = 0; lIndex < InCount; ++lIndex) { InBody(lIndex); }
        }

        void          Wait(const Opaax::JobHandle&) override {}
        void          DrainCompletions() override {}
        Opaax::Uint32 GetWorkerCount() const noexcept override { return 0; }

        /** Runs every queued job, then its completion. */
        void RunAll()
        {
            auto lJobs = Opaax::Move(m_Jobs);
            m_Jobs.clear();
            for (auto& [lWork, lDone] : lJobs)
            {
                if (lWork) { lWork(); }
                if (lDone) { lDone(); }
            }
        }

        bool HasJobs() const noexcept { return !m_Jobs.empty(); }

    private:
        Opaax::TDynArray<Opaax::TPair<Opaax::TFunction<void()>, Opaax::TFunction<void()>>> m_Jobs;
    };

    /** The audio asset classes, as the engine registers them. */
    struct AudioClassRegistry final : Opaax::ObjectClassRegistry
    {
        AudioClassRegistry()
        {
            Register<Opaax::AudioClipDef>();
            Register<Opaax::AudioBank>();
        }
    };

    /** What a resource manager reads the audio suites' asset files with. */
    inline const Opaax::ObjectClassRegistry& AudioClasses()
    {
        static const AudioClassRegistry s_Classes;
        return s_Classes;
    }

    /** Writes InAsset at InPath, its folders created. */
    inline bool SaveAsset(const Opaax::OpaaxString& InPath, const Opaax::Asset& InAsset)
    {
        Opaax::FileSystem::CreateDirectories(Opaax::PathString::Parent(InPath));
        return Opaax::AssetFile::Save(InPath, InAsset);
    }
}
