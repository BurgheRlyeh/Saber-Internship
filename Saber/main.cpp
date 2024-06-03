#include "Headers.h"

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows Header Files
#include <windows.h>

#include <shellapi.h> // For CommandLineToArgvW

// To avoid conflicts and use only min/max defined in <algorithm>
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <d3dx12.h>

// STL Headers
#include <algorithm>
#include <cassert>
#include <chrono>


// The number of swap chain back buffers.
const uint8_t g_NumFrames = 3;

// Use WARP adapter
bool g_UseWarp = false;

// Window client area size
uint32_t g_ClientWidth = 1280;
uint32_t g_ClientHeight = 720;

// Set to true once the DX12 objects have been initialized.
bool g_IsInitialized{};

// Window handle
HWND g_hWnd;

// Window rectangle (used to toggle fullscreen state)
RECT g_WindowRect;

// DirectX 12 Objects
ComPtr<ID3D12Device2> g_Device;
ComPtr<ID3D12CommandQueue> g_CommandQueue;
ComPtr<IDXGISwapChain4> g_SwapChain;
ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
ComPtr<ID3D12GraphicsCommandList> g_CommandList;
ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
UINT g_RTVDescriptorSize;
UINT g_CurrentBackBufferIndex;

// Synchronization objects
ComPtr<ID3D12Fence> g_Fence;
uint64_t g_FenceValue = 0;
uint64_t g_FrameFenceValues[g_NumFrames] = {};
HANDLE g_FenceEvent;

// By default, enable V-Sync.
// Can be toggled with the V key.
bool g_VSync = true;
bool g_TearingSupported = false;
// By default, use windowed mode.
// Can be toggled with the Alt+Enter or F11
bool g_Fullscreen = false;

// Window callback function.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);


void ParseCommandLineArguments() {
    int argc{};
    wchar_t** argv{ ::CommandLineToArgvW(::GetCommandLineW(), &argc) };

    for (size_t i{}; i < argc; ++i) {
        // Width
        if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
            g_ClientWidth = ::wcstol(argv[++i], nullptr, 10);

        // Height
        if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
            g_ClientHeight = ::wcstol(argv[++i], nullptr, 10);

        // Is using warp renderer
        g_UseWarp = ::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0;
    }

    // Free memory allocated by CommandLineToArgvW
    ::LocalFree(argv);
}

void EnableDebugLayer() {
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    //ThrowIfFailed(D3D12GetInterface(CLSID_D3D12Debug, IID_PPV_ARGS(&debugInterface))); // TODO
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName) {
    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass{
        .cbSize{ sizeof(WNDCLASSEX) },
        .style{ CS_HREDRAW | CS_VREDRAW },
        .lpfnWndProc{ &WndProc },
        .cbClsExtra{},
        .cbWndExtra{},
        .hInstance{ hInst },
        .hIcon{ ::LoadIcon(hInst, NULL) },
        .hCursor{ ::LoadCursor(NULL, IDC_ARROW) },
        .hbrBackground{ (HBRUSH)(COLOR_WINDOW + 1) },
        .lpszMenuName{},
        .lpszClassName{ windowClassName },
        .hIconSm{ ::LoadIcon(hInst, NULL) }
    };

    assert(::RegisterClassExW(&windowClass) > 0);
}

HWND CreateAppWindow(
    const wchar_t* windowClassName,
    HINSTANCE hInst,
    const wchar_t* windowTitle,
    uint32_t width,
    uint32_t height
) {
    int screenWidth{ ::GetSystemMetrics(SM_CXSCREEN) };     // The width of the screen of the primary display monitor, in pixels
    int screenHeight{ ::GetSystemMetrics(SM_CYSCREEN) };    // The height of the screen of the primary display monitor, in pixels

    RECT windowRect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    ::AdjustWindowRect(
        &windowRect,
        WS_OVERLAPPEDWINDOW,    // The window style of the window whose required size is to be calculated
        FALSE                   // Indicates whether the window has a menu
    );

    int windowWidth{ windowRect.right - windowRect.left };
    int windowHeight{ windowRect.bottom - windowRect.top };

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX{ std::max<int>(0, (screenWidth - windowWidth) / 2) };
    int windowY{ std::max<int>(0, (screenHeight - windowHeight) / 2) };

    HWND hWnd{ ::CreateWindowExW(
        NULL,                   // The extended window style of the window being created
        windowClassName,        // Class name got with RegisterClass(Ex)
        windowTitle,            // The window name
        WS_OVERLAPPEDWINDOW,    // The style of the window being created
        windowX,                // The initial horizontal position of the window
        windowY,                // The initial vertical position of the window
        windowWidth,            // The width, in device units, of the window
        windowHeight,           // The height, in device units, of the window
        NULL,                   // A handle to the parent or owner window of the window being created
        NULL,                   // A handle to a menu
        hInst,                  // A handle to the instance of the module to be associated with the window
        nullptr
    ) };

    assert(hWnd && "Failed to create window");

    return hWnd;
}

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp) {
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

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter) {
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

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
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

bool CheckTearingSupport() {
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

ComPtr<IDXGISwapChain4> CreateSwapChain(
    HWND hWnd,
    ComPtr<ID3D12CommandQueue> commandQueue,
    uint32_t width,
    uint32_t height,
    uint32_t bufferCount
) {
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


ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
    ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t numDescriptors
) {
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

// Cretate RTVs
void UpdateRenderTargetViews(
    ComPtr<ID3D12Device2> device,
    ComPtr<IDXGISwapChain4> swapChain,
    ComPtr<ID3D12DescriptorHeap> descriptorHeap
) {
    UINT rtvDescriptorSize{ device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) };

    // Get the CPU descriptor handle that represents the start of the heap
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i{}; i < g_NumFrames; ++i) {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(
            backBuffer.Get(),   // ID3D12Resource that represents a render target
            nullptr,            // RTV desc
            rtvHandle           // new RTV dest
        );

        g_BackBuffers[i] = backBuffer;

        // increment to the next handle in the descriptor heap
        rtvHandle.Offset(rtvDescriptorSize);
    }
}


ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
    ComPtr<ID3D12Device2> device,
    D3D12_COMMAND_LIST_TYPE type
) {
    // ID3D12CommandAllocator
    // Represents the allocations of storage for GPU commands
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(
    ComPtr<ID3D12Device2> device,
    ComPtr<ID3D12CommandAllocator> commandAllocator,
    D3D12_COMMAND_LIST_TYPE type
) {
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

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device) {
    // Represents a fence, an object used for synchronization of the CPU and one or more GPUs
    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(
        0,                      // init value
        D3D12_FENCE_FLAG_NONE,  // no flags
        IID_PPV_ARGS(&fence)
    ));

    return fence;
}

HANDLE CreateEventHandle() {
    HANDLE fenceEvent{ ::CreateEvent(NULL, FALSE, FALSE, NULL) };

    assert(fenceEvent && "Failed to create fence event.");

    return fenceEvent;
}

// The Signal function is used to signal the fence from the GPU
uint64_t Signal(
    ComPtr<ID3D12CommandQueue> commandQueue,
    ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue
) {
    uint64_t fenceValueForSignal = ++fenceValue;
    ThrowIfFailed(commandQueue->Signal(
        fence.Get(),        // pointer to the ID3D12Fence object
        fenceValueForSignal // value to set the fence to
    ));

    return fenceValueForSignal;
}

void WaitForFenceValue(
    ComPtr<ID3D12Fence> fence,
    uint64_t fenceValue,
    HANDLE fenceEvent,
    std::chrono::milliseconds duration = std::chrono::milliseconds::max()
) {
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
void Flush(
    ComPtr<ID3D12CommandQueue> commandQueue,
    ComPtr<ID3D12Fence> fence,
    uint64_t& fenceValue,
    HANDLE fenceEvent
) {
    uint64_t fenceValueForSignal{ Signal(commandQueue, fence, fenceValue) };
    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

void Update() {
    static uint64_t frameCounter{};
    static double elapsedSeconds{};
    static std::chrono::high_resolution_clock clock{};
    static auto t0 = clock.now();

    frameCounter++;
    auto t1 = clock.now();
    auto deltaTime = t1 - t0;
    t0 = t1;

    elapsedSeconds += deltaTime.count() * 1e-9;
    if (elapsedSeconds > 1.0) {
        auto fps = frameCounter / elapsedSeconds;

        frameCounter = 0;
        elapsedSeconds = 0.0;
    }
}

void Render() {
    auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
    auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];

    // Before any commands can be recorded into the command list, 
    // the command allocator and command list needs to be reset to its initial state.
    commandAllocator->Reset();
    g_CommandList->Reset(commandAllocator.Get(), nullptr);

    // Clear the render target.
    {
        // Before the render target can be cleared, it must be transitioned to the RENDER_TARGET state.
        CD3DX12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ) };

        // Notifies the driver that it needs to synchronize multiple accesses to resources
        g_CommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        // CPU descriptor handle to a RTV
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
            g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),  // RTV desc heap start
            g_CurrentBackBufferIndex,                                   // current index (offset) from start
            g_RTVDescriptorSize                                         // RTV desc size
        );

        g_CommandList->ClearRenderTargetView(
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
        g_CommandList->ResourceBarrier(1, &barrier);

        // This method must be called on the command list before being executed on the command queue
        ThrowIfFailed(g_CommandList->Close());

        ID3D12CommandList* const commandLists[]{ g_CommandList.Get() };
        g_CommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

        UINT syncInterval{ UINT(g_VSync ? 1 : 0) };
        UINT presentFlags{ g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0 };
        ThrowIfFailed(g_SwapChain->Present(
            // 0 - Cancel the remaining time on the previously presented frame 
            // and discard this frame if a newer frame is queued
            // n = 1...4 - Synchronize presentation for at least n vertical blanks
            syncInterval,
            presentFlags
        ));

        // Immediately after presenting the rendered frame to the screen, a signal is inserted into the queue
        g_FrameFenceValues[g_CurrentBackBufferIndex] = Signal(g_CommandQueue, g_Fence, g_FenceValue);

        // get the index of the swap chains current back buffer,
        // as order of back buffer indicies is not guaranteed to be sequential,
        // when using the DXGI_SWAP_EFFECT_FLIP_DISCARD flip model
        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        // Before overwriting the contents of the current back buffer with the content of the next frame,
        // the CPU thread is stalled using the WaitForFenceValue function described earlier.
        WaitForFenceValue(g_Fence, g_FrameFenceValues[g_CurrentBackBufferIndex], g_FenceEvent);
    }
}

void Resize(uint32_t width, uint32_t height) {
    if (g_ClientWidth != width || g_ClientHeight != height) {
        // Don't allow 0 size swap chain back buffers.
        g_ClientWidth = std::max(1u, width);
        g_ClientHeight = std::max(1u, height);

        // Flush the GPU queue to make sure the swap chain's back buffers
        // are not being referenced by an in-flight command list.
        Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

        // Any references to the back buffers must be released
        // before the swap chain can be resized.
        for (int i{}; i < g_NumFrames; ++i) {
            g_BackBuffers[i].Reset();
            g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        ThrowIfFailed(g_SwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(g_SwapChain->ResizeBuffers(
            g_NumFrames,                        // number of buffers in swap chain
            g_ClientWidth,                      // new width of back buffer
            g_ClientHeight,                     // new height of back buffer
            swapChainDesc.BufferDesc.Format,    // new format of back buffer
            swapChainDesc.Flags                 // flags
        ));

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);
    }
}

void SetFullscreen(bool fullscreen) {
    if (g_Fullscreen == fullscreen)
        return;

    g_Fullscreen = fullscreen;

    // Switching to fullscreen.
    if (g_Fullscreen) {
        // Store the current window dimensions so they can be restored 
        // when switching out of fullscreen state.
        ::GetWindowRect(g_hWnd, &g_WindowRect);

        // Set the window style to a borderless window so the client area fills
        // the entire screen.
        UINT windowStyle{ WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX) };

        ::SetWindowLongW(g_hWnd, GWL_STYLE, windowStyle);

        // Query the name of the nearest display device for the window.
        // This is required to set the fullscreen dimensions of the window
        // when using a multi-monitor setup.
        HMONITOR hMonitor{ ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST) };
        MONITORINFOEX monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);

        ::GetMonitorInfo(hMonitor, &monitorInfo);

        ::SetWindowPos(
            g_hWnd,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE
        );

        ::ShowWindow(g_hWnd, SW_MAXIMIZE);
    }
    else {
        // Restore all the window decorators.
        ::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

        ::SetWindowPos(g_hWnd, HWND_NOTOPMOST,
            g_WindowRect.left,
            g_WindowRect.top,
            g_WindowRect.right - g_WindowRect.left,
            g_WindowRect.bottom - g_WindowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(g_hWnd, SW_NORMAL);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_IsInitialized) {
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }


    switch (message) {
    case WM_PAINT: {
        Update();
        Render();
        break;
    }
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
        bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        switch (wParam) {
        case 'V':
            g_VSync = !g_VSync;
            break;
        case VK_ESCAPE:
            ::PostQuitMessage(0);
            break;
        case VK_RETURN:
            if (alt) {
        case VK_F11:
            SetFullscreen(!g_Fullscreen);
            }
            break;
        }
        break;
    }
    // The default window procedure will play a system notification sound 
    // when pressing the Alt+Enter keyboard combination if this message is 
    // not handled.
    case WM_SYSCHAR:
        break;
    case WM_SIZE: {
        RECT clientRect = {};
        ::GetClientRect(g_hWnd, &clientRect);

        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        Resize(width, height);
    }
    break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    default:
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return 0;
}

// Main Win32 apps entry point
int CALLBACK wWinMain(
    HINSTANCE hInst,        // Handle to instance/module
    HINSTANCE hPrevInst,    // Has no meaning (used in 16-bit window, now always zero)
    PWSTR lpCmdLine,        // Cmd-line args as Unicode string
    int nCmdShow            // is minimized / maximized / shown normally
) {
    // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
    // Using this awareness context allows the client area of the window 
    // to achieve 100% scaling while still allowing non-client window content to 
    // be rendered in a DPI sensitive fashion.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Window class name. Used for registering / creating the window.
    const wchar_t* windowClassName = L"SaberInternshipWindowClass";
    ParseCommandLineArguments();

    // Always enable the debug layer before doing anything DX12 related
    // so all possible errors generated while creating DX12 objects
    // are caught by the debug layer.
    EnableDebugLayer();

    g_TearingSupported = CheckTearingSupport();

    RegisterWindowClass(hInst, windowClassName);
    g_hWnd = CreateAppWindow(windowClassName, hInst, L"Saber Internship", g_ClientWidth, g_ClientHeight);

    // Initialize the global window rect variable.
    ::GetWindowRect(g_hWnd, &g_WindowRect);

    ComPtr<IDXGIAdapter4> dxgiAdapter4{ GetAdapter(g_UseWarp) };

    g_Device = CreateDevice(dxgiAdapter4);

    g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

    g_SwapChain = CreateSwapChain(g_hWnd, g_CommandQueue, g_ClientWidth, g_ClientHeight, g_NumFrames);

    g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

    g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);

    // Get the size of the handle increment for the given type of descriptor heap.
    // This value is typically used to increment a handle into a descriptor array by the correct amount.
    g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);

    for (int i{}; i < g_NumFrames; ++i) {
        g_CommandAllocators[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }
    g_CommandList = CreateCommandList(
        g_Device,
        g_CommandAllocators[g_CurrentBackBufferIndex],
        D3D12_COMMAND_LIST_TYPE_DIRECT
    );

    g_Fence = CreateFence(g_Device);
    g_FenceEvent = CreateEventHandle();

    g_IsInitialized = true;

    ::ShowWindow(g_hWnd, SW_SHOW);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    // Make sure the command queue has finished all commands before closing.
    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

    ::CloseHandle(g_FenceEvent);

    return 0;
}