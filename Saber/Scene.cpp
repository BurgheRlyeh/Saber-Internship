#include "Scene.h"

#include <algorithm>
#include <functional>
#include <limits>

#include "Buffer.h"
#include "Camera.h"
#include "LightSource.h"
#include "CommandList.h"
#include "ComputeObject.h"
#include "DepthBuffer.h"
#include "DescriptorHeapRange.h"
#include "DescriptorHeapManager.h"
#include "Device.h"
#include "DeviceContext.h"
#include "DirectionalLight.h"
#include "MaterialManager.h"
#include "PointLight.h"
#include "SpotLight.h"
#include "RenderObject.h"
#include "RenderSubsystem.h"
#include "Texture.h"

Scene::Scene(
    const std::wstring& name,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<HiDepthBuffer> pDepthBuffer,
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

    m_pCameraCB = std::make_shared<Buffer<CameraBuffer>>(
        m_name + L"/CameraCb",
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

    m_pDebugCamera = std::make_shared<FlyCamera>();
    m_pDebugCamera->GetSettings().speed = 20.f;

    m_pShadowMap = std::make_shared<DepthBuffer>(
        m_name + L"/ShadowMap",
        pDeviceContext,
        ShadowMapResolution, ShadowMapResolution
    );
    m_pShadowCameraCB = CreateUploadBufferWithUpdater<CameraBuffer>(
        m_name + L"/ShadowCameraCB",
        pDeviceContext
    );

    m_pCullingCameraCB = CreateUploadBufferWithUpdater<CameraBuffer>(
        m_name + L"/CullingCameraCB",
        pDeviceContext
    );

    m_pCullingStats = std::make_shared<GPUResource>(
        m_name + L"/CullingStats",
        pDeviceContext->GetDevice(),
        GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
        GPUResource::ResourceDesc{
            .resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
                sizeof(SceneCullingStats),
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            ) },
            .resInitState{ D3D12_RESOURCE_STATE_UNORDERED_ACCESS }
        }
    );
    m_pCullingStatsReadback = std::make_unique<ReadbackBuffer<SceneCullingStats>>(
        m_name + L"/CullingStatsReadback",
        pDeviceContext->GetDevice(),
        CullingStatsSlots
    );

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
void Scene::SetDepthBuffer(std::shared_ptr<HiDepthBuffer> pDepthBuffer) {
    m_pDepthBuffer = pDepthBuffer;
}
std::shared_ptr<HiDepthBuffer> Scene::GetDepthBuffer() {
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
    UpdateCameraBuffer(pDeviceContext, pCommandList);
    UpdateSimulation(deltaTime);

    UpdateShadowCameraBuffer();

    UpdateRenderSubsystems(pDeviceContext, pCommandList);
}

void Scene::UpdateRenderSubsystems(
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList
) {
    for (const auto& pRenderSubsystem : m_pRenderSubsystems) {
        if (pRenderSubsystem && pRenderSubsystem->IsUpdatePending()) {
            pRenderSubsystem->PerformUpdate(pDeviceContext, pCommandList);
        }
    }
}

void Scene::UpdateSimulation(float deltaTime) {
    std::scoped_lock<std::mutex> lock(m_gameHooksMutex);
    if (!m_simulation) {
        return;
    }

    m_simulation(deltaTime, *this);
}

void Scene::BeforeFrameJob(std::shared_ptr<CommandList> pCommandList) {
    ResetCullingStats(pCommandList);

    m_pDepthBuffer->Clear(pCommandList);
    m_pShadowMap->Clear(pCommandList);
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
    m_pDebugCamera->SetAspectRatio(aspectRatio);
}

void Scene::SwitchDebugCamera() {
    const bool isActive{ !m_isDebugCameraActive.load() };
    m_isDebugCameraActive.store(isActive);

    if (!isActive) {
        return;
    }

    // Starts where the gameplay camera stands, so switching does not teleport the
    // view somewhere unrelated and leave one hunting for the scene
    std::shared_ptr<Camera> pCurrent{ GetCurrentCamera() };
    if (!pCurrent) {
        return;
    }

    const DirectX::XMFLOAT3 position{ pCurrent->GetPosition() };
    const DirectX::XMFLOAT3 direction{ pCurrent->GetViewDirection() };

    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    FlyCamera::Settings& settings{ m_pDebugCamera->GetSettings() };

    settings.position = position;
    settings.yaw = std::atan2f(direction.x, direction.z);
    settings.pitch = std::asinf(std::clamp(direction.y, -1.f, 1.f));
}

bool Scene::IsDebugCameraActive() const {
    return m_isDebugCameraActive.load();
}

std::shared_ptr<Camera> Scene::GetRenderCamera() {
    if (m_isDebugCameraActive.load()) {
        return m_pDebugCamera;
    }
    return GetCurrentCamera();
}

bool Scene::Move(float forwardCoef, float rightCoef) {
    // Ahead of the game hook: while flying, the keys steer the camera and must not
    // also roll the ball
    if (m_isDebugCameraActive.load()) {
        std::scoped_lock<std::mutex> lock(m_camerasMutex);
        m_pDebugCamera->Move(forwardCoef, rightCoef, 0.f);
        return true;
    }

    if (std::unique_lock<std::mutex> hooksLock(m_gameHooksMutex); m_movementHandler) {
        std::function<void(float, float)> handler{ m_movementHandler };
        hooksLock.unlock();

        handler(forwardCoef, rightCoef);
        return true;
    }

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
    if (m_isDebugCameraActive.load()) {
        std::scoped_lock<std::mutex> lock(m_camerasMutex);
        m_pDebugCamera->Rotate(deltaTheta, deltaPhi);
        return true;
    }

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
    // A fly camera has nothing to zoom: the wheel would only change its speed,
    // which is on a slider anyway
    if (m_isDebugCameraActive.load()) {
        return false;
    }

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
}
bool Scene::AddLight(const std::shared_ptr<LightSource>& pLight) {
    if (!pLight) {
        return false;
    }

    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
    if (m_pLights.size() == LIGHTS_MAX_COUNT) {
        return false;
    }

    m_pLights.push_back(pLight);
    return true;
}

size_t Scene::GetLightCount() {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
    return m_pLights.size();
}

std::shared_ptr<LightSource> Scene::GetLight(size_t lightId) {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);
    return lightId < m_pLights.size() ? m_pLights[lightId] : nullptr;
}

RenderObjectHandle Scene::AddObject(
    const EnumFlags<RenderSubsystemType> type,
    std::shared_ptr<RenderObject> pObject
) {
    return RenderObjectHandle{
        type,
        m_pRenderSubsystems[ToId(type)]->Add(pObject)
    };
}

void Scene::UpdateObjectMatrix(
    RenderObjectHandle handle,
    const DirectX::XMMATRIX& modelMatrix
) {
    assert(handle.IsValid());
    m_pRenderSubsystems[ToId(handle.type)]->UpdateModelMatrix(handle.id, modelMatrix);
}

void Scene::SetSimulation(std::function<void(float deltaTime, Scene& scene)> simulation) {
    std::scoped_lock<std::mutex> lock(m_gameHooksMutex);
    m_simulation = std::move(simulation);
}

void Scene::SetMovementHandler(std::function<void(float forwardCoef, float rightCoef)> handler) {
    std::scoped_lock<std::mutex> lock(m_gameHooksMutex);
    m_movementHandler = std::move(handler);
}

void Scene::SetSettingsUI(std::function<void()> settingsUI) {
    std::scoped_lock<std::mutex> lock(m_gameHooksMutex);
    m_settingsUI = std::move(settingsUI);
}

std::shared_ptr<Camera> Scene::GetCurrentCamera() {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);
    return m_pCameras.empty() ? nullptr : m_pCameras.at(m_currCameraId);
}
void Scene::SetOcclusionCullingComputeObject(std::shared_ptr<ComputeObject> pOcclusionCulling) {
    m_pOcclusionCulling = pOcclusionCulling;
}

void Scene::ResetCullingStats(std::shared_ptr<CommandList> pCommandList) {
    if (!m_pCullingStats) {
        return;
    }

    m_pCullingStats->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
    m_pCullingStats->ResetRange(pCommandList, 0, sizeof(SceneCullingStats));
    m_pCullingStats->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

void Scene::CopyCullingStatsForReadback(std::shared_ptr<CommandList> pCommandList) {
    if (!m_pCullingStats) {
        return;
    }

    // Copied even with culling off: the block is zeroed every frame, so the slot
    // holds zeroes instead of whatever an unwritten readback resource happens to
    // contain
    m_pCullingStatsReadback->Copy(pCommandList, m_pCullingStats);
}

void Scene::FinishFrame(uint64_t fenceValue, uint64_t completedFenceValue) {
    if (m_pCullingStatsReadback) {
        m_pCullingStatsReadback->FinishFrame(fenceValue, completedFenceValue);
    }
}

void Scene::CullObjects(
    const EnumFlags<RenderSubsystemType> type,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    bool secondPass
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    if (!m_pOcclusionCulling || !m_cullingSettings.twoPassCulling) {
        return;
    }

    // Only the second pass reads the pyramid, but the descriptor table holding it
    // is bound either way, and every resource a bound table names has to be in the
    // state the table declares
    m_pDepthBuffer->TransitionHiZForReading(pCommandList);

    // Declared before the dispatch, not only before the copy that ends the second
    // pass: the first transition of a resource inside a list is resolved at submit
    // time and lands ahead of everything else in it, so leaving it to the copy puts
    // the buffer into copy source before the dispatch that has to write it
    m_pCullingStats->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    uint32_t flags{ secondPass ? CULLING_FLAG_SECOND_PASS : 0u };
    if (m_cullingSettings.frustumCulling) {
        flags |= CULLING_FLAG_FRUSTUM;
    }
    if (m_cullingSettings.occlusionCulling) {
        flags |= CULLING_FLAG_OCCLUSION;
    }

    const CullingParams params{
        .objectCount{},     // the subsystem knows how many objects it holds
        .flags{ flags },
        .boundingVolume{ static_cast<uint32_t>(m_cullingSettings.boundingVolume) },
        .hzbSize{ m_pDepthBuffer->GetHzbSize() }
    };

    std::scoped_lock<std::mutex> cameraBufferLock(m_cameraBufferMutex);
    m_pRenderSubsystems[ToId(type)]->Cull(
        pCommandList,
        m_pOcclusionCulling,
        pDeviceContext->GetDescriptorHeap(DescRangeType::Srv)->GetD3D12DescriptorHeap(),
        m_pDepthBuffer->GetSrvGpuDescHandleWithMips(),
        m_pCullingCameraCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress(),
        m_pCullingStats->GetD3D12Resource()->GetGPUVirtualAddress()
            + ToId(type) * sizeof(CullingStats),
        params
    );
}

void Scene::RenderObjects(
    const EnumFlags<RenderSubsystemType> type,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList,
    D3D12_VIEWPORT viewport,
    D3D12_RECT scissorRect,
    bool secondPass
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    // With one pass there is nothing left for a second one to draw
    const bool drawCulledCommands{ m_cullingSettings.twoPassCulling };
    if (secondPass && !drawCulledCommands) {
        return;
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
            m_pCameraCB->GetResource()->GetD3D12Resource()->GetGPUVirtualAddress()
        );
        pD3D12CommandList->SetDescriptorHeaps(1, pDeviceContext->GetDescriptorHeap(DescRangeType::Srv)->GetD3D12DescriptorHeap().GetAddressOf());
        if (type & RenderSubsystemType::AlphaKill) {
            const auto& pMaterialManager{ pDeviceContext->GetMaterialManager() };
            pD3D12CommandList->SetGraphicsRootDescriptorTable(3, pMaterialManager->GetMaterialCbvRange()->GetGpuHandle());
            pD3D12CommandList->SetGraphicsRootDescriptorTable(4, pMaterialManager->GetMaterialSrvRange()->GetGpuHandle());
        }
        };

    std::scoped_lock<std::mutex> cameraBufferLock(m_cameraBufferMutex);
    m_pRenderSubsystems[ToId(type)]->Render(
        pCommandList,
        commandListPrepare,
        drawCulledCommands
    );
}

void Scene::RenderObjectsDepth(
    const EnumFlags<RenderSubsystemType> type,
    std::shared_ptr<DeviceContext> pDeviceContext,
    std::shared_ptr<CommandList> pCommandList
) {
    if (std::scoped_lock<std::mutex> lock(m_camerasMutex); !m_isSceneReady.load() || m_pCameras.empty())
        return;

    if (m_shadowLightId == SHADOW_NO_LIGHT) {
        return;
    }

    auto commandListPrepare = [&] {
        auto pD3D12CommandList{ pCommandList->GetD3D12CommandList() };
        pD3D12CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Both come from the map, not from the window: a window-sized scissor would
        // clip away whatever falls outside it
        const CD3DX12_VIEWPORT viewport{
            0.f, 0.f,
            static_cast<float>(ShadowMapResolution),
            static_cast<float>(ShadowMapResolution)
        };
        const CD3DX12_RECT scissorRect{
            0, 0,
            static_cast<LONG>(ShadowMapResolution),
            static_cast<LONG>(ShadowMapResolution)
        };
        pD3D12CommandList->RSSetViewports(1, &viewport);
        pD3D12CommandList->RSSetScissorRects(1, &scissorRect);

        // Depth only. The pipeline state still declares the G-buffer targets and
        // still runs its pixel shader; writes to unbound targets are dropped
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

    std::scoped_lock<std::mutex> cameraBufferLock(m_cameraBufferMutex);
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

    m_pShadowMap->GetTexture()->ResourceTransition(
        pCommandListCompute,
        D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    );

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
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pDepthBuffer->GetSrvGpuDescHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialCbvRange()->GetGpuHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, pMaterialManager->GetMaterialSrvRange()->GetGpuHandle());
            pD3D12CommandList->SetComputeRootDescriptorTable(rootParamId++, m_pShadowMap->GetSrvGpuDescHandle());
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

bool Scene::UpdateCamera(float deltaTime) {
    std::scoped_lock<std::mutex> lock(m_camerasMutex);

    // Every frame, not only after an input: a fly camera coasts to a stop and needs
    // ticking to do it
    if (m_isDebugCameraActive.load()) {
        m_pDebugCamera->Update(deltaTime);
        return true;
    }

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

    // The one place that follows the debug camera. Shadows and the game keep to the
    // gameplay camera, which is the point of having a separate one
    std::shared_ptr<Camera> pCamera{
        m_isDebugCameraActive.load()
            ? std::static_pointer_cast<Camera>(m_pDebugCamera)
            : m_pCameras.at(m_currCameraId)
    };
    // Culling keeps to the gameplay camera, so flying the debug camera out shows
    // what was cut instead of re-culling for the new point of view
    std::shared_ptr<Camera> pCullingCamera{ m_pCameras.at(m_currCameraId) };

    std::unique_lock<std::mutex> cameraBufferLock(m_cameraBufferMutex);

    CameraBuffer sceneBuffer{ *m_pCameraCB->GetStorageData() };
    sceneBuffer.Update(pCamera);

    CameraBuffer cullingCameraBuffer{};
    cullingCameraBuffer.Update(pCullingCamera);

    camerasMutexLock.unlock();

    m_pCameraCB->UpdateAll(&sceneBuffer, 1);
    m_pCullingCameraCB->UpdateAll(&cullingCameraBuffer, 1);
    cameraBufferLock.unlock();

    m_pCameraCB->PerformUpdate(pDeviceContext, pCommandList);

    m_pCameraCB->GetResource()->ResourceTransition(
        pCommandList,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );
}

void Scene::UpdateLightBuffer() {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);

    m_lightBuffer.lightsCount.x = static_cast<uint32_t>(m_pLights.size());
    for (size_t lightId{}; lightId < m_pLights.size(); ++lightId) {
        m_lightBuffer.lights[lightId] = m_pLights[lightId]->GetLight();
    }

    // The very matrix the map was drawn with, not a freshly queried one
    m_lightBuffer.shadowViewProj = m_shadowViewProj;
    m_lightBuffer.shadowParams = m_shadowParams;
    m_lightBuffer.shadowLightId = { m_shadowLightId, 0, 0, 0 };

    m_pLightCB->UpdateAll(&m_lightBuffer, 1);
}

void Scene::UpdateShadowCameraBuffer() {
    std::scoped_lock<std::mutex> lock(m_lightBufferMutex);

    // The first directional light in the list owns the only map there is
    m_shadowLightId = SHADOW_NO_LIGHT;
    for (size_t lightId{}; lightId < m_pLights.size(); ++lightId) {
        if (m_pLights[lightId]->GetType() == LightType::Directional) {
            m_shadowLightId = static_cast<uint32_t>(lightId);
            break;
        }
    }

    if (m_shadowLightId == SHADOW_NO_LIGHT) {
        return;
    }

    const LightSource& light{ *m_pLights[m_shadowLightId] };
    const Camera& shadowCamera{ light.GetShadowCamera() };

    m_shadowViewProj = light.GetViewProjectionMatrix();

    const DirectX::XMFLOAT3 position{ shadowCamera.GetPosition() };
    const Camera::Settings& cameraSettings{ shadowCamera.GetSettings() };

    // Both settings are authored in world terms and converted here, where the map's
    // extent is known. An orthographic shadow camera has linear depth, so a world
    // offset scales into clip space by a plain division
    const float depthRange{ cameraSettings.farPlane - cameraSettings.nearPlane };
    const float worldTexelSize{ cameraSettings.orthographicViewWidth / ShadowMapResolution };

    m_shadowParams = {
        1.f / ShadowMapResolution,
        m_shadowSettings.depthBias / depthRange,
        m_shadowSettings.normalOffset * worldTexelSize,
        static_cast<float>(m_shadowSettings.pcfRadius)
    };

    const CameraBuffer shadowCameraBuffer{
        .viewProjMatrix{ m_shadowViewProj },
        .invViewProjMatrix{ DirectX::XMMatrixInverse(nullptr, m_shadowViewProj) },
        .cameraPosition{ position.x, position.y, position.z, 0.f },
        .nearFar{ cameraSettings.nearPlane, cameraSettings.farPlane, 0.f, 0.f },
        .viewFrustumPlanes{}
    };

    m_pShadowCameraCB->UpdateAll(&shadowCameraBuffer, 1);
}

// UI

#include "imgui.h"

void Scene::DrawSettingsUI() {
    // First: the camera block below returns early when there are no cameras
    {
        std::unique_lock<std::mutex> hooksLock(m_gameHooksMutex);
        if (m_settingsUI) {
            std::function<void()> settingsUI{ m_settingsUI };
            hooksLock.unlock();

            settingsUI();
        }
    }

	// Debug camera
    {
        if (ImGui::Begin("Debug Camera")) {
            bool isActive{ m_isDebugCameraActive.load() };
            if (ImGui::Checkbox("Fly around (F)", &isActive)) {
                SwitchDebugCamera();
            }

            if (m_isDebugCameraActive.load()) {
                ImGui::TextUnformatted("Shadows and culling still follow the game camera");
                ImGui::TextUnformatted("The depth pyramid is frozen with them, so what");
                ImGui::TextUnformatted("is drawn here is what the game camera sees");
                if (ImGui::Button("Snap to game camera")) {
                    // Already what switching on does, but handy after flying off
                    m_isDebugCameraActive.store(false);
                    SwitchDebugCamera();
                }

                std::scoped_lock<std::mutex> lock(m_camerasMutex);
                DrawSettings(*m_pDebugCamera);
            }
        }
        ImGui::End();
    }

	// Camera settings
    {
        std::scoped_lock<std::mutex> lock(m_camerasMutex);
        if (m_pCameras.empty()) {
            return;
        }
        DrawSettings(*m_pCameras.at(m_currCameraId));
    }

	// Light settings
    {
        std::scoped_lock<std::mutex> lock(m_lightBufferMutex);

        if (ImGui::Begin("Lights")) {
            DrawSettings(m_lightBuffer);

            ImGui::SeparatorText("Sources");
            ImGui::Text("Count: %zu / %d", m_pLights.size(), LIGHTS_MAX_COUNT);

            if (ImGui::Button("+") && m_pLights.size() < LIGHTS_MAX_COUNT) {
                m_pLights.push_back(std::make_shared<PointLight>());
            }
            ImGui::SameLine();
            if (ImGui::Button("-") && !m_pLights.empty()) {
                m_pLights.pop_back();
            }

            for (size_t lightId{}; lightId < m_pLights.size(); ++lightId) {
                ImGui::PushID(static_cast<int>(lightId));
                if (ImGui::TreeNode("source", "%s %zu", LightTypeName(m_pLights[lightId]->GetType()), lightId)) {
                    int lightType{ static_cast<int>(m_pLights[lightId]->GetType()) };
                    if (ImGui::Combo("Type", &lightType, "Point\0Directional\0Spot\0")) {
                        switch (static_cast<LightType>(lightType)) {
                        case LightType::Point:
                            m_pLights[lightId] = std::make_shared<PointLight>();
                            break;
                        case LightType::Directional:
                            m_pLights[lightId] = std::make_shared<DirectionalLight>();
                            break;
                        case LightType::Spot:
                            m_pLights[lightId] = std::make_shared<SpotLight>();
                            break;
                        }
                    }

                    DrawSettings(*m_pLights[lightId]);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    // Shadow map
    {
        if (ImGui::Begin("Shadow Map")) {
            std::unique_lock<std::mutex> lock(m_lightBufferMutex);
            const uint32_t shadowLightId{ m_shadowLightId };
            lock.unlock();

            if (shadowLightId == SHADOW_NO_LIGHT) {
                ImGui::TextUnformatted("No directional light in the scene");
            }
            else {
                ImGui::Text("Light %u, %u x %u",
                    shadowLightId, ShadowMapResolution, ShadowMapResolution);

                DrawSettings(m_shadowSettings);

                // The handle is passed by its raw value, as the D3D12 backend
                // expects. Single channel reversed depth, so it reads as red and is
                // brightest closest to the light
                constexpr float PreviewSize{ ShadowMapResolution / 8.f };
                ImGui::Image(
                    static_cast<ImTextureID>(m_pShadowMap->GetSrvGpuDescHandle().ptr),
                    ImVec2{ PreviewSize, PreviewSize }
                );
            }
        }
        ImGui::End();
    }

    // Culling
    {
        if (ImGui::Begin("Culling")) {
            DrawSettings(m_cullingSettings);
            DrawCullingStatsUI();
        }
        ImGui::End();
    }
}

void Scene::DrawCullingStatsUI() {
    // The order follows the flags: Default, Dynamic, AlphaKill, AlphaKill | Dynamic
    static constexpr const char* SubsystemNames[]{
        "Static", "Dynamic", "Static alpha", "Dynamic alpha"
    };
    constexpr size_t SubsystemCount{ static_cast<size_t>(RenderSubsystemType::Count) };
    static_assert(_countof(SubsystemNames) == SubsystemCount);

    ImGui::SeparatorText("Counts");

    if (!m_cullingSettings.twoPassCulling) {
        ImGui::TextUnformatted("Two pass culling is off, nothing is counted");
        return;
    }

    // Read back without stalling the gpu, so these describe a frame that has
    // already been presented
    const SceneCullingStats stats{ m_pCullingStatsReadback->GetLatest() };

    CullingStats total{};
    size_t totalObjects{};

    constexpr ImGuiTableFlags TableFlags{
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit
    };
    if (ImGui::BeginTable("CullingStats", 8, TableFlags)) {
        ImGui::TableSetupColumn("Subsystem");
        ImGui::TableSetupColumn("Objects");
        ImGui::TableSetupColumn("Drawn 1");
        ImGui::TableSetupColumn("Frustum 1");
        ImGui::TableSetupColumn("Drawn 2");
        ImGui::TableSetupColumn("Frustum 2");
        ImGui::TableSetupColumn("Occluded");
        ImGui::TableSetupColumn("Visible");
        ImGui::TableHeadersRow();

        auto drawRow = [](const char* name, size_t objects, const CullingStats& row) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name);

            ImGui::TableNextColumn();
            ImGui::Text("%zu", objects);

            const uint32_t values[]{
                row.drawnFirstPass,
                row.frustumCulledFirstPass,
                row.drawnSecondPass,
                row.frustumCulledSecondPass,
                row.occlusionCulledSecondPass,
                row.visible
            };
            for (const uint32_t value : values) {
                ImGui::TableNextColumn();
                ImGui::Text("%u", value);
            }
            };

        for (size_t i{}; i < SubsystemCount; ++i) {
            const CullingStats& row{ stats.perSubsystem[i] };
            const size_t objects{
                m_pRenderSubsystems[i] ? m_pRenderSubsystems[i]->GetObjectsCount() : 0
            };

            drawRow(SubsystemNames[i], objects, row);

            totalObjects += objects;
            total.drawnFirstPass += row.drawnFirstPass;
            total.frustumCulledFirstPass += row.frustumCulledFirstPass;
            total.drawnSecondPass += row.drawnSecondPass;
            total.frustumCulledSecondPass += row.frustumCulledSecondPass;
            total.occlusionCulledSecondPass += row.occlusionCulledSecondPass;
            total.visible += row.visible;
        }

        drawRow("Total", totalObjects, total);

        ImGui::EndTable();
    }

    // Everything the first pass redrew and the second pass then found hidden was
    // work spent on nothing, which is what the algorithm is judged by
    const uint32_t drawn{ total.drawnFirstPass + total.drawnSecondPass };
    ImGui::Text("Drawn %u, of them wasted %u", drawn, drawn - std::min(drawn, total.visible));

    // The second pass tests every object, so its numbers describe the whole frame:
    // the first pass only redraws what the previous frame left visible
    ImGui::TextUnformatted("Drawn 1 is what the previous frame saw, drawn 2 what appeared");
    ImGui::Text("Depth pyramid: %u x %u", m_pDepthBuffer->GetHzbSize(), m_pDepthBuffer->GetHzbSize());
}

bool DrawSettings(Scene::ShadowSettings& settings) {
    bool isChanged{};

    // Divided by the map's depth range before it reaches the shader
    isChanged |= ImGui::SliderFloat("Depth bias, units", &settings.depthBias, 0.f, .5f, "%.3f");

    // Multiplied by the map's own texel size, so the value keeps its meaning when
    // the light's ortho box is resized
    isChanged |= ImGui::SliderFloat("Normal offset, texels", &settings.normalOffset, 0.f, 4.f, "%.2f");

    // Each step costs (2r + 1)^2 comparisons, and the sampler filters 2x2 per tap
    isChanged |= ImGui::SliderInt("PCF radius", &settings.pcfRadius, 0, 4);

    return isChanged;
}

bool DrawSettings(Scene::CullingSettings& settings) {
    bool isChanged{};

    // Off is the reference picture: everything drawn in a single pass
    isChanged |= ImGui::Checkbox("Two pass culling", &settings.twoPassCulling);

    ImGui::BeginDisabled(!settings.twoPassCulling);
    isChanged |= ImGui::Checkbox("Frustum culling", &settings.frustumCulling);
    isChanged |= ImGui::Checkbox("Occlusion culling", &settings.occlusionCulling);

    // The sphere is what the article tests, the box survives non-uniform scale
    int volume{ static_cast<int>(settings.boundingVolume) };
    if (ImGui::Combo("Bounding volume", &volume, "AABB\0Sphere\0")) {
        settings.boundingVolume = static_cast<Scene::BoundingVolumeType>(volume);
        isChanged = true;
    }
    ImGui::EndDisabled();

    return isChanged;
}
