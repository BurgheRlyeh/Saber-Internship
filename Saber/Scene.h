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
#include "RenderModel.h"
#include "PostProcessing.h"
#include "RenderSubsystem.h"
#include "Texture.h"

class Scene {
    static constexpr size_t LIGHTS_MAX_COUNT{ 10 };

    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
        DirectX::XMMATRIX invViewProjMatrix{};
        DirectX::XMFLOAT4 cameraPosition{};
        DirectX::XMFLOAT4 nearFar{};
    } m_sceneBuffer;
    std::mutex m_sceneBufferMutex{};
    std::shared_ptr<DynamicUploadHeap> m_pDynamicUploadHeap{};
    DynamicAllocation m_sceneCBDynamicAllocation{};

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

    std::shared_ptr<RenderSubsystem> m_pStaticRenderSubsystem{};
    std::shared_ptr<RenderSubsystem> m_pDynamicRenderSubsystem{};
    std::shared_ptr<RenderSubsystem> m_pAlphaRenderSubsystem{};
    std::shared_ptr<IndirectRenderSubsystem> m_pIndirectRenderSubsystem{};

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
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
        std::shared_ptr<DepthBuffer> m_pDepthBuffer,
        std::shared_ptr<GBuffer> m_pGBuffer = nullptr
    );

    void AddIndirectObject(std::shared_ptr<MeshRenderObject> pObject) {
        m_pIndirectRenderSubsystem->Add(pObject);
    }
    void InitIndirectRenderSubsystem(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
        std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
        std::shared_ptr<ComputeObject> pIndirectUpdater
    ) {
        m_pIndirectRenderSubsystem->InitializeIndirectCommandBuffer(
            pDevice,
            pAllocator,
            pDescHeapManagerCbvSrvUav,
            pDynamicUploadHeap,
            pIndirectUpdater
        );
    }
    void PerformIndirectRenderSubsystemUpdate(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        std::shared_ptr<CommandQueue> pCommandQueueCopy,
        std::shared_ptr<CommandQueue> pCommandQueueDirect
    ) {
        m_pIndirectRenderSubsystem->PerformIndirectBufferUpdate(
            pDevice,
            pAllocator,
            pCommandQueueCopy,
            pCommandQueueDirect
        );
    }

    void RenderIndirectObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    ) {
        if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
            return;

        UpdateSceneBuffer();

        auto outerRootParametersSetter = [&](
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
            UINT& rootParamId
            ) {
                pCommandListDirect->SetGraphicsRootConstantBufferView(
                    rootParamId++,
                    m_sceneCBDynamicAllocation.gpuAddress
                );
            };

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
        if (m_pGBuffer) {
            rtvs = m_pGBuffer->GetRtvs();
        }
        else {
            rtvs.push_back(renderTargetView);
        }

        std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
        m_pIndirectRenderSubsystem->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            rtvs.data(),
            rtvs.size(),
            &m_pDepthBuffer->GetDsvCpuDescHandle(),
            outerRootParametersSetter
        );
    }

    void SetSceneReadiness(bool value);
    bool IsSceneReady();

    void SetDepthBuffer(std::shared_ptr<DepthBuffer> pDepthBuffer);
    std::shared_ptr<DepthBuffer> GetDepthBuffer();

    std::shared_ptr<GBuffer> GetGBuffer();
    void SetGBuffer(std::shared_ptr<GBuffer> pGBuffer);

    void Update(float deltaTime);
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

    void AddStaticObject(std::shared_ptr<RenderObject> pObject);
    void AddDynamicObject(std::shared_ptr<RenderObject> pObject);
    void AddAlphaObject(std::shared_ptr<RenderObject> pObject);
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
    void RenderAlphaObjects(
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

    void UpdateSceneBuffer();
    void UpdateLightBuffer();
};