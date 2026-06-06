// Copyright (c) Wojciech Figat. All rights reserved.

#if COMPILE_WITH_TEXTURE_TOOL

#include "TextureTool.h"
#include "Engine/Core/Log.h"
#include "Engine/Core/Math/Color.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Graphics/PixelFormatExtensions.h"
#include "Engine/Graphics/RenderTools.h"
#include "Engine/Graphics/Textures/TextureData.h"
#include "Engine/Platform/File.h"
#include "Engine/Profiler/ProfilerCPU.h"

namespace
{
    uint32 ReadU32LE(const byte* data)
    {
        return (uint32)data[0] | ((uint32)data[1] << 8) | ((uint32)data[2] << 16) | ((uint32)data[3] << 24);
    }

    uint64 ReadU64LE_Ktx2(const byte* data)
    {
        return (uint64)data[0]
            | ((uint64)data[1] << 8)
            | ((uint64)data[2] << 16)
            | ((uint64)data[3] << 24)
            | ((uint64)data[4] << 32)
            | ((uint64)data[5] << 40)
            | ((uint64)data[6] << 48)
            | ((uint64)data[7] << 56);
    }

    static const uint32 VK_FORMAT_R8G8B8A8_UNORM = 37u;

    bool LoadKtx2UncompressedRgba8(const StringView& path, TextureData& textureData, String* errorMsg)
    {
        Array<byte> fileData;
        if (File::ReadAllBytes(path, fileData) || fileData.Count() < 64)
        {
            if (errorMsg)
                *errorMsg = TEXT("Failed to read KTX file.");
            return true;
        }

        static const byte KtxIdentifier[12] = { 0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, '\r', '\n', 0x1A, '\n' };
        if (Platform::MemoryCompare(fileData.Get(), KtxIdentifier, sizeof(KtxIdentifier)) != 0)
        {
            if (errorMsg)
                *errorMsg = TEXT("Invalid KTX 2.0 identifier.");
            return true;
        }

        const byte* header = fileData.Get() + 12;
        if (ReadU32LE(header) != 0x04030201u)
        {
            if (errorMsg)
                *errorMsg = TEXT("Unsupported KTX endianness.");
            return true;
        }

        const uint32 vkFormat = ReadU32LE(header + 12);
        const uint32 pixelWidth = ReadU32LE(header + 20);
        const uint32 pixelHeight = ReadU32LE(header + 24);
        const uint32 pixelDepth = ReadU32LE(header + 28);
        const uint32 layerCount = ReadU32LE(header + 32);
        const uint32 faceCount = ReadU32LE(header + 36);
        const uint32 mipLevels = Math::Max(1u, ReadU32LE(header + 40));
        const uint32 supercompressionScheme = ReadU32LE(header + 44);

        // GL_UNSIGNED_BYTE + GL_RGBA, 2D, no array/cube.
        if (vkFormat != VK_FORMAT_R8G8B8A8_UNORM || pixelDepth != 0 || layerCount != 0 || faceCount != 1)
        {
            if (errorMsg)
                *errorMsg = TEXT("Unsupported KTX 2.0 format. Only uncompressed 2D RGBA8 textures are supported.");
            return true;
        }
        if (supercompressionScheme != 0)
        {
            if (errorMsg)
                *errorMsg = TEXT("Unsupported KTX 2.0 supercompression. Only scheme 0 (none) is supported.");
            return true;
        }
        if (pixelWidth == 0 || pixelHeight == 0 || pixelWidth > GPU_MAX_TEXTURE_SIZE || pixelHeight > GPU_MAX_TEXTURE_SIZE)
        {
            if (errorMsg)
                *errorMsg = TEXT("Invalid KTX 2.0 texture dimensions.");
            return true;
        }
        if (mipLevels > GPU_MAX_TEXTURE_MIP_LEVELS)
        {
            if (errorMsg)
                *errorMsg = TEXT("Too many KTX 2.0 mip levels.");
            return true;
        }

        const int32 levelIndexOffset = 80;
        const int32 levelIndexSize = (int32)mipLevels * 24;
        if (levelIndexOffset + levelIndexSize > fileData.Count())
        {
            if (errorMsg)
                *errorMsg = TEXT("Truncated KTX 2.0 level index.");
            return true;
        }

        textureData.Width = (int32)pixelWidth;
        textureData.Height = (int32)pixelHeight;
        textureData.Depth = 1;
        textureData.Format = PixelFormat::R8G8B8A8_UNorm;
        textureData.Items.Resize(1);
        textureData.Items[0].Mips.Resize((int32)mipLevels);

        for (uint32 mip = 0; mip < mipLevels; mip++)
        {
            const byte* entry = fileData.Get() + levelIndexOffset + mip * 24;
            const uint64 byteOffset = ReadU64LE_Ktx2(entry);
            const uint64 byteLength = ReadU64LE_Ktx2(entry + 8);

            if ((int64)(byteOffset + byteLength) > (int64)fileData.Count())
            {
                if (errorMsg)
                    *errorMsg = TEXT("Truncated KTX 2.0 mip data.");
                return true;
            }

            const int32 mipWidth = Math::Max(1, (int32)pixelWidth >> (int32)mip);
            const int32 mipHeight = Math::Max(1, (int32)pixelHeight >> (int32)mip);

            uint32 rowPitch, slicePitch;
            RenderTools::ComputePitch(PixelFormat::R8G8B8A8_UNorm, mipWidth, mipHeight, rowPitch, slicePitch);

            auto& mipData = textureData.Items[0].Mips[mip];
            mipData.RowPitch = rowPitch;
            mipData.DepthPitch = slicePitch;
            mipData.Lines = (uint32)mipHeight;
            mipData.Data.Allocate(slicePitch);
            Platform::MemoryCopy(mipData.Data.Get(), fileData.Get() + byteOffset, (int32)byteLength);
        }

        return false;
    }

    bool DetectAlpha(const TextureData& textureData, bool& hasAlpha)
    {
        hasAlpha = false;
        if (textureData.Items.IsEmpty() || textureData.Items[0].Mips.IsEmpty())
            return true;

        const auto& mip = textureData.Items[0].Mips[0];
        const int32 width = textureData.Width;
        const int32 height = textureData.Height;
        for (int32 y = 0; y < height; y++)
        {
            const byte* row = mip.Data.Get() + y * mip.RowPitch;
            for (int32 x = 0; x < width; x++)
            {
                if (row[x * 4 + 3] != 255)
                {
                    hasAlpha = true;
                    return false;
                }
            }
        }
        return false;
    }
}

bool TextureTool::ImportTextureKtx2(const StringView& path, TextureData& textureData, bool& hasAlpha)
{
    PROFILE_CPU();
    String errorMsg;
    if (LoadKtx2UncompressedRgba8(path, textureData, &errorMsg))
    {
        LOG(Warning, "Failed to import KTX 2.0 texture from '{0}'. {1}", path, errorMsg);
        return true;
    }
    return DetectAlpha(textureData, hasAlpha);
}

bool TextureTool::ImportTextureKtx2(const StringView& path, TextureData& textureData, const Options& options, String& errorMsg, bool& hasAlpha)
{
    PROFILE_CPU();
    if (LoadKtx2UncompressedRgba8(path, textureData, &errorMsg))
        return true;

    if (textureData.Width > options.MaxSize || textureData.Height > options.MaxSize)
    {
        errorMsg = String::Format(TEXT("KTX 2.0 texture {0}x{1} exceeds MaxSize {2}."), textureData.Width, textureData.Height, options.MaxSize);
        return true;
    }

    if (options.FlipY || options.FlipX || options.InvertRedChannel || options.InvertGreenChannel || options.InvertBlueChannel || options.InvertAlphaChannel || options.ReconstructZChannel)
    {
        Function<void(Color&)> transform;
        if (options.FlipY || options.FlipX)
        {
            // Flips are not implemented for precomputed KTX mip chains.
            LOG(Warning, "KTX 2.0 import ignores FlipX/FlipY when mip chain is embedded.");
        }
        if (options.InvertRedChannel || options.InvertGreenChannel || options.InvertBlueChannel || options.InvertAlphaChannel || options.ReconstructZChannel)
        {
            const bool invR = options.InvertRedChannel;
            const bool invG = options.InvertGreenChannel;
            const bool invB = options.InvertBlueChannel;
            const bool invA = options.InvertAlphaChannel;
            const bool reconZ = options.ReconstructZChannel;
            transform = [invR, invG, invB, invA, reconZ](Color& c)
            {
                if (invR)
                    c.R = 1.0f - c.R;
                if (invG)
                    c.G = 1.0f - c.G;
                if (invB)
                    c.B = 1.0f - c.B;
                if (invA)
                    c.A = 1.0f - c.A;
                if (reconZ)
                {
                    const float x = c.R * 2.0f - 1.0f;
                    const float y = c.G * 2.0f - 1.0f;
                    c.B = Math::Sqrt(Math::Max(0.0f, 1.0f - x * x - y * y));
                }
            };
            if (Transform(textureData, transform))
            {
                errorMsg = TEXT("Failed to transform KTX 2.0 texture data.");
                return true;
            }
        }
    }

    if (options.Compress)
    {
        PixelFormat targetFormat = TextureTool::ToPixelFormat(options.Type, textureData.Width, textureData.Height, true);
        if (targetFormat == PixelFormat::Unknown)
        {
            errorMsg = TEXT("Unsupported KTX 2.0 texture compression format.");
            return true;
        }
        if (options.sRGB)
            targetFormat = PixelFormatExtensions::TosRGB(targetFormat);

        TextureData compressed;
        if (Convert(compressed, textureData, targetFormat))
        {
            errorMsg = TEXT("Failed to compress KTX 2.0 texture.");
            return true;
        }
        textureData = MoveTemp(compressed);
    }
    else if (options.sRGB)
    {
        textureData.Format = PixelFormatExtensions::TosRGB(textureData.Format);
    }

    return DetectAlpha(textureData, hasAlpha);
}

#endif
