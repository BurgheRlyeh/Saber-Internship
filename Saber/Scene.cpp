#include "Scene.h"

#include <functional>

#include "Buffer.h"
#include "Camera.h"
#include "CommandList.h"
#include "ComputeObject.h"
#include "DepthBuffer.h"
#include "DescriptorHeapManager.h"
#include "Device.h"
#include "DeviceContext.h"
#include "DirectionalLight.h"
#include "MaterialManager.h"
#include "MeshRenderObject.h"
#include "RenderObject.h"
#include "RenderSubsystem.h"
#include "Texture.h"

#include "imgui.h"

Scene::Scene(
    const std::wstring& name,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<HiDepthBuffer> pDepthTarget,
    std::shared_ptr<GBuffer> pGBuffer
) : m_name(name),
    m_pDepthTarget(pDepthTarget),
    m_pGBuffer(pGBuffer)
{
    for (size_t i{}; i < static_cast<size_t>(RenderSubsystemType::Count); ++i) {
        m_pRenderSubsystems[i] = std::make_shared<RenderSubsystem<ConstMesh4IndirectCommand>>(
            m_name + L"/RenderSubsystem" + std::to_wstring(i + 1)
        );
    }

    m_pCameraCB = std::make_shared<Buffer<CameraBuffer>>(
        m_name + L"/CameraCB",
        pDeviceContext,
        1,
        GPUResource::AllocationDesc{},
        GPUResource::ResourceDesc{
            CD3DX12_RESOURCE_DESC::Buffer(0),
            D3D12_RESOURCE_STATE_GENERIC_READ
        },
        EnumFlags<ResourceView>{ ResourceView::None }
    );
    m_pCameraCB->CreateStorage<WholeBufferStorage<CameraBuffer>>();
    m_pCameraCB->CreateUpdater<WholeBufferUpdater<CameraBuffer>>();

    m_lightBuffer.SetAmbientLight({ .5f, .5f, .5f }, 1.f);
    m_pLightCB = CreateUploadBufferWithUpdater<LightBuffer>(
        m_name + L"/LightCB",
        pDeviceContext
    );
    m_pLightCB->UpdateAll(&m_lightBuffer, 1);

    m_pTargetTexture = std::make_shared<Texture>(
        m_name + L"/TargetTexture",
        pDeviceContext,
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            pGBuffer->GetWidth(), pGBuffer->GetHeight(), 1, 0, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        ),
        1
	);

    m_pLightVolumeTarget = std::make_shared<Texture>(
		m_name + L"/LightVolumeTarget",
		pDeviceContext,
		CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16_FLOAT,
			pGBuffer->GetWidth(), pGBuffer->GetHeight(), 1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        ),
		1
	);

    m_pDirLight = std::make_shared<DirectionalLight>();

    m_pShadowMap = std::make_shared<DepthBuffer>(
        m_name + L"/ShadowMap",
        pDeviceContext,
        m_shadowMapResolution, m_shadowMapResolution,
        DXGI_FORMAT_D32_FLOAT
    );
    m_pShadowCameraCB = CreateUploadBufferWithUpdater<CameraBuffer>(
        m_name + L"/ShadowCameraCB", pDeviceContext
    );
    m_pShadowCameraCB->CreateStorage<WholeBufferStorage<CameraBuffer>>();
}

void Scene::Resize(
    std::shared_ptr<Device> pDevice,
    uint64_t width, uint32_t height
) {
    m_pTargetTexture->Resize(pDevice, width, height);
    UpdateCamerasAspectRatio(static_cast<float>(width) / height);
}

void Scene::InitializeRenderSubsystems(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<ComputeObject> pIndirectUpdater
) const {
    for (size_t i{}; i < static_cast<size_t>(RenderSubsystemType::Count); ++i) {
        RenderSubsystemType type{ FromId<RenderSubsystemType>(i) };
        m_pRenderSubsystems[i]->InitializeIndirectCommandBuffer(
            pDeviceContext,
            // TODO: those weird bug with indirect updater is still here!
            // seems to occur because of states during multithread command lists filling (claude said so, needs to be checked)
            type & RenderSubsystemType::Dynamic ? /*pIndirectUpdater*/ nullptr : nullptr
        );
        m_pRenderSubsystems[i]->InitializeModelBuffer(
            pDeviceContext,
            nullptr
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
void Scene::SetDepthTarget(std::shared_ptr<HiDepthBuffer> pDepthTarget) {
    m_pDepthTarget = pDepthTarget;
}
std::shared_ptr<HiDepthBuffer> Scene::GetDepthTarget() {
    return m_pDepthTarget;
}

/* g-buffer */
void Scene::SetGBuffer(std::shared_ptr<GBuffer> pGBuffer) {
    m_pGBuffer = pGBuffer;
}
std::shared_ptr<GBuffer> Scene::GetGBuffer() {
    return m_pGBuffer;
}

void Scene::Update(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    float deltaTime
) {
    UpdateCamera(deltaTime);
    UpdateCameraBuffer(pDeviceContext, pCommandList);
    UpdateShadowCameraBuffer();
}

void Scene::BeforeFrameJob(std::shared_ptr<CommandList> pCommandList) {
    m_pDepthTarget->Clear(pCommandList);
    m_pShadowMap->Clear(pCommandList);

    m_pLightVolumeTarget->ChangeState(pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_pLightVolumeTarget->Clear(pCommandList);

    if (m_pGBuffer) {
        m_pGBuffer->ChangeState(pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_pGBuffer->Clear(pCommandList);
    }
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

bool Scene::MoveCamera(float forwardCoef, float rightCoef) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    DynamicCamera* pDynamicCamera{ dynamic_cast<DynamicCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pDynamicCamera) {
        return false;
    }

    pDynamicCamera->Move(forwardCoef, rightCoef, 0.f);

    m_isUpdateCamera.store(true);
    return true;
}

bool Scene::RotateCamera(float deltaTheta, float deltaPhi) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    DynamicCamera* pDynamicCamera{ dynamic_cast<DynamicCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pDynamicCamera) {
        return false;
    }

    pDynamicCamera->Rotate(deltaTheta, deltaPhi);
    m_isUpdateCamera.store(true);
    return true;
}

bool Scene::ZoomCamera(float delta) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    OrbitCamera* pOrbitCamera{ dynamic_cast<OrbitCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pOrbitCamera) {
        return false;
    }

    pOrbitCamera->Zoom(delta);
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

void Scene::SwitchCameraProjection() {
    std::unique_lock<std::mutex> lock(m_camerasMutex);
    ProjectionType& projectionType{ m_pCameras.at(m_currCameraId)->GetSettings().projectionType };
    projectionType = static_cast<ProjectionType>(!static_cast<size_t>(projectionType));
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

void Scene::AddObject(
    const EnumFlags<RenderSubsystemType> type,
    std::shared_ptr<RenderObject> pObject
) const {
    m_pRenderSubsystems[ToId(type)]->Add(pObject);
}
void Scene::RenderObjects(
    const EnumFlags<RenderSubsystemType> type,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    if (m_pRenderSubsystems[ToId(type)]->IsUpdatePending()) {
        m_pRenderSubsystems[ToId(type)]->PerformUpdate(pDeviceContext, pCommandList);
    }

    auto commandListPrepare = [&] {
        auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
        pD3D12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pD3D12CommandList->RSSetViewports(1, &viewport);
        pD3D12CommandList->RSSetScissorRects(1, &scissorRect);

        std::shared_ptr<Texture> rts{ m_pGBuffer ? m_pGBuffer : m_pTargetTexture };
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs{ rts->GetRtvs() };
        pD3D12CommandList->OMSetRenderTargets(
            static_cast<UINT>(rtvs.size()),
            rtvs.data(),
            FALSE,
            &m_pDepthTarget->GetDsvCpuDescHandle()
        );

        pD3D12CommandList->SetGraphicsRootConstantBufferView(
            0,
            m_pCameraCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
        );
        pD3D12CommandList->SetDescriptorHeaps(1, pDeviceContext->GetDescriptorHeap(DescRangeType::Srv)->GetD3D12DescriptorHeap().GetAddressOf());
        if (type & RenderSubsystemType::AlphaKill) {
            const auto& pMaterialManager{ pDeviceContext->GetMaterialManager() };
            pD3D12CommandList->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialCbvRange()->GetGpuHandle());
            pD3D12CommandList->SetGraphicsRootDescriptorTable(4, pMaterialManager->GetMaterialSrvRange()->GetGpuHandle());
        }
    };

    std::scoped_lock<std::mutex> sceneCBMutex(m_cameraBufferMutex);
    m_pRenderSubsystems[ToId(type)]->Render(
        pCommandList,
        commandListPrepare
    );
}

void Scene::InitLightVolumeGrid(
    std::shared_ptr<DeviceContext> pDeviceContext,
    const std::shared_ptr<CommandList>& pCommandList,
    const DirectX::XMMATRIX& modelMatrix
) {
    m_pLightVolumeGrid = TestLightVolumeRenderObject::CreateLightVolume(
        pDeviceContext,
        pCommandList,
        m_pLightVolumeTarget,
        modelMatrix
    );
}

void Scene::RenderLightVolumeGrid(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect
) {
    m_pLightVolumeGrid->SetPipelineStateAndRootSignature(pCommandList);

	if (m_pShadowMap->GetTexture()->GetState() != D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
        m_pShadowMap->GetTexture()->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }

    auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
	pD3D12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pD3D12CommandList->RSSetViewports(1, &CD3DX12_VIEWPORT(
		0.0f,
        0.0f,
		static_cast<float>(m_pLightVolumeTarget->GetWidth()),
		static_cast<float>(m_pLightVolumeTarget->GetHeight())
	));
	pD3D12CommandList->RSSetScissorRects(1, &scissorRect);

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs{ m_pLightVolumeTarget->GetRtvs() };
	pD3D12CommandList->OMSetRenderTargets(
		static_cast<UINT>(rtvs.size()),
		rtvs.data(),
		FALSE,
		nullptr
	);

	pD3D12CommandList->SetGraphicsRootConstantBufferView(
		0,
		m_pCameraCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
	);
	pD3D12CommandList->SetDescriptorHeaps(1, 
        pDeviceContext
            ->GetDescriptorHeap(DescRangeType::Srv)
            ->GetD3D12DescriptorHeap().GetAddressOf()
    );

    DirectX::XMMATRIX mm{ DirectX::XMMatrixInverse(nullptr, m_pDirLight->GetViewProjectionMatrix()) };
    pD3D12CommandList->SetGraphicsRoot32BitConstants(1, 16, &mm, 0);

    pD3D12CommandList->SetGraphicsRootDescriptorTable(2, m_pShadowMap->GetSrvGpuDescHandle());

    m_pLightVolumeGrid->Render(pCommandList, 0);
}

void Scene::RenderObjectsDepth(
	const EnumFlags<RenderSubsystemType> type,
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandList> pCommandList,
	D3D12_VIEWPORT viewport,
	D3D12_RECT scissorRect
) {
	if (m_pRenderSubsystems[ToId(type)]->IsUpdatePending()) {
		m_pRenderSubsystems[ToId(type)]->PerformUpdate(pDeviceContext, pCommandList);
	}

	auto commandListPrepare = [&] {
		auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
		pD3D12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		pD3D12CommandList->RSSetViewports(1, &CD3DX12_VIEWPORT(
            0.0f, 0.0f,
            static_cast<float>(m_shadowMapResolution), static_cast<float>(m_shadowMapResolution)
        ));
		pD3D12CommandList->RSSetScissorRects(1, &scissorRect);

		pD3D12CommandList->OMSetRenderTargets(
			0,
			nullptr,
			FALSE,
			&m_pShadowMap->GetDsvCpuDescHandle()
		);

		pD3D12CommandList->SetGraphicsRootConstantBufferView(
			0,
            m_pShadowCameraCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
		);
		pD3D12CommandList->SetDescriptorHeaps(1, pDeviceContext->GetDescriptorHeap(DescRangeType::Srv)->GetD3D12DescriptorHeap().GetAddressOf());
		if (type & RenderSubsystemType::AlphaKill) {
			const auto& pMaterialManager{ pDeviceContext->GetMaterialManager() };
			pD3D12CommandList->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialCbvRange()->GetGpuHandle());
			pD3D12CommandList->SetGraphicsRootDescriptorTable(4, pMaterialManager->GetMaterialSrvRange()->GetGpuHandle());
		}
		};

	std::scoped_lock<std::mutex> sceneCBMutex(m_cameraBufferMutex);
	m_pRenderSubsystems[ToId(type)]->Render(
		pCommandList,
		commandListPrepare
	);
}

void Scene::SetDeferredShadingComputeObject(std::shared_ptr<ComputeObject> pDeferredShadingCO) {
    m_pDeferredShadingComputeObject = pDeferredShadingCO;
}

void Scene::RunDeferredShading(
    std::shared_ptr<CommandList> pCommandListCompute,
    std::shared_ptr<DescriptorHeap> pResDescHeapManager,
    std::shared_ptr<MaterialManager> pMaterialManager,
    UINT width,
    UINT height
) {
    if (!m_pDeferredShadingComputeObject) {
        return;
    }

    m_pGBuffer->ChangeState(pCommandListCompute, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    m_pTargetTexture->ChangeState(pCommandListCompute, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    UpdateLightBuffer();

    std::scoped_lock<std::mutex> lightCBMutex(m_lightBufferMutex);

    constexpr int block_size{ 8 };
    m_pDeferredShadingComputeObject->Dispatch(
        pCommandListCompute,
        { (width + block_size - 1) / block_size, (height + block_size - 1) / block_size, 1 },
        [&](std::shared_ptr<CommandList> pCommandListCompute, UINT& rootParamId) {
            auto pD3D12CommandList{ pCommandListCompute->GetD3D12CommandList() };
            pD3D12CommandList->SetComputeRootConstantBufferView(
                rootParamId++,
                m_pCameraCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
            );
            pD3D12CommandList->SetComputeRootConstantBufferView(
                rootParamId++,
                m_pLightCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
            );
            pD3D12CommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetD3D12DescriptorHeap().GetAddressOf());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pGBuffer->GetSrvDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pTargetTexture->GetUavDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pDepthTarget->GetSrvGpuDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialCbvRange()->GetGpuHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialSrvRange()->GetGpuHandle());
        }
    );
}

void Scene::SetPostProcessing(std::shared_ptr<RenderObject> pPostProcessing) {
    m_pPostProcessing = pPostProcessing;
}

void Scene::RenderPostProcessing(
    std::shared_ptr<CommandList> pCommandList,
    std::shared_ptr<DescriptorHeap> pResDescHeapManager,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
) {
    if (!m_pPostProcessing) {
        return;
    }

    m_pTargetTexture->ChangeState(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

    // prepare command list
    UINT rootParameterIndex{};
    {
        auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };

        m_pPostProcessing->SetPipelineStateAndRootSignature(pCommandList);

        pD3D12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pD3D12CommandList->RSSetViewports(1, &viewport);
        pD3D12CommandList->RSSetScissorRects(1, &scissorRect);

        pD3D12CommandList->OMSetRenderTargets(1, &renderTargetView, TRUE, nullptr);

        pD3D12CommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetD3D12DescriptorHeap().GetAddressOf());
        pD3D12CommandList->SetGraphicsRootDescriptorTable(
            rootParameterIndex++,
            m_pTargetTexture->GetSrvDescHandle()
        );
    }

    m_pPostProcessing->Render(pCommandList, rootParameterIndex);
}

void Scene::SetLightVolumePostProcessing(std::shared_ptr<RenderObject> pPostProcessing) {
    m_pLightVolumePostProcessing = pPostProcessing;
}

void Scene::RenderLightVolumePostProcessing(
	std::shared_ptr<CommandList> pCommandList,
	std::shared_ptr<DescriptorHeap> pResDescHeapManager,
	D3D12_VIEWPORT viewport,
	D3D12_RECT scissorRect,
	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
) {
	if (!m_pLightVolumePostProcessing) {
		return;
	}


	m_pLightVolumeTarget->ChangeState(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

	// prepare command list
	UINT rootParameterIndex{};
	{
		auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };

        m_pLightVolumePostProcessing->SetPipelineStateAndRootSignature(pCommandList);

		pD3D12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		pD3D12CommandList->RSSetViewports(1, &viewport);
		pD3D12CommandList->RSSetScissorRects(1, &scissorRect);

		pD3D12CommandList->OMSetRenderTargets(1, &renderTargetView, TRUE, nullptr);

		pD3D12CommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetD3D12DescriptorHeap().GetAddressOf());
		pD3D12CommandList->SetGraphicsRootDescriptorTable(
			rootParameterIndex++,
            m_pLightVolumeTarget->GetSrvDescHandle()
		);

        pD3D12CommandList->SetGraphicsRoot32BitConstants(rootParameterIndex++, 1, &m_lightVolumeShadowCoef, 0);
	}

    m_pLightVolumePostProcessing->Render(pCommandList, rootParameterIndex);
}

bool Scene::UpdateCamera(float deltaTime) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    if (!m_isUpdateCamera.load()) {
        return false;
    }

    DynamicCamera* pDynamicCamera{ dynamic_cast<DynamicCamera*>(m_pCameras.at(m_currCameraId).get()) };
    if (!pDynamicCamera) {
        return false;
    }

    pDynamicCamera->Update(deltaTime);
    return true;
}

void Scene::UpdateCameraBuffer(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList
) {
    std::unique_lock<std::mutex> camerasMutexLock(m_camerasMutex);

    std::shared_ptr<Camera> pCamera{ m_pCameras.at(m_currCameraId) };
    DirectX::XMFLOAT3 cameraPosition{ pCamera->GetPosition() };

    std::unique_lock<std::mutex> sceneBufferMutexLock(m_cameraBufferMutex);

    CameraBuffer sceneBuffer{ *m_pCameraCB->GetStorageData() };
    sceneBuffer.viewProjMatrix = pCamera->GetViewProjectionMatrix();
    sceneBuffer.invViewProjMatrix = DirectX::XMMatrixInverse(nullptr, sceneBuffer.viewProjMatrix);
    sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };
    const Camera::Settings& cameraSettings{ pCamera->GetSettings() };
    sceneBuffer.nearFar = { cameraSettings.nearPlane, cameraSettings.farPlane, 0.f, 0.f };

    camerasMutexLock.unlock();

    m_pCameraCB->UpdateAll(&sceneBuffer, 1);
    sceneBufferMutexLock.unlock();

    m_pCameraCB->PerformUpdate(pDeviceContext, pCommandList);
}

void Scene::UpdateShadowCameraBuffer() {
    if (!m_pDirLight || !m_pShadowCameraCB) {
        return;
    }

    const Camera& lightCam{ m_pDirLight->GetShadowCamera() };

    CameraBuffer shadowCameraBuffer{ *m_pShadowCameraCB->GetStorageData() };
    shadowCameraBuffer.viewProjMatrix = m_pDirLight->GetViewProjectionMatrix();
    shadowCameraBuffer.invViewProjMatrix =
        DirectX::XMMatrixInverse(nullptr, shadowCameraBuffer.viewProjMatrix);

    DirectX::XMFLOAT3 lightPos{ lightCam.GetPosition() };
    shadowCameraBuffer.cameraPosition = { lightPos.x, lightPos.y, lightPos.z, 0.f };

    const Camera::Settings& s{ lightCam.GetSettings() };
    shadowCameraBuffer.nearFar = { s.nearPlane, s.farPlane, 0.f, 0.f };

    m_pShadowCameraCB->UpdateAll(&shadowCameraBuffer, 1);
}

void Scene::UpdateLightBuffer() {
    bool expected{ true };
    if (m_isUpdateLightCB.compare_exchange_strong(expected, false)) {
        std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
        m_pLightCB->UpdateAll(&m_lightBuffer, 1);
    }
}

// UI

void Scene::DrawCurrentCameraSettingsUI() {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    if (m_pCameras.empty()) {
        return;
    }
    DrawSettings(*m_pCameras.at(m_currCameraId));
}

void Scene::DrawDirectionalLightUI() {
	if (m_pDirLight) {
        ImGui::Begin("Directional Light");
		DrawSettings(*m_pDirLight);

		ImGui::Text("GPU handle = %p", m_pShadowMap->GetSrvGpuDescHandle().ptr);
		ImGui::Text(
            "size = %d x %d",
            m_pShadowMap->GetTexture()->GetWidth(),
            m_pShadowMap->GetTexture()->GetHeight()
        );
		// Note that we pass the GPU SRV handle here, *not* the CPU handle. We're passing the internal pointer value, cast to an ImTextureID
		ImGui::Image(
			(ImTextureID)m_pShadowMap->GetSrvGpuDescHandle().ptr,
			ImVec2((float)m_pShadowMap->GetTexture()->GetWidth() / 10.0f,
				    (float)m_pShadowMap->GetTexture()->GetHeight() / 10.0f)
		);

        ImGui::End();
    }
}

void Scene::DrawTestUI() {
    assert(m_pLightVolumeTarget);

    ImGui::Begin("Test");

    ImGui::SliderFloat("Shadow scale coef", &m_lightVolumeShadowCoef, 0.0f, 10.0f);

	ImGui::Text("GPU handle = %p", m_pLightVolumeTarget->GetSrvDescHandle().ptr);
	ImGui::Text(
		"size = %d x %d",
        m_pLightVolumeTarget->GetTexture()->GetWidth(),
        m_pLightVolumeTarget->GetTexture()->GetHeight()
	);
	ImGui::Image(
		(ImTextureID)m_pLightVolumeTarget->GetSrvDescHandle().ptr,
		ImVec2((float)m_pLightVolumeTarget->GetWidth() / 2.0f,
			    (float)m_pLightVolumeTarget->GetHeight() / 2.0f)
	);

    ImGui::End();
}

void Scene::DrawSettingsUI() {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);

    ImGui::Begin("Lights");

    bool changed{ false };

    ImGui::SeparatorText("Ambient");
    changed |= ImGui::ColorEdit3("Color##ambient", &m_lightBuffer.ambientColorAndPower.x);
    changed |= ImGui::SliderFloat("Power##ambient", &m_lightBuffer.ambientColorAndPower.w, 0.f, 10.f);

    ImGui::SeparatorText("Sources");

    uint32_t& count{ m_lightBuffer.lightsCount.x };
    ImGui::Text("Count: %u / %d", count, LIGHTS_MAX_COUNT);

    if (ImGui::Button("+")) {
        if (count < LIGHTS_MAX_COUNT) {
            m_lightBuffer.lights[count] = Light{
                .position{ 0.f, 0.f, 0.f, 1.f },
                .diffuseColorAndPower{ 1.f, 1.f, 1.f, 1.f },
                .specularColorAndPower{ 1.f, 1.f, 1.f, 1.f }
            };
            ++count;
            changed = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("-")) {
        if (count > 0) {
            --count;
            changed = true;
        }
    }

    for (uint32_t i{}; i < count; ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::TreeNode("source", "Light %u", i)) {
            Light& light{ m_lightBuffer.lights[i] };

            changed |= ImGui::DragFloat3("Position", &light.position.x, 0.1f, -100.f, 100.f);

            changed |= ImGui::ColorEdit3("Diffuse", &light.diffuseColorAndPower.x);
            changed |= ImGui::SliderFloat("Diffuse power", &light.diffuseColorAndPower.w, 0.f, 10.f);

            changed |= ImGui::ColorEdit3("Specular", &light.specularColorAndPower.x);
            changed |= ImGui::SliderFloat("Specular power", &light.specularColorAndPower.w, 0.f, 10.f);

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();

    if (changed) {
        m_isUpdateLightCB.store(true);
    }
}
