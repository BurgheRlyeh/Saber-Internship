#pragma once

#include "Headers.h"

#include <mutex>

#include "Camera.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "ConstantBuffer.h"
#include "DynamicUploadRingBuffer.h"
#include "RenderObject.h"
#include "RenderTarget.h"
#include "Texture.h"

class Scene {
    static constexpr size_t LIGHTS_MAX_COUNT{ 10 };

    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
        DirectX::XMFLOAT4 cameraPosition{};
    } m_sceneBuffer;
    std::mutex m_sceneBufferMutex{};
    std::shared_ptr<DynamicUploadHeap> m_pCameraHeap{};
    DynamicAllocation m_sceneCBDynamicAllocation{};
    std::atomic<bool> m_isUpdateSceneCB{};

    struct Light {
        DirectX::XMFLOAT4 position{};
        DirectX::XMFLOAT4 diffuseColorAndPower{};
        DirectX::XMFLOAT4 specularColorAndPower{};
    };
    struct LightBuffer {
        DirectX::XMFLOAT4 ambientColorAndPower{ 0.5f, 0.5f, 0.5f, 1.f};
        DirectX::XMUINT4 lightsCount{};
        Light lights[LIGHTS_MAX_COUNT]{};
    } m_lightBuffer;
    std::mutex m_lightBufferMutex{};
    std::shared_ptr<ConstantBuffer> m_pLightCB{};
    std::atomic<bool> m_isUpdateLightCB{};

    std::vector<std::shared_ptr<RenderObject>> m_pStaticObjects{};
    std::mutex m_staticObjectsMutex{};

    std::vector<std::shared_ptr<RenderObject>> m_pDynamicObjects{};
    std::mutex m_dynamicObjectsMutex{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::atomic<bool> m_isSceneReady{};
    std::atomic<bool> m_isUpdateCameraHeap{};

    std::shared_ptr<RenderTarget> m_pGBuffer{};
    size_t m_currGBufferId{};
    std::atomic<bool> m_isGBufferNeedResize{};

public:
    Scene() = delete;
    Scene(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
    );

    void SetSceneReadiness(bool value) {
        m_isSceneReady.store(value);
    }

    void AddStaticObject(const RenderObject&& object);
    void AddDynamicObject(const RenderObject&& object);

    void AddCamera(const std::shared_ptr<Camera>&& pCamera);

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

    void UpdateCamerasAspectRatio(float aspectRatio);

    void Update(float deltaTime);

    bool SetCurrentCamera(size_t cameraId);

    void NextCamera();

    bool TryMoveCamera(float forwardCoef, float rightCoef);

    bool TryRotateCamera(float deltaX, float deltaY);

    bool TryUpdateCamera(float deltaTime);

    DirectX::XMMATRIX GetViewProjectionMatrix();

    void RenderStaticObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
    );
    void RenderDynamicObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
    );

    void CreateGBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        uint8_t numBuffers,
        UINT64 width,
        UINT height
    ) {
        D3D12_RESOURCE_DESC resDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                width,
                height
        ) };
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        FLOAT clearColor[] = { .6f, .4f, .4f, 1.f };


        m_pGBuffer = std::make_shared<RenderTarget>(
            pDevice,
            pAllocator,
            numBuffers,
            resDesc,
            CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clearColor)
        );
    }

    void ResizeGBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        UINT64 width,
        UINT height
    ) {
        D3D12_RESOURCE_DESC resDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R8G8B8A8_UNORM,
                width,
                height
        ) };
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        FLOAT clearColor[] = { .6f, .4f, .4f, 1.f };

        m_pGBuffer->Resize(
            pDevice,
            pAllocator,
            resDesc,
            CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clearColor)
        );
    }

    void ClearGBuffer(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
    ) {
        float clearColor[]{
            .6f,
            .4f,
            .4f,
            1.f
        };

        RenderTarget::ClearRenderTarget(
            pCommandList,
            m_pGBuffer->GetCurrentBufferResource(m_currGBufferId),
            m_pGBuffer->GetCPUDescHandle(m_currGBufferId),
            clearColor
        );
    }

    void SetCurrentBackBuffer(size_t bufferId) {
        m_currGBufferId = bufferId;
    }

    void CopyRTV(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> pRenderTarget
    ) {
        Microsoft::WRL::ComPtr<ID3D12Resource> pTex{
            m_pGBuffer->GetCurrentBufferResource(m_currGBufferId)
        };

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pTex.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        ));
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST
        ));

        pCommandList->CopyResource(pRenderTarget.Get(), pTex.Get());

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pTex.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ));
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ));
    }

    void UpdateCameraHeap(uint64_t fenceValue, uint64_t lastCompletedFenceValue);

private:
    void UpdateSceneBuffer();
    void UpdateLightBuffer();
};