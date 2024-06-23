#pragma once

#include "Headers.h"

#include "RenderObject.h"
#include "Camera.h"

// TODO: UNSAFE!
class Scene {
    std::vector<std::shared_ptr<RenderObject>> m_staticObjects{};
    //std::vector<std::shared_ptr<Object>> m_dynamicObjects{};
    std::vector<std::shared_ptr<StaticCamera>> m_cameras{};

    size_t m_currCameraId{};

public:
    void AddStaticObject(const RenderObject&& object);

    void AddCamera(const StaticCamera&& camera);

    void UpdateCamerasAspectRatio(float aspectRatio);

    bool SetCurrentCamera(size_t cameraId);

    void NextCamera();

    void RenderStaticObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView,
        bool isLH = false
    );
};