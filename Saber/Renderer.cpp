#include "Renderer.h"

#include <cassert>
#include <random>

#include <string>  
#include <iostream> 
#include <sstream>

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
    m_pScenes.clear();
    m_pPostProcessing.reset();
    Flush();
}

void Renderer::Initialize(HWND hWnd) {
    m_pBackBuffers.resize(m_numFrames);
    m_frameFenceValues.resize(m_numFrames);

    Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter{ GetAdapter(m_useWarp) };

    m_pDevice = CreateDevice(pAdapter);
    m_pAllocator = CreateAllocator(m_pDevice, pAdapter);

    m_pCommandQueueDirect = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_pCommandQueueCopy = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_COPY);

    m_pSwapChain = CreateSwapChain(hWnd, m_pCommandQueueDirect->GetD3D12CommandQueue(), m_clientWidth, m_clientHeight, m_numFrames);

    m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();

    m_pRTVDescHeap = CreateDescriptorHeap(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numFrames);

    // Get the size of the handle increment for the given type of descriptor heap.
    // This value is typically used to increment a handle into a descriptor array by the correct amount.
    m_RTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CreateRenderTargetViews(m_pDevice, m_pSwapChain, m_pRTVDescHeap);

    CreateDSVDescHeap();

    m_pPSOLibrary = std::make_shared<PSOLibrary>(m_pDevice, L"psolibrary");

    m_pPostProcessing = SimplePostProcessing::CreateSimplePostProcessing(
        m_pDevice,
        m_pMeshAtlas,
        m_pShaderAtlas,
        m_pRootSignatureAtlas,
        m_pPSOLibrary
    );

    m_isInitialized = true;

    // Create scenes
    {
        m_pScenes.resize(5);

        // 0
        m_pScenes[0] = std::make_unique<Scene>(m_pDevice, m_pAllocator);

        // 1
        m_pScenes[1] = std::make_unique<Scene>(m_pDevice, m_pAllocator);
        m_pScenes[1]->AddStaticObject(TestColorRenderObject::CreateTriangle(
            m_pDevice,
            m_pAllocator,
            m_pCommandQueueCopy,
            m_pMeshAtlas,
            m_pShaderAtlas,
            m_pRootSignatureAtlas,
            m_pPSOLibrary
        ));

        // 2
        m_pScenes[2] = std::make_unique<Scene>(m_pDevice, m_pAllocator);
        m_pScenes[2]->AddStaticObject(TestTextureRenderObject::CreateTextureCube(
            m_pDevice,
            m_pAllocator,
            m_pCommandQueueCopy,
            m_pCommandQueueDirect,
            m_pMeshAtlas,
            m_pShaderAtlas,
            m_pRootSignatureAtlas,
            m_pPSOLibrary,
            L"../../Resources/Textures/Brick.dds",
            L"../../Resources/Textures/BrickNM.dds",
            DirectX::XMMatrixIdentity()
        ));

        // 3
        m_pScenes[3] = std::make_unique<Scene>(m_pDevice, m_pAllocator);
        for (size_t i{}; i < 15; ++i) {
            // add static triangle at random position
            m_pJobSystem->AddJob([&]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> posDist(-10.f, 10.f);
                m_pScenes[3]->AddStaticObject(TestColorRenderObject::CreateTriangle(
                    m_pDevice,
                    m_pAllocator,
                    m_pCommandQueueCopy,
                    m_pMeshAtlas,
                    m_pShaderAtlas,
                    m_pRootSignatureAtlas,
                    m_pPSOLibrary,
                    DirectX::XMMatrixTranslation(
                        posDist(gen),
                        posDist(gen),
                        posDist(gen)
                    )
                ));
                });
            // add dynamic cube at random position
            m_pJobSystem->AddJob([&]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> posDist(-10.f, 10.f);
                m_pScenes[3]->AddDynamicObject(TestColorRenderObject::CreateCube(
                    m_pDevice,
                    m_pAllocator,
                    m_pCommandQueueCopy,
                    m_pMeshAtlas,
                    m_pShaderAtlas,
                    m_pRootSignatureAtlas,
                    m_pPSOLibrary,
                    DirectX::XMMatrixTranslation(
                        posDist(gen),
                        posDist(gen),
                        posDist(gen)
                    )
                ));
            });
        }

        // 4
        m_pScenes[4] = std::make_unique<Scene>(m_pDevice, m_pAllocator);
        std::filesystem::path filepath{ L"../../Resources/StaticModels/barbarian_rig_axe_2_a.glb" };
        m_pScenes[4]->AddStaticObject(TestTextureRenderObject::CreateModelFromGLTF(
            m_pDevice,
            m_pAllocator,
            m_pCommandQueueCopy,
            m_pCommandQueueDirect,
            m_pMeshAtlas,
            filepath,
            m_pShaderAtlas,
            m_pRootSignatureAtlas,
            m_pPSOLibrary,
            L"../../Resources/Textures/barbarian_diffuse.dds",
            L"../../Resources/Textures/barb2_n.dds",
            DirectX::XMMatrixScaling(2.f, 2.f, 2.f) * DirectX::XMMatrixTranslation(0.f, -2.f, 0.f)
        ));

        // cameras for all scenes
        for (size_t sceneId{ 1 }; sceneId < 5; ++sceneId) {
            m_pJobSystem->AddJob([&, sceneId]() {
                m_pScenes.at(sceneId)->CreateGBuffer(
                    m_pDevice,
                    m_pAllocator,
                    m_numFrames,
                    m_clientWidth,
                    m_clientHeight
                );

                // dynamic camera
                m_pScenes.at(sceneId)->AddCamera(std::make_shared<DynamicCamera>());

                // standart camera
                m_pScenes[sceneId]->AddCamera(std::make_shared<StaticCamera>(
                    DirectX::XMFLOAT3{ 0.f, 0.f, 3.f },
                    DirectX::XMFLOAT3{ 0.f, 0.f, 0.f },
                    DirectX::XMFLOAT3{ 0.f, 1.f, 0.f }
                ));

                // standart light
                m_pScenes[sceneId]->AddLightSource(
                    { -1.5f, 0.f, 1.5f, 1.f },
                    { 1.f, 1.f, 0.f },
                    { 1.f, 1.f, 0.f }
                );

                // random lights
                for (size_t i{}; i < 9; ++i) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_real_distribution<float> posDist(-2.5f, 2.5f);
                    std::uniform_real_distribution<float> colorDist(0.f, 1.f);
                    m_pScenes[sceneId]->AddLightSource(
                        { posDist(gen), posDist(gen), posDist(gen), colorDist(gen) },
                        { colorDist(gen), colorDist(gen), colorDist(gen) },
                        { colorDist(gen), colorDist(gen), colorDist(gen) }
                    );
                }

                m_pScenes[sceneId]->SetSceneReadiness(true);
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
    m_clientWidth = std::max(1u, width);
    m_clientHeight = std::max(1u, height);

    // Flush the GPU queue to make sure the swap chain's back buffers
    // are not being referenced by an in-flight command list.
    Flush();

    // Any references to the back buffers must be released
    // before the swap chain can be resized.
    for (int i{}; i < m_numFrames; ++i) {
        m_pBackBuffers[i].Reset();
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

    CreateRenderTargetViews(m_pDevice, m_pSwapChain, m_pRTVDescHeap);

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight));

    // TODO update during switch
    m_pScenes[m_currSceneId]->UpdateCamerasAspectRatio(static_cast<float>(m_clientWidth) / m_clientHeight);

    for (size_t i{ 1 }; i < m_pScenes.size(); ++i) {
        m_pScenes.at(i)->ResizeGBuffer(
            m_pDevice,
            m_pAllocator,
            m_clientWidth,
            m_clientHeight
        );
    }
    
    ResizeDepthBuffer();
}

void Renderer::ResizeDepthBuffer() {
    // Flush any GPU commands that might be referencing the depth buffer.
    Flush();

    // Resize screen dependent resources.
    // Create a depth buffer.
    D3D12_CLEAR_VALUE optimizedClearValue{
        .Format{ DXGI_FORMAT_D32_FLOAT },
        .DepthStencil{ 0.0f, 0 }
    };

    ThrowIfFailed(m_pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            m_clientWidth,
            m_clientHeight,
            1,
            0,
            1,
            0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        ),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optimizedClearValue,
        IID_PPV_ARGS(&m_pDepthStencilBuffer)
    ));

    // Update the depth-stencil view.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{
        .Format{ DXGI_FORMAT_D32_FLOAT },
        .ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
        .Flags{ D3D12_DSV_FLAG_NONE },
        .Texture2D{ .MipSlice{} }
    };

    m_pDevice->CreateDepthStencilView(
        m_pDepthStencilBuffer.Get(),
        &dsv,
        m_pDSVDescHeap->GetCPUDescriptorHandleForHeapStart()
    );
}

void Renderer::Update() {
    m_frameCounter++;
    auto t1 = m_clock.now();
    auto deltaTime = t1 - m_time;
    m_time = t1;

    m_pScenes.at(m_currSceneId)->Update(deltaTime.count() * 1e-9f);

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

    auto& backBuffer = m_pBackBuffers[m_currBackBufferId];


    std::shared_ptr<CommandList> commandListBeforeFrame{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, 0)
    };

    // CPU descriptor handle to a RTV
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
        m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart(),   // RTV desc heap start
        m_currBackBufferId,                                     // current index (offset) from start
        m_RTVDescriptorSize                                     // RTV desc size
    );
    auto dsv = m_pDSVDescHeap->GetCPUDescriptorHandleForHeapStart();
    
    scene->SetCurrentBackBuffer(m_currBackBufferId);
    //m_pJobSystem->AddJob([&]()  // Some small work doesn't need to be moved to jobs, just as example
    {
        //RenderTarget::ClearRenderTarget(commandListBeforeFrame->m_pCommandList, backBuffer, rtv, nullptr); // no need to clear, we redraw it or copy there

        commandListBeforeFrame->m_pCommandList->ClearDepthStencilView(
            dsv,
            D3D12_CLEAR_FLAG_DEPTH,
            0.f,
            0,
            0,
            nullptr
        );

        GPUResource::ResourceTransition(
            commandListBeforeFrame->m_pCommandList,
            backBuffer,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        GPUResource::ResourceTransition(
            commandListBeforeFrame->m_pCommandList,
            scene->GetGBuffer()->GetTexture(0)->GetResource().Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );
        scene->ClearGBuffer(commandListBeforeFrame->m_pCommandList);

        commandListBeforeFrame->SetReadyForExection(); // but still it is cl to execute in proper order
    }//);

    // two command lists: static (1), dynamic (2)
    std::shared_ptr<CommandList> commandListForStaticObjects{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, 1)
    };
    m_pJobSystem->AddJob([&]() {
        scene->RenderStaticObjects(
            commandListForStaticObjects->m_pCommandList,
            m_viewport,
            m_scissorRect,
            rtv,
            dsv
        );
        commandListForStaticObjects->SetReadyForExection();
    });

    std::shared_ptr<CommandList> commandListForDynamicObjects{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, 2)
    };
    m_pJobSystem->AddJob([&]() {
        scene->RenderDynamicObjects(
            commandListForDynamicObjects->m_pCommandList,
            m_viewport,
            m_scissorRect,
            rtv,
            dsv
        );
        commandListForDynamicObjects->SetReadyForExection();
    });

    std::shared_ptr<CommandList> commandListAfterFrame{
        m_pCommandQueueDirect->GetCommandList(m_pDevice, true, 3)
    };
    m_pJobSystem->AddJob([&]() { // Some small work doesn't need to be moved to jobs, just as example? but here will be postprocessing or vfx or whatever else? so it will fit as job
        //scene->CopyRTV(
        //    commandListAfterFrame->m_pCommandList,
        //    m_pBackBuffers.at(m_currBackBufferId)
        //);

        std::shared_ptr<Textures> pGBuffer{ scene->GetGBuffer() };
        GPUResource::ResourceTransition(
            commandListAfterFrame->m_pCommandList,
            pGBuffer->GetTexture(0)->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        m_pPostProcessing->Render(
            commandListAfterFrame->m_pCommandList,
            m_viewport,
            m_scissorRect,
            rtv,
            [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, UINT& rootParamId) {
                Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap{ pGBuffer->GetSRVDescHeap() };
                pCommandListDirect->SetDescriptorHeaps(1, srvHeap.GetAddressOf());
                pCommandListDirect->SetGraphicsRootDescriptorTable(rootParamId++, pGBuffer->GetGpuSrvDescHandle(0));
            }
        );
        GPUResource::ResourceTransition(
            commandListAfterFrame->m_pCommandList,
            backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );

        commandListAfterFrame->SetReadyForExection();
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

    scene->UpdateCameraHeap(fenceValue, lastCompletedFenceValue);
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

void Renderer::EnableDebugLayer() {
#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> pDebugInterface;
    //ThrowIfFailed(D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&debugInterface))); // TODO
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugInterface)));
    pDebugInterface->EnableDebugLayer();
#endif
}

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

        if (!(dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            // Creates a device that represents the display adapter
            && SUCCEEDED(D3D12CreateDevice(
                pDXGIAdapter1.Get(),     // A pointer to the video adapter to use when creating a device
                D3D_FEATURE_LEVEL_11_0, // Feature Level
                __uuidof(ID3D12Device), // Device interface GUID
                nullptr                 // A pointer to a memory block that receives a pointer to the device
            ))) {
            ThrowIfFailed(pDXGIAdapter1.As(&pDXGIAdapter4));
        }
    }

    return pDXGIAdapter4;
}

Microsoft::WRL::ComPtr<ID3D12Device2> Renderer::CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter) {
    // Represents a virtual adapter
    Microsoft::WRL::ComPtr<ID3D12Device2> pD3D12Device2;
    // D3D12CreateDevice
    // Creates a device that represents the display adapter
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pD3D12Device2)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    // ID3D12InfoQueue
    // An information-queue interface stores, retrieves, and filters debug messages
    // The queue consists of a message queue, an optional storage filter stack, and a optional retrieval filter stack
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(pD3D12Device2.As(&pInfoQueue))) {
        // SetBreakOnSeverity
        // Set a message severity level to break on when a message with that severity level passes through the storage filter
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[]{};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[]{
            D3D12_MESSAGE_SEVERITY_INFO // Indicates an information message
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[]{
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER NewFilter{
            .DenyList{
                //.NumCategories{ _countof(Categories) },
                //.pCategoryList{ Categories },
                .NumSeverities{ _countof(Severities) },
                .pSeverityList{ Severities },
                .NumIDs{ _countof(DenyIds) },
                .pIDList{ DenyIds },
        }
        };

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return pD3D12Device2;
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

Microsoft::WRL::ComPtr<ID3D12CommandQueue> Renderer::CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
    // ID3D12CommandQueue Provides methods for submitting command lists, synchronizing command list execution,
    // instrumenting the command queue, and updating resource tile mappings.
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> pD3D12CommandQueue;

    D3D12_COMMAND_QUEUE_DESC desc{
        .Type{ type },
        .Priority{ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL },
        .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
        .NodeMask{}
    };

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&pD3D12CommandQueue)));

    return pD3D12CommandQueue;
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> Renderer::CreateSwapChain(HWND hWnd, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount) {
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
        .Flags{ CheckTearingSupport() ? UINT(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) : 0 },
    };

    Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
    // Creates a swap chain that is associated with an HWND handle to the output window for the swap chain
    ThrowIfFailed(pDXGIFactory4->CreateSwapChainForHwnd(
        commandQueue.Get(),
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

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Renderer::CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
    // ID3D12DescriptorHeap (array of resource views)
    // A descriptor heap is a collection of contiguous allocations of descriptors, one
    // allocation for every descriptor. Descriptor heaps contain many object types that are
    // not part of a Pipeline State Object (PSO), such as Shader Resource Views (SRVs),
    // Unordered Access Views (UAVs), Constant Buffer Views (CBVs), and Samplers
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type{ type },
        .NumDescriptors{ numDescriptors }
    };

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pDescriptorHeap)));

    return pDescriptorHeap;
}

void Renderer::CreateRenderTargetViews(Microsoft::WRL::ComPtr<ID3D12Device2> device, Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap) {
    UINT rtvDescriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };

    // Get the CPU descriptor handle that represents the start of the heap
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i{}; i < m_numFrames; ++i) {
        Microsoft::WRL::ComPtr<ID3D12Resource> pBackBufferResource;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBufferResource)));

        device->CreateRenderTargetView(
            pBackBufferResource.Get(),   // ID3D12Resource that represents a render target
            nullptr,            // RTV desc
            rtvHandle           // new RTV dest
        );

        m_pBackBuffers[i] = pBackBufferResource;

        // increment to the next handle in the descriptor heap
        rtvHandle.Offset(rtvDescriptorSize);
    }
}

// Ensure that any commands previously executed on the GPU have finished executing 
// before the CPU thread is allowed to continue processing
void Renderer::Flush() {
    m_pCommandQueueDirect->Flush();
    m_pCommandQueueCopy->Flush();
}

void Renderer::CreateDSVDescHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{
        .Type{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV },
        .NumDescriptors{ 1 },
        .Flags{ D3D12_DESCRIPTOR_HEAP_FLAG_NONE }
    };
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_pDSVDescHeap)));

    ResizeDepthBuffer();
}
