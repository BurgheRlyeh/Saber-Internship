/**
 * @file DDSTexture.h
 * @brief Texture resource loaded from a DDS file on disk.
 */
#pragma once

#include "Headers.h"

#include "TextureResource.h"

class CommandList;
class DeviceContext;

/**
 * @brief A @ref TextureResource populated by loading a DDS image from disk.
 *
 * The DDS file is decoded and uploaded to a default-heap GPU texture during
 * construction; an intermediate upload buffer is created and released via
 * the device context's intermediate-resource tracking.
 */
class DDSTexture : public TextureResource {
protected:
    using TextureResource::TextureResource;

public:
    /**
     * @brief Loads and uploads a DDS texture in a single step.
     * @param filename          Path to the @c .dds file.
     * @param pDeviceContext    Device context supplying the D3D12 device and intermediate tracking.
     * @param pCommandListDirect Direct command list used to issue copy commands.
     */
    DDSTexture(
        const std::wstring& filename,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandListDirect
    );

    /**
     * @brief Loads (or reloads) the texture data from the specified DDS file.
     *
     * May be used to hot-reload a texture without reconstructing the object.
     *
     * @param filename          Path to the @c .dds file.
     * @param pDeviceContext    Device context for device and intermediate access.
     * @param pCommandListDirect Direct command list for copy commands.
     */
    void LoadFromDDS(
        const std::wstring& filename,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandListDirect
    );
};
