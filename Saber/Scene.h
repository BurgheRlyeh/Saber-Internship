#pragma once

#include "Headers.h"

#include <mutex>

#include "RenderObject.h"
#include "Camera.h"
#include "ConstantBuffer.h"

class Scene {
    static constexpr size_t LIGHTS_MAX_COUNT{ 10 };

    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
        DirectX::XMFLOAT4 cameraPosition{};
    } m_sceneBuffer;
    std::mutex m_sceneBufferMutex{};
    std::shared_ptr<ConstantBuffer> m_pSceneCB{};
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

    std::vector<std::shared_ptr<RenderObject>> m_staticObjects{};
    std::mutex m_staticObjectsMutex{};

    std::vector<std::shared_ptr<RenderObject>> m_dynamicObjects{};
    std::mutex m_dynamicObjectsMutex{};

    std::vector<std::shared_ptr<StaticCamera>> m_cameras{};
    std::mutex m_camerasMutex{};

    size_t m_currCameraId{};
    
    std::atomic<bool> m_isSceneReady{};
    std::atomic<bool> m_isLH{};

public:
    Scene() = delete;
    Scene(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
    );

    void SetSceneReadiness(bool value) {
        m_isSceneReady.store(value);
    }

    void SwapViewProjMatrixHand() {
        m_isLH.store(!m_isLH.load());
    }

    void AddStaticObject(const RenderObject&& object);
    void AddDynamicObject(const RenderObject&& object);

    void AddCamera(const StaticCamera&& camera);

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

    bool SetCurrentCamera(size_t cameraId);

    void NextCamera();

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

private:
    void UpdateSceneBuffer();
    void UpdateLightBuffer();
};