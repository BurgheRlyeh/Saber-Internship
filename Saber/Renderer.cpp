#include "Renderer.h"

#include <cassert>
#include <random>
#include <string>  
#include <iostream> 
#include <sstream>

#include "pix3.h"

Renderer::Renderer(std::shared_ptr<JobSystem<>> pJobSystem, uint8_t backBuffersCnt, bool isUseWarp, uint32_t resWidth, uint32_t resHeight, bool isUseVSync)
    : m_useWarp(isUseWarp)
    , m_clientWidth(resWidth)
    , m_clientHeight(resHeight)
    , m_isVSync(isUseVSync)
    , m_isTearingSupported(CheckTearingSupport())
    , m_time(m_clock.now())
    , m_viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(resWidth), static_cast<float>(resHeight)))
    , m_pMeshAtlas(std::make_shared<Atlas<Mesh>>(L""))
    , m_pShaderAtlas(std::make_shared<Atlas<ShaderResource>>(L""))
    , m_pRootSignatureAtlas(std::make_shared<Atlas<RootSignatureResource>>(L""))
    , m_pJobSystem(pJobSystem)
{
    m_numFrames = backBuffersCnt;
}

Renderer::~Renderer() {
    GPUResource::DestroyCounterResetter();
    m_pBackBuffersDescHeapRange.reset();
    m_pScenes.clear();
    m_pGBuffers.clear();
    m_pDepthBuffers.clear();
    m_pMaterialManager.reset();
    m_pResourceDescHeapManager.reset();
    Flush();
}

void Renderer::Initialize(HWND hWnd) {
    m_pBackBuffers.resize(m_numFrames);
    m_frameFenceValues.resize(m_numFrames);

#if defined(_DEBUG)
    EnableDebugLayer();
    //EnableGPUBasedValidation();
#endif

    // device and allocator
    Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter{ GetAdapter(m_useWarp) };
    m_pDevice = CreateDevice(pAdapter);

#if defined(_DEBUG)
    SetInfoQueueFilter(m_pDevice);
#endif

    m_pAllocator = CreateAllocator(m_pDevice, pAdapter);

    // command queues
    m_pCommandQueueDirect = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_pCommandQueueCompute = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE);
    m_pCommandQueueCopy = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_COPY);

    GPUResource::InitCounterResetter(
        m_pDevice,
        m_pAllocator,
        m_pCommandQueueCopy,
        m_pCommandQueueDirect
    );

    m_pSwapChain = CreateSwapChain(hWnd, m_pCommandQueueDirect->GetD3D12CommandQueue(), m_clientWidth, m_clientHeight, m_numFrames);
    m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();

    constexpr size_t GBUFFER_SIZE{ 3 };

    m_pRtvDescHeapManager = std::make_shared<DescriptorHeapManager>(
        L"DescHeapManagerRtv",
        m_pDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        m_numFrames + GBUFFER_SIZE
    );
    m_pBackBuffersDescHeapRange = m_pRtvDescHeapManager->AllocateRange(L"BackBuffersRange", m_numFrames);

    m_pBackBuffers = CreateBackBuffers(m_pDevice, m_pSwapChain, m_pBackBuffersDescHeapRange);

    m_pPSOLibrary = std::make_shared<PSOLibrary>(m_pDevice, L"PSOLibrary");

    m_pDsvDescHeapManager = std::make_shared<DescriptorHeapManager>(
        L"DescHeapManagerDsv",
        m_pDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        1
    );

    m_pResourceDescHeapManager = std::make_shared<DescriptorHeapManager>(
        L"DescHeapManagerCbvSrvUav",
        m_pDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        8192,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
    );

    m_pDepthBuffers.resize(1);
    m_pDepthBuffers[0] = std::make_shared<DepthBuffer>(
        m_pDevice,
        m_pAllocator,
        m_pDsvDescHeapManager,
        m_pResourceDescHeapManager,
        m_clientWidth,
        m_clientHeight,
        std::make_shared<SinglePassDownsampler>(
            m_pDevice,
            m_pAllocator,
            m_pShaderAtlas,
            m_pRootSignatureAtlas,
            m_pPSOLibrary,
            m_pResourceDescHeapManager,
            m_clientWidth,
            m_clientHeight
        )
    );

    m_pGBuffers.resize(1);
    m_pGBuffers[0] = std::make_shared<TexturePack>(
        L"GBuffer",
        m_pDevice,
        m_pAllocator,
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R32G32B32A32_FLOAT,
            m_clientWidth, m_clientHeight, 1, 0, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        ),
        GBUFFER_SIZE,
        m_pResourceDescHeapManager,
        m_pRtvDescHeapManager
    );

    m_pMaterialManager = std::make_shared<MaterialManager>(
        L"../../Resources/Textures/",
        m_pDevice,
        m_pAllocator,
        m_pResourceDescHeapManager,
        1024
    );

    const size_t RingBufferDefaultSize{ 1024 };
    m_pRingBuffers.resize(RingBufferId::Count);
    m_pRingBuffers[RingBufferId::Cpu] = std::make_shared<DynamicUploadHeap>(
        m_pAllocator,
        RingBufferDefaultSize,
        true
    );
    m_pRingBuffers[RingBufferId::Gpu] = std::make_shared<DynamicUploadHeap>(
        m_pAllocator,
        RingBufferDefaultSize,
        false
    );
    m_pRingBuffers[RingBufferId::GpuWritable] = std::make_shared<DynamicUploadHeap>(
        m_pAllocator,
        RingBufferDefaultSize,
        false,
        true
    );

    m_isInitialized = true;

    {
        constexpr size_t ScenesCount{ 4 };
        m_pScenes.resize(ScenesCount);

        auto copyPostProcess{ CopyPostProcessing::Create(
            m_pDevice,
            m_pMeshAtlas,
            m_pShaderAtlas,
            m_pRootSignatureAtlas,
            m_pPSOLibrary
        ) };
        auto deferredShading{ DeferredShading::CreateDefferedShadingComputeObject(
            m_pDevice,
            m_pShaderAtlas,
            m_pRootSignatureAtlas,
            m_pPSOLibrary
        ) };
        for (size_t i{}; i < ScenesCount; ++i) {
            std::unique_ptr<Scene>& pScene{ m_pScenes[i] };
            pScene = std::make_unique<Scene>(
                L"Scene" + std::to_wstring(i),
                m_pDevice,
                m_pAllocator,
                m_pRingBuffers[RingBufferId::Cpu],
                m_pRingBuffers[RingBufferId::Gpu],
                m_pResourceDescHeapManager,
                m_pDepthBuffers[0],
                m_pGBuffers[0]
            );
            pScene->SetPostProcessing(copyPostProcess);
            pScene->SetDeferredShadingComputeObject(deferredShading);
        }
        
        std::vector<std::function<void(std::unique_ptr<Scene>&)>> sceneObjectAdders;
        sceneObjectAdders.resize(ScenesCount);
        sceneObjectAdders[0] = [&](std::unique_ptr<Scene>& pScene) {};
        sceneObjectAdders[1] = [&](std::unique_ptr<Scene>& pScene) {
            pScene->AddStaticObject(TestTextureRenderObject::CreateTextureCube(
                m_pDevice,
                m_pAllocator,
                m_pCommandQueueCopy,
                m_pCommandQueueDirect,
                m_pMeshAtlas,
                m_pShaderAtlas,
                m_pRootSignatureAtlas,
                m_pPSOLibrary,
                m_pGBuffers[0],
                m_pMaterialManager,
                DirectX::XMMatrixIdentity()
            ));
        };
        sceneObjectAdders[2] = [&](std::unique_ptr<Scene>& pScene) {
            std::filesystem::path filepath{ L"../../Resources/StaticModels/barbarian_rig_axe_2_a.glb" };
            pScene->AddDynamicObject(TestTextureRenderObject::CreateModelFromGLTF(
                m_pDevice,
                m_pAllocator,
                m_pCommandQueueCopy,
                m_pCommandQueueDirect,
                m_pMeshAtlas,
                filepath,
                m_pShaderAtlas,
                m_pRootSignatureAtlas,
                m_pPSOLibrary,
                m_pGBuffers[0],
                m_pMaterialManager,
                DirectX::XMMatrixScaling(2.f, 2.f, 2.f) * DirectX::XMMatrixTranslation(0.f, -2.f, 0.f)
            ));
            std::filesystem::path filepathGrass{ L"../../Resources/StaticModels/grass.glb" };
            pScene->AddStaticAlphaKillObject(TestAlphaRenderObject::CreateAlphaModelFromGLTF(
                m_pDevice,
                m_pAllocator,
                m_pCommandQueueCopy,
                m_pCommandQueueDirect,
                m_pMeshAtlas,
                filepathGrass,
                m_pShaderAtlas,
                m_pRootSignatureAtlas,
                m_pPSOLibrary,
                m_pGBuffers[0],
                m_pMaterialManager,
                DirectX::XMMatrixScaling(.025f, .025f, .025f) * DirectX::XMMatrixTranslation(0.f, -2.f, -1.f)
            ));
        };
        sceneObjectAdders[3] = [&](std::unique_ptr<Scene>& pScene) {
            std::filesystem::path filepathGrass{ L"../../Resources/StaticModels/grass.glb" };
            DirectX::XMMATRIX scale{ DirectX::XMMatrixScaling(.025f, .025f, .025f) };

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> posDist(-10.f, 10.f);
            for (size_t i{}; i < 100; ++i) {
                pScene->AddStaticAlphaKillObject(TestAlphaRenderObject::CreateAlphaModelFromGLTF(
                    m_pDevice,
                    m_pAllocator,
                    m_pCommandQueueCopy,
                    m_pCommandQueueDirect,
                    m_pMeshAtlas,
                    filepathGrass,
                    m_pShaderAtlas,
                    m_pRootSignatureAtlas,
                    m_pPSOLibrary,
                    m_pGBuffers[0],
                    m_pMaterialManager,
                    scale * DirectX::XMMatrixTranslation(posDist(gen), -1.f, posDist(gen))
                ));
            }
        };

        // cameras and lights for all scenes
        for (size_t i{}; i < ScenesCount; ++i) {
            std::function<void(std::unique_ptr<Scene>&)> addObjects{ sceneObjectAdders[i] };
            m_pJobSystem->AddJob([&, i, addObjects]() {
                std::unique_ptr<Scene>& pScene{ m_pScenes[i] };

                // dynamic camera
                pScene->AddCamera(std::make_shared<DynamicCamera>());

                // standart camera
                pScene->AddCamera(std::make_shared<StaticCamera>(
                    DirectX::XMFLOAT3{ 0.f, 0.f, 3.f },
                    DirectX::XMFLOAT3{ 0.f, 0.f, 0.f },
                    DirectX::XMFLOAT3{ 0.f, 1.f, 0.f }
                ));

                // standart light
                pScene->AddLightSource(
                    { -1.5f, 0.f, 1.5f, 1.f },
                    { 1.f, 1.f, 0.f },
                    { 1.f, 1.f, 0.f }
                );

                // random lights
                for (size_t i{}; i < 1; ++i) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_real_distribution<float> posDist(-2.5f, 2.5f);
                    std::uniform_real_distribution<float> colorDist(0.f, 1.f);
                    pScene->AddLightSource(
                        { posDist(gen), posDist(gen), posDist(gen), colorDist(gen) },
                        { colorDist(gen), colorDist(gen), colorDist(gen) },
                        { colorDist(gen), colorDist(gen), colorDist(gen) }
                    );
                }

                addObjects(pScene);
                pScene->InitializeRenderSubsystems(
                    m_pDevice,
                    m_pAllocator,
                    m_pResourceDescHeapManager,
                    m_pRingBuffers[RingBufferId::Cpu],
                    IndirectUpdater::CreateCbMesh4Updater(m_pDevice, m_pShaderAtlas, m_pRootSignatureAtlas, m_pPSOLibrary)
                );

                pScene->SetSceneReadiness(true);
            });
        }
    }
    m_pPSOLibrary->FlushCacheToFile();
}

bool Renderer::StartRenderThread() {
    if (!m_isInitialized)
        return false;

    m_pJobSystem->StartRunning();
    m_isRenderThreadRunning.store(true);
    m_renderThread = std::thread(&Renderer::RenderLoop, this);
    return true;
}

void Renderer::StopRenderThread() {
    m_isRenderThreadRunning.store(false);
    m_renderThread.join();
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    m_isNeedResize.store(true);
    m_resolutionWidthForResize.store(width);
    m_resolutionHeightForResize.store(height);
}

void Renderer::SwitchVSync() {
    m_isVSync = !m_isVSync;
}

void Renderer::SetSceneId(size_t sceneId) {
    m_nextSceneId.store(sceneId);
}

void Renderer::SwitchToNextCamera() {
    m_isSwitchToNextCamera.store(true);
}

inline void Renderer::RenderLoop() {
    while (!m_isInitialized) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    while (m_isRenderThreadRunning.load()) {
        if (m_isNeedResize.load()) {
            std::scoped_lock<std::mutex> lock(m_renderThreadMutex);
            PerformResize();
            m_isNeedResize.store(false);
        }
        m_currSceneId = m_nextSceneId.load();
        if (m_isSwitchToNextCamera.load()) {
            m_pScenes[m_currSceneId]->NextCamera();
            m_isSwitchToNextCamera.store(false);
        }

        Update();

        std::scoped_lock<std::mutex> lock(m_renderThreadMutex);
        Render();
    }
}

void Renderer::PerformResize() {
    assert(m_isInitialized);

    uint32_t width{ m_resolutionWidthForResize.load() };
    uint32_t height{ m_resolutionHeightForResize.load() };

    if (m_clientWidth == width && m_clientHeight == height)
        return;

    // Don't allow 0 size swap chain back buffers.
    m_clientWidth  = std::min<uint32_t>(std::max(1u, width), 4096);
    m_clientHeight = std::min<uint32_t>(std::max(1u, height), 4096);

    // Flush the GPU queue to make sure the swap chain's back buffers
    // are not being referenced by an in-flight command list.
    Flush();

    // Any references to the back buffers must be released
    // before the swap chain can be resized.
    for (int i{}; i < m_numFrames; ++i) {
        m_pBackBuffers[i] = nullptr;
        m_frameFenceValues[i] = m_frameFenceValues[m_currBackBufferId];
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    ThrowIfFailed(m_pSwapChain->GetDesc(&swapChainDesc));
    ThrowIfFailed(m_pSwapChain->ResizeBuffers(
        m_numFrames,                        // number of buffers in swap chain
        m_clientWidth,                      // new width of back buffer
        m_clientHeight,                     // new height of back buffer
        swapChainDesc.BufferDesc.Format,    // new format of back buffer
        swapChainDesc.Flags                 // flags
    ));

    m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();

    m_pBackBuffers = CreateBackBuffers(m_pDevice, m_pSwapChain, m_pBackBuffersDescHeapRange);

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight));

    // TODO update during switch
    for (auto& pScene : m_pScenes) {
        //pScene->UpdateCamerasAspectRatio(static_cast<float>(m_clientWidth) / m_clientHeight);
        pScene->Resize(m_pDevice, m_pAllocator, m_clientWidth, m_clientHeight);
    }
    for (auto& pDepthBuffer : m_pDepthBuffers) {
        pDepthBuffer->Resize(m_pDevice, m_pAllocator, m_clientWidth, m_clientHeight);
    }
    for (auto& pGBuffer : m_pGBuffers) {
        pGBuffer->Resize(m_pDevice, m_pAllocator, m_clientWidth, m_clientHeight);
    }
}

void Renderer::Update() {
    m_frameCounter++;
    auto t1 = m_clock.now();
    auto deltaTime = t1 - m_time;
    m_time = t1;

    std::unique_ptr<Scene>& pScene{ m_pScenes[m_currSceneId] };
    if (pScene->IsSceneReady()) {
        std::shared_ptr pCommandList{ m_pCommandQueueDirect->GetCommandList(m_pDevice) };
        pScene->Update(deltaTime.count() * 1e-9f, pCommandList);
        m_pCommandQueueDirect->ExecuteCommandListImmediately(pCommandList);

        pScene->UpdateRenderSubsystems(
            m_pDevice,
            m_pAllocator,
            m_pCommandQueueCopy,
            m_pCommandQueueDirect
        );
    }

    m_elapsedSeconds += deltaTime.count() * 1e-9;
    if (m_elapsedSeconds > 1.0) {
        auto fps = m_frameCounter / m_elapsedSeconds;

        std::wstringstream wss{};
        wss << "FPS: " << fps << std::endl;
        OutputDebugString(wss.str().c_str());

        m_frameCounter = 0;
        m_elapsedSeconds = 0.0;
    }
}

void Renderer::Render() {
    auto& scene = m_pScenes.at(m_currSceneId);
    if (!scene->IsSceneReady())
        return;

    size_t listPriority{};

    auto& backBuffer = m_pBackBuffers[m_currBackBufferId];
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{ m_pBackBuffersDescHeapRange->GetCpuHandle(m_currBackBufferId) };

    std::shared_ptr<CommandList> commandListBeforeFrame{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, listPriority)
    };

    // Some small work doesn't need to be moved to jobs, just as example
    {
        PIXBeginEvent(
            commandListBeforeFrame->GetD3D12CommandList().Get(),
            PIX_COLOR(0, 0, 0),
            L"Before frame part"
        );
        scene->BeforeFrameJob(commandListBeforeFrame);

        backBuffer->ResourceTransition(
            commandListBeforeFrame,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        if (!scene->GetGBuffer()) {
            float clearColor[]{ 0.6f, 0.4f, 0.4f, 1.0f };
            backBuffer->ClearRenderTarget(
                commandListBeforeFrame,
                rtv,
                clearColor
            );
        }

        PIXEndEvent(commandListBeforeFrame->GetD3D12CommandList().Get());
        commandListBeforeFrame->SetReadyForExection(); // but still it is cl to execute in proper order
    }

    // two command lists: static (1), dynamic (2)
    std::shared_ptr<CommandList> commandListForStaticObjects{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, ++listPriority)
    };
    m_pJobSystem->AddJob([&]() {
        PIXBeginEvent(
            commandListForStaticObjects->GetD3D12CommandList().Get(),
            PIX_COLOR(0, 0, 0),
            L"Static Objects rendering"
        );
        scene->RenderStaticObjects(
            commandListForStaticObjects,
            m_viewport,
            m_scissorRect,
            rtv,
            m_pResourceDescHeapManager
        );
        PIXEndEvent(commandListForStaticObjects->GetD3D12CommandList().Get());
        commandListForStaticObjects->SetReadyForExection();
        });

    std::shared_ptr<CommandList> commandListForAlphaObjects{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, listPriority)
    };
    m_pJobSystem->AddJob([&]() {
        PIXBeginEvent(
            commandListForAlphaObjects->GetD3D12CommandList().Get(),
            PIX_COLOR(0, 0, 0),
            L"Alpha Objects rendering"
        );
        scene->RenderStaticAlphaKillObjects(
            commandListForAlphaObjects,
            m_viewport,
            m_scissorRect,
            rtv,
            m_pResourceDescHeapManager,
            m_pMaterialManager
        );
        PIXEndEvent(commandListForAlphaObjects->GetD3D12CommandList().Get());
        commandListForAlphaObjects->SetReadyForExection();
        });

    uint64_t fenceValueAfterRender{};
    std::shared_ptr<CommandList> commandListForDynamicObjects{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, ++listPriority, [] {},
            [&]() { fenceValueAfterRender = m_pCommandQueueDirect->Signal(); }
        )
    };
    m_pJobSystem->AddJob([&]() {
        PIXBeginEvent(
            commandListForDynamicObjects->GetD3D12CommandList().Get(),
            PIX_COLOR(0, 0, 0),
            L"Dynamic Objects rendering"
        );
        scene->RenderDynamicObjects(
            commandListForDynamicObjects,
            m_viewport,
            m_scissorRect,
            rtv,
            m_pResourceDescHeapManager
        );
        PIXEndEvent(commandListForDynamicObjects->GetD3D12CommandList().Get());
        commandListForDynamicObjects->SetReadyForExection();
        });


    uint64_t fenceValueAfterHZB{ /*static_cast<uint64_t>(-1)*/ };
    std::shared_ptr<CommandList> commandListForHZB{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, ++listPriority,
            [&] { m_pCommandQueueDirect->WaitForFenceValue(fenceValueAfterRender); },
            [&] { fenceValueAfterHZB = m_pCommandQueueDirect->Signal(); }
        )
    };

    uint64_t fenceValueAfterDeferredShading{ /*static_cast<uint64_t>(-1)*/ };
    std::shared_ptr<CommandList> commandListForDeferredShading{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, ++listPriority,
            [&] { m_pCommandQueueDirect->WaitForFenceValue(fenceValueAfterHZB); },
            [&] { fenceValueAfterDeferredShading = m_pCommandQueueDirect->Signal(); }
        )
    };
    std::shared_ptr<CommandList> commandListAfterFrame{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, ++listPriority,
            [&] { m_pCommandQueueDirect->WaitForFenceValue(fenceValueAfterDeferredShading); }
        )
    };
    m_pJobSystem->AddJob([&]() {
        {
            PIXBeginEvent(
                commandListForHZB->GetD3D12CommandList().Get(),
                PIX_COLOR(0, 0, 0),
                L"Building HZB"
            );
            scene->GetDepthBuffer()->CreateHierarchicalDepthBuffer(
                commandListForHZB,
                m_pResourceDescHeapManager->GetDescriptorHeap()
            );
            PIXEndEvent(commandListForHZB->GetD3D12CommandList().Get());
            commandListForHZB->SetReadyForExection();
        }

        {
            PIXBeginEvent(
                commandListForDeferredShading->GetD3D12CommandList().Get(),
                PIX_COLOR(0, 0, 0),
                L"Deferred shading"
            );
            scene->RunDeferredShading(
                commandListForDeferredShading,
                m_pResourceDescHeapManager,
                m_pMaterialManager,
                m_clientWidth,
                m_clientHeight
            );
            PIXEndEvent(commandListForDeferredShading->GetD3D12CommandList().Get());
            commandListForDeferredShading->SetReadyForExection();
        }

        {
            PIXBeginEvent(
                commandListAfterFrame->GetD3D12CommandList().Get(),
                PIX_COLOR(0, 0, 0),
                L"Post Processing"
            );
            scene->RenderPostProcessing(
                commandListAfterFrame,
                m_pResourceDescHeapManager,
                m_viewport,
                m_scissorRect,
                rtv
            );
            backBuffer->ResourceTransition(
                commandListAfterFrame,
                D3D12_RESOURCE_STATE_PRESENT
            );

            PIXEndEvent(commandListAfterFrame->GetD3D12CommandList().Get());
            commandListAfterFrame->SetReadyForExection();
        }
        });

    uint64_t lastCompletedFenceValue{
        m_frameFenceValues[(m_currBackBufferId + m_numFrames - 1) % m_numFrames]
    };
    m_frameFenceValues[m_currBackBufferId] = m_pCommandQueueDirect->ExecutionTask(m_frameFenceValues[m_currBackBufferId]);
    uint64_t fenceValue{ m_frameFenceValues[m_currBackBufferId] };

    // Present
    {
        UINT syncInterval{ m_isVSync ? 1u : 0 };
        UINT presentFlags{ m_isTearingSupported && !syncInterval ? DXGI_PRESENT_ALLOW_TEARING : 0 };
        ThrowIfFailed(m_pSwapChain->Present(
            // 0 - Cancel the remaining time on the previously presented frame 
            // and discard this frame if a newer frame is queued
            // n = 1...4 - Synchronize presentation for at least n vertical blanks
            syncInterval,
            presentFlags
        ));

        // get the index of the swap chains current back buffer,
        // as order of back buffer indicies is not guaranteed to be sequential,
        // when using the DXGI_SWAP_EFFECT_FLIP_DISCARD flip model
        m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();
    }

    for (auto& pRingBuffer : m_pRingBuffers) {
        pRingBuffer->FinishFrame(fenceValue, lastCompletedFenceValue);
    }
}

void Renderer::MoveCamera(float forwardCoef, float rightCoef) {
    m_pScenes.at(m_currSceneId)->TryMoveCamera(forwardCoef, rightCoef);
}

void Renderer::RotateCamera(float deltaX, float deltaY) {
    m_pScenes.at(m_currSceneId)->TryRotateCamera(deltaX / m_clientWidth, deltaY / m_clientHeight);
}

bool Renderer::CheckTearingSupport() {
    BOOL allowTearing{};

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update
    Microsoft::WRL::ComPtr<IDXGIFactory4> pFactory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&pFactory4)))) {
        Microsoft::WRL::ComPtr<IDXGIFactory5> pFactory5;
        if (SUCCEEDED(pFactory4.As(&pFactory5))) {
            allowTearing = SUCCEEDED(pFactory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allowTearing,
                sizeof(allowTearing)
            ));
        }
    }

    return allowTearing;
}

#if defined(_DEBUG)
void Renderer::EnableDebugLayer() {
    Microsoft::WRL::ComPtr<ID3D12Debug> pDebugInterface;
    //ThrowIfFailed(D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&debugInterface))); // TODO
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugInterface)));
    pDebugInterface->EnableDebugLayer();
}

void Renderer::EnableGPUBasedValidation()
{
    Microsoft::WRL::ComPtr<ID3D12Debug> spDebugController0;
    Microsoft::WRL::ComPtr<ID3D12Debug1> spDebugController1;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0)));
    ThrowIfFailed(spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1)));
    spDebugController1->SetEnableGPUBasedValidation(true);
}

void Renderer::SetInfoQueueFilter(Microsoft::WRL::ComPtr<ID3D12Device2>& pDevice) {
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
    ThrowIfFailed(pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)));

    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

    // Suppress whole categories of messages
    //D3D12_MESSAGE_CATEGORY Categories[]{};

    // Suppress messages based on their severity level
    D3D12_MESSAGE_SEVERITY Severities[]{
        D3D12_MESSAGE_SEVERITY_INFO,
    };

    // Suppress individual messages by their ID
    D3D12_MESSAGE_ID DenyIds[] = {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.

        D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_HAS_NO_RESOURCE,            // For D3D12MA compatibility

        D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND,                     // Occurs when PSOLibrary tries to find unexisted PSO

        D3D12_MESSAGE_ID_RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH,
        D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE
    };
    D3D12_INFO_QUEUE_FILTER NewFilter{
        .DenyList{
            //.NumCategories{ _countof(Categories) },
            //.pCategoryList{ Categories },
            .NumSeverities{ _countof(Severities) },
            .pSeverityList{ Severities },
            .NumIDs{ _countof(DenyIds) },
            .pIDList{ DenyIds }
        }
    };

    ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
}
#endif

Microsoft::WRL::ComPtr<IDXGIAdapter4> Renderer::GetAdapter(bool useWarp) {
    // IDXGIFactory
    // An IDXGIFactory interface implements methods for generating DXGI objects (which handle full screen transitions)
    Microsoft::WRL::ComPtr<IDXGIFactory6> pDXGIFactory;

    // Enabling the DXGI_CREATE_FACTORY_DEBUG flag during factory creation enables errors
    // to be caught during device creation and while querying for the adapters
    UINT createFactoryFlags{};
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    // CreateDXGIFactory2
    // Creates a DXGI factory that you can use to generate other DXGI objects
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pDXGIFactory)));

    // The IDXGIAdapter interface represents a display subsystem (including one or more GPUs, DACs and video memory)
    Microsoft::WRL::ComPtr<IDXGIAdapter1> pDXGIAdapter1;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> pDXGIAdapter4;

    if (useWarp) {
        // IDXGIFactory4::EnumWarpAdapter
        // Provides an adapter which can be provided to D3D12CreateDevice to use the WARP renderer
        ThrowIfFailed(pDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pDXGIAdapter1)));
        ThrowIfFailed(pDXGIAdapter1.As(&pDXGIAdapter4));
        return pDXGIAdapter4;
    }

    SIZE_T maxDedicatedVideoMemory{};
    for (UINT adapterIndex{};
        pDXGIFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,   // The GPU preference for the app
            IID_PPV_ARGS(&pDXGIAdapter1)
        ) != DXGI_ERROR_NOT_FOUND;
        ++adapterIndex
        ) {
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
        ThrowIfFailed(pDXGIAdapter1->GetDesc1(&dxgiAdapterDesc1));

        if (dxgiAdapterDesc1.DedicatedVideoMemory <= maxDedicatedVideoMemory) {
            continue;
        }

        if (!(dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            // Creates a device that represents the display adapter
            && SUCCEEDED(D3D12CreateDevice(
                pDXGIAdapter1.Get(),     // A pointer to the video adapter to use when creating a device
                D3D_FEATURE_LEVEL_11_0, // Feature Level
                __uuidof(ID3D12Device), // Device interface GUID
                nullptr                 // A pointer to a memory block that receives a pointer to the device
            ))) {
            ThrowIfFailed(pDXGIAdapter1.As(&pDXGIAdapter4));
            maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
        }
    }

    return pDXGIAdapter4;
}

Microsoft::WRL::ComPtr<ID3D12Device2> Renderer::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter) {
    // Represents a virtual adapter
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice;

    // Creates a device that represents the display adapter
    ThrowIfFailed(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice)));
    return pDevice;
}

Microsoft::WRL::ComPtr<D3D12MA::Allocator> Renderer::CreateAllocator(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter
) {
    Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator{};

    D3D12MA::ALLOCATOR_DESC desc{
        .Flags{
            D3D12MA::ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED
            | D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED
        },
        .pDevice{ m_pDevice.Get() },
        .pAdapter{ pAdapter.Get() }
    };

    ThrowIfFailed(D3D12MA::CreateAllocator(&desc, pAllocator.GetAddressOf()));
    return pAllocator;
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Renderer::CreateSwapChain(
    HWND hWnd,
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue,
    uint32_t width,
    uint32_t height,
    uint32_t bufferCount
) {
    // An IDXGISwapChain interface implements one or more surfaces
    // for storing rendered data before presenting it to an output
    Microsoft::WRL::ComPtr<IDXGISwapChain4> pDXGISwapChain4;
    Microsoft::WRL::ComPtr<IDXGIFactory4> pDXGIFactory4;

    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pDXGIFactory4)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{
        .Width{ width },                                    // Resolution width
        .Height{ height },                                  // Resolution height
        .Format{ DXGI_FORMAT_R8G8B8A8_UNORM },              // Display format
        .Stereo{ FALSE },                                   // Is stereo
        .SampleDesc{ 1, 0 },                                // Multi-sampling parameters
        .BufferUsage{ DXGI_USAGE_RENDER_TARGET_OUTPUT },    // Surface usage and CPU access options for the back buffer
        .BufferCount{ bufferCount },                        // Number of buffers in the swap chain
        .Scaling{ DXGI_SCALING_STRETCH },                   // Resize behavior if the size of the back buffer is not equal to the target output
        .SwapEffect{ DXGI_SWAP_EFFECT_FLIP_DISCARD },       // Presentation model
        .AlphaMode{ DXGI_ALPHA_MODE_UNSPECIFIED },          // Transparency behavior
        // It is recommended to always allow tearing if tearing support is available.
        .Flags{ m_isTearingSupported ? UINT(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) : 0 },
    };

    Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
    // Creates a swap chain that is associated with an HWND handle to the output window for the swap chain
    ThrowIfFailed(pDXGIFactory4->CreateSwapChainForHwnd(
        pCommandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,        // description of a full-screen swap chain
        nullptr,        // pointer to interface for the output to restrict content to
        &pSwapChain1
    ));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(pDXGIFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(pSwapChain1.As(&pDXGISwapChain4));
    return pDXGISwapChain4;
}

std::vector<std::shared_ptr<Texture>> Renderer::CreateBackBuffers(
    Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
    Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
    std::shared_ptr<DescHeapRange> pDescHeapRange
) {
    DXGI_SWAP_CHAIN_DESC desc{};
    ThrowIfFailed(pSwapChain->GetDesc(&desc));
    ThrowIfFailed(pDevice->GetDeviceRemovedReason());

    std::vector<std::shared_ptr<Texture>> backBuffers{ desc.BufferCount };

    pDescHeapRange->Clear();
    for (size_t i{}; i < desc.BufferCount; ++i) {
        //backBuffers[i] = Texture::InitFromSwapChain(pSwapChain, i);
        backBuffers[i] = std::make_shared<Texture>(pSwapChain, i);
        backBuffers[i]->CreateRenderTargetView(
            pDevice,
            pDescHeapRange->GetNextCpuHandle()
        );
    }

    return backBuffers;
}

// Ensure that any commands previously executed on the GPU have finished executing 
// before the CPU thread is allowed to continue processing
void Renderer::Flush() {
    m_pCommandQueueDirect->Flush();
    m_pCommandQueueCopy->Flush();
}
