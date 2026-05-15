/**
 * @file Renderer.h
 * @brief Top-level D3D12 renderer: device selection, swap chain, scene management,
 *        and the render thread.
 *
 * @ref Renderer owns the @ref DeviceContext, the DXGI swap chain, back buffers,
 * depth buffers, G-buffers, and the scene list.  It drives the render loop from a
 * dedicated thread and marshals camera control / resize requests from the main thread
 * via atomic flags.
 */
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

/**
 * @brief Orchestrates the full D3D12 rendering pipeline.
 *
 * Construction selects the DXGI adapter (WARP or hardware), creates the
 * @ref DeviceContext, allocates back buffers, depth buffers, G-buffers, and
 * scenes.  @ref StartRenderThread / @ref StopRenderThread manage the background
 * render loop.  @ref Resize is safe to call from any thread; the actual resource
 * recreation happens at the start of the next frame.
 */
class Renderer {
    uint8_t m_numFrames{};           /**< @brief Number of swap-chain back buffers. */

    bool m_useWarp{};                /**< @brief Use the WARP software adapter. */

    uint32_t m_clientWidth{};        /**< @brief Current client-area width in pixels. */
    uint32_t m_clientHeight{};       /**< @brief Current client-area height in pixels. */

    bool m_isInitialized{};          /**< @brief Set to @c true after @ref Initialize completes. */

    std::shared_ptr<DeviceContext> m_pDeviceContext{};

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain{};
    std::vector<std::shared_ptr<TextureResource>> m_pBackBuffers{};
    std::shared_ptr<DescRange> m_pBackBuffersDescHeapRange{};
    UINT m_currBackBufferId{};
    std::vector<uint64_t> m_frameFenceValues{ m_numFrames }; /**< @brief Per-frame fence values for CPU-GPU synchronisation. */

    bool m_isTearingSupported{};     /**< @brief Whether the DXGI factory supports tearing. */
    bool m_isVSync{};                /**< @brief V-Sync enabled flag. */

    uint64_t m_frameCounter{};
    double m_elapsedSeconds{};
    std::chrono::high_resolution_clock m_clock{};
    std::chrono::steady_clock::time_point m_time{};
    std::chrono::steady_clock::time_point m_sceneTime{};

    std::thread m_renderThread{};
    std::mutex m_renderThreadMutex{};
    std::atomic<bool> m_isRenderThreadRunning{};

    std::atomic<bool> m_isNeedResize{};                /**< @brief Set when the window has been resized. */
    std::atomic<uint32_t> m_resolutionWidthForResize{}; /**< @brief New width to apply on next frame. */
    std::atomic<uint32_t> m_resolutionHeightForResize{}; /**< @brief New height to apply on next frame. */

    std::vector<std::shared_ptr<DepthBuffer>> m_pDepthBuffers{};

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{ CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX) };

    std::vector<std::unique_ptr<Scene>> m_pScenes{};
    size_t m_currSceneId{ 0 };

    std::atomic<size_t> m_nextSceneId{ m_currSceneId };
    std::atomic<bool> m_isSwitchToNextCamera{};
    std::atomic<bool> m_isSwitchCameraProjection{};

    std::vector<std::shared_ptr<GBuffer>> m_pGBuffers{};

    std::shared_ptr<JobSystem<>> m_pJobSystem{}; /**< @brief Shared worker thread pool. */

public:
    Renderer(const Renderer&) = delete;

    /**
     * @brief Constructs the renderer.  Call @ref Initialize with the HWND to finish setup.
     * @param pJobSystem      Shared worker thread pool.
     * @param backBuffersCnt  Number of swap-chain back buffers (default 3).
     * @param isUseWarp       Use the WARP software adapter (default @c false).
     * @param resWidth        Initial render width (default 1280).
     * @param resHeight       Initial render height (default 720).
     * @param isUseVSync      Enable V-Sync (default @c true).
     */
    Renderer(
        std::shared_ptr<JobSystem<>> pJobSystem,
        uint8_t backBuffersCnt = 3,
        bool isUseWarp = false,
        uint32_t resWidth = 1280,
        uint32_t resHeight = 720,
        bool isUseVSync = true
    );
    ~Renderer();

    /**
     * @brief Completes device and swap-chain initialisation for the given window.
     * @param hWnd Win32 window handle.
     */
    void Initialize(HWND hWnd);

    /**
     * @brief Launches the render thread.
     * @return @c true if the thread was started; @c false if already running.
     */
    bool StartRenderThread();

    /** @brief Signals the render thread to stop and blocks until it exits. */
    void StopRenderThread();

    /** @brief Toggles V-Sync on/off. */
    void SwitchVSync();

    /**
     * @brief Switches to the scene with the given index.
     * @param sceneId Zero-based scene index.
     */
    void SetSceneId(size_t sceneId);

    /** @brief Advances the active camera to the next one in the scene's camera list. */
    void SwitchToNextCamera();

    /** @brief Toggles the active camera between perspective and orthographic projection. */
    void SwitchCameraProjection();

    /**
     * @brief Requests a swap-chain and resource resize (thread-safe).
     * @param width  New client width in pixels.
     * @param height New client height in pixels.
     */
    void Resize(uint32_t width, uint32_t height);

    /** @brief Applies a pending resize request; must be called from the render thread. */
    void PerformResize();
    /** @brief Updates per-frame scene state (cameras, constant buffers). */
    void Update();
    /** @brief Records and submits GPU work for the current frame. */
    void Render();

    /**
     * @brief Translates the active camera along its forward/right axes.
     * @param forwardCoef Forward movement coefficient.
     * @param rightCoef   Right-strafe coefficient.
     */
    void MoveCamera(float forwardCoef, float rightCoef);

    /**
     * @brief Rotates the active camera (yaw/pitch).
     * @param deltaX Horizontal angular delta.
     * @param deltaY Vertical angular delta.
     */
    void RotateCamera(float deltaX, float deltaY);

    /**
     * @brief Zooms the active camera (field-of-view or radius).
     * @param delta Zoom delta.
     */
    void ZoomCamera(float delta);

private:
    /** @brief Main loop executed by the render thread. */
    void RenderLoop();

    /** @brief Queries DXGI for tearing (variable-refresh-rate) support. */
    bool CheckTearingSupport();

#if defined(_DEBUG)
    /** @brief Enables the D3D12 debug layer. */
    void EnableDebugLayer();
    /** @brief Enables GPU-based validation for the debug layer. */
    void EnableGPUBasedValidation();
    /** @brief Enables Device Removed Extended Data (DRED) for crash analysis. */
    void EnableDRED();
#endif

    /** @brief Creates a DXGI factory with the given creation flags. */
    Microsoft::WRL::ComPtr<IDXGIFactory6> CreateDxgiFactory(UINT createFlags = 0) const;

    /** @brief Returns the WARP DXGI adapter. */
    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDxgiAdapterWarp(
        Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory
    ) const;

    /**
     * @brief Returns the DXGI adapter matching the given GPU preference.
     * @param pDxgiFactory DXGI factory.
     * @param preference   GPU preference (default high-performance).
     * @param id           Zero-based adapter index among matching adapters.
     * @param flags        Required adapter flags (default none).
     */
    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDxgiAdapterByPreference(
        Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory,
        const DXGI_GPU_PREFERENCE& preference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        size_t id = 0,
        const DXGI_ADAPTER_FLAG& flags = DXGI_ADAPTER_FLAG_NONE
    ) const;

    /**
     * @brief Returns the DXGI adapter with the most dedicated video memory.
     * @param pDxgiFactory DXGI factory.
     * @param id           Zero-based rank among adapters sorted by VRAM.
     * @param flags        Required adapter flags.
     */
    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetDxgiAdapterByVideoMemory(
        Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory,
        size_t id = 0,
        const DXGI_ADAPTER_FLAG& flags = DXGI_ADAPTER_FLAG_NONE
    ) const;

    /**
     * @brief Creates the DXGI swap chain for the given window and command queue.
     * @param hWnd         Window handle.
     * @param commandQueue D3D12 command queue for presentation.
     * @param width        Back-buffer width.
     * @param height       Back-buffer height.
     * @param bufferCount  Number of back buffers.
     */
    Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain(
        HWND hWnd,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
        uint32_t width,
        uint32_t height,
        uint32_t bufferCount
    );

    /**
     * @brief Wraps each swap-chain back buffer in a @ref TextureResource and
     *        creates RTV descriptors for them.
     * @param pDevice       Device wrapper.
     * @param swapChain     The DXGI swap chain.
     * @param pDescHeapRange Descriptor range allocated for the back-buffer RTVs.
     */
    std::vector<std::shared_ptr<TextureResource>> CreateBackBuffers(
        std::shared_ptr<Device> pDevice,
        Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
        std::shared_ptr<DescRange> pDescHeapRange
    );

    /**
     * @brief Blocks the CPU until all previously submitted GPU work has completed.
     *
     * Used before resize and shutdown to ensure GPU resources are not in use.
     */
    void Flush();
};
