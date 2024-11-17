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
#include "GBuffer.h"
#include "MeshRenderObject.h"
#include "PostProcessing.h"
#include "RenderSubsystem.h"
#include "Texture.h"

class Scene {
    static constexpr size_t LIGHTS_MAX_COUNT{ 10 };

    std::wstring m_name{};

    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
        DirectX::XMMATRIX invViewProjMatrix{};
        DirectX::XMFLOAT4 cameraPosition{};
        DirectX::XMFLOAT4 nearFar{};
    } m_sceneBuffer;
    std::mutex m_sceneBufferMutex{};
    std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeapCpu{};
    std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeapGpu{};
    DynamicAllocation m_sceneCBDynamicAllocation{};
    std::atomic<bool> m_isUpdSceneCb{ true };
    std::shared_ptr<ConstantBuffer> m_pSceneCb{};

    struct Light {
        DirectX::XMFLOAT4 position{};
        DirectX::XMFLOAT4 diffuseColorAndPower{};
        DirectX::XMFLOAT4 specularColorAndPower{};
    };
    struct LightBuffer {
        DirectX::XMFLOAT4 ambientColorAndPower{ 0.5f, 0.5f, 0.5f, 1.f };
        DirectX::XMUINT4 lightsCount{};
        Light lights[LIGHTS_MAX_COUNT]{};
    } m_lightBuffer;
    std::mutex m_lightBufferMutex{};
    std::shared_ptr<ConstantBuffer> m_pLightCB{};
    std::atomic<bool> m_isUpdateLightCB{};

    enum RenderSubsystemId {
	    Static = 0,
        Dynamic = 1,
        StaticAlphaKill = 2,
        DynamicAlphaKill = 3,
        Count = 4
    };
    std::vector<std::shared_ptr<RenderSubsystem<CbMesh4IndirectCommand>>> m_pRenderSubsystems{};
    //std::shared_ptr<RenderSubsystem<CbMesh4IndirectCommand>> m_pStaticRenderSubsystem{};
    //std::shared_ptr<RenderSubsystem<CbMesh4IndirectCommand>> m_pDynamicRenderSubsystem{};
    //std::shared_ptr<RenderSubsystem<CbMesh4IndirectCommand>> m_pAlphaRenderSubsystem{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::atomic<bool> m_isSceneReady{};

    std::shared_ptr<DepthBuffer> m_pDepthBuffer{};
    std::shared_ptr<GBuffer> m_pGBuffer{};

    std::shared_ptr<PostProcessing> m_pPostProcessing{};

    std::shared_ptr<ComputeObject> m_pDeferredShadingComputeObject{};

public:
    Scene() = delete;
    Scene(
        const std::wstring& name,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeapCpu,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeapGpu,
        std::shared_ptr<DepthBuffer> m_pDepthBuffer,
        std::shared_ptr<GBuffer> m_pGBuffer
    );

    void InitializeRenderSubsystems(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
        std::shared_ptr<ComputeObject> pIndirectUpdater
    ) {
        for (size_t i{}; i < RenderSubsystemId::Count; ++i) {
            m_pRenderSubsystems[i]->InitializeIndirectCommandBuffer(
                pDevice,
                pAllocator,
                pDescHeapManagerCbvSrvUav,
                pDynamicUploadHeap,
                i == Static || i == StaticAlphaKill ? nullptr : pIndirectUpdater
            );
        }
    }

    void UpdateRenderSubsystems(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> pCommandQueueCopy,
        std::shared_ptr<CommandQueue> pCommandQueueDirect
    ) {
        for (auto& pRenderSubsystem : m_pRenderSubsystems) {
            pRenderSubsystem->PerformIndirectBufferUpdate(
                pDevice,
                pAllocator,
                pCommandQueueCopy,
                pCommandQueueDirect
            );
        }
    }

    void SetSceneReadiness(bool value);
    bool IsSceneReady();

    void SetDepthBuffer(std::shared_ptr<DepthBuffer> pDepthBuffer);
    std::shared_ptr<DepthBuffer> GetDepthBuffer();

    std::shared_ptr<GBuffer> GetGBuffer();
    void SetGBuffer(std::shared_ptr<GBuffer> pGBuffer);

    void Update(float deltaTime, std::shared_ptr<CommandList> pCommandList);
    void BeforeFrameJob(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList) {
        m_pDepthBuffer->Clear(pCommandList);
        if (m_pGBuffer) {
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
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    );
    void RenderDynamicObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    );
    void RenderStaticAlphaKillObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager
    );
    void RenderDynamicAlphaKillObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager
    );

    void SetDeferredShadingComputeObject(std::shared_ptr<ComputeObject> pDeferredShadingCO);
    void RunDeferredShading(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        std::shared_ptr<MaterialManager> pMaterialManager,
        UINT width,
        UINT height
    );

    void SetPostProcessing(std::shared_ptr<PostProcessing> pPostProcessing);
    void RenderPostProcessing(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    );

private:
    bool TryUpdateCamera(float deltaTime);

    void UpdateSceneBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList);
    void UpdateLightBuffer();
};