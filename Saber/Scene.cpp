#include "Scene.h"
#include "Scene.h"
#include "Scene.h"

Scene::Scene(
    const std::wstring& name,
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeapCpu,
    std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeapGpu,
    std::shared_ptr<DepthBuffer> pDepthBuffer,
    std::shared_ptr<GBuffer> pGBuffer
) : m_name(name),
	m_pDynamicUploadHeapCpu(pDynamicUploadHeapCpu),
	m_pDynamicUploadHeapGpu(pDynamicUploadHeapGpu),
	m_pDepthBuffer(pDepthBuffer),
	m_pGBuffer(pGBuffer)
{
    m_pRenderSubsystems.resize(RenderSubsystemId::Count);
    m_pRenderSubsystems[RenderSubsystemId::Static] =
        std::make_shared<RenderSubsystem<CbMesh4IndirectCommand>>(m_name + L"/Static");
    m_pRenderSubsystems[RenderSubsystemId::Dynamic] =
        std::make_shared<RenderSubsystem<CbMesh4IndirectCommand>>(m_name + L"/Dynamic");
    m_pRenderSubsystems[RenderSubsystemId::StaticAlphaKill] =
        std::make_shared<RenderSubsystem<CbMesh4IndirectCommand>>(m_name + L"/Static/AlphaKill");
    m_pRenderSubsystems[RenderSubsystemId::DynamicAlphaKill] =
        std::make_shared<RenderSubsystem<CbMesh4IndirectCommand>>(m_name + L"/Dynamic/AlphaKill");
	
    m_pSceneCb = std::make_shared<ConstantBuffer>(
        pAllocator,
        sizeof(SceneBuffer),
        nullptr,
        GPUResource::HeapData{ D3D12_HEAP_TYPE_DEFAULT }
    );
    m_pLightCB = std::make_shared<ConstantBuffer>(
        pAllocator,
        sizeof(LightBuffer)
    );
    m_lightBuffer.SetAmbientLight({ .5f, .5f, .5f }, 1.f);
}

void Scene::InitializeRenderSubsystems(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
	std::shared_ptr<DynamicUploadHeap> pDynamicUploadHeap,
    std::shared_ptr<ComputeObject> pIndirectUpdater
) const {
	for (size_t i{}; i < RenderSubsystemId::Count; ++i) {
		m_pRenderSubsystems[i]->InitializeIndirectCommandBuffer(
			pDevice,
			pAllocator,
			pDescHeapManagerCbvSrvUav,
			pDynamicUploadHeap,
			i == Static || i == StaticAlphaKill ? nullptr : pIndirectUpdater
		);
	}
}

void Scene::UpdateRenderSubsystems(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
    std::shared_ptr<CommandQueue> pCommandQueueCopy,
	std::shared_ptr<CommandQueue> pCommandQueueDirect
) const {
	for (auto& pRenderSubsystem : m_pRenderSubsystems) {
		pRenderSubsystem->PerformIndirectBufferUpdate(
			pDevice,
			pAllocator,
			pCommandQueueCopy,
			pCommandQueueDirect
		);
	}
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

void Scene::Update(float deltaTime, std::shared_ptr<CommandList> pCommandList) {
    TryUpdateCamera(deltaTime);
    UpdateSceneBuffer(pCommandList->m_pCommandList);
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
    m_lightBuffer.SetAmbientLight(color, power);
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
    bool result{ m_lightBuffer.Add(
        position,
        diffuseColor,
        diffusePower,
        specularColor,
        specularPower
    ) };

    if (result) {
        m_isUpdateLightCB.store(true);
    }
    return result;
}

void Scene::AddStaticObject(std::shared_ptr<RenderObject> pObject) const {
    m_pRenderSubsystems[Static]->Add(pObject);
}
void Scene::AddDynamicObject(std::shared_ptr<RenderObject> pObject) const {
    m_pRenderSubsystems[Dynamic]->Add(pObject);
}
void Scene::AddStaticAlphaKillObject(std::shared_ptr<RenderObject> pObject) const {
    m_pRenderSubsystems[StaticAlphaKill]->Add(pObject);
}
void Scene::AddDynamicAlphaKillObject(std::shared_ptr<RenderObject> pObject) const {
    m_pRenderSubsystems[DynamicAlphaKill]->Add(pObject);
}

void Scene::RenderStaticObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    if (m_pGBuffer) {
        rtvs = m_pGBuffer->GetRtvs();
    }
    else {
        rtvs.push_back(renderTargetView);
    }

    auto commandListPrepare = [&] {
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->RSSetViewports(1, &viewport);
        pCommandList->RSSetScissorRects(1, &scissorRect);

        pCommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvs.size()),
            rtvs.data(),
            FALSE,
            &m_pDepthBuffer->GetDsvCpuDescHandle()
        );

        pCommandList->SetGraphicsRootConstantBufferView(
            0,
            m_sceneCBDynamicAllocation.gpuAddress
        );
    };

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pRenderSubsystems[Static]->Render(
        pCommandList,
        commandListPrepare
    );
}

void Scene::RenderDynamicObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    if (m_pGBuffer) {
        rtvs = m_pGBuffer->GetRtvs();
    }
    else {
        rtvs.push_back(renderTargetView);
    }

    auto commandListPrepare = [&] {
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->RSSetViewports(1, &viewport);
        pCommandList->RSSetScissorRects(1, &scissorRect);

        pCommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvs.size()),
            rtvs.data(),
            FALSE,
            &m_pDepthBuffer->GetDsvCpuDescHandle()
        );

        pCommandList->SetGraphicsRootConstantBufferView(
            0,
            m_sceneCBDynamicAllocation.gpuAddress
        );
        };

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pRenderSubsystems[Dynamic]->Render(
        pCommandList,
        commandListPrepare
    );
}

void Scene::RenderStaticAlphaKillObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
    std::shared_ptr<MaterialManager> pMaterialManager
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    if (m_pGBuffer) {
        rtvs = m_pGBuffer->GetRtvs();
    }
    else {
        rtvs.push_back(renderTargetView);
    }

    auto commandListPrepare = [&] {
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->RSSetViewports(1, &viewport);
        pCommandList->RSSetScissorRects(1, &scissorRect);

        pCommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvs.size()),
            rtvs.data(),
            FALSE,
            &m_pDepthBuffer->GetDsvCpuDescHandle()
        );

        pCommandList->SetGraphicsRootConstantBufferView(
            0,
            m_sceneCBDynamicAllocation.gpuAddress
        );
        pCommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
        pCommandList->SetGraphicsRootDescriptorTable(2, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
        pCommandList->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
        };

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pRenderSubsystems[StaticAlphaKill]->Render(
        pCommandList,
        commandListPrepare
    );
}

void Scene::RenderDynamicAlphaKillObjects(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
    std::shared_ptr<MaterialManager> pMaterialManager
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    if (m_pGBuffer) {
        rtvs = m_pGBuffer->GetRtvs();
    }
    else {
        rtvs.push_back(renderTargetView);
    }

    auto commandListPrepare = [&] {
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->RSSetViewports(1, &viewport);
        pCommandList->RSSetScissorRects(1, &scissorRect);

        pCommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvs.size()),
            rtvs.data(),
            FALSE,
            &m_pDepthBuffer->GetDsvCpuDescHandle()
        );

        pCommandList->SetGraphicsRootConstantBufferView(
            0,
            m_sceneCBDynamicAllocation.gpuAddress
        );
        pCommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
        pCommandList->SetGraphicsRootDescriptorTable(2, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
        pCommandList->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
        };

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
    m_pRenderSubsystems[DynamicAlphaKill]->Render(
        pCommandList,
        commandListPrepare
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

void Scene::UpdateSceneBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList) {
    std::scoped_lock<std::mutex> sceneBufferMutexLock(m_sceneBufferMutex);
    std::scoped_lock<std::mutex> camerasMutexLock(m_camerasMutex);

    std::shared_ptr<Camera> pCamera{ m_pCameras.at(m_currCameraId) };

    m_sceneBuffer.viewProjMatrix = pCamera->GetViewProjectionMatrix();
    m_sceneBuffer.invViewProjMatrix = DirectX::XMMatrixInverse(nullptr, m_sceneBuffer.viewProjMatrix);


    DirectX::XMFLOAT3 cameraPosition{ pCamera->GetPosition() };
    m_sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };
    m_sceneBuffer.nearFar = { pCamera->m_near, pCamera->m_far, 0.f, 0.f };

    DynamicAllocation cpuAlloc = m_pDynamicUploadHeapCpu->Allocate(sizeof(SceneBuffer));

    memcpy(cpuAlloc.cpuAddress, &m_sceneBuffer, sizeof(SceneBuffer));

    m_sceneCBDynamicAllocation = m_pDynamicUploadHeapGpu->Allocate(sizeof(SceneBuffer));
    ResourceTransition(
        pCommandList,
        m_sceneCBDynamicAllocation.pBuffer->GetResource(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    pCommandList->CopyBufferRegion(
        m_sceneCBDynamicAllocation.pBuffer->GetResource().Get(),
        m_sceneCBDynamicAllocation.offset,
        cpuAlloc.pBuffer->GetResource().Get(),
        cpuAlloc.offset,
        sizeof(SceneBuffer)
    );
    ResourceTransition(
        pCommandList,
        m_sceneCBDynamicAllocation.pBuffer->GetResource(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
}

void Scene::UpdateLightBuffer() {
    bool expected{ true };
    if (m_isUpdateLightCB.compare_exchange_strong(expected, false)) {
        std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
        m_pLightCB->Update(&m_lightBuffer);
    }
}
