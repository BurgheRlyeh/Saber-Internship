#pragma once

#include "Headers.h"

#include <mutex>

#include "Camera.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "ConstantBuffer.h"
#include "ComputeObject.h"
#include "DynamicUploadRingBuffer.h"
#include "MeshRenderObject.h"
#include "PostProcessing.h"
#include "Texture.h"
#include "Textures.h"

class Scene {
    static constexpr size_t LIGHTS_MAX_COUNT{ 10 };

    struct SceneBuffer {
        DirectX::XMMATRIX viewProjMatrix{};
        DirectX::XMFLOAT4 cameraPosition{};
    } m_sceneBuffer;
    std::mutex m_sceneBufferMutex{};
    std::shared_ptr<DynamicUploadHeap> m_pCameraHeap{};
    DynamicAllocation m_sceneCBDynamicAllocation{};
    std::atomic<bool> m_isUpdateSceneCB{};

    struct Light {
        DirectX::XMFLOAT4 position{};
        DirectX::XMFLOAT4 diffuseColorAndPower{};
        DirectX::XMFLOAT4 specularColorAndPower{};
    };
    struct LightBuffer {
        DirectX::XMFLOAT4 ambientColorAndPower{ 0.5f, 0.5f, 0.5f, 1.f };
        DirectX::XMUINT4 lightsCount{};
        Light lights[LIGHTS_MAX_COUNT]{};
    } m_lightBuffer;
    std::mutex m_lightBufferMutex{};
    std::shared_ptr<ConstantBuffer> m_pLightCB{};
    std::atomic<bool> m_isUpdateLightCB{};

    std::vector<std::shared_ptr<MeshRenderObject>> m_pStaticObjects{};
    std::mutex m_staticObjectsMutex{};

    std::vector<std::shared_ptr<MeshRenderObject>> m_pDynamicObjects{};
    std::mutex m_dynamicObjectsMutex{};

    std::vector<std::shared_ptr<Camera>> m_pCameras{};
    std::mutex m_camerasMutex{};
    std::atomic<bool> m_isUpdateCamera{};
    size_t m_currCameraId{};

    std::atomic<bool> m_isSceneReady{};
    std::atomic<bool> m_isUpdateCameraHeap{};

    std::shared_ptr<Textures> m_pGBuffer{};
    std::atomic<bool> m_isGBufferNeedResize{};

    std::shared_ptr<PostProcessing> m_pPostProcessing{};

    std::shared_ptr<ComputeObject> m_pDeferredShadingComputeObject{};

public:
    Scene() = delete;
    Scene(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator
    );

    void SetSceneReadiness(bool value) {
        m_isSceneReady.store(value);
    }

    bool IsSceneReady() {
        return m_isSceneReady.load();
    }

    void AddStaticObject(const MeshRenderObject& object);
    void AddDynamicObject(const MeshRenderObject& object);

    void AddCamera(const std::shared_ptr<Camera>&& pCamera);

    void SetAmbientLight(
        const DirectX::XMFLOAT3& color,
        const float& power = 1.f
    );

    bool AddLightSource(
        const DirectX::XMFLOAT4& position,
        const DirectX::XMFLOAT3& diffuseColor,
        const DirectX::XMFLOAT3& specularColor,
        const float& diffusePower = 1.f,
        const float& specularPower = 1.f
    );

    void UpdateCamerasAspectRatio(float aspectRatio);

    void Update(float deltaTime);

    bool SetCurrentCamera(size_t cameraId);

    void NextCamera();

    bool TryMoveCamera(float forwardCoef, float rightCoef);

    bool TryRotateCamera(float deltaX, float deltaY);

    bool TryUpdateCamera(float deltaTime);

    void SetPostProcessing(std::shared_ptr<PostProcessing> pPostProcessing);

    DirectX::XMMATRIX GetViewProjectionMatrix();

    void RenderStaticObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
    );
    void RenderDynamicObjects(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView,
        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView
    );
    void RenderPostProcessing(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect,
        D3D12_VIEWPORT viewport,
        D3D12_RECT scissorRect,
        D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView
    ) {
        if (!m_pGBuffer) {
            return;
        }

        //Texture::ClearRenderTarget(
        //    commandListAfterFrame->m_pCommandList,
        //    pGBuffer->GetTexture(0)->GetResource(),
        //    pGBuffer->GetCpuRtvDescHandle(0),
        //    nullptr
        //);

        //scene->RunDeferredShading(pCommandListCompute->m_pCommandList, m_clientWidth, m_clientHeight);
        //pCommandListCompute->SetReadyForExection();

        GPUResource::ResourceTransition(
            pCommandListDirect,
            m_pGBuffer->GetTexture(0)->GetResource(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        m_pPostProcessing->Render(
            pCommandListDirect,
            viewport,
            scissorRect,
            &renderTargetView,
            1,
            [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListDirect, UINT& rootParamId) {
                pCommandListDirect->SetGraphicsRootConstantBufferView(
                    rootParamId++,
                    m_sceneCBDynamicAllocation.gpuAddress
                );
                pCommandListDirect->SetGraphicsRootConstantBufferView(
                    rootParamId++,
                    m_pLightCB->GetResource()->GetGPUVirtualAddress()
                );

                Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap{ m_pGBuffer->GetSrvUavDescHeap() };
                pCommandListDirect->SetDescriptorHeaps(1, srvHeap.GetAddressOf());
                pCommandListDirect->SetGraphicsRootDescriptorTable(rootParamId++, m_pGBuffer->GetGpuSrvUavDescHandle(0));
            }
        );
    }

    void InitDeferredShadingComputeObject(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        std::shared_ptr<Atlas<ShaderResource>> pShaderAtlas,
        std::shared_ptr<Atlas<RootSignatureResource>> pRootSignatureAtlas,
        std::shared_ptr<PSOLibrary> pPSOLibrary
    ) {
        m_pDeferredShadingComputeObject = DeferredShading::CreateDefferedShadingComputeObject(
            pDevice,
            pShaderAtlas,
            pRootSignatureAtlas,
            pPSOLibrary
        );
    }

    void RunDeferredShading(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute,
        UINT width,
        UINT height
    ) {
        m_pDeferredShadingComputeObject->Dispatch(
            pCommandListCompute,
            width,
            height,
            1,
            [&](Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandListCompute, UINT& rootParamId) {
                pCommandListCompute->SetGraphicsRootConstantBufferView(
                    rootParamId++,
                    m_sceneCBDynamicAllocation.gpuAddress
                );
                pCommandListCompute->SetGraphicsRootConstantBufferView(
                    rootParamId++,
                    m_pLightCB->GetResource()->GetGPUVirtualAddress()
                );
                Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pTexDescHeap{ m_pGBuffer->GetSrvUavDescHeap() };
                pCommandListCompute->SetDescriptorHeaps(1, pTexDescHeap.GetAddressOf());
                pCommandListCompute->SetGraphicsRootDescriptorTable(rootParamId++, m_pGBuffer->GetGpuSrvUavDescHandle(0));
            }
        );
    }

    void CreateGBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        uint8_t numBuffers,
        UINT64 width,
        UINT height
    ) {
        D3D12_RESOURCE_DESC resDescs[3]{
            //CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height),   // position
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height),   // position
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height),   // normals
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height)     // albedo
        };
        for (auto& resDesc : resDescs) {
            resDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        FLOAT clearColor[] = { .6f, .4f, .4f, 1.f };

        m_pGBuffer = Textures::CreateTexturePack(
            pDevice,
            pAllocator,
            resDescs,
            _countof(resDescs)
            //, &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clearColor)
        );
    }

    void ResizeGBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        UINT64 width,
        UINT height
    ) {
        if (!m_pGBuffer) {
            return;
        }

        D3D12_RESOURCE_DESC resDescs[3]{
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32_FLOAT, width, height),   // position
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32_FLOAT, width, height),   // normals
            CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height)     // albedo
        };
        for (auto& resDesc : resDescs) {
            resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        FLOAT clearColor[] = { .6f, .4f, .4f, 1.f };

        m_pGBuffer->Resize(
            pDevice,
            pAllocator,
            resDescs,
            _countof(resDescs),
            &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clearColor)
        );
    }

    void ClearGBuffer(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList
    ) {
        for (size_t i{}; i < m_pGBuffer->GetTexturesCount(); ++i) {
            GPUResource::ResourceTransition(
                pCommandList,
                m_pGBuffer->GetTexture(i)->GetResource().Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            float clearColor[]{
                .6f,
                .4f,
                .4f,
                1.f
            };

            Texture::ClearRenderTarget(
                pCommandList,
                m_pGBuffer->GetTexture(i)->GetResource(),
                m_pGBuffer->GetCpuRtvDescHandle(i),
                clearColor
            );
        }
    }

    std::shared_ptr<Textures> GetGBuffer() {
        return m_pGBuffer;
    }

    void CopyRTV(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> pRenderTarget
    ) {
        Microsoft::WRL::ComPtr<ID3D12Resource> pTex{
            m_pGBuffer->GetTexture(0)->GetResource()
        };

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pTex.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_SOURCE
        ));
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST
        ));

        pCommandList->CopyResource(pRenderTarget.Get(), pTex.Get());

        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pTex.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ));
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            pRenderTarget.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        ));
    }

    void UpdateCameraHeap(uint64_t fenceValue, uint64_t lastCompletedFenceValue);

private:
    void UpdateSceneBuffer();
    void UpdateLightBuffer();
};