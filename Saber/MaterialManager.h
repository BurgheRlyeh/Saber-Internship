/**
 * @file MaterialManager.h
 * @brief Manages texture and material registration for the deferred G-buffer pass.
 *
 * @ref TextureManager maintains a flat array of GPU textures with shared SRV
 * descriptors; @ref MaterialManager builds pairs of (albedo, normal) textures
 * into material slots and exposes a CBV for the packed material array.
 */
#pragma once

#include "Headers.h"

#include <unordered_map>
#include <mutex>

#include "MaterialCB.h"

template <typename T>
class Buffer;
class CommandList;
class DDSTexture;
class DescriptorHeapManager;
class DescRange;
class Device;
class DeviceContext;
class TextureResource;

/**
 * @brief Flat array of GPU textures with a shared SRV descriptor table.
 *
 * Textures are loaded on demand and indexed by filename.  The SRV descriptor
 * range can be bound as a bindless texture array in shaders.
 */
class TextureManager {
    std::wstring m_name{};
    std::wstring m_resourceFolder{};

    std::vector<std::shared_ptr<TextureResource>> m_pTextures{}; /**< @brief Loaded texture resources. */
    std::shared_ptr<DescRange> m_pSrvRange{};                    /**< @brief SRV descriptor range for all textures. */

    std::unordered_map<std::wstring, size_t> m_textureIdMap{};   /**< @brief Filename → texture-slot index map. */
    std::mutex m_textureIdMapMutex;

public:
    /**
     * @brief Constructs the texture manager and pre-allocates the SRV descriptor range.
     * @param name            Debug name.
     * @param resourceFolder  Base directory prepended to every filename.
     * @param heap            Descriptor heap from which to allocate the SRV range.
     * @param capacity        Maximum number of textures this manager can hold.
     */
    TextureManager(
        const std::wstring& name,
        const std::wstring& resourceFolder,
        std::shared_ptr<DescriptorHeapManager> heap,
        size_t capacity
    );

    /** @brief Returns the SRV descriptor range covering all loaded textures. */
    std::shared_ptr<DescRange> GetSrvRange() const;

    /**
     * @brief Returns the SRV index for @p filepath, loading the texture if it is new.
     *
     * Thread-safe; multiple threads may call this concurrently.
     *
     * @param filepath Path relative to the resource folder.
     * @param context  Device context for D3D12 resource creation.
     * @param cmd      Direct command list for upload commands.
     * @return Zero-based index into the SRV descriptor table.
     */
    size_t GetCreateTextureId(
        const std::wstring& filepath,
        std::shared_ptr<DeviceContext> context,
        std::shared_ptr<CommandList> cmd
    );
};

/**
 * @brief Maps (albedo, normal) filename pairs to packed material indices.
 *
 * Each unique material pair is assigned a slot in the @c MaterialCB constant
 * buffer and in the @ref TextureManager.  Call @ref GetCreateMaterial to look
 * up or create a material and obtain its index for use in @c ModelBuffer.
 */
class MaterialManager {
    static const std::wstring BASE_NAME;

    std::shared_ptr<TextureManager> m_pTexManager; /**< @brief Underlying texture registry. */

    /** @brief Hash key for a (albedo, normal) material pair. */
    struct MaterialKey {
        std::wstring albedo;
        std::wstring normal;

        bool operator==(const MaterialKey& other) const {
            return albedo == other.albedo && normal == other.normal;
        }

        struct Hasher {
            size_t operator()(const MaterialKey& k) const {
                return (std::hash<std::wstring>()(k.albedo)) ^
                    (std::hash<std::wstring>()(k.normal) << 1);
            }
        };
    };

    std::unordered_map<MaterialKey, size_t, MaterialKey::Hasher> m_materialIdMap;
    std::mutex m_materialIdMapMutex;

    MaterialCB m_materialBuffer{};                          /**< @brief CPU-side packed material constant buffer. */
    std::shared_ptr<Buffer<MaterialCB>> m_pMaterialBuffer; /**< @brief GPU constant buffer for materials. */

    size_t m_capacity{};

public:
    /**
     * @brief Constructs the manager, allocates the material GPU buffer, and initialises the texture manager.
     * @param resourceFolder     Base directory for texture files.
     * @param pDeviceContext     Device context.
     * @param pDescHeapManager   Descriptor heap manager for SRV/CBV allocations.
     * @param capacity           Maximum number of unique materials.
     */
    MaterialManager(
        const std::wstring& resourceFolder,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManager,
        size_t capacity
    );

    /** @brief Returns the CBV descriptor range for the packed material constant buffer. */
    std::shared_ptr<DescRange> GetMaterialCbvRange() const;

    /** @brief Returns the SRV descriptor range for the bindless texture array. */
    std::shared_ptr<DescRange> GetMaterialSrvRange() const;

    /**
     * @brief Returns the material index for the (albedo, normal) pair, creating it if new.
     *
     * Thread-safe.  Both textures are loaded lazily on first use.
     *
     * @param pDeviceContext   Device context for texture upload.
     * @param pCommandList     Direct command list for copy commands.
     * @param albedoFilepath   Relative path to the albedo DDS texture.
     * @param normalFilepath   Relative path to the normal-map DDS texture.
     * @return Zero-based material slot index to store in @c ModelBuffer::materialId.
     */
    size_t GetCreateMaterial(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList,
        const std::wstring& albedoFilepath,
        const std::wstring& normalFilepath
    );
};
