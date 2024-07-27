#include "Scene.h"

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

    if (m_pConstantBuffer->IsEmpty()) {
        m_sceneBuffer.viewProjMatrix = GetViewProjectionMatrix(false);
        m_pConstantBuffer->Update(&m_sceneBuffer);
    }
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    for (auto& camera : m_cameras) {
        camera->m_aspectRatio = aspectRatio;
    }
}

bool Scene::SetCurrentCamera(size_t cameraId) {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    if (m_cameras.size() <= cameraId)
        return false;
    lock.unlock();

    m_currCameraId = cameraId;
    m_sceneBuffer.viewProjMatrix = GetViewProjectionMatrix(false);
    m_pConstantBuffer->Update(&m_sceneBuffer);

    return true;
}

void Scene::NextCamera() {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    if (!m_cameras.empty()) {
        lock.unlock();
        SetCurrentCamera((m_currCameraId + 1) % m_cameras.size());
    }
}

DirectX::XMMATRIX Scene::GetViewProjectionMatrix(bool isLH) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    return isLH ? m_cameras.at(m_currCameraId)->GetViewProjectionMatrixLH()
        : m_cameras.at(m_currCameraId)->GetViewProjectionMatrixRH();
}

void Scene::RenderStaticObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
    bool isLH
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_cameras.empty())
        return;

    DirectX::XMMATRIX viewProjectionMatrix{ GetViewProjectionMatrix(isLH) };

    std::scoped_lock<std::mutex> staticObjectsLock(m_staticObjectsMutex);
    for (auto const& obj : m_staticObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            renderTargetView,
            depthStencilView,
            m_pConstantBuffer->GetResource()
        );
    }
}

void Scene::RenderDynamicObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
    bool isLH
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_cameras.empty())
        return;

    DirectX::XMMATRIX viewProjectionMatrix{ GetViewProjectionMatrix(isLH) };

    std::scoped_lock<std::mutex> dynamicObjectsLock(m_dynamicObjectsMutex);
    for (auto const& obj : m_dynamicObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            renderTargetView,
            depthStencilView,
            m_pConstantBuffer->GetResource()
        );
    }
}
