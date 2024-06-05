#pragma once

#include "Headers.h"

// To avoid conflicts and use only min/max defined in <algorithm>
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <algorithm>
#include <chrono>
#include <vector>

class Renderer {
	// The number of swap chain back buffers.
    uint8_t m_numFrames{};

	// Use WARP adapter
    bool m_useWarp{};

	// Window client area size
    uint32_t m_clientWidth{};
    uint32_t m_clientHeight{};

	// Set to true once the DX12 objects have been initialized.
	bool m_isInitialized{};

	// DirectX 12 Objects
    Microsoft::WRL::ComPtr<ID3D12Device2> m_pDevice{};
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue{};
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain{};
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_pBackBuffers{ m_numFrames };
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList{};
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_pCommandAllocators{ m_numFrames };
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVDescriptorHeap{};
	UINT m_RTVDescriptorSize{};
	UINT m_currBackBufferId{};

	// Synchronization objects
    Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{};
    uint64_t m_fenceValue{};
	
    std::vector<uint64_t> m_frameFenceValues{ m_numFrames };

    HANDLE m_fenceEvent{};

	// By default, enable V-Sync.
	// Can be toggled with the V key.
    bool m_isVSync{};
    bool m_isTearingSupported{};

    uint64_t m_frameCounter{};
    double m_elapsedSeconds{};
    std::chrono::high_resolution_clock m_clock{};
    std::chrono::steady_clock::time_point m_time{};

public:
    Renderer(const Renderer&) = delete;
    Renderer(
        uint8_t backBuffersCnt = 3,
        bool isUseWarp = false,
        uint32_t resWidth = 1280,
        uint32_t resHeight = 720,
        bool isUseVSync = true
    );

    ~Renderer();

    bool isInitialized() const;

    void switchVSync();

    void initialize(HWND hWnd);

    void Update();

    void Render();

    void Resize(uint32_t width, uint32_t height);

private:
    void EnableDebugLayer();

    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);

    Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);

    bool CheckTearingSupport();

    Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain(
        HWND hWnd,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        uint32_t width,
        uint32_t height,
        uint32_t bufferCount
    );

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t numDescriptors
    );

    // Create RTVs
    void UpdateRenderTargetViews(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap
    );

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        D3D12_COMMAND_LIST_TYPE type
    );

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CreateCommandList(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator,
        D3D12_COMMAND_LIST_TYPE type
    );

    Microsoft::WRL::ComPtr<ID3D12Fence> CreateFence(Microsoft::WRL::ComPtr<ID3D12Device2> device);

    HANDLE CreateEventHandle();

    // The Signal function is used to signal the fence from the GPU
    uint64_t Signal(
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        Microsoft::WRL::ComPtr<ID3D12Fence> fence,
        uint64_t& fenceValue
    );

    void WaitForFenceValue(
        Microsoft::WRL::ComPtr<ID3D12Fence> fence,
        uint64_t fenceValue,
        HANDLE fenceEvent,
        std::chrono::milliseconds duration = std::chrono::milliseconds::max()
    );

    // Ensure that any commands previously executed on the GPU have finished executing 
    // before the CPU thread is allowed to continue processing
    void Flush(
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        Microsoft::WRL::ComPtr<ID3D12Fence> fence,
        uint64_t& fenceValue,
        HANDLE fenceEvent
    );

};