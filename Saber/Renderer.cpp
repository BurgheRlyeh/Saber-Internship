#include "Renderer.h"

#include <cassert>
#include <random>
#include <string>  
#include <iostream> 
#include <sstream>

#include "CommandList.h"
#include "CommandQueue.h"
#include "DeferredShading.h"
#include "DepthBuffer.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "Device.h"
#include "DeviceContext.h"
#include "IncrementFence.h"
#include "IndirectUpdater.h"
#include "MaterialManager.h"
#include "MeshRenderObject.h"
#include "PostProcessing.h"
#include "PSOLibrary.h"
#include "Scene.h"
#include "SinglePassDownSampler.h"
#include "Texture.h"
#include "TextureResource.h"

#include "UIContext.h"

Renderer::Renderer(std::shared_ptr<JobSystem<>> pJobSystem, uint8_t backBuffersCnt, bool isUseWarp, uint32_t resWidth, uint32_t resHeight, bool isUseVSync)
    : m_useWarp(isUseWarp)
    , m_clientWidth(resWidth)
    , m_clientHeight(resHeight)
    , m_isTearingSupported(CheckTearingSupport())
    , m_viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(resWidth), static_cast<float>(resHeight)))
    , m_pJobSystem(pJobSystem)
{
    m_numFrames = backBuffersCnt;
    m_settings.vsync = isUseVSync;
}

Renderer::~Renderer() {
    Flush();
    m_pUI.reset();
    GPUResource::DestroyCounterResetter();
    m_pBackBuffersDescHeapRange.reset();
    m_pScenes.clear();
    m_pGBuffers.clear();
    m_pDepthBuffers.clear();
    m_pDeviceContext->SetMaterialManager(nullptr);
    m_pDeviceContext.reset();
}

void Renderer::Initialize(HWND hWnd) {
    m_pBackBuffers.resize(m_numFrames);
    m_frameFenceValues = std::vector<uint64_t>(m_numFrames, IncrementFence::IncrementFenceInitValue);

#if defined(_DEBUG)
    EnableDebugLayer();
    EnableGPUBasedValidation();
    EnableDRED();
#endif

	UINT factoryCreateFlags{};
#if defined(_DEBUG)
    factoryCreateFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory{ CreateDxgiFactory(factoryCreateFlags) };
    Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter{
        m_useWarp ? GetDxgiAdapterWarp(pFactory) : GetDxgiAdapterByVideoMemory(pFactory)
    };

    constexpr size_t GBUFFER_SIZE{ 2 }; // uvMaterial + tbn
    m_pDeviceContext = std::make_shared<DeviceContext>(pAdapter);
    m_pDeviceContext->InitializeContext(std::to_array<DescriptorHeapManager::DescHeapArgs>({
        // CBV_SRV_UAV
        { 8192, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE },
        // SAMPLER
        {},
        // RTV
        { m_numFrames + GBUFFER_SIZE },
        // DSV
        { 1 }
    }));
    m_pDeviceContext->SetMaterialManager(std::make_shared<MaterialManager>(
        L"../../Resources/Textures/",
        m_pDeviceContext,
        1024
    ));

    m_pSwapChain = CreateSwapChain(hWnd, m_pDeviceContext->GetCommandQueue()->GetD3D12CommandQueue(), m_clientWidth, m_clientHeight, m_numFrames);
    m_currBackBufferId = m_pSwapChain->GetCurrentBackBufferIndex();

    m_pBackBuffersDescHeapRange = m_pDeviceContext->AllocateDescRange(L"BackBuffers", DescRangeType::Rtv, m_numFrames);
    m_pBackBuffers = CreateBackBuffers(m_pDeviceContext->GetDevice(), m_pSwapChain, m_pBackBuffersDescHeapRange);

	m_pDepthBuffers.resize(1);
	m_pDepthBuffers[0] = std::make_shared<DepthBuffer>(
		L"DepthBuffer",
        m_pDeviceContext,
		m_clientWidth,
		m_clientHeight,
		std::make_shared<SinglePassDownsampler>(
			m_pDeviceContext,
			m_clientWidth,
			m_clientHeight
		)
	);

    m_pGBuffers.resize(1);
    m_pGBuffers[0] = std::make_shared<GBuffer>(
        L"GBuffer",
        m_pDeviceContext,
        m_clientWidth,
        m_clientHeight
	);

    m_pUI = std::make_unique<UIContext>(
        hWnd,
        m_pDeviceContext,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_numFrames
    );
    RegisterUIPanels();

    m_time = m_clock.now();
    m_isInitialized = true;

    {
        constexpr size_t ScenesCount{ 4 };
        m_pScenes.resize(ScenesCount);

        auto copyPostProcess{ std::make_shared<CopyPostProcessing>(m_pDeviceContext) };
        auto deferredShading{ DeferredShading::CreateDefferedShadingComputeObject(m_pDeviceContext) };
        for (size_t i{}; i < ScenesCount; ++i) {
            std::unique_ptr<Scene>& pScene{ m_pScenes[i] };
            pScene = std::make_unique<Scene>(
                L"Scene" + std::to_wstring(i),
                m_pDeviceContext,
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
            std::shared_ptr<CommandList> pCommandList{
                m_pDeviceContext->GetCommandQueue()->GetCommandList(m_pDeviceContext->GetDevice())
            };

            pScene->AddObject(RenderSubsystemType::Default, TestTextureRenderObject::CreateTextureCube(
                m_pDeviceContext,
                pCommandList,
                m_pGBuffers[0],
                DirectX::XMMatrixIdentity()
            ));

            m_pDeviceContext->GetCommandQueue()->ExecuteCommandListImmediately(pCommandList);
        };
        sceneObjectAdders[2] = [&](std::unique_ptr<Scene>& pScene) {
            std::shared_ptr<CommandList> pCommandList{
                m_pDeviceContext->GetCommandQueue()->GetCommandList(m_pDeviceContext->GetDevice())
            };

            std::filesystem::path filepath{ L"../../Resources/StaticModels/barbarian_rig_axe_2_a.glb" };
            pScene->AddObject(RenderSubsystemType::Dynamic, TestTextureRenderObject::CreateModelFromGLTF(
                m_pDeviceContext,
                pCommandList,
                filepath,
                m_pGBuffers[0],
                DirectX::XMMatrixScaling(2.f, 2.f, 2.f) * DirectX::XMMatrixTranslation(0.f, -2.f, 0.f)
            ));
            std::filesystem::path filepathGrass{ L"../../Resources/StaticModels/grass.glb" };
			pScene->AddObject(RenderSubsystemType::AlphaKill, TestAlphaRenderObject::CreateAlphaModelFromGLTF(
				m_pDeviceContext,
				pCommandList,
				filepathGrass,
				m_pGBuffers[0],
				DirectX::XMMatrixScaling(.025f, .025f, .025f) * DirectX::XMMatrixTranslation(0.f, -2.f, -1.f)
			));

            m_pDeviceContext->GetCommandQueue()->ExecuteCommandListImmediately(pCommandList);
        };
        sceneObjectAdders[3] = [&](std::unique_ptr<Scene>& pScene) {
            std::shared_ptr<CommandList> pCommandList{
                m_pDeviceContext->GetCommandQueue()->GetCommandList(m_pDeviceContext->GetDevice())
            };

            std::filesystem::path filepathGrass{ L"../../Resources/StaticModels/grass.glb" };
            DirectX::XMMATRIX scale{ DirectX::XMMatrixScaling(.025f, .025f, .025f) };

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> posDist(-10.f, 10.f);
            for (size_t i{}; i < 100; ++i) {
                pScene->AddObject(RenderSubsystemType::AlphaKill, TestAlphaRenderObject::CreateAlphaModelFromGLTF(
                    m_pDeviceContext,
                    pCommandList,
                    filepathGrass,
                    m_pGBuffers[0],
                    scale * DirectX::XMMatrixTranslation(posDist(gen), -1.f, posDist(gen))
                ));
            }

            m_pDeviceContext->GetCommandQueue()->ExecuteCommandListImmediately(pCommandList);
        };

        // cameras and lights for all scenes
        for (size_t i{}; i < ScenesCount; ++i) {
            std::function<void(std::unique_ptr<Scene>&)> addObjects{ sceneObjectAdders[i] };
            m_pJobSystem->AddJob([&, i, addObjects]() {
                std::unique_ptr<Scene>& pScene{ m_pScenes[i] };

                // dynamic cameras
                pScene->AddCamera(std::make_shared<OrbitCamera>());
                pScene->AddCamera(std::make_shared<FlyCamera>());

                // standard camera
                pScene->AddCamera(std::make_shared<StaticCamera>(
                    DirectX::XMFLOAT3{ 0.f, 0.f, 3.f },
                    DirectX::XMFLOAT3{ 0.f, 0.f, 0.f },
                    DirectX::XMFLOAT3{ 0.f, 1.f, 0.f }
                ));

                // standard light
                pScene->AddLightSource(
                    { -1.5f, 0.f, 1.5f, 1.f },
                    { 1.f, 1.f, 0.f },
                    { 1.f, 1.f, 0.f }
                );

                // random lights
                for (size_t i{}; i < 0; ++i) {
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
                    m_pDeviceContext,
                    IndirectUpdater::CreateConstMesh4Updater(m_pDeviceContext)
                );

                pScene->SetSceneReadiness(true);
            });
        }
    }
    m_pDeviceContext->GetPSOLibrary()->FlushCacheToFile();
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
    m_settings.vsync = !m_settings.vsync;
}

void Renderer::SetSceneId(size_t sceneId) {
    m_nextSceneId.store(sceneId);
}

void Renderer::SwitchToNextCamera() {
    m_isSwitchToNextCamera.store(true);
}

void Renderer::SwitchCameraProjection() {
    m_isSwitchCameraProjection.store(true);
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
        if (m_isSwitchCameraProjection.load()) {
            m_pScenes[m_currSceneId]->SwitchCameraProjection();
            m_isSwitchCameraProjection.store(false);
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

    m_pBackBuffers = CreateBackBuffers(m_pDeviceContext->GetDevice(), m_pSwapChain, m_pBackBuffersDescHeapRange);

    m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight));

    // TODO update during switch
    for (auto& pScene : m_pScenes) {
        //pScene->UpdateCamerasAspectRatio(static_cast<float>(m_clientWidth) / m_clientHeight);
        pScene->Resize(m_pDeviceContext->GetDevice(), m_clientWidth, m_clientHeight);
    }
    for (auto& pDepthBuffer : m_pDepthBuffers) {
        pDepthBuffer->Resize(m_pDeviceContext->GetDevice(), m_clientWidth, m_clientHeight);
    }
    for (auto& pGBuffer : m_pGBuffers) {
        pGBuffer->Resize(m_pDeviceContext->GetDevice(), m_clientWidth, m_clientHeight);
    }
}

void Renderer::Update() {
    m_frameCounter++;
    auto t1 = m_clock.now();
    m_elapsedSeconds += (t1 - m_time).count() * 1e-9;
    m_time = t1;

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
    auto& pScene = m_pScenes.at(m_currSceneId);
    if (!pScene->IsSceneReady())
        return;

    std::shared_ptr<CommandQueue>& pQueue{ m_pDeviceContext->GetCommandQueue() };

    std::shared_ptr<GBuffer>& pGBuf{ pScene->GetGBuffer() };
    std::shared_ptr<DepthBuffer>& pDepthBuf{ pScene->GetDepthBuffer() };

    std::shared_ptr<EnumFence<GBufferState>>& pGBufFence{ pGBuf->GetFence() };
    std::shared_ptr<EnumFence<DepthBufferState>>& pDepthBufFence{ pDepthBuf->GetFence() };

    size_t listPriority{};

    auto& backBuffer = m_pBackBuffers[m_currBackBufferId];
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{ m_pBackBuffersDescHeapRange->GetCpuHandle(m_currBackBufferId) };

    std::shared_ptr<CommandList> commandListBeforeFrame{
        pQueue->GetDeferredCommandList(
            L"BeforeFrameJob", m_pDeviceContext->GetDevice(), listPriority
        )
    };

    // Some small work doesn't need to be moved to jobs, just as example
    {
        commandListBeforeFrame->PixBeginEvent(L"Before frame part");
        pScene->BeforeFrameJob(commandListBeforeFrame);

        // TODO: move it to "before first exec" task in CommandQueue::ExecutionTask
        auto newTime = m_clock.now();
        auto deltaTime = newTime - m_sceneTime;
        m_sceneTime = newTime;
        pScene->Update(
            m_pDeviceContext,
            commandListBeforeFrame,
            deltaTime.count() * 1e-9f
        );

        backBuffer->ResourceTransition(
            commandListBeforeFrame,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

        commandListBeforeFrame->SetReadyForExecution(); // but still it is cl to execute in proper order
    }

    // two command lists: static (1), dynamic (2)
    std::shared_ptr<CommandList> commandListForStaticObjects{ 
        pQueue->GetDeferredCommandList(
            L"StaticObjects", m_pDeviceContext->GetDevice(), ++listPriority,
            [&] {
                pQueue->GpuWait(pGBufFence, GBufferState::Write);
                pQueue->GpuWait(pDepthBufFence, DepthBufferState::DepthWriting);
            }
        )
    };
    m_pJobSystem->AddJob([&]() {
        commandListForStaticObjects->PixBeginEvent(L"Static Objects rendering");
        pScene->RenderObjects(
            RenderSubsystemType::Default,
            m_pDeviceContext,
            commandListForStaticObjects,
            m_viewport,
            m_scissorRect
        );
        commandListForStaticObjects->SetReadyForExecution();
        });

    std::shared_ptr<CommandList> commandListForAlphaObjects{
        pQueue->GetDeferredCommandList(
            L"StaticAlphakillObjects", m_pDeviceContext->GetDevice(), ++listPriority
        )
    };
    m_pJobSystem->AddJob([&]() {
        commandListForAlphaObjects->PixBeginEvent(L"Alphakill Objects rendering");
        pScene->RenderObjects(
            RenderSubsystemType::AlphaKill,
            m_pDeviceContext,
            commandListForAlphaObjects,
            m_viewport,
            m_scissorRect
        );
        commandListForAlphaObjects->SetReadyForExecution();
    });

    std::shared_ptr<CommandList> commandListForDynamicObjects{
        pQueue->GetDeferredCommandList(
            L"DynamicObjects",
            m_pDeviceContext->GetDevice(),
            ++listPriority,
            [] {},
            [&]() {
                pQueue->Signal(pGBufFence, GBufferState::Read);
                pQueue->Signal(pDepthBufFence, DepthBufferState::HierarchicalDepthBuilding);
            }
        )
    };
    m_pJobSystem->AddJob([&]() {
        commandListForDynamicObjects->PixBeginEvent(L"Dynamic Objects rendering");
        pScene->RenderObjects(
            RenderSubsystemType::Dynamic,
            m_pDeviceContext,
            commandListForDynamicObjects,
            m_viewport,
            m_scissorRect
        );
        pScene->RenderObjects(
            RenderSubsystemType::AlphaKill | RenderSubsystemType::Dynamic,
            m_pDeviceContext,
            commandListForDynamicObjects,
            m_viewport,
            m_scissorRect
        );
        commandListForDynamicObjects->SetReadyForExecution();
        });


    std::shared_ptr<CommandList> commandListForHZB{
        pQueue->GetDeferredCommandList(
            L"HZB",
            m_pDeviceContext->GetDevice(),
            ++listPriority,
            [&] {
                pQueue->GpuWait(pDepthBufFence, DepthBufferState::HierarchicalDepthBuilding);
            },
            [&] {
                pQueue->Signal(pDepthBufFence, DepthBufferState::DepthReading);
            }
        )
    };

    std::shared_ptr<CommandList> commandListForDeferredShading{
        pQueue->GetDeferredCommandList(
            L"DeferredShading",
            m_pDeviceContext->GetDevice(),
            ++listPriority,
            [&] {
                pQueue->GpuWait(pGBufFence, GBufferState::Read);
                pQueue->GpuWait(pDepthBufFence, DepthBufferState::DepthReading);
            },
            [&] {
                pQueue->Signal(pGBufFence, GBufferState::Write);
                pQueue->Signal(pDepthBufFence, DepthBufferState::DepthWriting);
            }
        )
    };
    std::shared_ptr<CommandList> commandListAfterFrame{
        pQueue->GetDeferredCommandList(
            L"AfterFrameJob",
            m_pDeviceContext->GetDevice(),
            ++listPriority
        )
    };
    m_pJobSystem->AddJob([&]() {
        {
            commandListForHZB->PixBeginEvent(L"Building HZB");
            pScene->GetDepthBuffer()->CreateHierarchicalDepthBuffer(
                commandListForHZB,
                m_pDeviceContext->GetDescriptorHeap(DescRangeType::Srv)->GetD3D12DescriptorHeap()
            );
            commandListForHZB->SetReadyForExecution();
        }

        {
            commandListForDeferredShading->PixBeginEvent(L"Deferred shading");
            pScene->RunDeferredShading(
                commandListForDeferredShading,
                m_pDeviceContext->GetDescriptorHeap(DescRangeType::Srv),
                m_pDeviceContext->GetMaterialManager(),
                m_clientWidth,
                m_clientHeight
            );
            commandListForDeferredShading->SetReadyForExecution();
        }

        {
            commandListAfterFrame->PixBeginEvent(L"Post Processing");
            pScene->RenderPostProcessing(
                commandListAfterFrame,
                m_pDeviceContext->GetDescriptorHeap(DescRangeType::Srv),
                m_viewport,
                m_scissorRect,
                rtv
            );

            m_pUI->Render(commandListAfterFrame, m_pDeviceContext);

            backBuffer->ResourceTransition(
                commandListAfterFrame,
                D3D12_RESOURCE_STATE_PRESENT
            );

            commandListAfterFrame->SetReadyForExecution();
        }
        });

    uint64_t lastCompletedFenceValue{ m_frameFenceValues[m_currBackBufferId] };
    m_frameFenceValues[m_currBackBufferId] = pQueue->ExecutionTask(m_frameFenceValues[m_currBackBufferId]);
    uint64_t fenceValue{ m_frameFenceValues[m_currBackBufferId] };

    // Present
    {
        UINT syncInterval{ m_settings.vsync ? 1u : 0 };
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

    m_pDeviceContext->FinishFrame(fenceValue, lastCompletedFenceValue);
}

void Renderer::MoveCamera(float forwardCoef, float rightCoef) {
    m_pScenes.at(m_currSceneId)->MoveCamera(forwardCoef, rightCoef);
}

void Renderer::RotateCamera(float deltaX, float deltaY) {
    m_pScenes.at(m_currSceneId)->RotateCamera(deltaX / m_clientWidth, deltaY / m_clientHeight);
}

void Renderer::ZoomCamera(float delta) {
    m_pScenes.at(m_currSceneId)->ZoomCamera(delta);
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

void Renderer::EnableDRED() {
    Microsoft::WRL::ComPtr<ID3D12DeviceRemovedExtendedDataSettings> pDredSettings;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&pDredSettings)));

    // Turn on auto-breadcrumbs and page fault reporting.
    pDredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    pDredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
}
#endif

Microsoft::WRL::ComPtr<IDXGIFactory6> Renderer::CreateDxgiFactory(UINT createFlags) const {
	Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(createFlags, IID_PPV_ARGS(&pDxgiFactory)));
	return pDxgiFactory;
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> Renderer::GetDxgiAdapterWarp(
	Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory
) const {
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pDxgiAdapter;
	ThrowIfFailed(pDxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pDxgiAdapter)));
	return pDxgiAdapter;
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> Renderer::GetDxgiAdapterByPreference(
	Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory,
	const DXGI_GPU_PREFERENCE& preference,
	size_t id,
	const DXGI_ADAPTER_FLAG& flags
) const {
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pDxgiAdapter;
	if (FAILED(pDxgiFactory->EnumAdapterByGpuPreference(
		id,
		preference,
		IID_PPV_ARGS(&pDxgiAdapter)
	))) {
		return nullptr;
	}

	if (flags != DXGI_ADAPTER_FLAG_NONE) {
		DXGI_ADAPTER_DESC1 desc;
		if (FAILED(pDxgiAdapter->GetDesc1(&desc)) || !(desc.Flags & flags))
			return nullptr;
	}

	return pDxgiAdapter;
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> Renderer::GetDxgiAdapterByVideoMemory(
	Microsoft::WRL::ComPtr<IDXGIFactory6> pDxgiFactory,
	size_t id,
	const DXGI_ADAPTER_FLAG& flags
) const {
	struct AdapterInfo {
		Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter;
		SIZE_T videoMemory;
	};
	std::vector<AdapterInfo> adapters;

	for (UINT i{};; ++i) {
		Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter;
		if (pDxgiFactory->EnumAdapterByGpuPreference(
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&pAdapter)
		) == DXGI_ERROR_NOT_FOUND)
			break;

		DXGI_ADAPTER_DESC1 desc{};
		if (FAILED(pAdapter->GetDesc1(&desc)))
			continue;
		if (flags != DXGI_ADAPTER_FLAG_NONE && !(desc.Flags & flags))
			continue;
		adapters.push_back({ pAdapter, desc.DedicatedVideoMemory });
	}

	if (adapters.empty())
		return nullptr;

	std::sort(adapters.begin(), adapters.end(),
		[](const AdapterInfo& a, const AdapterInfo& b) {
			return a.videoMemory > b.videoMemory;
		});

	return adapters[std::min(id, adapters.size() - 1)].pAdapter;
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

std::vector<std::shared_ptr<TextureResource>> Renderer::CreateBackBuffers(
    std::shared_ptr<Device> pDevice,
    Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain,
    std::shared_ptr<DescRange> pDescHeapRange
) {
    DXGI_SWAP_CHAIN_DESC desc{};
    ThrowIfFailed(pSwapChain->GetDesc(&desc));

    std::vector<std::shared_ptr<TextureResource>> backBuffers{ desc.BufferCount };

    pDescHeapRange->FreeAll();
    for (size_t i{}; i < desc.BufferCount; ++i) {
        backBuffers[i] = TextureResource::FromSwapChain(pSwapChain, i);
        backBuffers[i]->CreateRenderTargetView(
            pDevice,
            pDescHeapRange->AllocateGetCpuHandle()
        );
    }

    return backBuffers;
}

// Ensure that any commands previously executed on the GPU have finished executing 
// before the CPU thread is allowed to continue processing
void Renderer::Flush() {
    m_pDeviceContext->GetCommandQueue()->Flush();
    m_pDeviceContext->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY)->Flush();
}

// UI

#include "imgui.h"

void Renderer::RegisterUIPanels() {
    // Stats
    m_pUI->RegisterPanel([this]() {
        float framerate{ ImGui::GetIO().Framerate };

        ImGui::Begin("Stats");
        ImGui::Text("%.2f ms/frame (%.0f FPS)", 1000.0f / framerate, framerate);
        ImGui::Text("Resolution: %ux%u", m_clientWidth, m_clientHeight);
        ImGui::Text("Scene id: %zu", m_currSceneId);
        ImGui::End();
    });

    // Renderer settings
    m_pUI->RegisterPanel([this] {
        ImGui::Begin("Renderer Settings");

        if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("V-Sync", &m_settings.vsync);
        }

        ImGui::End();
    });

    // Scene settings
    m_pUI->RegisterPanel([this]() {
        ImGui::Begin("Scene");

        int sceneId{ static_cast<int>(m_nextSceneId.load()) };
        if (ImGui::SliderInt("Scene id", &sceneId, 0, static_cast<int>(m_pScenes.size()) - 1)) {
            SetSceneId(static_cast<size_t>(sceneId));
        }
        if (ImGui::Button("Next camera"))
            SwitchToNextCamera();
        ImGui::SameLine();
        if (ImGui::Button("Switch projection"))
            SwitchCameraProjection();

        ImGui::End();
    });

    // Camera's settings
    m_pUI->RegisterPanel([this]() {
        if (m_pScenes.empty())
            return;
        m_pScenes[m_currSceneId]->DrawCurrentCameraSettingsUI();
    });

    // Light settings
    m_pUI->RegisterPanel([this]() {
        if (m_pScenes.empty())
            return;
        m_pScenes[m_currSceneId]->DrawSettingsUI();
    });
}
