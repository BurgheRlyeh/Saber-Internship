#pragma once

#include "Headers.h"

#include <mutex>

#include "Camera.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "ConstantBuffer.h"
#include "ComputeObject.h"
#include "DepthBuffer.h"
#include "DynamicUploadRingBuffer.h"
#include "Texture.h"
#include "LightBuffer.h"
#include "MeshRenderObject.h"
#include "PostProcessing.h"
#include "RenderSubsystem.h"
#include "SceneBuffer.h"

class Scene {
    std::wstring m_name{};

    std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeapCpu{};
    std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeapGpu{};

    SceneBuffer m_sceneBuffer;
    std::shared_ptr<ConstantBuffer> m_pSceneCb{};
    std::mutex m_sceneBufferMutex{};
    std::atomic<bool> m_isUpdSceneCb{ true };
    DynamicAllocation m_sceneCBDynamicAllocation{};

    LightBuffer m_lightBuffer;
    std::shared_ptr<ConstantBuffer> m_pLightCB{};
    std::mutex m_lightBufferMutex{};
    std::atomic<bool> m_isUpdateLightCB{};

    enum RenderSubsystemId {
	    Static = 0,
        Dynamic = 1,
        StaticAlphaKill = 2,
        DynamicAlphaKill = 3,
        Count = 4
    };
    std::vector<std::shared_ptr<RenderSubsystem<ConstMesh4IndirectCommand>>> m_pRenderSubsystems{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::atomic<bool> m_isSceneReady{};

    std::shared_ptr<Texture> m_pTargetTexture{};
    std::shared_ptr<DepthBuffer> m_pDepthBuffer{};
    std::shared_ptr<Texture> m_pGBuffer{};

    std::shared_ptr<ComputeObject> m_pDeferredShadingComputeObject{};

    std::shared_ptr<PostProcessing> m_pPostProcessing{};

public:
    Scene() = delete;
    Scene(
        const std::wstring& name,
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeapCpu,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeapGpu,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
        std::shared_ptr<DepthBuffer> m_pDepthBuffer,
        std::shared_ptr<Texture> m_pGBuffer
    );

    void Resize(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        uint64_t width,
        uint32_t height
    ) {
        ResizeTargetTexture(pDevice, pAllocator, width, height);
        UpdateCamerasAspectRatio(static_cast<float>(width) / height);
    }

    void ResizeTargetTexture(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        uint64_t width,
        uint32_t height
    ) {
        m_pTargetTexture->Resize(pDevice, pAllocator, width, height);
    }

    void InitializeRenderSubsystems(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
        std::shared_ptr<ComputeObject> pIndirectUpdater
    ) const;
    void UpdateRenderSubsystems(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> pCommandQueueCopy,
        std::shared_ptr<CommandQueue> pCommandQueueDirect
    ) const;

    void SetSceneReadiness(bool value);
    bool IsSceneReady();

    void SetDepthBuffer(std::shared_ptr<DepthBuffer> pDepthBuffer);
    std::shared_ptr<DepthBuffer> GetDepthBuffer();

    std::shared_ptr<Texture> GetGBuffer();
    void SetGBuffer(std::shared_ptr<Texture> pGBuffer);

    void Update(float deltaTime, std::shared_ptr<CommandList> pCommandList);
    void BeforeFrameJob(std::shared_ptr<CommandList> pCommandList) {
        m_pDepthBuffer->Clear(pCommandList);
        if (m_pGBuffer) {
            m_pGBuffer->ChangeState(pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
            m_pGBuffer->Clear(pCommandList);
        }
    }

    void AddCamera(const std::shared_ptr<Camera>&& pCamera);
    void UpdateCamerasAspectRatio(float aspectRatio);
    bool TryMoveCamera(float forwardCoef, float rightCoef);
    bool TryRotateCamera(float deltaX, float deltaY);
    bool SetCurrentCamera(size_t cameraId);
    void NextCamera();

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

    void AddStaticObject(std::shared_ptr<RenderObject> pObject) const;
    void AddDynamicObject(std::shared_ptr<RenderObject> pObject) const;
    void AddStaticAlphaKillObject(std::shared_ptr<RenderObject> pObject) const;
    void AddDynamicAlphaKillObject(std::shared_ptr<RenderObject> pObject) const;
    void RenderStaticObjects(
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager
    );
    void RenderDynamicObjects(
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager
    );
    void RenderStaticAlphaKillObjects(
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager
    );
    void RenderDynamicAlphaKillObjects(
        std::shared_ptr<CommandList> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager
    );

    void SetDeferredShadingComputeObject(std::shared_ptr<ComputeObject> pDeferredShadingCO);
    void RunDeferredShading(
        std::shared_ptr<CommandList> pCommandListCompute,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager,
        UINT width,
        UINT height
    );

    void SetPostProcessing(std::shared_ptr<PostProcessing> pPostProcessing);
    void RenderPostProcessing(
        std::shared_ptr<CommandList> pCommandListDirect,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    );

private:
    bool TryUpdateCamera(float deltaTime);

    void UpdateSceneBuffer(std::shared_ptr<CommandList> pCommandList);
    void UpdateLightBuffer();
};