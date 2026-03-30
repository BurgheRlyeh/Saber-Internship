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
#include "MaterialManager.h"
#include "RenderObject.h"
#include "RenderSubsystem.h"
#include "Texture.h"

Scene::Scene(
    const std::wstring& name,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<DepthBuffer> pDepthBuffer,
    std::shared_ptr<GBuffer> pGBuffer
) : m_name(name),
m_pDepthBuffer(pDepthBuffer),
m_pGBuffer(pGBuffer)
{
    for (size_t i{}; i < static_cast<size_t>(RenderSubsystemType::Count); ++i) {
        m_pRenderSubsystems[i] = std::make_shared<RenderSubsystem<ConstMesh4IndirectCommand>>(
            m_name + L"/RenderSubsystem" + std::to_wstring(i + 1)
        );
    }

    m_pSceneCb = std::make_shared<Buffer<SceneBuffer>>(
        m_name + L"/SceneCb",
        pDeviceContext,
        1,
        GPUResource::AllocationDesc{},
        GPUResource::ResourceDesc{
            CD3DX12_RESOURCE_DESC::Buffer(0),
            D3D12_RESOURCE_STATE_GENERIC_READ
        },
        EnumFlags<ResourceView>{ ResourceView::None }
    );
    m_pSceneCb->CreateStorage<WholeBufferStorage<SceneBuffer>>();
    m_pSceneCb->CreateUpdater<WholeBufferUpdater<SceneBuffer>>();

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
            type & RenderSubsystemType::Dynamic ? pIndirectUpdater : nullptr
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

void Scene::Update(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    float deltaTime
) {
    UpdateCamera(deltaTime);
    UpdateSceneBuffer(pDeviceContext, pCommandList);
}

void Scene::BeforeFrameJob(std::shared_ptr<CommandList> pCommandList) {
    m_pDepthBuffer->Clear(pCommandList);
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
    ProjectionType& projectionType{ m_pCameras.at(m_currCameraId)->m_projectionType };
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
            &m_pDepthBuffer->GetDsvCpuDescHandle()
        );

        pD3D12CommandList->SetGraphicsRootConstantBufferView(
            0,
            m_pSceneCb->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
        );
        pD3D12CommandList->SetDescriptorHeaps(1, pDeviceContext->GetDescriptorHeap()->GetDescriptorHeap().GetAddressOf());
        if (type & RenderSubsystemType::AlphaKill) {
            const auto& pMaterialManager{ pDeviceContext->GetMaterialManager() };
            pD3D12CommandList->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
            pD3D12CommandList->SetGraphicsRootDescriptorTable(4, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
        }
        };

    std::scoped_lock<std::mutex> sceneCBMutex(m_sceneBufferMutex);
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
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
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
                m_pSceneCb->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
            );
            pD3D12CommandList->SetComputeRootConstantBufferView(
                rootParamId++,
                m_pLightCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
            );
            pD3D12CommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pGBuffer->GetSrvDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pTargetTexture->GetUavDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pDepthBuffer->GetSrvGpuDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialCBVsRange()->GetGpuHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialSRVsRange()->GetGpuHandle());
        }
    );
}

void Scene::SetPostProcessing(std::shared_ptr<RenderObject> pPostProcessing) {
    m_pPostProcessing = pPostProcessing;
}

void Scene::RenderPostProcessing(
    std::shared_ptr<CommandList> pCommandList,
    std::shared_ptr<DescriptorHeapManager> pResDescHeapManager,
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

        pD3D12CommandList->SetDescriptorHeaps(1, pResDescHeapManager->GetDescriptorHeap().GetAddressOf());
        pD3D12CommandList->SetGraphicsRootDescriptorTable(
            rootParameterIndex++,
            m_pTargetTexture->GetSrvDescHandle()
        );
    }

    m_pPostProcessing->Render(pCommandList, rootParameterIndex);
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

void Scene::UpdateSceneBuffer(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList
) {
    std::unique_lock<std::mutex> camerasMutexLock(m_camerasMutex);

    std::shared_ptr<Camera> pCamera{ m_pCameras.at(m_currCameraId) };
    DirectX::XMFLOAT3 cameraPosition{ pCamera->GetPosition() };

    std::unique_lock<std::mutex> sceneBufferMutexLock(m_sceneBufferMutex);

    m_sceneBuffer.viewProjMatrix = pCamera->GetViewProjectionMatrix();
    m_sceneBuffer.invViewProjMatrix = DirectX::XMMatrixInverse(nullptr, m_sceneBuffer.viewProjMatrix);
    m_sceneBuffer.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.f };
    m_sceneBuffer.nearFar = { pCamera->m_near, pCamera->m_far, 0.f, 0.f };

    camerasMutexLock.unlock();

    m_pSceneCb->UpdateAll(&m_sceneBuffer, 1);
    sceneBufferMutexLock.unlock();

    m_pSceneCb->PerformUpdate(pDeviceContext, pCommandList);
}

void Scene::UpdateLightBuffer() {
    bool expected{ true };
    if (m_isUpdateLightCB.compare_exchange_strong(expected, false)) {
        std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
        m_pLightCB->UpdateAll(&m_lightBuffer, 1);
    }
}
