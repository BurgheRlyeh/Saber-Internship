#include "Scene.h"

void Scene::AddStaticObject(const RenderObject&& object) {
    //std::scoped_lock<std::mutex> lock(m_staticObjectsMutex);
    m_staticObjects.push_back(std::make_shared<RenderObject>(object));
}

void Scene::AddCamera(const StaticCamera&& camera) {
    //std::scoped_lock<std::mutex> lock(m_camerasMutex);
    m_cameras.push_back(std::make_shared<StaticCamera>(camera));
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    //std::scoped_lock<std::mutex> lock(m_camerasMutex);
    for (auto& camera : m_cameras) {
        camera->m_aspectRatio = aspectRatio;
    }
}

bool Scene::SetCurrentCamera(size_t cameraId) {
    //std::unique_lock<std::mutex> lock(m_camerasMutex);
    if (m_cameras.size() <= cameraId)
        return false;
    //lock.unlock();

    m_currCameraId = cameraId;
    return true;
}

void Scene::NextCamera() {
    //std::scoped_lock<std::mutex> lock(m_camerasMutex);
    if (!m_cameras.empty())
        m_currCameraId = (m_currCameraId + 1) % m_cameras.size();
}

void Scene::RenderStaticObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
    bool isLH
) {
    //std::unique_lock<std::mutex> camerasLock(m_camerasMutex);
    if (m_cameras.empty())
        return;

    DirectX::XMMATRIX viewProjection{
        isLH ? m_cameras.at(m_currCameraId)->GetViewProjectionMatrixLH()
        : m_cameras.at(m_currCameraId)->GetViewProjectionMatrixRH()
    };
    //camerasLock.unlock();

    //std::scoped_lock<std::mutex> staticObjectsLock(m_staticObjectsMutex);
    for (auto const& obj : m_staticObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            renderTargetView,
            depthStencilView,
            viewProjection
        );
    }
}
