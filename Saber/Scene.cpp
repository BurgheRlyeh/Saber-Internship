#include "Scene.h"

Scene::Scene(
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
    std::shared_ptr<DepthBuffer> pDepthBuffer,
    std::shared_ptr<GBuffer> pGBuffer
) : m_pDynamicUploadHeap(pDynamicUploadHeap), m_pDepthBuffer(pDepthBuffer), m_pGBuffer(pGBuffer) {
    m_pStaticRenderSubsystem = std::make_shared<RenderSubsystem>();
    m_pDynamicRenderSubsystem = std::make_shared<RenderSubsystem>();
    m_pAlphaRenderSubsystem = std::make_shared<RenderSubsystem>();

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
}

void Scene::UpdateCamerasAspectRatio(float aspectRatio) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    for (auto& camera : m_pCameras) {
        camera->SetAspectRatio(aspectRatio);
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

bool Scene::SetCurrentCamera(size_t cameraId) {
    if (std::unique_lock<std::mutex> lock(m_camerasMutex); m_pCameras.size() <= cameraId)
        return false;

    m_currCameraId = cameraId;
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

void Scene::AddStaticObject(std::shared_ptr<RenderObject> pObject) {
    m_pStaticRenderSubsystem->Add(pObject);
}
void Scene::AddDynamicObject(std::shared_ptr<RenderObject> pObject) {
    m_pDynamicRenderSubsystem->Add(pObject);
}
void Scene::AddAlphaObject(std::shared_ptr<RenderObject> pObject) {
    m_pAlphaRenderSubsystem->Add(pObject);
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

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pStaticRenderSubsystem->Render(
        pCommandListDirect,
        viewport,
        scissorRect,
        rtvs.data(),
        rtvs.size(),
        &m_pDepthBuffer->GetDsvCpuDescHandle(),
        outerRootParametersSetter
    );
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

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pDynamicRenderSubsystem->Render(
        pCommandListDirect,
        viewport,
        scissorRect,
        rtvs.data(),
        rtvs.size(),
        &m_pDepthBuffer->GetDsvCpuDescHandle(),
        outerRootParametersSetter
    );
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

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pAlphaRenderSubsystem->Render(
        pCommandListDirect,
        viewport,
        scissorRect,
        rtvs.data(),
        rtvs.size(),
        &m_pDepthBuffer->GetDsvCpuDescHandle(),
        outerRootParametersSetter
    );
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
    UpdateLightBuffer();

    std::scoped_lock<std::mutex> lightCBMutex(m_lightBufferMutex);

    constexpr int block_size{ 8 };
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
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
) {
    if (!m_pPostProcessing || !m_pGBuffer) {
        return;
    }

	// prepare command list
	UINT rootParameterIndex{};
	{
		m_pPostProcessing->SetPipelineStateAndRootSignature(pCommandList);

		pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		pCommandList->RSSetViewports(1, &viewport);
		pCommandList->RSSetScissorRects(1, &scissorRect);

		pCommandList->OMSetRenderTargets(1, &renderTargetView, TRUE, nullptr);

		pCommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
		pCommandList->SetGraphicsRootDescriptorTable(
            rootParameterIndex++,
			m_pGBuffer->GetSrvDescHandle(m_pGBuffer->GetSize() - 1)
		);
    }

    m_pPostProcessing->Render(pCommandList, rootParameterIndex);
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
    return true;
}

void Scene::UpdateSceneBuffer() {
    std::scoped_lock<std::mutex> sceneBufferMutexLock(m_sceneBufferMutex);
    std::scoped_lock<std::mutex> camerasMutexLock(m_camerasMutex);

    std::shared_ptr<Camera> pCamera{ m_pCameras.at(m_currCameraId) };

    m_sceneBuffer.viewProjMatrix = pCamera->GetViewProjectionMatrix();
    m_sceneBuffer.invViewProjMatrix = DirectX::XMMatrixInverse(nullptr, m_sceneBuffer.viewProjMatrix);


    DirectX::XMFLOAT3 cameraPosition{ pCamera->GetPosition() };
    m_sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };
    m_sceneBuffer.nearFar = { pCamera->m_near, pCamera->m_far, 0.f, 0.f };

    m_sceneCBDynamicAllocation = m_pDynamicUploadHeap->Allocate(sizeof(SceneBuffer));

    memcpy(m_sceneCBDynamicAllocation.cpuAddress, &m_sceneBuffer, sizeof(SceneBuffer));
}

void Scene::UpdateLightBuffer() {
    bool expected{ true };
    if (m_isUpdateLightCB.compare_exchange_strong(expected, false)) {
        std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
        m_pLightCB->Update(&m_lightBuffer);
    }
}
