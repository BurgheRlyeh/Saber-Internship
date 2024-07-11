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

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "Atlas.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "PSOLibrary.h"
#include "RenderObject.h"
#include "Resources.h"
#include "Scene.h"
#include "JobSystem.h"

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
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain{};
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_pBackBuffers{ m_numFrames };
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVDescHeap{};
	UINT m_RTVDescriptorSize{};
	UINT m_currBackBufferId{};
	
    std::vector<uint64_t> m_frameFenceValues{ m_numFrames };

	// By default, enable V-Sync.
	// Can be toggled with the V key.
    bool m_isTearingSupported{};
    bool m_isVSync{};

    uint64_t m_frameCounter{};
    double m_elapsedSeconds{};
    std::chrono::high_resolution_clock m_clock{};
    std::chrono::steady_clock::time_point m_time{};

    // render thread sync
    std::thread m_renderThread{};
    std::mutex m_renderThreadMutex{};
    std::atomic<bool> m_isRenderThreadRunning{};

    // for resize
    std::atomic<bool> m_isNeedResize{};
    std::atomic<uint32_t> m_resolutionWidthForResize{};
    std::atomic<uint32_t> m_resolutionHeightForResize{};

    std::shared_ptr<CommandQueue> m_pCommandQueueDirect{};
    std::shared_ptr<CommandQueue> m_pCommandQueueCopy{};

    // Depth buffer.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pDepthStencilBuffer{};
    // Descriptor heap for depth buffer.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDSVDescHeap{};

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{ CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX) };

    //Scene m_scene{};
    std::vector<Scene> m_scenes{};
    size_t m_currSceneId{ 1 };
    bool m_isCurrentHandLeft{};

    std::atomic<size_t> m_nextSceneId{ m_currSceneId };
    std::atomic<bool> m_isSwitchToNextCamera{};
    std::atomic<bool> m_isLeftHand{ m_isCurrentHandLeft };

    // Atlases
    std::shared_ptr<Atlas<Mesh>> m_pMeshAtlas{};
    std::shared_ptr<Atlas<ShaderResource>> m_pShaderAtlas{};
    std::shared_ptr<Atlas<RootSignatureResource>> m_pRootSignatureAtlas{};
    std::shared_ptr<PSOLibrary> m_pPSOLibrary{};
    //std::shared_ptr<Atlas<PipelineStateResource>> m_pPipelineStateAtlas{};
    //std::shared_ptr<MaterialAtlas<>> m_pMaterialAtlas{};

    std::shared_ptr<JobSystem<2>> m_pJobSystem{};

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

    void Initialize(HWND hWnd);

    bool StartRenderThread();
    void StopRenderThread();

    void SwitchVSync();

    void SetSceneId(size_t sceneId);

    void SwitchToNextCamera();

    void SwitchHand();

    void Resize(uint32_t width, uint32_t height);

    void PerformResize();
    void Update();
    void Render();

private:
    void RenderLoop();

    void ResizeDepthBuffer();

    bool CheckTearingSupport();

    void EnableDebugLayer();

    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp);
    Microsoft::WRL::ComPtr<ID3D12Device2> CreateDevice(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter);
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        D3D12_COMMAND_LIST_TYPE type
    );
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
    void CreateRenderTargetViews(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap
    );

    // Ensure that any commands previously executed on the GPU have finished executing 
    // before the CPU thread is allowed to continue processing
    void Flush();

    void TransitionResource(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter
    );

    void CreateDSVDescHeap();
};