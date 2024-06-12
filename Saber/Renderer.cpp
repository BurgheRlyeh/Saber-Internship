#include "Renderer.h"

#include <cassert>
#include <random>

#include <string>  
#include <iostream> 
#include <sstream>

Renderer::Renderer(uint8_t backBuffersCnt, bool isUseWarp, uint32_t resWidth, uint32_t resHeight, bool isUseVSync)
    : m_useWarp(backBuffersCnt)
    , m_clientWidth(resWidth)
    , m_clientHeight(resHeight)
    , m_isVSync(isUseVSync)
    , m_isTearingSupported(CheckTearingSupport())
    , m_time(m_clock.now()) 
{
    m_numFrames = backBuffersCnt;
}

Renderer::~Renderer() {}

void Renderer::Initialize(HWND hWnd) {
    m_pBackBuffers.resize(m_numFrames);
    m_frameFenceValues.resize(m_numFrames);

    Microsoft::WRL::ComPtr<IDXGIAdapter4> pDXGIAdapter4{ GetAdapter(m_useWarp) };

    m_pDevice = CreateDevice(pDXGIAdapter4);

    m_pCommandQueueDirect = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_pCommandQueueCopy = std::make_shared<CommandQueue>(m_pDevice, D3D12_COMMAND_LIST_TYPE_COPY);

    m_pSwapChain = CreateSwapChain(hWnd, m_pCommandQueueDirect->GetD3D12CommandQueue(), m_clientWidth, m_clientHeight, m_numFrames);

    m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();

    m_pRTVDescriptorHeap = CreateDescriptorHeap(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numFrames);

    // Get the size of the handle increment for the given type of descriptor heap.
    // This value is typically used to increment a handle into a descriptor array by the correct amount.
    m_RTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CreateRenderTargetViews(m_pDevice, m_pSwapChain, m_pRTVDescriptorHeap);

    m_isInitialized = true;
}

bool Renderer::StartRenderThread() {
    if (!m_isInitialized)
        return false;

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

inline void Renderer::RenderLoop() {
    while (m_isRenderThreadRunning.load()) {
        if (m_isNeedResize.load()) {
            std::scoped_lock<std::mutex> lock(m_renderThreadMutex);
            PerformResize(
                m_resolutionWidthForResize.load(),
                m_resolutionHeightForResize.load()
            );
            m_isNeedResize.store(false);
        }

        Update();

        std::scoped_lock<std::mutex> lock(m_renderThreadMutex);
        Render();
    }
}

void Renderer::PerformResize(uint32_t width, uint32_t height) {
    assert(m_isInitialized);

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

    CreateRenderTargetViews(m_pDevice, m_pSwapChain, m_pRTVDescriptorHeap);
}

void Renderer::Update() {
    m_frameCounter++;
    auto t1 = m_clock.now();
    auto deltaTime = t1 - m_time;
    m_time = t1;

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
    auto& backBuffer = m_pBackBuffers[m_currBackBufferId];

    // Before overwriting the contents of the current back buffer with the content of the next frame,
    // the CPU thread is stalled using the WaitForFenceValue function described earlier.
    m_pCommandQueueDirect->WaitForFenceValue(m_frameFenceValues[m_currBackBufferId]);

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList{
        m_pCommandQueueDirect->GetCommandList(m_pDevice)
    };

    // Clear the render target.
    {
        // Before the render target can be cleared, it must be transitioned to the RENDER_TARGET state.
        TransitionResource(
            pCommandList,
            backBuffer,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

        // just for testing
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 0.1);

        FLOAT clearColor[] = {
            0.4f + static_cast<float>(dis(gen)),
            0.6f + static_cast<float>(dis(gen)),
            0.9f + static_cast<float>(dis(gen)),
            1.0f
        };
        // CPU descriptor handle to a RTV
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
            m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),  // RTV desc heap start
            m_currBackBufferId,                                   // current index (offset) from start
            m_RTVDescriptorSize                                         // RTV desc size
        );

        pCommandList->ClearRenderTargetView(
            rtv,        // cpu desc handle
            clearColor, // color to fill RTV
            0,          // number of rectangles in array
            nullptr     // array of rectangles in resource view, if nullptr then clears entire resouce view
        );
    }

    // Present
    {
        TransitionResource(
            pCommandList,
            backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        );

        // This method must be called on the command list before being executed on the command queue
        m_frameFenceValues[m_currBackBufferId] = m_pCommandQueueDirect->ExecuteCommandList(pCommandList);

        UINT syncInterval{ m_isVSync ? 1u : 0};
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
}

bool Renderer::CheckTearingSupport() {
    BOOL allowTearing{};

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update. ??? TODO
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

void Renderer::TransitionResource(
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
    Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter
) {
    CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
        pResource.Get(),
        stateBefore,
        stateAfter
    ) };

    // Notifies the driver that it needs to synchronize multiple accesses to resources
    pCommandList->ResourceBarrier(1, &barrier);
}
