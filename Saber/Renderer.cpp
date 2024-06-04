#include "Renderer.h"

Renderer::Renderer(uint8_t backBuffersCnt, bool isUseWarp, uint32_t resWidth, uint32_t resHeight, bool isUseVSync) {
    m_numFrames = backBuffersCnt;
    m_pBackBuffers.resize(m_numFrames);
    m_pCommandAllocators.resize(m_numFrames);
    m_frameFenceValues.resize(m_numFrames);

    m_useWarp = isUseWarp;
    m_clientWidth = resWidth;
    m_clientHeight = resHeight;
    m_isVSync = isUseVSync;

    m_isTearingSupported = CheckTearingSupport();

    m_time = m_clock.now();
}

Renderer::~Renderer() {
    // Make sure the command queue has finished all commands before closing.
    Flush(m_pCommandQueue, m_pFence, m_fenceValue, m_fenceEvent);

    ::CloseHandle(m_fenceEvent);
}

inline bool Renderer::isInitialized() const {
    return m_isInitialized;
}

void Renderer::switchVSync() {
    m_isVSync = !m_isVSync;
}

void Renderer::initialize(HWND hWnd) {
    ComPtr<IDXGIAdapter4> dxgiAdapter4{ GetAdapter(m_useWarp) };

    m_pDevice = CreateDevice(dxgiAdapter4);

    m_pCommandQueue = CreateCommandQueue(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

    m_pSwapChain = CreateSwapChain(hWnd, m_pCommandQueue, m_clientWidth, m_clientHeight, m_numFrames);

    m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();

    m_pRTVDescriptorHeap = CreateDescriptorHeap(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, m_numFrames);

    // Get the size of the handle increment for the given type of descriptor heap.
    // This value is typically used to increment a handle into a descriptor array by the correct amount.
    m_RTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews(m_pDevice, m_pSwapChain, m_pRTVDescriptorHeap);

    for (int i{}; i < m_numFrames; ++i) {
        m_pCommandAllocators[i] = CreateCommandAllocator(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    m_pCommandList = CreateCommandList(
        m_pDevice,
        m_pCommandAllocators[m_currBackBufferId],
        D3D12_COMMAND_LIST_TYPE_DIRECT
    );

    m_pFence = CreateFence(m_pDevice);
    m_fenceEvent = CreateEventHandle();

    m_isInitialized = true;
}

void Renderer::Update() {
    assert(m_isInitialized);

    m_frameCounter++;
    auto t1 = m_clock.now();
    auto deltaTime = t1 - m_time;
    m_time = t1;

    m_elapsedSeconds += deltaTime.count() * 1e-9;
    if (m_elapsedSeconds > 1.0) {
        auto fps = m_frameCounter / m_elapsedSeconds;

        m_frameCounter = 0;
        m_elapsedSeconds = 0.0;
    }
}

void Renderer::Render() {
    assert(m_isInitialized);

    auto& commandAllocator = m_pCommandAllocators[m_currBackBufferId];
    auto& backBuffer = m_pBackBuffers[m_currBackBufferId];

    // Before overwriting the contents of the current back buffer with the content of the next frame,
    // the CPU thread is stalled using the WaitForFenceValue function described earlier.
    WaitForFenceValue(m_pFence, m_frameFenceValues[m_currBackBufferId], m_fenceEvent);

    // Before any commands can be recorded into the command list, 
    // the command allocator and command list needs to be reset to its initial state.
    commandAllocator->Reset();
    m_pCommandList->Reset(commandAllocator.Get(), nullptr);

    // Clear the render target.
    {
        // Before the render target can be cleared, it must be transitioned to the RENDER_TARGET state.
        CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ) };

        // Notifies the driver that it needs to synchronize multiple accesses to resources
        m_pCommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        // CPU descriptor handle to a RTV
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
            m_pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),  // RTV desc heap start
            m_currBackBufferId,                                   // current index (offset) from start
            m_RTVDescriptorSize                                         // RTV desc size
        );

        m_pCommandList->ClearRenderTargetView(
            rtv,        // cpu desc handle
            clearColor, // color to fill RTV
            0,          // number of rectangles in array
            nullptr     // array of rectangles in resource view, if nullptr then clears entire resouce view
        );
    }

    // Present
    {
        CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
        ) };
        m_pCommandList->ResourceBarrier(1, &barrier);

        // This method must be called on the command list before being executed on the command queue
        ThrowIfFailed(m_pCommandList->Close());

        ID3D12CommandList* const commandLists[]{ m_pCommandList.Get() };
        m_pCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        UINT syncInterval{ UINT(m_isVSync ? 1 : 0) };
        UINT presentFlags{ m_isTearingSupported && !m_isVSync ? DXGI_PRESENT_ALLOW_TEARING : 0 };
        ThrowIfFailed(m_pSwapChain->Present(
            // 0 - Cancel the remaining time on the previously presented frame 
            // and discard this frame if a newer frame is queued
            // n = 1...4 - Synchronize presentation for at least n vertical blanks
            syncInterval,
            presentFlags
        ));

        // Immediately after presenting the rendered frame to the screen, a signal is inserted into the queue
        m_frameFenceValues[m_currBackBufferId] = Signal(m_pCommandQueue, m_pFence, m_fenceValue);

        // get the index of the swap chains current back buffer,
        // as order of back buffer indicies is not guaranteed to be sequential,
        // when using the DXGI_SWAP_EFFECT_FLIP_DISCARD flip model
        m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();
    }
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    assert(m_isInitialized);

    if (m_clientWidth == width && m_clientHeight == height)
        return;

    // Don't allow 0 size swap chain back buffers.
    m_clientWidth = std::max(1u, width);
    m_clientHeight = std::max(1u, height);

    // Flush the GPU queue to make sure the swap chain's back buffers
    // are not being referenced by an in-flight command list.
    Flush(m_pCommandQueue, m_pFence, m_fenceValue, m_fenceEvent);

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

    UpdateRenderTargetViews(m_pDevice, m_pSwapChain, m_pRTVDescriptorHeap);
}

void Renderer::EnableDebugLayer() {
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    //ThrowIfFailed(D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&debugInterface))); // TODO
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
}

ComPtr<IDXGIAdapter4> Renderer::GetAdapter(bool useWarp) {
    // IDXGIFactory
    // An IDXGIFactory interface implements methods for generating DXGI objects (which handle full screen transitions)
    ComPtr<IDXGIFactory6> dxgiFactory;

    // Enabling the DXGI_CREATE_FACTORY_DEBUG flag during factory creation enables errors
    // to be caught during device creation and while querying for the adapters
    UINT createFactoryFlags{};
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    // CreateDXGIFactory2
    // Creates a DXGI factory that you can use to generate other DXGI objects
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    // The IDXGIAdapter interface represents a display subsystem (including one or more GPUs, DACs and video memory)
    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (useWarp) {
        // IDXGIFactory4::EnumWarpAdapter
        // Provides an adapter which can be provided to D3D12CreateDevice to use the WARP renderer
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
        return dxgiAdapter4;
    }

    for (UINT adapterIndex{};
        dxgiFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,   // The GPU preference for the app
            IID_PPV_ARGS(&dxgiAdapter1)
        ) != DXGI_ERROR_NOT_FOUND;
        ++adapterIndex
        ) {
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
        ThrowIfFailed(dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1));

        if (!(dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            // Creates a device that represents the display adapter
            && SUCCEEDED(D3D12CreateDevice(
                dxgiAdapter1.Get(),     // A pointer to the video adapter to use when creating a device
                D3D_FEATURE_LEVEL_11_0, // Feature Level
                __uuidof(ID3D12Device), // Device interface GUID
                nullptr                 // A pointer to a memory block that receives a pointer to the device
            ))) {
            ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
        }
    }

    return dxgiAdapter4;
}

ComPtr<ID3D12Device2> Renderer::CreateDevice(ComPtr<IDXGIAdapter4> adapter) {
    // Represents a virtual adapter
    ComPtr<ID3D12Device2> d3d12Device2;
    // D3D12CreateDevice
    // Creates a device that represents the display adapter
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

    // Enable debug messages in debug mode.
#if defined(_DEBUG)
    // ID3D12InfoQueue
    // An information-queue interface stores, retrieves, and filters debug messages
    // The queue consists of a message queue, an optional storage filter stack, and a optional retrieval filter stack
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue))) {
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

    return d3d12Device2;
}

ComPtr<ID3D12CommandQueue> Renderer::CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
    // ID3D12CommandQueue Provides methods for submitting command lists, synchronizing command list execution,
    // instrumenting the command queue, and updating resource tile mappings.
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

    D3D12_COMMAND_QUEUE_DESC desc{
        .Type{ type },
        .Priority{ D3D12_COMMAND_QUEUE_PRIORITY_NORMAL },
        .Flags{ D3D12_COMMAND_QUEUE_FLAG_NONE },
        .NodeMask{}
    };

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

    return d3d12CommandQueue;
}

bool Renderer::CheckTearingSupport() {
    BOOL allowTearing{};

    // Rather than create the DXGI 1.5 factory interface directly, we create the
    // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
    // graphics debugging tools which will not support the 1.5 factory interface 
    // until a future update. ??? TODO
    ComPtr<IDXGIFactory4> factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)))) {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5))) {
            allowTearing = SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allowTearing,
                sizeof(allowTearing)
            ));
        }
    }

    return allowTearing;
}

ComPtr<IDXGISwapChain4> Renderer::CreateSwapChain(HWND hWnd, ComPtr<ID3D12CommandQueue> commandQueue, uint32_t width, uint32_t height, uint32_t bufferCount) {
    // An IDXGISwapChain interface implements one or more surfaces
    // for storing rendered data before presenting it to an output
    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4> dxgiFactory4;

    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

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

    ComPtr<IDXGISwapChain1> swapChain1;
    // Creates a swap chain that is associated with an HWND handle to the output window for the swap chain
    ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,        // description of a full-screen swap chain
        nullptr,        // pointer to interface for the output to restrict content to
        &swapChain1
    ));

    // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
    // will be handled manually.
    ThrowIfFailed(dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));
    return dxgiSwapChain4;
}

ComPtr<ID3D12DescriptorHeap> Renderer::CreateDescriptorHeap(ComPtr<ID3D12Device2> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
    // ID3D12DescriptorHeap (array of resource views)
    // A descriptor heap is a collection of contiguous allocations of descriptors, one
    // allocation for every descriptor. Descriptor heaps contain many object types that are
    // not part of a Pipeline State Object (PSO), such as Shader Resource Views (SRVs),
    // Unordered Access Views (UAVs), Constant Buffer Views (CBVs), and Samplers
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc{
        .Type{ type },
        .NumDescriptors{ numDescriptors }
    };

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

// Create/Update RTVs
void Renderer::UpdateRenderTargetViews(ComPtr<ID3D12Device2> device, ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap) {
    UINT rtvDescriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };

    // Get the CPU descriptor handle that represents the start of the heap
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i{}; i < m_numFrames; ++i) {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(
            backBuffer.Get(),   // ID3D12Resource that represents a render target
            nullptr,            // RTV desc
            rtvHandle           // new RTV dest
        );

        m_pBackBuffers[i] = backBuffer;

        // increment to the next handle in the descriptor heap
        rtvHandle.Offset(rtvDescriptorSize);
    }
}

ComPtr<ID3D12CommandAllocator> Renderer::CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
    // ID3D12CommandAllocator
    // Represents the allocations of storage for GPU commands
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> Renderer::CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type) {
    // Encapsulates a list of graphics commands for rendering
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ThrowIfFailed(device->CreateCommandList(
        0,                          // for multi-adapter systems
        type,                       // type of command list to create
        commandAllocator.Get(),     // pointer to the command allocator
        nullptr,                    // pointer to the pipeline state
        IID_PPV_ARGS(&commandList)
    ));

    // Indicates that recording to the command list has finished
    ThrowIfFailed(commandList->Close());

    return commandList;
}

ComPtr<ID3D12Fence> Renderer::CreateFence(ComPtr<ID3D12Device2> device) {
    // Represents a fence, an object used for synchronization of the CPU and one or more GPUs
    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(
        0,                      // init value
        D3D12_FENCE_FLAG_NONE,  // no flags
        IID_PPV_ARGS(&fence)
    ));

    return fence;
}

HANDLE Renderer::CreateEventHandle() {
    HANDLE fenceEvent{ ::CreateEvent(NULL, FALSE, FALSE, NULL) };

    assert(fenceEvent && "Failed to create fence event.");

    return fenceEvent;
}

// The Signal function is used to signal the fence from the GPU

uint64_t Renderer::Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue) {
    uint64_t fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(
        fence.Get(),        // pointer to the ID3D12Fence object
        fenceValueForSignal // value to set the fence to
    ));

    return fenceValueForSignal;
}

void Renderer::WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent, std::chrono::milliseconds duration) {
    // Gets the current value of the fence
    if (fence->GetCompletedValue() < fenceValue) {
        // Specifies an event that's raised when the fence reaches a certain value
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        // Waits until the specified object is in the signaled state or the time-out interval elapses
        ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
    }
}

// Ensure that any commands previously executed on the GPU have finished executing 
// before the CPU thread is allowed to continue processing
void Renderer::Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent) {
    uint64_t fenceValueForSignal{ Signal(commandQueue, fence, fenceValue) };
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}
