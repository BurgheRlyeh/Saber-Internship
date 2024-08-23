#pragma once

#include "Headers.h"

#include <mutex>

#include "Camera.h"
#include "ConstantBuffer.h"
#include "DynamicUploadRingBuffer.h"
#include "RenderObject.h"

class Scene {
    static constexpr size_t LIGHTS_MAX_COUNT{ 10 };

    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
        DirectX::XMFLOAT4 cameraPosition{};
    } m_sceneBuffer;
    std::mutex m_sceneBufferMutex{};
    std::shared_ptr<DynamicUploadHeap> m_pCPUAccessibleDynamicUploadHeap{};
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

    std::atomic<bool> m_isFinishFrame{};

public:
    Scene() = delete;
    Scene(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
        //, std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap
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

    void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue);

private:
    void UpdateSceneBuffer();
    void UpdateLightBuffer();
};