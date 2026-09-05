#pragma once

#include "Headers.h"

#include <array>
#include <mutex>

#include "EnumHelpers.h"
#include "DynamicUploadRingBuffer.h"
#include "IndirectCommand.h"
#include "LightBuffer.h"
#include "CameraBuffer.h"

template <typename T>
class Buffer;
class Camera;
class CommandList;
class ComputeObject;
class DescriptorHeap;
class Device;
class DeviceContext;
class GBuffer;
class HiDepthBuffer;
class MaterialManager;
class RenderObject;
template <IndirectCommandConcept IndirectCommand>
class RenderSubsystem;
class Texture;

enum class RenderSubsystemType : size_t {
    Default     = 0 << 0,
    Dynamic     = 1 << 0,
    AlphaKill   = 1 << 1,
    Count       = 1 << 2
};
ENABLE_ENUM_FLAGS(RenderSubsystemType);

class Scene {
    std::wstring m_name{};

    std::shared_ptr<Buffer<CameraBuffer>> m_pCameraCB{};
    std::mutex m_cameraBufferMutex{};

    LightBuffer m_lightBuffer{};
    std::shared_ptr<Buffer<LightBuffer>> m_pLightCB{};
    std::mutex m_lightBufferMutex{};
    std::atomic<bool> m_isUpdateLightCB{};

    std::array<
        std::shared_ptr<RenderSubsystem<ConstMesh4IndirectCommand>>,
        static_cast<size_t>(RenderSubsystemType::Count)
    > m_pRenderSubsystems{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::atomic<bool> m_isSceneReady{};

    std::shared_ptr<Texture> m_pTargetTexture{};
    std::shared_ptr<HiDepthBuffer> m_pDepthBuffer{};
    std::shared_ptr<GBuffer> m_pGBuffer{};

    std::shared_ptr<ComputeObject> m_pDeferredShadingComputeObject{};

    std::shared_ptr<RenderObject> m_pPostProcessing{};

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
    bool MoveCamera(float forwardCoef, float rightCoef);
    bool RotateCamera(float deltaTheta, float deltaPhi);
    bool ZoomCamera(float delta);
    bool SetCurrentCamera(size_t cameraId);
    void NextCamera();
    void SwitchCameraProjection();

    void SetAmbientLight(
        const DirectX::XMFLOAT3& color,
        const float& power = 1.f
    );
    bool AddLightSource(
        const DirectX::XMFLOAT4& position,
        const DirectX::XMFLOAT3& diffuseColor,
        const DirectX::XMFLOAT3& specularColor,
        const float& diffusePower = 1.f,
        const float& specularPower = 1.f
    );

    void AddObject(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<RenderObject> pObject
    ) const;
    void RenderObjects(
        const EnumFlags<RenderSubsystemType> type,
        std::shared_ptr<DeviceContext> pDeviceContext,
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect
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

public:
    void DrawCurrentCameraSettingsUI();
    void DrawSettingsUI();
};