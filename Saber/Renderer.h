#pragma once

#include "Headers.h"

#include <algorithm>
#include <chrono>
#include <vector>

#include <atomic>
#include <mutex>

#include "Scene.h"
#include "JobSystem.h"

class DepthBuffer;
class DescRange;
class Device;
class DeviceContext;
class GBuffer;
class Scene;
class Texture;
class TextureResource;

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

    std::shared_ptr<DeviceContext> m_pDeviceContext{};

    // todo: move to some swapchain wrapper
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain{};
    std::vector<std::shared_ptr<TextureResource>> m_pBackBuffers{};
    std::shared_ptr<DescRange> m_pBackBuffersDescHeapRange{};
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
    std::chrono::steady_clock::time_point m_sceneTime{};

    // render thread sync
    std::thread m_renderThread{};
    std::mutex m_renderThreadMutex{};
    std::atomic<bool> m_isRenderThreadRunning{};

    // for resize
    std::atomic<bool> m_isNeedResize{};
    std::atomic<uint32_t> m_resolutionWidthForResize{};
    std::atomic<uint32_t> m_resolutionHeightForResize{};

    // Depth buffer.
    std::vector<std::shared_ptr<DepthBuffer>> m_pDepthBuffers{};

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{ CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX) };

    std::vector<std::unique_ptr<Scene>> m_pScenes{};
    size_t m_currSceneId{ 0 };

    std::atomic<size_t> m_nextSceneId{ m_currSceneId };
    std::atomic<bool> m_isSwitchToNextCamera{};
    std::atomic<bool> m_isSwitchCameraProjection{};

    std::vector<std::shared_ptr<GBuffer>> m_pGBuffers{};

    std::shared_ptr<JobSystem<>> m_pJobSystem{};

public:
    Renderer(const Renderer&) = delete;

    Renderer(
        std::shared_ptr<JobSystem<>> pJobSystem,
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
    void SwitchCameraProjection();

    void Resize(uint32_t width, uint32_t height);

    void PerformResize();
    void Update();
    void Render();

    void MoveCamera(float forwardCoef, float rightCoef);
    void RotateCamera(float deltaX, float deltaY);
    void ZoomCamera(float delta);

private:
    void RenderLoop();

    bool CheckTearingSupport();

#if defined(_DEBUG)
    void EnableDebugLayer();
    void EnableGPUBasedValidation();
    void EnableDRED();
#endif

	Microsoft::WRL::ComPtr<IDXGIFactory6> CreateDxgiFactory(UINT createFlags = 0) const;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDxgiAdapterWarp(
		Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory
	) const;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDxgiAdapterByPreference(
		Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory,
		const DXGI_GPU_PREFERENCE& preference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
		size_t id = 0,
		const DXGI_ADAPTER_FLAG& flags = DXGI_ADAPTER_FLAG_NONE
	) const;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDxgiAdapterByVideoMemory(
		Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory,
		size_t id = 0,
		const DXGI_ADAPTER_FLAG& flags = DXGI_ADAPTER_FLAG_NONE
	) const;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain(
        HWND hWnd,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        uint32_t width,
        uint32_t height,
        uint32_t bufferCount
    );
    std::vector<std::shared_ptr<TextureResource>> CreateBackBuffers(
        std::shared_ptr<Device> pDevice,
        Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
        std::shared_ptr<DescRange> pDescHeapRange
    );

    // Ensure that any commands previously executed on the GPU have finished executing 
    // before the CPU thread is allowed to continue processing
    void Flush();
};