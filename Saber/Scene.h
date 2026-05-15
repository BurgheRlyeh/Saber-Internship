/**
 * @file Scene.h
 * @brief Scene: camera management, lighting, render subsystems, deferred shading,
 *        and post-processing orchestration.
 *
 * @ref Scene owns:
 *  - A @ref SceneBuffer (view/projection matrices, frame time) and its GPU buffer.
 *  - A @ref LightBuffer (ambient + point lights) and its GPU buffer.
 *  - An array of @ref RenderSubsystem instances (one per @ref RenderSubsystemType).
 *  - A list of @ref Camera objects with a current-camera index.
 *  - References to the @ref DepthBuffer, @ref GBuffer, deferred-shading compute
 *    object, and post-processing render object.
 *
 * The @ref RenderSubsystemType enum (with @c ENABLE_ENUM_FLAGS) allows callers
 * to select subsets of subsystems via bitmask flags.
 */
#pragma once

#include "Headers.h"

#include <array>
#include <mutex>

#include "EnumHelpers.h"
#include "DynamicUploadRingBuffer.h"
#include "IndirectCommand.h"
#include "LightBuffer.h"
#include "SceneBuffer.h"

template <typename T>
class Buffer;
class Camera;
class CommandList;
class ComputeObject;
class DepthBuffer;
class DescriptorHeapManager;
class Device;
class DeviceContext;
class GBuffer;
class MaterialManager;
class RenderObject;
template <IndirectCommandConcept IndirectCommand>
class RenderSubsystem;
class Texture;

/**
 * @brief Bitmask flags identifying which render subsystem(s) to address.
 *
 * Values are designed as bit flags and can be combined with @c EnumFlags<>.
 */
enum class RenderSubsystemType : size_t {
    Default   = 0 << 0, /**< @brief Opaque objects rendered with the default PSO. */
    Dynamic   = 1 << 0, /**< @brief Dynamically updated objects. */
    AlphaKill = 1 << 1, /**< @brief Objects using alpha-discard (alpha-kill) shaders. */
    Count     = 1 << 2  /**< @brief Sentinel: number of subsystem slots. */
};
ENABLE_ENUM_FLAGS(RenderSubsystemType);

/**
 * @brief Aggregates all scene-level rendering state and drives per-frame GPU work.
 */
class Scene {
    std::wstring m_name{};

    SceneBuffer m_sceneBuffer{};
    std::shared_ptr<Buffer<SceneBuffer>> m_pSceneCb{};
    std::mutex m_sceneBufferMutex{};
    std::atomic<bool> m_isUpdSceneCb{ true };

    LightBuffer m_lightBuffer{};
    std::shared_ptr<Buffer<LightBuffer>> m_pLightCB{};
    std::mutex m_lightBufferMutex{};
    std::atomic<bool> m_isUpdateLightCB{};

    /** @brief One render subsystem per @ref RenderSubsystemType slot. */
    std::array<
        std::shared_ptr<RenderSubsystem<ConstMesh4IndirectCommand>>,
        static_cast<size_t>(RenderSubsystemType::Count)
    > m_pRenderSubsystems{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::atomic<bool> m_isSceneReady{}; /**< @brief Set to @c true when the scene has finished loading. */

    std::shared_ptr<Texture> m_pTargetTexture{};
    std::shared_ptr<DepthBuffer> m_pDepthBuffer{};
    std::shared_ptr<GBuffer> m_pGBuffer{};

    std::shared_ptr<ComputeObject> m_pDeferredShadingComputeObject{};

    std::shared_ptr<RenderObject> m_pPostProcessing{};

public:
    Scene() = delete;

    /**
     * @brief Constructs a scene, allocates GPU constant buffers, and creates subsystems.
     * @param name           Debug name.
     * @param pDeviceContext Device context.
     * @param m_pDepthBuffer Depth buffer to associate with this scene.
     * @param m_pGBuffer     G-buffer for the deferred geometry pass.
     */
    Scene(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<DepthBuffer> m_pDepthBuffer,
        std::shared_ptr<GBuffer> m_pGBuffer
    );

    /**
     * @brief Recreates resolution-dependent resources (depth buffer, G-buffer textures).
     * @param pDevice Device wrapper.
     * @param width   New render width.
     * @param height  New render height.
     */
    void Resize(
        std::shared_ptr<Device> pDevice,
        uint64_t width,
        uint32_t height
    );

    /**
     * @brief Calls @c InitializeModelBuffer and @c InitializeIndirectCommandBuffer
     *        on all render subsystems.
     * @param pDeviceContext   Device context.
     * @param pIndirectUpdater Optional GPU indirect-command updater.
     */
    void InitializeRenderSubsystems(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<ComputeObject> pIndirectUpdater
    ) const;

    /**
     * @brief Sets the scene's loading-complete flag.
     * @param value @c true once all objects and resources have been uploaded.
     */
    void SetSceneReadiness(bool value);

    /** @brief Returns @c true if the scene is fully loaded and ready to render. */
    bool IsSceneReady();

    /** @brief Replaces the scene's depth buffer reference. */
    void SetDepthBuffer(std::shared_ptr<DepthBuffer> pDepthBuffer);
    /** @brief Returns the current depth buffer. */
    std::shared_ptr<DepthBuffer> GetDepthBuffer();

    /** @brief Returns the current G-buffer. */
    std::shared_ptr<GBuffer> GetGBuffer();
    /** @brief Replaces the scene's G-buffer reference. */
    void SetGBuffer(std::shared_ptr<GBuffer> pGBuffer);

    /**
     * @brief Per-frame CPU update: camera, scene buffer, light buffer, and subsystem uploads.
     * @param pDeviceContext Device context.
     * @param pCommandList   Command list for GPU-upload commands.
     * @param deltaTime      Frame delta-time in seconds.
     */
    void Update(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList,
        float deltaTime
    );

    /**
     * @brief Enqueues per-frame GPU work that must finish before the main render pass.
     * @param pCommandList Command list for pre-frame commands.
     */
    void BeforeFrameJob(std::shared_ptr<CommandList> pCommandList);

    /**
     * @brief Appends a camera to the scene's camera list.
     * @param pCamera Camera to add (rvalue — ownership is transferred).
     */
    void AddCamera(const std::shared_ptr<Camera>&& pCamera);

    /**
     * @brief Updates the aspect ratio of all cameras (call after resize).
     * @param aspectRatio New width-to-height aspect ratio.
     */
    void UpdateCamerasAspectRatio(float aspectRatio);

    /**
     * @brief Moves the active camera along its local axes.
     * @param forwardCoef Forward movement coefficient.
     * @param rightCoef   Rightward movement coefficient.
     * @return @c true if the camera accepted the input.
     */
    bool MoveCamera(float forwardCoef, float rightCoef);

    /**
     * @brief Rotates the active camera.
     * @param deltaTheta Horizontal (yaw) delta in radians.
     * @param deltaPhi   Vertical (pitch) delta in radians.
     * @return @c true if the camera accepted the input.
     */
    bool RotateCamera(float deltaTheta, float deltaPhi);

    /**
     * @brief Zooms the active camera (e.g. adjusts orbit radius or FOV).
     * @param delta Zoom delta.
     * @return @c true if the camera accepted the input.
     */
    bool ZoomCamera(float delta);

    /**
     * @brief Switches the active camera to @p cameraId.
     * @param cameraId Zero-based camera index.
     * @return @c true if @p cameraId is valid.
     */
    bool SetCurrentCamera(size_t cameraId);

    /** @brief Cycles the active camera to the next one in the list. */
    void NextCamera();

    /** @brief Toggles the active camera between perspective and orthographic projection. */
    void SwitchCameraProjection();

    /**
     * @brief Sets the scene's ambient light.
     * @param color Ambient light colour (RGB).
     * @param power Intensity multiplier (default 1.0).
     */
    void SetAmbientLight(
        const DirectX::XMFLOAT3& color,
        const float& power = 1.f
    );

    /**
     * @brief Adds a point light source to the scene.
     * @param position       World-space position (XYZW, W unused).
     * @param diffuseColor   Diffuse light colour.
     * @param specularColor  Specular light colour.
     * @param diffusePower   Diffuse intensity (default 1.0).
     * @param specularPower  Specular intensity (default 1.0).
     * @return @c true if the light was added; @c false if the light buffer is full.
     */
    bool AddLightSource(
        const DirectX::XMFLOAT4& position,
        const DirectX::XMFLOAT3& diffuseColor,
        const DirectX::XMFLOAT3& specularColor,
        const float& diffusePower = 1.f,
        const float& specularPower = 1.f
    );

    /**
     * @brief Registers a render object in the specified subsystem(s).
     * @param type    Bitmask of @ref RenderSubsystemType values.
     * @param pObject Object to register.
     */
    void AddObject(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<RenderObject> pObject
    ) const;

    /**
     * @brief Records G-buffer geometry draw calls for the selected subsystem(s).
     * @param type               Bitmask of subsystems to draw.
     * @param pDeviceContext     Device context.
     * @param pCommandListDirect Direct command list.
     * @param viewport           Render viewport.
     * @param scissorRect        Scissor rectangle.
     */
    void RenderObjects(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect
    );

    /**
     * @brief Stores the deferred-shading compute object.
     * @param pDeferredShadingCO The compute object to use for the lighting pass.
     */
    void SetDeferredShadingComputeObject(std::shared_ptr<ComputeObject> pDeferredShadingCO);

    /**
     * @brief Dispatches the deferred-shading compute pass.
     * @param pCommandListCompute   Compute command list.
     * @param pResDescHeapManager   Descriptor heap manager for binding SRVs/UAVs.
     * @param pMaterialManager      Material manager providing the material CBV / texture SRV ranges.
     * @param width                 Render target width in pixels.
     * @param height                Render target height in pixels.
     */
    void RunDeferredShading(
        std::shared_ptr<CommandList> pCommandListCompute,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager,
        UINT width,
        UINT height
    );

    /**
     * @brief Stores the post-processing render object.
     * @param pPostProcessing Render object implementing the post-processing pass.
     */
    void SetPostProcessing(std::shared_ptr<RenderObject> pPostProcessing);

    /**
     * @brief Executes the post-processing pass and presents to the back buffer.
     * @param pCommandListDirect  Direct command list.
     * @param pResDescHeapManager Descriptor heap for binding the G-buffer SRV.
     * @param viewport            Render viewport.
     * @param scissorRect         Scissor rectangle.
     * @param renderTargetView    CPU descriptor handle for the swap-chain back buffer RTV.
     */
    void RenderPostProcessing(
        std::shared_ptr<CommandList> pCommandListDirect,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    );

private:
    /**
     * @brief Updates the active camera for the current frame delta.
     * @param deltaTime Frame delta-time in seconds.
     * @return @c true if the scene buffer must be re-uploaded this frame.
     */
    bool UpdateCamera(float deltaTime);

    /**
     * @brief Uploads the scene constant buffer if it has been dirtied.
     * @param pDeviceContext Device context.
     * @param pCommandList   Command list for the upload.
     */
    void UpdateSceneBuffer(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList
    );

    /** @brief Uploads the light constant buffer if it has been dirtied. */
    void UpdateLightBuffer();
};
