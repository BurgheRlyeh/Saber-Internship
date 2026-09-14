#pragma once

#include "Headers.h"

#include <array>
#include <functional>
#include <mutex>

#include "DynamicUploadRingBuffer.h"
#include "IndirectCommand.h"
#include "LightBuffer.h"
#include "CameraBuffer.h"
#include "CullingParams.h"
#include "ReadbackBuffer.h"
#include "RenderSubsystemTypes.h"

template <typename T>
class Buffer;
class Camera;
class CommandList;
class ComputeObject;
class DepthBuffer;
class DescriptorHeap;
class Device;
class DeviceContext;
class FlyCamera;
class GBuffer;
class HiDepthBuffer;
class LightSource;
class MaterialManager;
class RenderObject;
template <IndirectCommandConcept IndirectCommand>
class RenderSubsystem;
class Texture;

class Scene {
public:
    struct ShadowSettings {
        float depthBias{ .05f };
        float normalOffset{ 1.5f };
        int pcfRadius{ 1 };
    };

    // Both volumes live in ModelBuffer, the shaders pick one by this
    enum class BoundingVolumeType : uint32_t {
        AABB = 0,
        Sphere,

        Count
    };

    struct CullingSettings {
        // Off draws everything in one pass, which is the picture the two passes
        // have to match
        bool twoPassCulling{ true };
        bool frustumCulling{ true };
        bool occlusionCulling{ true };
        BoundingVolumeType boundingVolume{ BoundingVolumeType::AABB };
    };

    // What both culling passes counted, one block per subsystem
    struct SceneCullingStats {
        CullingStats perSubsystem[static_cast<size_t>(RenderSubsystemType::Count)]{};
    };

private:
    std::wstring m_name{};

    std::shared_ptr<Buffer<CameraBuffer>> m_pCameraCB{};
    std::mutex m_cameraBufferMutex{};

    std::vector<std::shared_ptr<LightSource>> m_pLights{};

    LightBuffer m_lightBuffer{};
    std::shared_ptr<Buffer<LightBuffer>> m_pLightCB{};
    std::mutex m_lightBufferMutex{};

    std::array<
        std::shared_ptr<RenderSubsystem<ConstMesh4IndirectCommand>>,
        static_cast<size_t>(RenderSubsystemType::Count)
    > m_pRenderSubsystems{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::shared_ptr<FlyCamera> m_pDebugCamera{};
    std::atomic<bool> m_isDebugCameraActive{};

    std::atomic<bool> m_isSceneReady{};

    std::shared_ptr<Texture> m_pTargetTexture{};
    std::shared_ptr<HiDepthBuffer> m_pDepthBuffer{};
    std::shared_ptr<GBuffer> m_pGBuffer{};

    static constexpr UINT ShadowMapResolution{ 2048 };
    std::shared_ptr<DepthBuffer> m_pShadowMap{};
    std::shared_ptr<Buffer<CameraBuffer>> m_pShadowCameraCB{};

    // Always the gameplay camera, never the debug one
    std::shared_ptr<Buffer<CameraBuffer>> m_pCullingCameraCB{};

    DirectX::XMMATRIX m_shadowViewProj{ DirectX::XMMatrixIdentity() };
    DirectX::XMFLOAT4 m_shadowParams{};
    uint32_t m_shadowLightId{ SHADOW_NO_LIGHT };

    ShadowSettings m_shadowSettings{};
    CullingSettings m_cullingSettings{};

    // Written by the culling passes, copied out once per frame and read back a few
    // frames later, so more slots than there are frames in flight
    static constexpr size_t CullingStatsSlots{ 8 };
    std::shared_ptr<GPUResource> m_pCullingStats{};
    std::unique_ptr<ReadbackBuffer<SceneCullingStats>> m_pCullingStatsReadback{};

    std::shared_ptr<ComputeObject> m_pOcclusionCulling{};
    std::shared_ptr<ComputeObject> m_pDeferredShadingComputeObject{};

    std::shared_ptr<RenderObject> m_pPostProcessing{};

    std::function<void(float deltaTime, Scene& scene)> m_simulation{};
    std::function<void(float forwardCoef, float rightCoef)> m_movementHandler{};
    std::function<void()> m_settingsUI{};
    std::mutex m_gameHooksMutex{};

public:
    Scene() = delete;
    Scene(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<HiDepthBuffer> m_pDepthBuffer,
        std::shared_ptr<GBuffer> m_pGBuffer
    );

    void Resize(
        std::shared_ptr<Device> pDevice,
        uint64_t width,
        uint32_t height
    );

    void InitializeRenderSubsystems(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<ComputeObject> pIndirectUpdater
    ) const;

    void SetSceneReadiness(bool value);
    bool IsSceneReady();

    void SetDepthBuffer(std::shared_ptr<HiDepthBuffer> pDepthBuffer);
    std::shared_ptr<HiDepthBuffer> GetDepthBuffer();

    std::shared_ptr<GBuffer> GetGBuffer();
    void SetGBuffer(std::shared_ptr<GBuffer> pGBuffer);

    void Update(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList,
        float deltaTime
    );
    void BeforeFrameJob(std::shared_ptr<CommandList> pCommandList);

    void AddCamera(const std::shared_ptr<Camera>&& pCamera);
    void UpdateCamerasAspectRatio(float aspectRatio);
    bool Move(float forwardCoef, float rightCoef);
    void SetMovementHandler(
        std::function<void(float forwardCoef, float rightCoef)> handler
    );

    // What the frame is built for: shadows, culling and the game read this one
    std::shared_ptr<Camera> GetCurrentCamera();

    // What the frame is drawn through, which is the debug camera while it is on
    std::shared_ptr<Camera> GetRenderCamera();

    void SwitchDebugCamera();
    bool IsDebugCameraActive() const;

    bool RotateCamera(float deltaTheta, float deltaPhi);
    bool ZoomCamera(float delta);
    bool SetCurrentCamera(size_t cameraId);
    void NextCamera();
    void SwitchCameraProjection();

    void SetAmbientLight(
        const DirectX::XMFLOAT3& color,
        const float& power = 1.f
    );
    bool AddLight(const std::shared_ptr<LightSource>& pLight);

    size_t GetLightCount();
    std::shared_ptr<LightSource> GetLight(size_t lightId);

    RenderObjectHandle AddObject(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<RenderObject> pObject
    );

    void UpdateObjectMatrix(
        RenderObjectHandle handle,
        const DirectX::XMMATRIX& modelMatrix
    );

    void SetSimulation(std::function<void(float deltaTime, Scene& scene)> simulation);

    // Extra ImGui drawn with the scene panels; optional, contents are the game's
    void SetSettingsUI(std::function<void()> settingsUI);
    void SetOcclusionCullingComputeObject(std::shared_ptr<ComputeObject> pOcclusionCulling);

    // Zeroed before the first pass, copied out after the second
    void ResetCullingStats(std::shared_ptr<CommandList> pCommandList);
    void CopyCullingStatsForReadback(std::shared_ptr<CommandList> pCommandList);
    void FinishFrame(uint64_t fenceValue, uint64_t completedFenceValue);

    // Picks the commands one of the two passes draws. The second pass is the one
    // that tests against the depth pyramid and leaves the visibility behind
    void CullObjects(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList,
        bool secondPass
    );

    void RenderObjects(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        bool secondPass = false
    );

    // Same geometry from the shadow camera, depth only. Call once per subsystem
    // before the lighting pass
    void RenderObjectsDepth(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandListDirect
    );

    void SetDeferredShadingComputeObject(std::shared_ptr<ComputeObject> pDeferredShadingCO);
    void RunDeferredShading(
        std::shared_ptr<CommandList> pCommandListCompute,
        std::shared_ptr<DescriptorHeap> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager,
        UINT width,
        UINT height
    );

    void SetPostProcessing(std::shared_ptr<RenderObject> pPostProcessing);
    void RenderPostProcessing(
        std::shared_ptr<CommandList> pCommandListDirect,
        std::shared_ptr<DescriptorHeap> pResDescHeapManager,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    );

private:
    bool UpdateCamera(float deltaTime);

    void UpdateCameraBuffer(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList
    );
    void UpdateLightBuffer();
    void UpdateShadowCameraBuffer();
    void UpdateRenderSubsystems(
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandList
    );
    void UpdateSimulation(float deltaTime);

public:
    void DrawSettingsUI();

private:
    void DrawCullingStatsUI();
};

// UI
bool DrawSettings(Scene::ShadowSettings& settings);
bool DrawSettings(Scene::CullingSettings& settings);