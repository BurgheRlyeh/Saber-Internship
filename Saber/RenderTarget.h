#pragma once

#include "Headers.h"

#include <vector>

#include "Texture.h"

class RenderTarget {
    const uint8_t m_numBuffers;

    std::vector<std::shared_ptr<Texture>> m_pTextures{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVDescHeap{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pSRVDescHeap{};
    UINT m_descSize;

public:
    static void ClearRenderTarget(
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
        const float* clearColor
    ) {
        // Before the render target can be cleared, it must be transitioned to the RENDER_TARGET state
        GPUResource::ResourceTransition(
            pCommandList,
            pBuffer,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
        );

        if (!clearColor) {
            static float defaultColor[] = {
                .4f,   // 
                .6f,   // 
                .9f,   // 
                1.f
            };

            clearColor = defaultColor;
        }

        pCommandList->ClearRenderTargetView(
            cpuDescHandle,
            clearColor,
            0,
            nullptr
        );
    }

    RenderTarget() = delete;
    RenderTarget(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        uint8_t numBuffers,
        const D3D12_RESOURCE_DESC& resDesc,
        const D3D12_CLEAR_VALUE& clearValue
    );

    void Resize(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        const D3D12_RESOURCE_DESC& resDesc,
        const D3D12_CLEAR_VALUE& clearValue
    );

    Microsoft::WRL::ComPtr<ID3D12Resource> GetCurrentBufferResource(size_t bufferId) const;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetRTVDescHeap() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPURTVDescHandle(int bufferId) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPURTVDescHandle(int bufferId) const;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSRVDescHeap() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUSRVDescHandle(int bufferId) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUSRVDescHandle(int bufferId) const;

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        Microsoft::WRL::ComPtr<ID3D12Device2> device,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32_t numDescriptors,
        D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    );

    void UpdateTextures(
        Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
        Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
        const D3D12_RESOURCE_DESC& resourceDesc,
        const D3D12_CLEAR_VALUE& clearValue
    );
};