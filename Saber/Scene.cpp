#include "Scene.h"

Scene::Scene(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
    , Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
) {
    m_pCameraHeap = std::make_shared<DynamicUploadHeap>(
        pAllocator,
        2 * sizeof(SceneBuffer),
        true
    );

    m_pLightCB = std::make_shared<ConstantBuffer>(
        pAllocator,
        sizeof(LightBuffer)
    );
}

void Scene::SetSceneReadiness(bool value) {
    m_isSceneReady.store(value);
}

bool Scene::IsSceneReady() {
    return m_isSceneReady.load();
}

void Scene::AddStaticObject(const MeshRenderObject& object) {
    std::scoped_lock<std::mutex> lock(m_staticObjectsMutex);
    m_pStaticObjects.push_back(std::make_shared<MeshRenderObject>(object));
}

void Scene::AddDynamicObject(const MeshRenderObject& object) {
    std::scoped_lock<std::mutex> lock(m_dynamicObjectsMutex);
    m_pDynamicObjects.push_back(std::make_shared<MeshRenderObject>(object));
}

void Scene::AddCamera(const std::shared_ptr<Camera>&& pCamera) {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    m_pCameras.push_back(pCamera);
    lock.unlock();

    if (!m_sceneCBDynamicAllocation.pBuffer) {
        m_isUpdateSceneCB.store(true);
    }
}

void Scene::SetAmbientLight(
    const DirectX::XMFLOAT3& color,
    const float& power
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
    return true;
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    for (auto& camera : m_pCameras) {
        camera->SetAspectRatio(aspectRatio);
    }
    m_isUpdateSceneCB.store(true);
}

void Scene::Update(float deltaTime) {
    TryUpdateCamera(deltaTime);
}

bool Scene::SetCurrentCamera(size_t cameraId) {
    if (std::unique_lock<std::mutex> lock(m_camerasMutex); m_pCameras.size() <= cameraId)
        return false;

    m_currCameraId = cameraId;

    m_isUpdateSceneCB.store(true);

    return true;
}

void Scene::NextCamera() {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    if (!m_pCameras.empty()) {
        lock.unlock();
        SetCurrentCamera((m_currCameraId + 1) % m_pCameras.size());
    }
}

bool Scene::TryMoveCamera(float forwardCoef, float rightCoef) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    DynamicCamera* pSphereCamera{ dynamic_cast<DynamicCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pSphereCamera) {
        return false;
    }

    pSphereCamera->Move(forwardCoef, rightCoef);

    m_isUpdateCamera.store(true);
    return true;
}

bool Scene::TryRotateCamera(float deltaX, float deltaY) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    DynamicCamera* pSphereCamera{ dynamic_cast<DynamicCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pSphereCamera) {
        return false;
    }

    pSphereCamera->Rotate(deltaX, deltaY);

    m_isUpdateCamera.store(true);
    return true;
}

bool Scene::TryUpdateCamera(float deltaTime) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    if (!m_isUpdateCamera.load()) {
        return false;
    }

    DynamicCamera* pSphereCamera{ dynamic_cast<DynamicCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pSphereCamera) {
        return false;
    }

    pSphereCamera->Update(deltaTime);
    m_isUpdateSceneCB.store(true);
    return true;
}

void Scene::SetPostProcessing(std::shared_ptr<PostProcessing> pPostProcessing) {
    m_pPostProcessing = pPostProcessing;
}

DirectX::XMMATRIX Scene::GetViewProjectionMatrix() {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    return m_pCameras.at(m_currCameraId)->GetViewProjectionMatrix();
}

void Scene::RenderStaticObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    UpdateSceneBuffer();
    UpdateLightBuffer();

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

    std::scoped_lock<std::mutex> staticObjectsLock(m_staticObjectsMutex);
    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    std::scoped_lock<std::mutex> lightCBMutex(m_lightBufferMutex);
    for (const auto& obj : m_pStaticObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            rtvs.data(),
            rtvs.size(),
            &depthStencilView,
            outerRootParametersSetter
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
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    UpdateSceneBuffer();
    UpdateLightBuffer();

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

    std::scoped_lock<std::mutex> dynamicObjectsLock(m_dynamicObjectsMutex);
    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    std::scoped_lock<std::mutex> lightCBMutex(m_lightBufferMutex);
    for (const auto& obj : m_pDynamicObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            rtvs.data(),
            rtvs.size(),
            &depthStencilView,
            outerRootParametersSetter
        );
    }
}

void Scene::RenderPostProcessing(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
) {
    if (!m_pGBuffer) {
        return;
    }

    ResourceTransition(
        pCommandListDirect,
        m_pGBuffer->GetTexture(0)->GetResource(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    m_pPostProcessing->Render(
        pCommandListDirect,
        viewport,
        scissorRect,
        &renderTargetView,
        1,
        [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, UINT& rootParamId) {
            pCommandListDirect->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
            pCommandListDirect->SetGraphicsRootDescriptorTable(rootParamId++, m_pGBuffer->GetSrvDescHandle(4));
        }
    );
}

void Scene::InitDeferredShadingComputeObject(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas, std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas, std::shared_ptr<PSOLibrary> pPSOLibrary) {
    m_pDeferredShadingComputeObject = DeferredShading::CreateDefferedShadingComputeObject(
        pDevice,
        pShaderAtlas,
        pRootSignatureAtlas,
        pPSOLibrary
    );
}

void Scene::RunDeferredShading(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
    std::shared_ptr<MaterialManager> pMaterialManager,
    UINT width,
    UINT height
) {
    if (!m_pDeferredShadingComputeObject) {
        return;
    }

    m_pDeferredShadingComputeObject->Dispatch(
        pCommandListCompute,
        width,
        height,
        1,
        [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute, UINT& rootParamId) {
            pCommandListCompute->SetComputeRootConstantBufferView(
                rootParamId++,
                m_sceneCBDynamicAllocation.gpuAddress
            );
            pCommandListCompute->SetComputeRootConstantBufferView(
                rootParamId++,
                m_pLightCB->GetResource()->GetGPUVirtualAddress()
            );
            pCommandListCompute->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, m_pGBuffer->GetSrvDescHandle());
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, m_pGBuffer->GetUavDescHandle());
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
            //pCommandList
        }
    );
}

void Scene::SetGBuffer(std::shared_ptr<GBuffer> pGBuffer) {
    m_pGBuffer = pGBuffer;
}

void Scene::ClearGBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList) {
    m_pGBuffer->Clear(pCommandList);
}

std::shared_ptr<GBuffer> Scene::GetGBuffer() {
    return m_pGBuffer;
}

void Scene::UpdateCameraHeap(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
    if (!m_isUpdateCameraHeap.load()) {
        return;
    }
    m_pCameraHeap->FinishFrame(fenceValue, lastCompletedFenceValue);
    m_isUpdateCameraHeap.store(false);
}

void Scene::UpdateSceneBuffer() {
    bool expected{ true };
    if (m_isUpdateSceneCB.compare_exchange_strong(expected, false)) {
        m_isUpdateCameraHeap.store(true);

        std::scoped_lock<std::mutex> lock(m_sceneBufferMutex);
        m_sceneBuffer.viewProjMatrix = GetViewProjectionMatrix();

        DirectX::XMFLOAT3 cameraPosition{ m_pCameras.at(m_currCameraId)->GetPosition() };
        m_sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };

        m_sceneCBDynamicAllocation = m_pCameraHeap->Allocate(sizeof(SceneBuffer));

        memcpy(m_sceneCBDynamicAllocation.cpuAddress, &m_sceneBuffer, sizeof(SceneBuffer));
        //m_pSceneCB->Update(&m_sceneBuffer);
    }
}

void Scene::UpdateLightBuffer() {
    bool expected{ true };
    if (m_isUpdateLightCB.compare_exchange_strong(expected, false)) {
        std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
        m_pLightCB->Update(&m_lightBuffer);
    }
}
