#include "Scene.h"

void Scene::AddStaticObject(const RenderObject&& object) {
    m_staticObjects.push_back(std::make_shared<RenderObject>(object));
}

void Scene::AddCamera(const StaticCamera&& camera) {
    m_cameras.push_back(std::make_shared<StaticCamera>(camera));
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    for (auto& camera : m_cameras) {
        camera->m_aspectRatio = aspectRatio;
    }
}

bool Scene::SetCurrentCamera(size_t cameraId) {
    if (m_cameras.size() <= cameraId)
        return false;

    m_currCameraId = cameraId;
    return true;
}

void Scene::NextCamera() {
    if (!m_cameras.empty())
        m_currCameraId = (m_currCameraId + 1) % m_cameras.size();
}

void Scene::RenderStaticObjects(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState, Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView, D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView, bool isLH) {
    if (m_cameras.empty())
        return;

    DirectX::XMMATRIX viewProjection{
        isLH ? m_cameras.at(m_currCameraId)->GetViewProjectionMatrixLH()
        : m_cameras.at(m_currCameraId)->GetViewProjectionMatrixRH()
    };

    for (auto const& obj : m_staticObjects) {
        obj->Render(
            pCommandListDirect,
            pPipelineState,
            pRootSignature,
            viewport,
            scissorRect,
            renderTargetView,
            depthStencilView,
            viewProjection
        );
    }
}
