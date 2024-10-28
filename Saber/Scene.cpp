#include "Scene.h"

Scene::Scene(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<DepthBuffer> pDepthBuffer,
    std::shared_ptr<GBuffer> pGBuffer
) : m_pDepthBuffer(pDepthBuffer), m_pGBuffer(pGBuffer) , m_pAllocator(pAllocator){
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

/* scene readiness */
void Scene::SetSceneReadiness(bool value) {
    m_isSceneReady.store(value);
}
bool Scene::IsSceneReady() {
    return m_isSceneReady.load();
}

/* depth buffer */
void Scene::SetDepthBuffer(std::shared_ptr<DepthBuffer> pDepthBuffer) {
    m_pDepthBuffer = pDepthBuffer;
}
std::shared_ptr<DepthBuffer> Scene::GetDepthBuffer() {
    return m_pDepthBuffer;
}

/* g-buffer */
void Scene::SetGBuffer(std::shared_ptr<GBuffer> pGBuffer) {
    m_pGBuffer = pGBuffer;
}
std::shared_ptr<GBuffer> Scene::GetGBuffer() {
    return m_pGBuffer;
}

void Scene::Update(float deltaTime) {
    TryUpdateCamera(deltaTime);
}

void Scene::AddCamera(const std::shared_ptr<Camera>&& pCamera) {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    m_pCameras.push_back(pCamera);
    lock.unlock();

    if (!m_sceneCBDynamicAllocation.pBuffer) {
        m_isUpdateSceneCB.store(true);
    }
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    for (auto& camera : m_pCameras) {
        camera->SetAspectRatio(aspectRatio);
    }
    m_isUpdateSceneCB.store(true);
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

void Scene::AddStaticObject(const MeshRenderObject& object) {
    std::scoped_lock<std::mutex> lock(m_staticObjectsMutex);
    m_pStaticObjects.push_back(std::make_shared<MeshRenderObject>(object));
}
void Scene::AddDynamicObject(const MeshRenderObject& object) {
    std::scoped_lock<std::mutex> lock(m_dynamicObjectsMutex);
    m_pDynamicObjects.push_back(std::make_shared<MeshRenderObject>(object));
}
void Scene::AddAlphaObject(const MeshRenderObject& object) {
    std::scoped_lock<std::mutex> lock(m_alphaObjectsMutex);
    m_pAlphaObjects.push_back(std::make_shared<MeshRenderObject>(object));
}

void Scene::RenderStaticObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
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
    if (!m_useIndirectDraw)
    {
        for (const auto& obj : m_pStaticObjects) {
            obj->Render(
                pCommandListDirect,
                viewport,
                scissorRect,
                rtvs.data(),
                rtvs.size(),
                &m_pDepthBuffer->GetDsvCpuDescHandle(),
                outerRootParametersSetter
            );
        }
    }
    else
    {
        if (m_pStaticObjects.size())
        {
            //now for test create command buffer for existind only objects
            if (!m_pStaticObjectsICB)
            {
                m_pStaticObjectsICB = std::make_shared<StaticObjectsICB>(m_pAllocator, m_pStaticObjects.size());
            }
            auto memSize = m_pStaticObjectsICB->GetMemSize();
            auto intermediateBuffer = m_pCameraHeap->Allocate(memSize);
            std::vector<SimpleIndirectCommand> commandBuffer = m_pStaticObjectsICB->GetCpuBuffer();

            //... here set common
            {
                m_pStaticObjects[0]->SetPsoRs(pCommandListDirect);

                pCommandListDirect->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                pCommandListDirect->RSSetViewports(1, &viewport);
                pCommandListDirect->RSSetScissorRects(1, &scissorRect);

                pCommandListDirect->OMSetRenderTargets(static_cast<UINT>(rtvs.size()), rtvs.data(), FALSE, &m_pDepthBuffer->GetDsvCpuDescHandle());
            }
            //... here fill data
            for(const auto& obj : m_pStaticObjects)
            {
                commandBuffer.emplace_back(obj->IndirectCommandParameters());
            }


            D3D12_SUBRESOURCE_DATA subresourceData{
               .pData{ commandBuffer.data() },
               .RowPitch{ static_cast<LONG_PTR>(memSize) },
               .SlicePitch{ subresourceData.RowPitch }
            };


            UpdateSubresources(
                pCommandListDirect.Get(),
                m_pStaticObjectsICB->GetResource().Get(),
                intermediateBuffer.pBuffer.Get(),
                intermediateBuffer.offset,
                0,
                1,
                &subresourceData
            );


        }

    }
}

void Scene::RenderDynamicObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
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
            &m_pDepthBuffer->GetDsvCpuDescHandle(),
            outerRootParametersSetter
        );
    }
}

void Scene::RenderAlphaObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
    std::shared_ptr<MaterialManager> pMaterialManager
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
            pCommandListDirect->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
            pCommandListDirect->SetGraphicsRootDescriptorTable(2, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
            pCommandListDirect->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
        };

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    if (m_pGBuffer) {
        rtvs = m_pGBuffer->GetRtvs();
    }
    else {
        rtvs.push_back(renderTargetView);
    }

    std::scoped_lock<std::mutex> alphaObjectsLock(m_alphaObjectsMutex);
    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    std::scoped_lock<std::mutex> lightCBMutex(m_lightBufferMutex);
    for (const auto& obj : m_pAlphaObjects) {
        obj->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            rtvs.data(),
            rtvs.size(),
            &m_pDepthBuffer->GetDsvCpuDescHandle(),
            outerRootParametersSetter
        );
    }
}

void Scene::SetDeferredShadingComputeObject(std::shared_ptr<ComputeObject> pDeferredShadingCO) {
    m_pDeferredShadingComputeObject = pDeferredShadingCO;
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

    constexpr int block_size = 8;

    m_pDeferredShadingComputeObject->Dispatch(
        pCommandListCompute,
        (width + block_size - 1) / block_size,
        (height + block_size - 1) / block_size,
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
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, m_pDepthBuffer->GetSrvGpuDescHandle());
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
            pCommandListCompute->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
        }
    );
}

void Scene::SetPostProcessing(std::shared_ptr<PostProcessing> pPostProcessing) {
    m_pPostProcessing = pPostProcessing;
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

    m_pPostProcessing->Render(
        pCommandListDirect,
        viewport,
        scissorRect,
        &renderTargetView,
        1,
        [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, UINT& rootParamId) {
            pCommandListDirect->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
            pCommandListDirect->SetGraphicsRootDescriptorTable(rootParamId++, m_pGBuffer->GetSrvDescHandle(m_pGBuffer->GetSize() - 1));
        }
    );
}

void Scene::UpdateCameraHeap(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
    if (!m_isUpdateCameraHeap.load()) {
        return;
    }
    m_pCameraHeap->FinishFrame(fenceValue, lastCompletedFenceValue);
    m_isUpdateCameraHeap.store(false);
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

void Scene::UpdateSceneBuffer() {
    bool expected{ true };
    if (m_isUpdateSceneCB.compare_exchange_strong(expected, false)) {
        m_isUpdateCameraHeap.store(true);

        std::scoped_lock<std::mutex> sceneBufferMutexLock(m_sceneBufferMutex);
        std::scoped_lock<std::mutex> camerasMutexLock(m_camerasMutex);

        std::shared_ptr<Camera> pCamera{ m_pCameras.at(m_currCameraId) };

        m_sceneBuffer.viewProjMatrix = pCamera->GetViewProjectionMatrix();
        m_sceneBuffer.invViewProjMatrix = DirectX::XMMatrixInverse(nullptr, m_sceneBuffer.viewProjMatrix);
        

        DirectX::XMFLOAT3 cameraPosition{ pCamera->GetPosition() };
        m_sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };
        m_sceneBuffer.nearFar = { pCamera->m_near, pCamera->m_far, 0.f, 0.f };

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
