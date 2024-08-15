#include "Scene.h"

Scene::Scene(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator) {
    m_pSceneCB = std::make_shared<ConstantBuffer>(
        pDevice,
        pAllocator,
        CD3DX12_RESOURCE_ALLOCATION_INFO(sizeof(SceneBuffer), 0)
    );

    m_pLightCB = std::make_shared<ConstantBuffer>(
        pDevice,
        pAllocator,
        CD3DX12_RESOURCE_ALLOCATION_INFO(sizeof(LightBuffer), 0)
    );
}

void Scene::AddStaticObject(const RenderObject&& object) {
    std::scoped_lock<std::mutex> lock(m_staticObjectsMutex);
    m_staticObjects.push_back(std::make_shared<RenderObject>(object));
}

void Scene::AddDynamicObject(const RenderObject&& object) {
    std::scoped_lock<std::mutex> lock(m_dynamicObjectsMutex);
    m_dynamicObjects.push_back(std::make_shared<RenderObject>(object));
}

void Scene::AddCamera(const StaticCamera&& camera) {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    m_cameras.push_back(std::make_shared<StaticCamera>(camera));
    lock.unlock();

    if (m_pSceneCB->IsEmpty()) {
        m_isUpdateSceneCB.store(true);
    }
}

void Scene::SetAmbientLight(
    const DirectX::XMFLOAT3& color,
    const float& power = 1.f
) {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);

    m_lightBuffer.ambientColorAndPower = {
        color.x,
        color.y,
        color.z,
        power
    };

    m_isUpdateLightCB.store(true);
}

bool Scene::AddLightSource(
    const DirectX::XMFLOAT4& position,
    const DirectX::XMFLOAT3& diffuseColor,
    const DirectX::XMFLOAT3& specularColor,
    const float& diffusePower,
    const float& specularPower
) {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
    if (m_lightBuffer.lightsCount.x == LIGHTS_MAX_COUNT) {
        return false;
    }

    m_lightBuffer.lights[m_lightBuffer.lightsCount.x++] = {
        position,
        {
            diffuseColor.x,
            diffuseColor.y,
            diffuseColor.z,
            diffusePower
        },
        {
            specularColor.x,
            specularColor.y,
            specularColor.z,
            specularPower
        }
    };

    m_isUpdateLightCB.store(true);
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    for (auto& camera : m_cameras) {
        camera->m_aspectRatio = aspectRatio;
    }
}

bool Scene::SetCurrentCamera(size_t cameraId) {
    if (std::unique_lock<std::mutex> lock(m_camerasMutex);  m_cameras.size() <= cameraId)
        return false;

    m_currCameraId = cameraId;

    m_isUpdateSceneCB.store(true);

    return true;
}

void Scene::NextCamera() {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    if (!m_cameras.empty()) {
        lock.unlock();
        SetCurrentCamera((m_currCameraId + 1) % m_cameras.size());
    }
}

DirectX::XMMATRIX Scene::GetViewProjectionMatrix() {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    return m_isLH.load() ? m_cameras.at(m_currCameraId)->GetViewProjectionMatrixLH()
        : m_cameras.at(m_currCameraId)->GetViewProjectionMatrixRH();
}

void Scene::RenderStaticObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_cameras.empty())
        return;

    UpdateSceneBuffer();
    UpdateLightBuffer();

    DirectX::XMMATRIX viewProjectionMatrix{ GetViewProjectionMatrix() };

    std::scoped_lock<std::mutex> staticObjectsLock(m_staticObjectsMutex);
    for (auto const& obj : m_staticObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            renderTargetView,
            depthStencilView,
            m_pLightCB->GetResource(),
            m_pSceneCB->GetResource()
        );
    }
}

void Scene::RenderDynamicObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_cameras.empty())
        return;

    UpdateSceneBuffer();
    UpdateLightBuffer();

    std::scoped_lock<std::mutex> dynamicObjectsLock(m_dynamicObjectsMutex);
    for (auto const& obj : m_dynamicObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            renderTargetView,
            depthStencilView,
            m_pLightCB->GetResource(),
            m_pSceneCB->GetResource()
        );
    }
}

void Scene::UpdateSceneBuffer() {
    if (!m_isUpdateSceneCB.load()) {
        return;
    }
    m_isUpdateSceneCB.store(false);

    std::scoped_lock<std::mutex> lock(m_sceneBufferMutex);
    m_sceneBuffer.viewProjMatrix = GetViewProjectionMatrix();

    DirectX::XMFLOAT3 cameraPosition{ m_cameras.at(m_currCameraId)->GetPosition() };
    m_sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };

    m_pSceneCB->Update(&m_sceneBuffer);
}

void Scene::UpdateLightBuffer() {
    if (!m_isUpdateLightCB.load()) {
        return;
    }
    m_isUpdateLightCB.store(false);

    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
    m_pLightCB->Update(&m_lightBuffer);
}
