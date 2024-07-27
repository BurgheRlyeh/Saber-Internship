#pragma once

#include "Headers.h"

#include <mutex>

#include "RenderObject.h"
#include "Camera.h"
#include "ConstantBuffer.h"

class Scene {
    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
    } m_sceneBuffer;
    std::shared_ptr<ConstantBuffer> m_pConstantBuffer{};

    std::vector<std::shared_ptr<RenderObject>> m_staticObjects{};
    std::mutex m_staticObjectsMutex{};

    std::vector<std::shared_ptr<RenderObject>> m_dynamicObjects{};
    std::mutex m_dynamicObjectsMutex{};

    std::vector<std::shared_ptr<StaticCamera>> m_cameras{};
    std::mutex m_camerasMutex{};

    size_t m_currCameraId{};
    
    std::atomic<bool> m_isSceneReady{};

public:
    Scene() = delete;
    Scene(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
    ) {
        
        m_pConstantBuffer = std::make_shared<ConstantBuffer>(
            pDevice,
            pAllocator,
            CD3DX12_RESOURCE_ALLOCATION_INFO(sizeof(ConstantBuffer), 0)
        );
    }

    void SetSceneReadiness(bool value) {
        m_isSceneReady.store(value);
    }

    void AddStaticObject(const RenderObject&& object);
    void AddDynamicObject(const RenderObject&& object);

    void AddCamera(const StaticCamera&& camera);

    void UpdateCamerasAspectRatio(float aspectRatio);

    bool SetCurrentCamera(size_t cameraId);

    void NextCamera();

    DirectX::XMMATRIX GetViewProjectionMatrix(bool isLH);

    void RenderStaticObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
        bool isLH = false
    );
    void RenderDynamicObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
        bool isLH = false
    );
};