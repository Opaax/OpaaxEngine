#include "Core/Image/PngWriter.h"

#include <algorithm>

#include "Core/IO/FileIO.h"

namespace Opaax::PngWriter
{
    namespace
    {
        /** The largest deflate stored block. */
        constexpr Uint64 STORED_BLOCK_MAX = 65535;

        Uint32 Crc32(const Uint8* InData, const Uint64 InSize, Uint32 InCrc = 0)
        {
            static const TFixedArray<Uint32, 256> s_Table = []()
            {
                TFixedArray<Uint32, 256> lTable{};
                for (Uint32 lByte = 0; lByte < 256; ++lByte)
                {
                    Uint32 lValue = lByte;
                    for (int lBit = 0; lBit < 8; ++lBit)
                    {
                        lValue = (lValue & 1u) != 0 ? 0xEDB88320u ^ (lValue >> 1) : lValue >> 1;
                    }
                    lTable[lByte] = lValue;
                }
                return lTable;
            }();

            Uint32 lCrc = ~InCrc;
            for (Uint64 lIndex = 0; lIndex < InSize; ++lIndex)
            {
                lCrc = s_Table[(lCrc ^ InData[lIndex]) & 0xFFu] ^ (lCrc >> 8);
            }
            return ~lCrc;
        }

        void PutBigEndian32(TDynArray<Uint8>& OutBytes, const Uint32 InValue)
        {
            OutBytes.push_back(static_cast<Uint8>(InValue >> 24));
            OutBytes.push_back(static_cast<Uint8>(InValue >> 16));
            OutBytes.push_back(static_cast<Uint8>(InValue >> 8));
            OutBytes.push_back(static_cast<Uint8>(InValue));
        }

        /** Length, type, data, then the CRC of type and data. */
        void PutChunk(TDynArray<Uint8>& OutBytes, const char* InType, const TDynArray<Uint8>& InData)
        {
            PutBigEndian32(OutBytes, static_cast<Uint32>(InData.size()));

            const size_t lTypeAt = OutBytes.size();
            OutBytes.insert(OutBytes.end(), InType, InType + 4);
            OutBytes.insert(OutBytes.end(), InData.begin(), InData.end());

            PutBigEndian32(OutBytes, Crc32(OutBytes.data() + lTypeAt, 4 + InData.size()));
        }
    }

    TDynArray<Uint8> Encode(const Uint32 InWidth, const Uint32 InHeight, const Uint32 InChannels, const Uint8* InPixels)
    {
        if (InWidth == 0 || InHeight == 0 || InPixels == nullptr || (InChannels != 3 && InChannels != 4))
        {
            return {};
        }

        // The image data: each row starts with its filter type (0, none).
        const Uint64 lRowBytes = static_cast<Uint64>(InWidth) * InChannels;
        TDynArray<Uint8> lRaw;
        lRaw.reserve(static_cast<size_t>((lRowBytes + 1) * InHeight));
        for (Uint32 lRow = 0; lRow < InHeight; ++lRow)
        {
            lRaw.push_back(0);
            const Uint8* lRowStart = InPixels + lRow * lRowBytes;
            lRaw.insert(lRaw.end(), lRowStart, lRowStart + lRowBytes);
        }

        // A zlib stream of stored deflate blocks.
        TDynArray<Uint8> lZlib;
        lZlib.reserve(lRaw.size() + lRaw.size() / STORED_BLOCK_MAX * 5 + 16);
        lZlib.push_back(0x78);
        lZlib.push_back(0x01);

        Uint64 lOffset = 0;
        do
        {
            const Uint64 lLength = std::min<Uint64>(lRaw.size() - lOffset, STORED_BLOCK_MAX);
            const bool   bLast   = (lOffset + lLength == lRaw.size());

            lZlib.push_back(bLast ? 1 : 0);
            lZlib.push_back(static_cast<Uint8>(lLength & 0xFF));
            lZlib.push_back(static_cast<Uint8>(lLength >> 8));
            lZlib.push_back(static_cast<Uint8>(~lLength & 0xFF));
            lZlib.push_back(static_cast<Uint8>((~lLength >> 8) & 0xFF));
            lZlib.insert(lZlib.end(), lRaw.begin() + static_cast<std::ptrdiff_t>(lOffset),
                         lRaw.begin() + static_cast<std::ptrdiff_t>(lOffset + lLength));

            lOffset += lLength;
        }
        while (lOffset < lRaw.size());

        Uint32 lA = 1;
        Uint32 lB = 0;
        for (const Uint8 lByte : lRaw)
        {
            lA = (lA + lByte) % 65521u;
            lB = (lB + lA) % 65521u;
        }
        PutBigEndian32(lZlib, (lB << 16) | lA);

        // The file.
        TDynArray<Uint8> lPng = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };

        TDynArray<Uint8> lHeader;
        PutBigEndian32(lHeader, InWidth);
        PutBigEndian32(lHeader, InHeight);
        lHeader.push_back(8);                          // bits per channel
        lHeader.push_back(InChannels == 4 ? 6 : 2);    // RGBA or RGB
        lHeader.push_back(0);                          // deflate
        lHeader.push_back(0);                          // filter method 0 (a filter type per row)
        lHeader.push_back(0);                          // not interlaced

        PutChunk(lPng, "IHDR", lHeader);
        PutChunk(lPng, "IDAT", lZlib);
        PutChunk(lPng, "IEND", {});
        return lPng;
    }

    bool Write(const OpaaxString& InAbsPath, const Uint32 InWidth, const Uint32 InHeight, const Uint32 InChannels,
               const Uint8* InPixels)
    {
        const TDynArray<Uint8> lPng = Encode(InWidth, InHeight, InChannels, InPixels);
        return !lPng.empty() && FileIO::WriteAllBytes(InAbsPath, lPng.data(), static_cast<Uint64>(lPng.size()));
    }
}
